# Beamer slides

@Fable5.2, 2026-11-12

The option `beamer_slides_opinionated_fixes` (on by default in this fork)
changes how sioyek shows a PDF that it detects as a deck made with the LaTeX
beamer class. Set `beamer_slides_opinionated_fixes 0` in `prefs_user.config`
to turn it off.

## Presentation mode by default

A detected deck opens in presentation mode. Presentation mode is existing
sioyek behaviour (`toggle_presentation_mode`), and it does the following:

- Only the current page is drawn, without page separators.
- On every frame the zoom is refit so that the whole page fits the window
  above the status bar, and the page is centered there. A manual zoom
  therefore does not stick while the mode is on, unless
  `presentation_mode_force_fit` is off (see "Zooming in presentation mode").
- Scrolling, the vertical and horizontal move commands and the screen-wise
  move commands step by whole pages.
- Following a link goes to the target page, not to an offset within it.
- The pages after the current one are prerendered when
  `prerender_next_page_presentation` is set.

An earlier version of this option kept normal mode and tried to imitate that
layout on link jumps (top alignment, then snapping the zoom). It left strips
of neighbouring slides in view and was removed in favour of the real mode.

## Zooming in presentation mode

`presentation_mode_force_fit` (default 1, the upstream behaviour) controls the
per-frame refit. With `presentation_mode_force_fit 0`:

- The page is fit and centered only when the page changes, when the mode is
  turned on, when the window is resized, or when a fit command runs. Between
  those moments the zoom and the position within the page are left alone.
- While the zoom is above the fit, scrolling and the move commands pan within
  the page. The view is clamped to the page, so panning never changes the
  page. At or below the fit they step by whole pages as before. `next_page`,
  `previous_page`, links and the table of contents change the page at any zoom.
- `fit_to_page_width`, `fit_to_page_height`, their `_smart` variants and
  `fit_to_page_height_and_width_smart` redo the presentation fit, so they
  double as "reset my zoom". With the option on they are unchanged (and their
  effect is overwritten by the next frame, as before).

## Remembered per document

Toggling presentation mode by hand (`toggle_presentation_mode`,
`turn_on_presentation_mode`) records that choice for the document, for every
PDF and not only decks. The **saved presentation mode** is applied whenever
the document is opened again and takes precedence over the detection. So a
deck that you switched back to normal mode stays in normal mode, and an
ordinary PDF that you left in presentation mode reopens in it.

The choice is stored in the table `presentation_modes` of `shared.db`, keyed
by the document checksum. It has its own table because the `opened_books` row
is rewritten as a whole on every save, also by sioyek builds that do not know
about this value. There is no command yet that forgets a saved choice.

History navigation (`prev_state`, `next_state` and their per-document
variants) no longer changes presentation mode. Before, going back to a history
entry recorded in the other mode switched the mode.

Following a link in presentation mode now adds a history entry, so
`prev_state` and `prev_state_in_document` return to the slide that held the
link. Upstream presentation mode jumped without recording one.

A table-of-contents jump in presentation mode now goes to the entry's page.
Before, it centered the entry's offset, which for an entry at the top edge of
a page could select the previous page.

## Preloading link targets

Jumping to a page that is not rendered yet shows a blank page for a moment.
sioyek therefore keeps the pages that the visible pages link to rendered in
the background (**link target preloading**). This happens in presentation
mode for every document, whatever `beamer_slides_opinionated_fixes` is set
to, and in normal mode for detected decks when the option is on. In a
document whose pages link to more than 16 other pages (a paper with many
citations), only the first 16 targets found are preloaded. At most 16 target pages are kept per view, and the
render cache grows by that many pages while they are needed. A beamer slide
typically links to each section start and to its neighbours, so this costs
some memory (one rendered slide per target) in exchange for instant jumps.

A related fix applies to all documents: the page after the last visible one
was prerendered under a cache key that the display never asked for (slice
index 0 instead of -1 for unsliced pages), so that prerendering had no effect.

## How a deck is detected

`Document::is_beamer_slides()` checks, once per opened document:

- the first page is wider than it is tall, and
- the PDF has a named destination `Navigation1` (beamer gives every frame a
  destination `Navigation<page number>`, also when the navigation symbols are
  hidden), or, failing that, the first page has the "replace" page transition
  (`/Trans << /S /R >>`) that beamer sets on its pages.

Slides exported from other tools (PowerPoint, Keynote, Google Slides) are not
detected, on purpose: the rule only matches what beamer writes.

## Verification status

The detection was checked against a real org-beamer deck (detected) and two
ordinary PDFs (not detected). In a headless instance with its own data
directory, the deck opened in presentation mode, a toggle was saved, and the
saved choice was applied after a relaunch. With `presentation_mode_force_fit`
off, `zoom_in` kept its zoom, `move_down` stayed on the page,
`fit_to_page_height` restored the fit zoom and `next_page` moved one page at
the fit zoom. The rendering, the link jumps and
the preloading were not exercised by an automated test, because they need a
real OpenGL window; they rely on manual checks.
