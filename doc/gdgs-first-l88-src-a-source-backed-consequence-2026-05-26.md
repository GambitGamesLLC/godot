# GDGS first-L88 `src.a` source-backed consequence note (2026-05-26)

## Goal

Continue exactly from Task 198 without reopening the packet-local/export ladder and without broadening back to the already-demoted destination-retention side.

The narrow question here is:

- once the tightest surviving **non-packet-local** source-alpha admission seam at the preserved `Command Graph (L88)` merge has already been fixed at the merge-side coefficient **`src.a`**,
- what is the **next narrower source-backed consequence** of that coefficient inside the same preserved merge framing,
- and is that next consequence best located at the admitted source-color contribution **`src.rgb * src.a`** or at the source-side alpha writeback contribution **`src.a * 1`**?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- packet-local ladder status inherited from Task 196: fully rejoined as `a = d`
- merge-side admission seam inherited from Task 198: fixed at the immediate blend-side coefficient `src.a`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 198

Task 198 already settled the merge-local narrowing:

```text
d
-> a (= d)
-> color.a
-> frag_color.a
-> preserved L88 BLEND_MODE_MIX source-alpha admission coefficient src.a
```

That means this task should **not** step backward into `color.a`, `frag_color.a`, or the earlier packet/export carrier framing.

It also should **not** broaden back to the already-demoted destination-retention side:

```text
dst.rgb * (1 - src.a)
dst.a * (1 - src.a)
```

So the only honest next split is among the **source-backed consequences of `src.a` itself** inside the same fixed-function merge.

## Exact source-backed merge contract

The exact fixed-function color/alpha merge remains source-backed by `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`:

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

Under the preserved `L88` merge, that yields the same live equations already pinned in prior tasks:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
alpha_out = src.a * 1 + dst.a * (1 - src.a)
```

Once destination-retention is held demoted, the surviving source-backed consequences of `src.a` are:

1. the admitted **source-color** contribution `src.rgb * src.a`
2. the source-side **alpha writeback** contribution `src.a * 1`

## Why `src.rgb * src.a` is the next narrower source-backed consequence

`src.a` was already classified as the immediate admission coefficient.

The next honest continuation must therefore ask: **what is the first thing that coefficient actively admits on the source side?**

That answer is the source-color contribution:

- `src.a` is the coefficient
- `src.rgb` is the incoming packet color being admitted
- `src.rgb * src.a` is the first full source-backed merge consequence that actually realizes that admission role

This is the right continuation because it stays on the same already-approved source-admission lane:

- it remains entirely on the **source side** of the merge
- it preserves the previously locked wording that `src.a` is the admission coefficient for incoming packet color
- it does not reopen the earlier packet/export ladder
- it does not broaden back to the destination-retention half

So after `src.a`, the next narrower source-backed consequence is the admitted incoming source-color term **`src.rgb * src.a`**.

## Why `src.a * 1` is not the tighter continuation here

`src.a * 1` is real, but it is not the tightest next continuation for the currently locked seam.

Why:

- it belongs to the **alpha equation** (`alpha_out`) rather than the already-selected source-color admission lane
- it is a source-side post-merge stored-alpha consequence, not the first source-color thing the admission coefficient lets into the preserved merge
- Task 188 already demoted alpha writeback relative to the tighter source-alpha color-weighting seam at the exact merge boundary

So `src.a * 1` remains a valid sibling downstream effect of the same coefficient, but it is not the best next narrowing if we stay faithful to the already-approved source-admission framing.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path after Task 198 fixed the merge-side admission seam at `src.a`:

- the current seam remains the immediate non-packet-local source-alpha admission coefficient `src.a`
- the **next narrower source-backed consequence** of that coefficient is the admitted incoming source-color term **`src.rgb * src.a`**
- the source-side alpha term **`src.a * 1`** remains real but is the broader sibling post-merge alpha consequence rather than the tightest continuation on the already-selected source-admission lane
- destination-retention terms stay demoted and should not be reopened on this slice

## Conclusion

After the exact first-L88 `a = d` rejoin and after Task 198 fixed the tightest non-packet-local admission seam at `src.a`, the next honest continuation should remain on the same source-backed merge lane.

Within that framing, the next narrower source-backed consequence of `src.a` is **`src.rgb * src.a`** — the first admitted incoming source-color contribution.

Best narrow wording after this slice:

> after the preserved `Command Graph (L88)` merge-side admission seam is fixed at `src.a`, the next narrower source-backed consequence is the admitted incoming source-color term `src.rgb * src.a`; the source-side alpha term `src.a * 1` remains a real sibling post-merge effect, but it is not the tightest continuation on the already-selected source-admission lane.
