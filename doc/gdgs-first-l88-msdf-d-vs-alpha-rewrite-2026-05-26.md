# GDGS first-L88 MSDF `d` versus final alpha rewrite note (2026-05-26)

## Goal

Stay on the exact first-L88 no-outline MSDF packet and separate the remaining live seam one step more narrowly:

- median-distance collapse itself: `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`
- versus the later alpha rewrite carrier: `color.a = a * color.a`

The task here is not to widen into a fix. It is only to classify which side is the tighter preserved carrier inside the already-locked `submit_serial=9` envelope.

## Scope lock

This note stays on the same approved seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` packet
- exact branch: the no-outline `sc_use_msdf()` rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved packet facts carried from Tasks 177–181

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broaden into a fix.

## Exact live formula already established

Prior notes already narrowed the exact packet to:

```glsl
vec4 color = color_interp;
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
vec2 msdf_size = vec2(textureSize(sampler2D(color_texture, texture_sampler), 0));
vec2 dest_size = vec2(1.0) / fwidth(uv);
float px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

And the packet-specific facts already pinned are:

- `outline_thickness = 0.0`
- `px_range = 1.0`
- source size = `14x16`
- destination size = `14x16`
- texture pixel size = `(1/256, 1/256)`

Task 181 already reduced the derivative term on this exact packet to:

```text
px_size = 1.0
```

So the exact live no-outline path simplifies to:

```text
a = clamp((d - 0.5) * 1.0 + 0.5, 0.0, 1.0)
  = clamp(d, 0.0, 1.0)
```

Because `d` is the median of three normalized sampled channels, this exact packet reduces further to:

```text
a = d
color.a = d * color.a
```

That reduction is the whole point of this slice.

## What changes at `d`

The first irreversible MSDF-specific collapse happens here:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
```

Before this line, the branch still has the raw sampled channel relationship available:

- `msdf_sample.r`
- `msdf_sample.g`
- `msdf_sample.b`

After this line, that three-channel relationship is collapsed to one scalar.

That makes `d` the narrowest place where the branch stops being “sampled RGB inputs” and becomes “one resolved coverage-driving value.”

## What changes at the final alpha rewrite

The later line is:

```glsl
color.a = a * color.a;
```

On this exact packet, since `a = d`, that line is equivalently:

```glsl
color.a = d * color.a;
```

But source-wise this line is not creating a new MSDF-specific scalar. It is only applying the already-resolved MSDF scalar to an existing fragment-alpha carrier that was present before the branch:

```glsl
vec4 color = color_interp;
```

So the final rewrite is a multiplicative handoff stage:

- **MSDF-owned input:** `d`
- **preexisting carrier:** `color.a`
- **result:** post-branch fragment alpha

That means the final rewrite is downstream of the median collapse rather than tighter than it.

## Why `d` is the tighter preserved carrier than `color.a = d * color.a`

The question is not which line is later. The question is which line is the narrower preserved MSDF-specific seam.

`d` is tighter for three source-backed reasons:

1. **`d` is the first scalar that uniquely belongs to the MSDF reinterpretation.**
   - It is produced only by the MSDF path.
   - It is the exact collapse from sampled RGB relationship into one resolved distance/coverage value.

2. **The final rewrite adds a non-MSDF multiplicative carrier.**
   - `color.a` already exists before the MSDF branch.
   - `color.a = d * color.a` therefore mixes the MSDF-owned scalar with an inherited upstream alpha lane.
   - That makes the rewrite broader than `d` itself.

3. **On this exact packet, `a` contributes no extra distinction beyond `d`.**
   - With `px_size = 1.0`, the no-outline formula reduces to `a = d`.
   - So there is no narrower live seam between `d` and `a` on this packet.
   - The only remaining distinction after that is the downstream multiplication by inherited `color.a`.

In short:

- raw channels -> `d` is the MSDF-specific collapse
- `d` -> `color.a = d * color.a` is only carrier application

That makes `d` the tighter preserved carrier.

## Why the raw sampled-channel relationship is broader than `d`

The alternative split was “raw sampled-channel relationship versus median collapse.”

That split is real, but it is broader than the `d` versus final-alpha split once Tasks 180–181 are already locked, because:

- raw sampled RGB is still the upstream input bundle
- `d` is the first resolved scalar actually used by the surviving no-outline packet after the derivative term has gone neutral
- the crash-preserving seam does not need all three raw channels as a *relationship* after the branch collapses them; it survives through the one scalar they produce

So among the two allowed comparisons, the tighter preserved carrier is:

- **`d` versus the final alpha rewrite**, with `d` winning as the narrower seam

## Classification

For the exact first-L88 no-outline MSDF packet, the tightest surviving preserved carrier is:

- **`d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`**

not:

- the broader raw sampled-channel relationship upstream of the collapse
- and not the downstream alpha rewrite `color.a = d * color.a`, which only applies `d` through the inherited `color.a` carrier

## Conclusion

On this exact packet, Task 181 already reduced the no-outline branch to `a = d`. That means the last remaining honest separation is:

- `d` = the MSDF-owned scalar collapse
- `color.a = d * color.a` = downstream application of that scalar to an existing fragment-alpha carrier

So the tighter source-backed preserved seam at failing `submit_serial=9` is the **median-distance scalar `d` itself**. The final alpha rewrite remains part of the same packet, but it is a broader carrier application stage rather than the narrower preserved MSDF-specific ingredient.
