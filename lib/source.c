/*
 * source.c — the source-provider registry and the external-plugin
 * bridge.
 *
 * Providers come in two flavours:
 *   - built-in: compiled-in entries in the table below (e.g. keyring);
 *   - external: `secret-source-<name>` executables discovered in
 *     $libexecdir/secret/sources/ and on $PATH, driven through a small
 *     base64 line protocol:
 *         secret-source-<name> available        -> exit 0 if usable
 *         secret-source-<name> export <store>   <- reads param<TAB>b64 lines
 *         secret-source-<name> import <store>   -> writes param<TAB>b64 lines
 *
 * Both directions key on the slash-form parameter id (host/name), so a
 * round-trip through any provider preserves the store layout.
 */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#define SOURCE_EXEC_PREFIX "secret-source-"

static const secret_source *const builtins[] = {
	&secret_source_keyring,
	NULL,
};

/* ---- external plugin bridge --------------------------------------- */

static int exec_available(secstore_t *s, const secret_source *self)
{
	(void)s;
	char *argv[] = { self->path, "available", NULL };
	return proc_run_quiet(argv, NULL) == 0;
}

static int exec_do_export(secstore_t *s, const secret_source *self,
                          const char *store, int argc, char **argv_extra)
{
	/* Build the param<TAB>base64(value) stream for the whole store. */
	strlist params;
	strlist_init(&params);
	param_list(s, store, &params);

	size_t cap = 256, len = 0;
	char  *buf = xmalloc(cap);
	for (size_t i = 0; i < params.len; i++) {
		char *slash = xstrdup(params.items[i]);
		str_translate(slash, ':', '/');
		char  *val = NULL;
		size_t vlen = 0;
		if (store_get_value(s, store, slash, &val, &vlen) == 0) {
			char  *b64 = b64_encode((unsigned char *)val, vlen);
			char  *rec = xasprintf("%s\t%s\n", slash, b64);
			size_t rl = strlen(rec);
			if (len + rl + 1 > cap) {
				while (len + rl + 1 > cap) cap *= 2;
				buf = xrealloc(buf, cap);
			}
			memcpy(buf + len, rec, rl);
			len += rl;
			free(rec); free(b64); free(val);
		}
		free(slash);
	}
	strlist_free(&params);

	/* argv: <plugin> export <store> [extra args forwarded verbatim] */
	char **argv = xmalloc((size_t)(argc + 4) * sizeof(*argv));
	int a = 0;
	argv[a++] = self->path;
	argv[a++] = "export";
	argv[a++] = (char *)store;
	for (int i = 0; i < argc; i++)
		argv[a++] = argv_extra[i];
	argv[a] = NULL;

	int rc = proc_run(argv, NULL, buf, len, NULL, NULL, 0);
	free(argv);
	free(buf);
	return rc;
}

