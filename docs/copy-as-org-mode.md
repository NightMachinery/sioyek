# Copying as Org-mode (`copy_as_org_mode`)

@Gemini4Neon/high, 2026-11-26

`copy_as_org_mode` copies the current PDF text selection to the clipboard as
Org-mode text, preserving internal PDF links as `sioyek-v1:` links, external
URLs as standard Org links, paragraph and bullet structure, and conservative
inline styling.

## Behavior

- **Internal PDF links:** Resolved via MuPDF (`fz_resolve_link_dest`) into
  `[[sioyek-v1:<path>::<target>][<text>]]`. Whole-page destinations (`Fit`,
  `FitB`, `FitV`, `FitBV`), destinations at the top of a page (`y <= 0.5`), and
  links copied in presentation mode emit `::page` so opening them runs
  `goto_page` cleanly without triggering the `y=0` top-edge offset bug.
  Mid-page targets emit `::page:<center_x>:<y>` (using `page_width / 2` to keep
  horizontal centering and omitting zoom unless `org_copy_include_zoom 1` is
  set).
- **External links:** `http://`, `https://`, `mailto:`, and other external URIs
  become `[[<uri>][<text>]]`, or `[[<uri>]]` when the selected text matches the
  URI.
- **Cross-file `file:` links:** Relative paths resolve against the current
  document's directory. `.pdf` targets emit `sioyek-v1:` links; other files
  emit `file:` links.
- **Escaping:** `[` and `]` are escaped as `\[` and `\]` inside link targets
  and replaced with `{` and `}` inside link descriptions so brackets never
  break Org link syntax.
- **Paragraphs, bullets, and hyphenation:** MuPDF text blocks separate
  paragraphs (`\n\n`). Common bullet glyphs (`•`, `▪`, `– `, etc.) normalize to
  `- ` items without blank lines between consecutive bullets. Wrapped lines
  within a paragraph join with spaces and dehyphenate word-end hyphens; wrapped
  links merge into a single Org link.
- **Inline styles (`org_copy_preserve_styles`):** Bold (`*...*`), italic
  (`/.../`), monospace (`~...~`, falling back to `=...=` if `~` appears inside),
  and strikeout (`+...+`) are emitted when Org pre/post border rules allow.
  Italic markup is suppressed for known math fonts (`CMMI`, `CMSY`, `Math`,
  `Symbol`, etc.) and isolated single-character tokens so inline equations stay
  readable.
- **Empty selection (`org_copy_fallback_to_location`):** By default (`0`),
  invoking `copy_as_org_mode` with nothing selected is a no-op so the clipboard
  is preserved. Set to `1` to fall back to copying a `sioyek-v1:` link to the
  current view location (`[[sioyek-v1:<path>::<page>:<x>:<y>:<zoom>]]`).

## Configuration (`prefs_user.config`)

- `org_copy_preserve_styles` (bool, default `1`): emit bold, italic, monospace,
  and strikeout markers. Set to `0` for plain prose plus links.
- `org_copy_include_zoom` (bool, default `0`): append `:<zoom>` to mid-page
  internal `sioyek-v1:` links using the window's current zoom level.
- `org_copy_fallback_to_location` (bool, default `0`): when `0`, empty
  selection is a no-op that preserves the clipboard. Set to `1` to copy a
  current-view `sioyek-v1:` link when invoked with no selection.
- `org_link_path_abbrevs` (string, default empty): semicolon-separated
  `alias=prefix` rules for shortening paths in `sioyek-v1:` and `file:` links,
  for example `~nt=~/notes;~bk=/Volumes/books`. Leading `~` in prefixes expands
  against `$HOME`; longest path-boundary prefix wins; `$HOME` always abbreviates
  to `~` when no rule matches.

## Keybindings

`copy_as_org_mode` is unbound by default in `pdf_viewer/keys.config`. Sioyek's
key trie cannot bind both a single key (such as `copy y`) and a longer sequence
starting with that key (`copy_as_org_mode yo`): whichever comes later either
unmaps the prefix or leaves the longer sequence unreachable. Two clean options
in `keys_user.config`:

- Vim-style: rebind `copy y` to `copy yy` and bind `copy_as_org_mode yo`.
- Single-key without touching `y`: bind `copy_as_org_mode Y` (and optionally
  `<space>yo`).

## Testing

Run the unit tests:

```
tests/org-copy/run.sh
```
