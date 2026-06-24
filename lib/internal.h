/*
 * libsecstore internal helpers — not part of the public API.
 */
#ifndef SECSTORE_INTERNAL_H
#define SECSTORE_INTERNAL_H

#include <stddef.h>
#include "secstore.h"

/* ---- small string helpers (util.c) --------------------------------- */

char *xstrdup(const char *s);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
/* printf into a freshly malloc'd buffer. Caller frees. */
char *xasprintf(const char *fmt, ...);
/* Join two path segments with a single '/'. Caller frees. */
char *path_join(const char *a, const char *b);
/* In place: translate every occurrence of c1 to c2. */
void  str_translate(char *s, char c1, char c2);
/* 1 if s contains the substring "..". */
int   has_dotdot(const char *s);

/* A growable, sortable list of owned strings. */
typedef struct {
	char  **items;
	size_t  len;
	size_t  cap;
} strlist;

void strlist_init(strlist *l);
void strlist_push(strlist *l, char *owned);          /* takes ownership   */
void strlist_push_copy(strlist *l, const char *s);
void strlist_sort_unique(strlist *l);                /* sort -u           */
void strlist_free(strlist *l);

/* ---- filesystem helpers (util.c) ----------------------------------- */

int  is_dir(const char *path);
int  is_file(const char *path);          /* regular file exists          */
int  path_exists(const char *path);      /* any type (stat)              */
int  mkdir_p(const char *path);
int  rm_rf(const char *path);
/* Append every regular file under dir (recursively) as a path relative
 * to dir into out. Top-level entries beginning with '.' are skipped, to
 * mirror the shell's `find * -type f` (the glob excludes dotfiles such
 * as the .gpg/ recipients directory). */
void list_files_rel(const char *dir, strlist *out);
/* List immediate sub-directory names of dir into out (sorted by caller). */
void list_subdirs(const char *dir, strlist *out);
/* File size in bytes, or -1 on error. */
long file_size(const char *path);
/* Slurp a whole file into a NUL-terminated buffer (caller frees). 0 on
 * success, -1 if it cannot be opened. */
int  read_file(const char *path, char **buf, size_t *len);
/* Write len bytes to path, truncating. 0 on success, -1 on error. */
int  write_file(const char *path, const char *buf, size_t len);

/* ---- subprocess helpers (proc.c) ----------------------------------- */

/* Spawn argv (NULL-terminated). If cwd != NULL, chdir there first. If
 * input != NULL, write input[0..input_len) to the child's stdin. If out
 * != NULL, capture the child's stdout into *out (NUL-terminated, caller
 * frees) and store its length in *out_len. When silence_stderr, the
 * child's stderr is redirected to /dev/null. Returns the child's exit
 * status (WEXITSTATUS), or -1 if the child could not be spawned. */
int proc_run(char *const argv[], const char *cwd,
             const char *input, size_t input_len,
             char **out, size_t *out_len, int silence_stderr);

/* Convenience: run argv inheriting the parent's stdio (interactive
 * tools such as $EDITOR), optionally chdir'ing first. */
int proc_run_tty(char *const argv[], const char *cwd);

/* 1 if `name` is found on $PATH. */
int have_tool(const char *name);

/* ---- store/parameter enumeration (store.c shared) ------------------ */

/* All store names (sorted, deduplicated), plus "password-store" when
 * ~/.password-store exists. */
void store_list(secstore_t *s, strlist *out);
/* Parameters of one store in colon form (sorted, deduplicated). */
void param_list(secstore_t *s, const char *store, strlist *out);
/* 0 if "<store>/<param>.gpg" exists for the raw argument, else 1. Used
 * by `has` and by the dispatcher shortcut. Fatal on empty / traversal. */
int  store_param_has(secstore_t *s, const char *raw);
/* Absolute on-disk directory backing a (already-lowercased) store name. */
char *store_dir(secstore_t *s, const char *store);

/* ---- GPG orchestration (crypto.c) ---------------------------------- */

/* Append every GPG fingerprint whose uid e-mail equals `key`. */
void  gpg_fingerprint(secstore_t *s, const char *key, strlist *out);
/* Armored public key for `key` (or the local identity when key==NULL),
 * read from ~/.config/account/gpg/<key>.pub if present, else exported
 * from the local keyring. Caller frees (may be empty). */
char *gpg_export_public_key(secstore_t *s, const char *key);
/* Persist `data` as <key>.pub under ~/.config/account/gpg and import it. */
void  gpg_import_public_key(secstore_t *s, const char *key,
                            const char *data, size_t len);
