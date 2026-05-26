# GDGS first-L88 caller-side source of MSDF draw `p_modulate.rgb` (2026-05-26)

## Goal

Continue exactly from Task 205's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 205 proved that rect-command `rect->modulate.rgb` is just a direct alias of caller-side `p_modulate.rgb`,
- what is the next honest caller-side source identity for that `p_modulate.rgb`?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 205: `p_modulate.rgb -> rect->modulate.rgb` on the MSDF rect path

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed answer

The next honest caller-side source identity is **not** another renderer-side factor. On the live MSDF text glyph path, the next step is:

```text
p_color.rgb -> modulate.rgb -> p_modulate.rgb
```

Source-backed chain:

1. `ImageTexture::draw_msdf_rect_region(..., const Color &p_modulate, ...)` forwards its `p_modulate` unchanged to `RenderingServer::canvas_item_add_msdf_texture_rect_region(...)`.
2. On the MSDF glyph path in `modules/text_server_adv/text_server_adv.cpp`, the caller of `draw_msdf_rect_region(...)` first does:

```cpp
Color modulate = p_color;
```

and then, on the no-outline MSDF branch, calls:

```cpp
ffsd->textures[fgl.texture_idx].texture->draw_msdf_rect_region(..., modulate, 0, ...);
```

So the exact caller-side source feeding the packet's `p_modulate.rgb` is the glyph-draw color carrier `p_color.rgb`, with a local alias through `modulate.rgb`.

## Why this is the right one-rung split

Task 205 already exhausted the renderer-side rect command provenance:

```text
p_modulate.rgb -> rect->modulate.rgb
```

The next honest inward move therefore has to stay on the caller side of that same MSDF draw path.

On that path, there is no extra blend/product before the draw submission. The live no-outline MSDF glyph code simply copies `p_color` into a local `modulate`, then forwards it as the `p_modulate` argument of `draw_msdf_rect_region(...)`.

So the correct one-rung continuation is not a broader scene-wide color theory. It is the tighter direct provenance alias:

```text
p_color.rgb -> modulate.rgb -> p_modulate.rgb
```

## Higher API framing that keeps the provenance consistent

This provenance is also consistent with the public text/font draw APIs:

- `TextServer::font_draw_glyph(..., const Color &p_color, ...)`
- `Font::draw_char(..., const Color &p_modulate, ...)` forwarding that modulation to `TS->font_draw_glyph(..., p_modulate, ...)`
- `TextServer::shaped_text_draw(..., const Color &p_color, ...)` forwarding that same color to `font_draw_glyph(..., p_color, ...)`

Those broader call surfaces matter only as confirmation that the live glyph path still treats this lane as a caller-provided text color/modulation input, not a renderer-internal recomposition.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the exact caller-side source of the packet's MSDF draw `p_modulate.rgb` is the glyph-draw color input **`p_color.rgb`**, copied locally through `modulate.rgb` before being forwarded into the MSDF rect draw:

```text
p_color.rgb -> modulate.rgb -> p_modulate.rgb -> rect->modulate.rgb
```

Best narrow wording after this slice:

> on the preserved first-`L88` no-outline MSDF glyph path, the packet's caller-side `p_modulate.rgb` is not terminal; it comes directly from the glyph draw color input `p_color.rgb`, copied through the local `modulate` variable and then forwarded unchanged into the MSDF rect draw call.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is to classify the exact higher caller-side source of **`p_color.rgb`** for the preserved packet.

That continuation should stay narrow:

- trace the live text/font caller that supplies `font_draw_glyph(..., p_color, ...)` on the preserved packet path
- do **not** jump sideways to the broader inherited `p_item->final_modulate.rgb` lane or outward to merge-side consequences
