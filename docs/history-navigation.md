# History navigation

@Fable5.2, 2026-11-12

Each window keeps a list of earlier view states (document, position, zoom).
Jumps such as following a link, a search or `goto_toc` add an entry.

- `prev_state` and `next_state` walk that list, and switch to another document
  when the entry belongs to one.
- `prev_state_in_document` and `next_state_in_document` (added in this fork)
  walk the same list but skip every entry of another document, so they never
  leave the current file. When no earlier or later entry of the current
  document exists, they do nothing.

The two pairs share one list, so they can be mixed. After a per-document step
has skipped over entries of other documents, those entries stay in the list
and `prev_state` / `next_state` still reach them. As with `prev_state`, a new
jump made after going back drops the entries that came after the current
position, including skipped ones of other documents.

`prev_state_in_document` was checked in a headless instance (jump to the end,
then back to the start). The skipping across several documents was not
exercised by a test.
