---
id: FEAT-199
type: feature
priority: low
status: resolved
resolved-in: 0.9.0
resolved-by-audit: audits/audit-2026-05-13.md
---

# `secret-user` agent skill

## Description

**As a** user delegating secret-management tasks to an AI
agent
**I want** a packaged skill that teaches the agent the
secret model — multi-store hierarchical KV with GPG
encryption at rest, git-backed sync between accounts,
the special `pass` integration
**So that** an agent can put / get / list / sync secrets
correctly without the model re-explained.

## Implementation

Layout:

    skills/
    └── secret-user/
        ├── SKILL.md
        └── opencode.md

`SKILL.md` frontmatter:

    ---
    name: secret-user
    description: Operate the `secret` per-account
      encrypted secret store — put / get / list / delete
      secrets, sync between accounts via git, manage
      GPG-key access, bootstrap the `pass` password store
      via `pass-init`. Trigger when the user wants to
      store / retrieve a credential, share a secret with
      another account, or set up `pass`.
    ---

Body sections per FEAT-192:

1. Design principles.
2. **Model**: per-account secret store under
   `$XDG_CONFIG_HOME/secret/<store>/`. Each parameter
   identified by a hierarchical id (`<store>/<param>`,
   colon-separated within a param). GPG-encrypted at
   rest; git-backed sync between accounts via SSH
   endpoints from `account`. Special `pass` store wraps
   `pass(1)`.
3. Workflow recipes:
   - init a new store
   - put / get / del a parameter
   - generate a random secret
   - render as QR code
   - push / pull / sync between accounts
   - add / remove GPG keys
   - `pass-init` to bootstrap password-store
4. Guardrails:
   - **Never log a secret value** — even in error paths
     (#1).
   - Verify GPG keys before adding to a store; an
     untrusted key gains read access to everything.
   - GPG-key rotation is a re-encrypt, not a swap; old
     blobs in git history remain readable to the old key.
   - Account credentials (the seed) for sibling tools
     (bitcoin / lightning) reference into secret; don't
     duplicate.
5. Where to read more: `man secret`, walkthrough,
   `docs/templates/CLAUDE.md.secret`.

Installation per the established pattern.

## Acceptance Criteria

1. `skills/secret-user/SKILL.md` and `opencode.md` exist.
2. `make install` + `make install-skills-user` work.
3. Guardrail "never log a secret value" called out as #1.
4. The GPG key-rotation gotcha is documented.
