# GDGS first-L88 source-color carrier vs admitted color-term note (2026-05-26)

## Goal

Continue exactly from Task 199 without reopening packet-local RGB composition and without broadening back to the already-demoted destination-retention side.

The narrow question here is:

- once Task 199 has already fixed the next narrower source-backed consequence of the preserved `Command Graph (L88)` merge-side `src.a` admission coefficient at the admitted source-color term **`src.rgb * src.a`**,
- what is the next honest split one rung deeper inside that same source-admission lane,
- and is that split best located at the merge-visible source-color carrier **`src.rgb`** itself,
- or only at the broader admitted color product **`src.rgb * src.a`**?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- packet-local alpha ladder status inherited from Task 196: fully rejoined as `a = d`
- merge-side source-alpha admission seam inherited from Task 198: fixed at the immediate coefficient `src.a`
- admitted source-color continuation inherited from Task 199: fixed at `src.rgb * src.a`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, packet-local RGB composition, or widen into a speculative fix.

## Starting point from Task 199

Task 199 already settled the current continuation on the approved source-admission lane:

```text
d
-> a (= d)
-> color.a
-> frag_color.a
-> preserved L88 BLEND_MODE_MIX source-alpha admission coefficient src.a
-> admitted incoming source-color term src.rgb * src.a
```

That means this task should **not** step backward into:

- the earlier packet/export alpha ladder (`color.a`, `frag_color.a`)
- the already-fixed admission coefficient seam (`src.a`)
- the already-demoted destination-retention side (`dst.rgb * (1 - src.a)`)
- packet-local RGB composition inside the source packet

So the only honest next split is inside the already-selected admitted source-color lane itself.

## Exact source-backed merge contract

The exact fixed-function color merge remains source-backed by `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`:

```cpp
case BLEND_MODE_MIX: {
	attachment.enable_blend = true;
	attachment.alpha_blend_op = RD::BLEND_OP_ADD;
	attachment.color_blend_op = RD::BLEND_OP_ADD;
	attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
	attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
	attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
} break;
```

And the packet/export side still ends in `servers/rendering/renderer_rd/shaders/canvas.glsl` as:

```glsl
frag_color = color;
```

Under the preserved `L88` merge, that keeps the exact color-side equation fixed as:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
```

Once Task 199 has already fixed `src.rgb * src.a` as the next source-backed consequence of `src.a`, the surviving source-side names to compare are:

1. the merge-visible source-color carrier **`src.rgb`**
2. the broader admitted color product **`src.rgb * src.a`**

## Why `src.rgb` is the tighter next split

If the task must go one rung deeper than `src.rgb * src.a` without reopening packet-local RGB composition, the smallest honest continuation is the merge-visible source-color carrier **`src.rgb`** itself.

Why:

- `src.rgb` is the incoming source-color identity that the fixed-function merge is about to admit
- it is already outside packet-local RGB composition, because it is the merge-visible source-color side after shader export naming
- the admitted color product `src.rgb * src.a` cannot exist until both `src.rgb` and the already-fixed coefficient `src.a` are live
- since `src.a` has already been separately fixed as the tighter admission coefficient seam, the remaining one-rung-deeper split on the source-color side lands at the carrier **`src.rgb`** rather than the already-broader combined product

So within the already-approved source-admission framing, `src.rgb` is the tighter surviving source-color carrier and `src.rgb * src.a` is its first broader admitted product.

## Why `src.rgb * src.a` is broader at this rung

`src.rgb * src.a` remains a valid and important source-backed consequence.

But at this rung it is already composite, because it bundles together:

- the merge-visible incoming source-color carrier `src.rgb`
- the already-fixed admission coefficient `src.a`

That makes it one step broader than the carrier alone.

This is the same shape of narrowing that earlier tasks applied on the alpha side:

- first isolate the coefficient/carrier
- then demote the first full admitted composite term as broader than the carrier that feeds it

Here, the exact analog on the source-color side is:

- tighter carrier: `src.rgb`
- broader admitted product: `src.rgb * src.a`

## Exact classification

For the exact preserved `Command Graph (L88)` crash path after Task 199 fixed the source-backed continuation at `src.rgb * src.a`:

- the already-approved source-admission lane stays fixed
- the next honest one-rung-deeper split lands at the merge-visible source-color carrier **`src.rgb`**
- the broader admitted source-color product **`src.rgb * src.a`** is the first composite consequence of that carrier under the already-fixed admission coefficient `src.a`
- packet-local RGB composition remains intentionally unopened on this slice
- destination-retention remains demoted and should stay closed unless new evidence contradicts the earlier reductions

## Conclusion

After the preserved `Command Graph (L88)` merge-side admission seam is fixed at `src.a` and its next source-backed consequence is fixed at `src.rgb * src.a`, the next honest split one rung deeper should remain on the same source-color lane.

Within that framing, the tighter surviving source-backed carrier is the merge-visible incoming source color **`src.rgb`**, while the admitted product **`src.rgb * src.a`** is already the broader composite consequence of that carrier under the fixed coefficient.

Best narrow wording after this slice:

> inside the preserved `Command Graph (L88)` admitted source-color lane, the next honest one-rung-deeper split lands at the merge-visible source-color carrier `src.rgb`; the broader admitted term `src.rgb * src.a` is the first composite consequence of that carrier under the already-fixed admission coefficient `src.a`.
