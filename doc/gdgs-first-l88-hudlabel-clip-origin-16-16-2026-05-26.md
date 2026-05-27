# GDGS first-L88 heading `G` packet: exact source-backed origin of the direct `HudLabel` clip rect left/top edge `(16,16)` on the preserved route (2026-05-26)

## Goal

Continue exactly from Task 218's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF rect packet,
- now that the packet's global rect is already fixed as `(15,19,14,16)` against owner clip rect `(16,16,504,460)`,
- what exact layout/global-rect path places the direct `HudLabel` clip owner so that its left/top clip edge is exactly `(16,16)`?

This slice stays source-backed and uses the already-fixed direct-owner/global-rect facts, without reopening packet-local glyph geometry or broader provenance theories.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact direct owner route already fixed: `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...) -> single draw_msdf_rect_region(...)`
- exact clip-overlap fact already fixed: packet global `(15,19,14,16)` vs owner clip rect `(16,16,504,460)` means left-edge-only non-containment by `1 px`
- exact owner clip route already fixed: direct `HudLabel` `RichTextLabel` with `clip_contents=true`

It does **not** reopen broader provenance links, caller-family identity, owner identity, font selection, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## The `HudMargin` node authors the `16,16` viewport-space anchor directly

The repro scene itself places the parent `MarginContainer` at `16 px` from the viewport left/top:

```tscn
[node name="HudMargin" type="MarginContainer" parent="CanvasLayer"]
offset_left = 16.0
offset_top = 16.0
offset_right = 520.0
offset_bottom = 220.0
```

The scene-builder script writes the same values programmatically when regenerating that scene:

```gdscript
margin.offset_left = 16.0
margin.offset_top = 16.0
margin.offset_right = 520.0
margin.offset_bottom = 220.0
```

So the first concrete source of the eventual clip-owner left/top edge is not inside glyph draw at all. It starts one level up in the scene layout: the authored `HudMargin` control is intentionally positioned at viewport-space `(16,16)`.

## Why those offsets are interpreted directly in viewport space on this route

For a `Control`, anchor/offset layout is resolved from `get_parent_anchorable_rect()`.

When a control has no parent `CanvasItem`, that parent anchorable rect falls back to the viewport visible rect:

```cpp
if (data.parent_canvas_item) {
	parent_rect = data.parent_canvas_item->get_anchorable_rect();
} else {
	parent_rect = get_viewport()->get_visible_rect();
}
```

`HudMargin` is parented directly under `CanvasLayer`, not under another `Control`, so on this route it resolves against the viewport visible rect.

The `CanvasLayer` scene entry does not author any transform/offset, and `CanvasLayer::get_final_transform()` returns its stored `transform` unchanged when follow-viewport is disabled:

```cpp
if (is_following_viewport()) {
	...
	return follow * transform;
}
return transform;
```

So on this preserved route there is no extra parent-side UI translation in front of `HudMargin`'s own `16,16` offsets.

## The `HudLabel` is then placed at local `(0,0)` inside `HudMargin`

`MarginContainer` lays out each child into an inner rect that begins at its theme margins:

```cpp
int w = s.width - theme_cache.margin_left - theme_cache.margin_right;
int h = s.height - theme_cache.margin_top - theme_cache.margin_bottom;
fit_child_in_rect(c, Rect2(theme_cache.margin_left, theme_cache.margin_top, w, h));
```

The default theme gives `MarginContainer` zero margins on all four sides:

```cpp
theme->set_constant("margin_left", "MarginContainer", 0);
theme->set_constant("margin_top", "MarginContainer", 0);
theme->set_constant("margin_right", "MarginContainer", 0);
theme->set_constant("margin_bottom", "MarginContainer", 0);
```

So for this exact `HudMargin`, the child placement rect starts at:

```text
Rect2(0, 0, width, height)
```

`Container::fit_child_in_rect(...)` then keeps that rect origin unless size-flag shrink rules move it. For `HudLabel`, the left/top edge is not shifted away from that origin, so the label is placed at local `(0,0)` inside `HudMargin`.

## That local `(0,0)` becomes global `(16,16)` through the standard Control transform path

Container placement ultimately sets the child control rect via:

```cpp
p_child->set_rect(r);
```

and `set_rect(...)` stores the rect with begin anchors:

```cpp
for (int i = 0; i < 4; i++) {
	data.anchor[i] = ANCHOR_BEGIN;
}
_compute_offsets(p_rect, data.anchor, data.offset);
```

When the control's canvas transform is updated, Godot adds that local control position into the control transform origin:

```cpp
Transform2D xform = _get_internal_transform();
xform[2] += get_position();
```

And `get_global_rect()` reports the global transform origin as the global rect position:

```cpp
Transform2D xform = get_global_transform();
return Rect2(xform.get_origin(), xform.get_scale() * get_size());
```

So on this route the left/top path is concrete:

```text
CanvasLayer identity transform
+ HudMargin authored viewport offsets (16,16)
+ HudLabel container-local position (0,0)
= HudLabel global rect origin / clip left-top edge (16,16)
```

## Why this is the owner clip edge specifically

`RichTextLabel` enables clipping in its constructor:

```cpp
set_clip_contents(true);
```

During cull, a clipping control's final clip rect is built from its global rect intersection:

```cpp
ci->final_clip_rect = p_clip_rect.intersection(global_rect);
```

with the no-parent-clip case taking the control's own global rect as the decisive boundary on this route.

So once the direct owner was already fixed to `HudLabel`, the clip edge at `(16,16)` is not mysterious renderer state. It is the same `HudLabel` global rect origin produced by the ordinary control layout path above.

## Classification

For the preserved first-`L88` heading-`G` packet, the exact source-backed origin of the owner clip rect left/top edge `(16,16)` is best classified as:

```text
scene-authored HudMargin viewport offsets (16,16)
-> zero-margin MarginContainer child layout
-> HudLabel local position (0,0) within HudMargin
-> Control global rect origin = (16,16)
-> direct HudLabel clip rect left/top edge = (16,16)
```

This means the clip edge is coming from ordinary UI layout placement of the direct owner control, not from glyph-local geometry and not from a hidden extra offset inserted later in the draw path.

## Conclusion

Task 218 closed the packet/clip overlap fact itself. This slice closes the next honest adjacent seam behind that fact.

For the exact preserved first heading `G` packet on the direct `HudLabel` route:

- the `HudLabel` clip left/top edge `(16,16)` originates upstream in the scene layout, not inside glyph emission
- `HudMargin` is explicitly authored at viewport offsets `(16,16)` in both the saved scene and the scene-builder script
- `MarginContainer` contributes no extra left/top inset here because its default theme margins are all zero
- `HudLabel` is placed at local `(0,0)` inside that zero-margin container rect
- Control global-rect calculation then exposes the label's global origin as `(16,16)`, which becomes the direct owner clip edge

So the already-fixed left-edge-only non-containment fact now has a concrete placement origin: the packet's owner clip starts at `(16,16)` because the direct owner control itself is laid out there by the scene's `HudMargin` container path.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the immediate owner-clip-origin seam is now closed at the concrete layout/global-rect level.

The next honest continuation would need to move to a genuinely new adjacent fact on the same preserved route, rather than reopening the already-closed owner, caller, glyph-local x/y, or clip-overlap/origin steps.
