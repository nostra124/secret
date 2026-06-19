/*
 * crypto.c — direct gpg(1) invocation for key handling and encryption.
 *
 * These wrap the same gpg calls bin/secret made, keeping the on-disk
 * ciphertext format identical so peers can interoperate. We never
 * delegate to crypt/account (CLAUDE.md §5).
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* Parse `gpg --with-colons --fingerprint --list-keys` output, emitting
 * the fingerprint of every key whose uid e-mail matches `key`. */
void gpg_fingerprint(secstore_t *s, const char *key, strlist *out)
{
	(void)s;
	if (!key || !*key)
		return;
	char *argv[] = {
		"gpg", "--batch", "--with-colons", "--fingerprint",
		"--list-keys", NULL
	};
	char  *data = NULL;
	size_t len = 0;
	if (proc_run(argv, NULL, NULL, 0, &data, &len, 1) != 0 || !data) {
		free(data);
		return;
	}

	char *cur_fpr = NULL;
	char *save = NULL;
	for (char *line = strtok_r(data, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		/* field 0 is the record type; field 9 the fpr / uid text. */
		char *fields[12] = { 0 };
		int   nf = 0;
		char *p = line;
		fields[nf++] = p;
		for (; *p && nf < 12; p++) {
			if (*p == ':') {
				*p = '\0';
				fields[nf++] = p + 1;
			}
		}
		if (nf < 1)
			continue;
		if (strcmp(fields[0], "fpr") == 0 && nf > 9) {
			free(cur_fpr);
			cur_fpr = xstrdup(fields[9]);
		} else if (strcmp(fields[0], "uid") == 0 && nf > 9 && cur_fpr) {
			/* Extract the e-mail the same way the shell did:
			 * `sed 's/.*<//g' | sed 's/>//g'` — take everything
			 * after the last '<' (or the whole uid when there is
			 * none, e.g. a bare "user@host"), then drop any '>'. */
			const char *uid = fields[9];
			const char *start = strrchr(uid, '<');
			start = start ? start + 1 : uid;
			char *email = xmalloc(strlen(start) + 1);
			char *w = email;
			for (const char *r = start; *r; r++)
				if (*r != '>')
					*w++ = *r;
			*w = '\0';
			if (strcmp(email, key) == 0)
				strlist_push_copy(out, cur_fpr);
			free(email);
		}
	}
	free(cur_fpr);
	free(data);
	strlist_sort_unique(out);
}

char *gpg_export_public_key(secstore_t *s, const char *key)
{
	char *id = NULL;
	if (!key || !*key) {
		id = secstore_identity(s);
		key = id;
	}
	if (!key) {
		free(id);
		return xstrdup("");
	}

	char *cfg = account_config_dir(s);
	char *pub = xasprintf("%s/gpg/%s.pub", cfg, key);
	char *result = NULL;
	if (is_file(pub)) {
		FILE *f = fopen(pub, "r");
		if (f) {
			size_t cap = 4096, len = 0;
			char  *buf = xmalloc(cap);
			size_t n;
			while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
				len += n;
				if (len == cap) { cap *= 2; buf = xrealloc(buf, cap); }
			}
			fclose(f);
			buf[len] = '\0';
			result = buf;
		}
	}
	if (!result) {
		char *argv[] = {
			"gpg", "--armor", "--quiet", "--export", (char *)key, NULL
		};
		size_t outlen = 0;
		proc_run(argv, NULL, NULL, 0, &result, &outlen, 1);
	}
	free(pub);
	free(cfg);
	free(id);
	return result ? result : xstrdup("");
}

void gpg_import_public_key(secstore_t *s, const char *key,
                           const char *data, size_t len)
{
	if (!key || !*key)
		secstore_fatal(s, "please specify a key id");

	char *cfg = account_config_dir(s);
	char *gpgdir = path_join(cfg, "gpg");
	mkdir_p(gpgdir);
	char *pub = xasprintf("%s/%s.pub", gpgdir, key);
	FILE *f = fopen(pub, "w");
	if (f) {
		if (data && len)
			fwrite(data, 1, len, f);
		fclose(f);
	}

	char *import[] = { "gpg", "--import", pub, NULL };
	proc_run(import, NULL, NULL, 0, NULL, NULL, 1);

	strlist fprs;
	strlist_init(&fprs);
	gpg_fingerprint(s, key, &fprs);
	for (size_t i = 0; i < fprs.len; i++) {
		char *sign[] = {
			"gpg", "--no-tty", "--quick-sign-key", fprs.items[i], NULL
		};
		proc_run(sign, NULL, NULL, 0, NULL, NULL, 1);
	}
	strlist_free(&fprs);

	free(pub);
	free(gpgdir);
	free(cfg);
}

int gpg_encrypt(secstore_t *s, char **recipients, size_t nrec,
                const char *in, size_t inlen, char **out, size_t *outlen)
{
	/* Resolve the recipient identities to fingerprints. With no
	 * recipients, fall back to every known account (as the shell did). */
	strlist ids;
	strlist_init(&ids);
	if (nrec > 0) {
		for (size_t i = 0; i < nrec; i++)
			strlist_push_copy(&ids, recipients[i]);
	} else {
		account_list(s, &ids);
	}

	strlist fprs;
	strlist_init(&fprs);
	for (size_t i = 0; i < ids.len; i++)
		gpg_fingerprint(s, ids.items[i], &fprs);
	strlist_free(&ids);

	/* argv: gpg --yes --encrypt --armor --quiet --trust-model always
	 *       --output - [--recipient FPR]... */
	size_t base = 9;
	size_t argc = base + fprs.len * 2 + 1;
	char **argv = xmalloc(argc * sizeof(*argv));
	size_t a = 0;
	argv[a++] = "gpg";
	argv[a++] = "--yes";
	argv[a++] = "--encrypt";
	argv[a++] = "--armor";
	argv[a++] = "--quiet";
	argv[a++] = "--trust-model";
	argv[a++] = "always";
	argv[a++] = "--output";
	argv[a++] = "-";
	for (size_t i = 0; i < fprs.len; i++) {
		argv[a++] = "--recipient";
		argv[a++] = fprs.items[i];
	}
	argv[a] = NULL;

	int rc = proc_run(argv, NULL, in, inlen, out, outlen, 1);

	free(argv);
	strlist_free(&fprs);
	return rc;
}

int gpg_decrypt(secstore_t *s, const char *in, size_t inlen,
                char **out, size_t *outlen)
{
	(void)s;
	char *argv[] = {
		"gpg", "--yes", "--decrypt", "--armor", "--quiet", NULL
	};
	return proc_run(argv, NULL, in, inlen, out, outlen, 1);
}
