/*
 * source_keyring.c — the built-in GNOME keyring (freedesktop Secret
 * Service) source provider.
 *
 * Talks to the keyring by shelling out to secret-tool(1) — the same
 * "shell out to the platform tool" approach secret already uses for
 * gpg/git/pass/ssh/qrencode. Items are namespaced with the attributes
 *   application=secret  store=<store>  param=<param>
 * and labelled  secret/<store>/<param>  so import can recover the
 * parameter name from the label and exact-fetch each value via
 * `secret-tool lookup`.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static int keyring_available(secstore_t *s, const secret_source *self)
{
	(void)s; (void)self;
	return have_tool("secret-tool");
}

/* Store one item: secret-tool store --label <label> application secret
 * store <store> param <param>  (value on stdin). */
static int keyring_put(secstore_t *s, const char *store, const char *param,
                       const char *value, size_t vlen)
{
	(void)s;
	char *label = xasprintf("secret/%s/%s", store, param);
	char *argv[] = {
		"secret-tool", "store", "--label", label,
		"application", "secret",
		"store", (char *)store,
		"param", (char *)param,
		NULL
	};
	int rc = proc_run(argv, NULL, value, vlen, NULL, NULL, 1);
	free(label);
	return rc;
}

/* Fetch one item's exact value. Returns 0 on success (*out owned). */
static int keyring_get(secstore_t *s, const char *store, const char *param,
                       char **out, size_t *outlen)
{
	(void)s;
	char *argv[] = {
		"secret-tool", "lookup",
		"application", "secret",
		"store", (char *)store,
		"param", (char *)param,
		NULL
	};
	return proc_run(argv, NULL, NULL, 0, out, outlen, 1) == 0 ? 0 : -1;
}

static int keyring_do_export(secstore_t *s, const secret_source *self,
                             const char *store, int argc, char **argv)
{
	(void)self; (void)argc; (void)argv;

	strlist params;
	strlist_init(&params);
	param_list(s, store, &params);     /* colon form: host:name */

	int n = 0;
	for (size_t i = 0; i < params.len; i++) {
		/* on-disk param is slash form; use it as the keyring param id */
		char *slash = xstrdup(params.items[i]);
		str_translate(slash, ':', '/');

		char  *val = NULL;
		size_t vlen = 0;
		if (store_get_value(s, store, slash, &val, &vlen) == 0) {
			if (keyring_put(s, store, slash, val, vlen) == 0)
				n++;
			else
				secstore_warn(s, "keyring: failed to store %s/%s", store, slash);
			free(val);
		}
		free(slash);
	}
	strlist_free(&params);
	secstore_info(s, "exported %d parameter(s) from %s to the keyring", n, store);
	return 0;
}

/* Recover the param ids of our items from `secret-tool search --all`
 * output by reading the `label = secret/<store>/<param>` lines. */
static void keyring_scan_params(secstore_t *s, const char *store, strlist *out)
{
	(void)s;
	char *argv[] = {
		"secret-tool", "search", "--all",
		"application", "secret",
		"store", (char *)store,
		NULL
	};
	char  *data = NULL;
	size_t dl = 0;
	if (proc_run(argv, NULL, NULL, 0, &data, &dl, 1) != 0 || !data) {
		free(data);
		return;
	}

	char *prefix = xasprintf("secret/%s/", store);
	size_t plen = strlen(prefix);
	char *save = NULL;
	for (char *line = strtok_r(data, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		if (strncmp(line, "label = ", 8) != 0)
			continue;
		const char *label = line + 8;
		if (strncmp(label, prefix, plen) == 0 && label[plen])
			strlist_push_copy(out, label + plen);
	}
	strlist_sort_unique(out);
	free(prefix);
	free(data);
}

/* Sanitise a free-form keyring label into a safe parameter id: lowercase,
 * collapse runs of unsafe characters to '-', trim. Returns NULL if the
 * result is empty or would traverse. Caller frees. */
static char *label_to_param(const char *label, const char *prefix)
{
	char  *out = xmalloc(strlen(label) + 1);
	size_t o = 0;
	int    dash = 0;
	for (const char *p = label; *p; p++) {
		unsigned char c = (unsigned char)*p;
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		    c == '/' || c == '_' || c == '.' || c == '-') {
			out[o++] = (char)c; dash = 0;
		} else if (c >= 'A' && c <= 'Z') {
			out[o++] = (char)(c - 'A' + 'a'); dash = 0;
		} else if (!dash) {
			out[o++] = '-'; dash = 1;
		}
	}
	out[o] = '\0';
	/* trim leading/trailing '-' and '/' */
	char *start = out;
	while (*start == '-' || *start == '/') start++;
	size_t e = strlen(start);
	while (e > 0 && (start[e - 1] == '-' || start[e - 1] == '/')) start[--e] = '\0';

	char *result = NULL;
	if (*start && !strstr(start, "..")) {
		result = prefix && *prefix
			? xasprintf("%s/%s", prefix, start)
			: xstrdup(start);
	}
	free(out);
	return result;
}

