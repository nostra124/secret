---
name: secret-user
description: Operate the secret multi-store secret manager
long_description: Manage GPG-encrypted-at-rest secrets across multiple stores with git-backed sync between peers. Use when storing API tokens, generating random secrets, syncing secrets between machines, or managing password-store integration.
role: [user]
---

# secret-user

Manage secrets in multiple **stores**. Each parameter is GPG-encrypted
at rest. Each store is a git repository for syncing between peers.

## When to use

- "Store an API token" or "Set a secret"
- "Get a secret" or "Read an encrypted value"
- "Generate a random password"
- "Sync secrets between machines"
- "Add a recipient to a store"
- "Bootstrap password-store"
- "List secrets in a store"

## The model

A **store** is a git repository at `$XDG_CONFIG_HOME/secret/<store>/`:

```
secret/<store>/
  .gpg/recipients      # who can decrypt
  .gpg/<account>.pub   # each recipient's public key
  <param>.gpg          # GPG-encrypted parameter
  <sub>/<param>.gpg    # nested parameters
```

A **parameter** is identified by `<store>/<sub>/<param>`.

The special **password-store** maps onto `pass(1)`.

## Workflow

### 1. Bootstrap

```bash
# Setup GPG identity (if needed)
secret setup

# Bootstrap password-store
secret pass-init

# Create a regular store
secret init bitcoin
```

### 2. Store and retrieve

```bash
# Encrypt a value
echo "$TOKEN" | secret set bitcoin/api/key

# Read a value
secret get bitcoin/api/key

# Generate a random 33-char password
secret gen bitcoin/api/token

# List parameters in a store
secret ls bitcoin
```

### 3. Sync between peers

```bash
# Sync all stores with peer
secret sync alice@example.com

# Push/pull specific store
secret push bitcoin alice@example.com
secret pull bitcoin alice@example.com
```

### 4. Manage recipients

```bash
# Add a recipient
secret add-gpg-key bitcoin bob@example.com

# List recipients
secret gpg-keys bitcoin
```

### 5. QR code

```bash
secret qr bitcoin/api/key
```

## Guardrails

1. **Never log secret values.** `secret get` writes to stdout — pipe carefully.
2. **Don't store secrets in `config`.** `config` is plaintext; `secret` is for credentials.
3. **`secret destroy <store>` is permanent.** It `rm -rf`'s the git repo.
4. **Pulling from a peer?** Run `secret init <store>` after pull to re-encrypt for new recipients.
5. **`-f` (force) on push/pull** bypasses conflict safety. Use only when you know which side wins.

## Files

| Path | Purpose |
|------|---------|
| `$XDG_CONFIG_HOME/secret/<store>/` | Store directory (git-tracked) |
| `~/.password-store/` | pass(1) password-store |
| `~/.config/account/gpg/<identity>.pub` | GPG public keys for interoperability |

## See also

- `man secret` — full command reference
- `man pass` — password-store(1) documentation