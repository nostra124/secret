---
name: secret-user
description: Operate the secret multi-store secret manager
long_description: |
  Operate the `secret` multi-store secret manager — put,
  get, list, sync GPG-encrypted-at-rest secrets across
  account-registered peers, including the special
  `password-store` (pass(1)) integration. Trigger when
  the user wants to store an API token, generate a
  random secret, sync secrets between machines, or learn
  the GPG-at-rest + git-backed-sync model.
---

# `secret-user` skill

## 1. Design principles

`secret` follows the four collection principles —
**educational, functional, decentralized, simple** —
specialised:

- **Educational.** Every parameter is just a
  GPG-encrypted file in a git directory. Reading
  `bin/secret` end-to-end teaches the GPG-at-rest +
  git-sync model.
- **Functional.** Each verb is a pure transformation
  over the filesystem: read a file, decrypt, print;
  read stdin, encrypt, write. Sync is git pull/push.
- **Decentralized.** No central secret server; each
  account holds its own stores. Sync is symmetric
  pull/push between peers.
- **Simple.** `secret` calls only `account` at
  runtime. SSH endpoints come from
  `account remote-url`.

## 2. The model

A **store** is a directory at
`$XDG_CONFIG_HOME/secret/<store>/`. It's a git
repository with one GPG-encrypted file per parameter:

    secret/<store>/
      .gpg/recipients      # who can decrypt
      .gpg/<account>.pub   # each recipient's public key
      <param>.gpg          # GPG-encrypted parameter
      <sub>/<param>.gpg    # nested

A **parameter** is identified by `<store>/<sub>/<param>`.
Reading takes `account gpg-decrypt`; writing takes
`account gpg-encrypt $(secret recipients <store>)`.

The special **password-store** maps onto `pass(1)`:
`secret pass-init` bootstraps it (FEAT-041);
`secret get password-store/<name>` and friends delegate
to `pass`.

## 3. Workflow recipes

1. **Bootstrap the pass(1) password-store.**

       secret pass-init

   Verifies `gpg` and `pass` are on PATH, then runs
   `pass init $(account gpg-fingerprint $(account
   identity))` and `pass git init`.

2. **Create a regular store.**

       secret init bitcoin

3. **Encrypt a value.**

       echo $TOKEN | secret set bitcoin/api/key

4. **Read a value.**

       secret get bitcoin/api/key

5. **List parameters.**

       secret ls bitcoin

6. **Generate a random parameter.**

       secret gen bitcoin/api/token > /dev/null
       secret get bitcoin/api/token

7. **Sync stores between peers.**

       secret sync alice@example.com
       secret push bitcoin alice@example.com
       secret pull bitcoin alice@example.com

   SSH endpoints come from
   `account remote-url <peer> secret/<store>` (FEAT-044).

8. **Render as QR for offline transfer.**

       secret qr bitcoin/api/key

9. **Add a recipient to a store.**

   Add the new account's public key under
   `<store>/.gpg/<account>.pub`, then
   `secret init <store>` to re-encrypt every parameter
   with the updated recipient set.

## 4. Guardrails

1. **Never log a secret value.** `secret get` writes to
   stdout — pipe carefully. `set -x` shell tracing
   captures pipelines verbatim. `secret -d` enables
   tracing; only use it on an isolated machine.
2. **Don't store secrets in `config`** — it's
   plaintext. `config` is for non-sensitive
   configuration; `secret` is for credentials.
3. **`secret destroy <store>` is permanent.** It
   `rm -rf`'s the git repo. Take a backup if
   uncertain.
4. **Pulling adds a recipient: re-encrypt.** When you
   sync from a peer that has additional recipients in
   its `<store>/.gpg/recipients`, `secret init <store>`
   afterward to re-encrypt every parameter for the
   updated recipient set.
5. **`-f` (force) on push/pull bypasses safety.** Use
   only when you've inspected the conflict and know
   which side wins.

## 5. Where to read more

- `man secret` — full reference (synopsis, every
  subcommand, env, files, examples).
- `man pass` — the password-store(1) tool that backs
  `password-store/`.
- This package's `CLAUDE.md` — developer-side
  conventions, the no-shared-lib policy, intentional
  duplications.
