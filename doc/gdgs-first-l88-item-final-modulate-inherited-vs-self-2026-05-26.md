# GDGS first-L88 cull-side item modulation: inherited cull input versus item-local `self_modulate` (2026-05-26)

## Goal

Continue exactly from Task 204's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 204 reduced the surviving CPU-side RGB identity to `p_item->final_modulate.rgb`,
- should that cull-side item modulation remain whole as `p_item->final_modulate.rgb`,
- or does the source already justify splitting one rung deeper between the inherited cull input `p_modulate.rgb` and the item-local contribution `ci->self_modulate.rgb`?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 204: `p_item->final_modulate.rgb = ci->final_modulate.rgb = p_modulate.rgb * ci->self_modulate.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory beyond this exact modulation seam, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed cull composition

The owning cull-side composition in `servers/rendering/renderer_canvas_cull.cpp` is:

```cpp
ci->final_modulate = p_modulate * ci->self_modulate;
```

The same function also shows how the inherited modulation carrier is propagated through the canvas-item tree:

```cpp
Color modulate = ci->modulate * p_modulate;
...
_cull_canvas_item(child_items[i], ..., modulate, ...);
```

And the public API/docs describe the two modulation families differently:

- `CanvasItem.modulate` / `canvas_item_set_modulate()` affects the item **and its children**
- `CanvasItem.self_modulate` / `canvas_item_set_self_modulate()` affects **only the node itself**
- `canvas_item_set_parent()` states that a child item inherits **modulation** from its parent

So on this rung:

- `p_modulate` is the already-inherited cull input arriving from the broader parent/ancestor chain
- `ci->self_modulate` is the current item's own local contribution that does not propagate onward to children

## Why the product should split one rung deeper

Unlike Task 204, there is no exact preserved-packet artifact here showing that either multiplicand is numerically identity on this packet.

But source ownership is still enough to classify the seam honestly.

`p_item->final_modulate.rgb` is already composite:

```text
p_item->final_modulate.rgb
= ci->final_modulate.rgb
= p_modulate.rgb * ci->self_modulate.rgb
```

Those two factors do different jobs:

- `p_modulate.rgb` is the broader inherited cull input
- `ci->self_modulate.rgb` is the item-local self-only multiplier for the exact canvas item that owns this preserved packet

That means the cull-side item modulation should **not** remain whole if the question is pushed one rung inward. It already splits cleanly by source semantics.

## Classification

For the preserved first-`L88` packet, the cull-side item modulation should **split one rung deeper** rather than remain whole.

It splits as:

```text
p_modulate.rgb | ci->self_modulate.rgb
```

and the source-backed role split is:

- **broader inherited side:** `p_modulate.rgb`
- **tighter item-local side:** `ci->self_modulate.rgb`

So the best narrow wording after this slice is:

> on the exact preserved first-`L88` rect packet, the surviving cull-side item modulation does not need to stay whole as `p_item->final_modulate.rgb`; source shows it is already the product of a broader inherited cull input and an item-local self-only contribution, and the tighter exact item-owned RGB factor on this rung is `ci->self_modulate.rgb` while `p_modulate.rgb` remains the broader inherited carrier.

## Why this does not widen scope

This is still a one-rung inward classification only.

It does **not** trace where the inherited `p_modulate` chain ultimately originates higher in the tree, and it does **not** introduce a new runtime contrast. It only uses:

1. the existing Task 204 stop point fixing the surviving identity at `p_item->final_modulate.rgb`
2. the cull composition in `renderer_canvas_cull.cpp`
3. the recursive cull propagation showing `p_modulate` as inherited modulation state
4. the CanvasItem / RenderingServer docs distinguishing `modulate` from `self_modulate`

That is enough to decide this split honestly without widening the lane.

## Conclusion

For the exact preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the surviving cull-side item modulation

```text
p_item->final_modulate.rgb
= p_modulate.rgb * ci->self_modulate.rgb
```

should be split one rung deeper rather than kept whole.

On this rung, the broader inherited contributor is:

```text
p_modulate.rgb
```

and the tighter exact item-local contribution is:

```text
ci->self_modulate.rgb
```

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward continuation is to stay on the exact item-local side and classify whether `ci->self_modulate.rgb` is terminal for this lane or has a still-earlier exact source identity worth tracing for the preserved packet, rather than jumping back out to the broader inherited `p_modulate.rgb` side.
