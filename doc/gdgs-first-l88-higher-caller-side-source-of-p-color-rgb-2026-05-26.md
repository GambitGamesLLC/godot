# GDGS first-L88 higher caller-side source of glyph-draw `p_color.rgb` (2026-05-26)

## Goal

Continue exactly from Task 206's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 206 proved that the packet's caller-side `p_modulate.rgb` comes from glyph-draw `p_color.rgb`,
- what is the next honest higher caller-side source identity for that `p_color.rgb`?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 206: `p_color.rgb -> modulate.rgb -> p_modulate.rgb -> rect->modulate.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed answer

The next honest higher caller-side source identity on the shared text-rendering path is:

```text
TextServer::shaped_text_draw(..., p_color, ...)
-> TextServer::font_draw_glyph(..., p_color, ...)
-> TextServerAdvanced::_font_draw_glyph(..., p_color, ...)
-> modulate.rgb
-> p_modulate.rgb
```

Source-backed reason:

- `servers/text/text_server.cpp` forwards its own `p_color` argument directly into `font_draw_glyph(..., p_color, ...)` while iterating shaped glyphs.
- `modules/text_server_adv/text_server_adv.cpp` receives that same `p_color` in `_font_draw_glyph(...)` and, on the MSDF path already classified in Task 206, does `Color modulate = p_color;` before forwarding it into the MSDF rect draw.

So the immediate higher caller-side source of the glyph-draw `p_color.rgb` is not another hidden renderer-side color factor. It is the shared text-layout draw color parameter on `TextServer::shaped_text_draw(...)`.

## Why this is the right one-rung split

Task 206 already exhausted the MSDF glyph implementation layer:

```text
p_color.rgb -> modulate.rgb -> p_modulate.rgb
```

The next honest inward move therefore has to step one layer higher in the text draw stack.

`TextServer::shaped_text_draw(...)` is the first shared caller-side surface in live source that forwards a single color parameter directly into repeated `font_draw_glyph(..., p_color, ...)` submissions for shaped glyphs. That makes it the correct next source identity for this rung.

Going farther than that in this same slice would stop being a one-rung continuation, because above `shaped_text_draw(...)` the engine fans out into multiple widget/control-specific callers and helper wrappers.

## Important scope boundary

This slice can classify the next exact higher shared source identity, but it cannot honestly name one unique widget/control caller for the preserved packet from static source alone.

Live source shows several upstream call families that can supply color into the text stack, including:

- `TextServer::shaped_text_draw(..., p_color, ...)`
- direct widget/control `TS->font_draw_glyph(..., font_color)` calls such as labels, line edits, rich text, and text edits
- `Font::draw_char(..., p_modulate, ...)` forwarding to `TS->font_draw_glyph(..., p_modulate, ...)`

So the exact one-rung answer here is the shared text-layout caller-side parameter `TextServer::shaped_text_draw(..., p_color, ...)`, not a claim that one specific GUI control has already been uniquely proven for the preserved packet.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the next honest higher caller-side source of glyph-draw `p_color.rgb` is the shared text draw parameter on `TextServer::shaped_text_draw(...)`, which forwards its `p_color` unchanged into `font_draw_glyph(..., p_color, ...)`:

```text
TextServer::shaped_text_draw(..., p_color, ...)
-> TextServer::font_draw_glyph(..., p_color, ...)
-> TextServerAdvanced::_font_draw_glyph(..., p_color, ...)
-> modulate.rgb
-> p_modulate.rgb
-> rect->modulate.rgb
```

Best narrow wording after this slice:

> on the preserved first-`L88` no-outline MSDF glyph path, the next higher caller-side source of glyph-draw `p_color.rgb` is the shared text-layout draw color parameter on `TextServer::shaped_text_draw(...)`, which forwards `p_color` unchanged into `font_draw_glyph(...)`; static source alone does not yet justify collapsing that farther to one unique widget/control caller for the preserved packet.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is to classify the exact higher caller-side source of `TextServer::shaped_text_draw(..., p_color, ...)` for the preserved packet, or explicitly prove that the packet is instead coming from one of the direct widget/control `font_draw_glyph(..., font_color)` call sites.

That continuation should remain narrow and evidence-backed rather than guessing a specific UI control.
