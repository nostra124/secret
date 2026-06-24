/*
 * sync.c — git-over-ssh synchronisation: remotes, pull, push, sync.
 *
 * Each store is a git repository; peers exchange them over SSH using the
 * account(1) remote-URL convention (account:~/.config/secret/<store>).
 * Conflicts are auto-resolved by taking the remote side, matching the
 * original command:pull behaviour.
 */
#define _XOPEN_SOURCE 700
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

int git_quiet(secstore_t *s, const char *cwd, ...)
{
	(void)s;
	char  *argv[32];
	size_t a = 0;
	argv[a++] = "git";
	va_list ap;
	va_start(ap, cwd);
	char *arg;
	while ((arg = va_arg(ap, char *)) != NULL && a < 31)
		argv[a++] = arg;
	va_end(ap);
	argv[a] = NULL;
	return proc_run_quiet(argv, cwd);
}

/* Capture a store's git remotes into out (one per line). */
static void store_remotes(secstore_t *s, const char *store, strlist *out)
{
	char *dir = path_join(s->self_config, store);
	if (is_dir(dir)) {
		char *re[] = { "git", "remote", NULL };
		char  *data = NULL;
		size_t dl = 0;
		if (proc_run(re, dir, NULL, 0, &data, &dl, 1) == 0 && data) {
			char *save = NULL;
			for (char *l = strtok_r(data, "\n", &save); l;
			     l = strtok_r(NULL, "\n", &save))
				if (*l) strlist_push_copy(out, l);
		}
		free(data);
	}
	free(dir);
}

int cmd_remotes(secstore_t *s, int argc, char **argv)
{
	strlist out;
	strlist_init(&out);

	if (argc == 0) {
		strlist stores;
		strlist_init(&stores);
		store_list(s, &stores);
		for (size_t i = 0; i < stores.len; i++)
			store_remotes(s, stores.items[i], &out);
		strlist_free(&stores);
	} else {
		for (int i = 0; i < argc; i++) {
			char *store = secstore_lower(argv[i]);
			store_remotes(s, store, &out);
			free(store);
		}
	}

	strlist_sort_unique(&out);
	for (size_t i = 0; i < out.len; i++)
		puts(out.items[i]);
	strlist_free(&out);
	return 0;
}

/* 1 if the named git remote already exists in cwd. */
static int has_remote(const char *cwd, const char *account)
{
	char *re[] = { "git", "remote", NULL };
	char  *data = NULL;
	size_t dl = 0;
	int    found = 0;
	if (proc_run(re, cwd, NULL, 0, &data, &dl, 1) == 0 && data) {
		char *save = NULL;
		for (char *l = strtok_r(data, "\n", &save); l;
		     l = strtok_r(NULL, "\n", &save))
			if (strcmp(l, account) == 0) { found = 1; break; }
	}
	free(data);
	return found;
}

/* ssh $account $self exists $store  ->  0 if the remote store exists. */
static int remote_store_exists(secstore_t *s, const char *account, const char *store)
{
	char *ssh[] = { "ssh", (char *)account, s->self, "exists", (char *)store, NULL };
	return proc_run_quiet(ssh, NULL) == 0;
}

int cmd_pull(secstore_t *s, int argc, char **argv)
{
	char *store   = (argc >= 1) ? secstore_lower(argv[0]) : xstrdup("");
	char *account = (argc >= 2) ? secstore_lower(argv[1]) : xstrdup("");

	if (!*store) {
		strlist stores;
		strlist_init(&stores);
		store_list(s, &stores);
		for (size_t i = 0; i < stores.len; i++)
			cmd_pull(s, 1, (char *[]){ stores.items[i] });
		strlist_free(&stores);
		goto done;
	}
	if (!*account) {
		strlist accounts;
		strlist_init(&accounts);
		account_list(s, &accounts);
		for (size_t i = 0; i < accounts.len; i++)
			cmd_pull(s, 2, (char *[]){ store, accounts.items[i] });
		strlist_free(&accounts);
		goto done;
	}
	if (!account_online(s, account)) {
		secstore_warn(s, "pulling store %s - account %s not reachable", store, account);
		goto done;
	}

	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir)) {
		free(dir);
		secstore_fatal(s, "store %s does not exist", store);
	}
	char *purpose = xasprintf("secret/%s", store);
	char *url = account_remote_url(s, account, purpose);
	free(purpose);
	if (has_remote(dir, account))
		git_quiet(s, dir, "remote", "set-url", account, url, NULL);
	else
		git_quiet(s, dir, "remote", "add", account, url, NULL);
	free(url);

	if (remote_store_exists(s, account, store)) {
		secstore_info(s, "pulling store %s from account %s", store, account);
		char  *pull[10];
		size_t a = 0;
		pull[a++] = "git";
		pull[a++] = "pull";
		if (s->force[0])
			pull[a++] = (char *)s->force;
		pull[a++] = "--no-edit";
		pull[a++] = "--commit";
		pull[a++] = "--allow-unrelated-histories";
		pull[a++] = account;
		pull[a] = NULL;
		proc_run(pull, dir, NULL, 0, NULL, NULL, 0);
		/* Auto-resolve conflicts by taking theirs. */
		char *diff[] = { "git", "diff", "--name-only", "--diff-filter=U", NULL };
		char  *names = NULL;
		size_t nl = 0;
		if (proc_run(diff, dir, NULL, 0, &names, &nl, 1) == 0 && names && *names) {
			char *save = NULL;
			for (char *f = strtok_r(names, "\n", &save); f;
			     f = strtok_r(NULL, "\n", &save))
				git_quiet(s, dir, "checkout", "--theirs", f, NULL);
			git_quiet(s, dir, "commit", "-a", "-m", "resolved merging conflicts", NULL);
		}
		free(names);
	} else {
		secstore_warn(s, "%s store %s does not exists at account %s",
		              s->self, store, account);
	}
	free(dir);
