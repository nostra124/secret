/*
 * recipients.c — the GPG recipient ("gpg-key") commands:
 *   gpg-keys, add-gpg-key, del-gpg-key, has-gpg-key.
 *
 * A store's recipients live as <store>/.gpg/<account>.pub files; the
 * password-store keeps them in ~/.password-store/.gpg-id.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* Read newline-separated entries from a file into out. */
static void read_lines(const char *path, strlist *out)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	char  *line = NULL;
	size_t cap = 0;
	ssize_t n;
	while ((n = getline(&line, &cap, f)) >= 0) {
		while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = '\0';
		if (n > 0)
			strlist_push_copy(out, line);
	}
	free(line);
	fclose(f);
}

void store_recipients(secstore_t *s, const char *store, strlist *out)
{
	if (strcmp(store, "password-store") == 0) {
		char *gpgid = xasprintf("%s/.password-store/.gpg-id", s->home);
		read_lines(gpgid, out);
		free(gpgid);
		char *id = secstore_identity(s);
		if (id)
			strlist_push(out, id);
	} else {
		char *dir = path_join(s->self_config, store);
		if (is_dir(dir)) {
			char *gpgdir = path_join(dir, ".gpg");
			strlist files;
			strlist_init(&files);
			list_files_rel(gpgdir, &files);
			for (size_t i = 0; i < files.len; i++) {
				char  *name = files.items[i];
				size_t n = strlen(name);
				if (n >= 4 && strcmp(name + n - 4, ".pub") == 0)
					name[n - 4] = '\0';
				strlist_push_copy(out, name);
			}
			strlist_free(&files);
			free(gpgdir);
		}
		free(dir);
	}
	strlist_sort_unique(out);
}

int cmd_gpg_keys(secstore_t *s, int argc, char **argv)
{
	/* No store: aggregate recipients across every store. */
	if (argc < 1) {
		strlist stores, all;
		strlist_init(&stores);
		strlist_init(&all);
		store_list(s, &stores);
		for (size_t i = 0; i < stores.len; i++)
			store_recipients(s, stores.items[i], &all);
		strlist_sort_unique(&all);
		for (size_t i = 0; i < all.len; i++)
			puts(all.items[i]);
		strlist_free(&all);
		strlist_free(&stores);
		return 0;
	}

	/* Store, no param: recipients for the named store(s). */
	if (argc < 2) {
		char   *store = secstore_lower(argv[0]);
		strlist out;
		strlist_init(&out);
		store_recipients(s, store, &out);
		for (size_t i = 0; i < out.len; i++)
			puts(out.items[i]);
		strlist_free(&out);
		free(store);
		return 0;
	}

	/* Store + param: extract the key ids that encrypted the file. */
	char *store = secstore_lower(argv[0]);
	char *param = secstore_lower(argv[1]);
	str_translate(param, ':', '/');

	char *dir = path_join(s->self_config, store);
	int   rc  = 0;
	if (is_dir(dir)) {
		char *file = xasprintf("%s/%s.gpg", dir, param);
		char *lp[] = { "gpg", "--list-packets", file, NULL };
		char  *out = NULL;
		size_t olen = 0;
		strlist uids;
		strlist_init(&uids);
		if (proc_run(lp, NULL, NULL, 0, &out, &olen, 1) == 0 && out) {
			char *save = NULL;
			for (char *l = strtok_r(out, "\n", &save); l;
			     l = strtok_r(NULL, "\n", &save)) {
				char *kw = strstr(l, "keyid ");
				if (l == strstr(l, "\t") || strstr(l, "pubkey")) {
					if (kw) {
						char *keyid = kw + 6;
						char *lk[] = { "gpg", "--list-key", keyid, NULL };
						char  *ko = NULL;
						size_t kl = 0;
						if (proc_run(lk, NULL, NULL, 0, &ko, &kl, 1) == 0 && ko) {
							char *s2 = NULL;
							for (char *u = strtok_r(ko, "\n", &s2); u;
							     u = strtok_r(NULL, "\n", &s2)) {
								if (strstr(u, "uid")) {
									char *lt = strrchr(u, '<');
									const char *st = lt ? lt + 1 : u;
									char *em = xmalloc(strlen(st) + 1);
									char *w = em;
									for (const char *r = st; *r; r++)
										if (*r != '>' && *r != ' ' && *r != '\t')
											*w++ = *r;
									*w = '\0';
									if (*em)
										strlist_push(&uids, em);
									else
										free(em);
								}
							}
						}
						free(ko);
					}
				}
			}
		}
		free(out);
		strlist_sort_unique(&uids);
		for (size_t i = 0; i < uids.len; i++)
			puts(uids.items[i]);
		strlist_free(&uids);
		free(file);
	}
	free(dir);
	free(store);
	free(param);
	return rc;
}

