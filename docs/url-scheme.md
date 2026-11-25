# The `sioyek://` URL scheme (macOS)

With `SIOYEK_NIGHT_P=y`, the build registers the URL scheme `sioyek` in the
bundle's `Info.plist` (`build_mac.sh` adds `CFBundleURLTypes`), and sioyek
handles such links. A link clicked in a browser, or passed to `open`, opens a
PDF at a page, whether sioyek is running or not. It behaves like
`sioyek --page N <file>`: same window choice, same configuration.

## Grammar

```
sioyek://open?path=<percent-encoded path>[&page=N]
```

- `path`: required, exactly once. It is percent-decoded (`%20` is a space).
  `+` is **not** a space: encode with `urllib.parse.quote`, not `quote_plus`
  or `URLSearchParams`. A leading `~/` expands to the home directory.
- `page`: optional, 1-based like `--page` (`page=1` is the first page). A
  value that is not a positive integer is ignored with a warning, and the
  document opens at its remembered position.
- Every other query parameter is ignored.

Making a link from a shell:

```
python3 -c 'import sys, urllib.parse as u; print("sioyek://open?path=" + u.quote(sys.argv[1], safe="/~") + "&page=3")' '~/papers/some paper.pdf'
```

## Safety rules

Any web page can fire a registered scheme, so the handler only ever opens an
existing PDF. It rejects, opening nothing and logging a `qWarning` with the
reason:

- any URL other than `sioyek://open?...` (for example `sioyek://execute?...`);
- a missing, empty or repeated `path`;
- a path that is not absolute after expanding `~/`;
- a path with a `..` component;
- a path not under the home directory (the cleaned path as written, before
  symlinks are resolved);
- a path that does not end in `.pdf` (case-insensitive);
- a file that does not exist or is not a regular file.

A URL never runs sioyek commands, macros, config or scripts: the handler
passes only `--page N -- <path>` to the same code that handles command-line
arguments.

The warnings go to the unified log:
`log show --last 5m --predicate 'process == "sioyek"' | grep ignoring`
(each line shows up twice there; the handler runs once). Launching with
`open --env QT_FORCE_STDERR_LOGGING=1 --stderr <file> -a sioyek` sends them
to a file instead.

## Code

- `pdf_viewer/OpenWithApplication.cpp`: a `sioyek:` URL arrives as a
  `QFileOpenEvent` with an empty `file()`; it is emitted as `url_ready`, or
  queued until something connects (a cold launch can deliver it early).
- `pdf_viewer/main.cpp`: `sioyek_url_to_args` validates the URL and builds
  the arguments; `handle_sioyek_url` passes them to `handle_args`.

## Testing

After installing (see [local-build-and-install.md](local-build-and-install.md)),
check that LaunchServices maps the scheme to the bundle:

```
osascript -l JavaScript -e 'ObjC.import("AppKit"); $.NSWorkspace.sharedWorkspace.URLForApplicationToOpenURL($.NSURL.URLWithString("sioyek://open")).path.js'
```

If it prints nothing or another copy of sioyek, register the bundle:
`/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/sioyek.app`.

Then, with a PDF whose path has a space in it:

1. Warm: with sioyek running on another page, `open 'sioyek://open?path=~/...&page=3'`
   shows page 3.
2. Cold: quit sioyek, run the same command.
3. Browser: put the link in a local HTML file and click it in Chrome and in
   Safari, warm and cold. Each browser asks once whether to open sioyek.
4. Rejections: `path=/etc/hosts`, `path=~/../../etc/hosts`, a missing `.pdf`,
   a non-PDF file under the home directory, `sioyek://execute?command=goto_end`
   and `sioyek://open` must open nothing, keep the current page, and log a
   warning.

To make the warm and cold tests meaningful, move the document to a different
page first: sioyek remembers the last position, so a link to the page it is
already on proves nothing.
