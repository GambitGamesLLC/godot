# GDGS first-L88 source-alpha admission: export vs color-term note (2026-05-26)

## Goal

Continue exactly from Task 191 without widening scope.

The narrow question here is:

- after Task 191 reduced the surviving source-alpha seam to **incoming packet-color admission** at the preserved `L88` merge,
- is the tightest remaining admission-side seam best located at the exported source-alpha carrier itself (`frag_color.a` / `src.a`),
- or only one step later at the full admitted color term `src.rgb * src.a`?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 191

Task 191 already fixed the surviving source-alpha side to:

- the exact preserved merge still uses `src.rgb * src.a + dst.rgb * (1 - src.a)`
- the tighter surviving side is incoming packet-color admission, not retained destination context
- the same no-outline MSDF packet still carries `d -> a -> color.a -> frag_color.a`

So this task does not revisit destination retention. It only asks whether the admission-side seam is tightest at the exported alpha carrier or at the full admitted color term.

## Exact source-backed chain

The current exact source-backed chain remains:

1. `d` is reduced packet-locally in `canvas.glsl`
2. `color.a` is updated from that packet-local scalar
3. the shader exports `frag_color = color`
4. the live `BLEND_MODE_MIX` merge uses `src_color_blend_factor = SRC_ALPHA`
5. the first incoming color term becomes `src.rgb * src.a`

That means `src.a` is the first merge-visible carrier, while `src.rgb * src.a` is the first full incoming color admission product.

## Exported source alpha vs full admitted color term

### Exported source alpha (`frag_color.a` / `src.a`)

This is the tighter surviving seam.

Why:

- it is the first exact source-backed value that crosses from packet-local state into merge-visible state
- it is the coefficient that directly controls the admission weight
- it also remains reusable on both sides of the same color merge (`src.rgb * src.a`, `dst.rgb * (1 - src.a)`), even though Task 191 already demoted the destination-retention side as broader context
- the full admitted color term cannot exist until this exported coefficient is already live

### Full admitted color term (`src.rgb * src.a`)

This is one step later and therefore broader.

It is still the first full incoming color contribution, but it already bundles:

- the packet RGB payload
n- the already-live exported alpha coefficient

So it is not the tightest surviving admission-side seam; it is the first composite consequence of that tighter seam.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path:

- the source-alpha admission side remains tighter at the **exported source-alpha carrier itself** (`frag_color.a` / `src.a`)
- the full admitted incoming color term `src.rgb * src.a` is the first broader composite consequence of that exported carrier
- this stays narrower than Task 191 without reopening the already-demoted preserved destination side

## Conclusion

At the exact preserved `L88` `BLEND_MODE_MIX` merge, the smallest honest next split on the surviving admission side lands at the **exported source-alpha carrier itself** rather than the later full incoming color term.

Best narrow wording after this slice:

> inside the preserved `L88` source-alpha admission seam, the tightest surviving source-backed carrier is the exported packet alpha (`frag_color.a` / `src.a`), while the admitted incoming color term `src.rgb * src.a` is the first broader composite consequence of that carrier.
