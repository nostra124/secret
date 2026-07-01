---
id: FEAT-224
type: feature
priority: high
status: open
milestone: 0.16.0
---

# Crypto foundation: native BIP-39/32 derivation and `secret derive`

## Description

**As a** wallet-template user
**I want** `secret` to derive keys and addresses from my stored mnemonic
**So that** the seed I already keep in a `wallet` entry becomes a usable
signing identity — the substrate every later Nostr/BIP-322 feature needs.

Today the `wallet` template stores `mnemonic` / `passphrase` /
`derivation` / `network` but nothing *uses* them (unlike `wifi`→`qr`,
`mfa`→`otp`). This lands the crypto engine and the first bound action.

## Scope note — CLAUDE.md policy change

Per the 2026-07-01 design round, cryptographic signing/derivation is now
**in scope** (CLAUDE.md §1 previously listed "cryptography primitives
separate from GPG" as out of scope). §2's "shell out to well-known tools"
gets a carve-out: **storage/sync still shells to gpg/git/ssh, but
signing/derivation is native C** on OpenSSL + libsecp256k1. This issue
includes the doc amendment so policy and code agree.

## Implementation

1. Build: link **OpenSSL libcrypto** and **libsecp256k1**; add
   `configure` probes with a clear error if headers/libs are missing;
   add both as runtime deps in every `packaging/*` manifest.
2. Derivation engine (new `lib/wallet.c` + `lib/bip.c`):
   - BIP-39 mnemonic + optional passphrase → seed (PBKDF2-HMAC-SHA512,
     2048 iterations) via libcrypto.
   - BIP-32 HD derivation (HMAC-SHA512 + libsecp256k1 tweak-add),
     hardened and non-hardened paths.
   - Address/key encodings: vendor bech32/bech32m and base58check
     (small, no crypto); HASH160 (SHA-256 + RIPEMD-160) via libcrypto.
3. Verb `secret derive <store>/<entry> [--path m/..'] [--as
   xpub|address|pubkey]`, duck-typed on the wallet template's
   `mnemonic` + `derivation` fields; `network` selects mainnet/testnet
   prefixes.
4. Extend the `wallet` template as needed (e.g. an optional `account`
   index) without breaking existing entries.

## Acceptance Criteria

1. `secret derive` reproduces known BIP-39/32/44 test vectors (assert
   against published vectors in a unit test).
2. bech32/bech32m and base58check round-trip against reference vectors.
3. The seed never touches disk unencrypted; derivation runs on the
   gpg-decrypted mnemonic in memory only.
4. `configure` fails clearly when OpenSSL or libsecp256k1 is absent.
5. CLAUDE.md §1/§2 reflect the new in-scope native-crypto policy.
6. Existing `wallet` entries created before this change still load.
