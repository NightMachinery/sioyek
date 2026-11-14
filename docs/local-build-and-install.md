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
macos-local-signing codesign "Sioyek Local Signing" --force --deep /Applications/sioyek.app
```

- Signing uses a stable self-signed certificate from
  [macos-local-signing](https://github.com/NightMachinery/macos-local-signing).
  An ad hoc signature (`codesign --sign -`) identifies the app by the hash of
  that one build, so every rebuild loses macOS privacy grants such as folder
  access. The certificate signature identifies it by bundle id and
  certificate, which survive rebuilds. Create the identity once per Mac, at
  its screen (it shows one password dialog):
  `macos-local-signing ensure "Sioyek Local Signing"`. Without it, fall back
  to `codesign --force --deep --sign - /Applications/sioyek.app`.
- The bundle is owned by the current user, so neither needs `sudo`.
- Some signature is required, or macOS refuses to launch the freshly copied
  binary.
- Add nothing to the bundle after signing: an added file breaks the seal
  (`codesign --verify --deep --strict` reports "a sealed resource is missing
  or invalid"). Older layouts kept `*.db` files in `Contents/MacOS/`; sioyek
  on macOS now reads them only from `~/Library/Application Support/sioyek/`.
- A running sioyek keeps executing the old binary. Quit and relaunch it to
  pick up the new build.

The same steps exist as the `build-sioyek` shell function in the
developer's dotfiles.

## Sanity checks after installing

```
codesign -dvv /Applications/sioyek.app 2>&1 | grep Authority  # Authority=Sioyek Local Signing
codesign --verify --deep --strict /Applications/sioyek.app   # no output when the seal is intact
grep -c file_selector_extensions /Applications/sioyek.app/Contents/Resources/prefs.config
```

## User configuration

The personal `prefs_user.config` and `keys_user.config` are hardlinked into
`~/.config/sioyek/` from a dotfiles repository, so edits to the copies inside
`pdf_viewer/` in this repository do not affect the installed configuration.

## Installing on a second Mac by copying the build

@Opus5.5, 2026-11-14

A build from `SIOYEK_BUILD_INCREMENTAL_P=y` does not bundle Qt: the binary
links the Homebrew libraries under `/opt/homebrew/opt/qt/lib` and
`/opt/homebrew/opt/hiredis/lib` (`otool -L` lists them). To copy it to
another Mac instead of building there:

- Install the full `qt` formula and `hiredis` on the target. The split Qt
  formulae (`qtbase` and so on) are not enough, because only `qt` creates the
  `/opt/homebrew/opt/qt` path the binary uses. A newer Qt 6 on the target is
  fine: Qt 6 runs programs built against an older 6.x.
- hiredis 1.5 renamed its library to `libhiredis.1.dylib` (1.2 shipped
  `libhiredis.1.1.0.dylib`). If the target's file name differs from the one
  `otool -L` shows, repoint the binary before signing:
  `install_name_tool -change <old path> <new path> sioyek.app/Contents/MacOS/sioyek`.
- Copy the bundle with `tar` over ssh, move it into `/Applications`, then sign
  it on the target with that Mac's own `Sioyek Local Signing` identity and
  check it with `codesign --verify --deep --strict`.
- Test it with a headless launch that uses its own home directory
  (`HOME=<tmp> CFFIXED_USER_HOME=<tmp> QT_QPA_PLATFORM=offscreen ... --instance-name <name>`),
  so it does not talk to a sioyek that is already running.

Running `macdeployqt` to make the bundle self-contained was tried first and
left it incomplete (QtDBus, QtConcurrent, QtPdf, the virtual keyboard
frameworks and `libbrotlicommon` were not copied), so installing the Homebrew
packages on the target is the simpler route.
