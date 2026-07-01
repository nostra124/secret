/*
 * init.c — store bootstrap commands: init, setup, pass-init.
 *
 * init mirrors the original command:init: it git-inits a store, imports
 * every recipient public key from .gpg/, and re-encrypts each parameter
 * for the current recipient set. setup ensures a GPG identity exists and
 * exports its public key for account(1) interoperability.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* Ensure a GPG identity exists, generating one if the keyring lacks it,
 * and export its public key under ~/.config/account/gpg/. Returns the
 * identity string (caller frees). */
static char *ensure_identity(secstore_t *s)
{
	char *id = secstore_identity(s);
	if (!id || !*id) {
		secstore_fatal(s, "GPG identity setup failed");
	}
	/* Generate a key only if none matches the identity yet. */
	strlist fprs;
	strlist_init(&fprs);
	gpg_fingerprint(s, id, &fprs);
	if (fprs.len == 0) {
		secstore_info(s, "no GPG key for %s — generating new key", id);
		char *gen[] = {
			"gpg", "--batch", "--passphrase", "", "--quick-generate-key",
			id, "default", "default", NULL
		};
		proc_run_quiet(gen, NULL);
	}
	strlist_free(&fprs);

	char *cfg = account_config_dir(s);
	char *gpgdir = path_join(cfg, "gpg");
	mkdir_p(gpgdir);
	char *pub = xasprintf("%s/%s.pub", gpgdir, id);
	char *data = gpg_export_public_key(s, id);
	FILE *f = fopen(pub, "w");
	if (f) { fputs(data, f); fclose(f); }
	free(data); free(pub); free(gpgdir); free(cfg);
	return id;
}

/* Bring the pass(1) password store under git with a per-repo identity.
 *
 * pass(1) commits every change through its own internal git, which fails
 * silently when no git identity is configured (no global git config in a
 * fresh container / CI). `pass init` writes .gpg-id but the follow-up
 * `pass git init` then cannot commit it, and the legacy `pass git reset
 * --hard` wiped the uncommitted .gpg-id — leaving the store unusable so
 * `set`/`get` could never round-trip (BUG-216). Configuring the identity
 * and committing .gpg-id ourselves (instead of the destructive reset)
 * makes the store usable and keeps pass's later commits working. */
static void pass_store_git_setup(secstore_t *s, const char *id)
{
	char *dir = store_dir(s, "password-store");

	char *gi[] = { "git", "init", NULL };
	proc_run_quiet(gi, dir);

	if (id && *id) {
		char *ce[] = { "git", "config", "user.email", (char *)id, NULL };
		char *cn[] = { "git", "config", "user.name",  (char *)id, NULL };
		proc_run_quiet(ce, dir);
		proc_run_quiet(cn, dir);
	}

	char *cfg[] = { "git", "config", "--add",
	                "receive.denyCurrentBranch", "ignore", NULL };
	proc_run_quiet(cfg, dir);

	char *add[]    = { "git", "add", "-A", NULL };
	char *commit[] = { "git", "commit", "-m", "initialized password store", NULL };
	proc_run_quiet(add, dir);
	proc_run_quiet(commit, dir);

	free(dir);
}

