/*
 * secret.c — the secret(1) command-line front-end.
 *
 * A thin dispatcher over libsecstore: it parses the global -d/-q/-f
 * flags, applies the FEAT-205 "<store>/<param>" shortcut (a bare
 * existing parameter prints its value), then routes to the matching
 * subcommand. All real work lives in the library.
 */
#include <stdlib.h>
#include <string.h>

#include "secstore.h"
#include "internal.h"

int main(int argc, char **argv)
{
	secstore_t *s = secstore_new(argv[0]);

	/* Parse leading short flags (-d/-q/-f, bundling allowed), mirroring
	 * the getopts "dqf" loop. Stop at the first non-flag token. */
	int i = 1;
	for (; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] != '-' || a[1] == '\0')
			break;
		if (!strcmp(a, "--")) { i++; break; }
		for (const char *c = a + 1; *c; c++) {
			switch (*c) {
			case 'd': s->debug = 1; break;
			case 'q': s->quiet = 1; break;
			case 'f': s->force = "-f"; break;
			default:  /* unknown flag: ignore, as getopts does */ break;
			}
		}
	}

	int   rest_argc = argc - i;
	char **rest = argv + i;

	if (rest_argc < 1) {
		cmd_help(s, 0, NULL);
		secstore_free(s);
		return 0;
	}

	const char *cmd = rest[0];

	/* FEAT-205 dispatcher shortcut: `secret <store>/<param>` prints the
	 * value when that parameter exists, then exits 0. */
	if (store_param_has(s, cmd) == 0) {
		cmd_get(s, 1, (char *[]){ (char *)cmd });
		secstore_free(s);
		return 0;
	}

	secstore_cmd_fn fn = secstore_lookup(cmd);
	if (!fn)
		secstore_fatal(s, "unknown command %s", cmd);

	secstore_debug(s, "executing command:%s", cmd);
	int rc = fn(s, rest_argc - 1, rest + 1);

	secstore_free(s);
	return rc;
}
