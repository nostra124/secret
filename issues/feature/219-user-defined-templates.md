---
id: FEAT-219
type: feature
priority: medium
status: open
milestone: 0.16.0
---

# User-defined named templates

## Description

**As a** user with a recurring structured secret that isn't one of the
built-ins
**I want** to define my own named template and expand it with `secret new
<custom>`
**So that** I get the same one-param-per-field materialisation the
built-in `login` / `wifi` / `mfa` / `passkey` / `wallet` templates give,
for my own schemas.

The built-in templates are pure schemas expanded by `secret new`
(`lib/template.c`). This adds a way for users to register additional
schemas without recompiling, while keeping the built-ins authoritative
for the duck-typed actions (`qr` / `otp` / `clip`).

## Implementation

1. Define a user template location (e.g.
   `$XDG_CONFIG_HOME/secret/.templates/<name>`) with a simple
   field-per-line schema format.
2. Merge user templates into the `templates` listing and `new`
   expansion, with built-ins taking precedence on name collision.
3. Document that user templates are schema-only: the template-bound
   actions (`qr`, `otp`) remain duck-typed on field names, so a custom
   template that names its fields like a built-in inherits the action.
4. Reject `..` / path-traversal in template names (per the FEAT-207
   policy).

## Acceptance Criteria

1. A user can create a custom template and `secret new <custom>`
   materialises one parameter per field.
2. `secret templates` lists user templates alongside built-ins.
3. Built-in templates cannot be shadowed by a user template of the same
   name.
4. Template names are validated against path traversal.
