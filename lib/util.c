/*
 * util.c — string, list and filesystem helpers shared across the library.
 */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "internal.h"

/* ---- allocation ---------------------------------------------------- */

void *xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p) { fprintf(stderr, "secret: fatal - out of memory\n"); exit(1); }
	return p;
}

void *xrealloc(void *p, size_t n)
{
	void *q = realloc(p, n ? n : 1);
	if (!q) { fprintf(stderr, "secret: fatal - out of memory\n"); exit(1); }
	return q;
}

char *xstrdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char  *p = xmalloc(n);
	memcpy(p, s, n);
	return p;
}

char *xasprintf(const char *fmt, ...)
{
	va_list ap;
	int     n;
	char   *buf;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (n < 0) return xstrdup("");
	buf = xmalloc((size_t)n + 1);
	va_start(ap, fmt);
	vsnprintf(buf, (size_t)n + 1, fmt, ap);
	va_end(ap);
	return buf;
}

char *path_join(const char *a, const char *b)
{
	size_t la = strlen(a);
	if (la > 0 && a[la - 1] == '/')
		return xasprintf("%s%s", a, b);
	return xasprintf("%s/%s", a, b);
}

void str_translate(char *s, char c1, char c2)
{
	for (; *s; s++)
		if (*s == c1)
			*s = c2;
}

int has_dotdot(const char *s)
{
	return strstr(s, "..") != NULL;
}

char *secstore_lower(const char *in)
{
	char *out = xstrdup(in);
	for (char *p = out; *p; p++)
		*p = (char)tolower((unsigned char)*p);
	return out;
}

/* ---- base64 (RFC 4648, used by the external source protocol) ------- */

static const char b64_alpha[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *b64_encode(const unsigned char *data, size_t len)
{
	size_t olen = 4 * ((len + 2) / 3);
	char  *out = xmalloc(olen + 1);
	size_t i, o = 0;
	for (i = 0; i + 2 < len; i += 3) {
		unsigned v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
		out[o++] = b64_alpha[(v >> 18) & 0x3f];
		out[o++] = b64_alpha[(v >> 12) & 0x3f];
		out[o++] = b64_alpha[(v >> 6) & 0x3f];
		out[o++] = b64_alpha[v & 0x3f];
	}
	if (i < len) {
		unsigned v = data[i] << 16;
		int rem = (int)(len - i);          /* 1 or 2 */
		if (rem == 2)
			v |= data[i + 1] << 8;
		out[o++] = b64_alpha[(v >> 18) & 0x3f];
		out[o++] = b64_alpha[(v >> 12) & 0x3f];
		out[o++] = (rem == 2) ? b64_alpha[(v >> 6) & 0x3f] : '=';
		out[o++] = '=';
	}
	out[o] = '\0';
	return out;
}

static int b64_val(unsigned char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

unsigned char *b64_decode(const char *text, size_t *out_len)
{
	size_t cap = strlen(text) / 4 * 3 + 3, o = 0;
	unsigned char *out = xmalloc(cap + 1);
	int acc = 0, bits = 0;
	for (const char *p = text; *p; p++) {
		if (*p == '=' || *p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')
			continue;
		int v = b64_val((unsigned char)*p);
		if (v < 0)
			continue;                      /* skip stray chars */
		acc = (acc << 6) | v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out[o++] = (unsigned char)((acc >> bits) & 0xff);
		}
	}
	out[o] = '\0';
	if (out_len)
		*out_len = o;
	return out;
}

/* ---- strlist ------------------------------------------------------- */

void strlist_init(strlist *l)
{
	l->items = NULL;
	l->len = l->cap = 0;
}

void strlist_push(strlist *l, char *owned)
{
	if (l->len == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->items = xrealloc(l->items, l->cap * sizeof(*l->items));
	}
	l->items[l->len++] = owned;
}

void strlist_push_copy(strlist *l, const char *s)
{
	strlist_push(l, xstrdup(s));
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

void strlist_sort_unique(strlist *l)
{
	if (l->len == 0)
		return;
	qsort(l->items, l->len, sizeof(*l->items), cmp_str);
	size_t w = 1;
	for (size_t r = 1; r < l->len; r++) {
		if (strcmp(l->items[r], l->items[w - 1]) == 0) {
			free(l->items[r]);
		} else {
			l->items[w++] = l->items[r];
		}
	}
	l->len = w;
}

void strlist_free(strlist *l)
{
	for (size_t i = 0; i < l->len; i++)
		free(l->items[i]);
	free(l->items);
	strlist_init(l);
}

/* ---- filesystem ---------------------------------------------------- */

int is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int is_file(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int path_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

long file_size(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return -1;
	return (long)st.st_size;
}

int read_file(const char *path, char **buf, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	size_t cap = 4096, n = 0;
	char  *b = xmalloc(cap);
	size_t r;
	while ((r = fread(b + n, 1, cap - n, f)) > 0) {
		n += r;
		if (n == cap) { cap *= 2; b = xrealloc(b, cap); }
	}
	fclose(f);
	b[n] = '\0';
	*buf = b;
	if (len)
		*len = n;
	return 0;
}

int write_file(const char *path, const char *buf, size_t len)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	if (len)
		fwrite(buf, 1, len, f);
	fclose(f);
	return 0;
}

int mkdir_p(const char *path)
{
	char  *tmp = xstrdup(path);
	size_t len = strlen(tmp);
	int    rc = 0;

	if (len > 1 && tmp[len - 1] == '/')
		tmp[len - 1] = '\0';

	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0777) != 0 && !is_dir(tmp)) { rc = -1; break; }
			*p = '/';
		}
	}
	if (rc == 0 && mkdir(tmp, 0777) != 0 && !is_dir(tmp))
		rc = -1;
	free(tmp);
	return rc;
}

int rm_rf(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
		return 0; /* already gone */

	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if (d) {
			struct dirent *e;
			while ((e = readdir(d))) {
				if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
					continue;
				char *child = path_join(path, e->d_name);
				rm_rf(child);
				free(child);
			}
			closedir(d);
		}
		return rmdir(path);
	}
	return unlink(path);
}

static void list_files_rel_into(const char *base, const char *rel, strlist *out)
{
	char *dir = (rel && *rel) ? path_join(base, rel) : xstrdup(base);
	DIR  *d = opendir(dir);
	if (!d) { free(dir); return; }

	struct dirent *e;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		/* Skip top-level dotfiles (mirrors the shell's `find *`). */
		if ((!rel || !*rel) && e->d_name[0] == '.')
			continue;
		char *childrel = (rel && *rel)
			? xasprintf("%s/%s", rel, e->d_name)
			: xstrdup(e->d_name);
		char *full = path_join(base, childrel);
		struct stat st;
		if (stat(full, &st) == 0) {
			if (S_ISDIR(st.st_mode))
				list_files_rel_into(base, childrel, out);
			else if (S_ISREG(st.st_mode))
				strlist_push(out, xstrdup(childrel));
		}
		free(full);
		free(childrel);
	}
	closedir(d);
	free(dir);
}

void list_files_rel(const char *dir, strlist *out)
{
	list_files_rel_into(dir, "", out);
}

void list_subdirs(const char *dir, strlist *out)
{
	DIR *d = opendir(dir);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d))) {
		/* Skip "." / ".." and any hidden entry (e.g. the .groups
		 * config dir) — stores are never dot-named. */
		if (e->d_name[0] == '.')
			continue;
		char *full = path_join(dir, e->d_name);
		if (is_dir(full))
			strlist_push_copy(out, e->d_name);
		free(full);
	}
	closedir(d);
}
