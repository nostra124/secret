/*
 * help.c — `help` and `help <subcommand>`.
 *
 * The top-level usage text mirrors bin/secret's here-doc (printed to
 * stderr), with the FEAT-era `skills` verb added to the generic group.
 * Per-subcommand help exists for init / setup / identity.
 */
#include <stdio.h>
#include <string.h>

#include "internal.h"

static void print_general(secstore_t *s)
{
	const char *self = s->self;
	fprintf(stderr,
"usage: %s <options> <command>\n"
"\n"
"%s implements a simple secret management as user and system wide.\n"
"\n"
"The %s entries are stored as files in a storage directory. Each %s\n"
"parameter is identified by an hierarchical id separated by colons ':'\n"
"(e.g. host:name).\n"
"\n"
"The special store \"password-store\" is handled via the pass(1) script.\n"
"\n"
"options:\n"
"   -d                            enable debug mode\n"
"   -q                            enable quiet mode\n"
"   -f                            enable force mode (push and pull)\n"
"\n"
"generic commands:\n"
"   help                          print this help message\n"
"   help <command>                prints command specific help.\n"
"   version                       print the version of %s\n"
"   identity                      show the main GPG identity\n"
"   setup                         central setup — GPG identity, git config, keys\n"
"   skills [name]                 list skill files, or print one by name\n"
"\n"
"store management commands\n"
"   stores                        show the existing stores\n"
"   params <store>                show the parameters of the store\n"
"   init [store]                  initializes the store or bootstrap GPG identity\n"
"   pass-init                     bootstraps the pass(1) password-store\n"
"   exists <store>                check if the store <store> exists\n"
"   destroy <store>               destroys the store <store>\n"
"   clean <store>                 cleans empty parameters in store <store>\n"
"\n"
"store parameter commands:\n"
"   ls [store]                    lists all parameters in store <store>\n"
"   has <store>/<param>           checks if the parameter <param> exists\n"
"   set <store>/<param>           set parameter <store>/<param> from stdin\n"
"   get <store>/<param>           get parameter <store>/<param> to stdout\n"
"   del <store>/<param>           delete param <param> in store <store>\n"
"   qr  <store>/<param>           QR-encode a param (or a wifi/mfa entry)\n"
"   clip <store>/<param>          copy a param to the clipboard (auto-clears)\n"
"   gen <store>/<param>           generates a random and prints to stdout\n"
"   def <store>/<param> <value>   creates a default with a default value\n"
"   ins <store>/<param>           inserts the line into param from stdin\n"
"   rem <store>/<param>           removes the line from param from stdin\n"
"   edit <store>/<param>          edit the param <param> in store <store>\n"
"\n"
"synchronisation commands:\n"
"   remotes [stores]              lists all remotes of the stores\n"
"   pull [store] [account]        pulls the store from account (ssh)\n"
"   push [store] [account]        pushes the store to account (ssh)\n"
"   sync [account]                syncs all stores with the account\n"
"   serve                         serve stores for HTTP pull (/app/secret)\n"
"   port                          print the uid-derived HTTP port\n"
"   groups [group]                list sharing groups and their endpoints\n"
"   group-add <group> <endpoint>  add an http(s) endpoint to a group\n"
"   group-del <group> <endpoint>  remove an endpoint from a group\n"
"   pull-http <store> [group]     pull a store from a group over HTTP\n"
"\n"
"account encryption commands\n"
"   gpg-keys [store] [param]      lists all gpg-keys of the store or param\n"
"   add-gpg-key <store> <account> adds the gpg-key to the store\n"
"   del-gpg-key <store> <account> deletes the gpg-key from the store\n"
"   has-gpg-key <store> <account> checks if the store has the gpg-key\n"
"\n"
"data transfer commands\n"
"   sources                       list import/export sources and availability\n"
"   export <source> <store>       push a store's parameters to <source>\n"
"   import <source> <store>       pull <source>'s items into <store>\n"
"\n"
"template commands\n"
"   templates [name]              list entry templates, or show one's fields\n"
"   new <template> <store>/<name> create a structured entry from a template\n"
"   otp <store>/<name> [-c]       print (or -c copy) the current TOTP code\n"
"\n",
		self, self, self, self, self);
}

static void help_init(secstore_t *s)
{
	fprintf(stderr,
"\n"
"usage: %s init [store]\n"
"\n"
"When called without a store, ensures a GPG identity exists and then\n"
"re-initialises every existing store.\n"
"\n"
"When called with a store, creates %s/<store> as a git repository,\n"
"imports the current account's GPG public key, and re-encrypts every\n"
"parameter for the current recipient set.\n"
"\n"
"Examples:\n"
"\n"
"    %s init                     # bootstrap GPG identity + re-init all stores\n"
"    %s init bitcoin             # create or re-initialise a specific store\n",
		s->self, s->self_config, s->self, s->self);
}

static void help_setup(secstore_t *s)
{
	fprintf(stderr,
"\n"
"usage: %s setup\n"
"\n"
"One-shot central setup: ensure a GPG identity exists, export the public\n"
"key to ~/.config/account/gpg/ for interoperability with account(1), and\n"
"create the secret store root.\n"
"\n"
"Run this once on a new machine before using %s.\n",
		s->self, s->self);
}

static void help_identity(secstore_t *s)
{
	fprintf(stderr,
"\n"
"usage: %s identity\n"
"\n"
"Print the main GPG identity. Exits non-zero if no identity has been set\n"
"up yet.\n",
		s->self);
}

int cmd_help(secstore_t *s, int argc, char **argv)
{
	if (argc < 1) {
		print_general(s);
		return 0;
	}

	const char *name = argv[0];
	if (!secstore_lookup(name)) {
		fprintf(stderr, "'%s' is not an %s command\n\nplease see '%s help'\n",
		        name, s->self, s->self);
		return 1;
	}

	/* Subcommands with dedicated help. Any trailing option is rejected,
	 * matching the shell's help:<cmd> guard. */
	int has_topic =
		!strcmp(name, "init") || !strcmp(name, "setup") ||
		!strcmp(name, "identity");

	if (!has_topic) {
		fprintf(stderr, "No help found for '%s'\n\nplease see '%s help'\n",
		        name, s->self);
		return 0;
	}

	if (argc > 1)
		secstore_fatal(s, "unknown option %s", argv[1]);

	if (!strcmp(name, "init"))     help_init(s);
	else if (!strcmp(name, "setup"))    help_setup(s);
	else                                help_identity(s);
	return 0;
}
