# Beamer slides

@Fable5.2, 2026-11-11

The option `beamer_slides_opinionated_fixes` (on by default in this fork)
changes how jumps land in a PDF that sioyek detects as a deck made with the
LaTeX beamer class. Set `beamer_slides_opinionated_fixes 0` in
`prefs_user.config` to turn it off.

## What changes

A **whole-slide jump** shows the target slide the way presentation mode
does. Following a PDF link (by mouse or with `open_link`) and choosing an entry
in the table of contents (`goto_toc`) are whole-slide jumps in a detected deck.
A whole-slide jump does one of two things:

- **Snap to fit.** When the current zoom is within 5% of the zoom at which the
  slide exactly fills the height of the window above the status bar, and that
  height is what limits the fit, the jump sets the zoom to that exact fit and
  centers the slide in that area. No strip of a neighbouring slide remains.
- **Top alignment.** Otherwise the top of the slide is aligned with the top of
  the window, and the zoom is left alone. Whatever space is left shows the
  next slide below.

(Earlier versions centered a slide that fits, which left a strip of the
previous slide above it, and then only top-aligned, which left a strip below
it whenever the zoom was a few pixels short of the fit.)

Without the option, a link jump centers the link's destination. In a beamer
deck every destination is the top edge of a slide, so the window showed the
bottom half of the previous slide and the top half of the target.

Presentation mode already shows one page at a time and is unchanged. The
options `align_link_dest_to_top` and `toc_jump_align_top` are ignored for
whole-slide jumps. Other jumps (search results, highlights, smart jump,
marks) are not affected.

## Preloading link targets

Jumping to a page that is not rendered yet shows a blank page for a moment.
In a detected deck, sioyek therefore keeps the pages that the visible slides
link to rendered in the background (**link target preloading**), in normal and
in presentation mode. At most 16 target pages are kept per view, and the
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
ordinary PDFs (not detected). The jumps and the preloading were not exercised
by an automated test, because they need a real OpenGL window; they rely on
manual checks.
