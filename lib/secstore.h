/*
 * libsecstore — the C core of the `secret` multi-store secret manager.
 *
 * This library inlines all of `secret`'s logic (store/parameter handling,
 * name validation, GPG/git/pass/ssh orchestration) so the `secret(1)` CLI
 * is a thin dispatcher over it. It is named `libsecstore` to avoid the
 * collision with the unrelated GNOME/freedesktop `libsecret` Secret Service
 * library.
 *
 * The library shells out to the same external tools the original bash
 * implementation used — gpg(1), git(1), pass(1), ssh(1), qrencode(1) — so
 * its on-disk and on-wire formats stay byte-compatible with peers running
 * the shell version.
 */
#ifndef SECSTORE_H
#define SECSTORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime context: resolved configuration plus the log/force switches. */
typedef struct {
	char       *self;             /* program basename, e.g. "secret"     */
	char       *self_config;      /* resolved store root                 */
	char       *home;             /* $HOME                               */
	char       *xdg_config_home;  /* $XDG_CONFIG_HOME or $HOME/.config   */
	char       *exe_dir;          /* directory containing the binary     */
	int         euid;             /* effective uid                       */
	int         debug;            /* SELF_DEBUG / -d                     */
	int         quiet;            /* SELF_QUIET / -q                     */
	const char *force;            /* SELF_FORCE: "-f" or "" (-f flag)    */
} secstore_t;

/* ---- context lifecycle (config.c) ---------------------------------- */

/* Resolve configuration from argv[0] and the environment. Mirrors the
 * SELF_CONFIG resolution in the original bin/secret (lines 234-254):
 * XDG_SECRET_STORES wins verbatim; otherwise /etc/<self> as root or
 * $XDG_CONFIG_HOME/<self> as a normal user (creating it). */
secstore_t *secstore_new(const char *argv0);
void        secstore_free(secstore_t *s);

/* Package version, resolved from VERSION / .rpk/version /
 * share/<self>/version relative to the binary (config.c). Caller frees. */
char *secstore_version(const secstore_t *s);

/* Local identity: "$(whoami)@$(hostname -f)" lowercased. Caller frees,
 * or NULL if it cannot be computed (config.c). */
char *secstore_identity(const secstore_t *s);

/* ---- logging (log.c) ----------------------------------------------- */

void secstore_fatal(const secstore_t *s, const char *fmt, ...);  /* exits */
void secstore_error(const secstore_t *s, const char *fmt, ...);
void secstore_warn (const secstore_t *s, const char *fmt, ...);
void secstore_info (const secstore_t *s, const char *fmt, ...);
void secstore_debug(const secstore_t *s, const char *fmt, ...);

/* ---- name handling (util.c) ---------------------------------------- */

/* fatal() on a '..' path-traversal segment (FEAT-207). kind is the noun
 * used in the message ("store" / "parameter"). */
void  secstore_validate_name(const secstore_t *s, const char *kind, const char *name);
/* ASCII-lowercased copy. Caller frees. */
char *secstore_lower(const char *in);

/* ---- commands (one per subcommand) --------------------------------- */
/* Each returns a process exit status (0 == success). */

int cmd_help     (secstore_t *s, int argc, char **argv);   /* help.c     */
int cmd_version  (secstore_t *s, int argc, char **argv);
int cmd_identity (secstore_t *s, int argc, char **argv);
int cmd_skills   (secstore_t *s, int argc, char **argv);   /* skills.c   */

int cmd_stores   (secstore_t *s, int argc, char **argv);   /* store.c    */
int cmd_params   (secstore_t *s, int argc, char **argv);
int cmd_exists   (secstore_t *s, int argc, char **argv);
int cmd_destroy  (secstore_t *s, int argc, char **argv);
int cmd_clean    (secstore_t *s, int argc, char **argv);
int cmd_ls       (secstore_t *s, int argc, char **argv);
int cmd_has      (secstore_t *s, int argc, char **argv);

int cmd_init     (secstore_t *s, int argc, char **argv);   /* init.c     */
int cmd_setup    (secstore_t *s, int argc, char **argv);
int cmd_pass_init(secstore_t *s, int argc, char **argv);

int cmd_set      (secstore_t *s, int argc, char **argv);   /* param.c    */
int cmd_get      (secstore_t *s, int argc, char **argv);
int cmd_del      (secstore_t *s, int argc, char **argv);
int cmd_qr       (secstore_t *s, int argc, char **argv);
int cmd_gen      (secstore_t *s, int argc, char **argv);
int cmd_def      (secstore_t *s, int argc, char **argv);
int cmd_ins      (secstore_t *s, int argc, char **argv);
int cmd_rem      (secstore_t *s, int argc, char **argv);
int cmd_edit     (secstore_t *s, int argc, char **argv);

int cmd_remotes  (secstore_t *s, int argc, char **argv);   /* sync.c     */
int cmd_pull     (secstore_t *s, int argc, char **argv);
int cmd_push     (secstore_t *s, int argc, char **argv);
int cmd_sync     (secstore_t *s, int argc, char **argv);

int cmd_gpg_keys    (secstore_t *s, int argc, char **argv); /* recipients.c */
int cmd_add_gpg_key (secstore_t *s, int argc, char **argv);
int cmd_del_gpg_key (secstore_t *s, int argc, char **argv);
int cmd_has_gpg_key (secstore_t *s, int argc, char **argv);

int cmd_sources  (secstore_t *s, int argc, char **argv);   /* transfer.c */
int cmd_export   (secstore_t *s, int argc, char **argv);
int cmd_import   (secstore_t *s, int argc, char **argv);

int cmd_templates(secstore_t *s, int argc, char **argv);   /* template.c */
int cmd_new      (secstore_t *s, int argc, char **argv);
int cmd_otp      (secstore_t *s, int argc, char **argv);

int cmd_clip     (secstore_t *s, int argc, char **argv);   /* clip.c */

#ifdef __cplusplus
}
#endif

#endif /* SECSTORE_H */
