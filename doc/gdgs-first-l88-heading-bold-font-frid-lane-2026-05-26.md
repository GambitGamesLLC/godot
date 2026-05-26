# GDGS first-L88 heading bold-font `frid` lane behind the direct RichTextLabel `font_draw_glyph(...)` packet (2026-05-26)

## Goal

Continue exactly from Task 210's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once the immediate draw-call inputs were already split to `font_color = white` plus an unresolved bold-font `frid` lane,
- what is the exact source-backed theme/font/size resolution path behind that heading `[b]` selection?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact owner/emission route already fixed earlier: direct `HudLabel` `RichTextLabel` path, `RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(..., font_color)`
- exact immediate-input stop point inherited from Task 210: `font_color = white`, while the `frid` side still needs classification

It does **not** reopen owner identity, shadow/outline branches, non-default-self-modulate theory, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed `frid` lane

### 1) The heading `[b]` tag pushes the default bold-font lane

When `RichTextLabel` parses `[b]`, it does:

```cpp
_push_def_font(RTL_BOLD_FONT);
```

unless italics is already active, in which case it would choose `RTL_BOLD_ITALICS_FONT` instead. On the preserved heading route, the opening content is simply:

```text
[b]GDGS render-path tweak harness[/b]
```

so the first visible heading text enters the plain bold lane, not the bold-italics lane.

`_push_def_font(RTL_BOLD_FONT)` creates an `ItemFont` with:

- `def_font = RTL_BOLD_FONT`
- `def_size = true`

and no custom project font override is authored on the repro `HudLabel` path.

### 2) `_find_font(...)` resolves that bold lane from the RichTextLabel theme cache

Later, when the line is shaped or drawn, `_find_font(...)` walks up to the nearest `ITEM_FONT` and resolves default-font placeholders against `theme_cache`.

For `RTL_BOLD_FONT`, the exact branch is:

```cpp
fi->font = theme_cache.bold_font;
if (fi->def_size) {
    fi->font_size = theme_cache.bold_font_size;
}
```

So the still-open `frid` lane from Task 210 is not an arbitrary hidden font choice. It collapses to the RichTextLabel theme's **bold font object plus bold font size**.

### 3) The theme cache entries are bound to RichTextLabel theme items

`RichTextLabel` binds those cache fields directly as theme items:

```cpp
BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, bold_font);
BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, bold_font_size);
```

That means the heading's `[b]` selection resolves through the standard Control theme lookup path for the `RichTextLabel` type, not through a special GDGS-only font lane.

### 4) Default theme values on this route

In the default theme setup for `RichTextLabel`:

```cpp
theme->set_font("bold_font", "RichTextLabel", bold_font);
theme->set_font_size("bold_font_size", "RichTextLabel", -1);
```

Separately, the default theme sets the theme-wide defaults:

```cpp
theme->set_default_font(default_font);
theme->set_default_font_size(Math::round(default_font_size * scale));
```

and theme lookup for font sizes returns:

- the named font size if it is `> 0`
- otherwise the theme's `default_font_size`
- otherwise the global fallback font size

So on the preserved heading route, the bold size lane is source-backed as:

```text
theme_cache.bold_font_size
-> RichTextLabel theme item "bold_font_size"
-> default theme stores -1 there
-> Theme::get_font_size(...) falls through to the theme default font size
```

while the font object lane is:

```text
theme_cache.bold_font
-> RichTextLabel theme item "bold_font"
-> default RichTextLabel bold_font slot in the theme
```

### 5) The repro scene does not override that lane

The authored repro scene/script set up `HudLabel` with BBCode text and layout behavior, but do **not** author any theme/font/font-size overrides for the label:

- no `theme_override_*` properties in the `.tscn`
- no `add_theme_font_override(...)`
- no `add_theme_font_size_override(...)`
- no custom `theme = ...` assignment on the `HudLabel`

So this lane stays on the stock RichTextLabel theme resolution path.

### 6) `_shape_line(...)` feeds that resolved font/size into shaping, which later becomes `glyphs[i].font_rid`

In the `ITEM_TEXT` path, `_shape_line(...)` does:

- start from base `theme_cache.normal_font` / `theme_cache.normal_font_size`
- replace them with `_find_font(it)` / `_find_font_size(it)` results when present
- call:

```cpp
l.text_buf->add_string(tx, font, font_size, lang, it->rid);
```

Then the draw step later reads shaped glyph data from the paragraph buffer and uses:

```cpp
RID frid = glyphs[i].font_rid;
```

before issuing `TS->font_draw_glyph(...)`.

So the exact `frid` provenance on this route is:

```text
[b]
-> _push_def_font(RTL_BOLD_FONT)
-> ItemFont{def_font = RTL_BOLD_FONT, def_size = true}
-> _find_font(...) resolves to theme_cache.bold_font + theme_cache.bold_font_size
-> _shape_line(...).add_string(tx, font, font_size, ...)
-> shaped glyph buffer
-> glyphs[i].font_rid
-> TS->font_draw_glyph(...)
```

## Classification

For the preserved first-`Command Graph (L88)` direct `HudLabel` heading packet, the bold-font `frid` lane is best classified as:

```text
heading [b]
-> RTL_BOLD_FONT default-font tag
-> RichTextLabel theme_cache.bold_font
-> RichTextLabel theme_cache.bold_font_size
   (with default-theme -1 on the named bold slot falling through to the theme default font size)
-> _shape_line(...).add_string(...)
-> glyphs[i].font_rid
```

More narrowly:

- the font object side resolves through the RichTextLabel `bold_font` theme item
- the size side resolves through the RichTextLabel `bold_font_size` theme item, which defaults to fallback-to-theme-default size on this route
- the repro scene does not override either side

## Conclusion

Task 210 left the direct `font_draw_glyph(...)` packet split into a closed white `font_color` lane and an open bold-font `frid` lane. This slice closes that remaining lane to a specific theme-backed path.

For the preserved first-L88 packet, the `frid` side is not an unspecified internal font choice. It comes from the heading's `[b]` tag selecting `RTL_BOLD_FONT`, which `_find_font(...)` resolves to `theme_cache.bold_font` plus `theme_cache.bold_font_size`, with the default RichTextLabel bold size slot falling through to the theme default font size because the default theme stores `-1` there and the repro scene does not override it.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is no longer the broad bold-font theme path. That is now resolved.

The next narrow seam would be to split one rung deeper inside that closed theme-backed lane, such as:

- the exact concrete font resource / fallback font object that populates `theme_cache.bold_font` on this runtime path, and/or
- the exact numeric theme-default font size that the `-1` bold size slot falls through to on the preserved repro runtime

without reopening `font_color`, caller-family identity, or broader crash theories.
