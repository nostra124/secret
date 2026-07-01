---
id: FEAT-221
type: feature
priority: medium
status: open
milestone: 0.16.0
---

# More source providers (1Password / Bitwarden / `.env` / cloud KMS)

## Description

**As a** user migrating into or out of `secret`
**I want** import/export providers for the secret stores I already use
**So that** `secret import`/`export` covers the common ecosystems beyond
the built-in `keyring` and bundled `keepass` providers.

The source-provider system (`lib/source.c`) already supports built-in
providers plus external `secret-source-<name>` executables speaking the
base64 line protocol documented in `docs/sources.md`. This adds more
providers as external plugins, requiring no core changes.

## Implementation

1. Add external plugins under `libexec/secret/sources/`:
   - `secret-source-onepassword` (via the `op` CLI)
   - `secret-source-bitwarden` (via the `bw` CLI)
   - `secret-source-env` (`.env` files — import/export flat KEY=VALUE)
   - `secret-source-kms` (a cloud KMS-backed provider)
2. Each implements the `available` / `export` / `import` verbs of the
   plugin protocol and is discovered at `<exe>/../libexec/secret/sources/`
   and on `$PATH`.
3. Add SIT coverage per provider where the backing CLI can be faked, and
   document each in `docs/sources.md`.

## Acceptance Criteria

1. `secret sources` lists the new providers when their backing tools are
   present.
2. `secret import <name>` and `secret export <name>` round-trip at least
   one entry for each new provider.
3. No changes to `lib/source.c` are required beyond documentation — the
   providers are pure external plugins.
4. Each provider is documented in `docs/sources.md`.
