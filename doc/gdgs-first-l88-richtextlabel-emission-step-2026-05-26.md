# GDGS first-L88 direct `HudLabel` owner route: exact RichTextLabel emission step for the first clipped white no-outline MSDF rect packet (2026-05-26)

## Goal

Continue exactly from Task 208's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped preserve-rect packet,
- now that Task 208 fixed the owner RID/path to the direct `CanvasLayer/HudMargin/HudLabel` `RichTextLabel` canvas item,
- which exact RichTextLabel text/glyph emission step produces that packet inside the direct owner path?

This slice stays source-backed and uses the already-preserved packet artifact.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 208: the packet belongs to the direct `HudLabel` `RichTextLabel` canvas item RID, not to a separate internal canvas item owner

It does **not** reopen owner identity, non-default-self-modulate theory, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Preserved packet facts that constrain the step

The already-approved preserved packet artifact records:

```text
command_type=rect
outline=0.000000
modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}
```

For this task, those three facts are enough to classify the exact RichTextLabel emission branch.

## RichTextLabel glyph-emission branches on this path

Inside `RichTextLabel::_draw_line(...)`, the glyph loop emits through four relevant branches:

```cpp
if (step == DRAW_STEP_TEXT) {
	TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
} else if (step == DRAW_STEP_SHADOW_OUTLINE && frid != RID()) {
	TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, p_shadow_outline_size, fx_offset + char_off + p_shadow_ofs, gl, font_color);
} else if (step == DRAW_STEP_SHADOW && frid != RID()) {
	TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off + p_shadow_ofs, gl, font_color);
} else if (step == DRAW_STEP_OUTLINE && frid != RID()) {
	TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, outline_size, fx_offset + char_off, gl, font_color);
}
```

So there are only two branch families that can yield a glyph packet here:

- `font_draw_glyph(...)` branches: `DRAW_STEP_TEXT` and `DRAW_STEP_SHADOW`
- `font_draw_glyph_outline(...)` branches: `DRAW_STEP_SHADOW_OUTLINE` and `DRAW_STEP_OUTLINE`

## Why the outline branches are ruled out

The preserved packet logs:

```text
outline=0.000000
```

The outline branches route through `font_draw_glyph_outline(...)`, not `font_draw_glyph(...)`.

They are also skipped entirely when outline/shadow-outline requirements are not live:

```cpp
if (step == DRAW_STEP_OUTLINE && (outline_size <= 0 || font_outline_color.a == 0)) {
	continue;
} else if (step == DRAW_STEP_SHADOW_OUTLINE && (font_shadow_color.a == 0 || p_shadow_outline_size <= 0)) {
	continue;
}
```

On the default runtime theme used by the reproducer path:

```cpp
theme->set_color("font_shadow_color", "RichTextLabel", Color(0, 0, 0, 0));
theme->set_constant("outline_size", "RichTextLabel", 0);
```

And the reproducer scene/script do not add any RichTextLabel outline tags or shadow theme overrides; the BBCode in `HudLabel` only uses `[b]...[/b]`.

That means the packet is not coming from either outline family branch.

## Why the shadow branch is also ruled out

The remaining non-outline branches are:

- `DRAW_STEP_TEXT -> font_draw_glyph(...)`
- `DRAW_STEP_SHADOW -> font_draw_glyph(...)` with `+ p_shadow_ofs` and `font_shadow_color`

But the shadow branch is skipped when shadow alpha is zero:

```cpp
else if (step == DRAW_STEP_SHADOW && (font_shadow_color.a == 0)) {
	continue;
}
```

And the default RichTextLabel runtime theme sets:

```cpp
font_shadow_color = Color(0, 0, 0, 0)
```

So on this reproducer path, the shadow glyph-emission branch is not live.

## Positive match for the surviving step

That leaves the direct text branch as the surviving exact emission step:

```cpp
step == DRAW_STEP_TEXT
-> TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)
```

This also matches the packet-local facts:

- packet is a normal glyph rect, not an outline packet: `outline=0.000000`
- default RichTextLabel text color on the runtime theme is white:

```cpp
theme->set_color("default_color", "RichTextLabel", Color(1, 1, 1));
```

- RichTextLabel seeds `_draw_line(...)` with that theme color:

```cpp
_draw_line(..., theme_cache.default_color, theme_cache.outline_size, theme_cache.font_outline_color, theme_cache.font_shadow_color, ...)
```

- the scene's `HudLabel` BBCode only applies bold formatting, not color tags

So the preserved packet's white no-outline MSDF rect is best classified as the normal visible-text glyph submission, not a shadow or outline variant.

## Classification

For the exact preserved first clipped white no-outline MSDF rect packet inside the direct `HudLabel` owner route, the exact RichTextLabel emission step is:

```text
RichTextLabel::_draw_line(...)
-> DRAW_STEP_TEXT
-> TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)
-> TextServerAdvanced::_font_draw_glyph(..., p_color = font_color, ...)
```

not:

```text
DRAW_STEP_SHADOW
DRAW_STEP_OUTLINE
DRAW_STEP_SHADOW_OUTLINE
```

## Conclusion

The first clipped preserve-rect packet on the first `Command Graph (L88)` lane is best classified as the **normal RichTextLabel visible-text glyph emission step**:

```text
DRAW_STEP_TEXT -> font_draw_glyph(...)
```

The deciding evidence is narrow and source-backed:

- the packet is no-outline and white
- RichTextLabel outline branches go through `font_draw_glyph_outline(...)` and are skipped here because `outline_size == 0`
- RichTextLabel shadow branches are skipped here because the default runtime `font_shadow_color` alpha is zero
- the default runtime `default_color` for RichTextLabel is white, and the reproducer scene does not override it with color BBCode or theme shadow/outline settings

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward continuation is to stay on this now-fixed direct `DRAW_STEP_TEXT -> font_draw_glyph(...)` route and classify the exact surviving higher caller-side color/input identity inside that branch for the preserved first clipped glyph packet — for example, whether the tightest useful next rung is the `font_color` value after `_find_color(...)` / bold-format resolution, or the exact first glyph/subspan within the `HudLabel` BBCode text that maps to the preserved packet.
