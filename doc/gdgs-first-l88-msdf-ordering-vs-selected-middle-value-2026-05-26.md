# GDGS first-L88 MSDF three-channel ordering versus selected middle-value output note (2026-05-26)

## Goal

Take the surviving first-L88 `msdf_median(...)` seam one rung narrower and classify whether the tighter preserved carrier is:

- the broader three-channel ordering relationship that determines which sampled channel is in the middle, or
- the exact selected middle-value scalar output `d` that `msdf_median(...)` emits and forwards

This note stays source-backed, packet-local, and documentation-only.

## Scope lock

This slice stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact branch: the no-outline `sc_use_msdf()` rect branch in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved packet facts and reductions from Tasks 177–183

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 183

Task 183 already reduced the surviving packet-local seam to the exact median collapse:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

with the helper:

```glsl
float msdf_median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}
```

So the remaining split is narrower still:

- is the surviving preserved seam the order-statistic relationship among `r`, `g`, and `b`, or
- is it the exact scalar value actually selected and emitted as `d`

## Source-backed split

### Broader upstream ingredient: three-channel ordering relationship

`msdf_median(r, g, b)` depends on the relative ordering among the three sampled channels.

In plain terms, the helper asks which sampled component is the middle one once `r`, `g`, and `b` are ordered. That ordering relation is necessary because it determines which candidate value survives the collapse.

So the ordering relationship is a real upstream ingredient of the MSDF-only collapse. Without it, the helper cannot know whether the emitted median is coming from `r`, `g`, or `b`.

But the ordering relationship is still the broader carrier, because it does not itself forward the numeric payload used downstream. It only determines *which* sampled channel value gets selected.

### Tighter downstream carrier: exact selected middle-value scalar output

The value actually forwarded out of the helper is the scalar:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
```

That scalar is tighter than the ordering relationship because it is the exact payload the rest of the already-reduced packet consumes.

Once `d` has been chosen:

- the broader ordering information is no longer forwarded separately
- the downstream no-outline packet keeps only the emitted scalar
- the later packet-local use is value-consumptive, not ordering-consumptive

On this exact packet, after Task 181 reduced `px_size` to `1.0`, the live no-outline form is effectively:

```glsl
color.a = d * color.a;
```

So the surviving downstream packet does not consume a separate “ordering descriptor.” It consumes only the exact emitted scalar `d`.

## Why the selected middle-value scalar is the tighter preserved seam

The tighter preserved carrier is the **exact selected middle-value scalar output `d`**, not the broader three-channel ordering relationship.

Why:

1. The ordering relationship is upstream selection context.
   - It determines which sampled channel becomes the median.
   - But by itself it does not carry the numeric payload that the rest of the packet actually uses.
2. The emitted scalar is the surviving forwarded payload.
   - After the helper returns, the packet keeps `d` and uses it directly in the already-reduced no-outline alpha path.
   - No separate ordering structure survives past that point.
3. The scalar is strictly narrower than the ordering relation.
   - Many possible channel triplets can share the same ordering pattern while producing different exact median values.
   - The ordering relation therefore preserves less exact downstream information than the emitted scalar.
4. This fits the prior reductions.
   - Task 180 demoted outline behavior.
   - Task 181 demoted derivative scaling by reducing `px_size` to `1.0`.
   - Task 182 demoted the generic final alpha-application shell.
   - Task 183 demoted the broader raw sampled RGB relationship in favor of the median collapse.
   - The next honest packet-local reduction is therefore inside that collapse itself: ordering context versus the exact emitted scalar, and the emitted scalar is tighter.

## Important interpretation boundary

This classification does **not** say the ordering relationship is irrelevant. It remains required selection context inside `msdf_median(...)`.

The narrower claim is only this:

- the ordering relationship explains how the helper chooses the winner
- the exact selected middle-value scalar `d` is the tighter preserved carrier because it is the precise payload that survives and is consumed by the rest of the already-reduced packet

## Why this remains inside the locked crash envelope

This conclusion stays inside the same already-approved preservation envelope:

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
- the broader raw sampled RGB bundle is already demoted because Task 183 reduced the surviving seam to the branch-local median collapse itself

That leaves the exact emitted middle-value scalar `d` as the tightest surviving preserved carrier presently identified inside the same already-locked packet shell.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the tighter preserved seam is **the exact selected middle-value scalar output `d` emitted by `msdf_median(...)`**, not the broader three-channel ordering relationship.

The reason is narrow and source-backed:

- the ordering relationship is required upstream selection context
- but the rest of the already-reduced packet does not carry that ordering context forward as its own payload
- the packet forwards and consumes only the exact emitted scalar `d`
- so within the `msdf_median(...)` collapse, the selected middle-value output is the tighter preserved carrier

## Next honest packet-local seam

If this seam is reduced one rung farther without widening scope, the next honest split would be:

- the identity of which sampled channel supplied the selected middle value (`r` vs `g` vs `b`), versus
- the exact numeric scalar value emitted as `d`

That would stay inside the same `msdf_median(...)` packet-local collapse while asking whether the tighter preserved carrier is the selected-channel identity or the final emitted scalar magnitude itself.
