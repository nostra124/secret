/*
 * proc.c — fork/exec subprocess helpers.
 *
 * libsecstore shells out to gpg(1), git(1), pass(1), ssh(1) and
 * qrencode(1) exactly as the original bash did. These helpers spawn a
 * child, optionally feed it stdin, and optionally capture its stdout —
 * the C equivalent of the shell's pipelines and command substitutions.
 */
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "internal.h"

int proc_run(char *const argv[], const char *cwd,
             const char *input, size_t input_len,
             char **out, size_t *out_len, int silence_stderr)
{
	int  in_pipe[2]  = { -1, -1 };
	int  out_pipe[2] = { -1, -1 };
	pid_t pid;

	if (input && pipe(in_pipe) != 0)
		return -1;
	if (out && pipe(out_pipe) != 0) {
		if (input) { close(in_pipe[0]); close(in_pipe[1]); }
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		if (input)  { close(in_pipe[0]);  close(in_pipe[1]); }
		if (out)    { close(out_pipe[0]); close(out_pipe[1]); }
		return -1;
	}

	if (pid == 0) {
		/* child */
		if (cwd && chdir(cwd) != 0)
			_exit(127);
		if (input) {
			dup2(in_pipe[0], STDIN_FILENO);
			close(in_pipe[0]);
			close(in_pipe[1]);
		}
		if (out) {
			dup2(out_pipe[1], STDOUT_FILENO);
			close(out_pipe[0]);
			close(out_pipe[1]);
		}
		if (silence_stderr) {
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull >= 0) {
				dup2(devnull, STDERR_FILENO);
				close(devnull);
			}
		}
		execvp(argv[0], argv);
		_exit(127);
	}

	/* parent */
	if (input) {
		close(in_pipe[0]);
		size_t off = 0;
		while (off < input_len) {
			ssize_t n = write(in_pipe[1], input + off, input_len - off);
			if (n <= 0) {
				if (n < 0 && errno == EINTR)
					continue;
				break;
			}
			off += (size_t)n;
		}
		close(in_pipe[1]);
	}

	if (out) {
		close(out_pipe[1]);
		size_t cap = 4096, len = 0;
		char  *buf = xmalloc(cap);
		for (;;) {
			if (len + 4096 > cap) {
				cap *= 2;
				buf = xrealloc(buf, cap);
			}
			ssize_t n = read(out_pipe[0], buf + len, 4096);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (n == 0)
				break;
			len += (size_t)n;
		}
		close(out_pipe[0]);
		buf[len] = '\0';
		*out = buf;
		if (out_len)
			*out_len = len;
	}

	int status;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

int proc_run_tty(char *const argv[], const char *cwd)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		if (cwd && chdir(cwd) != 0)
			_exit(127);
		execvp(argv[0], argv);
		_exit(127);
	}
	int status;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

int have_tool(const char *name)
{
	char *path = getenv("PATH");
	if (!path)
		return 0;
	char  *copy = xstrdup(path);
	int    found = 0;
	for (char *tok = strtok(copy, ":"); tok; tok = strtok(NULL, ":")) {
		char *full = path_join(tok, name);
		if (access(full, X_OK) == 0)
			found = 1;
		free(full);
		if (found)
			break;
	}
	free(copy);
	return found;
}
