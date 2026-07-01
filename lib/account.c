/*
 * account.c — interoperability with account(1) configuration.
 *
 * secret is self-contained but reads ~/.config/account/ directly so its
 * keys and remotes line up with account(1) when that tool is also
 * installed (CLAUDE.md §4-5). None of this calls account(1) at runtime.
 */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

char *account_config_dir(secstore_t *s)
{
	return path_join(s->xdg_config_home, "account");
}

void account_list(secstore_t *s, strlist *out)
{
	char *cfg = account_config_dir(s);
	char *ssh = path_join(cfg, "ssh");
	DIR  *d = opendir(ssh);
	if (d) {
		struct dirent *e;
		while ((e = readdir(d))) {
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			char  *name = xstrdup(e->d_name);
			size_t n = strlen(name);
			if (n >= 4 && strcmp(name + n - 4, ".pub") == 0)
				name[n - 4] = '\0';   /* strip trailing .pub */
			strlist_push(out, name);
		}
		closedir(d);
	}
	free(ssh);
	free(cfg);
}

int account_has_gpg_key(secstore_t *s, const char *account)
{
	if (!account || !*account)
		return 0;
	char *acc = secstore_lower(account);
	char *cfg = account_config_dir(s);
	char *pub = xasprintf("%s/gpg/%s.pub", cfg, acc);
	int   rc  = is_file(pub);
	free(pub);
	free(cfg);
	free(acc);
	return rc;
}

int account_online(secstore_t *s, const char *account)
{
	(void)s;
	if (!account || !*account)
		return 0;
	char *acc  = secstore_lower(account);
	const char *at = strchr(acc, '@');
	const char *host = at ? at + 1 : acc;

	int reachable = 0;
	if (have_tool("fping")) {
		char *fp[] = { "fping", "-4", "-q", (char *)host, NULL };
		if (proc_run_quiet(fp, NULL) == 0) {
			char *ssh[] = {
				"ssh", "-o", "StrictHostKeyChecking=no",
				"-o", "IdentitiesOnly=yes", "-o", "BatchMode=yes",
				"-o", "ConnectTimeout=5", acc, "true", NULL
			};
			if (proc_run_quiet(ssh, NULL) == 0)
				reachable = 1;
		}
	}
	free(acc);
	return reachable;
}

char *account_remote_url(secstore_t *s, const char *account, const char *purpose)
{
	(void)s;
	if (!account || !*account)
		return NULL;
	if (!purpose || !*purpose || strcmp(purpose, "password-store") == 0)
		return xasprintf("%s:~/.password-store", account);
	if (strncmp(purpose, "secret/", 7) == 0)
		return xasprintf("%s:~/.config/secret/%s", account, purpose + 7);
	return xasprintf("%s:~/.config/%s", account, purpose);
}
