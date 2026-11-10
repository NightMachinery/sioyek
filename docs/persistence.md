@Opus5.5, 2026-11-10

# Session persistence

Sioyek remembers two things between runs: where you were in each document, and
which documents (tabs and windows) to reopen at startup. This page describes
where that state lives, when it is written, and what a crash can still lose.

## What is stored where

Both files live in the sioyek data directory (the platform's standard
application data location, next to `shared.db` and `local.db`).

- **The `opened_books` row.** One row per document in the `opened_books` table
  of `shared.db`, keyed by the document checksum. It holds the zoom level, the x
  and y offsets, the detected paper name and `last_access_time`. The recents
  list is ordered by `last_access_time`, and reopening a document restores its
  zoom and position from this row.
- **`last_document_path.txt`.** The session to restore at startup. Without
  `restore_all_windows_on_startup`, it holds the current window's tab list, one
  path per line, the current document first. With it, it holds one
  `[window] <geometry hex>` line per window, each followed by that window's tab
  list.

Writing both of these is called a **persist** below. It is done by
`MainWidget::persist()`, which updates the `opened_books` row of each relevant
document (setting `last_access_time` to now) and then rewrites
`last_document_path.txt`.

`last_document_path.txt` is written atomically: sioyek writes a temporary file
and renames it over the old one (`QSaveFile`), so a crash during the write
leaves either the old or the new content, never an empty file.

## When a persist happens

- **On open.** As soon as a document is opened in a window, whether from the
  open dialog, a link or portal, history navigation, a tab switch, drag and
  drop or the command line, sioyek persists, so the new document is at the top
  of the recents list and in the session file immediately.
- **On leave.** Before a window switches to another document, the row of the
  document being left is updated with its final position.
- **On tab close and new window.** Closing a tab from the tab list, or opening
  a new window, persists so the session file matches the visible tabs.
- **On clean close.** Closing a window or quitting persists, including drawings
  (see below).
- **Periodic checkpoint.** Every `persistance_period` seconds (default 30) each
  window compares a cheap **snapshot** of the state a persist would write (for
  each window: document path, zoom, offsets, tab list, and window geometry
  when restoring all windows) with the snapshot taken at its last persist. It
  persists only if they differ, so an idle sioyek does not write to disk.
  Closing one of several windows is reflected in the session file at the next
  checkpoint, since the closing window is still listed while it closes.

During startup, while the previous session is still being reopened, persists
update the `opened_books` rows but do not rewrite `last_document_path.txt`, so
a crash halfway through restoring several windows cannot replace the saved
session with a partial one. Once startup is done, sioyek persists once.

## The `persistance_period` key

`persistance_period` (in `prefs_user.config`) is the checkpoint interval in
seconds. The default is `30`. A value of `0` or `-1` disables the periodic
checkpoint; the persists on open, leave and close still happen.

The separate `fail_safe_auto_save_session` key, when enabled, makes the persist
on open also save drawings, annotations and extras.

## What a crash can still lose

- **Recent scrolling and zooming.** At most the last `persistance_period`
  seconds of position and zoom changes in the current document.
- **Freehand drawings and document extras.** These are kept in sidecar files
  that are only written when leaving a document, on clean close, or on open
  when `fail_safe_auto_save_session` is enabled. Drawings made since then are
  lost in a crash. Periodic checkpoints do not save drawings.
- **Not lost:** highlights, bookmarks, marks and portals. They are written to
  the database as soon as they are created.
