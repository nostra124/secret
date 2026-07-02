---
id: FEAT-227
type: feature
priority: high
status: open
milestone: 0.17.0
---

# BIP-322 "sign in with Bitcoin" login flow

## Description

**As a** user logging into a service that supports Bitcoin sign-in
**I want** to point `secret` at a login QR/URL and have it prove control
of a per-service address
**So that** I authenticate with a signature instead of a password, using
a distinct unlinkable identity per site.

## Flow

1. Input: a login URL directly, or an image decoded with `zbarimg`
   (`secret bip322-login <url|image.png>`). Live camera scanning is the
   iOS/Android apps' job, not core.
2. **Per-service derivation** (decision 2a — LUD-04 / LNURL-auth style):
   derive a deterministic key/address from the wallet seed indexed by
   the URL's registrable domain, so each service gets a distinct,
   reproducible address with nothing stored. The derivation convention
   is documented as a `secret` spec (no canonical BIP-322 equivalent
   exists).
3. **Structured challenge** (decision 3a): extract the nonce/challenge
   from the URL and sign a canonical message binding **domain + nonce**
   (SIWE/CAIP-122-like), never the raw attacker-controlled URL. Display
   the domain and require confirmation before signing.
4. **BIP-322** message signing (simple/full per address type; start with
   P2WPKH + P2TR) using the derived key.
5. POST `{address, signature, challenge}` (documented JSON) to the
   service callback; the service re-derives/verifies. Address is
   derivable from the domain, so it need not be pre-registered.

## Security

Signing an attacker-supplied blob is a blind-signing/phishing risk;
binding the signed message to the domain + a server nonce and confirming
the domain mitigates replay and cross-site reuse.

## Acceptance Criteria

1. BIP-322 signatures verify against reference vectors for the supported
   address types.
2. The same (seed, domain) always yields the same address; different
   domains yield different, unlinkable addresses.
3. The signed message is the structured domain+nonce form, not the raw
   URL; the domain is shown and confirmed before signing.
4. `zbarimg` decodes an image input; a bare URL also works; neither path
   requires a camera.
5. The `{address, signature, challenge}` wire format and the derivation
   convention are documented in `docs/`.
