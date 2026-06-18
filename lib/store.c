/*
 * store.c — store/parameter enumeration and lifecycle commands:
 *   stores, params, exists, destroy, clean, ls, has.
 *
 * Behaviour mirrors the corresponding command:* functions in the
 * original bin/secret, including the colon<->slash translation, the
 * password-store special case, and the FEAT-207 path-traversal guard.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* ---- shared helpers ------------------------------------------------ */

char *store_dir(secstore_t *s, const char *store)
{
	if (strcmp(store, "password-store") == 0)
		return path_join(s->home, ".password-store");
	return path_join(s->self_config, store);
}

void store_list(secstore_t *s, strlist *out)
{
	list_subdirs(s->self_config, out);
	strlist_sort_unique(out);
	char *pstore = path_join(s->home, ".password-store");
	if (is_dir(pstore))
		strlist_push_copy(out, "password-store");
	free(pstore);
}

void param_list(secstore_t *s, const char *store, strlist *out)
{
	char   *dir = store_dir(s, store);
	if (is_dir(dir)) {
		strlist files;
		strlist_init(&files);
		list_files_rel(dir, &files);
		for (size_t i = 0; i < files.len; i++) {
			char  *name = files.items[i];
			size_t n = strlen(name);
			if (n >= 4 && strcmp(name + n - 4, ".gpg") == 0)
				name[n - 4] = '\0';          /* strip .gpg suffix  */
			str_translate(name, '/', ':');   /* host/name -> host:name */
			strlist_push_copy(out, name);
		}
		strlist_free(&files);
		strlist_sort_unique(out);
	}
	free(dir);
}

/* Resolve a single store argument: lowercase, reject empty / traversal.
 * Returns a malloc'd lowercased store name (caller frees). */
static char *resolve_store_arg(secstore_t *s, const char *raw)
{
	if (!raw || !*raw)
		secstore_fatal(s, "please specify a store");
	char *store = secstore_lower(raw);
	secstore_validate_name(s, "store", store);
	return store;
}

int store_param_has(secstore_t *s, const char *raw)
{
	char *store = NULL, *param = NULL;
	if (parse_param(s, raw, 0, &store, &param) != 0)
		return 1;                            /* no separator -> absent */

	char *dir  = store_dir(s, store);
	char *file = xasprintf("%s/%s.gpg", dir, param);
	int   rc   = path_exists(file) ? 0 : 1;

	free(file);
	free(dir);
	free(store);
	free(param);
	return rc;
}

/* ---- commands ------------------------------------------------------ */

int cmd_stores(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	strlist l;
	strlist_init(&l);
	store_list(s, &l);
	for (size_t i = 0; i < l.len; i++)
		puts(l.items[i]);
	strlist_free(&l);
	return 0;
}

int cmd_params(secstore_t *s, int argc, char **argv)
{
	char *store = resolve_store_arg(s, argc ? argv[0] : NULL);
	strlist l;
	strlist_init(&l);
	param_list(s, store, &l);
	for (size_t i = 0; i < l.len; i++)
		puts(l.items[i]);
	strlist_free(&l);
	free(store);
	return 0;
}

int cmd_exists(secstore_t *s, int argc, char **argv)
{
	char *store = resolve_store_arg(s, argc ? argv[0] : NULL);
	char *dir   = store_dir(s, store);
	int   rc    = is_dir(dir) ? 0 : 1;
	free(dir);
	free(store);
	return rc;
}

int cmd_destroy(secstore_t *s, int argc, char **argv)
{
	char *store = resolve_store_arg(s, argc ? argv[0] : NULL);
	char *dir   = store_dir(s, store);
	if (is_dir(dir))
		rm_rf(dir);
	free(dir);
	free(store);
	return 0;
}

int cmd_clean(secstore_t *s, int argc, char **argv)
{
	char *store = resolve_store_arg(s, argc ? argv[0] : NULL);
	char *dir   = store_dir(s, store);
	if (!is_dir(dir))
		secstore_fatal(s, "store %s does not exist", store);

	strlist files;
	strlist_init(&files);
	list_files_rel(dir, &files);
	for (size_t i = 0; i < files.len; i++) {
		const char *name = files.items[i];
		size_t      n = strlen(name);
		if (n >= 4 && strcmp(name + n - 4, ".gpg") == 0) {
			char *full = path_join(dir, name);
			if (file_size(full) == 0)
				remove(full);
			free(full);
		}
	}
	strlist_free(&files);
	free(dir);
	free(store);
	return 0;
}

int cmd_ls(secstore_t *s, int argc, char **argv)
{
	char *query = argc ? secstore_lower(argv[0]) : xstrdup("");
	size_t qlen = strlen(query);

	strlist stores;
	strlist_init(&stores);
	store_list(s, &stores);

	for (size_t i = 0; i < stores.len; i++) {
		strlist params;
		strlist_init(&params);
		param_list(s, stores.items[i], &params);
		for (size_t j = 0; j < params.len; j++) {
			char *line  = xasprintf("%s/%s", stores.items[i], params.items[j]);
			char *lower = secstore_lower(line);
			if (qlen == 0 || strncmp(lower, query, qlen) == 0)
				puts(line);
			free(lower);
			free(line);
		}
		strlist_free(&params);
	}

	strlist_free(&stores);
	free(query);
	return 0;
}

int cmd_has(secstore_t *s, int argc, char **argv)
{
	return store_param_has(s, argc ? argv[0] : NULL);
}
