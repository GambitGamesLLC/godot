# GDGS first-L88 higher caller family: shared `shaped_text_draw` path versus direct `RichTextLabel` glyph path (2026-05-26)

## Goal

Continue exactly from Task 208's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 208 proved that static source alone could not choose a unique higher caller family above `TextServer::shaped_text_draw(..., p_color, ...)`,
- can the narrowest already-approved evidence distinguish whether the preserved packet comes from the shared `shaped_text_draw(..., p_color, ...)` family,
- or from a direct widget/control `font_draw_glyph(..., font_color)` family?

This slice stays narrow and evidence-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 208: provenance was valid up to `TextServer::shaped_text_draw(..., p_color, ...)`, but static source alone could not uniquely choose a higher caller family

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Decisive already-available evidence

Two previously established facts become jointly decisive when combined:

1. the preserved packet owner was already narrowed to the scene path `CanvasLayer/HudMargin/HudLabel`, and the live runtime owner path was pinned to `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel`
2. that owner node is a `RichTextLabel`, whose text draw path emits glyphs by calling `TS->font_draw_glyph(..., font_color)` directly on its own canvas item RID rather than routing through `TextServer::shaped_text_draw(..., p_color, ...)`

That means the family distinction no longer depends on broad static guesswork. The existing owner proof plus the exact owner-class draw path is enough to distinguish the caller family.

## Source-backed direct `RichTextLabel` glyph path

The exact owner note already fixed that the preserved packet belongs to the `HudLabel` `RichTextLabel` node's own canvas item route.

Inside `scene/gui/rich_text_label.cpp`, the draw path uses the label's canvas item directly:

```cpp
RID ci = get_canvas_item();
```

and emits glyphs with direct widget glyph calls:

```cpp
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, outline_size, fx_offset + char_off, gl, font_color);
```

Crucially, this route does **not** call `TextServer::shaped_text_draw(..., p_color, ...)` for the actual glyph emission step inside the RichTextLabel draw loop.

So once the packet owner is known to be the `HudLabel` `RichTextLabel`, the higher caller family for the preserved packet is source-backed:

```text
direct widget/control glyph family
= RichTextLabel -> TS->font_draw_glyph(..., font_color)
```

not:

```text
shared shaped-text family
= TextServer::shaped_text_draw(..., p_color) -> font_draw_glyph(..., p_color)
```

## Why this is enough now

Task 208 was correct that source alone could not pick one family while the packet owner was still unresolved at that rung.

But the owner seam was separately resolved by the narrow owner note:

- owner scene path: `CanvasLayer/HudMargin/HudLabel`
- runtime path: `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel`
- node type: `RichTextLabel`
- packet emitted onto that same node's own canvas item RID

Given that owner identity, there is no need for a broader contrast run just to choose between the two higher caller families. The owner class itself supplies the deciding draw route.

## Classification

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the higher caller family is **not** the shared `TextServer::shaped_text_draw(..., p_color, ...)` family.

It is the direct widget/control glyph family, specifically the `RichTextLabel` draw path:

```text
HudLabel (RichTextLabel)
-> RichTextLabel::_draw_line(...)
-> TS->font_draw_glyph(..., font_color)
-> TextServerAdvanced::_font_draw_glyph(..., p_color)
-> modulate.rgb
-> p_modulate.rgb
-> rect->modulate.rgb
```

So the preserved packet is now distinguished to the direct widget/control side of the fork.

## Conclusion

The narrowest honest evidence step is already sufficient.

Because the preserved packet owner was previously fixed to `CanvasLayer/HudMargin/HudLabel`, and that node is a `RichTextLabel` whose glyph draw path uses direct `TS->font_draw_glyph(..., font_color)` calls on its own canvas item, the preserved first-`L88` packet belongs to the **direct widget/control glyph caller family**, not the shared `TextServer::shaped_text_draw(..., p_color, ...)` family.

Best narrow wording after this slice:

> on the preserved first-`L88` no-outline MSDF lane, the higher caller-family fork is now resolved: the packet comes from the `HudLabel` `RichTextLabel` direct widget glyph path (`TS->font_draw_glyph(..., font_color)`), not from the shared `TextServer::shaped_text_draw(..., p_color, ...)` family.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is to stay on that now-fixed direct `HudLabel`/`RichTextLabel` route and classify the exact higher caller-side source of `font_color` for the preserved packet, rather than widening back out to other unrelated text caller families.
