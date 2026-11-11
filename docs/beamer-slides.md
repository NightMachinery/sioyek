# Beamer slides

@Fable5.2, 2026-11-11

The option `beamer_slides_opinionated_fixes` (on by default in this fork)
changes how jumps land in a PDF that sioyek detects as a deck made with the
LaTeX beamer class. Set `beamer_slides_opinionated_fixes 0` in
`prefs_user.config` to turn it off.

## What changes

A **whole-slide jump** puts the target slide in the middle of the window when
the slide fits at the current zoom, and aligns the slide's top with the top of
the window when it does not fit. Following a PDF link (by mouse or with
`open_link`) and choosing an entry in the table of contents (`goto_toc`) are
whole-slide jumps in a detected deck.

Without the option, a link jump centers the link's destination. In a beamer
deck every destination is the top edge of a slide, so the window showed the
bottom half of the previous slide and the top half of the target.

Presentation mode already shows one page at a time and is unchanged. The
options `align_link_dest_to_top` and `toc_jump_align_top` are ignored for
whole-slide jumps. Other jumps (search results, highlights, smart jump,
marks) are not affected.

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
ordinary PDFs (not detected). The jump itself was not exercised by an
automated test, because sioyek's link labels need a real OpenGL window.
