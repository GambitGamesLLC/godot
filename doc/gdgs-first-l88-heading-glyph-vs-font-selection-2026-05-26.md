# GDGS first-L88 direct `HudLabel` text branch: first heading glyph emission versus immediate `font_color` / `frid` selection (2026-05-26)

## Goal

Continue exactly from Task 209's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped white no-outline MSDF packet,
- now that Task 209 fixed the direct owner-side emission step as `RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...)`,
- what is the tightest honest next rung inside that branch:
  - the exact first heading glyph emission itself,
  - the immediate `font_color` selection,
  - or the immediate `frid` selection?

This slice stays source-backed and keeps the same preserved packet lane.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` white no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 209: the packet belongs to the direct `HudLabel` owner route and is emitted by the normal visible-text branch `DRAW_STEP_TEXT -> TS->font_draw_glyph(...)`

It does **not** reopen owner identity, shadow/outline branches, non-default-self-modulate theory, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Fixed direct branch

The already-fixed direct branch is:

```text
RichTextLabel::NOTIFICATION_DRAW
-> RichTextLabel::_draw_line(...)
-> DRAW_STEP_TEXT
-> TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)
```

So the next inward split can only happen among the immediate inputs to that live call.

## Immediate `font_color` classification

Inside `_draw_line(...)`, the text-step color is seeded by:

```cpp
Color font_color = ... ? _find_color(it, p_base_color) : Color();
```

And `_find_color(...)` is simple:

```cpp
while (item) {
	if (item->type == ITEM_COLOR) {
		return color->color;
	}
	item = item->parent;
}
return p_default_color;
```

For this reproducer path:

- `_draw_line(...)` is entered from `NOTIFICATION_DRAW` with `p_base_color = theme_cache.default_color`
- the default runtime RichTextLabel theme sets `default_color = Color(1, 1, 1)`
- the scene's `HudLabel` text starts with `[b]GDGS render-path tweak harness[/b]`
- the reproducer scene/script do **not** use `[color]`, `[fgcolor]`, `[bgcolor]`, or other color-tag overrides on that heading route

So the immediate text-step color selection for the first heading packet is source-backed as:

```text
font_color = theme_cache.default_color = Color(1, 1, 1)
```

## Immediate `frid` classification

The heading begins with BBCode bold:

```text
[b]GDGS render-path tweak harness[/b]
```

RichTextLabel parses that with:

```cpp
if (tag == "b") {
	_push_def_font(RTL_BOLD_FONT);
}
```

And font resolution later maps that default-font marker to the live theme font:

```cpp
case RTL_BOLD_FONT:
	fi->font = theme_cache.bold_font;
	if (fi->def_size) {
		fi->font_size = theme_cache.bold_font_size;
	}
```

At glyph draw time, the live font RID passed into the draw call is:

```cpp
RID frid = glyphs[i].font_rid;
```

So the tightest honest source-backed classification is:

```text
frid for the first heading packet is the shaped glyph RID produced from the heading's active bold-font selection lane,
which resolves from [b] -> RTL_BOLD_FONT -> theme_cache.bold_font (and its effective size)
```

This is tighter and safer than inventing a raw numeric RID from static source alone.

## First heading glyph emission itself

The authored heading text begins immediately after `[b]` with:

```text
G
```

So the first visible heading glyph emitted on this direct branch is source-backed as the first heading character:

```text
'G' from "GDGS render-path tweak harness"
```

However, that glyph identity is still slightly broader than the immediate draw-call input selection rung, because the preserved packet artifact by itself does not add a stricter glyph-atlas-to-character proof beyond the already-fixed owner path and heading-first draw order.

## Which split is tighter here?

For this rung, the tightest honest split is the immediate draw-call input selection, not the higher-level prose name of the first heading glyph.

More precisely:

- immediate `font_color` source identity: **white default RichTextLabel text color**
- immediate `frid` source identity: **bold-font shaped glyph RID from `[b] -> RTL_BOLD_FONT -> theme_cache.bold_font`**
- first heading glyph identity: **leading `'G'` of the heading**, but that is a slightly broader semantic label than the draw-call input provenance itself

Among those, `font_color` is the fully collapsed side on this rung because it resolves exactly to default white from source plus scene text, while `frid` remains one step less terminal because it is still the shaped RID outcome of the heading's bold-font selection lane.

## Classification

For the preserved first clipped white no-outline MSDF packet on the fixed direct `HudLabel` text branch, the next honest inward split is:

```text
DRAW_STEP_TEXT -> font_draw_glyph(...)
=> immediate inputs split into:
   - font_color = theme_cache.default_color = white
   - frid = shaped bold-font RID from [b] -> RTL_BOLD_FONT -> theme_cache.bold_font
```

The packet should therefore **not** be described only as the higher-level first heading glyph emission once this rung is available.

The tighter exact provenance is the immediate draw-call input pair above, with `font_color` already collapsed to default white and `frid` still pointing to the heading's bold-font selection lane.

## Conclusion

Inside the fixed direct `HudLabel` `DRAW_STEP_TEXT -> font_draw_glyph(...)` route, the next honest split is the immediate input provenance pair:

- `font_color`: **default white RichTextLabel text color**
- `frid`: **bold-font shaped glyph RID selected by the heading's `[b]...[/b]` lane**

The first heading glyph is still best read as the leading `'G'`, but for provenance purposes the immediate `font_color` / `frid` split is the tighter rung.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward continuation is to stay on this immediate input split and classify whether the tighter unfinished side is the bold-font `frid` lane itself — for example, resolving the exact theme font/size path behind the heading's bold selection — while keeping the already-collapsed white `font_color` side closed.
