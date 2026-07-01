/*
 * param.c — store-parameter commands:
 *   set, get, del, qr, gen, def, ins, rem, edit
 * plus the shared parse_param() helper that turns a raw
 * "<store>/<param>" (or "<store>/<a>:<b>") argument into its components.
 */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

/* ---- argument parsing ---------------------------------------------- */

int parse_param(secstore_t *s, const char *raw, int require_slash,
                char **store, char **param)
{
	if (!raw || !*raw)
		secstore_fatal(s, "please specify a parameter");

	char *work = secstore_lower(raw);
	str_translate(work, ':', '/');          /* host:name -> host/name */
	secstore_validate_name(s, "parameter", work);

	if (strchr(raw, '/') == NULL) {
		free(work);
		if (require_slash)
			secstore_fatal(s, "please specify in the format <store>/<param>");
		return -1;                          /* soft: no separator      */
	}

	char *slash = strchr(work, '/');
	*store = xstrdup(work);
	(*store)[slash - work] = '\0';
	*param = xstrdup(slash + 1);
	free(work);
	return 0;
}

/* ---- small io helpers ---------------------------------------------- */

static void read_stdin(char **buf, size_t *len)
{
	size_t cap = 4096, n = 0;
	char  *b = xmalloc(cap);
	ssize_t r;
	while ((r = read(STDIN_FILENO, b + n, cap - n)) > 0) {
		n += (size_t)r;
		if (n == cap) { cap *= 2; b = xrealloc(b, cap); }
	}
	b[n] = '\0';
	*buf = b;
	*len = n;
}

static void require_gpg_identity(secstore_t *s)
{
	char *id = secstore_identity(s);
	if (!id || !*id)
		secstore_warn(s, "no GPG identity — run '%s setup' to configure one", s->self);
	free(id);
}

/* Apply 0644/0755 to a store subtree when running as root (parity with
 * the EUID branch of the shell). */
static void fix_perms(secstore_t *s, const char *dir)
{
	if (s->euid != 0)
		return;
	char *chmod_d[] = { "chmod", "-R", "u=rwX,go=rX", (char *)dir, NULL };
	proc_run_quiet(chmod_d, NULL);
}

/* Commit one parameter file inside its store. */
static void git_commit_param(secstore_t *s, const char *dir,
                             const char *relfile, const char *msg)
{
	(void)s;
	char *add[]    = { "git", "add", "-f", (char *)relfile, NULL };
	char *commit[] = { "git", "commit", "-m", (char *)msg, (char *)relfile, NULL };
	proc_run_quiet(add, dir);
	proc_run_quiet(commit, dir);
}

/* ---- core get / set (used by gen / def / qr / ins / rem) ----------- */

/* Decrypt <store>/<param> into *out. Returns 0 on success, -1 if the
 * parameter file is absent. (Exposed to the source providers.) */
int store_get_value(secstore_t *s, const char *store, const char *param,
                     char **out, size_t *outlen)
{
	if (strcmp(store, "password-store") == 0) {
		char *pass[] = { "pass", (char *)param, NULL };
		return proc_run(pass, NULL, NULL, 0, out, outlen, 0) == 0 ? 0 : -1;
	}
	char *file = xasprintf("%s/%s/%s.gpg", s->self_config, store, param);
	if (!path_exists(file)) {
		free(file);
		return -1;
	}
	char  *ct = NULL;
	size_t ctlen = 0;
	read_file(file, &ct, &ctlen);
	free(file);
	int rc = gpg_decrypt(s, ct, ctlen, out, outlen);
	free(ct);
	return rc == 0 ? 0 : -1;
}

/* Encrypt `value` for `store`'s recipients and commit it. (Exposed to
 * the source providers.) */
