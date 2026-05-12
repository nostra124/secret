---
id: FEAT-203
type: bug
priority: medium
status: in-progress
---

# Quote variable expansions throughout `bin/secret` to support paths with spaces

## Description

**As a** user whose `$HOME`, `$XDG_CONFIG_HOME`, or store name contains a space
**I want** `secret` to handle it correctly
**So that** the tool is robust on non-standard but valid POSIX paths.

Multiple locations in `bin/secret` use unquoted variable expansions where the path
could contain whitespace. This causes word-splitting, leading to `no such file`
errors or silent incorrect behaviour.

## Inventory (non-exhaustive)

| Line | Unquoted expansion | Risk |
|---|---|---|
| 71 | `eval SELF_CONFIG=/etc/${SELF}` then `mkdir -p ${SELF_CONFIG}` | if SELF contains spaces |
| 75 | `eval SELF_CONFIG=${XDG_CONFIG_HOME}/${SELF}` | if XDG_CONFIG_HOME contains spaces |
| 177 | `ls -1 ${SELF_CONFIG}` | if SELF_CONFIG contains spaces |
| 195 | `if [ -d ${SELF_CONFIG}/${STORE} ]` | if SELF_CONFIG or STORE contains spaces |
| 262 | `find * -type f` (inside pushd) | if param names contain spaces |
| 315 | `for STORE in $(command:stores)` | if a store directory name contains spaces |
| 316-318 | `for PARAM in $(command:params $STORE)` | if store or param contains spaces |

The `eval` pattern on lines 71 and 75 is additionally unsafe: if `$XDG_CONFIG_HOME`
or `$SELF` contains shell metacharacters, the `eval` executes them. Prefer direct
assignment without `eval`.

## Implementation

1. Audit every variable expansion in `bin/secret` with:
   ```bash
   shellcheck -S style bin/secret
   ```
   and fix all SC2086 (unquoted variable) and SC2046 (unquoted command substitution)
   warnings.
2. Replace `eval SELF_CONFIG=...` with direct assignment:
   ```bash
   SELF_CONFIG="/etc/${SELF}"
   ```
3. Where `for X in $(command:...)` iterates over newline-separated output, use
   `while IFS= read -r X; do ... done < <(command:...)` to handle names with spaces,
   or document that store and parameter names must not contain spaces and add a guard
   that rejects such names early.
4. Add a unit test that creates a store under a sandboxed `SELF_CONFIG` path containing
   a space and verifies `secret stores`, `secret exists`, and `secret params` work.

## Acceptance Criteria

1. `shellcheck -S warning bin/secret` reports zero SC2086/SC2046 warnings.
2. `secret stores` lists a store whose directory name contains a space (unit test,
   sandboxed via `$XDG_SECRET_STORES`).
3. No `eval` with user-controlled input remains in `bin/secret`.
4. Existing 40 unit tests still pass.
