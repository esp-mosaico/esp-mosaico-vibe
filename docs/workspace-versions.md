# Workspace versions and updates

[简体中文](workspace-versions_CN.md) | [Documentation index](README.md)

A workspace release is an official main-repository tag plus the submodule commit
IDs (Git gitlinks) stored in that commit. Submodules need not have tags with the
same name. `python mosaico.py --version` reports the product CLI version, not the
workspace release. There is no separate version file to keep in sync.

Stable release tags use `vN.N.N`, such as `v26.0.1`. Select the highest numeric
version, not the most recent tag date or the default branch tip. Prerelease tags
such as `v26.0.2-rc1` require an explicit user choice. Published release tags must
not be moved; fixes belong in a new tag whose commit pins the complete dependency
set. Maintainers should validate those pins before publishing a release.

## Check before creating a project

Before `project init`, `game create` or `game new`, run these commands from the
workspace root (shell examples use POSIX syntax; use `python3` if needed):

```sh
mosaico_release_source=https://github.com/esp-mosaico/esp-mosaico-vibe.git
git ls-remote --tags --sort='-version:refname' "$mosaico_release_source" 'v*'
git rev-parse HEAD
git tag --points-at HEAD
git ls-tree HEAD submodule/
git submodule status --recursive
git diff --cached --submodule=short
git status --short --untracked-files=all --ignore-submodules=none
git submodule foreach --recursive 'git status --short --untracked-files=all --ignore-submodules=none'
```

Use the live official tag list, even if `origin` is a fork. Only names matching
`refs/tags/vN.N.N` are stable candidates. For an annotated tag, its `^{}` entry
gives the commit to compare with `HEAD`; the other entry is a tag object ID.
For a lightweight tag, the single entry gives the commit. Local tags may be
missing, stale or private; `git tag --points-at HEAD` is only a local hint.
Do not use a nearest-ancestor `git describe` result as an exact release match.

`git submodule status` compares against the index: a leading space means a match,
`+` means another commit, `-` means uninitialized and `U` means a conflict. Check
the staged diff as well so a staged gitlink change cannot hide a mismatch with
`HEAD`. The status commands also reveal file changes within initialized modules.
Uninitialized modules have known pins but unverified contents; initialize the
ones needed by the task, then check their nested modules too. Leave unused ones
uninitialized and record that limitation.

| Result | Agent action before project creation |
| --- | --- |
| `HEAD` equals the latest stable tag's commit and required dependencies match | Report the tag, commit and dependency status; proceed. |
| `HEAD` equals an older official stable tag | Show current/latest tags and recommend updating. |
| `HEAD` is not any official stable tag, including a branch ahead of the latest tag | Show the commit and recommend switching to the latest stable tag. |
| Submodule commit mismatch, staged gitlink change, conflict or dirty files | Report affected paths and propose alignment while preserving local work. |
| Official lookup fails or has no stable tags | Report that the latest release is unknown; do not claim local tags are current. |

Resolve the version choice with the user before creating the project. An explicit
instruction to use a particular tag or development commit already settles that
choice for the task; record the exception and avoid repeated prompts. Merely
being on a feature branch does not settle it. A version check does not authorize
switching an existing workspace. If local work prevents safe alignment, use a
separate checkout or agree how to preserve it; never automatically stash, reset,
clean, force-switch or replace tags.

## Update to a selected tag

After the user has selected a version, inspect and preserve local changes in the
main repository and initialized submodules using the checks above. Keep existing
branches and local commits reachable; a separate checkout is preferable when the
workspace contains ongoing work. These commands require a clean checkout and stop
at the first failure. Replace the example tag with the selected official tag:

```sh
mosaico_release_source=https://github.com/esp-mosaico/esp-mosaico-vibe.git
mosaico_release_tag=v26.0.1
git fetch --no-tags --no-recurse-submodules "$mosaico_release_source" \
  "refs/tags/$mosaico_release_tag:refs/tags/$mosaico_release_tag" &&
git switch --detach "refs/tags/$mosaico_release_tag" &&
git submodule sync --recursive &&
git submodule update --checkout --recursive &&
git submodule update --init --checkout --recursive submodule/esp-mosaico-utils
```

The explicit fetch fails if a different local tag already uses that name; do not
force it. Diagnose the conflicting tag or use a separate checkout. `--checkout`
ensures submodules use the recorded commits even if local configuration normally
requests merge or rebase. The first update aligns already initialized modules;
the second initializes utils, which is needed for project creation. Add BSP for
firmware work, and BSP plus the engine for games:

```sh
git submodule update --init --checkout --recursive submodule/esp-mosaico-bsp
# For games, also initialize the engine.
git submodule update --init --checkout --recursive submodule/raylib-lite-engine
```

Use only the paths needed by the task. Do not run `git pull` in submodules or
`git submodule update --remote`: they follow branches instead of the release pins.
If any step fails, report the partial state and resolve it before proceeding.

Verify the update with the following commands and repeat the status checks above:

```sh
git rev-parse HEAD
git rev-parse "refs/tags/$mosaico_release_tag^{commit}"
git ls-tree "refs/tags/$mosaico_release_tag" submodule/
git submodule status --recursive
```

The two main-repository SHAs must match the selected official tag's commit. All
initialized submodules must match its recorded pins recursively, with no staged
gitlink changes, conflicts or dirty dependency files; unused modules may remain
uninitialized. Re-read that release's `AGENTS.md`, documentation and ESP-IDF pin
before creating or building a project. A source update does not update a device.

For a fresh checkout, select an official tag with the same discovery procedure,
then clone it into a new directory and initialize only the needed dependencies:

```sh
git clone --branch "$mosaico_release_tag" --single-branch \
  "$mosaico_release_source" esp-mosaico-release &&
cd esp-mosaico-release &&
git submodule update --init --checkout --recursive submodule/esp-mosaico-utils
```

Repeat verification there. To commit application work, create a new branch from
the verified release with `git switch -c my-project`; creating the branch alone
does not change its release commit. Keep the selected tag and submodule SHAs in
the task's version report.

Git command references: [remote tags](https://git-scm.com/docs/git-ls-remote),
[fetching tags](https://git-scm.com/docs/git-fetch),
[submodule status and checkout](https://git-scm.com/docs/git-submodule).
