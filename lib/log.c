/*
 * log.c — the four log levels plus the fatal() convenience.
 *
 * Output formats mirror bin/secret's fatal/error/warn/info/debug
 * helpers byte-for-byte (including the ANSI colour escapes) so the
 * logging contract pinned by skills/logging.md and tests/unit/secret.bats
 * is preserved. All log output goes to stderr; only command payloads go
 * to stdout.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "internal.h"

static void vlog(const secstore_t *s, const char *color,
                 const char *label, const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: %s%s", s ? s->self : "secret", color, label);
	vfprintf(stderr, fmt, ap);
	fputs("\033[0m\n", stderr);
}

void secstore_fatal(const secstore_t *s, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	/* matches: "$SELF: \033[31;mfatal - <msg>\033[0m" */
	vlog(s, "\033[31;m", "fatal - ", fmt, ap);
	va_end(ap);
	exit(1);
}

void secstore_error(const secstore_t *s, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vlog(s, "\033[31;m", "error - ", fmt, ap);
	va_end(ap);
}

void secstore_warn(const secstore_t *s, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vlog(s, "\033[33;1;m", "warn - ", fmt, ap);
	va_end(ap);
}

void secstore_info(const secstore_t *s, const char *fmt, ...)
{
	va_list ap;
	if (s && s->quiet)
		return;
	va_start(ap, fmt);
	/* matches: "$SELF: info - \033[32;m<msg>\033[0m" */
	fprintf(stderr, "%s: info - \033[32;m", s ? s->self : "secret");
	vfprintf(stderr, fmt, ap);
	fputs("\033[0m\n", stderr);
	va_end(ap);
}

void secstore_debug(const secstore_t *s, const char *fmt, ...)
{
	va_list ap;
	if (!s || !s->debug)
		return;
	va_start(ap, fmt);
	vlog(s, "\033[90;m", "debug - ", fmt, ap);
	va_end(ap);
}
