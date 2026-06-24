/*
 * group.c — sharing groups and HTTP pull.
 *
 * A "group" is a named set of HTTP/HTTPS endpoints that share secret
 * stores. Endpoints live one-per-line under $SELF_CONFIG/.groups/<group>.
 * `secret pull-http <store> [group]` pulls a store from a group's
 * endpoints over the dumb-HTTP path /app/secret/<store> (pull only;
 * pushing is over SSH).
 *
 * Endpoints may be given loosely — "host", "host:port",
 * "http://host", "https://host:port" — and are normalised to a base URL,
 * defaulting the scheme to http and the port to the local uid-derived
 * port (so a same-uid peer needs no port).
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

char *groups_dir(secstore_t *s)
{
	return path_join(s->self_config, ".groups");
}

/* host[:port] / scheme://host[:port]  ->  scheme://host:port (no path). */
char *endpoint_base_url(secstore_t *s, const char *endpoint)
{
	const char *scheme = "http://";
	const char *rest = endpoint;
	if (strncmp(endpoint, "http://", 7) == 0) { scheme = "http://";  rest = endpoint + 7; }
	else if (strncmp(endpoint, "https://", 8) == 0) { scheme = "https://"; rest = endpoint + 8; }

	/* Strip any trailing path. */
	char  *hostport = xstrdup(rest);
	char  *slash = strchr(hostport, '/');
	if (slash) *slash = '\0';

	char *url;
	if (strchr(hostport, ':')) {
		url = xasprintf("%s%s", scheme, hostport);    /* port already present */
	} else if (strcmp(scheme, "https://") == 0) {
		url = xasprintf("%s%s", scheme, hostport);    /* https default 443 */
	} else {
		url = xasprintf("%s%s:%d", scheme, hostport, secstore_http_port(s));
	}
	free(hostport);
	return url;
}

/* Read newline-separated, non-blank lines of a file into out. */
static void read_lines(const char *path, strlist *out)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	char  *line = NULL;
	size_t cap = 0;
	ssize_t n;
	while ((n = getline(&line, &cap, f)) >= 0) {
		while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
		if (n > 0)
			strlist_push_copy(out, line);
	}
	free(line);
	fclose(f);
}

static void group_endpoints(secstore_t *s, const char *group, strlist *out)
{
	char *gdir = groups_dir(s);
	char *file = path_join(gdir, group);
	read_lines(file, out);
	free(file);
	free(gdir);
}

int cmd_groups(secstore_t *s, int argc, char **argv)
{
	char *gdir = groups_dir(s);
	strlist names;
	strlist_init(&names);

	if (argc >= 1) {
		strlist_push_copy(&names, argv[0]);     /* a specific group */
	} else {
		list_files_rel(gdir, &names);
		strlist_sort_unique(&names);
	}

	for (size_t i = 0; i < names.len; i++) {
		strlist eps;
		strlist_init(&eps);
		group_endpoints(s, names.items[i], &eps);
		for (size_t j = 0; j < eps.len; j++)
			printf("%s\t%s\n", names.items[i], eps.items[j]);
		strlist_free(&eps);
	}
	strlist_free(&names);
	free(gdir);
	return 0;
}

int cmd_group_add(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a group");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify an endpoint");
	secstore_validate_name(s, "group", argv[0]);

	char *gdir = groups_dir(s);
	mkdir_p(gdir);
	char *file = path_join(gdir, argv[0]);

	strlist eps;
	strlist_init(&eps);
	read_lines(file, &eps);
	for (size_t i = 0; i < eps.len; i++)
		if (strcmp(eps.items[i], argv[1]) == 0) {
			strlist_free(&eps); free(file); free(gdir);
			return 0;                            /* already present */
		}
	strlist_free(&eps);

	FILE *f = fopen(file, "a");
	if (!f)
		secstore_fatal(s, "cannot write group %s", argv[0]);
	fprintf(f, "%s\n", argv[1]);
	fclose(f);
	secstore_info(s, "added endpoint %s to group %s", argv[1], argv[0]);
	free(file);
	free(gdir);
	return 0;
}

