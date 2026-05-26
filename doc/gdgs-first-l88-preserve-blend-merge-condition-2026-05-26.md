# GDGS first-L88 preserve-blend merge-condition note (2026-05-26)

## Goal

Continue one rung from Task 188 while staying on the same preserved crash path and without widening into a fix.

The narrow question here is:

- once Task 188 has already classified the tightest carrier *inside* the exact `L88` merge as source-alpha color weighting,
- does the preserved crash path need only that source-alpha-driven color-weighting seam,
- or must the preserve/load dependency also return as an independently live condition at that same attachment-level merge?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live `Command Graph (L88)` preserve-content `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 188

Task 188 already classified the exact merge itself this way:

- **tightest carrier inside the merge:** source-alpha color weighting
- **broader/later consequence:** alpha writeback
- **broader prerequisite/context:** preserve/load dependency

So Task 189 does not revisit that ranking.

Instead it asks a slightly different question:

> if we keep the investigation framed as the **preserved** `L88` crash path, can preserve/load remain demoted permanently, or must it come back as an independently live condition because source-alpha color weighting alone is not enough to preserve the same exact merge family?

## Source-backed merge contract

The live canvas blend contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` remains:

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

That implies the standard merge:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
alpha_out = src.a * 1 + dst.a * (1 - src.a)
```

Task 187 and Task 188 already narrowed the packet-owned live carry reaching this boundary to exported source alpha descended from `d`.

## What source-alpha weighting alone really buys

Task 188 was correct to treat source-alpha color weighting as the **tightest seam inside the merge**, because that is the first active role played by the packet-owned carry when the merge executes.

But source-alpha weighting alone does **not** fully specify the preserved-path merge family.

Why not:

- `src.a` can weight packet color only against whatever destination participant is actually present at the merge
- the color-side term `dst.rgb * (1 - src.a)` is not just an abstract second operand; on the preserved path it specifically means **already-loaded root contents**
- likewise `dst.a * (1 - src.a)` on the alpha side presupposes that destination alpha was preserved into the merge boundary

So source-alpha weighting alone tells us *how* the packet-owned carry participates once a destination side exists, but it does **not** by itself preserve the stronger fact that this is the exact **preserve-content** merge against prior root contents.

## Why preserve/load must return as an independently live condition for the preserved path

For the exact preserved `L88` crash path, preserve/load must come back as an independently live condition.

This is the narrow source-backed reason:

1. Task 188 already proved preserve/load was too broad to be the tightest carrier *inside* the merge.
2. But the phrase **preserved path** is stronger than merely “a source-alpha blend happened.”
3. The exact preserved-path identity requires that the destination participant at the merge still be the prior root attachment contents.
4. That stronger condition is carried by the live load/preserve contract, not by source-alpha weighting alone.

In other words:

- if the question is “what is the narrowest active coefficient inside the merge?” the answer remains **source-alpha color weighting**
- but if the question is “what does this exact preserved crash path still require at that merge?” then **preserve/load dependency must return as an independently live condition**, because otherwise the destination side is no longer guaranteed to be preserved prior root contents

So preserve/load is not the tightest *carrier*, but it is still an independently live *merge condition* for the exact preserved-path framing.

## Why alpha writeback still does not become the better answer

Alpha writeback still remains downstream of that distinction.

Even after preserve/load returns as a live condition, alpha writeback is still not the best framing for this task because:

- it is a result of the merge equations already being applied
- it does not define the preserved-vs-non-preserved destination participation
- it still sits later than the first question this task is trying to answer: whether the destination-preservation contract itself must remain live alongside source-alpha weighting

So Task 189 does **not** promote alpha writeback.

## Exact classification

The honest classification after Task 188 is therefore two-level:

### Inside-the-merge tightest carrier

- still **source-alpha color weighting**

### Exact preserved-path independently live merge condition

- **preserve/load dependency must return as independently live condition**

This is not a contradiction. It is a refinement of what each phrase is responsible for:

- source-alpha color weighting = the tightest active merge role of the packet-owned carry
- preserve/load dependency = the independently live condition that keeps this the exact **preserve-content** merge family against prior root contents rather than a more generic destination term

## Why this stays inside the approved scope

This slice stays documentation-only and does not claim:

- that the correct fix is to force clear/overwrite
- that the source-alpha merge is proven root cause
- that preserve/load alone is now the primary suspect
- that a non-preserved path would necessarily be healthy

It only sharpens the seam wording so the next tiny reversible contrast, if one is authorized later, can ask the right binary question.

## Conclusion

The preserved `L88` crash path does **not** reduce all the way down to source-alpha color weighting alone.

More precisely:

- the **tightest seam inside the exact merge** remains source-alpha color weighting, exactly as Task 188 classified
- but the **exact preserved-path framing** still requires preserve/load dependency as an independently live condition, because source-alpha weighting alone does not preserve the stronger fact that the destination participant is already-loaded prior root contents

So the best next wording is:

> the live seam remains the first-L88 preserve-content `BLEND_MODE_MIX` source-alpha color merge, and the preserved-path framing still independently depends on the attachment `LOAD` / preserve contract that keeps prior root contents live at that merge.
