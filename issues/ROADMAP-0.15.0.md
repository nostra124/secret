---
milestone: 0.15.0
theme: correctness & test hardening; then sharing/source depth toward 1.0
opened: 2026-07-01
status: active
---

# ROADMAP 0.15.0 — and the horizon to 1.0

> Planning backlog per `skills/milestones.md`. `secret` is now a complete
> C tool (GPG-encrypted multi-store manager; SSH + HTTP sync; a source
> plugin system; structured templates; native packaging). This roadmap
> captures what's next. **Bugs come before features at the same priority.**

## Context — shipped (all merged to `master`)

| Release / PR | What |
|---|---|
| #8 | C reimplementation — `libsecstore` + `secret` CLI |
| #9 | Native packaging (deb / rpm / apk / macOS pkg) |
| #10 | Import/export **sources** + `keyring` + external plugin system |
| #11 | KeePass plugin + foreign keyring import (`--query`) |
| #12 | Structured **templates** + WiFi/MFA QR + clipboard |
| #13 | **HTTP sharing** (`serve` / groups / `pull-http`) + git-identity fix |
| #14 | Test layering (SIT/PIT) + fault injection → ~89% coverage *(in review)* |

Latest tagged release: **0.14.4**.

## 0.15.0 — correctness & test hardening

Land #14 first, then:

- **BUG-216** — `password-store` `set`/`get` do not round-trip (legacy
  pass-insert entry-naming carried verbatim from the shell version).
- **FEAT-217** — raise coverage to ~95%+ and run the SIT suite in CI
  (install the tools) with a `SECRET_COV_MIN` gate.
- **FEAT-218** — run the rpm/apk PIT legs on a runner whose container
  registry/mirrors are reachable (recipes are ready; the sandbox TLS
  proxy blocked them).

## 0.16.0 — sharing & source depth

- **FEAT-219** — user-defined named templates (`secret new <custom>`).
- **FEAT-220** — HTTPS-native + optional auth for `serve`; peer discovery
  (mDNS) so groups self-populate.
- **FEAT-221** — more source providers (1Password / Bitwarden CLI, `.env`
  files, cloud KMS) as external `secret-source-*` plugins.

## 1.0.0 — integrity & polish

- **FEAT-222** — GPG-signed commits in each store (tamper-evidence on sync).
- **FEAT-223** — first-class macOS `.dylib` build rule for `libsecstore`.
- Freeze the CLI contract, bump to 1.0, run a full traceability audit
  (`skills/audit.md`).

## Process note

`skills/milestones.md`, `skills/features.md`, `skills/bugs.md` and
`skills/audit.md` are referenced by `CLAUDE.md` but are **not present** in
the tree (only `skills/secret-user.md` and the new `skills/testing.md`
exist). Restoring these process skills is itself a candidate task; until
then this ROADMAP and the `issues/feature|bug/*` files are the plan of
record.