/* Encrypt in[0..inlen) for the given recipient identities (or every known
 * account when nrec==0). Returns gpg's exit status; on success *out holds
 * the armored ciphertext (caller frees). */
int   gpg_encrypt(secstore_t *s, char **recipients, size_t nrec,
                  const char *in, size_t inlen, char **out, size_t *outlen);
/* Decrypt armored ciphertext. Returns gpg's exit status. */
int   gpg_decrypt(secstore_t *s, const char *in, size_t inlen,
                  char **out, size_t *outlen);

/* ---- account interop (account.c) ----------------------------------- */

char *account_config_dir(secstore_t *s);                 /* ~/.config/account  */
void  account_list(secstore_t *s, strlist *out);         /* names under ssh/   */
int   account_has_gpg_key(secstore_t *s, const char *account);
int   account_online(secstore_t *s, const char *account);
char *account_remote_url(secstore_t *s, const char *account, const char *purpose);

/* ---- recipients (recipients.c) ------------------------------------- */

/* Recipient identities for a store == `secret gpg-keys <store>`. */
void  store_recipients(secstore_t *s, const char *store, strlist *out);

/* ---- git convenience (sync.c) -------------------------------------- */

/* Run `git <args...>` inside `cwd`, silencing output. Returns status. */
int   git_quiet(secstore_t *s, const char *cwd, ...);    /* NULL-terminated */

/* ---- dispatch table (dispatch.c) ----------------------------------- */

typedef int (*secstore_cmd_fn)(secstore_t *, int, char **);
/* Look up a subcommand by name; NULL if it is not a known command. */
secstore_cmd_fn secstore_lookup(const char *name);

/* ---- source providers: import/export plugin system (source.c) ------ */
/* A "source" is an external secret store secret can move parameters to
 * and from. Built-in providers (e.g. the GNOME keyring) are entries in a
 * compiled-in table; external providers are `secret-source-<name>`
 * executables discovered in $libexecdir/secret/sources/ and on $PATH,
 * bridged through a small base64 line protocol. */
struct secret_source;
typedef struct secret_source {
	char *name;
	/* Backend usable right now? (e.g. secret-tool present). */
	int (*available)(secstore_t *s, const struct secret_source *self);
	/* Push every <store> parameter into the backend. */
	int (*do_export)(secstore_t *s, const struct secret_source *self,
	                 const char *store, int argc, char **argv);
	/* Pull the backend's items for <store> into the store. */
	int (*do_import)(secstore_t *s, const struct secret_source *self,
	                 const char *store, int argc, char **argv);
	char *path;   /* external providers: executable path; NULL if built-in */
} secret_source;

/* Resolve a provider by name (built-in table first, then an external
 * secret-source-<name> executable). Returns NULL if unknown; sets *owned
 * to 1 when the caller must secret_source_free() the result. */
secret_source *secret_source_lookup(secstore_t *s, const char *name, int *owned);
void           secret_source_free(secret_source *src);
/* "name\tavailable" / "name\tunavailable" lines for every provider. */
void           secret_source_list(secstore_t *s, strlist *out);

/* The built-in GNOME keyring provider (source_keyring.c). */
extern const secret_source secret_source_keyring;

/* ---- store value get/set (param.c, shared with source providers) --- */
/* Decrypt <store>/<param> into *out (caller frees). 0 on success, -1 if
 * the parameter is absent. */
int store_get_value(secstore_t *s, const char *store, const char *param,
                    char **out, size_t *outlen);
/* Encrypt `value` for the store's recipients and commit it. */
int store_set_value(secstore_t *s, const char *store, const char *param,
                    const char *value, size_t vlen);

/* ---- base64 (util.c) ----------------------------------------------- */
char          *b64_encode(const unsigned char *data, size_t len);
unsigned char *b64_decode(const char *text, size_t *out_len);

/* ---- parameter parsing (param.c / store.c shared) ------------------ */

/* Parse a raw "<store>/<param>" (or "<store>:<a>:<b>") argument the way
 * the shell commands do: lowercase, ':' -> '/', reject '..'. On success
 * returns 0 and sets *store and *param (both malloc'd, caller frees).
 * `require_slash` controls whether the missing-separator case is fatal
 * (set/get/etc.) or a soft non-zero return (has). Emits the same fatal
 * messages as the shell on the empty / bad-format / traversal paths. */
int parse_param(secstore_t *s, const char *raw, int require_slash,
                char **store, char **param);

#endif /* SECSTORE_INTERNAL_H */
