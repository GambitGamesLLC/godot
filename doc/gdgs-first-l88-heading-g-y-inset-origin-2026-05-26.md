# GDGS first-L88 heading `G` packet: source-backed origin of the local `y=3` inset (2026-05-26)

## Goal

Continue exactly from Task 216's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF rect packet,
- now that the `1 px` left overhang is already fixed as glyph-local,
- what exact source-backed draw-anchor term produces the packet's remaining local `y=3` inset on the same first-heading-`G` route?

This slice stays on the same preserved route and uses source-backed tracing plus the smallest reversible headless probe only where source alone stops short of the exact runtime metrics.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact direct owner route already fixed: `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...) -> single draw_msdf_rect_region(...)`
- exact packet-local rect already fixed: `(-1, 3, 14, 16)`
- exact shaped/runtime pair already fixed: `font_rid RID(687194767361)`, `font_size 16`, `glyph index 42`

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## The source-backed draw chain

On the direct `HudLabel` route, `RichTextLabel::_draw_line(...)` builds the glyph draw anchor from the line offset plus the line ascent before calling `TS->font_draw_glyph(...)`:

```cpp
Vector2 off;
...
if (line > 0) {
	off.y += (theme_cache.line_separation + p_vsep);
}
...
double l_ascent = TS->shaped_text_get_ascent(rid);
...
off.y += l_ascent;
...
char_xform.set_origin(p_ofs + off_step);
...
TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
```

For the first visible heading glyph on the first line of this route:

- `off` starts at `(0,0)`
- `line == 0`, so there is no prior line-separation contribution
- the preserved runtime probe reports `label.position = (0,0)` and `label.get_line_offset(0) = 0`
- the same source path shows no extra baseline shift is inserted before `font_draw_glyph(...)`

So the vertical draw anchor for this first packet reduces to the line-ascent term itself:

```text
p_pos.y = shaped_text_get_ascent(first line RID)
```

## What the exact preserved runtime reports

A tiny reversible headless probe on the preserved repro route reported:

```text
label.position = (0,0)
label.get_line_offset(0) = 0
font_rid = RID(687194767361)
font_size = 16
font_msdf_size = 48
font_baseline_offset = 0.0
font_ascent = 18.0
font_descent = 5.0
font_spacing_top = 0
font_spacing_bottom = 0
shaped_text_get_ascent(...) = 18.0
first shaped glyph offset = (0,0)
font_get_glyph_offset(...) = (-1.0, -15.0)
font_get_glyph_size(...) = (14.0, 16.0)
```

That means the exact local packet y comes from:

```text
packet local y = shaped_text_get_ascent(first line) + font_get_glyph_offset(...).y
               = 18 + (-15)
               = 3
```

So the baseline-side draw anchor behind the packet's local `y=3` inset is **18**, not a separate hidden clip artifact.

## Where the `18` itself comes from in source

For horizontal shaped text, `TextServerAdvanced::_shaped_text_get_ascent(...)` returns:

```cpp
return sd->ascent + sd->extra_spacing[SPACING_TOP];
```

and the same probe showed:

- `font_spacing_top = 0`
- `font_baseline_offset = 0.0`
- `shaped_text_get_ascent(...) = font_ascent = 18.0`

So on this exact route the anchor is not being raised by extra top spacing or baseline offset. The first-line draw anchor is simply the shaped line ascent, and on the preserved runtime that ascent is `18`.

## Stricter source-only tie between `font_get_glyph_offset(...)` and `fgl.rect.position`

The MSDF path gives a tighter source-only equivalence than Task 216 needed.

`TextServerAdvanced::_font_get_glyph_offset(...)` returns:

```cpp
return fgl.rect.position * (double)p_size.x / (double)fd->msdf_source_size;
```

and `TextServerAdvanced::_font_draw_glyph(...)` uses that same cache field in the emitted packet position:

```cpp
Point2 cpos = p_pos;
cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
draw_msdf_rect_region(..., Rect2(cpos, csize), ...)
```

So on this exact MSDF route, the queried `font_get_glyph_offset(...)` value is not merely analogous to the internal packet-local glyph term — it is the same scaled `fgl.rect.position` contribution that `_font_draw_glyph(...)` adds into `cpos`.

That makes the full local packet-y decomposition source-backed and exact:

```text
packet local y
= RichTextLabel first-line shaped ascent
+ TextServerAdvanced MSDF glyph rect-position y term
= 18 + (-15)
= 3
```

## Classification

For the preserved first-`Command Graph (L88)` heading `G` packet, the exact source-backed origin of the local `y=3` inset is best classified as:

```text
first-line RichTextLabel baseline anchor from shaped line ascent (18)
plus the MSDF glyph rect-position / glyph-offset y term (-15)
```

More explicitly:

```text
first shaped glyph layout offset = (0,0)
line-0 extra vertical offset     = 0
shaped line ascent               = 18
font baseline offset             = 0
font spacing top                 = 0
glyph-local MSDF y term          = -15
packet local y                   = 3
```

## Conclusion

Task 216 fixed that the left overhang is glyph-local. This slice fixes the adjacent vertical rung.

On the preserved first-heading-`G` route, the packet's local `y=3` inset comes from the normal first-line `RichTextLabel` baseline anchor produced by `shaped_text_get_ascent(...)`, with no extra line-offset/top-spacing/baseline-offset contribution on this route, plus the same MSDF glyph rect-position term exposed by `font_get_glyph_offset(...)`. Concretely, the exact decomposition is:

```text
18 + (-15) = 3
```

So the remaining geometry is now source-backed and closed on this lane; there is no need to invent a broader provenance theory just to explain the inset.
