# GDGS first-L88 heading `G` packet: origin of the 1-pixel left overhang (2026-05-26)

## Goal

Continue exactly from Task 215's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF rect packet,
- now that its clip geometry is fixed as a 1-pixel left-edge overhang,
- does that overhang come directly from the glyph's own MSDF rect/bearing, or from an upstream line/layout offset?

This slice stays narrow and uses source-backed tracing plus the smallest reversible runtime probe only where source alone stops short of the exact glyph metrics.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact preserved packet-local geometry already fixed: local packet rect `(-1, 3, 14, 16)` against direct `HudLabel` clip boundary at `(16,16)`
- exact direct owner route already fixed: `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...) -> single draw_msdf_rect_region(...)`

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed decomposition of the packet rect

On the MSDF glyph path, packet geometry is emitted as:

```cpp
Point2 cpos = p_pos;
cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
Size2 csize = fgl.rect.size * (double)p_size / (double)fd->msdf_source_size;
draw_msdf_rect_region(..., Rect2(cpos, csize), ...)
```

So the packet-local rect position can only come from two adjacent sources on this route:

1. the draw-call anchor `p_pos`
2. the glyph-local MSDF rectangle offset `fgl.rect.position`

## What the first shaped `G` actually carries at runtime

A tiny reversible preserved-runtime probe shaped the first heading body, inspected the first shaped glyph, and then queried the font glyph metrics for the preserved runtime pair (`font_rid = RID(687194767361)`, `font_size = 16`, `glyph index = 42`).

Relevant facts:

- first shaped glyph offset = `(0, 0)`
- `font_get_glyph_offset(...) = (-1, -14)`
- `font_get_glyph_size(...) = (14, 16)`
- `font_get_glyph_uv_rect(...) = [P: (1, 1), S: (14, 16)]`
- `font_get_glyph_advance(...) = (11.909375..., 21.79688)`

Those runtime glyph metrics match the preserved packet-local rect exactly on the horizontal side:

```text
packet local x = -1
packet width   = 14
```

which is the same as the queried glyph offset/size:

```text
glyph x offset = -1
glyph width    = 14
```

## Why the left overhang is glyph-local, not layout-local

The preserved first shaped glyph itself has `offset = (0, 0)`, so there is no extra shaped-glyph layout shift on the first `G` before the glyph-local rectangle is applied.

The direct `HudLabel` route also already starts at the left clip boundary:

- `HudLabel` anchor = `(16, 16)`
- local text branch starts from that direct owner route
- first shaped glyph is the first visible heading glyph on the line

Since the packet-local x-position is already `-1` **before** translation to the global clip boundary, and since the first shaped glyph has no extra x-offset, the 1-pixel left overhang is best classified as coming directly from the glyph's own MSDF rect/bearing term:

```text
fgl.rect.position.x = -1  (after the active scale on this route)
```

not from an upstream line/layout offset.

## The vertical inset on the same route

The same probe plus source reads also make the `y=3` inset understandable on the same route.

The glyph-local font offset reports:

```text
glyph y offset = -14
```

while the emitted packet rect reports:

```text
packet local y = 3
```

That means the draw-call anchor `p_pos.y` on this route is effectively the line baseline origin for the glyph, and the packet's top edge is then shifted upward by the glyph-local vertical bearing:

```text
p_pos.y + (-14) = 3
=> p_pos.y = 17
```

This is consistent with the RichTextLabel line draw path, where the baseline-side anchor is built from the line offset and ascent before `TS->font_draw_glyph(...)` is called. So the vertical inset is not an unrelated clip artifact; it is the baseline anchor plus the glyph's negative vertical bearing.

## Classification

For the preserved first-`Command Graph (L88)` heading `G` packet, the 1-pixel left overhang is best classified as:

```text
direct glyph-local MSDF rect/bearing origin
```

More explicitly:

```text
first shaped glyph layout offset = (0, 0)
glyph-local font offset = (-1, -14)
glyph-local font size   = (14, 16)
packet local rect       = (-1, 3, 14, 16)
=> left overhang comes from the glyph's own x bearing / MSDF rect position, not upstream layout
```

And on the same route:

```text
packet y = 3
= baseline-side draw anchor y (17) + glyph-local y bearing (-14)
```

## Conclusion

Task 215 fixed how the packet is clipped. This slice fixes why that clipping starts 1 pixel left of the boundary.

For the preserved first-L88 heading `G` packet, the 1-pixel left overhang does **not** come from an upstream line/layout offset. It comes directly from the glyph's own MSDF rect/bearing term on the preserved runtime route. The first shaped glyph itself carries no additional x-offset, and the queried font glyph metrics line up exactly with the preserved packet-local rect.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest adjacent seam is no longer the broad origin of the left overhang. That is now resolved.

The next narrow seam would be to split one rung deeper inside the same glyph-local geometry, such as:

- the exact source-backed origin of the baseline-side anchor `p_pos.y = 17` that combines with glyph y bearing `-14` to produce packet local `y = 3`, and/or
- whether the preserved route needs any stricter source-only tie between `font_get_glyph_offset(...)` and the internal `fgl.rect.position` cache field for this exact MSDF glyph

without reopening already-closed provenance links or broader crash theories.
