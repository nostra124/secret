/*
 * template.c — structured entry templates.
 *
 * A template is a named schema: an ordered set of fields. `secret new`
 * materialises an entry from a template by storing each field as its own
 * parameter (`<entry>/<field>`), so templates compose with every existing
 * verb (get/set/params/ls/export/...). A template carries no persistent
 * state on the store — it just defines which parameters an entry has.
 *
 * Extension templates (mfa, passkey) work both standalone and attached to
 * a base entry; when attached (`--attach`) their fields live under
 * `<entry>/<template>/<field>`.
 *
 * `secret otp` reads an mfa entry's fields and prints the current TOTP
 * code by shelling out to oathtool(1).
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

/* field flags */
#define TF_SECRET 1u   /* sensitive (not echoed when prompting)        */
#define TF_GEN    2u   /* auto-generate a random value when omitted    */
#define TF_OPT    4u   /* optional: skip silently when omitted         */
#define TF_MULTI  8u   /* value may be multiline (informational)       */

typedef struct { const char *name; const char *def; unsigned flags; } tfield;

typedef struct {
	const char  *name;
	const char  *desc;
	int          extension;   /* 1 if attachable to a base entry */
	const tfield *fields;
	int           nfields;
} template_t;

/* ---- built-in templates -------------------------------------------- */

static const tfield f_login[] = {
	{ "username", NULL, 0 },
	{ "password", NULL, TF_SECRET | TF_GEN },
	{ "url",      NULL, TF_OPT },
	{ "notes",    NULL, TF_OPT | TF_MULTI },
};
static const tfield f_wifi[] = {
	{ "ssid",     NULL,   0 },
	{ "password", NULL,   TF_SECRET },
	{ "security", "wpa2", 0 },
	{ "hidden",   "false",0 },
	{ "identity", NULL,   TF_OPT },        /* WPA-Enterprise */
};
static const tfield f_mfa[] = {
	{ "secret",    NULL,  TF_SECRET },     /* base32 TOTP seed */
	{ "issuer",    NULL,  TF_OPT },
	{ "account",   NULL,  TF_OPT },
	{ "algorithm", "sha1",0 },
	{ "digits",    "6",   0 },
	{ "period",    "30",  0 },
};
static const tfield f_passkey[] = {
	{ "rp_id",         NULL,   0 },        /* relying-party domain */
	{ "user_name",     NULL,   TF_OPT },
	{ "user_handle",   NULL,   TF_OPT },
	{ "credential_id", NULL,   TF_OPT },
	{ "private_key",   NULL,   TF_SECRET | TF_MULTI },
	{ "algorithm",     "es256",0 },
	{ "sign_count",    "0",    0 },
};
static const tfield f_wallet[] = {
	{ "mnemonic",   NULL,           TF_SECRET | TF_MULTI },
	{ "passphrase", NULL,           TF_SECRET | TF_OPT },
	{ "derivation", "m/44'/0'/0'",  0 },   /* comma-separated paths ok */
	{ "network",    "mainnet",      0 },
	{ "label",      NULL,           TF_OPT },
};

#define T(arr) arr, (int)(sizeof(arr)/sizeof((arr)[0]))
static const template_t templates[] = {
	{ "login",   "username / password / url / notes",       0, T(f_login)   },
	{ "wifi",    "Wi-Fi network credentials",                0, T(f_wifi)    },
	{ "mfa",     "TOTP multi-factor secret (oathtool)",      1, T(f_mfa)     },
	{ "passkey", "WebAuthn passkey backup fields",           1, T(f_passkey) },
	{ "wallet",  "crypto wallet mnemonic + derivation",      0, T(f_wallet)  },
};
static const int ntemplates = (int)(sizeof(templates)/sizeof(templates[0]));

static const template_t *template_lookup(const char *name)
{
	for (int i = 0; i < ntemplates; i++)
		if (strcmp(templates[i].name, name) == 0)
			return &templates[i];
	return NULL;
}

/* ---- helpers ------------------------------------------------------- */

static void gen_secret(char *buf, int n)
{
	static const char alpha[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	FILE *ur = fopen("/dev/urandom", "rb");
	for (int i = 0; i < n; i++) {
		unsigned char c = 0;
		if (!ur || fread(&c, 1, 1, ur) != 1)
			c = (unsigned char)(i * 7 + 3);
		buf[i] = alpha[c % 62];
	}
	buf[n] = '\0';
	if (ur) fclose(ur);
}

/* Find a `name=value` argument; returns the value or NULL. */
static const char *arg_value(int argc, char **argv, const char *name)
{
	size_t nl = strlen(name);
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], name, nl) == 0 && argv[i][nl] == '=')
			return argv[i] + nl + 1;
	}
	return NULL;
}

/* Read a scalar field value, trimmed of a trailing newline. NULL if the
 * field parameter does not exist. Caller frees. */
static char *read_field(secstore_t *s, const char *store, const char *base,
                        const char *name)
{
	char  *param = xasprintf("%s/%s", base, name);
	char  *val = NULL;
	size_t len = 0;
	int rc = store_get_value(s, store, param, &val, &len);
	free(param);
	if (rc != 0) { free(val); return NULL; }
	while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r' ||
	                   val[len - 1] == ' '  || val[len - 1] == '\t'))
		val[--len] = '\0';
	return val;
}

/* ---- commands ------------------------------------------------------ */