done:
	free(store);
	free(account);
	return 0;
}

int cmd_push(secstore_t *s, int argc, char **argv)
{
	char *store   = (argc >= 1) ? secstore_lower(argv[0]) : xstrdup("");
	char *account = (argc >= 2) ? secstore_lower(argv[1]) : xstrdup("");

	if (!*store) {
		strlist stores;
		strlist_init(&stores);
		store_list(s, &stores);
		for (size_t i = 0; i < stores.len; i++)
			cmd_push(s, 1, (char *[]){ stores.items[i] });
		strlist_free(&stores);
		goto done;
	}
	if (!*account) {
		strlist accounts;
		strlist_init(&accounts);
		account_list(s, &accounts);
		for (size_t i = 0; i < accounts.len; i++)
			cmd_push(s, 2, (char *[]){ store, accounts.items[i] });
		strlist_free(&accounts);
		goto done;
	}
	if (!account_online(s, account)) {
		secstore_warn(s, "pushing store %s - account %s not reachable", store, account);
		goto done;
	}

	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir)) {
		free(dir);
		secstore_fatal(s, "store %s does not exist", store);
	}
	char *purpose = xasprintf("secret/%s", store);
	char *url = account_remote_url(s, account, purpose);
	free(purpose);
	if (has_remote(dir, account))
		git_quiet(s, dir, "remote", "set-url", account, url, NULL);
	else
		git_quiet(s, dir, "remote", "add", account, url, NULL);
	free(url);

	if (!remote_store_exists(s, account, store)) {
		char *ssh[] = { "ssh", account, s->self, "init", store, NULL };
		proc_run(ssh, NULL, NULL, 0, NULL, NULL, 0);
	}
	secstore_info(s, "pushing store %s to account %s", store, account);
	if (s->force[0])
		git_quiet(s, dir, "push", (char *)s->force, account, NULL);
	else
		git_quiet(s, dir, "push", account, NULL);
	free(dir);
done:
	free(store);
	free(account);
	return 0;
}

int cmd_sync(secstore_t *s, int argc, char **argv)
{
	char *account = (argc >= 1) ? secstore_lower(argv[0]) : xstrdup("");

	if (!*account) {
		secstore_info(s, "no account given - merging all accounts");
		strlist accounts;
		strlist_init(&accounts);
		account_list(s, &accounts);
		for (size_t i = 0; i < accounts.len; i++)
			cmd_sync(s, 1, (char *[]){ accounts.items[i] });
		strlist_free(&accounts);
		free(account);
		return 0;
	}
	if (!account_online(s, account)) {
		secstore_warn(s, "account %s not reachable", account);
		free(account);
		return 0;
	}

	/* Pull every remote store, then push every local store. */
	char *ssh[] = { "ssh", account, s->self, "stores", NULL };
	char  *data = NULL;
	size_t dl = 0;
	if (proc_run(ssh, NULL, NULL, 0, &data, &dl, 1) == 0 && data) {
		char *save = NULL;
		for (char *st = strtok_r(data, "\n", &save); st;
		     st = strtok_r(NULL, "\n", &save))
			cmd_pull(s, 2, (char *[]){ st, account });
	}
	free(data);

	strlist stores;
	strlist_init(&stores);
	store_list(s, &stores);
	for (size_t i = 0; i < stores.len; i++)
		cmd_push(s, 2, (char *[]){ stores.items[i], account });
	strlist_free(&stores);

	free(account);
	return 0;
}
