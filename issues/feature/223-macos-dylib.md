---
id: FEAT-223
type: feature
priority: low
status: open
milestone: 1.0.0
---

# First-class macOS `.dylib` build rule for `libsecstore`

## Description

**As a** developer embedding `libsecstore` on macOS
**I want** the build to emit a proper `libsecstore.dylib` with the right
install-name and versioning
**So that** the shared library links and loads correctly on macOS, the
same way `libsecstore.so` does on Linux.

`make` currently emits `libsecstore.so` (ELF) and `libsecstore.a`. On
macOS the shared object needs the Mach-O `.dylib` extension, an
`-install_name`, and `-compatibility_version`/`-current_version` flags —
a distinct link rule, not just a renamed `.so`.

## Implementation

1. Detect Darwin in `configure`/`Makefile.in` and select a `.dylib` link
   rule (`-dynamiclib -install_name @rpath/libsecstore.dylib
   -compatibility_version <maj> -current_version <ver>`).
2. Stow/install the `.dylib` under `libdir` and reference it from the
   macOS pkg packaging (`packaging/macos/`).
3. Verify `bin/secret` links against the `.dylib` from the source tree
   with no `DYLD_LIBRARY_PATH`, mirroring the Linux behaviour.

## Acceptance Criteria

1. On macOS, `make` produces `libsecstore.dylib` with a correct
   install-name and version fields.
2. `bin/secret` runs from the source tree on macOS without
   `DYLD_LIBRARY_PATH`.
3. The macOS pkg ships the `.dylib`.
4. The Linux `.so` build is unchanged.