int store_set_value(secstore_t *s, const char *store, const char *param,
                     const char *value, size_t vlen)
{
	if (strcmp(store, "password-store") == 0) {
		/* pass insert reads the value from stdin (-m/--multiline) and
		 * stores it under the parameter name, overwriting any existing
		 * entry (-f/--force) so `set` is idempotent. The entry name must
		 * be `param` (not a commit message) so `pass show <param>` and
		 * `secret get password-store/<param>` address the same file. */
		char *pass[] = { "pass", "insert", "-m", "-f", (char *)param, NULL };
		return proc_run(pass, NULL, value, vlen, NULL, NULL, 0);
	}

	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir)) {
		free(dir);
		secstore_fatal(s, "store %s does not exist", store);
	}

	strlist recips;
	strlist_init(&recips);
	store_recipients(s, store, &recips);

	char  *ct = NULL;
	size_t ctlen = 0;
	gpg_encrypt(s, recips.items, recips.len, value, vlen, &ct, &ctlen);
	strlist_free(&recips);

	/* Create any intermediate directories the param path implies. */
	char *rel = xasprintf("%s.gpg", param);
	char *file = path_join(dir, rel);
	char *slash = strrchr(file, '/');
	if (slash) {
		*slash = '\0';
		mkdir_p(file);
		*slash = '/';
	}
	write_file(file, ct, ctlen);
	free(ct);

	char *msg = xasprintf("set %s value %s", s->self, param);
	git_commit_param(s, dir, rel, msg);
	free(msg);
	fix_perms(s, dir);

	free(file);
	free(rel);
	free(dir);
	return 0;
}

/* ---- commands ------------------------------------------------------ */

int cmd_set(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);
	require_gpg_identity(s);

	char  *in = NULL;
	size_t inlen = 0;
	read_stdin(&in, &inlen);
	int rc = store_set_value(s, store, param, in, inlen);

	free(in);
	free(store);
	free(param);
	return rc;
}

int cmd_get(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);
	require_gpg_identity(s);

	int rc;
	if (strcmp(store, "password-store") == 0) {
		char *pass[] = { "pass", param, NULL };
		rc = proc_run_tty(pass, NULL);
	} else {
		char *dir = path_join(s->self_config, store);
		if (!is_dir(dir)) {
			free(dir);
			secstore_fatal(s, "store %s does not exist", store);
		}
		free(dir);
		char  *plain = NULL;
		size_t plen = 0;
		if (store_get_value(s, store, param, &plain, &plen) == 0) {
			if (plen)
				fwrite(plain, 1, plen, stdout);
			free(plain);
			rc = 0;
		} else {
			secstore_error(s, "%s parameter %s/%s does not exist",
			               s->self, store, param);
			rc = 1;
		}
	}
	free(store);
	free(param);
	return rc;
}

int cmd_del(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);

	if (strcmp(store, "password-store") == 0) {
		char *pass[] = { "pass", "rm", "-f", param, NULL };
		int rc = proc_run_tty(pass, NULL);
		free(store); free(param);
		return rc;
	}

	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir)) {
		free(dir);
		secstore_fatal(s, "store %s does not exist", store);
	}
	char *rel  = xasprintf("%s.gpg", param);
	char *file = path_join(dir, rel);
	if (is_file(file)) {
		char *rm[] = { "git", "rm", (char *)rel, NULL };
		proc_run_quiet(rm, dir);
		char *msg = xasprintf("removed %s parameter %s", s->self, param);
		char *commit[] = { "git", "commit", "-m", msg, (char *)rel, NULL };
		proc_run_quiet(commit, dir);
		free(msg);
	}
	free(file); free(rel); free(dir);
	free(store); free(param);
	return 0;
}

/* Read <base>/<name>, trimmed of trailing whitespace; NULL if absent. */
static char *qr_field(secstore_t *s, const char *store, const char *base,
                      const char *name)
{
	char  *p = xasprintf("%s/%s", base, name);
	char  *v = NULL;
	size_t n = 0;
	int rc = store_get_value(s, store, p, &v, &n);
	free(p);
	if (rc != 0) { free(v); return NULL; }
	while (n > 0 && (v[n-1] == '\n' || v[n-1] == '\r' ||
	                 v[n-1] == ' '  || v[n-1] == '\t'))
		v[--n] = '\0';
	return v;
}

