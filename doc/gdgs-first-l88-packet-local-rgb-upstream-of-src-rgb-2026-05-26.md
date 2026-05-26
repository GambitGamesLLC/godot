# GDGS first-L88 packet-local RGB immediately upstream of merge-visible `src.rgb` (2026-05-26)

## Goal

Continue exactly from Task 201 after the old scope lock terminated at merge-visible `src.rgb`.

The reopened question here is narrower and fully source-backed:

- once the preserved `Command Graph (L88)` source-admission lane has already reduced the merge-visible carrier to **`src.rgb`**,
- what is the exact packet-local RGB composition immediately upstream of that carrier on the same first-`L88` no-outline MSDF lane,
- and does that upstream RGB come from the MSDF sampled texture RGB, or from an inherited packet-local modulation carrier that the MSDF branch leaves untouched?

This note stays documentation-only.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- Task 196 status inherited: packet-local alpha ladder already rejoined as `a = d`
- Task 201 status inherited: merge-visible source-color lane under the old constraint set already terminated at `src.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 201

Task 201 established the terminal stop under the old constraint set:

```text
src.a
-> src.rgb * src.a
-> src.rgb
```

That task also explicitly recorded why it had to stop there:

- any further inward split would reopen packet-local RGB composition

Task 202 reopens exactly that previously-forbidden inward seam, but only one rung inward and only on the same preserved packet.

## Exact packet-local/writeback relation on the live no-outline MSDF lane

The exact fragment-side source-backed relation in `servers/rendering/renderer_rd/shaders/canvas.glsl` is:

```glsl
void main() {
	vec4 color = color_interp;
```

Then, on the exact rect/no-outline MSDF branch:

```glsl
if (sc_use_msdf()) {
	...
	float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
	...
	float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
	color.a = a * color.a;
}
```

And later the fragment ends with:

```glsl
frag_color = color;
```

Those three facts matter together:

1. `color` starts this packet as the inherited interpolated vector `color_interp`
2. on the exact no-outline MSDF lane, the branch mutates **`color.a` only**
3. `frag_color` is then only a whole-vector writeback alias of `color`

So on this exact packet, the RGB side does **not** get recomposed by the MSDF branch. The MSDF sampled texture RGB participates only in the scalar-distance path:

```text
msdf_sample.r/g/b -> msdf_median(...) -> d -> a -> color.a
```

That is the alpha lane already classified earlier, not the live packet-local RGB carrier feeding merge-visible `src.rgb`.

## Exact packet-local RGB composition immediately upstream of `src.rgb`

Because the no-outline MSDF branch never rewrites `color.rgb`, the immediate upstream packet-local RGB chain is:

```text
color_interp.rgb -> color.rgb -> frag_color.rgb -> src.rgb
```

The first honest reopened seam is therefore **not** inside `msdf_sample.rgb`.

It is the packet-local shared-fragment RGB carrier **`color.rgb`**, whose exact composition on this lane is simply the inherited interpolated packet RGB **`color_interp.rgb`** carried forward unchanged until export.

That yields the exact classification for this slice:

- **tightest packet-local/shared-fragment RGB carrier immediately upstream of merge-visible `src.rgb`:** `color.rgb`
- **immediate export/writeback alias of that same carrier:** `frag_color.rgb`
- **exact packet-local RGB composition on this no-outline MSDF lane:** inherited `color_interp.rgb`, preserved unchanged by the MSDF branch
- **not the live RGB carrier on this slice:** `msdf_sample.rgb`, because that sampled RGB is consumed only to derive scalar `d` for alpha on the exact no-outline path

## Why this is the next honest inward seam

This is the first honest inward step because it reopens only what Task 201 intentionally left closed:

- it moves exactly one rung inward from merge-visible `src.rgb`
- it does so by packet-local/writeback facts already present in live source
- it does not widen back out to the broader admitted term `src.rgb * src.a`
- it does not jump sideways into destination-retention or pipeline/cache theory

It also clarifies a potentially misleading alternative:

- the sampled MSDF texture RGB is upstream in the packet, but on this exact no-outline lane it is **not** the packet-local RGB composition immediately upstream of `src.rgb`
- instead, the sampled texture RGB only feeds the already-classified scalar alpha ladder

## Exact next seam now materialized

With this reopening complete, the next honest inward seam is now explicit:

```text
color.rgb -> color_interp.rgb
```

If continuation is still wanted on the same preserved lane, the next slice should classify where this exact `color_interp.rgb` comes from on the rect packet path, rather than jumping back to sampled MSDF RGB or outward to the merge composite.

In source terms, that next seam starts with the inherited packet-local input `vec4 color = color_interp;` in `canvas.glsl` and continues backward to the rect-path batch payload that populates `color_interp`.

## Conclusion

Reopening the previously-forbidden inward seam does **not** reveal MSDF sampled RGB as the immediate upstream carrier of merge-visible `src.rgb` on the exact first-`L88` no-outline packet.

Instead, the exact packet-local RGB composition immediately upstream of merge-visible `src.rgb` is the unchanged inherited packet RGB:

```text
color_interp.rgb -> color.rgb -> frag_color.rgb -> src.rgb
```

Best narrow wording after this slice:

> on the preserved first-`Command Graph (L88)` no-outline MSDF lane, the exact packet-local RGB composition immediately upstream of merge-visible `src.rgb` is the unchanged inherited packet RGB `color_interp.rgb`, carried through packet-local `color.rgb` and then exported as `frag_color.rgb`; the MSDF sampled texture RGB is upstream only of the alpha-distance ladder, not of the live RGB carrier on this slice.
