---
id: FEAT-231
type: feature
priority: high
status: open
milestone: 0.16.0
---

# Add `libsecstore-client`: a thin client library alongside `libsecstore`

## Description

**As a** developer integrating `secret` into another app
**I want** a small, dependency-light client library to talk to a running
`secstored`
**So that** I can consume signing/store operations without linking the
whole engine (OpenSSL + libsecp256k1 + gpg/git shell-outs).

Today there is one library, `libsecstore`. Going forward we split along
the daemon boundary — the engine keeps its name, and a new client library
is added:

- **`libsecstore`** (unchanged) — the **engine + daemon** library: store
  lifecycle (gpg/git), the signer core (OpenSSL + libsecp256k1), socket
  serving + auth + approval/unlock, and mTLS peer sync. Linked by the
  `secstored` binary, the `secret` CLI (inline mode), and the mobile apps
  (embedded, in-process, no daemon).
- **`libsecstore-client`** (new) — the **client**: speaks the `secstored`
  RPC over the Unix socket (connect → token handshake → marshal
  `derive`/`sign`/`nostr`/`bip322`/`nip98` → parse). **No crypto**, so it
  stays tiny and cheap to link and bind. Linked by consumer apps, the
  browser native-messaging bridge, and the CLI in client mode.

The **wire protocol** is the contract between the two, defined in
FEAT-228; both implement it.

## Naming (decided)

The engine keeps **`libsecstore`** and the **`secstore_*`** symbol prefix
— deliberately not `libsecret*`, to preserve the no-collision rationale
with GNOME/freedesktop **libsecret** (CLAUDE.md §4). The client library is
**`libsecstore-client`** with symbols under the retained
`secstore_client_*` family. The daemon binary is **`secstored`**.

## Implementation

1. **No engine rename.** `libsecstore`, `secstore.h`, and the `secstore_*`
   symbols stay as-is.
2. Add the **`secstored`** daemon binary (built from `libsecstore`) — see
   FEAT-228.
3. Add **`libsecstore-client`**: new `client/` sources + public header
   `secstore_client.h` (symbols `secstore_client_*`) implementing the RPC
   to `secstored`. Minimal deps — **no** OpenSSL/libsecp256k1. Ships
   `.so`/`.a` + header like the engine.
4. `make install` stows both libraries, both headers, and pkg-config
   files (`libsecstore.pc`, `libsecstore-client.pc`); package manifests
   updated across deb/rpm/apk/macOS.
5. The `secret` CLI links `libsecstore-client` for client mode and
   `libsecstore` for inline mode; document when each is used.
6. CLAUDE.md §2/§7 + docs describe the two-library layout.

## Sequencing

The client library completes **alongside FEAT-228**, once the RPC wire
protocol exists. Nothing here renames existing artifacts, so it does not
disturb the current build.

## Acceptance Criteria

1. `make` builds `libsecstore.{so,a}` and `libsecstore-client.{so,a}`;
   `bin/secret` still runs from the source tree without `LD_LIBRARY_PATH`.
2. `libsecstore-client` links **without** OpenSSL/libsecp256k1 (verified
   with `ldd`).
3. A minimal example links only `libsecstore-client` + `secstore_client.h`
   and performs a `derive`/`sign` round-trip against a running `secstored`.
4. All packages install both libraries, both headers, and both pkg-config
   files.
5. CLAUDE.md §2/§7 and `docs/` reflect the two-library layout; the
   no-`libsecret` rationale is retained.
