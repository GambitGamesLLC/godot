# GDGS first-L88 item-local `ci->self_modulate.rgb`: terminal renderer leaf versus still-earlier caller/property source (2026-05-26)

## Goal

Continue exactly from Task 205's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 205 split the surviving cull-side item modulation into the broader inherited `p_modulate.rgb` side and the tighter item-local `ci->self_modulate.rgb` side,
- is `ci->self_modulate.rgb` terminal for this lane,
- or does it still have a source-backed earlier identity worth tracing one rung inward?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 205: `p_item->final_modulate.rgb = p_modulate.rgb * ci->self_modulate.rgb`, with `ci->self_modulate.rgb` already identified as the tighter exact item-local side

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed provenance chain

On the renderer-cull side, the exact contribution used on this lane is:

```cpp
ci->final_modulate = p_modulate * ci->self_modulate;
```

So the item-local factor currently under test is the `RendererCanvasCull::Item::self_modulate` field.

But that renderer-cull field is **not** authored in place as a terminal origin. The exact write into it is:

```cpp
void RendererCanvasCull::canvas_item_set_self_modulate(RID p_item, const Color &p_color) {
	...
	canvas_item->self_modulate = p_color;
}
```

That setter is itself just the RenderingServer-side alias of the public CanvasItem property:

```cpp
void CanvasItem::set_self_modulate(const Color &p_self_modulate) {
	...
	self_modulate = p_self_modulate;
	RenderingServer::get_singleton()->canvas_item_set_self_modulate(canvas_item, self_modulate);
}
```

And the owning scene-side property storage is declared directly on `CanvasItem` as:

```cpp
Color self_modulate = Color(1, 1, 1, 1);
```

The docs preserve the same identity boundary:

- `CanvasItem.self_modulate` is the color applied to this `CanvasItem`, affecting only the node itself
- `RenderingServer.canvas_item_set_self_modulate()` is explicitly equivalent to `CanvasItem.self_modulate`

So the exact one-rung-earlier source chain is:

```text
CanvasItem::self_modulate / set_self_modulate(p_self_modulate)
-> RenderingServer::canvas_item_set_self_modulate(..., p_self_modulate)
-> RendererCanvasCull::canvas_item_set_self_modulate(..., p_color)
-> ci->self_modulate
```

## Classification

For this lane, `ci->self_modulate.rgb` is **not terminal for the whole source path**.

It is terminal only as the last renderer-cull storage slot before composition into `ci->final_modulate`, but it already has a still-earlier exact source identity worth tracing:

```text
CanvasItem.self_modulate.rgb
```

more precisely, the caller/property value passed as:

```text
p_self_modulate.rgb -> canvas_item_set_self_modulate(..., p_color) -> ci->self_modulate.rgb
```

That means the tightest honest wording after this slice is:

> on the preserved first-`L88` lane, the item-local cull factor `ci->self_modulate.rgb` is not the terminal origin; it is the renderer-cull copy of the scene/API-side `CanvasItem.self_modulate` property value, forwarded through `CanvasItem::set_self_modulate()` / `RenderingServer::canvas_item_set_self_modulate()`.

## Why this does not over-claim packet ownership

This slice does **not** claim we already know the exact gameplay/UI callsite or scene-file write that set the preserved packet owner's `CanvasItem.self_modulate` value.

Source is enough to classify the next provenance rung honestly:

- the renderer-side field is only a copied sink
- the still-earlier exact identity is the scene/API-side `CanvasItem.self_modulate` property value

Anything beyond that — for example, the exact node/caller/scene file that authored the preserved packet owner's property, or whether it simply stayed at the default white `Color(1, 1, 1, 1)` — is a **further** seam and would require identifying the packet owner first.

## Conclusion

For the exact preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the tighter item-local factor

```text
ci->self_modulate.rgb
```

is **not terminal** for the lane's full provenance.

Its still-earlier exact source identity is the scene/API-side CanvasItem property lane:

```text
CanvasItem.self_modulate.rgb
-> RenderingServer::canvas_item_set_self_modulate(...)
-> ci->self_modulate.rgb
```

So the best narrow classification is:

- terminal as the last renderer-cull storage slot before composition: **yes**
- terminal as the lane's earlier source origin: **no**
- next traceable exact identity: **`CanvasItem.self_modulate.rgb` / `set_self_modulate(p_self_modulate)`**

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward continuation is to stay on this item-local side and classify the still-earlier owner/caller of the preserved packet's `CanvasItem.self_modulate.rgb` value — i.e. whether the owning CanvasItem simply retains the default white property or receives a specific non-default `set_self_modulate(...)` / RenderingServer write from an identifiable scene-side caller.

That continuation should remain narrow:

- identify the preserved packet's owning CanvasItem if the current artifact set makes that possible
- then classify whether its `CanvasItem.self_modulate` is just the default property value or is set by a specific caller
- do **not** jump back out to the broader inherited `p_modulate.rgb` side or outward to merge-side consequences
