---
milestone: 0.16.0
theme: signing & identity — native crypto, Nostr/BIP-322 signing, secretd, mTLS peer sync
opened: 2026-07-01
status: planned
---

# ROADMAP 0.16.0 — signing & identity

> Planning backlog for the "signing & identity" arc, agreed in a design
> round on 2026-07-01. `secret` gains **native cryptographic signing**
> built on the `wallet` seed template: Nostr (NIP-06/07/98), Bitcoin
> BIP-322 sign-in, a local signer daemon (`secretd`), and mTLS peer
> sync. Spans 0.16.0 → 0.18.0. **Bugs come before features at the same
> priority.**

## The redesign, in one paragraph

The signer core lives in `libsecstore` (C, linking **OpenSSL libcrypto**
+ **libsecp256k1**) so it is reachable three ways: **inline** by the CLI,
**embedded** by the mobile apps (local stores, no daemon), and over an
**authenticated Unix socket** by `secretd`'s local clients (the CLI and a
browser-extension bridge). The only network surface is **secretd ↔
secretd store sync over mTLS** — encrypted git repos replicate between
your own daemons, mutually authenticated by pinned (TOFU) certs. Remote
*signing* does not exist: signatures over plaintext never leave the box.

## Locked decisions (design round, 2026-07-01)

| # | Decision |
|---|---|
| Crypto engine | **In-core C**, no shell-out. Link **OpenSSL libcrypto** (SHA-2/HMAC/PBKDF2/RIPEMD-160/AES/ChaCha20/HKDF) + **libsecp256k1** (ECDSA/Schnorr/ECDH/BIP-32 tweak). Vendor only bech32/bech32m + base58check. |
| BIP-322 address | **Per-service, domain→path** deterministic derivation (LUD-04 / LNURL-auth style). Unlinkable across sites, reproducible, nothing stored. |
| What is signed | **Structured challenge** binding domain + nonce (SIWE/CAIP-122-like), not the raw URL. Show domain, confirm before signing. |
| QR input | **URL or image** (`zbarimg`); no camera in core. Live camera scanning is the iOS/Android apps' job. |
| Browser reach | **Local only** — WebExtension implements `window.nostr` (NIP-07) and bridges to `secretd` via a native-messaging host over the Unix socket. |
| Local transport | **Unix domain socket**, authenticated: `SO_PEERCRED` (same uid) **and** a per-client token so clients can be individually authorised/revoked. |
| Mobile | **Embedded `libsecstore`**, local stores, in-process. No daemon, no IPC. |
| Peer sync | **mTLS, replacing** git-over-ssh + dumb-HTTP. **TOFU** cert pinning (peer certs stored alongside account keys). |
| ~~NIP-46 / remote bunker~~ | **Dropped** — remote signing is out of scope; there are no remote clients to `secretd`. |

Two sub-decisions are deliberately deferred to **before FEAT-228**:
**approval model** (per-request prompt vs per-origin allowlist vs both)
and **unlock model** (gpg-agent re-decrypt per op vs session-held seed).

## 0.16.0 — crypto foundation & Nostr

- **FEAT-224** — crypto foundation: link OpenSSL + libsecp256k1;
  BIP-39 mnemonic→seed, BIP-32 HD derivation, bech32/base58 encoding;
  `secret derive`; extend the `wallet` template; amend CLAUDE.md scope.
- **FEAT-225** — Nostr signing core: NIP-06 key derivation, BIP-340
  event signing, NIP-04/44 encrypt/decrypt; `secret nostr` verbs.
- **FEAT-226** — NIP-98 HTTP Auth: mint `Authorization: Nostr <b64>`
  headers from a signed kind-27235 event.

## 0.17.0 — Bitcoin sign-in & the local daemon

- **FEAT-227** — BIP-322 login: message signing, per-domain derivation,
  structured-challenge builder, `secret bip322-login <url|image>`,
  documented `{address, signature, challenge}` wire format.
- **FEAT-228** — `secretd`: local signer daemon over an authenticated
  Unix socket (SO_PEERCRED + per-client token); RPC to the signer core;
  approval + unlock policy (resolve the two deferred sub-decisions).

## 0.18.0 — mTLS sync & the browser

- **FEAT-229** — mTLS peer sync **replacing** ssh/HTTP: secretd↔secretd
  encrypted-store replication, mutual X.509 with TOFU pinning; retire
  `serve` / `pull-http` / `groups` / git-over-ssh. Likely a **major**
  bump (removes the #13 sharing CLI contract).
- **FEAT-230** — browser extension: `window.nostr` (NIP-07) via a
  native-messaging bridge to the `secretd` Unix socket.

## Interactions with the 0.15.0 forward-look

- **FEAT-220** (HTTPS + auth for `serve`, mDNS discovery) is
  **superseded** by FEAT-229 — the whole HTTP-sharing surface it built
  on is retired in favour of mTLS. Fold any still-wanted discovery into
  FEAT-229.
- **FEAT-222** (GPG-signed store commits) stays complementary: mTLS
  authenticates the *transport*, signed commits authenticate the
  *history*.
- **FEAT-219** (user templates) and **FEAT-221** (more source providers)
  are independent and unaffected.
