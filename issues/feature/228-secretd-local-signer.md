---
id: FEAT-228
type: feature
priority: high
status: open
milestone: 0.17.0
---

# `secretd`: local signer daemon over an authenticated Unix socket

## Description

**As a** user with local clients (the CLI, a browser-extension bridge)
that need signing without re-decrypting per call
**I want** a local daemon exposing the signer core over an authenticated
Unix socket
**So that** `window.nostr` / BIP-322 / derive requests are served
locally, with per-client authorisation and no network exposure.

Signatures are over plaintext requests, so this surface is **local
only** — never a TCP port, never remote.

## Implementation

1. `secretd` serves an RPC over a **Unix domain socket** (mode `0600`
   in a per-uid runtime dir) mapping to the FEAT-224/225/227 signer
   core: `derive`, `nostr.get-pubkey`, `nostr.sign-event`,
   `nostr.encrypt/decrypt`, `nip98`, `bip322.login`.
2. **Authentication, two layers**: (a) `SO_PEERCRED` — reject any peer
   whose uid ≠ the daemon's; (b) a per-client token presented in a
   handshake, so individual clients (e.g. the browser bridge) can be
   authorised and revoked independently of the CLI.
3. `secret` CLI verbs transparently use `secretd` when its socket is
   present, else run inline (same code path via the embedded core).

## Deferred sub-decisions (resolve in this issue)

- **Approval model** — per-request prompt, per-origin allowlist, or
  both, when a client (esp. the browser) asks to sign. This is the
  phishing defense and the product's UX core.
- **Unlock model** — re-decrypt the seed per operation via `gpg` /
  gpg-agent (consistent with GPG-at-rest, smaller exposure window) vs.
  hold the decrypted seed in the daemon for a session (faster, larger
  window). Leaning gpg-agent per-op.

## Acceptance Criteria

1. A different-uid process cannot connect (SO_PEERCRED rejection
   tested); a same-uid client without a valid token is refused.
2. A token-authorised client can invoke every signer RPC and gets
   results identical to the inline CLI path.
3. Revoking a client's token blocks it without affecting others.
4. The chosen approval + unlock models are implemented and documented.
5. No TCP listener is ever opened by `secretd`.
