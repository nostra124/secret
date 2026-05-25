---
name: logging
description: |
  Emit logs at the right level in `bin/secret` (and any sibling
  script in the rpk family). Trigger when adding a new code path
  that prints to stderr, deciding between warn / error / fatal,
  silencing or tracing output during a test run, or grep'ing for a
  failure mode in production. Covers the four supported levels, the
  env-var switches, and the colouring contract that downstream log
  parsers depend on.
---

# `logging` skill — four levels, one stream

## 1. The four levels

| Level | Function | Goes to | Shown by default? | Suppressed by | Colour |
|---|---|---|---|---|---|
| **debug** | `debug "msg"` | stderr | no | (off unless `$SELF_DEBUG`) | grey (`\033[90m`) |
| **info** | `info "msg"` | stderr | yes | `$SELF_QUIET` or `-q` | green (`\033[32m`) |
| **warn** | `warn "msg"` | stderr | yes | nothing | yellow bold (`\033[33;1m`) |
| **error** | `error "msg"` | stderr | yes | nothing | red (`\033[31m`) |

A fifth helper, `fatal "msg"`, is `error "msg"` followed by
`exit -1`. It is **not** a separate level — it is the conventional
shorthand for "log an error and abort". The current implementation
prints the prefix `fatal -` rather than `error -`; tests pin both
prefixes, so do not rename one without renaming the other.

All four definitions live at the top of `bin/secret` (currently
lines 25–48). They are duplicated across rpk packages by design
(see CLAUDE.md §4–5 — no shared lib).

## 2. When to use which

| You are about to log… | Use |
|---|---|
| A trace of internal state or an intermediate command being run (only useful with `-d`) | `debug` |
| A normal operational message ("pulling store X from account Y") | `info` |
| A condition the user should notice but the operation continued anyway ("account not reachable, skipping") | `warn` |
| The operation failed and the caller's request was not satisfied, but the script will keep going (e.g. one of many stores in a loop) | `error` |
| The operation failed and the script cannot continue (bad arguments, missing dependencies) | `fatal` |

Rule of thumb: **return code 0 with `warn` output is fine; return
code ≠ 0 with only `warn` output is a bug.** A non-zero exit
deserves at least one `error` (or `fatal`) line so the user knows
which path triggered the failure.

## 3. Environment controls

| Var / flag | Effect |
|---|---|
| `-d` or `SELF_DEBUG=1` | Enables `debug` output and runs the early `set -vx` for line-by-line tracing |
| `-q` or `SELF_QUIET=1` | Suppresses `info` output (warn / error / fatal still print) |
| `-f` or `SELF_FORCE=-f` | Unrelated to logging — passes through to git push/pull |

In tests, `setup()` exports `SELF_QUIET=1` so info chatter does not
pollute `$output` assertions. Tests that need to verify info
specifically should `unset SELF_QUIET` locally.

## 4. Output format

```
secret: <level> - <message>
```

The leading `secret:` is `$SELF` (the basename of `$0`). The level
tag is followed by ` - ` (space-dash-space) and then the message.
Colours apply only to the level tag and message portion, never to
`$SELF:` — this keeps grep-friendly prefixes intact.

Bats tests that assert on log output should match the tag with
`*"error -"*` rather than `*"error"*` to avoid matching the word
"error" inside an unrelated message body.

## 5. Adding a new level

Don't. Four levels cover every documented use case, and adding a
fifth (e.g. `notice`, `trace`) breaks the symmetry with sibling
packages. If you find yourself wanting a new level, either:

- Re-tier the message to one of the existing four, or
- Use `debug` for the new fine-grained channel and gate it on a
  more specific env var (e.g. `SECRET_TRACE_GIT=1`) inside the
  caller, not in `debug()` itself.

## 6. Testing log output

The unit suite already pins the `debug -` and `error -` prefixes
(`tests/unit/secret.bats`, "Logging contract" section). When
adding a new log call:

1. If the path is reachable from a unit test, add an assertion that
   pins the prefix (`*"warn -"*`, `*"info -"*`, etc.).
2. Use `run --separate-stderr` (bats ≥1.5; the test file declares
   `bats_require_minimum_version 1.5.0` at the top) when you need
   to assert against stderr specifically.
3. Tests that match against `$output` (combined) work for any
   level, but `$stderr` is preferred for log-level assertions.

## 7. Common mistakes

- **Calling `echo "...$@..."` directly** instead of going through
  `error`/`warn`/etc. — bypasses the level tag, breaks log parsing.
- **Using `warn` for something that is genuinely an error.** If the
  function then returns non-zero, the user sees a yellow message
  and an error exit code — confusing. Use `error` (or `fatal`).
- **Forgetting `>&2`** when writing custom log helpers. All four
  built-in helpers write to stderr; stdout is reserved for command
  output (`secret get` payload, `secret stores` listing, etc.).
- **Adding colour escape codes manually** in a message body. They
  will not be stripped on non-TTY output, leaking into pipes and
  log files.