/* Import items selected by --query attributes ("foreign" items not in
 * secret's own namespace). The value is reconstructed from the
 * `secret-tool search --all` block (multiline-aware); each item's label
 * becomes a sanitised parameter id. */
static int keyring_import_foreign(secstore_t *s, const char *store,
                                  char **query, int nquery, const char *prefix)
{
	size_t argc = 3 + (size_t)nquery * 2 + 1;
	char **argv = xmalloc(argc * sizeof(*argv));
	size_t a = 0;
	argv[a++] = "secret-tool";
	argv[a++] = "search";
	argv[a++] = "--all";
	for (int i = 0; i < nquery; i++) {
		char *eq = strchr(query[i], '=');
		if (!eq)
			secstore_fatal(s, "keyring: --query expects attr=value, got %s", query[i]);
		*eq = '\0';
		argv[a++] = query[i];     /* attr */
		argv[a++] = eq + 1;       /* value */
	}
	argv[a] = NULL;

	char  *data = NULL;
	size_t dl = 0;
	int rc = proc_run(argv, NULL, NULL, 0, &data, &dl, 1);
	free(argv);
	if (rc != 0 || !data) {
		free(data);
		secstore_warn(s, "keyring: no items matched the query");
		return 0;
	}

	int    n = 0;
	char  *label = NULL, *val = NULL;
	size_t vlen = 0;
	int    cap_secret = 0;

	/* Manual line walk (strtok would drop the blank lines inside
	 * multiline secrets). */
	char *line = data;
	while (line && *line) {
		char *nl = strchr(line, '\n');
		if (nl) *nl = '\0';

		if (strncmp(line, "[/", 2) == 0) {
			/* block boundary: flush the previous item */
			if (label && val) {
				char *param = label_to_param(label, prefix);
				if (param) { store_set_value(s, store, param, val, vlen); n++; free(param); }
			}
			free(label); free(val);
			label = NULL; val = NULL; vlen = 0; cap_secret = 0;
		} else if (strncmp(line, "label = ", 8) == 0) {
			free(label); label = xstrdup(line + 8); cap_secret = 0;
		} else if (strncmp(line, "secret = ", 9) == 0) {
			free(val); val = xstrdup(line + 9); vlen = strlen(val); cap_secret = 1;
		} else if (cap_secret && (strncmp(line, "created = ", 10) == 0 ||
		                          strncmp(line, "modified = ", 11) == 0 ||
		                          strncmp(line, "schema = ", 9) == 0)) {
			cap_secret = 0;
		} else if (cap_secret) {
			size_t ll = strlen(line);
			val = xrealloc(val, vlen + ll + 2);
			val[vlen++] = '\n';
			memcpy(val + vlen, line, ll);
			vlen += ll;
			val[vlen] = '\0';
		}

		line = nl ? nl + 1 : NULL;
	}
	if (label && val) {
		char *param = label_to_param(label, prefix);
		if (param) { store_set_value(s, store, param, val, vlen); n++; free(param); }
	}
	free(label); free(val); free(data);

	secstore_info(s, "imported %d item(s) from the keyring into %s", n, store);
	return 0;
}

static int keyring_do_import(secstore_t *s, const secret_source *self,
                             const char *store, int argc, char **argv)
{
	(void)self;

	/* Parse options: --query attr=val (repeatable) selects foreign
	 * items; --prefix p namespaces the imported params. */
	char **query = NULL;
	int    nquery = 0;
	const char *prefix = NULL;
	for (int i = 0; i < argc; i++) {
		if ((!strcmp(argv[i], "--query") || !strcmp(argv[i], "-q")) && i + 1 < argc) {
			query = xrealloc(query, (size_t)(nquery + 1) * sizeof(*query));
			query[nquery++] = argv[++i];
		} else if (!strcmp(argv[i], "--prefix") && i + 1 < argc) {
			prefix = argv[++i];
		} else {
			secstore_fatal(s, "keyring import: unknown option %s", argv[i]);
		}
	}

	if (nquery > 0) {
		int rc = keyring_import_foreign(s, store, query, nquery, prefix);
		free(query);
		return rc;
	}
	free(query);

	/* Default: round-trip secret's own namespaced items. */
	strlist params;
	strlist_init(&params);
	keyring_scan_params(s, store, &params);

	int n = 0;
	for (size_t i = 0; i < params.len; i++) {
		const char *param = params.items[i];   /* slash form */
		char  *val = NULL;
		size_t vlen = 0;
		/* lookup also rejects spoofed label lines: a param with no real
		 * item returns failure and is skipped. */
		if (keyring_get(s, store, param, &val, &vlen) == 0 && val) {
			store_set_value(s, store, param, val, vlen);
			n++;
		}
		free(val);
	}
	strlist_free(&params);
	secstore_info(s, "imported %d parameter(s) from the keyring into %s", n, store);
	return 0;
}

const secret_source secret_source_keyring = {
	.name      = "keyring",
	.available = keyring_available,
	.do_export = keyring_do_export,
	.do_import = keyring_do_import,
	.path      = NULL,
};
