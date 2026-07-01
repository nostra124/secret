---
id: FEAT-226
type: feature
priority: medium
status: open
milestone: 0.16.0
---

# NIP-98 HTTP Auth header minting

## Description

**As a** user calling a Nostr-authenticated HTTP endpoint
**I want** `secret` to mint a NIP-98 `Authorization` header from my
Nostr identity
**So that** I can authenticate HTTP requests with a signed event instead
of a bearer token or password.

Cheap to build once FEAT-225 lands (it is just a specific event kind).

## Implementation

1. `secret nip98 <method> <url> [--payload-file f]` builds a kind-27235
   event with `u` (absolute URL) and `method` tags (and a `payload`
   SHA-256 tag when a body is supplied), signs it (FEAT-225), and prints
   `Authorization: Nostr <base64(event)>`.
2. Enforce the NIP-98 validity window (created_at recency) and exact
   URL/method binding when we *verify* (for use as a server-side check).
3. Provide a verification entry point in `libsecstore` so a future
   authenticated endpoint can validate an incoming NIP-98 header.

## Acceptance Criteria

1. A minted header round-trips through our own verifier: correct
   `u`/`method`/`payload` binding and a fresh `created_at` verify; a
   tampered URL, method, or stale timestamp is rejected.
2. The base64 payload decodes to a valid signed kind-27235 event.
3. Unit tests cover mint + verify, including each rejection case.
4. Documented in `docs/` with a curl example.
