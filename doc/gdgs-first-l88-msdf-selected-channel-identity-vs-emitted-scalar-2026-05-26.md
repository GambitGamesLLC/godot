# GDGS first-L88 MSDF selected-channel identity versus emitted scalar note (2026-05-26)

## Goal

Take the surviving first-L88 `msdf_median(...)` seam one rung narrower and classify whether the tighter preserved carrier is:

- the exact contributing source-channel identity that supplied the selected median value, or
- the emitted scalar value `d` itself after `msdf_median(...)` returns

This note stays source-backed, packet-local, and documentation-only.

## Scope lock

This slice stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact branch: the `sc_use_msdf()` no-outline rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved reductions from Tasks 177–184

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 184

Task 184 already reduced the live packet to this preserved branch-local form:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

Task 184 also already classified the tighter carrier *inside* `msdf_median(...)` as the exact selected middle-value scalar output, not the broader three-channel ordering relationship used to choose it.

So the remaining split is narrower still:

- is the tighter preserved seam the identity of which source channel supplied that selected middle value, or
- is it the emitted scalar value `d` that actually leaves the helper and flows forward into packet execution?

## Source-backed split

### Broader internal provenance: contributing source-channel identity

The helper implementation is:

```glsl
float msdf_median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}
```

This guarantees that the returned scalar equals one of the three sampled channel values and corresponds to the middle order statistic.

So there is an internal provenance fact behind each emitted `d`: at any given fragment, the selected scalar came from one of `msdf_sample.r`, `msdf_sample.g`, or `msdf_sample.b`.

But that channel identity is still a broader internal selection fact rather than the tightest forwarded carrier, because the helper does not export a channel label, branch flag, or sideband provenance bit. It exports only a float.

### Tighter forwarded carrier: emitted scalar value `d`

What actually leaves the helper and survives into the already-reduced packet is:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

The downstream packet consumes only the scalar `d`.

It does **not** consume:

- whether `d` came from `r`, `g`, or `b`
- a preserved channel-identity tag
- an exported ordering witness beyond the selected value itself

So once Task 184 already demoted the broader ordering relationship, the next tighter preserved carrier is still the emitted scalar value `d`, not the exact contributing source-channel identity.

## Why the emitted scalar is the tighter preserved seam

The tighter preserved seam is the **emitted scalar value `d`**, not the exact contributing source-channel identity.

Why:

1. The helper's public output is scalar-only.
   - `msdf_median(...)` returns a `float`, not a `(value, source_channel)` pair.
2. Channel identity remains internal provenance.
   - It explains where the chosen value came from, but that identity is not forwarded into the already-reduced packet shell as a separate carried value.
3. The packet uses only the scalar.
   - The next surviving live statement is `color.a = d * color.a`, which consumes the emitted value directly.
4. This is narrower than channel provenance.
   - The fact that the scalar originated from one of three possible channels is a broader explanatory relationship.
   - The actual forwarded carrier is only the scalar that survived selection.

## Important interpretation boundary

This is still classification-only.

It does **not** claim the source-channel identity is meaningless or impossible to matter in a different analysis. It only classifies the tightest packet-local preserved carrier on this exact already-reduced seam: the helper exports only the scalar value, so that value is the narrowest thing the live packet unquestionably carries forward.

## Why this remains inside the locked crash envelope

This conclusion stays inside the already-approved preservation result from Task 177 and the packet-local reductions from Tasks 180–184:

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
- the broader sampled-RGB relationship and broader three-channel ordering relationship are already demoted in favor of the branch-local median collapse and its exact selected scalar output

That leaves the emitted scalar value `d` as the tighter surviving preserved carrier on this exact packet.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the tighter preserved seam is **the emitted scalar value `d` returned by `msdf_median(...)`**, not the exact contributing source-channel identity.

The narrow source-backed reason is simple:

- channel identity is internal provenance behind the selection
- the helper exports only a scalar float
- the already-reduced packet consumes only that scalar in `color.a = d * color.a`

So after the prior reductions, the surviving packet-local carrier tightens again from “which channel supplied the value” to “the selected scalar value that was actually emitted and forwarded.”