static int init_one_store(secstore_t *s, const char *store)
{
	char *dir = path_join(s->self_config, store);
	mkdir_p(dir);

	/* git init if not already a repo. */
	char *toplevel = NULL;
	size_t tl = 0;
	char *rev[] = { "git", "rev-parse", "--show-toplevel", NULL };
	if (proc_run(rev, dir, NULL, 0, &toplevel, &tl, 1) != 0) {
		char *gi[] = { "git", "init", NULL };
		proc_run_quiet(gi, dir);
		char *cfg[] = { "git", "config", "--add",
		                "receive.denyCurrentBranch", "ignore", NULL };
		proc_run_quiet(cfg, dir);
		char *reset[] = { "git", "reset", "--hard", NULL };
		proc_run_quiet(reset, dir);
	}
	free(toplevel);

	/* Configure a per-repo git identity from the secret identity so
	 * commits succeed without relying on a global git config — otherwise
	 * `git commit` fails silently and the store has no history to sync
	 * (over SSH or HTTP). */
	{
		char *gid = secstore_identity(s);
		if (gid) {
			char *ce[] = { "git", "config", "user.email", gid, NULL };
			char *cn[] = { "git", "config", "user.name",  gid, NULL };
			proc_run_quiet(ce, dir);
			proc_run_quiet(cn, dir);
			free(gid);
		}
	}

	/* Recipient public keys: export ours, import all present. */
	char *gpgdir = path_join(dir, ".gpg");
	mkdir_p(gpgdir);
	char *recipients = path_join(gpgdir, "recipients");
	remove(recipients);
	free(recipients);

	char *id = secstore_identity(s);
	if (id) {
		char *mypub = xasprintf("%s/%s.pub", gpgdir, id);
		char *data = gpg_export_public_key(s, id);
		FILE *f = fopen(mypub, "w");
		if (f) { fputs(data, f); fclose(f); }
		free(data); free(mypub);
	}

	strlist pubs;
	strlist_init(&pubs);
	list_files_rel(gpgdir, &pubs);
	for (size_t i = 0; i < pubs.len; i++) {
		const char *name = pubs.items[i];
		size_t n = strlen(name);
		if (n < 4 || strcmp(name + n - 4, ".pub") != 0)
			continue;
		char *key = xstrdup(name);
		key[n - 4] = '\0';
		char  *full = path_join(gpgdir, name);
		char  *data = NULL;
		size_t dl = 0;
		read_file(full, &data, &dl);
		gpg_import_public_key(s, key, data, dl);
		char *relpub = xasprintf(".gpg/%s", name);
		char *add[] = { "git", "add", "-f", relpub, NULL };
		proc_run_quiet(add, dir);
		free(relpub); free(data); free(full); free(key);
	}
	strlist_free(&pubs);

	/* Re-encrypt every parameter for the current recipient set. */
	strlist recips;
	strlist_init(&recips);
	store_recipients(s, store, &recips);

	strlist files;
	strlist_init(&files);
	list_files_rel(dir, &files);
	for (size_t i = 0; i < files.len; i++) {
		const char *rel = files.items[i];
		size_t n = strlen(rel);
		if (n < 4 || strcmp(rel + n - 4, ".gpg") != 0)
			continue;
		char *full = path_join(dir, rel);
		if (file_size(full) > 0) {
			char  *ct = NULL, *pt = NULL, *nct = NULL;
			size_t ctl = 0, ptl = 0, nctl = 0;
			read_file(full, &ct, &ctl);
			if (gpg_decrypt(s, ct, ctl, &pt, &ptl) == 0 &&
			    gpg_encrypt(s, recips.items, recips.len, pt, ptl,
			                &nct, &nctl) == 0 && nctl > 0) {
				write_file(full, nct, nctl);
			}
			free(ct); free(pt); free(nct);
		} else {
			remove(full);
		}
		free(full);
	}
	strlist_free(&files);
	strlist_free(&recips);

	char *commit[] = { "git", "commit", "-a", "-m", "reinitialized secret", NULL };
	proc_run_quiet(commit, dir);

	if (s->euid == 0) {
		char *ch[] = { "chmod", "-R", "u=rwX,go=rX", dir, NULL };
		proc_run_quiet(ch, NULL);
	}

	free(id);
	free(gpgdir);
	free(dir);
	return 0;
}

int cmd_init(secstore_t *s, int argc, char **argv)
{
	char *store = (argc >= 1 && argv[0]) ? secstore_lower(argv[0]) : NULL;

	if (!store || !*store) {
		free(store);
		char *id = ensure_identity(s);
		free(id);
		mkdir_p(s->self_config);
		strlist stores;
		strlist_init(&stores);
		store_list(s, &stores);
		for (size_t i = 0; i < stores.len; i++)
			cmd_init(s, 1, (char *[]){ stores.items[i] });
		strlist_free(&stores);
		return 0;
	}

	if (strcmp(store, "password-store") == 0) {
		char *id = secstore_identity(s);
		strlist fprs;
		strlist_init(&fprs);
		if (id) gpg_fingerprint(s, id, &fprs);
		if (fprs.len > 0) {
			size_t argc2 = 2 + fprs.len + 1;
			char **pi = xmalloc(argc2 * sizeof(*pi));
			size_t a = 0;
			pi[a++] = "pass";
			pi[a++] = "init";
			for (size_t i = 0; i < fprs.len; i++)
				pi[a++] = fprs.items[i];
			pi[a] = NULL;
			proc_run(pi, NULL, NULL, 0, NULL, NULL, 0);
			free(pi);
		}
		strlist_free(&fprs);
		pass_store_git_setup(s, id);
		free(id);
		free(store);
		return 0;
	}

	mkdir_p(s->self_config);
	int rc = init_one_store(s, store);
	free(store);
	return rc;
}

int cmd_setup(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	secstore_info(s, "running central setup — GPG identity, git config, keys");

	char *id = ensure_identity(s);

	char *cfg = account_config_dir(s);
	char *sshdir = path_join(cfg, "ssh");
	mkdir_p(sshdir);
	free(sshdir);
	free(cfg);

	mkdir_p(s->self_config);

	secstore_info(s, "%s setup complete — identity: %s", s->self, id);
	free(id);
	return 0;
}

int cmd_pass_init(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	if (!have_tool("gpg"))
		secstore_fatal(s, "gpg(1) missing — install your distro's 'gnupg' package");
	if (!have_tool("pass"))
		secstore_fatal(s, "pass(1) missing — install your distro's 'pass' package");

	char *id = secstore_identity(s);
	if (!id || !*id)
		secstore_fatal(s, "no identity — run '%s setup' first", s->self);

	strlist fprs;
	strlist_init(&fprs);
	gpg_fingerprint(s, id, &fprs);
	size_t argc2 = 2 + fprs.len + 1;
	char **pi = xmalloc(argc2 * sizeof(*pi));
	size_t a = 0;
	pi[a++] = "pass";
	pi[a++] = "init";
	for (size_t i = 0; i < fprs.len; i++)
		pi[a++] = fprs.items[i];
	pi[a] = NULL;
	proc_run(pi, NULL, NULL, 0, NULL, NULL, 0);
	free(pi);
	strlist_free(&fprs);

	pass_store_git_setup(s, id);

	free(id);
	return 0;
}
