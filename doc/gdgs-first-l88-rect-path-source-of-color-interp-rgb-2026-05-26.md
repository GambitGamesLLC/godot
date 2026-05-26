# GDGS first-L88 rect-path source of packet-local `color_interp.rgb` (2026-05-26)

## Goal

Continue exactly from Task 202's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 202 fixed the packet-local RGB ladder as `color_interp.rgb -> color.rgb -> frag_color.rgb -> src.rgb`,
- what exact rect-path source populates `color_interp.rgb` itself?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact packet-local stop point inherited from Task 202: `color_interp.rgb -> color.rgb -> frag_color.rgb -> src.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Source-backed rect-path trace

The live rect-path source chain in `servers/rendering/renderer_rd/shaders/canvas.glsl` is:

```glsl
vec4 color = read_draw_data_modulation;
...
color_interp = color;
```

On the non-attribute rect path in that same shader, `read_draw_data_modulation` is defined as:

```glsl
#define read_draw_data_modulation attrib_C
```

So for this packet, `color_interp.rgb` is populated directly from the rect instance-buffer payload carried in `attrib_C.rgb`.

The owning CPU-side rect-path write in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` is:

```cpp
Color base_color = p_item->final_modulate;
...
Color modulated = rect->modulate * base_color;
...
instance_data->modulation[0] = modulated.r;
instance_data->modulation[1] = modulated.g;
instance_data->modulation[2] = modulated.b;
instance_data->modulation[3] = modulated.a;
```

And the matching rect instance layout in `servers/rendering/renderer_rd/renderer_canvas_render_rd.h` stores that payload as:

```cpp
struct InstanceData {
	...
	float modulation[4];
	...
};
```

So the exact rect-path population chain for the packet-local RGB carrier is:

```text
rect->modulate.rgb * p_item->final_modulate.rgb
-> modulated.rgb
-> instance_data->modulation[0..2]
-> attrib_C.rgb
-> read_draw_data_modulation.rgb
-> color.rgb
-> color_interp.rgb
```

Because the rect path writes one modulation vec4 per instance and then assigns `color_interp = color` before the fragment stage, `color_interp.rgb` is **not** sourced from sampled MSDF texture RGB and is **not** synthesized later inside the fragment. It is the rect-instance modulation payload already composed on the CPU side.

## What `p_item->final_modulate` means on this slice

The only additional source fact needed here is where `base_color` comes from.

`servers/rendering/renderer_canvas_cull.cpp` sets:

```cpp
ci->final_modulate = p_modulate * ci->self_modulate;
```

So on this exact slice, the rect-path RGB source can be stated precisely without tracing farther up the scene tree:

```text
color_interp.rgb
= rect-instance modulation rgb
= rect->modulate.rgb * p_item->final_modulate.rgb
```

with `p_item->final_modulate` already being the cull-stage item modulation product.

That is the exact rect-path source classification needed for this rung. Going farther inward than `p_item->final_modulate` would be a new seam, not part of this one-rung continuation.

## Conclusion

For the preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the exact rect-path source that populates packet-local `color_interp.rgb` is the rect instance modulation payload already composed on the CPU side:

```text
rect->modulate.rgb * p_item->final_modulate.rgb
-> instance_data->modulation.rgb
-> attrib_C.rgb
-> read_draw_data_modulation.rgb
-> color_interp.rgb
```

Best narrow wording after this slice:

> on the preserved first-`L88` rect path, packet-local `color_interp.rgb` is populated from the rect instance modulation payload, not from sampled MSDF RGB; concretely it comes from `rect->modulate.rgb * p_item->final_modulate.rgb`, written into `InstanceData.modulation.rgb` and consumed by the non-attribute rect shader path as `attrib_C.rgb` / `read_draw_data_modulation.rgb`.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward split is no longer inside the shader packet. It is the CPU-side modulation composition itself:

```text
rect->modulate.rgb * p_item->final_modulate.rgb
```

The next tight question would be whether the preserved packet's first meaningful RGB identity should stay at that product as a whole, or split one rung deeper between the rect command's own `rect->modulate.rgb` contribution and the already-cull-composed `p_item->final_modulate.rgb` contribution.
