# `secret` templates — structured entry types

> Templates capture common multi-field secrets (logins, Wi-Fi, MFA,
> passkeys, wallets). Companion to `docs/secret.md` (the CLI contract)
> and `lib/template.c` (the implementation).

## Model

A **template** is a named, ordered set of **fields**. It is *just a
schema* — there is no per-store binding and nothing template-specific is
written to disk. `secret new <template> <store>/<entry>` materialises an
entry by storing each field as its own parameter:

```
<entry>/<field>      →  <store>/<entry>/<field>.gpg
```

Because the result is ordinary parameters, every other verb already
works on them:

```
secret new login accounts/github username=octocat
secret get    accounts/github:username      # octocat
secret params accounts                       # github:password, github:url, …
secret export keyring accounts                # the whole entry moves
```

### Field flags

| Flag | Meaning |
|---|---|
| `[secret]` | sensitive value (prompt is not echoed-back as a default) |
| `[generated]` | auto-generated (33-char alphanumeric) when left unset |
| `[optional]` | skipped when unset |
| `[multiline]` | value may span multiple lines |

Field values are resolved in this order: a `field=value` argument → the
template default → auto-generation (`[generated]` only) → an interactive
prompt (only when stdin is a TTY) → fatal (required field, non-interactive).

## Built-in templates

| Template | Kind | Fields |
|---|---|---|
| `login` | entry | `username`, `password` *(secret, generated)*, `url` *(opt)*, `notes` *(opt, multiline)* |
| `wifi` | entry | `ssid`, `password` *(secret)*, `security` *(=wpa2)*, `hidden` *(=false)*, `identity` *(opt)* |
| `mfa` | extension | `secret` *(secret)*, `issuer` *(opt)*, `account` *(opt)*, `algorithm` *(=sha1)*, `digits` *(=6)*, `period` *(=30)* |
| `passkey` | extension | `rp_id`, `user_name` *(opt)*, `user_handle` *(opt)*, `credential_id` *(opt)*, `private_key` *(secret, multiline)*, `algorithm` *(=es256)*, `sign_count` *(=0)* |
| `wallet` | entry | `mnemonic` *(secret, multiline)*, `passphrase` *(secret, opt)*, `derivation` *(=m/44'/0'/0')*, `network` *(=mainnet)*, `label` *(opt)* |

`secret templates` lists these; `secret templates <name>` prints a
template's fields and defaults.

## Extensions: standalone or attached

`mfa` and `passkey` are **extension** templates: they can stand alone as
their own entry, or be **attached** to a base entry with `--attach`, in
which case their fields nest under `<entry>/<template>/`:

```
# standalone
secret new mfa accounts/github-totp secret=JBSWY3DPEHPK3PXP
secret otp accounts/github-totp            # reads github-totp/secret

# attached to an existing login entry
secret new login accounts/github username=octocat
secret new mfa   accounts/github --attach secret=JBSWY3DPEHPK3PXP
secret otp accounts/github                  # reads github/mfa/secret
```

## TOTP codes (`otp`)

`secret otp <store>/<entry>` prints the current time-based one-time
password for an `mfa` entry by shelling out to `oathtool(1)`. It looks
for the seed at `<entry>/secret` first, then `<entry>/mfa/secret`, and
honours the entry's `algorithm` (`sha1`/`sha256`/`sha512`), `digits` and
`period` fields. Requires `oathtool` on `$PATH` (Debian/Ubuntu:
`oathtool`).

## Passkeys

The `passkey` template is a **backup/transport** schema for software
(resident) WebAuthn credentials — `secret` stores the fields but does not
itself perform passkey assertions. Hardware-bound passkeys are
non-exportable by design and cannot be captured this way.

## See also

- `docs/secret.md` — the `templates` / `new` / `otp` CLI contract.
- `lib/template.c` — the template registry and commands.
- `oathtool(1)`, RFC 6238 (TOTP), WebAuthn / FIDO2.
