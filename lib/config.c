/*
 * config.c — runtime context, version and identity resolution.
 *
 * SELF_CONFIG resolution mirrors bin/secret lines 234-254:
 *   * $XDG_SECRET_STORES, if set, wins verbatim (the bats escape hatch);
 *   * else /etc/<self> when running as root (EUID 0);
 *   * else $XDG_CONFIG_HOME/<self> (creating it).
 */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

static char *dirname_of(const char *path)
{
	char *copy = xstrdup(path);
	char *slash = strrchr(copy, '/');
	if (!slash) {
		free(copy);
		return xstrdup(".");
	}
	if (slash == copy)
		slash[1] = '\0';   /* keep the root "/" */
	else
		*slash = '\0';
	return copy;
}

static char *basename_of(const char *path)
{
	const char *slash = strrchr(path, '/');
	return xstrdup(slash ? slash + 1 : path);
}

/* Absolute directory containing the running binary. Prefer
 * /proc/self/exe; fall back to the realpath of argv[0]'s directory. */
static char *resolve_exe_dir(const char *argv0)
{
	char buf[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n > 0) {
		buf[n] = '\0';
		return dirname_of(buf);
	}
	char *dir = dirname_of(argv0);
	char  real[PATH_MAX];
	if (realpath(dir, real)) {
		free(dir);
		return xstrdup(real);
	}
	return dir;
}

secstore_t *secstore_new(const char *argv0)
{
	secstore_t *s = xmalloc(sizeof(*s));
	memset(s, 0, sizeof(*s));

	s->self     = basename_of(argv0);
	s->exe_dir  = resolve_exe_dir(argv0);
	s->euid     = (int)geteuid();

	const char *home = getenv("HOME");
	s->home = xstrdup(home ? home : "");

	const char *xdg = getenv("XDG_CONFIG_HOME");
	s->xdg_config_home = (xdg && *xdg)
		? xstrdup(xdg)
		: path_join(s->home, ".config");

	s->debug = getenv("SELF_DEBUG") != NULL;
	s->quiet = getenv("SELF_QUIET") != NULL;
	s->force = getenv("SELF_FORCE") ? "-f" : "";

	const char *stores = getenv("XDG_SECRET_STORES");
	if (stores && *stores) {
		s->self_config = xstrdup(stores);
	} else if (s->euid == 0) {
		s->self_config = path_join("/etc", s->self);
		mkdir_p(s->self_config);
	} else {
		s->self_config = path_join(s->xdg_config_home, s->self);
		mkdir_p(s->self_config);
	}

	return s;
}

void secstore_free(secstore_t *s)
{
	if (!s)
		return;
	free(s->self);
	free(s->self_config);
	free(s->home);
	free(s->xdg_config_home);
	free(s->exe_dir);
	free(s);
}

static char *read_trimmed(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char   *buf = NULL;
	size_t  cap = 0, len = 0;
	int     c;
	while ((c = fgetc(f)) != EOF) {
		if (len + 1 >= cap) {
			cap = cap ? cap * 2 : 64;
			buf = xrealloc(buf, cap);
		}
		buf[len++] = (char)c;
	}
	fclose(f);
	if (!buf)
		return xstrdup("");
	while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' ||
	                   buf[len - 1] == ' '  || buf[len - 1] == '\t'))
		len--;
	buf[len] = '\0';
	return buf;
}

char *secstore_version(const secstore_t *s)
{
	char *candidates[3];
	candidates[0] = path_join(s->exe_dir, "../VERSION");
	candidates[1] = path_join(s->exe_dir, "../.rpk/version");
	candidates[2] = xasprintf("%s/../share/%s/version", s->exe_dir, s->self);

	char *result = NULL;
	for (int i = 0; i < 3 && !result; i++) {
		char *v = read_trimmed(candidates[i]);
		if (v && *v)
			result = v;
		else
			free(v);
	}
	for (int i = 0; i < 3; i++)
		free(candidates[i]);

	return result ? result : xstrdup("0.8");
}

char *secstore_identity(const secstore_t *s)
{
	(void)s;
	/* user part: effective uid's name, falling back to $USER. */
	char         *user = NULL;
	struct passwd *pw = getpwuid(geteuid());
	if (pw && pw->pw_name)
		user = xstrdup(pw->pw_name);
	else if (getenv("USER"))
		user = xstrdup(getenv("USER"));
	if (!user || !*user) {
		free(user);
		return NULL;
	}

	/* host part: `hostname -f`, lowercased; fall back to gethostname. */
	char *host = NULL;
	char *hn_argv[] = { "hostname", "-f", NULL };
	char *out = NULL;
	size_t outlen = 0;
	if (proc_run(hn_argv, NULL, NULL, 0, &out, &outlen, 1) == 0 && out) {
		size_t e = outlen;
		while (e > 0 && (out[e - 1] == '\n' || out[e - 1] == '\r'))
			e--;
		out[e] = '\0';
		if (*out)
			host = xstrdup(out);
	}
	free(out);
	if (!host) {
		char hb[256];
		if (gethostname(hb, sizeof(hb)) == 0)
			host = xstrdup(hb);
	}
	if (!host || !*host) {
		free(user);
		free(host);
		return NULL;
	}

	char *lower_host = secstore_lower(host);
	char *id = xasprintf("%s@%s", user, lower_host);
	free(user);
	free(host);
	free(lower_host);
	return id;
}

void secstore_validate_name(const secstore_t *s, const char *kind, const char *name)
{
	if (has_dotdot(name))
		secstore_fatal(s, "%s name may not contain '..' (path traversal): %s",
		               kind, name);
}
