# GDGS first-L88 source-alpha carrier: packet-local shared alpha vs export/writeback slot note (2026-05-26)

## Goal

Continue exactly from Task 193 without widening scope.

The narrow question here is:

- after Task 193 reduced the surviving exported source-alpha carrier to the shader-export identity `frag_color.a`,
- is the tightest remaining seam still best named at that export/writeback slot,
- or one rung deeper at the last packet-local/shared-fragment carrier `color.a` that is copied into `frag_color.a`?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 193

Task 193 already reduced the surviving source-backed carrier to the exported shader alpha name itself:

- the exact no-outline packet still carries `d -> a -> color.a -> frag_color.a`
- the later merge-side alias `src.a` was demoted as only the first fixed-function consumer name
- the broader admitted color term `src.rgb * src.a` stayed demoted as a later composite consequence

So this task only asks whether the remaining carrier is tightest at the final export/writeback slot name or at the last packet-local/shared-fragment carrier immediately before that writeback.

## Exact packet-local/writeback relation

The exact source-backed relation here is a direct whole-vector copy, not a new alpha transform.

In `servers/rendering/renderer_rd/shaders/canvas.glsl` on the exact no-outline MSDF lane:

```glsl
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

Later, the fragment ends with:

```glsl
frag_color = color;
```

So for the surviving source-alpha carrier:

- `color.a` is the last packet-local/shared-fragment alpha value still owned by shader flow
- `frag_color.a` is the immediate export/writeback slot name produced by copying that same `color` vector out of the fragment

On the currently approved evidence, there is no narrower alpha-only rewrite, selector, or additional transform between those two names.

## Packet-local carrier vs export/writeback slot

### `color.a`

This is the tighter surviving carrier one rung deeper than Task 193.

Why:

- it is the last exact packet-local/shared-fragment value identity before export
- it is still described entirely inside shader-owned state
- the later `frag_color.a` name is produced by copying this same carrier into the fragment output slot

### `frag_color.a`

This is one step later and therefore broader.

It is still the exact same live alpha at the merge boundary, but as a name it now refers to the export/writeback slot rather than the last packet-local carrier. That makes it the first outward-facing alias of the already-established `color.a`, not a smaller surviving seam.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path:

- the surviving exported shader-alpha carrier splits one rung deeper into **packet-local/shared-fragment `color.a`** versus the later export/writeback slot `frag_color.a`
- the tighter surviving carrier is **`color.a`**
- `frag_color.a` remains only the immediate fragment-output/writeback alias of that same carrier
- this stays narrower than Task 193 without reopening the later merge-side alias `src.a` or the broader admitted color term

## Conclusion

At the exact preserved `L88` source-alpha admission seam, the exported shader-alpha carrier can be split one rung deeper by exact packet-local/writeback facts:

- **tightest surviving carrier:** packet-local/shared-fragment alpha `color.a`
- **next later alias:** export/writeback slot `frag_color.a`

Best narrow wording after this slice:

> inside the preserved `L88` source-alpha admission seam, the surviving exported shader-alpha carrier is tighter at the last packet-local/shared-fragment alpha `color.a`; `frag_color.a` is only the immediate fragment-output writeback alias of that same carrier.
