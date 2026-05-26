# GDGS first-L88 split of the rect-path modulation product feeding `color_interp.rgb` (2026-05-26)

## Goal

Continue exactly from Task 203's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 203 fixed the rect-path source of packet-local `color_interp.rgb` as `rect->modulate.rgb * p_item->final_modulate.rgb -> instance_data->modulation.rgb -> attrib_C.rgb -> read_draw_data_modulation.rgb -> color_interp.rgb`,
- should the tightest surviving RGB identity remain at that CPU-side product as a whole,
- or split one rung deeper between the exact rect command contribution and the already-cull-composed item contribution?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 203: packet-local `color_interp.rgb` is populated from the rect-path modulation payload written into `InstanceData.modulation`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed split of the CPU-side modulation product

The owning producer in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` is:

```cpp
Color base_color = p_item->final_modulate;
...
Color modulated = rect->modulate * base_color;
```

So the producer feeding `InstanceData.modulation.rgb` is already a **two-factor product**, not an irreducible source identity.

Task 203 stopped honestly at that product because it was the exact rect-path source written into the instance payload. But once the question moves one rung inward, the product itself is already composite and should not remain the terminal identity for this narrower slice.

The two source-backed factors are:

```text
rect->modulate.rgb
p_item->final_modulate.rgb
```

Those factors are not equivalent in scope.

## Exact rect command contribution

`servers/rendering/renderer_canvas_cull.cpp` creates the exact rect packet with:

```cpp
rect->modulate = p_modulate;
```

for both ordinary texture rects and MSDF texture rect-region commands.

That means `rect->modulate.rgb` is the **exact command-local RGB contribution** carried by this specific rect command before the renderer-side product is formed.

It is the narrower factor on this rung because it belongs to the exact packet command itself, not to the broader already-cull-composed item state.

## Already-cull-composed item contribution

The sibling factor is:

```cpp
ci->final_modulate = p_modulate * ci->self_modulate;
```

from `servers/rendering/renderer_canvas_cull.cpp`, later observed in the render path as:

```cpp
Color base_color = p_item->final_modulate;
```

So `p_item->final_modulate.rgb` is already a broader inherited item modulation carrier, not a command-local value authored directly by the rect packet itself.

On this slice it is still live and still part of the exact source product, but it is the broader sibling factor because it arrives pre-composed from the cull stage and is then reused by the rect-path renderer.

## Classification for this rung

Therefore the tightest surviving RGB identity should **not** remain at the whole product

```text
rect->modulate.rgb * p_item->final_modulate.rgb
```

once we are allowed to move one rung inward.

The honest source-backed split is:

```text
rect->modulate.rgb   ×   p_item->final_modulate.rgb
```

And within that split:

- **narrower exact rect-command contribution:** `rect->modulate.rgb`
- **broader inherited item-side sibling:** `p_item->final_modulate.rgb`

So the product is still the immediate CPU-side producer of `InstanceData.modulation.rgb`, but it is no longer the tightest identity after this one-rung-deeper classification. The tighter exact packet-owned factor is the rect command's own modulation contribution.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the CPU-side modulation product feeding `color_interp.rgb` should **split one rung deeper** rather than remain whole.

Best narrow wording after this slice:

> on the preserved first-`L88` rect path, the source product `rect->modulate.rgb * p_item->final_modulate.rgb` is already composite; the next honest inward split separates the exact rect command contribution `rect->modulate.rgb` from the broader already-cull-composed item contribution `p_item->final_modulate.rgb`, with `rect->modulate.rgb` being the tighter exact packet-owned RGB factor on this rung.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam should stay narrow and explicit:

- either continue on the **exact rect-command-owned side** by classifying whether `rect->modulate.rgb` is terminal for this lane or has a still-earlier source identity worth tracing,
- or explicitly choose the broader inherited item side and trace `p_item->final_modulate.rgb` one rung deeper.

Under the current narrowing logic, the tighter continuation is the first option: stay on the exact rect-command-owned contribution rather than jump outward to the broader inherited item modulation side.
