# GDGS first-L88 MSDF median ordering versus selected middle-value note (2026-05-26)

## Goal

Stay on the exact first-L88 no-outline MSDF packet and tighten the surviving `msdf_median(...)` seam one rung farther:

- broader three-channel ordering relationship inside `msdf_median(r, g, b)`
- versus the exact selected middle-value scalar output that the helper emits

This slice is documentation/analysis only. It does not widen into a fix.

## Scope lock

This note stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` packet
- exact branch: the no-outline `sc_use_msdf()` rect branch in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved packet facts carried from Tasks 177–183

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broaden into a fix.

## Packet-local starting point already established

Prior slices already reduced the exact packet to:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

Task 183 already established that the tighter preserved seam is the branch-only `median(r, g, b)` collapse itself rather than the broader raw sampled RGB relationship.

So the remaining honest split is now internal to the helper:

```glsl
float msdf_median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}
```

## Exact source-backed split inside `msdf_median(...)`

The helper has two distinguishable notions embedded in it:

1. **Broader three-channel ordering relationship**
   - which channel is low, middle, high
   - pairwise comparisons and ordering constraints among `r`, `g`, and `b`
   - the larger comparison structure needed to decide what the median is

2. **Exact selected middle-value scalar output**
   - the one scalar returned by the helper
   - whichever of `r`, `g`, or `b` is the middle value under that ordering
   - the only quantity that actually leaves the helper as `d`

The helper implementation is important here:

```glsl
return max(min(r, g), min(max(r, g), b));
```

This is not preserving the full ordering structure as an output object. It uses ordering relations only long enough to select one scalar — the middle value — and then returns only that scalar.

## Why the selected middle-value scalar is the tighter preserved seam

The tighter preserved seam is **the exact selected middle-value scalar output**, not the broader ordering relationship.

### 1. The ordering relationship is broader internal decision structure

To know the median, the helper implicitly relies on the relative ordering of all three channels.
That internal comparison structure is real, but it is broader than the surviving packet-local carrier because it contains more information than the packet exports:

- who was smallest
- who was largest
- who was middle
- the relative ordering constraints that made that true

That full ordering structure does not survive as a downstream packet value.

### 2. The helper exports only one scalar

What actually leaves `msdf_median(...)` is only:

```glsl
float d
```

That output is the selected middle-value scalar itself.
Once the helper returns, the broader three-channel ordering relationship is no longer preserved as an independently carried packet value. It has been consumed to choose one scalar.

### 3. The downstream packet only carries the selected scalar further

On this exact packet, earlier reductions already demoted everything after `d`:

- derivative scaling is neutral here (`px_size = 1.0`)
- the no-outline alpha path reduces to `a = d`
- the final alpha application shell is broader carrier application: `color.a = d * color.a`

So after `msdf_median(...)` resolves, the only branch-local quantity that actually survives forward is the returned middle-value scalar `d`.

### 4. This is tighter than Task 183's boundary for the same reason

Task 183 separated:

- broader raw sampled RGB relationship
- versus the branch-only median collapse

Task 184 repeats that same narrowing one level deeper inside the collapse itself:

- broader ordering relationship inside the helper
- versus the exact selected scalar that the helper emits

In both cases, the tighter seam is the **carried scalar result**, not the broader upstream comparison structure used to derive it.

## Classification

For the exact first-L88 no-outline MSDF packet, the tighter preserved seam inside `msdf_median(...)` is:

- **the exact selected middle-value scalar output returned as `d`**

not:

- the broader three-channel ordering relationship used internally to choose that value

Reason:

- the ordering relationship is the broader internal decision structure
- the helper consumes that structure and exports only one scalar
- the already-reduced live packet carries only that scalar forward into `color.a = d * color.a`

## Important interpretation boundary

This conclusion remains classification-only.

It does **not** claim the ordering relationship is irrelevant to computing `d`; it is necessary. The narrower point is simply that, on this exact preserved packet, the ordering structure is an internal selection mechanism, while the selected middle-value scalar is the surviving branch-local carrier actually forwarded by the packet.

## Why this remains inside the locked crash envelope

This conclusion stays fully inside the already-approved preservation result from Task 177 and the packet-local reductions from Tasks 180–183:

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
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the median collapse itself

So this Task 184 slice only tightens that same packet-local classification one rung farther: inside the median helper, the surviving seam is the selected middle-value scalar output, not the broader internal ordering relationship.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the tighter preserved seam inside `msdf_median(...)` is **the exact selected middle-value scalar output returned as `d`**, not the broader three-channel ordering relationship used internally to determine it.

That is the narrowest source-backed surviving MSDF-owned carrier left on this packet while staying inside the same already-locked `submit_serial=9` envelope.