/* Backslash-escape the WiFi-QR metacharacters \ ; , : " */
static char *wifi_escape(const char *in)
{
	char  *out = xmalloc(strlen(in) * 2 + 1);
	char  *w = out;
	for (const char *p = in; *p; p++) {
		if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"')
			*w++ = '\\';
		*w++ = *p;
	}
	*w = '\0';
	return out;
}

/* Percent-encode everything outside the RFC 3986 unreserved set. */
static char *pct_encode(const char *in)
{
	static const char hex[] = "0123456789ABCDEF";
	char  *out = xmalloc(strlen(in) * 3 + 1);
	char  *w = out;
	for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
		    (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
		    *p == '.' || *p == '~') {
			*w++ = (char)*p;
		} else {
			*w++ = '%'; *w++ = hex[*p >> 4]; *w++ = hex[*p & 0xf];
		}
	}
	*w = '\0';
	return out;
}

static int qr_emit(const char *payload, size_t len)
{
	char *qr[] = { "qrencode", "-t", "utf8", NULL };
	return proc_run(qr, NULL, payload, len, NULL, NULL, 0);
}

int cmd_qr(secstore_t *s, int argc, char **argv)
{
	if (argc < 1 || !argv[0] || !*argv[0])
		secstore_fatal(s, "please specify a parameter");
	char *store = NULL, *path = NULL;
	parse_param(s, argv[0], 1, &store, &path);

	int rc = 1;

	/* Template-aware QR: a wifi entry (ssid+password) yields a scannable
	 * WIFI: join code; an mfa entry yields an otpauth:// provisioning
	 * code; otherwise the parameter's raw value is encoded. */
	char *ssid = qr_field(s, store, path, "ssid");
	char *wpw  = ssid ? qr_field(s, store, path, "password") : NULL;
	if (ssid && wpw) {
		char *sec = qr_field(s, store, path, "security");
		char *hid = qr_field(s, store, path, "hidden");
		const char *type = "WPA";
		if (sec) {
			if (strcasestr(sec, "wep")) type = "WEP";
			else if (strcasestr(sec, "none") || strcasestr(sec, "open") ||
			         strcasestr(sec, "nopass") || !*sec) type = "nopass";
		}
		int hidden = hid && (strcasestr(hid, "true") || !strcmp(hid, "1") ||
		                     strcasestr(hid, "yes") || strcasestr(hid, "on"));
		char *es = wifi_escape(ssid), *ep = wifi_escape(wpw);
		char *uri = xasprintf("WIFI:T:%s;S:%s;P:%s;H:%s;;",
		                      type, es, ep, hidden ? "true" : "false");
		rc = qr_emit(uri, strlen(uri));
		free(uri); free(ep); free(es); free(hid); free(sec);
		free(wpw); free(ssid);
		free(store); free(path);
		return rc;
	}
	free(wpw); free(ssid);

	/* mfa: seed at <entry>/secret (standalone) or <entry>/mfa/secret. */
	char *base = xstrdup(path);
	char *seed = qr_field(s, store, base, "secret");
	if (!seed) { free(base); base = xasprintf("%s/mfa", path); seed = qr_field(s, store, base, "secret"); }
	if (seed) {
		char *issuer  = qr_field(s, store, base, "issuer");
		char *account = qr_field(s, store, base, "account");
		char *algo    = qr_field(s, store, base, "algorithm");
		char *digits  = qr_field(s, store, base, "digits");
		char *period  = qr_field(s, store, base, "period");
		char *label   = pct_encode(account ? account : "secret");
		char *ienc    = issuer ? pct_encode(issuer) : NULL;
		/* upper-case the algorithm for the otpauth URI */
		char  algup[8] = "SHA1";
		if (algo) { size_t i; for (i = 0; i < sizeof(algup)-1 && algo[i]; i++) algup[i] = (char)toupper((unsigned char)algo[i]); algup[i] = '\0'; }
		char *uri = xasprintf(
			"otpauth://totp/%s?secret=%s&algorithm=%s&digits=%s&period=%s%s%s",
			label, seed, algup, digits ? digits : "6", period ? period : "30",
			ienc ? "&issuer=" : "", ienc ? ienc : "");
		rc = qr_emit(uri, strlen(uri));
		free(uri); free(ienc); free(label); free(period); free(digits);
		free(algo); free(account); free(issuer); free(seed); free(base);
		free(store); free(path);
		return rc;
	}
	free(seed); free(base);

	/* Plain parameter: encode its raw value. */
	char  *plain = NULL;
	size_t plen = 0;
	if (store_get_value(s, store, path, &plain, &plen) == 0) {
		rc = qr_emit(plain, plen);
		free(plain);
	}
	free(store); free(path);
	return rc;
}

