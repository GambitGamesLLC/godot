# GDGS first-L88 heading `G` packet: deeper packet-local clip geometry on the preserved route (2026-05-26)

## Goal

Continue exactly from Task 214's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF rect packet,
- now that the packet is already fixed as the first shaped heading `G` emission on the direct `HudLabel` route,
- what is the deeper packet-local geometric/clip relation of that exact packet itself?

This slice stays narrow and uses already-preserved packet artifacts plus the smallest reversible runtime check only to confirm the label's preserved runtime anchor.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact preserved runtime shaping pair already fixed: `font_rid = RID(687194767361)`, `font_size = 16`, `glyph index = 42`
- exact direct owner route already fixed: `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...) -> single draw_msdf_rect_region(...)`

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Preserved packet-local geometry already recorded

The preserved first-clipped packet artifact already recorded the exact packet-local draw rect and clip rect:

```text
clip_rect = {x=16, y=16, w=504, h=460}
command.rect = {x=-1, y=3, w=14, h=16}
```

with the same packet also already fixed as the first `HudLabel` heading `G` MSDF rect packet.

## Narrow runtime anchor check

A tiny reversible headless probe on both the managed runtime and the preserved repro runtime confirmed the direct `HudLabel` anchor position:

```text
label global rect position = (16, 16)
label local position = (0, 0)
parent = HudMargin
```

That is enough to relate the preserved packet-local rect to the preserved clip boundary without inventing a wider experiment.

## Source-backed relation of the packet rect to the clip edge

On the MSDF path, the glyph draw code emits the packet rect directly from:

```cpp
Point2 cpos = p_pos;
cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
Size2 csize = fgl.rect.size * (double)p_size / (double)fd->msdf_source_size;
draw_msdf_rect_region(..., Rect2(cpos, csize), ...)
```

For the fixed first `G` packet, the preserved packet artifact has already given the emitted rect as:

```text
Rect2(-1, 3, 14, 16)
```

and the preserved direct-owner anchor is `(16, 16)`.

So on the preserved route, the packet's translated coverage is best read as:

```text
global packet rect = (15, 19) size (14, 16)
```

while the preserved clip/scissor coverage begins at:

```text
global clip rect = (16, 16) size (504, 460)
```

That means the exact clip relation is:

- **left edge only** is clipped
- by **1 pixel** of horizontal overhang
- while the packet's top/bottom and right edges already lie inside the preserved clip rect

More explicitly, in local packet terms:

- local left edge = `-1`, so the glyph box starts one pixel before the label's left clip boundary
- local top edge = `3`, so it is already below the clip's top edge
- local width = `14`, so after the 1-pixel left overhang the remaining width stays well inside the 504-pixel clip width
- local height = `16`, fully inside the clip height once translated

## Why this is the honest tighter geometry result

This slice does **not** need to reopen glyph provenance or do another packet-identity trace.

The preserved packet-local artifact already gives the rect and clip numbers, and the preserved direct-owner route already gives the anchor point. Together, that is enough to classify the packet-local clip geometry precisely:

```text
the first `G` packet is clipped because its MSDF glyph rect overhangs the left clip boundary by 1 pixel
```

not because of:

- a top-edge clip
- a bottom-edge clip
- a right-edge clip
- a broader multi-edge scissor mismatch
- a later unrelated packet on the same route

## Classification

For the preserved first-`Command Graph (L88)` heading `G` packet, the deeper packet-local geometric/clip relation is best classified as:

```text
single-edge left clip of the glyph rect
= 1-pixel left overhang past the label clip boundary
```

More explicitly:

```text
HudLabel anchor = (16, 16)
packet rect local = (-1, 3, 14, 16)
packet rect global = (15, 19, 14, 16)
clip rect global = (16, 16, 504, 460)
=> only the leftmost 1-pixel column is clipped
```

## Conclusion

Task 214 fixed which packet this is. This slice fixes how that exact packet is clipped.

On the preserved route, the first heading `G` MSDF packet is not generically “some clipped rect.” It is a very specific geometry case: the glyph packet sits one pixel too far left for the label clip boundary, while remaining vertically and rightward inside the clip. So the preserved clipping relation is a **single-edge left overhang clip**, not a broader multi-edge or later-packet ambiguity.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest adjacent seam is no longer the coarse clip relation. That is now resolved.

The next narrow seam would be to split one rung deeper inside this geometry result, such as:

- whether that 1-pixel left overhang comes directly from the glyph's own MSDF rect/bearing (`fgl.rect.position.x = -1` after scale) versus some upstream line/layout offset, and/or
- the exact source-backed origin of the packet's `y=3` vertical inset on the same first `G` route

without reopening already-closed provenance links or broader crash theories.
