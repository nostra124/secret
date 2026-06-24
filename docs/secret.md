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
can be pulled, pushed, and synchronised between peers over SSH.

The special store **`password-store`** is mapped onto `pass(1)`
(see `pass-init` below).

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

#### `identity`

Print the main GPG identity. Computed as `$(whoami)@$(hostname -f)`.
Exits non-zero if no identity has been set up yet.

```
secret identity
```

#### `setup`

One-shot central setup: ensure a GPG identity exists (generating
one if needed), export the public key to `~/.config/account/gpg/`
for interoperability with `account(1)`, and create the secret
store root directory.

```
secret setup
```

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
(generating one if needed), then re-initialise every existing store.

When called with a store, initialise `store` as a new git
repository under `$SELF_CONFIG`. For `password-store`, delegates
to `pass init`. For other stores, sets up `.gpg/recipients` with
the current identity and re-encrypts any pre-existing
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

QR-encode for terminal display (via `qrencode -t utf8`). Template-aware:
- a **wifi** entry (has `ssid` + `password`) yields a scannable
  `WIFI:T:…;S:…;P:…;H:…;;` join code;
- an **mfa** entry (has `secret`, standalone or at `<entry>/mfa/secret`)
  yields an `otpauth://totp/…` provisioning code;
- otherwise the parameter's raw value is encoded.

Requires `qrencode(1)`.

#### `clip <store>/<param>`

Copy a parameter's value to the system clipboard, then auto-clear it
after a timeout (default 45s; `$SECRET_CLIP_TIME`, `0` disables). The
backend is auto-detected — `wl-copy` (when `$WAYLAND_DISPLAY` is set),
`xclip`/`xsel` (when `$DISPLAY` is set), `pbcopy`, or `clip.exe` — or
overridden with `$SECRET_CLIP_CMD` (a shell command fed the value on
stdin).

```
secret clip bitcoin/api:key
```

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

Pull `store` from `account` over SSH. The remote URL is constructed
as `account:~/.config/secret/<store>` (or `account:~/.password-store`
for password-store). Merge conflicts are auto-resolved by taking the remote side
(`git checkout --theirs`).

- No args: pull every store from every account.
- `store` only: pull `store` from every account.
- `store account`: pull `store` from `account`.

If `account` is unreachable (SSH connection fails or `account`
doesn't have the peer registered), emit a warning and exit 0.

```
secret pull bitcoin alice
```

#### `push [store] [account]`

Mirror of `pull` for pushing. If the remote store doesn't exist
at the destination, runs `ssh $ACCOUNT $SELF init $store` first.

#### `sync [account]`

Pull every store from `account`, then push every store to
`account`. Same warn-on-unreachable behaviour as `pull`/`push`.

### HTTP sharing (pull only)

In addition to SSH peers, stores can be **pulled** over HTTP from
**groups** of endpoints. Pushing stays on SSH (`push`/`sync`). Only
GPG ciphertext and public keys are served, so the channel needs no
transport encryption to stay confidential. See [`sharing.md`](sharing.md).

#### `serve`

Run a read-only HTTP server exposing every store's git repository for
"dumb HTTP" git pull under `/app/secret/<store>`. Binds
`$SECRET_HTTP_BIND` (default `0.0.0.0`) on the port from `port` below.
Runs until interrupted.

```
secret serve
# peer:  git clone http://<host>:<port>/app/secret/<store>
```

#### `port`

Print the TCP port the server uses: `$SECRET_HTTP_PORT` if set, else a
value derived deterministically from the effective uid
(`49152 + uid % 16384`). A same-uid peer therefore knows the port
without configuration.

#### `groups [group]`

List sharing groups and their endpoints as `group<TAB>endpoint` lines
(all groups, or just `group`).

#### `group-add <group> <endpoint>` / `group-del <group> <endpoint>`

Add or remove an endpoint in a group (stored under
`$SELF_CONFIG/.groups/<group>`). An endpoint may be `host`,
`host:port`, or a full `http(s)://host[:port]` URL; a missing port
defaults to the local `port`.

#### `pull-http <store> [group]`

Pull `store` from a group's endpoints over HTTP (or from every group
when none is named) — cloning it if absent, otherwise pulling and
auto-resolving conflicts by taking the remote side. Pull only.

```
secret group-add team http://alice.example
secret pull-http bitcoin team
```

### Account encryption commands

#### `gpg-keys [store [param]]`

- No args: list every recipient key across every store, sorted and deduplicated.
- `store`: list recipient keys for that store.
- `store param`: extract the GPG key ids that encrypted that
  specific parameter (uses `gpg --list-packets`).

#### `add-gpg-key <store> <account>`

Add `account`'s public key (read from `~/.config/account/gpg/<account>.pub`)
to `store`'s `.gpg/recipients` and re-encrypt every parameter to include
the new recipient. Uses the `secret identity` format as the basis for
account names.

#### `del-gpg-key <store> <account>`

Remove `account`'s public key and re-encrypt without it.

#### `has-gpg-key <store> <account>`

Exit 0 if `account` is in `store`'s recipients, non-zero
otherwise.

### Data transfer (import / export)

`secret` can move parameters to and from other secret stores ("sources")
through a small provider plugin system. The first built-in provider is
`keyring` — the GNOME / freedesktop **Secret Service** (e.g.
`gnome-keyring`, KeePassXC), accessed by shelling out to `secret-tool(1)`.

