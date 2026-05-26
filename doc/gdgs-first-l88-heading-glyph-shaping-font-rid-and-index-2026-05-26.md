# GDGS first-L88 heading glyph shaping: exact `font_rid` / glyph-index pair for the first visible heading `G` (2026-05-26)

## Goal

Continue exactly from Task 212's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once the direct `HudLabel` heading font lane is already fixed to the concrete bold default-theme resource at `16 px`,
- what is the exact final shaped-glyph transition from those inputs to the eventual `glyphs[i].font_rid` / glyph index pair for the first visible heading `G`?

This slice stays narrow and uses the smallest reversible runtime trace only where source alone stops being exact.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet family already fixed earlier: direct `HudLabel` `RichTextLabel` `DRAW_STEP_TEXT -> TS->font_draw_glyph(...)`
- exact shaping inputs already fixed by Task 212: `theme_cache.bold_font = FontVariation(base_font = default theme font, embolden = 1.2)` and `theme_cache.bold_font_size = 16 px` on the ordinary default-scale route
- exact target of this slice: the first visible heading glyph `G`

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Where source alone stops

Source already fixed the path up to shaping:

```text
[b]
-> _push_def_font(RTL_BOLD_FONT)
-> _find_font(...) resolves to theme_cache.bold_font + theme_cache.bold_font_size
-> _shape_line(...).add_string(tx, font, font_size, lang, it->rid)
-> shaped glyph buffer
-> glyphs[i].font_rid / glyphs[i].index
-> TS->font_draw_glyph(...)
```

But source alone does **not** supply the exact runtime numeric `RID(...)` value or the actual glyph index for the first visible `G`. That is the honest stop point for static source.

So this slice uses the smallest reversible runtime trace: shape the heading body with the actual `HudLabel` theme font and size, inspect the first shaped glyph dictionary, and immediately stop.

## Minimal runtime trace

A temporary headless probe script loaded the repro scene, fetched the direct `HudLabel` RichTextLabel theme font/size, shaped the heading body `"GDGS render-path tweak harness"`, and printed the first shaped glyph entry.

Relevant probe facts from both available runtimes:

### Managed runtime (`~/.local/bin/godot`, 4.6.2.stable)

- `probe_font_class=FontVariation`
- `probe_font_rids=[RID(450971566081)]`
- `probe_size=16`
- `probe_first_glyph={ ..., "font_rid": RID(450971566081), "font_size": 16, "index": 42 }`
- direct check: `font_get_glyph_index(first["font_rid"], first["font_size"], 'G', 0) == 42`

### Preserved repro runtime (`4.7.dev5.official.a8643700c`)

- `probe_font_class=FontVariation`
- `probe_font_rids=[RID(687194767361)]`
- `probe_size=16`
- `probe_first_glyph={ ..., "font_rid": RID(687194767361), "font_size": 16, "index": 42, "span_index": 0 }`
- direct check: `font_get_glyph_index(first["font_rid"], first["font_size"], 'G', 0) == 42`

The numeric RID differs across runtimes, which is expected. The important preserved-lane fact is that on the actual preserved repro runtime, the first heading glyph shapes to:

```text
font_rid = RID(687194767361)
font_size = 16
glyph index = 42
```

with the first shaped glyph covering `start=0`, `end=1`, i.e. the opening `G`.

## Classification

For the preserved first-L88 direct `HudLabel` heading route, the final shaped-glyph step is best classified as:

```text
concrete bold theme inputs
= FontVariation(base_font = default theme font, embolden = 1.2) at 16 px
-> TextServer shaping of heading body "GDGS render-path tweak harness"
-> first shaped glyph (start 0, end 1)
-> preserved repro runtime pair: font_rid RID(687194767361), glyph index 42
-> TS->font_draw_glyph(...)
```

More narrowly:

- the first visible heading glyph is the opening `G`
- its shaped glyph index is `42`
- the preserved runtime's shaped glyph uses the same first RID exposed by `font.get_rids()` on that route
- the RID value itself is runtime-instance-specific, but the preserved-runtime probe pins the exact value for this session's approved lane

## Conclusion

Task 212 fixed the concrete theme resource and size. This slice closes the final shaping handoff needed to name the first heading glyph's actual draw inputs.

For the preserved first-L88 packet lane on the preserved repro runtime:

- `font_class = FontVariation`
- `font_size = 16`
- first visible heading glyph = `G`
- exact first shaped glyph pair = `font_rid RID(687194767361)`, `glyph index 42`

That is the narrowest honest closure of the shaping side without widening back into broader crash theories.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest seam is no longer the shaping pair itself. That is now resolved.

The next narrow seam would be to decide whether the first preserved clipped MSDF rect packet can be pinned directly to this exact shaped `G` emission on the live preserved packet path without further ambiguity, or whether one last minimal runtime trace is still needed to bridge from the shaped first-glyph pair to the first clipped packet selection itself.
