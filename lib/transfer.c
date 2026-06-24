/*
 * transfer.c — the import / export / sources commands.
 *
 *   secret sources               list providers and their availability
 *   secret export <source> <store>   push a store's params to a source
 *   secret import <source> <store>   pull a source's items into a store
 *
 * Providers are resolved through source.c (built-in table, then external
 * secret-source-<name> executables).
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

int cmd_sources(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	strlist list;
	strlist_init(&list);
	secret_source_list(s, &list);
	for (size_t i = 0; i < list.len; i++)
		puts(list.items[i]);
	strlist_free(&list);
	return 0;
}

/* Shared front-half for export/import: validate args, resolve the
 * provider and confirm the store exists. Returns the (maybe owned)
 * provider and the lowercased store name (caller frees store). */
static secret_source *resolve(secstore_t *s, int argc, char **argv,
                              char **store_out, int *owned)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a source");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify a store");

	const char *name = argv[0];
	char *store = secstore_lower(argv[1]);
	secstore_validate_name(s, "store", store);

	secret_source *src = secret_source_lookup(s, name, owned);
	if (!src) {
		secstore_fatal(s, "unknown source %s — see '%s sources'", name, s->self);
	}
	if (!src->available(s, src)) {
		char *n = xstrdup(name);
		if (*owned) secret_source_free(src);
		secstore_fatal(s, "source %s is not available on this system", n);
	}

	char *dir = store_dir(s, store);
	if (!is_dir(dir)) {
		free(dir);
		if (*owned) secret_source_free(src);
		secstore_fatal(s, "store %s does not exist", store);
	}
	free(dir);

	*store_out = store;
	return src;
}

int cmd_export(secstore_t *s, int argc, char **argv)
{
	char *store = NULL;
	int   owned = 0;
	secret_source *src = resolve(s, argc, argv, &store, &owned);

	secstore_info(s, "exporting store %s to source %s", store, src->name);
	int rc = src->do_export(s, src, store, argc - 2, argv + 2);

	if (owned) secret_source_free(src);
	free(store);
	return rc;
}

int cmd_import(secstore_t *s, int argc, char **argv)
{
	char *store = NULL;
	int   owned = 0;
	secret_source *src = resolve(s, argc, argv, &store, &owned);

	secstore_info(s, "importing into store %s from source %s", store, src->name);
	int rc = src->do_import(s, src, store, argc - 2, argv + 2);

	if (owned) secret_source_free(src);
	free(store);
	return rc;
}
