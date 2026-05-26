# GDGS first-L88 MSDF emitted-scalar `d` irreducible-carrier note (2026-05-26)

## Goal

Stay on the exact first-L88 no-outline MSDF packet and determine whether the surviving emitted scalar `d` can be narrowed any further as a packet-local preserved seam, or whether `d` is now the irreducible carrier unless the investigation widens beyond the current exact packet.

This note stays source-backed, packet-local, and documentation-only.

## Scope lock

This slice stays on the already-approved exact seam only:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact branch: the `sc_use_msdf()` no-outline rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact already-approved reductions from Tasks 177–185

It does **not** widen into a fix and does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or broader route-selection theories.

## Starting point from Task 185

Task 185 already reduced the surviving packet-local branch to this form:

```glsl
vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

And it already demoted the exact contributing source-channel identity behind `d`.

So the remaining question is narrower still:

- is there any packet-local carrier *smaller than* the emitted scalar `d`, or
- is `d` now the irreducible preserved carrier unless the investigation widens beyond this exact packet?

## Source-backed narrowing check

### What the packet still forwards

On this exact packet, the branch-local value that leaves the MSDF-specific helper and continues forward is:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
color.a = d * color.a;
```

After the prior reductions, there is no remaining packet-local sideband attached to `d`:

- no surviving outline parameter on this packet (`outline = 0.0` already demoted)
- no surviving derivative gain term on this packet (`px_size = 1.0` already demoted)
- no surviving ordering witness or channel label exported from `msdf_median(...)`
- no later packet-local branch that reinterprets `d` into a smaller categorical state before the packet rejoins shared flow

So the branch-local handoff is scalar-only: the packet carries forward the float value `d` and nothing narrower beside it.

### Why obvious smaller candidates do not survive packet-locally

#### 1) Not the exact source-channel identity

Task 185 already demoted this:

- the helper returns a float, not `(value, source_channel)`
- channel identity is internal provenance only

So that candidate is broader than `d`, not narrower.

#### 2) Not the broader three-channel ordering relationship

Task 184 already demoted this:

- ordering is used internally to choose the median
- the packet exports only the chosen scalar

So that candidate is also broader than `d`, not narrower.

#### 3) Not the post-multiply alpha result as a tighter packet-local seam

Task 182 already demoted the downstream alpha shell in favor of `d`:

```glsl
color.a = d * color.a;
```

The inherited `color.a` exists before branch selection in shared fragment state, so the multiply result is not a narrower MSDF-only carrier. It is a downstream use of `d` inside a broader shared shell.

#### 4) Not a thresholded or categorical sub-property of `d`

Within this exact packet, source does not collapse `d` to a smaller packet-local category such as:

- sign bit only
- `d > 0.5` only
- “inside/outside” only
- any boolean or enum derived from `d`

That kind of reduction would require an additional packet-local compare/export stage, but the no-outline packet does not do that after the Task 181 reduction. It consumes the scalar directly in multiplication.

So the packet-local consumer is value-consumptive, not category-consumptive.

## Packet-local irreducibility classification

At this point the surviving packet-local MSDF-only carrier is irreducible at the current seam.

Why:

1. `d` is already the narrowest branch-local value exported by the last specialization-only transform.
   - `msdf_median(...)` exports only the selected scalar.
2. Every broader provenance fact behind `d` has already been demoted.
   - raw RGB relationship
   - median collapse ordering relationship
   - contributing channel identity
3. Every obvious downstream shell around `d` has already been demoted.
   - derivative gain term
   - outline path
   - final alpha-application shell
4. The packet does not further compress `d` into a smaller packet-local token before rejoining shared flow.
   - it uses `d` directly as a scalar multiplier

So any attempt to narrow further would stop being an honest packet-local reduction and would instead widen into a different class of question, such as:

- exact numeric behavior of the inherited pre-branch `color.a`
- downstream framebuffer/blend consequences after this packet
- cross-packet or later-pass effects
- runtime numeric instrumentation of the sampled values themselves

Those are all beyond the current exact packet-local seam.

## Why this remains inside the locked crash envelope

This conclusion stays inside the already-approved preservation result from Task 177 and the packet-local reductions from Tasks 180–185:

- exact `packed_0 0x0 -> 0x2` specialization flip still preserves the same outer identity
  - `submit_serial=9`
  - `fence_wait_error submit_serial=9`
  - `Tonemap (L87)`
  - `Command Graph (L88)`
  - later `BLIT_PASS`
  - exit `-6`
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the branch-local median collapse
- Task 184 already demoted the broader three-channel ordering relationship in favor of the selected scalar output
- Task 185 already demoted contributing source-channel identity in favor of emitted scalar `d`

That leaves no smaller honest packet-local carrier than `d` on this exact branch.

## Conclusion

On the exact first-L88 no-outline MSDF packet, the emitted scalar `d` is now the **irreducible preserved carrier** as a packet-local seam.

The narrow source-backed reason is:

- all broader provenance behind `d` has already been demoted
- all obvious surrounding shells around `d` have already been demoted
- the packet exports no smaller sideband than `d`
- the packet consumes `d` directly rather than collapsing it into a smaller packet-local categorical value

So any further honest narrowing would require widening beyond the current exact packet rather than continuing the same packet-local reduction ladder.
