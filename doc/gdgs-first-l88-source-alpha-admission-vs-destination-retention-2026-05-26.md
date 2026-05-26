# GDGS first-L88 source-alpha admission vs preserved-destination retention note (2026-05-26)

## Goal

Continue one rung from Task 190 while staying on the same preserved crash path and without widening into a fix.

The narrow question here is:

- after Task 190 exhausted the returned `LOAD` side as destination-retention-only context,
- where is the still-live **source-alpha-driven color-weighting** seam actually tightest at the exact preserved `Command Graph (L88)` merge,
- at **incoming packet-color admission** or at **preserved destination-retention** under the same live `BLEND_MODE_MIX` coefficient?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live `Command Graph (L88)` preserve-content `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 190

Task 190 already locked the returned preserve/load side to a narrow role:

- `attachment_load_ops=[0:LOAD]` matters only by retaining prior root contents as the destination participant
- there is no smaller honest `LOAD`-specific merge-local witness beyond that destination-retention prerequisite

So this task stays only on the surviving source-alpha side.

## Live source contract at the exact merge

The active blend contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` remains:

```text
src_color_blend_factor = SRC_ALPHA
dst_color_blend_factor = ONE_MINUS_SRC_ALPHA
src_alpha_blend_factor = ONE
dst_alpha_blend_factor = ONE_MINUS_SRC_ALPHA
```

On this exact seam, the color merge is therefore:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
```

The source packet side also stays source-backed and explicit in `servers/rendering/renderer_rd/shaders/canvas.glsl`:

- the no-outline MSDF lane computes `float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`
- then applies `color.a = a * color.a`
- and finally exports `frag_color = color`

So the same packet-local alpha descended from `d` becomes the live `SRC_ALPHA` coefficient at the exact preserved merge.

## Admission vs preserved-destination retention

Under that exact equation, the same coefficient touches both sides, but not with the same structural role.

### Incoming packet-color admission

`src.a` is the direct admission weight for new packet color:

```text
src.rgb contribution = src.rgb * src.a
```

This is the first active place where the surviving packet-local carrier decides how much new `L88` packet color is allowed into the preserved-content merge at all.

### Preserved destination-retention

The same coefficient also governs the retained destination side:

```text
dst.rgb contribution = dst.rgb * (1 - src.a)
```

But Task 190 already fixed the destination side's narrower role: it is only present because `LOAD` kept prior root contents alive. The retained destination half is therefore still real, but structurally it remains the preserved-path participant/context that the source-side coefficient acts against.

## Tightest surviving read

The still-live seam is tighter at **incoming packet-color admission** than at preserved destination-retention.

Why:

1. the source-alpha coefficient is imported from packet-local state (`d -> a -> color.a -> frag_color.a`) before the merge
2. that coefficient first becomes active as the admission weight for new packet color (`src.rgb * src.a`)
3. the destination-retention half is still necessary for the preserved-path framing, but after Task 190 it remains the broader retained participant/context rather than the tighter surviving side of the source-alpha seam
4. saying the seam is tightest at preserved destination-retention would partly collapse back into the already-exhausted `LOAD` side, which this task explicitly does not reopen

So the honest narrow wording is not merely “source alpha controls both sides.” It is:

> the surviving source-alpha seam is tightest at incoming packet-color admission into the preserved `L88` merge, while preserved destination retention remains the broader already-restored context that the same coefficient modulates.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path:

- the surviving source-alpha-driven color-weighting seam is tightest at **incoming packet-color admission**
- preserved destination-retention remains real under the same coefficient, but only as the broader retained participant/context already restored by Tasks 189-190
- this does **not** demote the preserve/load co-condition; it only says the tighter still-live source-alpha side sits on the incoming packet-color admission half of the same merge

## Conclusion

At the exact preserved `L88` `BLEND_MODE_MIX` merge, the surviving source-alpha color-weighting seam is tighter at **incoming packet-color admission** than at preserved destination-retention.

Best narrow wording after this slice:

> the preserved `L88` crash path still hinges on source-alpha-driven `BLEND_MODE_MIX` color weighting over loaded preserved destination/root contents, and within that weighting seam the tightest surviving side is the admission of incoming packet color (`src.rgb * src.a`) rather than the already-restored retained destination side.
