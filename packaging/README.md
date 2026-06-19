# Packaging

Native packages for `secret` (the C reimplementation: `libsecstore` +
the `secret` CLI). Every recipe stages files with the Makefile's
`install-dist` target and then wraps the staged tree with the platform's
packager. Artifacts land in `dist/` at the repo root (git-ignored).

| Format | Platform | Tooling | Recipe |
|---|---|---|---|
| `.deb` | Debian / Ubuntu | `dpkg-deb` | `packaging/deb/` |
| `.rpm` | Fedora / RHEL / openSUSE | `rpmbuild` | `packaging/rpm/` |
| `.apk` | Alpine | `abuild` | `packaging/apk/` |
| `.pkg` | macOS | `pkgbuild` / `productbuild` | `packaging/macos/` |

## Build

From the repo root:

```sh
make package-deb       # one format (build on the matching platform)
make package-rpm
make package-apk
make package-macos
make packages          # everything this host has tooling for
```

Each recipe can also be run directly, e.g. `packaging/deb/build.sh`.
A C compiler is required; the binary is built first via `make all`.

## What gets installed

`make install-dist DESTDIR=… prefix=…` lays down:

| Path (relative to `prefix`, except `/etc`) | Contents |
|---|---|
| `bin/secret` | the CLI |
| `lib/libsecstore.so` | shared library |
| `lib/libsecstore.a` | static library (`-devel`/`-dev` subpackage) |
| `include/secstore.h` | public header (`-devel`/`-dev` subpackage) |
| `share/man/man1/secret.1` | man page |
| `/etc/bash_completion.d/secret` | bash completion |
| `share/secret/version` | runtime version string |
| `share/doc/secret/` | `README.md`, `secret.md` |

deb/rpm/apk install under `prefix=/usr`; the macOS pkg uses
`/usr/local`.

## Runtime dependencies

`git` and `gnupg` are hard dependencies (the tool is useless without
them). `pass`, `qrencode` and the SSH client are recommended/optional —
needed only for the `password-store` integration, `qr`, and
`pull`/`push`/`sync` respectively. Package-manager names differ:

| Tool | deb | rpm | apk |
|---|---|---|---|
| GnuPG | `gnupg` | `gnupg2` | `gnupg` |
| pass | `pass` | `pass` | `pass` |
| qrencode | `qrencode` | `qrencode` | `libqrencode-tools` |
| SSH client | `openssh-client` | `openssh-clients` | `openssh-client` |

## Notes

- **deb** uses `dpkg-deb --root-owner-group`, so no `fakeroot` is
  needed.
- **rpm** and **apk** build from a clean `git archive` of `HEAD`; commit
  before packaging a release.
- **apk** needs a signing key once: `abuild-keygen -a -i`.
- **macOS**: `libsecstore.so` is shipped as produced by `cc -shared`.
  For a first-class `.dylib`, add a dylib build rule before packaging;
  the CLI itself statically embeds the library objects and needs no
  shared lib at runtime.
