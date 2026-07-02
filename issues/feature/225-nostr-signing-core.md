---
id: FEAT-225
type: feature
priority: high
status: open
milestone: 0.16.0
---

# Nostr signing core: NIP-06 keys, event signing, NIP-04/44

## Description

**As a** Nostr user
**I want** `secret` to derive my Nostr identity from my wallet seed and
sign events / encrypt DMs locally
**So that** my key lives in a GPG-encrypted store and never in a browser
extension or a plaintext file.

Builds directly on FEAT-224's derivation engine.

## Implementation

1. **NIP-06** key derivation: `m/44'/1237'/<account>'/0/0` from the
   wallet mnemonic → secp256k1 keypair; expose `npub`/`nsec`
   (bech32) via `secret nostr get-pubkey [--account N]`.
2. **Event signing** (BIP-340 Schnorr via libsecp256k1): canonical
   serialization → SHA-256 id → Schnorr sig. `secret nostr sign-event`
   reads an unsigned event JSON on stdin, emits the signed event.
3. **NIP-04** (legacy DM): ECDH + AES-256-CBC via libcrypto.
   **NIP-44** (v2): ECDH → HKDF-SHA256 → ChaCha20 + HMAC-SHA256.
   `secret nostr encrypt|decrypt --peer <npub>`.
4. Deterministic-nonce Schnorr (BIP-340 aux) so signatures are testable
   against fixed vectors.

## Acceptance Criteria

1. Derived npub matches published NIP-06 test vectors for a known
   mnemonic.
2. A signed event verifies under its pubkey (verify in-test with
   libsecp256k1); the event id is the SHA-256 of the canonical form.
3. NIP-44 encrypt→decrypt round-trips and matches the NIP-44 v2 test
   vectors; NIP-04 round-trips.
4. Private key material is only ever derived in memory from the
   gpg-decrypted mnemonic.
5. All Nostr verbs are covered by unit tests (tool-free where the
   vectors allow) and documented in `docs/`.
