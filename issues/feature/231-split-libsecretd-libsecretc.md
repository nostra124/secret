---
id: FEAT-231
type: feature
priority: high
status: open
milestone: 0.16.0
---

# Split the library: `libsecretd` (engine/daemon) + `libsecretc` (client)

## Description

**As a** developer integrating `secret` into another app
**I want** a small, dependency-light client library to talk to a running
`secretd`
**So that** I can consume signing/store operations without linking the
whole engine (OpenSSL + libsecp256k1 + gpg/git shell-outs).

Today everything is one library, `libsecstore`. Going forward we split
along the daemon boundary into two:

- **`libsecretd`** — the **engine + daemon** (today's `libsecstore`,
  renamed): store lifecycle (gpg/git), the signer core (OpenSSL +
  libsecp256k1), socket serving + auth + approval/unlock, and mTLS peer
  sync. Linked by the `secretd` binary, the `secret` CLI (inline mode),
  and the mobile apps (embedded, in-process, no daemon).
- **`libsecretc`** — the **client**: speaks the `secretd` RPC over the
  Unix socket (connect → token handshake → marshal
  `derive`/`sign`/`nostr`/`bip322`/`nip98` → parse). **No crypto**, so
  it stays tiny and cheap to link and bind. Linked by consumer apps, the
  browser native-messaging bridge, and the CLI in client mode.

The **wire protocol** is the contract between the two, defined in
FEAT-228; both libraries implement it.

## Naming

`libsecstore` avoided the GNOME/freedesktop **libsecret** collision
(CLAUDE.md §4). `libsecretd`/`libsecretc` sit next to that name; the
`-lsecretd`/`-lsecretc` sonames disambiguate at link time. Names are
provisional pending sign-off; alternatives considered:
`libsecstore` + `libsecstore-client`.

## Implementation

1. **Rename** `libsecstore` → `libsecretd` across `Makefile.in`,
   `configure`, the public header (`secstore.h` → `secretd.h`), every
   `packaging/*` manifest (deb/rpm/apk/macOS), and CLAUDE.md §2/§4 + docs.
   Keep the `secstore_*` symbol prefix or rename to `secretd_*` — decide
   and apply consistently (pre-1.0, no ABI stability promised yet).
2. **Add `libsecretc`**: new `client/` sources + public header
   (`secretc.h`) implementing the RPC to `secretd`. Minimal deps (no
   OpenSSL/libsecp256k1). Ships `.so`/`.a` + header like the engine.
3. `make install` stows both libraries, both headers, and pkg-config
   files (`libsecretd.pc`, `libsecretc.pc`).
4. The `secret` CLI links `libsecretc` for client mode and `libsecretd`
   for inline mode; document when each is used.

## Sequencing

The **rename** (step 1) lands in 0.16.0 so later signing code is written
under the final name. The **client library** (step 2) completes
**alongside FEAT-228**, once the RPC wire protocol exists.

## Acceptance Criteria

1. `make` builds `libsecretd.{so,a}` and `libsecretc.{so,a}`; `bin/secret`
   still runs from the source tree without `LD_LIBRARY_PATH`.
2. `libsecretc` links without OpenSSL/libsecp256k1 (verified with `ldd`).
3. A minimal example program links only `libsecretc` + `secretc.h` and
   performs a `derive`/`sign` round-trip against a running `secretd`.
4. All packages install both libraries, both headers, and both
   pkg-config files.
5. CLAUDE.md §2/§4, `docs/`, and the man page reflect the two-library
   layout and the retained no-`libsecret` policy rationale.
