# GDGS first-L88 heading bold-font lane: exact concrete font resource and exact default size fallback (2026-05-26)

## Goal

Continue exactly from Task 211's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once the broad heading `[b] -> RTL_BOLD_FONT -> theme_cache.bold_font + theme_cache.bold_font_size -> glyphs[i].font_rid` lane is already fixed,
- what is the exact concrete font resource that fills `theme_cache.bold_font`, and what exact numeric size does the `-1` `bold_font_size` slot fall through to on this runtime path?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact direct emission route already fixed earlier: `HudLabel` `RichTextLabel` -> `_draw_line(...)` -> `DRAW_STEP_TEXT` -> `TS->font_draw_glyph(...)`
- exact immediate-input stop point inherited from Task 211: the `frid` lane already collapses to the RichTextLabel bold theme font plus default-size path

It does **not** reopen `font_color`, caller-family identity, owner identity, shadow/outline branches, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Exact concrete font resource on this runtime path

### 1) The repro project does not supply a custom project/theme font

In the repro project, there is no authored `HudLabel` theme override, no `add_theme_font_override(...)`, no `add_theme_font_size_override(...)`, and no project-local GUI font override surfaced in the searched config/script/scene files.

So the `HudLabel` route stays on the engine default theme path rather than switching into a project-authored font resource.

### 2) Default theme construction decides the concrete base font resource

In `default_theme.cpp`, the default theme builder does:

```cpp
if (p_font.is_valid()) {
    default_font = p_font;
} else {
    Ref<FontFile> dynamic_font;
    dynamic_font.instantiate();
#ifdef BROTLI_ENABLED
    dynamic_font->set_data_ptr(_font_OpenSans_SemiBold, _font_OpenSans_SemiBold_size);
    ...
#endif
    default_font = dynamic_font;
}
```

Then, when `default_font` is valid, it constructs the bold lane as:

```cpp
bold_font.instantiate();
bold_font->set_base_font(default_font);
bold_font->set_variation_embolden(1.2);
```

So the exact concrete RichTextLabel bold-font object on this engine-default route is **not** a separate baked bold TTF chosen directly per label. It is a `FontVariation` object whose base font is the default theme's `default_font`, with `variation_embolden(1.2)` applied.

### 3) On the default built-in path, that base font is the embedded OpenSans SemiBold font file

If no custom project font is supplied into `make_default_theme(...)`, the engine instantiates a `FontFile` from:

```cpp
_font_OpenSans_SemiBold
```

via `set_data_ptr(_font_OpenSans_SemiBold, _font_OpenSans_SemiBold_size)`.

So on the stock runtime path relevant to this repro, the concrete bold-font resource behind `theme_cache.bold_font` is:

```text
FontVariation(base_font = embedded OpenSans SemiBold FontFile, variation_embolden = 1.2)
```

unless the wider runtime injected a custom `p_font`, which the repro project evidence here does not indicate.

## Exact numeric size fallback on this runtime path

### 4) RichTextLabel stores `bold_font_size = -1` in the default theme

The default RichTextLabel theme entries are:

```cpp
theme->set_font("bold_font", "RichTextLabel", bold_font);
theme->set_font_size("bold_font_size", "RichTextLabel", -1);
```

That means the named RichTextLabel bold size slot is intentionally not given a positive explicit size.

### 5) The default theme's exact default font size is `16 * scale`

Earlier in the same default theme build, the engine sets:

```cpp
static const int default_font_size = 16;
...
theme->set_default_font_size(Math::round(default_font_size * scale));
```

and the theme was initialized with:

```cpp
float default_scale = CLAMP(p_scale, 0.5, 8.0);
...
fill_default_theme(..., default_scale);
ThemeDB::get_singleton()->set_fallback_font_size(default_font_size * default_scale);
```

Theme font-size lookup then returns:

- the named font size if `> 0`
- else the theme default font size
- else the global fallback font size

So for RichTextLabel `bold_font_size = -1`, the exact size fallback on this route is:

```text
Math::round(16 * scale)
```

For the ordinary default-scale path (`scale = 1.0`), that is **16 px**.

## Exact closed lane wording

For the preserved first-L88 heading packet, the `frid`-side theme lane can now be stated more concretely as:

```text
[b]
-> _push_def_font(RTL_BOLD_FONT)
-> _find_font(...) resolves to theme_cache.bold_font + theme_cache.bold_font_size
-> theme_cache.bold_font = FontVariation(base_font = default theme font, embolden = 1.2)
-> on the built-in default path, default theme font = embedded OpenSans SemiBold FontFile
-> theme_cache.bold_font_size comes from RichTextLabel bold_font_size slot = -1
-> Theme::get_font_size(...) falls through to theme default font size = Math::round(16 * scale)
-> default-scale runtime path => 16 px
-> _shape_line(...).add_string(...)
-> glyphs[i].font_rid
```

## Conclusion

Task 211 fixed the broad theme-backed bold-font lane. This slice resolves the remaining concrete resource/size details on the same route.

For the preserved first-L88 packet:

- the concrete font object behind `theme_cache.bold_font` is a `FontVariation`
- its base font is the default theme font
- on the engine built-in default path, that base font is the embedded OpenSans SemiBold font file
- the bold variation applies `variation_embolden(1.2)`
- the RichTextLabel `bold_font_size` slot is explicitly `-1`
- that falls through to the theme default font size, which is `Math::round(16 * scale)`
- on the ordinary default-scale runtime path, the exact numeric fallback is **16 px**

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is no longer the concrete bold theme resource/size path. That is now resolved.

The next narrow seam would be to resolve the final step from this now-concrete theme lane into the shaped-glyph side itself, such as:

- the exact point where the resolved `FontVariation` + `16 px` shaping inputs become the eventual `glyphs[i].font_rid` / glyph index pair for the first visible heading `G`, and/or
- whether that last step can be closed source-only or requires the smallest reversible runtime trace

without reopening broader caller, owner, or crash theories.