int cmd_gen(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);

	char  *plain = NULL;
	size_t plen = 0;
	if (store_get_value(s, store, param, &plain, &plen) == 0) {
		if (plen) fwrite(plain, 1, plen, stdout);
		free(plain);
		free(store); free(param);
		return 0;
	}

	/* Ensure the store exists, then generate 33 alnum chars. */
	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir))
		cmd_init(s, 1, (char *[]){ store });
	free(dir);

	static const char alpha[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	char secret[34];
	FILE *ur = fopen("/dev/urandom", "rb");
	for (int i = 0; i < 33; i++) {
		unsigned char c;
		if (ur && fread(&c, 1, 1, ur) == 1)
			secret[i] = alpha[c % 62];
		else
			secret[i] = alpha[(unsigned)rand() % 62];
	}
	secret[33] = '\0';
	if (ur) fclose(ur);

	store_set_value(s, store, param, secret, 33);

	char  *out = NULL;
	size_t olen = 0;
	if (store_get_value(s, store, param, &out, &olen) == 0) {
		if (olen) fwrite(out, 1, olen, stdout);
		free(out);
	}
	free(store); free(param);
	return 0;
}

int cmd_def(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);

	char  *plain = NULL;
	size_t plen = 0;
	if (store_get_value(s, store, param, &plain, &plen) != 0) {
		const char *value = (argc >= 2 && argv[1]) ? argv[1] : "";
		char *v = xasprintf("%s\n", value);
		store_set_value(s, store, param, v, strlen(v));
		free(v);
	} else {
		free(plain);
	}

	char  *out = NULL;
	size_t olen = 0;
	if (store_get_value(s, store, param, &out, &olen) == 0) {
		if (olen) fwrite(out, 1, olen, stdout);
		free(out);
	}
	free(store); free(param);
	return 0;
}

/* Shared validation for ins/rem: the raw arg must be non-empty and
 * contain a '/' (FEAT-212 hoisted these guards out of get/set). */
static void ins_rem_validate(secstore_t *s, const char *raw)
{
	if (!raw || !*raw)
		secstore_fatal(s, "please specify a parameter");
	if (strchr(raw, '/') == NULL)
		secstore_fatal(s, "please specify in the format <store>/<param>");
	char *work = secstore_lower(raw);
	str_translate(work, ':', '/');
	secstore_validate_name(s, "parameter", work);
	free(work);
}

