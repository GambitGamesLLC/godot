# GDGS first-L88 heading `G` packet: exact clip-overlap / containment fact on the preserved route (2026-05-26)

## Goal

Continue exactly from Task 217's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF rect packet,
- now that the packet-local geometry is fixed exactly as local `(-1, 3, 14, 16)` from the first shaped heading `G`,
- what exact overlap / non-containment fact with the owner clip rect makes this packet belong to the preserved clipped route?

This slice stays source-backed and uses only a tiny reversible preserved-runtime probe to lock the live `HudLabel` clip-owner rect/flag to the same numbers already seen in the preserved packet artifact.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact direct owner route already fixed: `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...) -> single draw_msdf_rect_region(...)`
- exact packet-local geometry already fixed: local rect `(-1, 3, 14, 16)`
- exact glyph-local geometry already fixed: left overhang from glyph-local `x = -1`, vertical inset from `18 + (-15) = 3`

It does **not** reopen broader provenance links, caller-family identity, owner identity, font selection, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## The owner clip rect is real and source-backed on this route

On the direct `HudLabel` route, `RichTextLabel` enables content clipping in its constructor:

```cpp
set_clip_contents(true);
```

During canvas cull, clipped items produce a `final_clip_rect` by intersecting the item's global rect with the inherited clip:

```cpp
ci->final_clip_rect = p_clip_rect.intersection(global_rect);
```

If that intersection collapses below `0.5` pixels in either dimension, the item is dropped entirely:

```cpp
if (ci->final_clip_rect.size.width < 0.5 || ci->final_clip_rect.size.height < 0.5) {
	return;
}
```

A tiny preserved-runtime probe on the exact repro scene fixed the direct owner rect/flag to:

```text
clip_contents = true
global_rect = (16, 16, 504, 460)
local position = (0, 0)
size = (504, 460)
```

Those values match the previously preserved first-packet artifact's `clip_rect={x=16,y=16,w=504,h=460}`.

## The packet is not fully contained; it only partially overlaps

The exact packet-local rect is already fixed from the preserved packet artifact:

```text
local packet rect = (-1, 3, 14, 16)
```

Translated through the direct `HudLabel` owner rect at `(16, 16)`, the same packet covers:

```text
global packet rect = (15, 19, 14, 16)
```

Compare that against the owner clip rect:

```text
clip rect = (16, 16, 504, 460)
```

Edge-by-edge:

- packet left   = `15`
- packet right  = `29`
- packet top    = `19`
- packet bottom = `35`

- clip left     = `16`
- clip right    = `520`
- clip top      = `16`
- clip bottom   = `476`

So the exact containment/overlap fact is:

- **full containment fails only on the left edge** because `15 < 16`
- right edge is contained because `29 <= 520`
- top edge is contained because `19 >= 16`
- bottom edge is contained because `35 <= 476`

The surviving visible overlap is therefore the exact intersection:

```text
intersection = (16, 19, 13, 16)
```

which keeps:

- `13 / 14` columns of width
- all `16 / 16` rows of height
- area `208 / 224`

So the packet is **partially overlapping, not fully contained, and not rejected**.

## Why it survives as a clipped packet instead of disappearing entirely

The cull path only drops the clip owner if the final clip intersection collapses below `0.5` pixels in width or height.

That does **not** happen here:

```text
intersection width  = 13
intersection height = 16
```

Both dimensions stay far above the drop threshold, so the packet's owner route survives with a valid `final_clip_rect`.

This is the exact clip-overlap fact one rung deeper than the earlier "1-pixel left overhang" wording:

```text
not fully contained,
but still positively intersecting by 13x16,
so it remains drawable under the owner's clip rect and appears as a clipped glyph packet
```

## How that maps to the preserved "clipped preserve-rect" lane

The temporary renderer-side preserve-rect matcher classifies a batch as part of that lane when it is:

```cpp
rect-like && current_batch->clip != nullptr && gdgs_canvas_blend_mode_uses_prior_color(batch_blend_mode)
```

So the renderer-side lane classification itself is keyed to:

- rect-like command family
- presence of a clip owner / `final_clip_rect`
- preserve-style blend mode that uses prior color

For this exact first heading `G` packet, the overlap math above explains why the packet is not merely on a potentially-clipped owner route, but is **actually clipped** on that route:

- the `HudLabel` owner clip is active and preserved
- the packet remains drawable because overlap is positive (`13x16`)
- the packet is not fully contained because the left edge misses by exactly `1 px`

That is the exact fact pattern behind the preserved clipped packet on this lane.

## Classification

For the preserved first-`Command Graph (L88)` heading `G` packet, the exact clip-overlap / containment fact is best classified as:

```text
single-edge partial-overlap with the owner clip rect
```

More explicitly:

```text
owner clip rect  = (16, 16, 504, 460)
packet global    = (15, 19, 14, 16)
intersection     = (16, 19, 13, 16)
containment      = false (left edge only)
visible overlap  = true
```

So the packet lands on the preserved clipped route as a **non-empty partial-overlap** case, not a full-containment case and not a zero-overlap/drop case.

## Conclusion

Task 217 closed the packet-local x/y inputs. This slice closes the next honest adjacent clip fact.

For the exact first heading `G` packet on the preserved first-`L88` route:

- it is **not** fully contained in the `HudLabel` clip rect
- it misses containment only on the left edge by `1 px`
- it still overlaps the clip rect by `13x16`
- that positive overlap keeps it drawable under the active owner clip, so it appears as the preserved clipped packet rather than disappearing

That is the exact overlap/containment fact one rung deeper than the already-fixed local geometry.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the immediate geometry/clip seam is now closed at the packet/clip intersection level.

The next honest continuation would need to move to a genuinely new adjacent fact on the same packet or same preserved lane, rather than reopening the already-closed owner, caller, glyph-local x/y, or clip-overlap/containment steps.
