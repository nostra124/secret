# `secret` source providers — the import/export plugin protocol

> How `secret import` / `secret export` talk to other secret stores, and
> how to write your own source plugin. Companion to `docs/secret.md`
> (the CLI contract) and `lib/source.c` (the reference implementation).

## Concepts

A **source** is an external store `secret` can move parameters to and
from: the GNOME keyring, another password manager, a cloud vault, a flat
file, etc. Each provider implements three operations:

- **available** — is the backend usable right now?
- **export** — receive every parameter of a store and write it to the
  backend (`secret` → source).
- **import** — produce the backend's items for a store so `secret` can
  store them (source → `secret`).

Parameters are keyed by their **slash form** (`host/name`, the on-disk
layout), so a round-trip through any provider preserves the store's
structure.

## Two kinds of provider

1. **Built-in** — compiled into `libsecstore` as a `secret_source` entry
   (see `lib/source.c`'s table and `lib/source_keyring.c`). The only one
   today is `keyring`.
2. **External** — a standalone executable named `secret-source-<name>`,
   discovered at runtime in:
   1. `$PREFIX/libexec/secret/sources/`, then
   2. each directory on `$PATH`.

   External plugins can be written in any language; `secret` bridges to
   them with the protocol below. This is the extension point — drop an
   executable in place and `secret sources` picks it up, no rebuild.

## External plugin protocol

The plugin is invoked as `secret-source-<name> <verb> [<store>]`.

### `available`

```
secret-source-<name> available
```

Exit `0` if the backend is usable (credentials present, daemon running,
required CLI installed…), non-zero otherwise. Used by `secret sources`
and as a pre-flight check before `import`/`export`.

### `export <store>`

```
secret-source-<name> export <store>
```

`secret` writes the store's contents to the plugin's **stdin**, one
record per line:

```
<param>\t<base64(value)>\n
```

- `<param>` is the slash-form parameter id (`host/name`); it never
  contains a tab or newline.
- `<value>` is the raw secret bytes, base64-encoded (RFC 4648, standard
  alphabet) so arbitrary binary/multiline values are safe.

The plugin stores each record in its backend and exits `0` on success.

### `import <store>`

```
secret-source-<name> import <store>
```

The plugin writes records to its **stdout** in the exact same
`<param>\t<base64(value)>` form. `secret` decodes each value and stores
it under `<store>/<param>`, encrypting to the store's recipients. Lines
without a tab are ignored. The plugin exits `0` on success.

## Example: a flat-file plugin

```sh
#!/bin/sh
# secret-source-file — persist a store in $SECRET_FILE as the wire format.
: "${SECRET_FILE:?set SECRET_FILE to the backing file}"
case "$1" in
  available) exit 0 ;;
  export)    cat > "$SECRET_FILE" ;;          # records arrive on stdin
  import)    [ -f "$SECRET_FILE" ] && cat "$SECRET_FILE" ;;
  *) echo "usage: $0 available|export|import <store>" >&2; exit 2 ;;
esac
```

Make it executable and put it on `$PATH` (or in
`$PREFIX/libexec/secret/sources/`):

```
secret sources                 # -> file<TAB>available
secret export file mystore     # writes the store to $SECRET_FILE
secret import file mystore     # reads it back
```

## Built-in: `keyring` (GNOME / Secret Service)

The `keyring` provider shells out to `secret-tool(1)` (from `libsecret`).
Items are namespaced so import only ever touches `secret`'s own data:

| Field | Value |
|---|---|
| attribute `application` | `secret` |
| attribute `store` | the store name |
| attribute `param` | the slash-form parameter id |
| label | `secret/<store>/<param>` |

- **export**: `secret-tool store --label secret/<store>/<param>
  application secret store <store> param <param>` (value on stdin).
- **import**: `secret-tool search --all application secret store <store>`
  recovers the parameter ids from the item labels, then each value is
  fetched exactly with `secret-tool lookup … param <param>` and stored.

Requires a running Secret Service provider (e.g. `gnome-keyring-daemon`)
and `secret-tool` on `$PATH`.

## See also

- `docs/secret.md` — the `import` / `export` / `sources` CLI contract.
- `lib/source.c`, `lib/source_keyring.c` — the reference implementations.
- `secret-tool(1)`, the freedesktop Secret Service API.
