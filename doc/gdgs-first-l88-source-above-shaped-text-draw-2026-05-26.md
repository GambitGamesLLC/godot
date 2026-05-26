# GDGS first-L88 source above `TextServer::shaped_text_draw(..., p_color, ...)` (2026-05-26)

## Goal

Continue exactly from Task 207's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 207 proved that the next higher shared caller-side source is `TextServer::shaped_text_draw(..., p_color, ...)`,
- can static source alone collapse that one rung farther to one exact higher caller-side source,
- or can it instead prove that the packet comes from a direct widget/control `font_draw_glyph(..., font_color)` path?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 207: `TextServer::shaped_text_draw(..., p_color, ...) -> font_draw_glyph(..., p_color, ...) -> _font_draw_glyph(..., p_color, ...) -> modulate.rgb -> p_modulate.rgb -> rect->modulate.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed classification

Static source does **not** justify collapsing the preserved packet one rung farther to a **single exact higher caller** above `TextServer::shaped_text_draw(..., p_color, ...)`, and it also does **not** prove that this packet instead came from a direct widget/control `font_draw_glyph(..., font_color)` path.

The immediate higher caller rung is a **fan-in point** with multiple live callers.

### Live `shaped_text_draw(..., p_color, ...)` callers

Examples in live source:

- `TextLine::draw(..., const Color &p_color, ...)` returns `TS->shaped_text_draw(rid, p_canvas, ofs, clip_l, clip_l + width, p_color, p_oversampling);`
- `TextParagraph::draw(..., const Color &p_color, ...)` calls `TS->shaped_text_draw(lines_rid[i], p_canvas, ofs, clip_l, clip_l + l_width, p_color, p_oversampling);`
- `CodeEdit::_draw_line_numbers()` calls `TS->shaped_text_draw(text_rid, get_text_canvas_item(), ofs, -1, -1, number_color);`

So above `TextServer::shaped_text_draw(...)`, there is not one unique static caller identity. There are multiple legitimate higher caller families that can supply the same `p_color` lane.

### Direct widget/control `font_draw_glyph(..., font_color)` callers also exist

Live source also shows separate direct widget/control glyph calls, for example:

- `Label` calling `TS->font_draw_glyph(..., p_font_color)`
- `RichTextLabel` calling `TS->font_draw_glyph(..., font_color)`
- `LineEdit` and `TextEdit` calling `TS->font_draw_glyph(..., font_color/gl_color)`

But static source alone does **not** prove that the preserved packet came from one of those direct widget glyph paths instead of the shared `shaped_text_draw(...)` path already identified in Task 207.

## Honest result for this rung

The exact higher caller-side source **cannot be uniquely collapsed farther** from static source alone.

The most precise source-backed wording is:

```text
shared text-path stop point = TextServer::shaped_text_draw(..., p_color, ...)
next higher caller rung = multiple live callers (TextLine, TextParagraph, CodeEdit, ...)
alternative direct glyph rung also exists = Label / RichTextLabel / LineEdit / TextEdit / ...
packet-side winner not uniquely proven by static source alone
```

So this slice ends in a **precise ambiguity**, not a speculative choice.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet:

- the shared provenance path through `TextServer::shaped_text_draw(..., p_color, ...)` remains valid
- there are multiple live higher callers above that rung
- direct widget/control `font_draw_glyph(..., font_color)` paths also exist in parallel
- static source alone does **not** uniquely prove which of those higher caller families supplied the preserved packet

Best narrow wording after this slice:

> on the preserved first-`L88` no-outline MSDF lane, static source can narrow the color provenance up to the shared `TextServer::shaped_text_draw(..., p_color, ...)` rung, but not uniquely beyond it; above that point the code fans into multiple live `shaped_text_draw` callers, while separate direct widget `font_draw_glyph(..., font_color)` paths also exist, so choosing one exact higher caller would require new evidence rather than static-source guesswork.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest seam is **not** another static-source-only collapse. It is a narrow evidence step to distinguish the higher caller family for the preserved packet, for example:

- source-backed breadcrumbing or the smallest reversible contrast that can distinguish the shared `shaped_text_draw(..., p_color, ...)` family from the direct widget `font_draw_glyph(..., font_color)` family
- or, if runtime evidence already exists elsewhere in the approved plan scope, tie that evidence directly to one of those caller families

Without such new evidence, the exact higher caller above `TextServer::shaped_text_draw(...)` should remain intentionally unresolved.
