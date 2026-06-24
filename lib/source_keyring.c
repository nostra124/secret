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

static int keyring_do_import(secstore_t *s, const secret_source *self,
                             const char *store, int argc, char **argv)
{
	(void)self; (void)argc; (void)argv;

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
