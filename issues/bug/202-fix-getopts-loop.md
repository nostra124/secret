---
id: FEAT-202
type: bug
priority: medium
status: open
---

# Fix broken `getopts` loop in `bin/secret`

## Description

**As a** user invoking `secret` with flags like `-d`, `-q`, or `-f`
**I want** flags to be parsed reliably before the subcommand
**So that** debug, quiet, and force modes actually take effect.

The `getopts` loop at `bin/secret:48-59` has two defects:

1. **Manual `shift` inside the loop** — `getopts` tracks its own position via `OPTIND`.
   Shifting inside the loop desynchronises `OPTIND` from `$@`, so the second flag in
   `-d -q stores` is never seen.
2. **`-c:` declared but never handled** — the optstring includes `c:` (meaning `-c`
   takes an argument), but no `if` branch processes it. Any caller passing `-c value`
   will silently discard `value` and may misparse the next argument as a flag.

## Root cause

```bash
# bin/secret:48-59
while getopts "c:dqf" flag; do
    if [ x"$1" = x'-q' ]; then SELF_QUIET=1; fi
    if [ x"$1" = x'-d' ]; then SELF_DEBUG=1; fi
    if [ x"$1" = x'-f' ]; then SELF_FORCE="-f"; fi
    shift 1   # <-- wrong: desynchronises OPTIND
done
```

The correct pattern uses `$flag` (the variable set by `getopts`) and lets `getopts`
advance via `OPTIND` automatically:

```bash
while getopts "dqf" flag; do
    case "$flag" in
        q) SELF_QUIET=1 ;;
        d) SELF_DEBUG=1 ;;
        f) SELF_FORCE="-f" ;;
    esac
done
shift $((OPTIND - 1))
```

## Implementation

1. Rewrite the `getopts` loop to use `$flag` in a `case` statement.
2. Remove `-c:` from the optstring unless there is a concrete use for it (no call site
   in the codebase passes `-c`; if unused, remove it).
3. Add `shift $((OPTIND - 1))` after the loop to consume all parsed flags before the
   subcommand dispatcher reads `$1`.
4. Add unit tests for `secret -q stores`, `secret -d version`, and the combination
   `secret -q -d version` to pin the parsing behaviour.

## Acceptance Criteria

1. `secret -q stores` does not print info-level lines; exits 0.
2. `secret -d version` sets debug mode (output contains debug lines); exits 0.
3. `secret -q -d version` combines both flags correctly.
4. `secret -f push somestore someaccount` passes `-f` through to the git push (SIT
   scope; document as out of scope for unit tests).
5. `shellcheck -S warning bin/secret` does not warn about the `getopts` construct.
