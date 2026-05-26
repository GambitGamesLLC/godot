# GDGS first-L88 preserve-`LOAD` consequence note (2026-05-26)

## Goal

Continue one rung from Task 189 while staying on the same preserved crash path and without widening into a fix.

The narrow question here is:

- once Task 189 restored preserve/load as an independently live co-condition for the exact preserved `L88` merge,
- is that returned `LOAD` state required only as a **destination-retention prerequisite**,
- or is there a narrower attachment-level consequence of `LOAD` itself at the exact merge that should replace that wording before any wider theory is introduced?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live `Command Graph (L88)` preserve-content `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 189

Task 189 already established the two-level read:

- **tightest active carrier inside the merge:** source-alpha color weighting
- **independently live condition for the exact preserved-path framing:** attachment `LOAD` / preserve dependency

So Task 190 does not revisit whether preserve/load comes back. It asks whether that returned condition can itself be narrowed further.

## What `LOAD` contributes at the exact merge

At the exact attachment-level merge, `LOAD` contributes one precise fact:

- the destination attachment contents that enter the blend equations are **retained prior root contents** rather than a freshly discarded/cleared/undefined destination side

That is the direct merge-relevant consequence of `LOAD`.

Under the live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`, the destination side is consumed as:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
alpha_out = src.a * 1 + dst.a * (1 - src.a)
```

So the returned `LOAD` condition matters here only by making `dst.rgb` / `dst.a` be previously retained root contents.

## Is there a narrower source-backed `LOAD` consequence than destination retention?

On the current approved evidence, no narrower honest attachment-level `LOAD` consequence survives at this exact merge.

Why:

1. Task 188 already demoted `LOAD` as the *tightest active carrier inside the merge* in favor of source-alpha weighting.
2. Task 189 already restored `LOAD` only as the independently live condition that preserves the stronger “preserve-content merge against prior root contents” framing.
3. Re-reading the exact blend equations does not expose any smaller `LOAD`-specific sideband beyond destination retention itself.
4. `LOAD` does not introduce a separate new coefficient, selector bit, or sub-channel transform at the blend boundary; it only determines whether the destination participant is retained prior content.

That means there is no narrower honest phrasing like:

- a `LOAD`-specific alpha-only side effect,
- a `LOAD`-specific color-only micro-carrier distinct from destination retention,
- or some smaller attachment-local witness emitted by `LOAD` before the broader preserve-content framing.

At this exact merge, the merge-visible consequence of `LOAD` is simply that the destination side remains the already-loaded root contents.

## Why alpha writeback and source-alpha weighting do not change this answer

This task does not promote either neighboring seam.

- **source-alpha weighting** still remains the tighter active carrier *inside* the merge, but it is not a narrower consequence *of `LOAD` itself*
- **alpha writeback** is still a downstream result of the merge, but again not a narrower `LOAD`-specific attachment-level consequence

So neither adjacent seam replaces destination retention as the honest returned role of `LOAD`.

## Exact classification

For the exact preserved `L88` merge:

- the returned preserve/load co-condition is required **purely as destination-retention prerequisite**
- there is **not** a narrower source-backed attachment-level `LOAD` consequence at this exact merge on the current approved evidence

This is narrower than Task 189's wording only in the sense that it clarifies what the preserve/load condition is actually doing:

- not a new active coefficient
- not a separate post-merge carrier
- simply the condition that keeps `dst` equal to retained prior root contents at the blend boundary

## Why this stays inside the approved scope

This slice stays documentation-only and does not claim:

- that changing `LOAD` would fix the crash
- that destination retention is the root cause
- that the correct next step is to force clear/overwrite
- that a different load-op policy is safe or correct

It only tightens the semantics of the already-restored preserve/load condition.

## Conclusion

At the exact preserved `Command Graph (L88)` attachment merge, the returned preserve/load co-condition is required **purely as destination-retention prerequisite**.

There is no narrower honest source-backed attachment-level `LOAD` consequence remaining at this exact merge on the current approved evidence.

So the best narrow wording after Task 190 is:

> the preserved `L88` crash path still requires source-alpha-driven `BLEND_MODE_MIX` color weighting, and its returned `LOAD` condition matters only by retaining prior root contents as the destination participant at that merge.
