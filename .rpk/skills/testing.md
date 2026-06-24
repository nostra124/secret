# skill: testing — the unit / SIT / PIT layering and coverage

How `secret` is tested, and how to keep coverage honest.

## The three layers

| Layer | Where | Needs | Run with |
|---|---|---|---|
| **unit** | `tests/unit/*.bats` | nothing but the built binary | `make check` |
| **SIT** (system integration) | `tests/sit/*.bats` | real tools on the host (gpg, git, ssh/sshd, pass, secret-tool + gnome-keyring, oathtool, qrencode, a clipboard) | `make check-sit` |
| **PIT** (package integration) | `tests/pit/` | docker/podman + registry access | `make check-pit` |

- **unit** is the contract gate (CI runs it). Tests must be tool-free and
  deterministic — every command path that needs gpg/ssh/pass/etc. is
  validated only up to the point where it would shell out (argument
  validation, dispatch, store/param handling). SIT-only assertions hide
  behind `[ -n "$SECRET_SIT" ] || skip`.
- **SIT** drives the real integrations: gpg encrypt/decrypt, git history,
  SSH push/pull/sync (a localhost sshd loopback), the password-store via
  `pass`, the keyring provider via a headless `gnome-keyring-daemon`, the
  KeePass plugin via `pykeepass`, TOTP via `oathtool`, QR via `qrencode`,
  and the clipboard. Each test skips cleanly when its tool is absent.
- **PIT** builds each native package (`packaging/`) and installs it into a
  clean distro container (`debian`/`fedora`/`alpine`), then runs
  `tests/pit/smoke.sh` against the installed binary. Formats whose distro
  mirror can't be reached are skipped.

## Coverage

`make coverage` builds an instrumented binary (`-O0 --coverage
-DSECRET_FAULTS`), runs unit + SIT + fault layers, and prints gcov line
coverage per file and a total. `SECRET_COV_MIN=NN make coverage` fails
below `NN%`.

### Fault injection
Defensive branches (OOM, `fork`/`socket`/`bind`/`listen`/`pipe`
failures) are unreachable by normal inputs. `lib/fault.h` provides
`SECRET_FAULT("name")`, which is a no-op in production builds and an
`$SECRET_FAULT`-driven failpoint under `-DSECRET_FAULTS`. `tests/fault/`
arms each failpoint (`SECRET_FAULT=malloc secret version`, etc.). The
coverage build also `__gcov_dump()`s in forked children before `execvp`
so their lines are observable (a child that execs otherwise never flushes
gcov data).

### What is *not* reachable (and why)
Aim for "every reachable line"; the residual is documented, not hidden:
- **EINTR retry loops** and **partial-write loops** (`proc.c`, `http.c`) —
  need a signal/short-write injected mid-syscall.
- **`pbcopy` / `clip.exe`** clipboard backends — macOS / WSL only.
- **`resolve_exe_dir` argv0 fallback** / **identity `$USER` fallback** —
  `/proc/self/exe` and `getpwuid` always succeed on Linux.
- **`execvp` failure `_exit(127)`** — only when a spawned tool is missing.

When you add such a line, either cover it (a fault, a crafted PATH, a
container) or note it here.

## Adding tests
- New verb or branch → a unit test for its tool-free paths, plus a SIT
  test for the real behaviour if it shells out.
- New package file → update the rpm `%files` (rpm fails on unpackaged
  files) and `tests/pit/smoke.sh` if it should be asserted.
- Run `make coverage` and don't regress the total.
