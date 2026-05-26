# GDGS first-L88 MSDF raw RGB relationship versus median-collapse note (2026-05-26)

## Goal

Take the surviving first-L88 median-distance seam one rung narrower and classify whether the tighter preserved carrier is:

- the raw sampled RGB channel relationship itself, or
- the `median(r, g, b)` collapse that turns those channels into the scalar carrier `d`

This note remains source-backed and packet-local.

## Scope lock

This slice stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect L88 packet
- exact branch: the no-outline `sc_use_msdf()` rect branch in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved packet facts and reductions from Tasks 177–182

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 182

Task 182 already reduced the live packet to the effective form:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

That left the median-distance carrier `d` as the tighter preserved seam than the generic final alpha-application shell.

So the remaining split is narrower still:

- is the surviving preserved seam merely the existence/relationship of the sampled RGB channels, or
- is it the branch-only `median(r, g, b)` collapse that converts those shared channels into the scalar carrier?

## Source-backed split

### Shared sample-side ingredient: raw sampled RGB relationship

Inside the exact MSDF branch, the first step is:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
```

That gives the packet access to raw sampled texture channels, including `msdf_sample.r`, `.g`, and `.b`.

But this by itself is **not** yet the branch-only preserved fork, because the baseline non-MSDF branch also samples the same texture from the same source at the same `uv`:

```glsl
color *= texture(sampler2D(color_texture, texture_sampler), uv);
```

So the existence of raw sampled RGB data — and even the underlying RGB relationship present in the sampled texel — is not unique to the MSDF branch. The baseline path also sees that same texel payload; it simply uses it as ordinary display modulation instead of collapsing it into a distance value.

### Branch-only ingredient: `median(r, g, b)` collapse

The MSDF-only reinterpretation step is:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
```

with the helper:

```glsl
float msdf_median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}
```

This is the narrower branch-only transform.

It is the exact step that:

- discards channel identity/order detail beyond the median ordering relation
- converts the sampled RGB triplet into a single scalar carrier `d`
- feeds the already-reduced no-outline alpha path that survives on this packet

The baseline path never performs this collapse. It samples the same texel, but it does not reinterpret sampled RGB as a signed-distance carrier by collapsing the triplet through `msdf_median(...)`.

## Why the median collapse is the tighter preserved seam

The tighter preserved seam is the **`median(r, g, b)` collapse**, not the raw sampled RGB relationship alone.

Why:

1. Raw sampled RGB is shared input, not the specialization-only fork.
   - Both branches sample the same texture at the same `uv`.
   - So the mere presence of RGB channel data cannot be the narrowest MSDF-specific preserved seam.
2. The branch-only reinterpretation happens at the collapse step.
   - `msdf_median(...)` is the exact specialization-only operation that turns the shared RGB triplet into the scalar distance carrier used by the surviving packet.
3. The collapse is narrower than the full raw-channel relationship.
   - The raw sampled triplet contains more information than the surviving packet-local formula actually keeps.
   - The live MSDF path retains only the median-derived scalar `d` for the already-reduced no-outline packet.
4. This stays aligned with prior reductions.
   - Task 180 demoted outline behavior.
   - Task 181 demoted derivative scaling by reducing `px_size` to `1.0`.
   - Task 182 demoted the generic final alpha-application shell.
   - The next surviving branch-only carrier is therefore not “raw RGB exists,” but “raw RGB is collapsed by `msdf_median(...)` into `d`."

So the narrowest source-backed preserved-crash classification now becomes:

- the exact first-L88 no-outline MSDF seam tracks the **median-collapse transform** more tightly than the raw sampled RGB relationship by itself

## Important interpretation boundary

This conclusion remains classification-only.

It does **not** claim that the underlying sampled texture contents are irrelevant. They are obviously required inputs. The narrower point is only that, on this exact preserved packet, the raw RGB triplet is still a shared sampled input surface, while the `median(r, g, b)` collapse is the branch-only transform that survives after the earlier reductions.

## Why this remains inside the locked crash envelope

This conclusion stays fully inside the already-approved preservation result from Task 177 and the subsequent packet-local reductions from Tasks 180–182:

- exact `packed_0 0x0 -> 0x2` specialization flip still preserves the same outer identity
  - `submit_serial=9`
  - `fence_wait_error submit_serial=9`
  - `Tonemap (L87)`
  - `Command Graph (L88)`
  - later `BLIT_PASS`
  - exit `-6`
- outline is already demoted on this packet because `outline = 0.0`
- derivative scaling is already demoted because `px_size = 1.0`
- the final alpha-application shell is already demoted because `color.a` is inherited shared fragment state

That leaves the branch-only `median(r, g, b)` collapse as the tighter surviving preserved seam inside the same already-locked packet shell.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the tighter preserved seam is **the `median(r, g, b)` collapse itself**, not the raw sampled RGB relationship alone.

The reason is narrow and source-backed:

- the raw sampled RGB triplet is shared with the baseline texture-sampling path
- the MSDF-specific divergence happens when that shared RGB payload is collapsed through `msdf_median(...)`
- after the prior packet-local reductions, that collapse is the tightest surviving branch-only carrier still present inside the locked `submit_serial=9` envelope
