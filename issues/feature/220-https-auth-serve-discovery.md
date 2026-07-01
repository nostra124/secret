---
id: FEAT-220
type: feature
priority: medium
status: open
milestone: 0.16.0
---

# HTTPS-native `serve` with optional auth and peer discovery

## Description

**As a** user sharing stores across a LAN or the internet
**I want** `secret serve` to speak HTTPS with optional authentication,
and groups to self-populate via discovery
**So that** pull-only HTTP sharing (FEAT-217/#13) is safe over untrusted
networks and less manual to configure.

Today `serve` exposes stores over plain HTTP at
`/app/secret/<store>` on a uid-derived port, and groups are populated by
hand with `group-add`. Stores are GPG-encrypted at rest so the payload is
already confidential, but transport metadata is exposed and endpoints
must be typed in by hand.

## Implementation

1. Add HTTPS to `lib/http.c` (TLS cert path via config/flag), keeping
   plain HTTP available for loopback/testing.
2. Optional bearer/basic auth on `serve`, checked before serving refs;
   pull-http learns to send the credential from the group entry.
3. Peer discovery (mDNS/DNS-SD) so `groups` can self-populate reachable
   `secret serve` endpoints on the local network; discovery is
   advisory — pulling still requires an explicit group.
4. Keep it pull-only (push stays over SSH, per the sharing design).

## Acceptance Criteria

1. `secret serve --tls ...` serves stores over HTTPS; `pull-http` clones
   over `https://` endpoints.
2. An authenticated `serve` rejects unauthenticated pulls and accepts
   pulls carrying the group's credential.
3. Discovered peers appear in `groups` output without manual
   `group-add`.
4. Plain-HTTP loopback still works for the SIT round-trip test.