#### `sources`

List the available providers, one per line as `name<TAB>status`, where
`status` is `available` or `unavailable`. Both built-in providers and
external `secret-source-<name>` plugins (see below) are listed.

```
secret sources
```

#### `export <source> <store>`

Push every parameter of `store` into `source`. For `keyring`, each
parameter becomes a Secret Service item with attributes
`application=secret store=<store> param=<param>` and label
`secret/<store>/<param>`.

```
secret export keyring bitcoin
```

#### `import <source> <store> [options]`

Pull `source`'s items for `store` back into the (already-existing) store,
encrypting each to the store's recipients.

```
secret import keyring bitcoin
```

For `keyring`, the default imports only items in `secret`'s own namespace
(the label/attributes above). To pull **foreign** items (stored by other
applications), select them by attribute:

```
secret import keyring personal --query application=org.gnome.Epiphany
secret import keyring personal --query app=mybrowser --prefix imported
```

- `--query attr=value` — repeatable; ANDed together (passed to
  `secret-tool search --all`).
- `--prefix p` — namespace the imported parameters under `p/`.

Each foreign item's label is sanitised into a parameter id (lowercased,
unsafe characters collapsed to `-`). Multiline values are reconstructed
from the `secret-tool` output.

`<store>` must already exist (run `secret init <store>` first); `..`
segments are rejected as for every other store argument (FEAT-207).
Options after `<store>` are forwarded to the provider, so external
plugins can take their own flags (e.g. the `keepass` plugin's
`--db PATH`).

##### KeePass (`keepass`)

A bundled external plugin (`secret-source-keepass`) bridges to a KeePass
`.kdbx` database via the pure-Python `pykeepass` library (no KeePassXC /
Qt needed). Each store maps to a KeePass group, each parameter to an
entry (title = parameter id, password field = value).

```
export KEEPASS_DB=~/secrets.kdbx KEEPASS_PASSWORD=…   # or --db / --keyfile
secret export keepass bitcoin
secret import keepass bitcoin
```

#### External source plugins

A provider that is not built in is resolved to an executable named
`secret-source-<name>`, searched for in `$PREFIX/libexec/secret/sources/`
and then on `$PATH`. The protocol is line-oriented and binary-safe via
base64 — see [`sources.md`](sources.md) for the full specification:

```
secret-source-<name> available        # exit 0 if the backend is usable
secret-source-<name> export <store>   # reads  "<param>\t<base64(value)>" lines on stdin
secret-source-<name> import <store>   # writes "<param>\t<base64(value)>" lines on stdout
```

### Template commands

Templates are named schemas of fields for common structured secrets. A
template carries no persistent state — `new` simply materialises each
field as its own parameter (`<entry>/<field>`), so the result is ordinary
parameters that every other verb (get/set/params/export/…) already
handles. See [`templates.md`](templates.md) for the full field lists.

#### `templates [name]`

List the built-in templates, or print one template's fields (with
`[secret]` / `[generated]` / `[optional]` / `[multiline]` flags and
defaults).

```
secret templates
secret templates wifi
```

#### `new <template> <store>/<entry> [field=value …] [--attach]`

Create an entry from `template`: each field is stored under
`<entry>/<field>`. Field values come from `field=value` arguments, the
template's defaults, auto-generation (for `[generated]` secret fields
left unset), or an interactive prompt (TTY only); `[optional]` fields are
skipped when unset. The store is auto-initialised if missing.

Extension templates (`mfa`, `passkey`) may be `--attach`ed to an existing
entry, in which case their fields land under `<entry>/<template>/<field>`.

```
secret new login   accounts/github username=octocat
secret new wifi    networks/home ssid=HomeNet password=hunter2 security=wpa3
secret new wallet  coins/btc "mnemonic=word1 word2 …" derivation=m/84'/0'/0'
secret new mfa     accounts/github --attach secret=JBSWY3DPEHPK3PXP
```

#### `otp <store>/<entry> [-c]`

Print the current TOTP code for an `mfa` entry, shelling out to
`oathtool(1)`. The seed is read from `<entry>/secret` (standalone) or
`<entry>/mfa/secret` (attached), honouring the entry's `algorithm`,
`digits` and `period` fields. With `-c` / `--clip`, copy the code to the
clipboard (auto-clearing) instead of printing it.

```
secret otp accounts/github          # print
secret otp accounts/github -c       # copy to clipboard
```

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
| `SECRET_HTTP_PORT` | Override the `serve`/`pull-http` port (default: `49152 + uid % 16384`). |
| `SECRET_HTTP_BIND` | Address `serve` binds to (default `0.0.0.0`). |
| `SECRET_CLIP_CMD` / `SECRET_CLIP_TIME` | Clipboard command override / auto-clear seconds (default 45). |

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

`secret` is self-contained and does not require `account(1)` to be
installed. It reads account configuration directly from
`~/.config/account/` for interoperability:

- **GPG public keys** — stored in `~/.config/account/gpg/<identity>.pub`
- **Remote account list** — `~/.config/account/ssh/` directory listing
- **Identity format** — computed directly as `$(whoami)@$(hostname -f)`

When `account(1)` is installed later, it finds all keys in the
expected locations.

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
