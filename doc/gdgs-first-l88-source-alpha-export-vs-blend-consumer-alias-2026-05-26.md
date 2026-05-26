# GDGS first-L88 source-alpha carrier: shader export vs blend-consumer alias note (2026-05-26)

## Goal

Continue exactly from Task 192 without widening scope.

The narrow question here is:

- after Task 192 reduced the surviving admission-side seam to the **exported source-alpha carrier itself**,
- is the tightest remaining seam best located at the shader-export identity `frag_color.a`,
- or only one step later at the blend-side consumed alias `src.a` under the exact preserved `L88` merge?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 192

Task 192 already reduced the admission-side seam to the exported source-alpha carrier itself:

- the no-outline MSDF lane still carries `d -> a -> color.a -> frag_color.a`
- the full incoming color term `src.rgb * src.a` was demoted as a broader composite consequence

So this task only asks whether the remaining carrier is tightest at the shader export site or the immediate blend-side alias.

## Exact source-backed relation

The exact source-backed relation here is direct aliasing, not a new transform.

In `servers/rendering/renderer_rd/shaders/canvas.glsl`:

- the packet finishes with `frag_color = color`
- the surviving carrier therefore leaves the shader as `frag_color.a`

In the exact merge contract from `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`:

- `src_color_blend_factor = SRC_ALPHA`
- so the same exported alpha is consumed immediately by the fixed-function blend stage as `src.a`

There is no additional packet-side rewrite, selector, or value transform between those two names on the current approved evidence.

## Export identity vs blend-side alias

### `frag_color.a`

This is the tighter surviving seam.

Why:

- it is the last exact programmable/source-backed value identity before the merge boundary
- it is the point where packet-local state stops and merge-visible state begins
- `src.a` at this seam is only the immediate fixed-function consumer name for that same exported value

### `src.a`

This is one step later and therefore broader.

It is still the exact same live value at the merge, but it no longer identifies a smaller carrier; it is simply the first consumer-side alias of the already-exported `frag_color.a`.

So the split does not reveal a new smaller value — only that the export identity is the tightest source-backed naming of the same surviving carrier.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path:

- the surviving source-alpha carrier is tightest at the shader-export identity **`frag_color.a`**
- the blend-side `src.a` name is the immediate fixed-function consumer alias of that same carrier, not a smaller surviving seam
- this stays one rung narrower than Task 192 without reopening the later composite color term or the already-demoted preserved destination side

## Conclusion

At the exact preserved `L88` `BLEND_MODE_MIX` merge, the tightest remaining source-backed carrier naming is the shader-export identity **`frag_color.a`**, while `src.a` is only the immediate merge-side alias that consumes it.

Best narrow wording after this slice:

> inside the preserved `L88` source-alpha admission seam, the tightest surviving source-backed carrier is the exported shader alpha `frag_color.a`; the merge-side `src.a` term is only the first fixed-function alias of that same carrier.