int cmd_group_del(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a group");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify an endpoint");
	secstore_validate_name(s, "group", argv[0]);

	char *gdir = groups_dir(s);
	char *file = path_join(gdir, argv[0]);

	strlist eps;
	strlist_init(&eps);
	read_lines(file, &eps);

	int kept = 0;
	FILE *f = fopen(file, "w");
	if (f) {
		for (size_t i = 0; i < eps.len; i++)
			if (strcmp(eps.items[i], argv[1]) != 0) {
				fprintf(f, "%s\n", eps.items[i]);
				kept++;
			}
		fclose(f);
	}
	if (kept == 0)
		remove(file);                            /* drop the empty group file */
	strlist_free(&eps);
	secstore_info(s, "removed endpoint %s from group %s", argv[1], argv[0]);
	free(file);
	free(gdir);
	return 0;
}

/* Pull one store from one endpoint base URL. Clones if absent, else
 * pulls and auto-resolves conflicts by taking the remote side. */
static void pull_one(secstore_t *s, const char *base, const char *store)
{
	char *url = xasprintf("%s/app/secret/%s", base, store);
	char *dir = path_join(s->self_config, store);
	char *git = path_join(dir, ".git");

	if (!is_dir(git)) {
		secstore_info(s, "cloning %s from %s", store, url);
		char *clone[] = { "git", "clone", url, dir, NULL };
		proc_run(clone, NULL, NULL, 0, NULL, NULL, 0);
	} else {
		secstore_info(s, "pulling %s from %s", store, url);
		char  *branch = NULL;
		size_t bl = 0;
		char *bb[] = { "git", "branch", "--show-current", NULL };
		proc_run(bb, dir, NULL, 0, &branch, &bl, 1);
		if (branch) { while (bl && (branch[bl-1] == '\n' || branch[bl-1] == '\r')) branch[--bl] = '\0'; }
		const char *br = (branch && *branch) ? branch : "master";

		char *pull[10]; int a = 0;
		pull[a++] = "git"; pull[a++] = "pull";
		if (s->force[0]) pull[a++] = (char *)s->force;
		pull[a++] = "--no-edit"; pull[a++] = "--commit";
		pull[a++] = "--allow-unrelated-histories";
		pull[a++] = url; pull[a++] = (char *)br; pull[a] = NULL;
		proc_run(pull, dir, NULL, 0, NULL, NULL, 0);

		/* auto-resolve conflicts: take theirs */
		char *diff[] = { "git", "diff", "--name-only", "--diff-filter=U", NULL };
		char  *names = NULL;
		size_t nl = 0;
		if (proc_run(diff, dir, NULL, 0, &names, &nl, 1) == 0 && names && *names) {
			char *save = NULL;
			for (char *fn = strtok_r(names, "\n", &save); fn; fn = strtok_r(NULL, "\n", &save)) {
				char *co[] = { "git", "checkout", "--theirs", fn, NULL };
				proc_run(co, dir, NULL, 0, NULL, NULL, 1);
			}
			char *ci[] = { "git", "commit", "-a", "-m", "resolved merging conflicts", NULL };
			proc_run(ci, dir, NULL, 0, NULL, NULL, 1);
		}
		free(names);
		free(branch);
	}
	free(git); free(dir); free(url);
}

int cmd_pull_http(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a store");
	char *store = secstore_lower(argv[0]);
	secstore_validate_name(s, "store", store);

	/* Collect endpoints from the named group, or from every group. */
	strlist eps;
	strlist_init(&eps);
	if (argc >= 2 && argv[1] && *argv[1]) {
		group_endpoints(s, argv[1], &eps);
		if (eps.len == 0)
			secstore_warn(s, "group %s has no endpoints", argv[1]);
	} else {
		char *gdir = groups_dir(s);
		strlist groups;
		strlist_init(&groups);
		list_files_rel(gdir, &groups);
		for (size_t i = 0; i < groups.len; i++)
			group_endpoints(s, groups.items[i], &eps);
		strlist_free(&groups);
		free(gdir);
	}

	for (size_t i = 0; i < eps.len; i++) {
		char *base = endpoint_base_url(s, eps.items[i]);
		pull_one(s, base, store);
		free(base);
	}
	strlist_free(&eps);
	free(store);
	return 0;
}
