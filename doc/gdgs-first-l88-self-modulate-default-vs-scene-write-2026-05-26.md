# GDGS first-L88 `CanvasItem.self_modulate.rgb`: default white property versus identifiable non-default scene-side write (2026-05-26)

## Goal

Continue exactly from Task 206's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 206 proved that `ci->self_modulate.rgb` is only the renderer-cull copy of the scene/API-side `CanvasItem.self_modulate.rgb` property lane,
- does that preserved packet's earlier `CanvasItem.self_modulate.rgb` value simply remain the default white property,
- or does it come from an identifiable non-default `set_self_modulate(...)` / `RenderingServer::canvas_item_set_self_modulate(...)` write by a concrete scene-side caller?

This slice stays as narrow as possible and prefers source-backed classification over new runtime theory.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 206: `ci->self_modulate.rgb` is just the renderer-cull copy of `CanvasItem.self_modulate.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Fixed engine-side property/default lane

The engine-side property lane stays exactly as established in Task 206:

```cpp
Color self_modulate = Color(1, 1, 1, 1);
```

```cpp
void CanvasItem::set_self_modulate(const Color &p_self_modulate) {
	...
	if (self_modulate == p_self_modulate) {
		return;
	}
	self_modulate = p_self_modulate;
	RenderingServer::get_singleton()->canvas_item_set_self_modulate(canvas_item, self_modulate);
}
```

```cpp
void RendererCanvasCull::canvas_item_set_self_modulate(RID p_item, const Color &p_color) {
	...
	canvas_item->self_modulate = p_color;
}
```

So a non-default item-local color on this lane would need to come from a real earlier property write or API write, not from an unexplained renderer-side mutation.

## Reproducer-side source scan

Within the GDGS reproducer project used for this preserved lane, the scene-authored UI text path is extremely small:

- `scenes/gdgs_happy_path_control.tscn` contains one scene-authored text node on the UI path:
  - `CanvasLayer/HudMargin/HudLabel`
  - type: `RichTextLabel`
- `scripts/build_control_scene.gd` creates that same `HudLabel` node programmatically when regenerating the scene
- `scripts/gdgs_tweak_matrix_harness.gd` updates only the label text content

Crucially, a project-wide scan over `*.gd`, `*.tscn`, and `*.cs` in `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/` finds **no** occurrences of:

- `self_modulate`
- `set_self_modulate`
- `canvas_item_set_self_modulate`

The serialized scene entry for `HudLabel` likewise does **not** store a `self_modulate = ...` override, and the scene-builder script does **not** call `info.self_modulate = ...` or `info.set_self_modulate(...)`.

That means the reproducer project contributes:

- no explicit serialized non-default `CanvasItem.self_modulate`
- no script-side non-default `set_self_modulate(...)`
- no direct `RenderingServer.canvas_item_set_self_modulate(...)` write

## Packet-side corroboration

The preserved packet artifact itself still logs a white modulation payload:

```text
modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}
```

for the exact first clipped preserve-rect packet.

That packet field is broader than `self_modulate` alone because the rect path later carries the cull-composed modulation product, so this line by itself does **not** prove ownership or isolate `self_modulate` independently.

But it does remain fully consistent with the source scan above: nothing in the reproducer project is trying to author a non-default item-local color here.

## Classification

For this preserved lane, the best narrow classification is:

- the earlier source identity remains `CanvasItem.self_modulate.rgb`
- in the reproducer project, there is **no identifiable non-default scene-side write** to that property path
- therefore the preserved packet's item-local `CanvasItem.self_modulate.rgb` is best classified as simply retaining the default white property value

In short:

```text
CanvasItem.self_modulate.rgb = Color(1, 1, 1, 1).rgb
```

for this lane, with no concrete project-side caller found that changes it.

## Ownership caveat kept narrow

This slice still avoids over-claiming more than the evidence supports.

The current source corpus is enough to classify the item-local property value as default white, but it does **not** strictly prove whether the exact first clipped packet belongs to:

- the `HudLabel` CanvasItem directly, or
- a RichTextLabel-internal canvas item used while laying out/drawing that same label

That distinction does not change the classification reached here, because the project still contains no non-default `self_modulate` authoring path on either route.

## Conclusion

For the exact preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the item-local scene/API-side property lane

```text
CanvasItem.self_modulate.rgb
```

is best classified as **default white**, not as the product of an identifiable non-default scene-side `set_self_modulate(...)` / RenderingServer write.

The reason is source-backed and narrow:

- `CanvasItem.self_modulate` defaults to white in engine source
- the reproducer project contains no serialized `self_modulate` override
- the reproducer project contains no script-side `set_self_modulate(...)` or RenderingServer self-modulate write
- the preserved packet's logged modulation payload stays white, which is consistent with that default-property reading

## Exact next seam now materialized

If continuation is still wanted on this same item-local side, the next honest seam is **not** another default-vs-write theory expansion.

The remaining narrow seam would instead be a stricter ownership proof for the exact preserved packet — i.e. emit or recover the owning CanvasItem/RID/path for the first clipped preserve-rect packet to decide whether it is the `HudLabel` CanvasItem directly or a RichTextLabel-internal item on the same label path.

That seam would strengthen owner identity only; it is not required to classify the current `self_modulate` value as default white.
