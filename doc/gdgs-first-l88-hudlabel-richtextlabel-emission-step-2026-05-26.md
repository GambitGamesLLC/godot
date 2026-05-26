# GDGS first-L88 direct `HudLabel` owner path: exact RichTextLabel glyph-emission step (2026-05-26)

## Goal

Continue exactly from Task 208's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline rect packet,
- now that Task 208 proved the packet belongs directly to the `HudLabel` `RichTextLabel` canvas item,
- which exact RichTextLabel text/glyph emission step inside that direct owner path produces the packet?

This slice stays source-backed and uses no new runtime instrumentation.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect packet on the first `Command Graph (L88)` lane
- exact owner fixed by Task 208: direct `HudLabel` `RichTextLabel` canvas item, not a separate internal canvas-item owner
- exact packet-local facts kept only as context: white modulation (`modulate={1,1,1,1}`), `outline=0`, clipped rect, later `submit_serial=9` / `fence_wait_error` / `BLIT_PASS`

It does **not** reopen owner identity, non-default-self-modulate theory, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed draw order inside `RichTextLabel`

`RichTextLabel::NOTIFICATION_DRAW` draws visible lines in order by calling:

```cpp
_draw_line(main, from_line, ofs, text_rect.size.x, vsep, theme_cache.default_color, ...)
```

So once Task 208 fixed the packet owner to `HudLabel.get_canvas_item()`, the next inward seam lives inside `_draw_line(...)`.

Inside `_draw_line(...)`, the actual shaped glyphs are emitted by an explicit per-line, per-step, per-glyph loop:

```cpp
const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
...
for (int step = DRAW_STEP_BACKGROUND; step < DRAW_STEP_MAX; step++) {
    ...
    for (int i = 0; i < gl_size; i++) {
        ...
        if (step == DRAW_STEP_TEXT) {
            TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
        } else if (step == DRAW_STEP_OUTLINE) {
            TS->font_draw_glyph_outline(...);
        } else if (step == DRAW_STEP_SHADOW) {
            TS->font_draw_glyph(... + p_shadow_ofs, ...);
        } else if (step == DRAW_STEP_SHADOW_OUTLINE) {
            TS->font_draw_glyph_outline(... + p_shadow_ofs, ...);
        }
    }
}
```

That means the direct `HudLabel` packet is **not** coming from `TextServer::shaped_text_draw(...)` on this route. Once the owner is fixed to the RichTextLabel path, the exact emission surface is the direct `_draw_line(...)` glyph loop.

## Which step matches the preserved packet facts

The preserved packet facts already constrain the step tightly:

- packet is a **glyph rect**, not a background/foreground selection box
- packet is **no-outline** (`outline=0`), so it is not the outline or shadow-outline branch
- packet is **white**, matching the ordinary text-color lane rather than a special shadow tint
- packet belongs to the direct `HudLabel` owner path, so it must be one of the direct `ci` glyph draws inside `_draw_line(...)`

Inside the RichTextLabel draw-step enum, that leaves the exact matching branch as:

```cpp
DRAW_STEP_TEXT
```

and the exact emission call as:

```cpp
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
```

not:

- `DRAW_STEP_BACKGROUND`
- `DRAW_STEP_FOREGROUND`
- `DRAW_STEP_SHADOW_OUTLINE`
- `DRAW_STEP_SHADOW`
- `DRAW_STEP_OUTLINE`
- `TS->font_draw_glyph_outline(...)`
- a separate `TextServer::shaped_text_draw(...)` helper on this owner-fixed route

## Why the first packet is on the main text body, not a prefix helper

`RichTextLabel` can draw a `text_prefix` for list rows, but that is a special earlier branch:

```cpp
if (l.text_prefix.is_valid() && line == 0 && !skip_prefix) {
    ...
    l.text_prefix->draw(...)
}
```

and prefixes are only synthesized for list rows by `_add_list_prefixes(...)`.

The reproducer's opening `HudLabel` content is not a list prefix line. It starts with the heading:

```text
[b]GDGS render-path tweak harness[/b]
```

followed later by the `Controls` bullet lines.

So the first direct-owner text packet on this path is best classified as coming from the **main shaped text body of line 0**, not from a list-prefix helper lane.

## One rung deeper: which visible text span this is

The opening visible text in the authored `HudLabel` content is:

```text
[b]GDGS render-path tweak harness[/b]
```

So the first direct-owner main-body glyph packet on this route is best classified as the first visible glyph emission from that opening bold heading span — i.e. the first glyph of the heading body after BBCode parsing, not a later bullet/control line.

This slice does **not** need a new runtime contrast to identify that rung, because the source draw order is already deterministic here:

1. `NOTIFICATION_DRAW` starts from the first visible line.
2. `_draw_line(...)` processes that line in step order.
3. non-text steps that do not match the preserved packet facts are excluded.
4. the opening heading is the first authored visible text body in the scene.

## Classification

For the preserved first clipped white no-outline rect packet inside the direct `HudLabel` owner path, the exact RichTextLabel text/glyph emission step is best classified as:

```text
RichTextLabel::NOTIFICATION_DRAW
-> RichTextLabel::_draw_line(...)
-> main line-0 shaped text body
-> DRAW_STEP_TEXT
-> direct per-glyph call TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)
```

More narrowly:

- owner path: direct `HudLabel` canvas item
- emission surface: RichTextLabel direct glyph loop, not `TextServer::shaped_text_draw(...)`
- step: `DRAW_STEP_TEXT`
- packet type: no-outline ordinary glyph draw
- first authored body on this route: the opening bold heading `[b]GDGS render-path tweak harness[/b]`

## Conclusion

Task 208 fixed **who owns** the packet. This slice fixes **which exact RichTextLabel emission step** produces it.

The first clipped white no-outline packet inside the direct `HudLabel` owner path is not from a hidden internal owner, not from a list-prefix helper, and not from an outline/shadow/background lane. It is the first matching ordinary text-pass glyph emission on the `HudLabel` RichTextLabel route:

```text
_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...)
```

for the opening line-0 heading body.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is no longer owner identity or the broad RichTextLabel step class. The next narrow seam would be to classify the first matching **line-0 / heading-span glyph packet** one rung deeper — for example, whether the first preserved packet should be pinned to the exact opening heading glyph emission itself (the first visible heading glyph after BBCode parsing) and/or to the immediate caller-side `font_color` / `frid` selection that that first `DRAW_STEP_TEXT` emission receives.
