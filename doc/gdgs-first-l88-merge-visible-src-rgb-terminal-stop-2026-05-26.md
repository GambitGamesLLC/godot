# GDGS first-L88 merge-visible `src.rgb` terminal-stop note (2026-05-26)

## Goal

Continue exactly from Task 200 without reopening packet-local RGB composition and without broadening back toward the already-demoted destination-retention side.

The narrow question here is:

- once the preserved `Command Graph (L88)` source-admission lane has already been reduced to the merge-visible source-color carrier **`src.rgb`**,
- is there any still-narrower **source-backed consequence** of that carrier available under the current constraints,
- or has this lane reached a terminal stop point unless we either reopen packet-local RGB composition or broaden back out to a composite post-carrier consequence?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- packet-local alpha ladder status inherited from Task 196: fully rejoined as `a = d`
- merge-side admission seam inherited from Task 198: fixed at `src.a`
- admitted source-color continuation inherited from Task 199: fixed at `src.rgb * src.a`
- one-rung-deeper carrier split inherited from Task 200: fixed at merge-visible source-color carrier `src.rgb`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, packet-local RGB composition, or widen into a speculative fix.

## Starting point from Task 200

Task 200 already fixed the current stop point on the approved source-admission lane:

```text
src.a
-> src.rgb * src.a
-> src.rgb
```

with one important constraint:

- `src.rgb` was selected only because the task explicitly forbade reopening packet-local RGB composition

That means this task must not split `src.rgb` back into packet-local shader-side RGB ingredients.

It also must not broaden back outward into:

- the already-composite admitted product `src.rgb * src.a`
- the demoted destination-retention side `dst.rgb * (1 - src.a)`

So the only honest remaining question is whether any **still-narrower source-backed consequence** of merge-visible `src.rgb` survives under those constraints.

## Exact source-backed state still available

The relevant source-backed facts remain:

From `servers/rendering/renderer_rd/shaders/canvas.glsl`:

```glsl
frag_color = color;
```

From `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`:

```cpp
attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
```

Which keeps the live merge equation fixed as:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
```

Under this framing, the currently approved merge-visible source-color carrier is already exactly:

```text
src.rgb
```

## Why no still-narrower consequence survives under the current constraints

At this rung, any attempt to continue narrowing has only two honest directions:

1. **split inside `src.rgb` itself**
   - but that would reopen packet-local RGB composition, which this task explicitly forbids
2. **step outward into what `src.rgb` participates in**
   - but that immediately returns to the already-broader composite admitted product `src.rgb * src.a`

There is no third smaller source-backed merge-visible consequence available on the current evidence.

Why this matters:

- `src.rgb` is already the carrier-level identity on the admitted source-color side
- the next outward consequence of that carrier is the composite admitted term `src.rgb * src.a`, which Task 200 already demoted as broader at this rung
- any next inward split would require reopening the forbidden packet-local RGB composition lane

So under the current scope lock, **`src.rgb` is a terminal stop point**.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path after Task 200 fixed the one-rung-deeper source-color split at merge-visible `src.rgb`:

- there is **no still-narrower source-backed consequence** of `src.rgb` available under the current task constraints
- `src.rgb` is the terminal merge-visible source-color carrier on this approved lane
- the next outward source-backed consequence is already the broader admitted composite term `src.rgb * src.a`
- the next inward split would require reopening packet-local RGB composition, which remains explicitly out of scope on this slice

## Conclusion

Inside the preserved `Command Graph (L88)` admitted source-color lane, the merge-visible source-color carrier **`src.rgb`** is the terminal stop point under the current approved constraints.

Best narrow wording after this slice:

> inside the preserved `Command Graph (L88)` admitted source-color lane, `src.rgb` is the terminal merge-visible source-color carrier on the current approved evidence; no still-narrower source-backed consequence survives without either reopening packet-local RGB composition or broadening back out to the already-composite admitted term `src.rgb * src.a`.
