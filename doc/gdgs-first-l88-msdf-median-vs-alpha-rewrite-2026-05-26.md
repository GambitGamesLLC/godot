# GDGS first-L88 MSDF median-distance versus final alpha rewrite note (2026-05-26)

## Goal

Take the already-surviving first-L88 no-outline MSDF seam one rung narrower and classify whether the tighter preserved carrier is:

- the median-distance reconstruction itself (`d = median(r, g, b)`), or
- the final alpha application step (`color.a = a * color.a`)

This note stays source-backed and packet-local.

## Scope lock

This slice stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect L88 packet
- exact branch: the no-outline `sc_use_msdf()` rect branch in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved packet facts from Tasks 177–181

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 181

Task 181 already reduced the exact live packet to:

- `px_range = 1.0`
- `outline = 0.0`
- `texpixel_size = (1/256, 1/256)`
- source rect `14x16`
- destination rect `14x16`
- therefore `px_size = 1.0` on this exact packet

That means the live no-outline MSDF fragment branch simplifies to:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp(d, 0.0, 1.0);
color.a = a * color.a;
```

And because the median of normalized texture channels already lies in `[0, 1]`, this further reduces to the packet-local effective form:

```glsl
a = d;
color.a = d * color.a;
```

So the remaining question is whether the preserved seam tracks the `d` reconstruction or the final multiplication by the inherited incoming alpha.

## Source-backed split between `d` and the alpha rewrite

### Branch-local step: `d = msdf_median(...)`

In the exact branch source:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
```

This is the branch-local reinterpretation step.

It is the place where the sampled texture stops being ordinary display RGBA and starts being treated as MSDF distance data. In the baseline non-MSDF branch, there is no corresponding median collapse at all; the code instead does:

```glsl
color *= texture(sampler2D(color_texture, texture_sampler), uv);
```

So `d = median(r, g, b)` is a specialization-only semantic fork.

### Shared incoming-alpha carrier: `color.a`

The final fragment-stage alpha application is:

```glsl
color.a = a * color.a;
```

On this packet, with `a = d`, that becomes:

```glsl
color.a = d * color.a;
```

But `color.a` here is not a new MSDF-specific quantity introduced by the branch. It is inherited fragment color state that already exists before the branch runs:

```glsl
vec4 color = color_interp;
```

and `color_interp` is emitted earlier from the shared vertex path:

```glsl
color_interp = color;
```

That upstream `color` is built from the ordinary rect/instance/modulation pipeline and exists regardless of whether the fragment later takes the MSDF branch or the baseline non-MSDF branch.

So the `* color.a` part is an application of preexisting packet alpha/modulation state, not the branch-specific semantic fork.

## Why `d` is the tighter preserved carrier

The tighter preserved carrier is the **median-distance reconstruction `d`**, not the final alpha rewrite shell.

Why:

1. The final multiplication step is structurally generic.
   - `color.a = something * color.a` uses the already-shared incoming alpha carrier that is present before branch selection.
   - It is an application site, not the specialization-only reinterpretation.
2. The branch-specific semantic fork is earlier.
   - `d = median(msdf_sample.r, msdf_sample.g, msdf_sample.b)` is the exact point where the sampled texel is reinterpreted as distance data instead of ordinary textured color.
3. Task 181 already demoted the derivative term.
   - Once `px_size = 1.0`, there is no remaining derivative-driven scaling seam to compete here.
4. The remaining effective formula is `color.a = d * color.a`.
   - In that product, the branch-only factor is `d`.
   - The trailing `* color.a` is inherited packet alpha modulation from the shared path, not a new MSDF-only concept.

So the narrowest source-backed preserved-crash classification is:

- the exact first-L88 no-outline MSDF seam tracks the **median-distance reconstruction** more tightly than the final alpha rewrite shell
- the alpha rewrite remains part of the executed packet, but it is the generic carrier that applies the already-derived branch-local value into the shared fragment color state

## Why this remains inside the locked crash envelope

This conclusion stays inside the already-approved preservation result from Task 177:

- exact `packed_0 0x0 -> 0x2` specialization flip still preserves the same outer identity
  - `submit_serial=9`
  - `fence_wait_error submit_serial=9`
  - `Tonemap (L87)`
  - `Command Graph (L88)`
  - later `BLIT_PASS`
  - exit `-6`

And it stays inside the already-approved packet-local reductions from Tasks 180 and 181:

- outline behavior is dormant here because `outline = 0.0`
- derivative scaling is demoted here because `px_size = 1.0`

That leaves the median-distance reconstruction as the surviving live branch-only ingredient inside the same already-locked packet shell.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the tighter preserved carrier is **`d = median(r, g, b)`**, not the outer `color.a = a * color.a` application shell.

The reason is narrow and source-backed:

- `d` is the branch-only reinterpretation of sampled RGB into distance data
- `color.a` is inherited shared packet alpha from the upstream vertex/modulation path
- after Task 181's reduction, the live packet is effectively `color.a = d * color.a`
- therefore the surviving preserved seam tracks the **median-distance factor** more tightly than the generic alpha multiply that applies it
