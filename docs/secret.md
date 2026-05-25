# `secret(1)` — CLI contract

> The full subcommand surface of `secret`, with arguments, env
> vars, exit codes, and examples. Companion to
> `share/man/man1/secret.1` (groff) and `bin/secret help`. When
> the three disagree, this file is the source of truth.

## Synopsis

```
secret [options] <subcommand> [args...]
```

## Description

`secret` manages secrets in multiple **stores**. Each parameter is
GPG-encrypted at rest. Each store is a git repository, so changes
can be pulled, pushed, and synchronised between
`account(1)`-registered peers.

The special store **`password-store`** is mapped onto `pass(1)`
(see `pass-init` below).

## Options

| Flag | Effect |
|---|---|
| `-d` | Enable debug (shell tracing) mode (sets `SELF_DEBUG=1`) |
| `-q` | Enable quiet mode — suppress `info`-level output (sets `SELF_QUIET=1`) |
| `-f` | Force mode — passed through to `git push --force` and `git pull --force` (sets `SELF_FORCE=-f`) |

## Subcommands

### Generic

#### `help [subcommand]`

Print top-level help, or per-subcommand help if `subcommand` is
given.

#### `version`

Print the package version. Resolved by reading `VERSION`,
`.rpk/version`, then `share/secret/version` (see
`skills/version.md` §1).

#### `skills [name]`

List available skill files under `skills/`. With a `name` argument,
print the full text of that skill. Primarily used by coding agents
to load domain knowledge at runtime.

```
secret skills
secret skills secret-user
```

### Store management

#### `stores`

List existing stores: directory names under `$SELF_CONFIG`, plus
`password-store` if `~/.password-store` exists.

```
secret stores
```

#### `params <store>`

List every parameter in `store`, sorted and deduplicated. Output
uses `:` as the hierarchy separator (matching the `set`/`get`
input form).

```
secret params bitcoin
```

#### `init [store]`

When called without a store, ensure a GPG identity exists
(delegating to `account init` for key restoration or generation)
then re-initialise every existing store.

When called with a store, initialise `store` as a new git
repository under `$SELF_CONFIG`. For `password-store`, delegates
to `pass init`. For other stores, sets up `.gpg/recipients` with
the current account's identity and re-encrypts any pre-existing
parameters.

```
secret init                    # bootstrap GPG + re-init all stores
secret init bitcoin             # create or re-initialise a specific store
```

#### `pass-init`

Bootstrap the `pass(1)` password-store: verify `gpg` and `pass`
are on `$PATH`, run `pass init <gpg-fingerprint>`, configure
`pass git`'s remotes for `receive.denyCurrentBranch ignore` so
peers can push to it.

Fails fast if `gpg` or `pass` is missing — naming the missing
distro package in the error.

#### `exists <store>`

Exit 0 if `store` exists, non-zero otherwise.

#### `destroy <store>`

Remove `store` (`rm -rf`). For `password-store`, removes
`~/.password-store/`. For other stores, removes
`$SELF_CONFIG/<store>/`.

```
secret destroy bitcoin
```

#### `clean <store>`

Remove empty `.gpg` files from `store`. Useful after a botched
`secret set` left a zero-byte file.

```
secret clean bitcoin
```

### Store parameter commands

For every command in this group, `<store>/<param>` may also be
spelled `<store>/<a>:<b>` — colons are translated to slashes so
`host:name` becomes the file `host/name.gpg`.

`..` segments in either component are rejected (FEAT-207).

#### `ls [store]`

List every parameter across every store as `store/param` lines.
With `store` as a prefix, filters case-insensitively to that
store (FEAT-209).

```
secret ls
secret ls bitcoin
```

#### `has <store>/<param>`

Exit 0 if `<store>/<param>.gpg` exists, non-zero otherwise.

#### `set <store>/<param>`

Read stdin, encrypt to `store`'s recipients, write to
`<store>/<param>.gpg`. Commits the change to the store's git
repo with message `set secret value <param>`.

```
echo "$TOKEN" | secret set bitcoin/api:key
```

#### `get <store>/<param>`

Decrypt `<store>/<param>.gpg` to stdout. For `password-store`,
delegates to `pass <param>`.

```
secret get bitcoin/api:key
```

#### `del <store>/<param>`

`git rm` the encrypted file and commit. For `password-store`,
runs `pass rm -f <param>`.

#### `qr <store>/<param>`

Decrypt and pipe through `qrencode -t utf8` for terminal display.
Requires `qrencode(1)`.

#### `gen <store>/<param>`

Generate a 33-character alphanumeric random secret, encrypt and
store it under `<store>/<param>`, then print to stdout. If the
parameter already exists, just print the existing value.

```
secret gen bitcoin/api:key
```

#### `def <store>/<param> <value>`

Define a default: if `<store>/<param>` does not exist, encrypt
and store `<value>`. Always prints the resulting (existing or
new) value.

#### `ins <store>/<param>`

Insert a line into a multi-line parameter: read stdin, prepend
to the existing value, dedupe, re-encrypt.

```
echo "new-line" | secret ins bitcoin/notes
```

#### `rem <store>/<param>`

Remove a line from a multi-line parameter: read stdin, remove
matching lines from the existing value, re-encrypt.

#### `edit <store>/<param>`

Decrypt to a tempfile, open in `$EDITOR` (default `vi`),
re-encrypt on save, `shred` the tempfile.

### Synchronisation

#### `remotes [stores...]`

List git remotes for the named stores (or all stores). Output is
sorted and deduplicated.

