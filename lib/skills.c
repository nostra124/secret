/*
 * skills.c — the `skills` command plus the trivial `version` / `identity`
 * commands.
 *
 * `skills` lists the skill files shipped under skills/ (a symlink to
 * .rpk/skills in the dev tree, $PREFIX/share/<self>/skills once
 * installed); with a name argument it prints that skill's content. These
 * are primarily consumed by coding agents loading domain knowledge.
 */
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static char *resolve_skills_dir(secstore_t *s)
{
	char *candidates[3];
	candidates[0] = path_join(s->exe_dir, "../skills");
	candidates[1] = path_join(s->exe_dir, "../.rpk/skills");
	candidates[2] = xasprintf("%s/../share/%s/skills", s->exe_dir, s->self);

	char *result = NULL;
	for (int i = 0; i < 3; i++) {
		if (!result && is_dir(candidates[i]))
			result = xstrdup(candidates[i]);
		free(candidates[i]);
	}
	return result;
}

int cmd_skills(secstore_t *s, int argc, char **argv)
{
	char *dir = resolve_skills_dir(s);
	if (!dir)
		secstore_fatal(s, "no skills directory found");

	if (argc < 1) {
		strlist names;
		strlist_init(&names);
		DIR *d = opendir(dir);
		if (d) {
			struct dirent *e;
			while ((e = readdir(d))) {
				size_t n = strlen(e->d_name);
				if (n > 3 && strcmp(e->d_name + n - 3, ".md") == 0) {
					char *name = xstrdup(e->d_name);
					name[n - 3] = '\0';
					strlist_push(&names, name);
				}
			}
			closedir(d);
		}
		strlist_sort_unique(&names);
		for (size_t i = 0; i < names.len; i++)
			puts(names.items[i]);
		strlist_free(&names);
		free(dir);
		return 0;
	}

	char *file = xasprintf("%s/%s.md", dir, argv[0]);
	if (!is_file(file)) {
		char *name = xstrdup(argv[0]);
		free(file);
		free(dir);
		secstore_fatal(s, "unknown skill %s", name);
	}
	FILE *f = fopen(file, "r");
	if (f) {
		char   buf[4096];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			fwrite(buf, 1, n, stdout);
		fclose(f);
	}
	free(file);
	free(dir);
	return 0;
}

int cmd_version(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	char *v = secstore_version(s);
	puts(v);
	free(v);
	return 0;
}

int cmd_identity(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	char *id = secstore_identity(s);
	if (!id || !*id) {
		free(id);
		secstore_fatal(s, "no GPG identity — run '%s setup' first", s->self);
	}
	puts(id);
	free(id);
	return 0;
}
