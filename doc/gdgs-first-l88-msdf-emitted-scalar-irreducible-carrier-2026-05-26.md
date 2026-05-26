# GDGS first-L88 MSDF emitted-scalar irreducible-carrier note (2026-05-26)

## Goal

Stay on the exact first-L88 no-outline MSDF packet and determine whether the surviving emitted scalar seam

- `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`

can be narrowed any further while remaining packet-local, or whether `d` is now the irreducible preserved carrier unless the investigation widens beyond the current exact packet.

This slice is documentation/analysis only.

## Scope lock

This note stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact branch: the `sc_use_msdf()` no-outline rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved reductions from Tasks 177–185

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 185

Task 185 already reduced the live packet to the narrowest carried branch-local form established so far:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

And Task 185 already classified the tighter seam as:

- **the emitted scalar value `d`**

rather than:

- the exact contributing source-channel identity behind that value

So the remaining honest question is narrower still:

- can `d` itself be separated any further while staying packet-local, or
- is `d` now the irreducible preserved carrier unless we widen the analysis beyond the current exact packet?

## Source-backed answer

On this exact packet, `d` is now the **irreducible preserved carrier**.

### Why `d` cannot be narrowed further without widening

At the current reduction level, the branch exports only one MSDF-owned scalar into downstream packet execution:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
```

Everything broader has already been demoted:

- outline behavior is inactive on this packet (`outline = 0.0`)
- derivative scaling is neutral on this packet (`px_size = 1.0`)
- the broader sampled-RGB relationship is already demoted in favor of the median collapse
- the broader three-channel ordering relationship is already demoted in favor of the selected scalar output
- contributing source-channel identity is already demoted in favor of the emitted scalar itself
- the final alpha rewrite is broader carrier application rather than a narrower MSDF-owned ingredient

What remains is only the scalar `d`.

There is no additional sideband value exported from the helper alongside it:

- no channel-id output
- no ordering witness output
- no derivative-gain output on this packet
- no outline-side output on this packet
- no separate packet-local subfield of `d`

So inside the current exact packet classification, there is nothing narrower still that the branch clearly carries forward as a distinct preserved value.

## What would count as widening beyond the current packet

To go narrower than `d`, the investigation would have to leave the current packet-local carrier classification and widen into one of these larger surfaces:

1. **Exact numeric value / spatial distribution analysis**
   - asking what particular floating-point value `d` takes at specific fragments or regions
   - that moves from carrier classification into value-instance behavior across the packet

2. **Upstream sampled-field provenance beyond the exported scalar**
   - asking for per-fragment channel ordering witnesses or richer source provenance than the helper exports
   - that widens back into internal explanatory structure rather than the forwarded carrier

3. **Downstream consequence analysis beyond the scalar itself**
   - asking how particular `d` values affect later alpha, blending, framebuffer contents, or later passes
   - that widens into packet consequences rather than the narrowest preserved carrier

Those may be valid later investigations, but they are wider than the current exact packet-local seam.

## Classification

For the exact first-L88 no-outline MSDF packet, the surviving emitted scalar seam **cannot be narrowed any further while staying packet-local**.

The honest packet-local classification is therefore:

- **`d` is the irreducible preserved carrier**

until or unless the investigation widens beyond the current exact packet.

## Why this remains inside the locked crash envelope

This conclusion stays inside the already-approved preservation result from Task 177 and the packet-local reductions from Tasks 180–185:

- exact `packed_0 0x0 -> 0x2` specialization flip still preserves the same outer identity
  - `submit_serial=9`
  - `fence_wait_error submit_serial=9`
  - `Tonemap (L87)`
  - `Command Graph (L88)`
  - later `BLIT_PASS`
  - exit `-6`
- Tasks 180–185 already reduced the exact packet-local seam step by step down to the emitted scalar `d`
- this Task 186 slice does not add a new mechanism; it only classifies that no narrower packet-local preserved carrier remains without widening scope

## Conclusion

On the exact first-L88 no-outline MSDF packet, the emitted scalar value `d` returned by `msdf_median(...)` is now the **irreducible preserved carrier**.

A narrower answer would require widening beyond the current exact packet-local seam into value-instance behavior, richer upstream provenance, or downstream consequences. Inside the present approved scope, `d` is the narrowest honest preserved carrier left.