int cmd_has_gpg_key(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a store");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify an account");

	char *store   = secstore_lower(argv[0]);
	char *account = secstore_lower(argv[1]);
	int   rc;

	if (strcmp(store, "password-store") == 0) {
		char   *gpgid = xasprintf("%s/.password-store/.gpg-id", s->home);
		strlist lines;
		strlist_init(&lines);
		read_lines(gpgid, &lines);
		rc = 1;
		for (size_t i = 0; i < lines.len; i++)
			if (strstr(lines.items[i], account)) { rc = 0; break; }
		strlist_free(&lines);
		free(gpgid);
	} else {
		char *pub = xasprintf("%s/%s/.gpg/%s.pub", s->self_config, store, account);
		rc = is_file(pub) ? 0 : 1;
		free(pub);
	}
	free(store);
	free(account);
	return rc;
}

int cmd_add_gpg_key(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a store");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify an account");

	char *store   = secstore_lower(argv[0]);
	char *account = secstore_lower(argv[1]);

	if (!account_has_gpg_key(s, account))
		secstore_fatal(s, "unknown account %s - not found in ~/.config/account/gpg/",
		               account);

	if (strcmp(store, "password-store") == 0) {
		secstore_info(s, "adding gpg-key %s to store %s", account, store);
		char *gpgid = xasprintf("%s/.password-store/.gpg-id", s->home);
		FILE *f = fopen(gpgid, "a");
		if (f) {
			fprintf(f, "%s\n", account);
			char *id = secstore_identity(s);
			if (id) fprintf(f, "%s\n", id);
			free(id);
			fclose(f);
		}
		free(gpgid);
		cmd_init(s, 1, (char *[]){ store });
	} else {
		char *dir = path_join(s->self_config, store);
		if (!is_dir(dir))
			secstore_fatal(s, "store %s does not exist", store);
		secstore_info(s, "adding gpg-key %s to store %s", account, store);
		char *pubdata = gpg_export_public_key(s, account);
		char *gpgdir  = path_join(dir, ".gpg");
		mkdir_p(gpgdir);
		char *pub = xasprintf("%s/%s.pub", gpgdir, account);
		FILE *f = fopen(pub, "w");
		if (f) { fputs(pubdata, f); fclose(f); }
		char *relpub = xasprintf(".gpg/%s.pub", account);
		char *add[] = { "git", "add", "-f", relpub, NULL };
		proc_run_quiet(add, dir);
		char *msg = xasprintf("added gpg-key %s", account);
		char *commit[] = { "git", "commit", "-m", msg, "-a", NULL };
		proc_run_quiet(commit, dir);
		free(msg); free(relpub); free(pub); free(gpgdir);
		free(pubdata); free(dir);
		cmd_init(s, 1, (char *[]){ store });
	}
	free(store);
	free(account);
	return 0;
}

int cmd_del_gpg_key(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a store");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify an account");

	char *store   = secstore_lower(argv[0]);
	char *account = secstore_lower(argv[1]);

	if (strcmp(store, "password-store") == 0) {
		secstore_warn(s, "del-gpg-key for password-store is not supported");
	} else {
		char *dir = path_join(s->self_config, store);
		if (!is_dir(dir))
			secstore_fatal(s, "user store %s does not exist", store);
		char *pub = xasprintf("%s/.gpg/%s.pub", dir, account);
		if (is_file(pub)) {
			secstore_info(s, "removing gpg-key %s from store %s", account, store);
			char *relpub = xasprintf(".gpg/%s.pub", account);
			char *rm[] = { "git", "rm", "-f", relpub, NULL };
			proc_run_quiet(rm, dir);
			char *msg = xasprintf("removed gpg-key %s", account);
			char *commit[] = { "git", "commit", "-m", msg, "-a", NULL };
			proc_run_quiet(commit, dir);
			free(msg); free(relpub);
			cmd_init(s, 1, (char *[]){ store });
		}
		free(pub);
		free(dir);
	}
	free(store);
	free(account);
	return 0;
}
