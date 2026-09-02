# Local build and install (macOS)

How this fork is built and installed on the development machine. Kept here so
the procedure does not have to be rediscovered each time.

## Build

Run from the repository root:

```
SIOYEK_BUILD_INCREMENTAL_P=y SIOYEK_NIGHT_P=y MAKE_PARALLEL=8 ./build_mac.sh
```

- `SIOYEK_BUILD_INCREMENTAL_P=y` skips the release-only steps (bundling Qt,
  clean rebuilds) so repeated builds are fast. Object files land in the
  repository root, which is why it is full of `*.o` and `moc_*` files.
- `SIOYEK_NIGHT_P=y` enables this fork's additions.
- Prerequisites come from Homebrew: `qt` (6.9 at the time of writing),
  `freeglut`, `mesa`, `harfbuzz`, `hiredis`.
- A full build takes several minutes; an incremental one after touching
  `ui.h` still recompiles most of `pdf_viewer/` because that header is
  included almost everywhere.
- The result is `build/sioyek.app`. Bundle resources (`prefs.config`,
  `keys.config`, shaders, tutorial) live in `Contents/Resources`.

### mupdf submodule gotcha

mupdf 1.26+ has nested submodules (brotli, zint, zxing-cpp). After bumping
the mupdf pointer, run:

```
git -C mupdf submodule update --init --recursive
```

or the mupdf build fails on a missing `brotli/encode.h`.

## Install

The installed copy is `/Applications/sioyek.app`. Replacing it:

```
trash -- /Applications/sioyek.app          # keep the old bundle recoverable
cp -R build/sioyek.app /Applications/
codesign --force --sign - --deep /Applications/sioyek.app
```

- The bundle is owned by the current user, so `codesign` does not need
  `sudo`.
- The ad-hoc signature is required or macOS refuses to launch the freshly
  copied binary.
- Check `/Applications/sioyek.app/Contents/MacOS/` for `*.db` files before
  trashing the bundle. Older layouts stored the databases there; if any
  exist, copy them aside first and restore them after the copy. Currently
  the databases live in the standard data directory, not in the bundle.
- A running sioyek keeps executing the old binary. Quit and relaunch it to
  pick up the new build.

The same steps exist as the `build-sioyek` shell function in the
developer's dotfiles, which also handles the database backup.

## Sanity checks after installing

```
codesign -dv /Applications/sioyek.app        # Signature=adhoc
grep -c file_selector_extensions /Applications/sioyek.app/Contents/Resources/prefs.config
```

## User configuration

The personal `prefs_user.config` and `keys_user.config` are hardlinked into
`~/.config/sioyek/` from a dotfiles repository, so edits to the copies inside
`pdf_viewer/` in this repository do not affect the installed configuration.
