# GDGS first-L88 first heading `G` packet: packet-local geometry versus clip relation (2026-05-26)

## Goal

Continue exactly from Task 214's explicit next seam without widening scope.

The narrowed question here is:

- now that the first preserved clipped white no-outline MSDF packet is already pinned directly to the first shaped heading `G` emission,
- what is the next honest adjacent packet-local geometric/clip fact about that exact packet itself?

This slice stays source-backed and uses no new runtime probe.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet owner already fixed: direct `HudLabel` `RichTextLabel` canvas item
- exact branch already fixed: `_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...)`
- exact shaped packet already fixed on the preserved runtime: first heading `G`, `font_rid RID(687194767361)`, `font_size 16`, `glyph index 42`
- exact preserved packet facts reused from the existing artifact:
  - `clip_rect={x=16,y=16,w=504,h=460}`
  - `command.rect={x=-1,y=3,w=14,h=16}`
  - `command.source={x=1,y=1,w=14,h=16}`
  - `outline=0`
  - white modulation

It does **not** reopen provenance links already closed, or widen into speculative fixes.

## The right next packet-local split

The clean next split is not another provenance question. That lane is already closed.

The next adjacent fact is the geometric/clip relationship of the fixed first `G` packet itself:

- is the packet already pre-cropped before submission,
- or is a full glyph quad submitted and then clipped later by the owner-side scissor/clip state?

Source plus the preserved packet artifact now answer that directly.

## Preserved packet facts

The existing preserved artifact logs the first clipped packet as:

```text
clip_rect={x=16.000000,y=16.000000,w=504.000000,h=460.000000}
command.rect={x=-1.000000,y=3.000000,w=14.000000,h=16.000000}
command.source={x=1.000000,y=1.000000,w=14.000000,h=16.000000}
outline=0
```

Two narrow facts matter here:

1. the packet still carries a **full submitted rect/source pair** for the glyph draw (`w=14`, `h=16` on both rect and source)
2. clipping is represented separately as a **batch clip/scissor rect**, not as a reduced glyph source rect or a rewritten packet size

That already suggests the clip is happening after packet submission, not by replacing the glyph packet with a pre-cropped one.

## Source-backed packet geometry

On the MSDF route, `TextServerAdvanced::_font_draw_glyph(...)` computes a destination rect and submits the glyph through one direct call:

```cpp
Point2 cpos = p_pos;
cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
Size2 csize = fgl.rect.size * (double)p_size / (double)fd->msdf_source_size;
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

The fallback server mirrors the same MSDF pattern.

So packet-local geometry is produced as one ordinary glyph quad:

```text
destination rect = Rect2(cpos, csize)
source rect = fgl.uv_rect
```

not as a special “already clipped” text packet type.

## Source-backed clip path

The clip side is established independently by the owner/control route.

`RichTextLabel` enables clipping at the control level in its constructor:

```cpp
set_clip_contents(true);
```

On the canvas-cull side, clipped controls get a `final_clip_rect` by intersecting the item's global rect with the inherited clip rect:

```cpp
ci->final_clip_rect = p_clip_rect.intersection(global_rect);
```

Then on the canvas-render side, batches with a clip owner re-establish scissor using that `final_clip_rect`:

```cpp
RD::get_singleton()->draw_list_enable_scissor(draw_list, current_clip->final_clip_rect);
```

The temporary packet logger also prints the packet's `clip_rect` from that same `current_batch->clip->final_clip_rect`, separately from the packet's own `command.rect`.

So the draw path is structurally:

```text
full glyph packet submitted
+
owner-side clip/scissor state applied by the batch
```

not:

```text
glyph packet geometry already rewritten to its visible intersection before submission
```

## Classification

For the fixed first preserved heading `G` packet, the deeper packet-local geometric/clip relation is best classified as:

```text
full no-outline MSDF glyph quad submission, clipped later by owner-side RichTextLabel/Control scissor
```

More narrowly:

- the packet remains a normal glyph rect submission with its own destination rect and source rect intact
- the “clipped” part is **not** encoded as a smaller packet-local source/destination rewrite in the preserved artifact
- clipping is imposed afterward by the batch's separate `clip_rect` / scissor state from the direct `HudLabel` control clip path

Best narrow wording after this slice:

> the first preserved `G` packet is still submitted as a full no-outline MSDF glyph rect, and the visible clipping happens at the later owner-side `final_clip_rect` scissor stage rather than as an earlier packet-local rect/source crop.

## Conclusion

The next honest adjacent fact on the now-fixed first `G` packet is no longer about who emitted it. It is about **how its geometry meets clipping**.

That relation is now closed:

- packet geometry stays a normal full glyph-quad submission
- clipping happens later and separately through the direct `HudLabel`/RichTextLabel clip-owner scissor path

So the deeper packet-local geometric/clip truth for this lane is **full packet first, owner-side scissor clip second**.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest adjacent seam would be to split this newly fixed geometry/clip relation one rung deeper — for example, classify whether the first visible clipping on the fixed `G` packet is best described by the packet's own local destination-rect placement (`Rect2(cpos, csize)`) versus the later scissor rectangle boundary, without reopening any already-closed provenance identities.
