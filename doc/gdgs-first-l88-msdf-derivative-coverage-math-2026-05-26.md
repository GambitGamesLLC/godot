# GDGS first-L88 MSDF derivative / coverage / outline math note (2026-05-26)

## Goal

Inspect the exact derivative / coverage / outline math inside the first-L88 `sc_use_msdf()` fragment branch and classify, as narrowly as possible, why that branch still preserves the locked crash envelope at failing `submit_serial=9`.

## Scope lock

This note stays on the same already-approved seam only:

- exact packet: the first clipped preserve-rect L88 packet already isolated by prior tasks
- exact branch: `if (sc_use_msdf())` in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact contrast: baseline first-L88 packet versus the approved value-only specialization flip from Task 177

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, or CPU-side vertex-format-cache theory.

## Exact packet facts reused from approved artifacts

From the already-approved first-kept clipped preserve-rect trace and value-only specialization artifacts:

- first kept batch payload on the exact L88 packet:
  - `uses_prior_color=true`
  - `use_msdf=false`
  - `use_lcd=false`
  - `outline=0.000000`
  - `px_range=1.000000`
  - `texpixel_size={x=0.003906,y=0.003906}`
  - rect payload is small and mundane: destination `14x16`, source `14x16`
- first-kept prereq state stayed in the already-known shell:
  - same `pre_l88_draw` scissor reestablishment package
  - same batch texture uniform-set role
  - same instanced/indexed draw bindings
- approved Task 177 value-only specialization experiment proved:
  - `baseline_packed_0=0x0`
  - `experiment_packed_0=0x2`
  - `held_use_msdf=false`
  - outer crash identity stayed locked: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

So this slice is only about the math *inside* the specialized fragment branch.

## Source location

The exact branch is in `servers/rendering/renderer_rd/shaders/canvas.glsl`:

```glsl
if (sc_use_msdf()) {
	float px_range = params.msdf.x;
	float outline_thickness = params.msdf.y;

	vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
	vec2 msdf_size = vec2(textureSize(sampler2D(color_texture, texture_sampler), 0));
	vec2 dest_size = vec2(1.0) / fwidth(uv);
	float px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0);
	float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);

	if (outline_thickness > 0) {
		float cr = clamp(outline_thickness, 0.0, (px_range / 2.0) - 1.0) / px_range;
		d = min(d, msdf_sample.a);
		float a = clamp((d - 0.5 + cr) * px_size, 0.0, 1.0);
		color.a = a * color.a;
	} else {
		float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
		color.a = a * color.a;
	}
}
```

The non-MSDF contrast on the same packet is still just:

```glsl
color *= texture(sampler2D(color_texture, texture_sampler), uv);
```

## What is live on the exact first-L88 packet

For this exact packet, the prior artifact evidence narrows the live inputs to:

- `px_range = 1.0`
- `outline_thickness = 0.0`
- `read_draw_data_color_texture_pixel_size = (1/256, 1/256)` from `0.003906`
- therefore `msdf_size` is the same texture footprint family, i.e. effectively `256 x 256` for this packet

That means the exact first-L88 MSDF branch does **not** execute the outline sub-branch. The live branch reduces to:

1. sample the same texture at the same `uv`
2. compute destination pixel footprint from derivatives:
   - `dest_size = 1 / fwidth(uv)`
3. convert the configured MSDF pixel range into a derivative-scaled coverage gain:
   - `px_size = max(0.5 * dot((1/256, 1/256), dest_size), 1.0)`
4. reconstruct the signed-distance proxy from sampled RGB:
   - `d = median(r, g, b)`
5. convert that distance proxy into coverage alpha:
   - `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`
6. modulate only fragment alpha:
   - `color.a = a * color.a`

So the exact live first-L88 specialized math is **derivative-scaled coverage only**. The optional outline math is present in source but dormant on this packet because `outline_thickness == 0.0`.

## Exact classification of each math stage

### 1) Derivative stage

`dest_size = 1 / fwidth(uv)` turns screen-space UV derivatives into an estimate of how large the textured shape is in destination-pixel terms.

This is the only place in the exact branch where fragment derivatives are introduced.

### 2) Coverage scale stage

`px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0)`

On this packet, `px_range=1.0`, so this becomes a texture-size-normalized coverage scale. In plain English, it says: use the destination footprint and the MSDF atlas size to decide how sharply the median-distance value should transition around the 0.5 cutoff, but never let that gain fall below `1.0`.

### 3) Distance reconstruction stage

`d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`

This reinterprets sampled RGB as MSDF distance carriers instead of display color. That is the semantic fork from the non-MSDF path.

### 4) Outline stage

The source contains an outline-capable sub-branch, but for the exact first-L88 packet it is inactive:

- `outline_thickness > 0` is false
- `cr = clamp(...) / px_range` is not evaluated live
- `d = min(d, msdf_sample.a)` is not evaluated live
- `a = clamp((d - 0.5 + cr) * px_size, 0.0, 1.0)` is not the live formula here

So outline support exists in source, but it is **not** part of the live first-L88 preserved-crash classification.

### 5) Final alpha rewrite stage

The live packet uses:

- `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`
- `color.a = a * color.a`

That means the specialized path rewrites fragment opacity from MSDF coverage, while the non-MSDF path would have multiplied full RGBA by the sampled texel directly.

## Narrow contrast versus the non-MSDF path

For this exact first-L88 packet:

- both paths sample the same `color_texture`
- both paths use the same `texture_sampler`
- both paths sample at the same `uv`
- both paths stay inside the same rect fragment packet and later rejoin the same downstream flow

What changes is only the interpretation and ALU after sampling:

- non-MSDF path:
  - sampled texel is treated as ordinary RGBA
  - full `color` is modulated directly
- exact live MSDF path:
  - sampled RGB is treated as distance data
  - derivative-scaled coverage alpha is synthesized
  - only `color.a` is rewritten by that synthesized coverage
  - outline support exists in source but is inactive on this packet

## Why this still preserves the locked crash envelope

The narrowest honest classification is:

1. The exact first-L88 preserved-crash branch does **not** widen into a broader packet change.
2. On this packet, the live MSDF fork is only:
   - same sample
   - plus derivative math (`fwidth`)
   - plus median-distance reconstruction
   - plus alpha-only coverage rewrite
3. The optional outline path is dormant because `outline=0.0`, so the preserved envelope does **not** require outline-specific behavior.
4. The surrounding first-L88 shell already stayed fixed in the approved artifacts:
   - same packet/lane
   - same prereq scissor/uniform-set/draw-binding shell
   - same vertex-input recipe family
   - same indexed draw shape
   - same outer `submit_serial=9` failure identity

So the most specific source-backed classification is:

- the MSDF branch preserves the same locked crash envelope because, on the exact first-L88 packet, it only swaps in a **local fragment derivative/coverage computation** inside the same already-locked packet shell
- and within that swap, the **live** preserved path is the no-outline coverage formula, not the outline sub-branch

## Conclusion

On the exact first-L88 packet, the live MSDF-specialized fragment math is:

- same texture sample at the same `uv`
- derivative-derived destination footprint via `fwidth(uv)`
- texture-size-normalized coverage gain with `px_range=1.0`
- median RGB distance reconstruction
- no-outline alpha formula `clamp((d - 0.5) * px_size + 0.5, 0, 1)`
- final `color.a = a * color.a`

That is the exact derivative / coverage / outline classification for the preserved-crash seam. The outline-capable path exists in source but is dormant on this packet, and the live branch still preserves the locked `submit_serial=9` envelope because it remains a contained fragment-ALU swap inside the same already-fixed first-L88 packet shell.