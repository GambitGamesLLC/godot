# GDGS first-L88 rect-command `rect->modulate.rgb` source identity (2026-05-26)

## Goal

Continue exactly from Task 204's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 204 split the CPU-side modulation product and identified `rect->modulate.rgb` as the tighter exact packet-owned RGB factor,
- is `rect->modulate.rgb` terminal for this lane,
- or does it have a still-earlier source identity worth tracing one rung inward?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact stop point inherited from Task 204: `rect->modulate.rgb` is the tighter exact packet-owned RGB factor inside the CPU-side product feeding `color_interp.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed answer

`rect->modulate.rgb` is **not terminal** for this lane.

The exact owning write in `servers/rendering/renderer_canvas_cull.cpp` is:

```cpp
rect->modulate = p_modulate;
```

That assignment appears on the relevant rect command constructors, including the MSDF rect path:

```cpp
void RendererCanvasCull::canvas_item_add_msdf_texture_rect_region(..., const Color &p_modulate, ...) {
	...
	rect->modulate = p_modulate;
	...
}
```

So the next honest inward source identity is simply the API/input-side modulation argument:

```text
p_modulate.rgb -> rect->modulate.rgb
```

That means `rect->modulate.rgb` is still packet-owned, but it is already an alias of an earlier source identity rather than the terminal origin.

## Why this is the right one-rung split

Task 204 already separated the packet-owned rect command factor from the broader inherited item-side factor. The next honest inward move should therefore stay on that tighter exact packet-owned side, not jump back to the broader sibling.

On that exact side, `rect->modulate.rgb` does not undergo any further composition inside `RendererCanvasCull` before being stored into the command. It is just assigned from `p_modulate.rgb`.

So the correct one-rung answer is not another product or blend; it is a direct provenance alias:

```text
p_modulate.rgb -> rect->modulate.rgb
```

## What `p_modulate` belongs to on this slice

The relevant API surfaces keep the same source identity intact.

Examples from live source:

- `CanvasItem::draw_msdf_texture_rect_region(..., const Color &p_modulate, ...)` forwards that same `p_modulate` to `RenderingServer::canvas_item_add_msdf_texture_rect_region(...)`
- `ImageTexture::draw_msdf_rect_region(..., const Color &p_modulate, ...)` likewise forwards the same `p_modulate`
- the RenderingServer interface itself exposes `canvas_item_add_msdf_texture_rect_region(..., const Color &p_modulate = Color(1, 1, 1), ...)`

So on this lane, the still-earlier source identity worth tracing is the caller-provided **MSDF draw modulation argument `p_modulate.rgb`**, not a renderer-internal recomposition.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, `rect->modulate.rgb` is **not terminal**.

Its next honest inward source identity is:

```text
p_modulate.rgb -> rect->modulate.rgb
```

Best narrow wording after this slice:

> on the preserved first-`L88` MSDF rect path, the tighter packet-owned factor `rect->modulate.rgb` is itself just the command-side alias of the caller-provided modulation argument `p_modulate.rgb`; there is no additional renderer-internal composition between those two identities.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward seam is to stay on that exact command-owned provenance path and classify the still-earlier caller-side source of **`p_modulate.rgb`** for this packet.

That continuation should remain narrow:

- trace the exact draw-call/API-side modulation source feeding `canvas_item_add_msdf_texture_rect_region(..., p_modulate, ...)`
- do **not** jump sideways to the broader inherited `p_item->final_modulate.rgb` lane or outward to merge-side consequences
