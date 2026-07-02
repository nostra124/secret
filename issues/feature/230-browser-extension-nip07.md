---
id: FEAT-230
type: feature
priority: medium
status: open
milestone: 0.18.0
---

# Browser extension: `window.nostr` (NIP-07) via native-messaging bridge

## Description

**As a** user of Nostr web apps
**I want** a browser extension that provides `window.nostr` backed by my
local `secstored`
**So that** web apps sign with my GPG-protected key without the key ever
living in the browser.

We are not a remote bunker (NIP-46 is dropped); the extension reaches
`secstored` **locally**.

## Implementation

1. WebExtension (Chrome + Firefox) injecting a NIP-07 `window.nostr`:
   `getPublicKey`, `signEvent`, `getRelays`, `nip04.encrypt/decrypt`,
   `nip44.encrypt/decrypt`.
2. A **native-messaging host** (`secret` invoked as the browser's native
   host) bridges the extension's stdio messages to the `secstored` Unix
   socket, presenting the extension's per-client token (FEAT-228).
3. Each signing request is subject to `secstored`'s approval model
   (FEAT-228) — per-origin authorisation, surfaced so the user sees
   which site is asking.
4. Ship the extension + native-messaging manifests in-repo
   (`clients/webext/`) with install instructions.

## Acceptance Criteria

1. A Nostr web app can `getPublicKey` and `signEvent` through the
   extension; the signature verifies under the derived npub.
2. Signing requests honour `secstored`'s approval policy and show the
   requesting origin.
3. The key never enters the browser process — only signatures cross the
   native-messaging boundary.
4. Works on Chrome and Firefox with documented install steps.
