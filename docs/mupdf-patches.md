# Local mupdf patches

@Opus5.5, 2026-11-10

`mupdf` is a git submodule that tracks upstream Artifex mupdf, which we cannot
push to. Fixes we need in mupdf are therefore carried as patch files in
`patches/mupdf/`, and the build applies them to the submodule's working tree.

## How the build applies them

`scripts/apply_mupdf_patches.sh` applies every `patches/mupdf/*.patch` to
`mupdf/`, in name order. It is idempotent: a patch that already applies in
reverse is taken to be applied and skipped. A patch that neither applies nor
reverses means the submodule has moved under it; the script then fails and the
patch must be refreshed against the new mupdf commit.

`build_mac.sh`, `build_linux.sh`, `build_and_release.sh` and
`linuxdeploy_build_and_release.sh` call it before building mupdf.
`build_windows.bat` does not: on Windows, run the script from Git Bash (or
`git -C mupdf apply ../patches/mupdf/<name>.patch`) before building.

Because `.gitmodules` sets `ignore = dirty` for mupdf, the applied patch does
not show up in `git status` of this repository. Check it with
`git -C mupdf diff`.

To add a patch, edit the file under `mupdf/`, then save
`git -C mupdf diff -- <files>` into a new numbered file in `patches/mupdf/`,
with a short explanation above the first `diff --git` line (git ignores text
there).

## 0001: do not compare store keys across documents

### Crash signature

A render thread died with SIGSEGV at address `0x8`:

```
pdf_get_xref_entry_aux
do_objcmp
do_objcmp
fz_find_item
pdf_load_shading
...
fz_new_pixmap_from_page_number
PdfRenderer::run
```

### Mechanism

Terms used below:

- The *store* (`fz_store`) is mupdf's cache of decoded resources such as
  images, fonts and shadings, looked up by a *key*. For PDF resources the key
  is the `pdf_obj` that describes the resource.
- An *inline* object is a PDF dictionary written directly where it is used,
  as opposed to an *indirect* object (`12 0 R`) that has its own number in the
  file's cross-reference table (the *xref*).
- Every `pdf_obj` dictionary is *bound* to the `pdf_document` it was parsed
  from.

sioyek gives each thread its own `fz_clone_context()` and its own copy of the
document. Cloned contexts share one store. Keys that are indirect objects are
found through a hash table keyed on (document, object number). Inline keys,
such as an inline shading dictionary, are found by a linear scan
(`fz_find_item` in `source/fitz/store.c`) that compares the wanted key with
every inline key in the store, whichever document it belongs to.

That comparison (`pdf_cmp_key` calling `pdf_objcmp`) walks both dictionaries.
When they match, it finishes by checking whether each one is a stream, which
reads the xref of each dictionary's document. So thread A, looking up a
shading in its own document, reads the xref of thread B's document without any
lock. If thread B is closing or reloading its document at that moment, it is
worse: `pdf_drop_document` frees the xref first and removes that document's
items from the store only later, so thread A reads freed memory and crashes.

It is also a correctness bug. Indirect references inside a key compare by
object number only, so two inline dictionaries from different files that look
alike could share a cached shading even though they refer to different
objects.

### The fix

`pdf_cmp_key` (`source/pdf/pdf-store.c`) now reports two keys bound to
different documents as different before calling `pdf_objcmp`. The check uses
`pdf_get_bound_document`, which reads only the key object's own `doc` field and
never touches the document. Lookups within one document behave as before; a
resource is no longer shared between two open copies of the same file through
an inline key, which costs at most one extra decode per copy.

The bug is unfixed in upstream mupdf as of 2026-11-10. Upstream status:

- The fix, rebased on upstream master, is the branch
  `pdf-store-cmp-key-cross-document` of https://github.com/NightMachinery/mupdf.
- Reported as https://bugs.ghostscript.com/show_bug.cgi?id=709981
  (2026-11-10), with the fix attached. The GitHub mirror
  `ArtifexSoftware/mupdf` has issues disabled and refuses pull requests from
  outside accounts, so Bugzilla is the only channel.
- Once upstream fixes it and the submodule is bumped past the fix,
  `scripts/apply_mupdf_patches.sh` will fail on this patch. Delete the patch
  file then.

### Reproducer

`tests/mupdf-store-race/` holds:

- `make_pdf.py`, which writes a one-page PDF whose 48 shadings (and their
  `/Function` dictionaries) are inline, painted with the `sh` operator from the
  page and from a form XObject.
- `stress.c`, which does what sioyek does: one base context with real lock
  callbacks, and a cloned context per thread. Each thread loops: open the PDF
  as its own document, render page 0, drop the document.
- `run.sh`, which builds mupdf with AddressSanitizer (ASan, the compiler's
  memory-error checker) into `mupdf/build/sanitize`, builds the stress program
  against it into `tests/mupdf-store-race/build/`, and runs it. It never
  writes to `mupdf/build/release`.

Run it from anywhere:

```
tests/mupdf-store-race/run.sh [THREADS] [SECONDS]              # patched, default 4 threads, 20 s
tests/mupdf-store-race/run.sh --unpatched [THREADS] [SECONDS]  # without the fix
```

`--unpatched` reverses the patch for the build, builds into
`mupdf/build/sanitize-unpatched`, and re-applies the patch when it exits.
Without the fix, it fails within a few seconds with the stack of the crash:
`pdf_get_xref_entry_aux` called from `do_objcmp`, `fz_find_item` and
`pdf_load_shading`. ASan reports either a heap-use-after-free (the memory was
freed by `pdf_drop_document_imp` on another thread) or a SEGV at address `0x8`,
the address of the original crash. With the fix, 4 threads for 120 s (about
6000 open, render and drop cycles) and 8 threads for 90 s both end with
`0 errors` and exit status 0.

### Checking the fix in the app

@Fable5.2, 2026-11-10

The reproducer PDF also crashes an unpatched sioyek itself, with the original
stack: start sioyek, then open `tests/mupdf-store-race/build/inline-shadings.pdf`
in the running instance. A build that links the patched mupdf survived 15 such
opens in a row.

This check caught a build pitfall. The app's Makefile used to have no
dependency on `libmupdf.a`, so an incremental build after a change in
`patches/mupdf/` rebuilt the library but kept the old one inside the sioyek
binary. `pdf_viewer_build_config.pro` now lists the mupdf libraries in
`PRE_TARGETDEPS`, which makes the app relink whenever they change.
