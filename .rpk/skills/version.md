---
name: version
description: |
  Update the package version of `secret`. Trigger when a sprint/
  milestone is closing, a release tag is about to be cut, or a
  contributor asks "how do I bump the version" / "where is the
  version stored". Covers the four files that must move in lockstep
  (`VERSION`, `.rpk/version`, `.rpk/versions`, `git tag`) and the
  semver decision rules from CLAUDE.md §8.
---

# `version` skill — release the next `secret`

## 1. Where the version lives

| File | Role | Updated by |
|---|---|---|
| `VERSION` | **Canonical** source of truth, project root. Read first by `bin/secret`, `configure`, and `make version`. | Release author or `make package`. |
| `.rpk/version` | FEAT-194 legacy mirror, kept in sync with `VERSION` for older tooling. | Release author or `make package`. |
| `.rpk/versions` | TSV ledger: `<version>\t<git-sha>` per release, append-only (rpk's BUG-001 lesson — no orphan SHAs). | Release author or `make package`. |
| `git tag v<x.y.z>` | Annotated tag pointing at the release commit. | Release author or `make package`. |
| `share/secret/version` (installed only) | Output of `make install`; mirrors `VERSION` for the binary's fallback chain. | `make install`. |

`bin/secret`'s resolution order (`bin/secret:15-21`):

```
VERSION  →  .rpk/version  →  share/secret/version  →  hardcoded '0.8'
```

The `version` subcommand (`bin/secret version`), `configure`, and
`make version` all use the same order.

## 2. When to bump — semver rules (CLAUDE.md §8)

| Change kind | Bump |
|---|---|
| Bug fix, no observable behaviour change | **patch** (`x.y.Z`) |
| New subcommand, new option, output format addition that does not break existing parsers | **minor** (`x.Y.0`) |
| Anything that breaks the bats contract in `tests/unit/secret.bats` (existing assertion would fail against the new binary) | **major** (`X.0.0`) |
| Removed subcommand, removed option, exit code change for a documented path | **major** |
| User-visible error message string change | **minor** if existing bats patterns still match; **major** if they don't |

Reminder: bugs come before features at the same priority level
(CLAUDE.md §3), and **a major-version bump triggers a roadmap
review** — flag breaking changes in the corresponding
`issues/ROADMAP-<x.y.z>.md`.

## 3. Release flow (manual)

Use this when you want to control the commit message and inspect the
files before tagging.

```sh
# 1. Pick the next version per §2.
NEW=0.9.1

# 2. Verify the tree is clean and the bats contract passes against
#    the to-be-released code.
git status            # must be clean
bats tests/unit/secret.bats

# 3. Update both version files atomically.
echo "$NEW" > VERSION
echo "$NEW" > .rpk/version

# 4. Append a ledger entry. The SHA points at the parent commit
#    (the last commit before the version bump). See rpk's BUG-001
#    for the rationale (no orphan SHAs).
printf '%s\t%s\n' "$NEW" "$(git rev-parse HEAD)" >> .rpk/versions

# 5. Commit and tag.
git add VERSION .rpk/version .rpk/versions
git commit -m "Release $NEW"
git tag -a "v$NEW" -m "Release $NEW"

# 6. Sanity check.
./bin/secret version    # must print $NEW
make version            # must print $NEW
./configure --help      # configure must print "configured secret $NEW"
```

## 4. Release flow (automated, `make package`)

`make package VERSION=x.y.z` runs steps 3–5 in one go:

```sh
make package VERSION=0.9.1
```

The target validates the input is semver, refuses anything else, then
writes `VERSION`, `.rpk/version`, appends to `.rpk/versions`, commits,
and tags `v0.9.1`. It does **not** run the bats suite — run that
manually beforehand.

## 5. Common mistakes (caught in review)

- **Updating only one of `VERSION` / `.rpk/version`.** They must
  agree; `bin/secret` reads `VERSION` first but `.rpk/version` is
  still read by older tooling and the FEAT-194 installed fallback.
- **Skipping the ledger.** A release without a `.rpk/versions` entry
  is the orphan-SHA bug rpk explicitly warns about. Always append.
- **Tagging without bumping `VERSION`.** The tag and the in-tree
  version drift, breaking `bin/secret version` for anyone who checks
  out the tag.
- **Bumping for a no-op change.** If the bats contract and behaviour
  are identical, do not bump — that just pollutes the ledger.
- **Major bump without a roadmap entry.** Major bumps require a
  corresponding `issues/ROADMAP-<x.y.z>.md` file. See `0.9.0` for an
  example of the frontmatter (`version`, `from`, `status`,
  `semver-justification`).

## 6. Verifying a release

After a release, run all four resolvers and confirm they agree:

```sh
cat VERSION
cat .rpk/version
./bin/secret version
make version
```

All four must print the same string. If any disagree, the release is
incomplete — fix the mismatch and amend the release commit (with
care; do not amend a tagged commit that has been pushed).
