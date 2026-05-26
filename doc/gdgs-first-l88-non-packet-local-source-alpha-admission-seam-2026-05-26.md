# GDGS first-L88 non-packet-local source-alpha admission seam note (2026-05-26)

## Goal

Continue exactly from Task 197 without reopening the rejoined packet-local `a = d` ladder.

The narrow question here is:

- once Task 197 has already fixed the **first downstream non-packet-local consequence** as source-alpha-driven incoming packet-color admission at the preserved `Command Graph (L88)` merge,
- what is the **tightest surviving non-packet-local admission seam** inside that same preserved merge framing,
- and is it best located at the immediate blend-side admission coefficient `src.a` or only one step later at the broader admitted incoming color term `src.rgb * src.a`?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- packet-local ladder status inherited from Task 196: fully rejoined as `a = d`
- first outward step inherited from Task 197: exported source alpha descended from `d` becomes the admission coefficient at the preserved merge

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 197

Task 197 already settled the first outward step after the packet-local rejoin:

```text
d
-> a (= d)
-> color.a
-> frag_color.a
-> preserved L88 BLEND_MODE_MIX source-alpha admission coefficient
```

That means this task should **not** step back down into `color.a`, `frag_color.a`, or the earlier packet-local/export ladder. Those remain necessary provenance, but they are no longer the right seam if the question is explicitly the **surviving non-packet-local admission seam**.

So the next honest split must stay inside the preserved merge framing itself.

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

Under the preserved `L88` merge, that leaves two remaining admission-side names to compare:

1. the immediate blend-side consumed coefficient **`src.a`**
2. the broader admitted incoming color term **`src.rgb * src.a`**

## Why `src.a` is the tightest surviving non-packet-local seam

Once we require the seam to be **non-packet-local**, `frag_color.a` is already one step too early because it is still the shader-export identity at the packet boundary.

The first honest non-packet-local admission-side carrier is therefore the immediate fixed-function consumer coefficient:

- **`src.a`** is the first value identity that exists only inside the merge contract rather than inside packet/export naming
- it is the coefficient that directly gates how much incoming packet color may enter the preserved destination merge
- the later admitted color term `src.rgb * src.a` cannot exist until `src.a` is already live as the admission coefficient

So within the exact preserved merge framing, the tightest surviving non-packet-local admission seam is **not** the broader full incoming color term. It is the immediate blend-side source-alpha coefficient itself.

## Why `src.rgb * src.a` is broader

`src.rgb * src.a` is still the first full incoming color contribution, but it is already composite.

It bundles together:

- the packet RGB payload (`src.rgb`)
- the already-live admission coefficient (`src.a`)

That makes it one step later and structurally broader than the coefficient alone.

This is the same kind of narrowing Task 192 previously applied on the broader admission lane, but with one important scope correction: now that the packet-local ladder has been explicitly rejoined and the task is restricted to the **non-packet-local** seam, the tightest honest surviving carrier is the **merge-side `src.a` coefficient**, not the earlier shader-export name and not the later full color term.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path after the packet-local `a = d` rejoin:

- the first outward non-packet-local consequence remains source-alpha-driven incoming packet-color admission at the preserved merge
- inside that same admission framing, the **tightest surviving non-packet-local seam** is the immediate blend-side coefficient **`src.a`**
- the broader admitted incoming color term **`src.rgb * src.a`** is the first composite consequence of that tighter non-packet-local coefficient
- packet-local/export-side names (`color.a`, `frag_color.a`) remain true provenance, but they are no longer the right seam once the question is constrained to the non-packet-local merge side

## Conclusion

After the exact first-L88 `a = d` rejoin is complete, the honest next narrowing should stay inside the preserved `Command Graph (L88)` merge framing rather than reopening the packet-local ladder.

Within that non-packet-local admission framing, the tightest surviving seam is the immediate blend-side source-alpha coefficient **`src.a`**, while `src.rgb * src.a` is already the broader admitted incoming color product.

Best narrow wording after this slice:

> after the exact first-L88 `a = d` rejoin, the tightest surviving **non-packet-local** source-alpha admission seam at the preserved `Command Graph (L88)` merge is the immediate blend-side admission coefficient `src.a`; the later admitted incoming color term `src.rgb * src.a` is the first broader composite consequence of that coefficient.
