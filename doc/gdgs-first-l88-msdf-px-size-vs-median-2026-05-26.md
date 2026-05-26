# GDGS first-L88 MSDF `px_size` versus median-distance `d` note (2026-05-26)

## Goal

Separate the two still-live MSDF-specific ingredients inside the exact first-L88 no-outline branch and classify, as narrowly as possible, which ingredient is the one that still preserves the locked crash envelope at failing `submit_serial=9`.

The two ingredients are:

- derivative-driven coverage scale: `px_size`
- median-distance reconstruction: `d = median(r, g, b)`

## Scope lock

This note stays on the already-approved seam only:

- exact packet: the first clipped preserve-rect L88 packet
- exact branch: the no-outline `sc_use_msdf()` path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact approved comparison surface: the Task 177 value-only `packed_0 0x0 -> 0x2` preservation run plus the first-kept batch/prereq packet facts already recorded in prior notes

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, or CPU-side vertex-format-cache theory.

## Exact live formula on this packet

Prior Task 180 already reduced the live branch on this packet to:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
vec2 msdf_size = vec2(textureSize(sampler2D(color_texture, texture_sampler), 0));
vec2 dest_size = vec2(1.0) / fwidth(uv);
float px_size = max(0.5 * dot((vec2(1.0) / msdf_size), dest_size), 1.0);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

The same approved packet facts also pin:

- `px_range = 1.0`
- `outline = 0.0`
- `texpixel_size = (1/256, 1/256)`
- source rect size = `14x16`
- destination rect size = `14x16`

So the only remaining split is between the derivative-driven gain term and the median-distance reconstruction.

## Ingredient 1: derivative-driven `px_size`

Source meaning:

- `dest_size = 1 / fwidth(uv)` converts UV derivatives into destination-pixel footprint
- `px_size` then rescales the distance transition width using `px_range` and the MSDF texture size

For this exact packet, the already-approved geometry is unusually specific:

- the sampled source footprint is `14x16`
- the destination footprint is also `14x16`
- the texture pixel size is `1/256` in both axes

That means the nominal UV advance per destination pixel is exactly one texel in each axis:

- `fwidth(uv.x) ≈ 1/256`
- `fwidth(uv.y) ≈ 1/256`
- therefore `dest_size ≈ (256, 256)`

Substituting the exact packet values into the shader expression gives:

```text
px_size
= max(0.5 * dot((1/256, 1/256), (256, 256)), 1.0)
= max(0.5 * (1 + 1), 1.0)
= max(1.0, 1.0)
= 1.0
```

So on this exact first-L88 packet, the derivative-driven term algebraically collapses to the floor value `1.0` rather than introducing a stronger scaling fork.

In practical terms, that means `px_size` is neutral here: it does **not** sharpen or widen the transition relative to the base no-outline formula.

## Ingredient 2: median-distance reconstruction `d`

Source meaning:

```glsl
d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)
```

This is the step where the sampled RGB channels stop being ordinary display color and become a signed-distance proxy.

That is the real semantic fork from the non-MSDF path:

- baseline path: sampled texel RGBA directly modulates fragment RGBA
- exact no-outline MSDF path: sampled RGB is reinterpreted as a distance field, then converted into alpha coverage

Once `px_size` collapses to `1.0`, the live formula simplifies further to:

```text
a = clamp((d - 0.5) * 1.0 + 0.5, 0.0, 1.0)
  = clamp(d, 0.0, 1.0)
```

Because the median of three sampled normalized texture channels is itself already within `[0, 1]`, this reduces to:

```text
a = d
```

So on the exact first-L88 packet, the surviving no-outline MSDF branch is effectively:

- same texture sample
- neutral derivative scale (`px_size = 1.0`)
- alpha rewritten directly from the median-distance reconstruction

## Classification: which ingredient preserves the locked crash envelope?

The narrower preserved ingredient is the **median-distance reconstruction `d`**, not the derivative-driven `px_size` term.

Why:

1. The approved Task 177 value-only run already proved that enabling the exact `sc_use_msdf()` branch on this packet preserves the same outer crash identity:
   - `submit_serial=9`
   - `fence_wait_error submit_serial=9`
   - `Tonemap (L87)`
   - `Command Graph (L88)`
   - later `BLIT_PASS`
   - exit `-6`
2. Task 180 already narrowed that preserved branch to the no-outline formula only.
3. The packet geometry and texel-size facts reduce the derivative-driven `px_size` term to the neutral floor value `1.0` on this exact packet.
4. That leaves the median-distance reconstruction as the surviving live MSDF-specific semantic fork inside the already-locked packet shell.

So the best source-backed classification is:

- `px_size` is present in source, but on this packet it collapses to a neutral scale and is therefore demoted as the preserved seam carrier
- `d = median(r, g, b)` is the ingredient that still carries the preserved crash envelope, because it is the remaining live reinterpretation of the sampled texel that directly rewrites fragment alpha within the same packet

## Why this stays inside the same seam

This conclusion is still narrow and non-causal:

- it does **not** claim the median function is the root cause of the GPU loss
- it only classifies which remaining live MSDF ingredient still survives after the derivative term is reduced by the exact packet geometry
- it stays inside the same already-locked first-L88 packet and the same approved outer failure envelope

## Conclusion

On the exact first-L88 no-outline MSDF packet, the derivative-driven term does **not** survive as the tighter preserved seam because the packet's `14x16 -> 14x16` one-texel-per-pixel mapping and `1/256` texel size reduce `px_size` to `1.0`.

That leaves the median-distance reconstruction as the remaining live MSDF-specific ingredient:

- `px_size` is source-present but neutral on this packet
- `d = median(r, g, b)` remains the active semantic fork
- therefore the preserved locked crash envelope at failing `submit_serial=9` tracks the median-distance reconstruction more tightly than the derivative-driven scale term
