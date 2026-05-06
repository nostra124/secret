# `secret`

> GPG-encrypted secrets store, account-scoped, with pass(1)-style flat-file layout

## Install

    git clone https://github.com/nostra124/secret
    cd secret
    ./install --prefix=$HOME/.local

Or in two steps:

    ./configure --prefix=$HOME/.local
    make install

## Quick start

    secret help
    secret version

## Layout

| Path | Purpose |
|---|---|
| `bin/secret` | the entry point |
| `libexec/secret/` | sub-commands (where applicable) |
| `docs/secret.md` | CLI contract reference |
| `share/man/man1/secret.1` | man page |
| `share/doc/secret/standards/` | vendored references (educational) |
| `skills/secret-user/` | agent skill |
| `tests/unit/secret.bats` | unit tests |
| `tests/sit/` | system integration (when present) |
| `.cpk/` | container packaging overlay |
| `.rpk/` | rpk metadata (version, versions ledger, depends/) |

## Documentation

- `man secret`
- `docs/secret.md` — CLI contract reference
- `share/doc/secret/standards/README.md` — vendored standards
- `CLAUDE.md` — agent guide
- `skills/secret-user/SKILL.md` — agent skill

## Conventions

This package follows the rpk per-script repo convention:

- Per-script repo: this repo contains only `secret`'s artefacts.
- No shared library: helper boilerplate is duplicated, not factored out (see `CLAUDE.md` §4–5).
- Stow-based install via `make install`.
- Versioning: semver, with `.rpk/version` as the source of truth and `.rpk/versions` as the per-release SHA ledger.

## License

GPL-3 (per the cross-cutting policy in the parent `scripts` collection).
