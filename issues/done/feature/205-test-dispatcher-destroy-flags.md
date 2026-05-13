---
id: FEAT-205
type: feature
priority: medium
status: resolved
resolved-in: 0.11.0
---

# Add tests for the `store/param` dispatcher shortcut, `destroy password-store`, and flag parsing

## Description

**As a** maintainer
**I want** the three currently untested dispatch/flag behaviours covered by unit tests
**So that** they cannot regress silently between releases.

### 1. `store/param` dispatcher shortcut (`bin/secret:820-823`)

When `$1` looks like `store/param` (contains `/`), the script calls `command:has $1`
and, if the file exists, routes to `command:get $1` before the normal subcommand
dispatch. This is a documented but untested convenience shorthand. Tests needed:

- `secret mystore/version` → behaves like `secret get mystore/version` when the
  `.gpg` file is present (exit 0, output delegated to `get`).
- `secret mystore/missing` with `.gpg` absent → falls through to the "unknown
  command" fatal (exit non-zero).

### 2. `destroy password-store`

The test file comment at line 248 notes this is skipped "to avoid `rm -rf $HOME`",
but `setup()` already sandboxes `$HOME`. The branch is safe to test:

- `destroy password-store` with `$HOME/.password-store` present removes the
  directory and exits 0.
- `destroy password-store` with `$HOME/.password-store` absent exits 0 (no-op).

### 3. Flag parsing (`-q`, `-d`, combined)

After FEAT-202 fixes the `getopts` loop:

- `secret -q stores` exits 0; `$output` is empty (quiet mode suppresses info lines)
  or lists stores (no info lines visible from the test runner since the binary uses
  `>&2` for info).
- `secret -d version` exits 0 and includes `debug` in stderr output.

## Implementation

Add three sections to `tests/unit/secret.bats`:

```bash
# --- Dispatcher shortcut ---
@test "store/param shortcut resolves to get when .gpg file exists"
@test "store/param shortcut falls through to unknown-command when .gpg absent"

# --- destroy password-store ---
@test "destroy password-store removes ~/.password-store"
@test "destroy password-store is a no-op when ~/.password-store absent"

# --- Flag parsing (depends on FEAT-202) ---
@test "secret -q suppresses info output"
@test "secret -d version includes debug output on stderr"
```

Flag-parsing tests are gated on FEAT-202. Mark them with a `skip` and
`# TODO FEAT-202` comment until the getopts fix lands.

## Acceptance Criteria

1. All 6 new tests pass.
2. `destroy password-store` tests use the sandboxed `$HOME` from `setup()`.
3. Dispatcher shortcut tests seed a real `.gpg` stub file in the sandbox.
4. Flag-parsing tests capture stderr separately (bats ≥1.5 `--separate-stderr` or
   redirect `2>&1`).
5. Existing 40 tests still pass.
