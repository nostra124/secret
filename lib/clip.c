/*
 * clip.c — general clipboard support, with auto-clear.
 *
 * `secret clip <store>/<param>` copies a secret to the system clipboard;
 * `secret otp <store>/<entry> -c` copies a generated TOTP code. The value
 * is wiped from the clipboard after a timeout (default 45s), like
 * pass(1)'s -c. The clipboard backend is auto-detected (wl-copy / xclip /
 * xsel / pbcopy / clip.exe) or overridden with $SECRET_CLIP_CMD (a shell
 * command fed the value on stdin) — which also makes the feature testable
 * on a headless host.
 */
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

/* Resolve the copy and clear argv for the active backend. Returns 0 on
 * success. *clear_empty means "clear by re-running the copy command with
 * empty input" (no dedicated clear command). */
static int clip_backend(secstore_t *s, char ***copy, char ***clear, int *clear_empty)
{
	const char *custom = getenv("SECRET_CLIP_CMD");
	if (custom && *custom) {
		static char *c[] = { "sh", "-c", NULL, NULL };
		c[2] = (char *)custom;
		*copy = c; *clear = c; *clear_empty = 1;
		return 0;
	}
	/* Prefer the backend matching the active session: wl-copy needs a
	 * Wayland socket, xclip/xsel need an X display. pbcopy / clip.exe
	 * (macOS / WSL) need no display variable. */
	int wayland = getenv("WAYLAND_DISPLAY") != NULL;
	int x11     = getenv("DISPLAY") != NULL;
	if (wayland && have_tool("wl-copy")) {
		static char *c[] = { "wl-copy", NULL };
		static char *x[] = { "wl-copy", "--clear", NULL };
		*copy = c; *clear = x; *clear_empty = 0; return 0;
	}
	if (x11 && have_tool("xclip")) {
		static char *c[] = { "xclip", "-selection", "clipboard", NULL };
		*copy = c; *clear = c; *clear_empty = 1; return 0;
	}
	if (x11 && have_tool("xsel")) {
		static char *c[] = { "xsel", "--clipboard", "--input", NULL };
		static char *x[] = { "xsel", "--clipboard", "--clear", NULL };
		*copy = c; *clear = x; *clear_empty = 0; return 0;
	}
	if (have_tool("pbcopy")) {
		static char *c[] = { "pbcopy", NULL };
		*copy = c; *clear = c; *clear_empty = 1; return 0;
	}
	if (have_tool("clip.exe")) {
		static char *c[] = { "clip.exe", NULL };
		*copy = c; *clear = c; *clear_empty = 1; return 0;
	}
	(void)s;
	return -1;
}

void secstore_clip(secstore_t *s, const char *data, size_t len)
{
	char **copy = NULL, **clear = NULL;
	int    clear_empty = 0;
	if (clip_backend(s, &copy, &clear, &clear_empty) != 0)
		secstore_fatal(s, "no clipboard tool found — install wl-clipboard / "
		               "xclip / xsel, or set SECRET_CLIP_CMD");

	int seconds = 45;
	const char *t = getenv("SECRET_CLIP_TIME");
	if (t && *t) { int v = atoi(t); if (v >= 0) seconds = v; }

	if (proc_run(copy, NULL, data, len, NULL, NULL, 1) != 0)
		secstore_fatal(s, "clipboard copy failed");

	if (seconds > 0) {
		/* Detach a sleeper that clears the clipboard after the timeout. */
		pid_t pid = fork();
		if (pid == 0) {
			setsid();
			int devnull = open("/dev/null", O_RDWR);
			if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); }
			sleep((unsigned)seconds);
			proc_run(clear, NULL, clear_empty ? "" : NULL, 0, NULL, NULL, 1);
			_exit(0);
		}
	}

	secstore_info(s, "copied to clipboard%s",
	              seconds > 0 ? " (clears shortly)" : "");
}

int cmd_clip(secstore_t *s, int argc, char **argv)
{
	char *store = NULL, *param = NULL;
	parse_param(s, argc ? argv[0] : NULL, 1, &store, &param);

	char  *val = NULL;
	size_t len = 0;
	if (store_get_value(s, store, param, &val, &len) != 0) {
		secstore_error(s, "%s parameter %s/%s does not exist", s->self, store, param);
		free(store); free(param);
		return 1;
	}
	/* Drop a single trailing newline so the clipboard gets a clean value. */
	if (len > 0 && val[len - 1] == '\n') len--;
	secstore_clip(s, val, len);

	free(val); free(store); free(param);
	return 0;
}
