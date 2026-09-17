# DevilTouch integration branch

This is a modified DevilutionX 1.5.5 source tree for
[DevilTouch](https://github.com/chrissotraidis/deviltouch), based on upstream
`7223eeac9e8274fbf665b4de86fda26d3b22c52f` without an engine upgrade.
The `deviltouch/ios-1.5.5` branch preserves the existing native Files import,
app identity and direct-touch integration. The app supplies the importer and
asset catalog through its existing relative paths. Build through the app scripts.

The engine implementation is the work of the DevilutionX/Devilution authors.
Their history, copyright notices and Sustainable Use License remain intact.
This branch is a modified version, not an official upstream release. Component
licenses still apply separately. No Diablo or Hellfire data is included.

Compare changes with `git diff 7223eeac9e8274fbf665b4de86fda26d3b22c52f..HEAD`.
New fixes belong in normal source commits on this branch. Select an immutable
commit in the app; do not silently move other consumers or upgrade the base.
Upstream contributions must be independently scoped and reviewed; app support
starts in DevilTouch, and no upstream submission is implied by this fork.