#### `pull [store] [account]`

Pull `store` from `account` over SSH. The remote URL is resolved
via `account remote-url "$ACCOUNT" "secret/<store>"` (FEAT-044).
Merge conflicts are auto-resolved by taking the remote side
(`git checkout --theirs`).

- No args: pull every store from every account.
- `store` only: pull `store` from every account.
- `store account`: pull `store` from `account`.

If `account` is unreachable (`account online` returns non-zero),
emit a warning and exit 0.

```
secret pull bitcoin alice
```

#### `push [store] [account]`

Mirror of `pull` for pushing. If the remote store doesn't exist
at the destination, runs `ssh $ACCOUNT $SELF init $store` first.

#### `sync [account]`

Pull every store from `account`, then push every store to
`account`. Same warn-on-unreachable behaviour as `pull`/`push`.

### Account encryption commands

#### `gpg-keys [store [param]]`

- No args: list every recipient key across every store, sorted and deduplicated.
- `store`: list recipient keys for that store.
- `store param`: extract the GPG key ids that encrypted that
  specific parameter (uses `gpg --list-packets`).

#### `add-gpg-key <store> <account>`

Add `account`'s public key to `store`'s `.gpg/recipients` and
re-encrypt every parameter to include the new recipient.

#### `del-gpg-key <store> <account>`

Remove `account`'s public key and re-encrypt without it.

#### `has-gpg-key <store> <account>`

Exit 0 if `account` is in `store`'s recipients, non-zero
otherwise.

## Environment

| Variable | Effect |
|---|---|
| `XDG_CONFIG_HOME` | Defaults to `$HOME/.config`. Stores live under `$XDG_CONFIG_HOME/secret/`. |
| `XDG_SECRET_STORES` | Override the store root entirely (escape hatch — see `bin/secret:64`). Used by the bats sandbox. |
| `HOME` | Used to locate `~/.password-store/` for the `password-store` integration. |
| `EDITOR` | Used by `secret edit`. Defaults to `vi`. |
| `EUID` | When 0 (root), `$SELF_CONFIG` falls back to `/etc/secret/`. |
| `SELF_DEBUG` | When set (or `-d`), enables `set -vx` tracing and `debug` log output. |
| `SELF_QUIET` | When set (or `-q`), suppresses `info` log output. |
| `SELF_FORCE` | When set (or `-f`), passed as `--force` to `git push`/`pull`. |
| `SELF_VERBOSE` | Enables `set -vx` at a different point in the script (legacy). |
| `SELF_PLACE` | Defaults to `user`. Currently unused at runtime. |
| `SELF_PREFIX` | Optional sudo-style prefix for `mkdir -p` on the config dir. |

## Files

| Path | Purpose |
|---|---|
| `$XDG_CONFIG_HOME/secret/<store>/` | Per-store directory, git-tracked. Contains `.gpg/recipients` and per-parameter `<name>.gpg` files. |
| `/etc/secret/<store>/` | Same, when running as root (EUID=0). |
| `~/.password-store/` | The `pass(1)` password-store. Managed by `pass` itself, not by `secret`. |
| `VERSION` | Project-root canonical version (see `skills/version.md`). |
| `.rpk/version` | FEAT-194 legacy mirror of `VERSION`. |
| `.rpk/versions` | Append-only TSV ledger of `<version>\t<git-sha>` per release. |
| `share/secret/version` | Installed copy of `VERSION` (read when `bin/secret` is invoked from the installed prefix). |

## Exit status

| Code | Meaning |
|---|---|
| 0 | Success. |
| non-zero | Failure. The `fatal` handler prints `secret: fatal - <message>` on stderr and exits 255 (`exit -1` in bash). |

See `skills/logging.md` for the four log levels (`debug`, `info`,
`warn`, `error`) and the `fatal` (= `error + exit`) convenience.

## Cross-script dependencies

`secret` calls only one sibling script at runtime:

- **`account(1)`** — identity, GPG key management, SSH endpoint
  resolution (`account gpg-encrypt`, `account gpg-decrypt`,
  `account gpg-fingerprint`, `account identity`,
  `account gpg-export-public-key`, `account gpg-import-public-key`,
  `account has-gpg-key`, `account list`, `account online`,
  `account remote-url`).

Plus standard CLI tools:

- `git(1)` — every store is a git repo.
- `gpg(1)` — only for `gpg-keys <store> <param>` (the
  `--list-packets` path).
- `pass(1)` — for the `password-store` integration.
- `ssh(1)` — for `pull`/`push`/`sync` over the network.
- `qrencode(1)` — for `secret qr`.

The deliberately-duplicated-not-shared policy is documented in
`CLAUDE.md` §4–5.

## Examples

```sh
# Initialise a new store and put a value in it.
secret init bitcoin
echo "$API_TOKEN" | secret set bitcoin/api:key

# Read it back.
secret get bitcoin/api:key

# Generate a random secret in one shot.
secret gen postgres/password

# Sync every store with a peer.
secret sync alice@example.com

# Add a co-recipient to an existing store.
secret add-gpg-key bitcoin bob@example.com
```

## See also

- `share/man/man1/secret.1` — groff man page (mirror of this
  document).
- `CLAUDE.md` — agent guide and project conventions.
- `skills/secret-user.md` — agent skill describing typical
  workflows.
- `skills/version.md`, `skills/testing.md`, `skills/logging.md` —
  the development-process skills.
- `account(1)`, `pass(1)`, `gpg(1)`, `git(1)`.
