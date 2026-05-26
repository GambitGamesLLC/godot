# GDGS first-L88 clipped preserve-rect packet owner: direct `HudLabel` canvas item versus RichTextLabel-internal item (2026-05-26)

## Goal

Continue exactly from Task 207's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- now that Task 207 classified the item-local `CanvasItem.self_modulate` lane as default white,
- does the packet belong directly to the scene node path `CanvasLayer/HudMargin/HudLabel`,
- or does it instead belong to a separate RichTextLabel-internal canvas item on that same label path?

This slice stays source-backed and uses the smallest ownership trace needed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 207: the packet's `CanvasItem.self_modulate.rgb` lane is best classified as default white rather than a non-default scene-side write

It does **not** reopen non-default-self-modulate theory, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Scene-side candidate owner path

The reproducer scene contains exactly one authored UI text node on this lane:

```text
CanvasLayer/HudMargin/HudLabel
```

and that node is a `RichTextLabel`.

Both the serialized scene and the scene-builder script agree on this path:

- `scenes/gdgs_happy_path_control.tscn`
- `scripts/build_control_scene.gd`

The label is the node that owns the BBCode text shown in the preserved packet lane.

## Source-backed ownership trace inside `RichTextLabel`

The key ownership question is whether RichTextLabel text drawing creates a separate canvas item for glyph packets, or whether it draws them into the label's own canvas item.

The relevant source answer is direct.

Inside `RichTextLabel::_draw_line(...)`:

```cpp
RID ci = get_canvas_item();
```

That same `ci` is then passed to the text server for glyph drawing:

```cpp
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, outline_size, fx_offset + char_off, gl, font_color);
```

and to direct canvas-item rect helpers used by the label:

```cpp
RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(rect_off, rect_size), last_color);
```

At the higher draw entrypoint, `NOTIFICATION_DRAW` also starts from the label's own canvas item:

```cpp
RID ci = get_canvas_item();
```

Then it calls `_draw_line(...)`, which continues using that same canvas item route for the rendered text.

The text-server side keeps the same owner RID rather than swapping to a hidden text-only canvas item. In both `TextServerAdvanced::_font_draw_glyph(...)` and `TextServerFallback::_font_draw_glyph(...)`, the glyph texture eventually reaches:

```cpp
ffsd->textures[fgl.texture_idx].texture->draw_rect_region(p_canvas, Rect2(cpos, csize), fgl.uv_rect, modulate, false, false);
```

where `p_canvas` is exactly the `ci` that RichTextLabel passed in.

## Smallest reversible runtime ownership trace

Because the source path already narrowed the answer to "whatever `HudLabel.get_canvas_item()` is at runtime," I used the smallest temporary contrast needed to pin the exact live RID on the control-scene path:

- temporarily printed `_hud_label.get_canvas_item().get_id()` in `scripts/gdgs_tweak_matrix_harness.gd`
- temporarily printed `_hud_label.get_children(true)` and any child `CanvasItem` RIDs
- ran the project headless with `~/.local/bin/godot --headless --path . --quit-after 3`
- then reverted the script to its original contents

The run printed:

```text
[gdgs-harness] hud_label_owner={path=/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel,type=RichTextLabel,canvas_item_rid=128849018881,...}
[gdgs-harness] hud_label_internal_child={name=@VScrollBar@2,type=VScrollBar,canvas_item_rid=146028888066}
[gdgs-harness] hud_label_internal_child={name=@Timer@3,type=Timer,canvas_item_rid=n/a}
```

So on the live repro scene used for this validation:

- the direct `HudLabel` owner path resolved to `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel`
- that node's canvas-item RID was **`128849018881`**
- the only internal child `CanvasItem` surfaced by the node tree was the internal `VScrollBar`, with a **different** RID: `146028888066`
- the timer is not a `CanvasItem`

## What is *not* present in this path

For this ownership fork, the decisive negative evidence is also important:

- in `scene/gui/rich_text_label.cpp`, the text/glyph draw path does **not** create a separate canvas item for glyphs before drawing them
- the glyph emission path uses the node's existing `get_canvas_item()` RID directly
- the internal `VScrollBar` child does have its own RID, but it is a separate child control, not the canvas-item owner used by the RichTextLabel text draw path
- I did not find a RichTextLabel-specific alternate glyph-owner RID being created and substituted into `TS->font_draw_glyph(...)` for this route

So the packet is not sourced from a hidden text-only canvas-item surrogate on this route.

## Classification

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the better source-backed ownership classification is:

```text
direct owner path = CanvasLayer/HudMargin/HudLabel
runtime owner path = /root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel
owner canvas item RID on validation run = 128849018881
packet type = RichTextLabel text/glyph draw emitted onto that same canvas item
```

not:

```text
separate RichTextLabel-internal canvas item owner
```

RichTextLabel may have internal text/layout structures, but on this exact drawing route those structures render through the `HudLabel` node's own canvas item RID, not through a distinct internal canvas item owner.

## Why this resolves the prior ambiguity enough

Task 207 left one narrow ambiguity open:

- direct `HudLabel` ownership
- versus RichTextLabel-internal ownership on the same label path

This slice resolves that fork at the canvas-item ownership level.

Even if RichTextLabel maintains internal layout items while shaping text, the emitted glyph/MSDF packet still uses:

```text
HudLabel.get_canvas_item()
```

as the actual RenderingServer owner RID.

That is the ownership identity that matters for this preserved packet lane.

## Conclusion

For the exact preserved first clipped preserve-rect packet on the first `Command Graph (L88)` lane, the owning CanvasItem/RID/path is best classified as:

```text
scene path = CanvasLayer/HudMargin/HudLabel
runtime path = /root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel
runtime owner RID on validation run = 128849018881
```

with the packet emitted onto the `HudLabel` `RichTextLabel` node's own canvas item RID via `get_canvas_item()`.

So the earlier ambiguity is resolved in favor of:

- **direct HudLabel canvas-item ownership: yes**
- **separate RichTextLabel-internal canvas-item ownership for the packet: no**

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward continuation is no longer about packet owner identity. The direct owner path is now fixed. The next narrow seam, if any, should stay on the already-owned `HudLabel` text draw path itself — for example, classifying which exact RichTextLabel text/glyph emission step produces the first clipped white MSDF rect packet inside that direct owner route — without reopening broader theories.
