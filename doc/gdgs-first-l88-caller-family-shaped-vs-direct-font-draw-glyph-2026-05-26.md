# GDGS first-L88 higher caller family: shared `shaped_text_draw(...)` vs direct widget `font_draw_glyph(...)` (2026-05-26)

## Goal

Continue exactly from Task 208's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once static source proved provenance only up to the shared `TextServer::shaped_text_draw(..., p_color, ...)` rung,
- what is the narrowest new evidence that distinguishes whether the packet actually comes from that shared `shaped_text_draw(...)` family or from a direct widget/control `font_draw_glyph(..., font_color)` family?

This slice stays documentation-first and reuses already-collected narrow evidence instead of inventing a wider experiment.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact ambiguity inherited from Task 208: static source narrowed the packet only up to the shared `TextServer::shaped_text_draw(..., p_color, ...)` rung, while direct widget/control `font_draw_glyph(..., font_color)` callers also existed in parallel

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Narrowest distinguishing evidence already available

Task 208's source-only ambiguity can now be resolved by combining two later narrow findings that stayed on the same preserved lane:

1. **Owner attribution evidence** fixed the packet's runtime owner to the direct `HudLabel` `RichTextLabel` canvas item path, not to some separate hidden canvas-item branch.
2. **Owner-path source classification** then fixed the matching draw step inside that direct owner path as `RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(..., font_color)`.

That combination is enough to distinguish the higher caller family without adding another broader runtime experiment.

## Evidence chain

### 1) The exact packet owner is the direct `HudLabel` canvas item

From the existing durable owner-attribution note:

- runtime owner path was resolved to `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel`
- `HudLabel.get_canvas_item().get_id() == 128849018881`
- the only exposed internal child `CanvasItem` was the internal `VScrollBar`, with a different RID, so it does not displace the direct glyph-owner route
- `RichTextLabel::NOTIFICATION_DRAW` and `RichTextLabel::_draw_line(...)` both begin from `RID ci = get_canvas_item()` and pass that same `ci` into `TS->font_draw_glyph(...)`

This matters because it constrains the preserved packet to the direct `HudLabel` owner route rather than leaving it floating among generic text caller families.

### 2) Inside that owner path, the matching emission surface is the direct glyph loop

From the existing durable RichTextLabel step-classification note:

- `RichTextLabel::NOTIFICATION_DRAW` reaches `_draw_line(...)` for the first visible line
- `_draw_line(...)` emits actual shaped glyph packets through its direct per-glyph loop
- the preserved packet facts exclude background/foreground rect branches and exclude outline/shadow branches
- the remaining matching branch is `DRAW_STEP_TEXT`
- the exact emission call on that route is:

```cpp
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
```

So once the packet is fixed to the direct `HudLabel` RichTextLabel owner path, the matching caller family is the direct widget/control glyph path, not the shared `TextServer::shaped_text_draw(...)` helper family.

## Why this resolves Task 208's ambiguity honestly

Task 208 ended with a precise ambiguity because static source alone could not pick a winner between:

- shared higher caller families that eventually feed `TextServer::shaped_text_draw(..., p_color, ...)`, and
- direct widget/control callers that invoke `TextServer::font_draw_glyph(..., font_color)` themselves.

The later owner-path evidence is the narrow distinguishing step Task 208 explicitly called for:

- the packet is fixed to a direct `RichTextLabel` owner path
- on that direct owner path, the glyph packet is emitted by `_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(..., font_color)`
- that route does **not** use `TextServer::shaped_text_draw(...)` for this packet

So the ambiguity is no longer unresolved once those two narrower findings are combined.

## Classification

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the higher caller family is best classified as:

```text
Direct widget/control glyph family
= HudLabel (RichTextLabel) direct owner path
= RichTextLabel::NOTIFICATION_DRAW
-> RichTextLabel::_draw_line(...)
-> DRAW_STEP_TEXT
-> TS->font_draw_glyph(..., font_color)
```

and **not** as a packet coming from the shared `TextServer::shaped_text_draw(..., p_color, ...)` family.

More narrowly:

- the shared `shaped_text_draw(...)` rung remains a valid general higher provenance family in engine source
- but for this exact preserved packet, the distinguishing owner-path evidence selects the direct `RichTextLabel`/widget glyph branch instead

## Conclusion

Task 208's source-only stop point is now resolved by the narrowest already-available evidence.

For the same preserved first-L88 packet:

- static source alone could only narrow provenance up to `TextServer::shaped_text_draw(..., p_color, ...)`
- later narrow owner attribution fixed the live packet to the direct `HudLabel` `RichTextLabel` canvas item path
- later owner-path source classification fixed the matching emission step to `DRAW_STEP_TEXT -> TS->font_draw_glyph(..., font_color)`
- therefore the exact higher caller family for this preserved packet is the **direct widget/control `font_draw_glyph(..., font_color)` family**, not the shared `shaped_text_draw(..., p_color, ...)` family

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is no longer “which caller family?” That is now resolved.

The next narrow seam would be to stay on the fixed direct `HudLabel` route and split one rung deeper inside that owner path, such as:

- the exact first line-0 heading glyph emission itself, and/or
- the immediate `font_color` / `frid` selection that the first matching `DRAW_STEP_TEXT` emission receives

without reopening owner identity, shared-vs-direct caller-family ambiguity, or broader crash theories.