/* Split text into non-blank lines. */
static void split_lines(const char *text, strlist *out)
{
	char *copy = xstrdup(text);
	char *save = NULL;
	for (char *l = strtok_r(copy, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		int blank = 1;
		for (char *p = l; *p; p++)
			if (*p != ' ' && *p != '\t') { blank = 0; break; }
		if (!blank)
			strlist_push_copy(out, l);
	}
	free(copy);
}

int cmd_ins(secstore_t *s, int argc, char **argv)
{
	const char *raw = argc ? argv[0] : NULL;
	ins_rem_validate(s, raw);

	char *store = NULL, *param = NULL;
	parse_param(s, raw, 1, &store, &param);

	char  *ins = NULL;
	size_t ilen = 0;
	read_stdin(&ins, &ilen);

	char  *old = NULL;
	size_t olen = 0;
	store_get_value(s, store, param, &old, &olen);

	strlist lines;
	strlist_init(&lines);
	split_lines(ins, &lines);
	if (old) split_lines(old, &lines);
	strlist_sort_unique(&lines);

	size_t cap = 1;
	for (size_t i = 0; i < lines.len; i++)
		cap += strlen(lines.items[i]) + 1;
	char *merged = xmalloc(cap);
	size_t off = 0;
	for (size_t i = 0; i < lines.len; i++)
		off += (size_t)sprintf(merged + off, "%s\n", lines.items[i]);

	store_set_value(s, store, param, merged, off);

	free(merged);
	strlist_free(&lines);
	free(old); free(ins);
	free(store); free(param);
	return 0;
}

int cmd_rem(secstore_t *s, int argc, char **argv)
{
	const char *raw = argc ? argv[0] : NULL;
	ins_rem_validate(s, raw);

	char *store = NULL, *param = NULL;
	parse_param(s, raw, 1, &store, &param);

	char  *rem = NULL;
	size_t rlen = 0;
	read_stdin(&rem, &rlen);
	/* Strip trailing newline of the match pattern. */
	while (rlen > 0 && (rem[rlen - 1] == '\n' || rem[rlen - 1] == '\r'))
		rem[--rlen] = '\0';

	char  *old = NULL;
	size_t olen = 0;
	store_get_value(s, store, param, &old, &olen);

	strlist lines;
	strlist_init(&lines);
	if (old) {
		strlist all;
		strlist_init(&all);
		split_lines(old, &all);
		for (size_t i = 0; i < all.len; i++)
			if (!*rem || strstr(all.items[i], rem) == NULL)
				strlist_push_copy(&lines, all.items[i]);
		strlist_free(&all);
	}
	strlist_sort_unique(&lines);

	size_t cap = 1;
	for (size_t i = 0; i < lines.len; i++)
		cap += strlen(lines.items[i]) + 1;
	char  *merged = xmalloc(cap);
	size_t off = 0;
	for (size_t i = 0; i < lines.len; i++)
		off += (size_t)sprintf(merged + off, "%s\n", lines.items[i]);

	store_set_value(s, store, param, merged, off);

	free(merged);
	strlist_free(&lines);
	free(old); free(rem);
	free(store); free(param);
	return 0;
}

int cmd_edit(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);

	const char *editor = getenv("EDITOR");
	if (!editor || !*editor)
		editor = "vi";
	require_gpg_identity(s);

	if (strcmp(store, "password-store") == 0) {
		char *pass[] = { "pass", "edit", param, NULL };
		int rc = proc_run_tty(pass, NULL);
		free(store); free(param);
		return rc;
	}

	char *dir = path_join(s->self_config, store);
	if (!is_dir(dir)) {
		free(dir);
		secstore_fatal(s, "store %s does not exist", store);
	}

	char tmpl[] = "/tmp/secret-edit.XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd >= 0) close(fd);

	char  *plain = NULL;
	size_t plen = 0;
	if (store_get_value(s, store, param, &plain, &plen) == 0) {
		write_file(tmpl, plain, plen);
		free(plain);
	}

	char *ed[] = { (char *)editor, tmpl, NULL };
	proc_run_tty(ed, NULL);

	char  *edited = NULL;
	size_t elen = 0;
	read_file(tmpl, &edited, &elen);
	store_set_value(s, store, param, edited ? edited : "", elen);
	free(edited);

	char *shred[] = { "shred", "-f", tmpl, NULL };
	if (proc_run_quiet(shred, NULL) != 0)
		remove(tmpl);

	free(dir);
	free(store); free(param);
	return 0;
}
