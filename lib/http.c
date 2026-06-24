/*
 * http.c — the HTTP pull server and the uid-derived port.
 *
 * `secret serve` runs a minimal, read-only HTTP server that exposes every
 * store's git repository for "dumb HTTP" git pull under the path
 * /app/secret/<store>. Peers fetch with
 *     git clone http://<host>:<port>/app/secret/<store>
 * and decrypt locally; only GPG ciphertext and public keys are served, so
 * the channel needs no transport encryption to stay confidential. Pull
 * only — pushing is done over SSH (see push/sync).
 *
 * The port is derived deterministically from the effective uid so a peer
 * who knows the uid (typically: the same user on another machine) knows
 * the port without extra configuration.
 */
#define _XOPEN_SOURCE 700
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"

/* Dynamic/private port range: 49152..65535 (16384 ports). */
int secstore_http_port(secstore_t *s)
{
	const char *env = getenv("SECRET_HTTP_PORT");
	if (env && *env) {
		int p = atoi(env);
		if (p > 0 && p < 65536)
			return p;
	}
	return 49152 + (int)((unsigned)s->euid % 16384u);
}

/* URL-decode in place (%xx and '+'). */
static void url_decode(char *s)
{
	char *w = s;
	for (char *r = s; *r; r++) {
		if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
			int hi = r[1] <= '9' ? r[1] - '0' : (tolower(r[1]) - 'a' + 10);
			int lo = r[2] <= '9' ? r[2] - '0' : (tolower(r[2]) - 'a' + 10);
			*w++ = (char)((hi << 4) | lo);
			r += 2;
		} else {
			*w++ = *r;
		}
	}
	*w = '\0';
}

static void send_status(int fd, const char *status)
{
	char buf[256];
	int n = snprintf(buf, sizeof(buf),
		"HTTP/1.0 %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", status);
	(void)!write(fd, buf, (size_t)n);
}

/* Serve the file at `path` with a 200 response. */
static void send_file(int fd, const char *path)
{
	struct stat st;
	FILE *f = fopen(path, "rb");
	if (!f || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
		if (f) fclose(f);
		send_status(fd, "404 Not Found");
		return;
	}
	char hdr[256];
	int n = snprintf(hdr, sizeof(hdr),
		"HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n"
		"Content-Length: %lld\r\nConnection: close\r\n\r\n",
		(long long)st.st_size);
	(void)!write(fd, hdr, (size_t)n);

	char   buf[65536];
	size_t r;
	while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
		size_t off = 0;
		while (off < r) {
			ssize_t w = write(fd, buf + off, r - off);
			if (w <= 0) { fclose(f); return; }
			off += (size_t)w;
		}
	}
	fclose(f);
}

/* Handle one connection: parse the request line and serve the mapped
 * file under <store>/.git, refreshing info/refs on demand. */
static void handle_conn(secstore_t *s, int fd)
{
	char req[4096];
	ssize_t n = read(fd, req, sizeof(req) - 1);
	if (n <= 0) { close(fd); return; }
	req[n] = '\0';

	/* Request line: METHOD SP TARGET SP HTTP/x */
	char *sp1 = strchr(req, ' ');
	if (!sp1) { send_status(fd, "400 Bad Request"); close(fd); return; }
	*sp1 = '\0';
	char *method = req;
	char *target = sp1 + 1;
	char *sp2 = strchr(target, ' ');
	if (sp2) *sp2 = '\0';
	if (strcmp(method, "GET") != 0) { send_status(fd, "405 Method Not Allowed"); close(fd); return; }

	char *q = strchr(target, '?');     /* drop the query string */
	if (q) *q = '\0';
	url_decode(target);

	const char *prefix = "/app/secret/";
	if (strncmp(target, prefix, strlen(prefix)) != 0 || strstr(target, "..")) {
		send_status(fd, "404 Not Found");
		close(fd);
		return;
	}
	char *rel = target + strlen(prefix);          /* "<store>/<rest>" */
	char *slash = strchr(rel, '/');
	if (!slash || slash == rel || !slash[1]) {
		send_status(fd, "404 Not Found");
		close(fd);
		return;
	}
	*slash = '\0';
	const char *store = rel;
	const char *rest  = slash + 1;

	/* Refresh the dumb-HTTP advertisement when refs are requested. */
	if (strcmp(rest, "info/refs") == 0) {
		char *sdir = path_join(s->self_config, store);
		char *usi[] = { "git", "update-server-info", NULL };
		proc_run(usi, sdir, NULL, 0, NULL, NULL, 1);
		free(sdir);
	}

	char *file = xasprintf("%s/%s/.git/%s", s->self_config, store, rest);
	send_file(fd, file);
	free(file);
	close(fd);
}

int cmd_serve(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	signal(SIGPIPE, SIG_IGN);

	int port = secstore_http_port(s);
	const char *bindaddr = getenv("SECRET_HTTP_BIND");
	if (!bindaddr || !*bindaddr) bindaddr = "0.0.0.0";

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		secstore_fatal(s, "socket: %s", strerror(errno));
	int one = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	if (inet_pton(AF_INET, bindaddr, &addr.sin_addr) != 1)
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		secstore_fatal(s, "bind %s:%d: %s", bindaddr, port, strerror(errno));
	if (listen(sock, 16) != 0)
		secstore_fatal(s, "listen: %s", strerror(errno));

	secstore_info(s, "serving stores at http://%s:%d/app/secret/ (pull only; Ctrl-C to stop)",
	              bindaddr, port);

	for (;;) {
		int c = accept(sock, NULL, NULL);
		if (c < 0) {
			if (errno == EINTR) continue;
			break;
		}
		/* Sequential, one request per connection (Connection: close). */
		struct timeval tv = { 10, 0 };
		setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		handle_conn(s, c);
	}
	close(sock);
	return 0;
}

int cmd_port(secstore_t *s, int argc, char **argv)
{
	(void)argc; (void)argv;
	printf("%d\n", secstore_http_port(s));
	return 0;
}
