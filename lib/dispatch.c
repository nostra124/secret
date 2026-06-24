/*
 * dispatch.c — the subcommand name table.
 *
 * Single source of truth for which verbs exist, shared by the CLI
 * dispatcher (src/secret.c) and by `help` (which needs to know whether a
 * given word is a real command).
 */
#include <string.h>

#include "internal.h"

static const struct {
	const char     *name;
	secstore_cmd_fn fn;
} table[] = {
	{ "help",        cmd_help        },
	{ "version",     cmd_version     },
	{ "identity",    cmd_identity    },
	{ "setup",       cmd_setup       },
	{ "skills",      cmd_skills      },
	{ "stores",      cmd_stores      },
	{ "params",      cmd_params      },
	{ "init",        cmd_init        },
	{ "pass-init",   cmd_pass_init   },
	{ "exists",      cmd_exists      },
	{ "destroy",     cmd_destroy     },
	{ "clean",       cmd_clean       },
	{ "ls",          cmd_ls          },
	{ "has",         cmd_has         },
	{ "set",         cmd_set         },
	{ "get",         cmd_get         },
	{ "del",         cmd_del         },
	{ "qr",          cmd_qr          },
	{ "gen",         cmd_gen         },
	{ "def",         cmd_def         },
	{ "ins",         cmd_ins         },
	{ "rem",         cmd_rem         },
	{ "edit",        cmd_edit        },
	{ "remotes",     cmd_remotes     },
	{ "pull",        cmd_pull        },
	{ "push",        cmd_push        },
	{ "sync",        cmd_sync        },
	{ "gpg-keys",    cmd_gpg_keys    },
	{ "add-gpg-key", cmd_add_gpg_key },
	{ "del-gpg-key", cmd_del_gpg_key },
	{ "has-gpg-key", cmd_has_gpg_key },
	{ "sources",     cmd_sources     },
	{ "export",      cmd_export      },
	{ "import",      cmd_import      },
	{ "templates",   cmd_templates   },
	{ "new",         cmd_new         },
	{ "otp",         cmd_otp         },
	{ NULL,          NULL            },
};

secstore_cmd_fn secstore_lookup(const char *name)
{
	for (int i = 0; table[i].name; i++)
		if (strcmp(table[i].name, name) == 0)
			return table[i].fn;
	return NULL;
}