int cmd_templates(secstore_t *s, int argc, char **argv)
{
	if (argc >= 1) {
		const template_t *t = template_lookup(argv[0]);
		if (!t)
			secstore_fatal(s, "unknown template %s", argv[0]);
		printf("%s — %s%s\n", t->name, t->desc,
		       t->extension ? " [extension]" : "");
		for (int i = 0; i < t->nfields; i++) {
			const tfield *f = &t->fields[i];
			printf("  %-14s", f->name);
			if (f->flags & TF_SECRET) printf(" [secret]");
			if (f->flags & TF_GEN)    printf(" [generated]");
			if (f->flags & TF_OPT)    printf(" [optional]");
			if (f->flags & TF_MULTI)  printf(" [multiline]");
			if (f->def)               printf(" (default: %s)", f->def);
			printf("\n");
		}
		return 0;
	}

	for (int i = 0; i < ntemplates; i++)
		printf("%-9s %s%s\n", templates[i].name, templates[i].desc,
		       templates[i].extension ? " [extension]" : "");
	return 0;
}

int cmd_new(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a template");
	if (argc < 2 || !argv[1] || !*argv[1])
		secstore_fatal(s, "please specify in the format <store>/<entry>");

	const template_t *t = template_lookup(argv[0]);
	if (!t)
		secstore_fatal(s, "unknown template %s — see '%s templates'", argv[0], s->self);

	char *store = NULL, *entry = NULL;
	parse_param(s, argv[1], 1, &store, &entry);

	/* Options after <store>/<entry>: --attach and field=value pairs. */
	int   attach = 0;
	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--attach"))
			attach = 1;
		else if (strchr(argv[i], '=') == NULL)
			secstore_fatal(s, "unknown option %s (expected field=value or --attach)",
			               argv[i]);
	}
	if (attach && !t->extension)
		secstore_fatal(s, "template %s is not an extension; --attach not allowed", t->name);

	char *base = attach ? xasprintf("%s/%s", entry, t->name) : xstrdup(entry);

	/* Auto-init the store if it does not exist yet (matches `gen`). */
	char *dir = store_dir(s, store);
	if (!is_dir(dir))
		cmd_init(s, 1, (char *[]){ store });
	free(dir);

	int created = 0;
	for (int i = 0; i < t->nfields; i++) {
		const tfield *f = &t->fields[i];
		const char   *provided = arg_value(argc - 2, argv + 2, f->name);
		char         *value = NULL;
		int           owned = 0;

		if (provided) {
			value = (char *)provided;
		} else if (f->def) {
			value = (char *)f->def;
		} else if (f->flags & TF_GEN) {
			char buf[34];
			gen_secret(buf, 33);
			value = xstrdup(buf);
			owned = 1;
		} else if (f->flags & TF_OPT) {
			continue;                       /* skip optional, unset */
		} else if (isatty(STDIN_FILENO)) {
			fprintf(stderr, "%s%s: ", f->name,
			        (f->flags & TF_SECRET) ? " (hidden input not masked)" : "");
			char  *line = NULL;
			size_t cap = 0;
			ssize_t n = getline(&line, &cap, stdin);
			if (n < 0) { free(line); secstore_fatal(s, "field %s is required", f->name); }
			while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
			value = line; owned = 1;
		} else {
			secstore_fatal(s, "field %s is required (pass %s=value)", f->name, f->name);
		}

		char *param = xasprintf("%s/%s", base, f->name);
		store_set_value(s, store, param, value, strlen(value));
		free(param);
		if (owned) free(value);
		created++;
	}

	secstore_info(s, "created %s entry %s/%s (%d field%s)",
	              t->name, store, base, created, created == 1 ? "" : "s");
	free(base);
	free(store);
	free(entry);
	return 0;
}

int cmd_otp(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify in the format <store>/<entry>");
	if (!have_tool("oathtool"))
		secstore_fatal(s, "oathtool(1) missing — install your distro's 'oathtool' package");

	char *store = NULL, *entry = NULL;
	parse_param(s, argv[0], 1, &store, &entry);

	/* The mfa secret lives at <entry>/secret (standalone) or
	 * <entry>/mfa/secret (attached to a base entry). */
	char *base = xstrdup(entry);
	char *seed = read_field(s, store, base, "secret");
	if (!seed) {
		free(base);
		base = xasprintf("%s/mfa", entry);
		seed = read_field(s, store, base, "secret");
	}
	if (!seed)
		secstore_fatal(s, "no MFA secret for %s/%s — create one with '%s new mfa ...'",
		               store, entry, s->self);

	char *algo   = read_field(s, store, base, "algorithm");
	char *digits = read_field(s, store, base, "digits");
	char *period = read_field(s, store, base, "period");

	char *totp = xasprintf("--totp=%s", algo ? algo : "sha1");
	char *dig  = xasprintf("--digits=%s", digits ? digits : "6");
	char *step = xasprintf("--time-step-size=%ss", period ? period : "30");
	char *oa[] = { "oathtool", totp, "-b", dig, step, seed, NULL };

	char  *out = NULL;
	size_t olen = 0;
	int rc = proc_run(oa, NULL, NULL, 0, &out, &olen, 0);
	if (rc == 0 && out)
		fputs(out, stdout);

	free(out); free(step); free(dig); free(totp);
	free(period); free(digits); free(algo);
	free(seed); free(base); free(entry); free(store);
	return rc == 0 ? 0 : 1;
}
