# GDGS first-L88 heading `G` versus the first clipped MSDF packet (2026-05-26)

## Goal

Continue exactly from Task 213's explicit next seam without widening scope.

The narrowed question here is:

- once the preserved lane already has the exact first heading shaped-glyph pair fixed as
  - first visible heading glyph = `G`
  - `font_rid = RID(687194767361)`
  - `font_size = 16`
  - `glyph index = 42`
- can the first preserved clipped MSDF rect packet now be pinned directly to that exact shaped heading `G` emission,
- or is one last packet-bridge trace still honestly required?

This slice stays source-backed and uses no new runtime trace, because the existing chain is already sufficient.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact owner already fixed: direct `HudLabel` `RichTextLabel` canvas item
- exact RichTextLabel branch already fixed: `_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...)`
- exact shaping pair already fixed on the preserved runtime: first heading `G` -> `font_rid RID(687194767361)`, `glyph index 42`, `font_size 16`
- exact packet-local facts kept only as context: clipped white no-outline packet, later `submit_serial=9` / `fence_wait_error` / `BLIT_PASS`

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## The remaining bridge question

After Task 213, only one narrow ambiguity remained:

```text
first shaped heading glyph `G`
vs
first preserved clipped packet selection
```

That would require another trace only if there were still a plausible earlier packet on the same direct-owner route between:

```text
first shaped glyph entry
-> first matching `TS->font_draw_glyph(...)`
-> first clipped MSDF packet
```

The source-backed draw order closes that gap tightly enough that another packet-bridge probe is no longer needed.

## Source-backed packet ordering on this exact route

Inside `RichTextLabel::_draw_line(...)`, once the packet is narrowed to the direct-owner text branch, draw order is explicit:

1. lines are processed from the first visible line forward
2. within a line, steps are processed in enum order
3. within `DRAW_STEP_TEXT`, glyphs are processed in shaped-glyph order
4. for each visible glyph, the direct call is:

```cpp
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
```

For this lane, the earlier competing branches are already closed:

- no separate packet owner before `HudLabel`
- no background/foreground rect branch for this glyph packet
- no shadow/outline branch for the preserved no-outline packet
- no list-prefix helper before the heading body
- no earlier higher shared helper like `shaped_text_draw(...)` on this owner-fixed route

So once the first visible heading glyph is fixed as the opening `G`, the first matching direct-owner glyph emission on this route is that `G` emission itself.

## Why one glyph emission maps honestly to one MSDF packet here

The remaining honest concern would be whether a single `font_draw_glyph(...)` call might still fan out into multiple candidate clipped packets before we can pin one packet to one glyph.

On the MSDF path, the text-server implementation answers that directly.

In `TextServerAdvanced::_font_draw_glyph(...)` (and equivalently in `TextServerFallback::_font_draw_glyph(...)`), once the glyph is ensured and `fd->msdf` is true, the implementation computes the draw rect and emits exactly one MSDF rect-region draw call:

```cpp
ffsd->textures[fgl.texture_idx].texture->draw_msdf_rect_region(
    p_canvas,
    Rect2(cpos, csize),
    fgl.uv_rect,
    modulate,
    0,
    fd->msdf_range,
    (double)p_size / (double)fd->msdf_source_size
);
```

That is the packet-producing step already fixed earlier as the clipped white no-outline MSDF rect family.

So on this exact no-outline MSDF route:

```text
one visible `font_draw_glyph(...)` emission
-> one MSDF rect-region draw submission
-> one glyph packet candidate on this route
```

not a remaining ambiguous multi-packet bridge that still needs a new trace.

## Why the preserved packet can now be pinned directly

All previously-open forks are now closed in the same direction:

- owner: direct `HudLabel`
- branch: `_draw_line(...) -> DRAW_STEP_TEXT`
- immediate call: `TS->font_draw_glyph(...)`
- immediate inputs: white `font_color`, preserved-runtime bold-font `font_rid`, `font_size 16`, glyph index `42`
- first visible heading glyph: `G`
- MSDF draw implementation: single `draw_msdf_rect_region(...)` emission for that glyph on this route

Because the heading body is the first visible direct-owner text body, and because glyph order inside that body is the shaped-glyph order already probed in Task 213, the first preserved clipped MSDF packet can now be pinned directly to the first shaped heading `G` emission.

There is no remaining source-backed reason to require one more packet-bridge trace just to connect those two now-adjacent identities.

## Classification

For the preserved first-`Command Graph (L88)` clipped white no-outline MSDF packet, the best narrow classification is now:

```text
it is directly the first shaped heading `G` emission on the fixed direct-owner RichTextLabel route
```

More explicitly:

```text
HudLabel
-> RichTextLabel::_draw_line(...)
-> DRAW_STEP_TEXT
-> first visible heading glyph `G`
-> shaped pair on preserved runtime:
   font_rid RID(687194767361), font_size 16, glyph index 42
-> TS->font_draw_glyph(...)
-> single MSDF draw_msdf_rect_region(...)
-> first preserved clipped packet
```

## Conclusion

Task 213 closed the shaped-glyph side tightly enough that this final bridge no longer needs a new trace.

The first preserved clipped MSDF rect packet can now be pinned directly to the exact first shaped heading `G` emission on the preserved runtime. A further packet-bridge probe would not narrow the approved lane in a materially new way; it would only restate an already-closed adjacency.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest seam is no longer owner identity, draw-step identity, font-color identity, theme-font identity, or the first-glyph bridge. Those are now closed.

The next narrow continuation would need to move to a genuinely new adjacent fact on the same exact packet — for example, a deeper packet-local geometric fact of the first `G` rect itself (its exact glyph-rect placement/clip relation on the preserved route) — rather than reopening already-closed provenance links.