static int exec_do_import(secstore_t *s, const secret_source *self,
                          const char *store, int argc, char **argv_extra)
{
	char **argv = xmalloc((size_t)(argc + 4) * sizeof(*argv));
	int a = 0;
	argv[a++] = self->path;
	argv[a++] = "import";
	argv[a++] = (char *)store;
	for (int i = 0; i < argc; i++)
		argv[a++] = argv_extra[i];
	argv[a] = NULL;

	char  *out = NULL;
	size_t olen = 0;
	int spawn = proc_run(argv, NULL, NULL, 0, &out, &olen, 0);
	free(argv);
	if (spawn != 0 || !out) {
		free(out);
		return 1;
	}

	int n = 0;
	char *save = NULL;
	for (char *line = strtok_r(out, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		if (!tab)
			continue;
		*tab = '\0';
		const char *param = line;
		size_t vlen = 0;
		unsigned char *val = b64_decode(tab + 1, &vlen);
		if (*param) {
			store_set_value(s, store, param, (char *)val, vlen);
			n++;
		}
		free(val);
	}
	free(out);
	secstore_info(s, "imported %d parameter(s) into %s", n, store);
	return 0;
}

/* ---- discovery ----------------------------------------------------- */

/* Directory holding external source plugins, relative to the binary. */
static char *sources_libexec_dir(secstore_t *s)
{
	return xasprintf("%s/../libexec/secret/sources", s->exe_dir);
}

/* Full path to secret-source-<name> if it exists in libexec or on PATH. */
static char *find_source_exec(secstore_t *s, const char *name)
{
	char *fname = xasprintf("%s%s", SOURCE_EXEC_PREFIX, name);

	char *libexec = sources_libexec_dir(s);
	char *cand = path_join(libexec, fname);
	free(libexec);
	if (access(cand, X_OK) == 0) { free(fname); return cand; }
	free(cand);

	const char *path = getenv("PATH");
	if (path) {
		char *copy = xstrdup(path);
		for (char *tok = strtok(copy, ":"); tok; tok = strtok(NULL, ":")) {
			char *full = path_join(tok, fname);
			if (access(full, X_OK) == 0) {
				free(copy); free(fname);
				return full;
			}
			free(full);
		}
		free(copy);
	}
	free(fname);
	return NULL;
}

static secret_source *make_exec_source(const char *name, char *path /*owned*/)
{
	secret_source *src = xmalloc(sizeof(*src));
	src->name      = xstrdup(name);
	src->available = exec_available;
	src->do_export = exec_do_export;
	src->do_import = exec_do_import;
	src->path      = path;
	return src;
}

secret_source *secret_source_lookup(secstore_t *s, const char *name, int *owned)
{
	for (int i = 0; builtins[i]; i++) {
		if (strcmp(builtins[i]->name, name) == 0) {
			*owned = 0;
			return (secret_source *)builtins[i];
		}
	}
	char *path = find_source_exec(s, name);
	if (path) {
		*owned = 1;
		return make_exec_source(name, path);
	}
	*owned = 0;
	return NULL;
}

void secret_source_free(secret_source *src)
{
	if (!src)
		return;
	free(src->name);
	free(src->path);
	free(src);
}

/* Collect external plugin names (basename minus the prefix) from a dir. */
static void scan_exec_dir(const char *dir, strlist *out)
{
	DIR *d = opendir(dir);
	if (!d)
		return;
	size_t plen = strlen(SOURCE_EXEC_PREFIX);
	struct dirent *e;
	while ((e = readdir(d))) {
		if (strncmp(e->d_name, SOURCE_EXEC_PREFIX, plen) != 0)
			continue;
		char *full = path_join(dir, e->d_name);
		if (access(full, X_OK) == 0)
			strlist_push_copy(out, e->d_name + plen);
		free(full);
	}
	closedir(d);
}

void secret_source_list(secstore_t *s, strlist *out)
{
	/* Built-ins first. */
	strlist seen;
	strlist_init(&seen);
	for (int i = 0; builtins[i]; i++) {
		const secret_source *b = builtins[i];
		int ok = b->available(s, b);
		strlist_push(out, xasprintf("%s\t%s", b->name,
		                            ok ? "available" : "unavailable"));
		strlist_push_copy(&seen, b->name);
	}

	/* External plugins: libexec dir + every PATH entry. */
	strlist names;
	strlist_init(&names);
	char *libexec = sources_libexec_dir(s);
	scan_exec_dir(libexec, &names);
	free(libexec);
	const char *path = getenv("PATH");
	if (path) {
		char *copy = xstrdup(path);
		for (char *tok = strtok(copy, ":"); tok; tok = strtok(NULL, ":"))
			scan_exec_dir(tok, &names);
		free(copy);
	}
	strlist_sort_unique(&names);

	for (size_t i = 0; i < names.len; i++) {
		/* Skip names that shadow a built-in. */
		int dup = 0;
		for (size_t j = 0; j < seen.len; j++)
			if (strcmp(seen.items[j], names.items[i]) == 0) { dup = 1; break; }
		if (dup)
			continue;
		int owned = 0;
		secret_source *src = secret_source_lookup(s, names.items[i], &owned);
		int ok = src && src->available(s, src);
		strlist_push(out, xasprintf("%s\t%s", names.items[i],
		                            ok ? "available" : "unavailable"));
		if (owned)
			secret_source_free(src);
	}
	strlist_free(&names);
	strlist_free(&seen);
}
