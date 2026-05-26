# GDGS first-L88 MSDF emitted-scalar `d` downstream blend-seam note (2026-05-26)

## Goal

Widen one rung beyond the exhausted packet-local ladder from Task 186 while staying on the same preserved crash path.

The narrow question here is:

- what is the first downstream use/consequence of the exact first-L88 emitted scalar `d` once it leaves the packet-local classification ladder, and
- where is the earliest point outside that exact packet where the preserved crash path becomes **structurally different** rather than merely carrying the same scalar through shared flow?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the already-approved seam:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact branch: the `sc_use_msdf()` no-outline rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- exact preserved outer identity: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a fix.

## Starting point from Task 186

Task 186 already exhausted the honest packet-local reduction ladder and left the exact no-outline packet at:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

For this exact packet, prior approved facts already reduce that to:

- `outline_thickness = 0.0`
- `px_size = 1.0`
- therefore `a = d`

So the last packet-local handoff is effectively:

```glsl
color.a = d * color.a;
```

Task 186 already established that `d` is the irreducible packet-local carrier. So the next honest question must step outward from the packet rather than trying to split `d` further inside the same local branch.

## First downstream path after the packet-local handoff

Once the no-outline MSDF branch rejoins shared fragment flow in `servers/rendering/renderer_rd/shaders/canvas.glsl`, the scalar no longer exists as a standalone MSDF-local symbol. It survives only as part of shared fragment alpha.

The immediate source-visible path is:

```glsl
color.a = d * color.a;          // last packet-local MSDF-owned handoff
...
vec4 base_color = color;        // first shared-flow carry
...
color *= canvas_data.canvas_modulation;   // shared modulation when not MODE_UNSHADED
...
frag_color = color;             // fragment export
```

So the first downstream consequence of `d` beyond packet-local classification is:

- `d` becomes the alpha component of shared fragment state (`color.a`)
- that shared alpha is then carried forward through the common canvas fragment tail
- and the packet finally exports it as `frag_color.a`

That is the first honest downstream trace.

## Why the early shared-flow steps are not yet the first structural difference

The shared-flow steps immediately after the packet do matter, but they are still only **carrier transport / shared modulation**:

- `vec4 base_color = color;`
- optional `color *= canvas_data.canvas_modulation;`
- eventual `frag_color = color;`

These steps do not yet introduce a new external participant. They keep the same packet-owned result inside fragment-local/shared shader flow.

So although `frag_color.a` is the first downstream exported form of `d`, the path is not yet structurally different in the sense of adding a new owner or new data dependency outside the packet. The scalar has only changed containers:

- from MSDF-local `d`
- to shared fragment `color.a`
- to exported `frag_color.a`

## Earliest structural difference outside the packet: fixed-function blend against preserved root contents

The earliest point where the preserved crash path becomes structurally different outside the packet is the **fixed-function blend stage of the live `L88` UI pass**, not the earlier shared-flow carries.

Why this is the first structural change:

1. The exported packet result stops being packet-only.
2. A new participant enters: the preexisting destination/root attachment contents.
3. The packet result is consumed under a preserve-content blend contract instead of an overwrite-only write.

That structural step is source-backed in two places.

### 1) The live `L88` pass is preserve-content UI, not overwrite-only

The existing durable QA note in `REF-07` already records the live source-built repro classification:

- `owner_label="Command Graph (L88) (Draw)" owner_level=88 breadcrumb=UI_PASS attachment_load_ops=[0:LOAD]`
- `batch_summary={rendered=10,rect_like=10,...,clipped=10,destination_color=10,blend_disabled=0,first_blend_mode="mix",first_destination_color_blend_mode="mix"}`
- pipeline provenance reports `CanvasShaderRD:0` with `blend_enabled_attachment_mask="0x1"`

That means this exact `L88` lane preserves prior root contents and blends on top of them.

The matching source-side classifier in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` says the same thing:

- `gdgs_canvas_blend_mode_uses_prior_color(BLEND_MODE_MIX)` returns `true`
- `preserve_required = !clear && (destination_color_batch_count > 0 || lcd_blend_batch_count > 0 || clip_batch_count > 0 || rendered_batch_count > 1)`
- the debug classifier names the live case `preserve_prior_root_contents`
- UI rendering begins with `draw_list_begin(..., ui_draw_flags, ..., RDD::BreadcrumbMarker::UI_PASS)`

So outside the packet, the first real attachment-level consequence is on a pass that **loads** and **preserves** prior root contents instead of replacing them wholesale.

### 2) The canvas mix blend recipe consumes source alpha directly

The same pass's blend contract is source-backed in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`:

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

That means the first downstream exported form of `d` is not just written out passively. On this live `L88` packet it immediately becomes part of the source-alpha weighting used to merge the packet with already-present root attachment contents.

So the narrow downstream trace is:

```text
d
-> a (= d on this exact packet)
-> color.a = d * inherited_color.a
-> frag_color.a
-> BLEND_MODE_MIX source-alpha weighting against loaded root contents in the L88 UI pass
```

## Exact classification

For the exact first-L88 preserved-crash no-outline MSDF packet:

- the **first downstream use/consequence** of the emitted scalar `d` beyond the exhausted packet-local ladder is that it survives as shared/exported fragment alpha (`color.a` -> `frag_color.a`)
- the **earliest point where the preserved crash path becomes structurally different outside the packet** is the live `L88` fixed-function **mix blend over preserved root contents**, because that is the first step where a new external participant (`dst` attachment contents) joins the computation

This is narrower and more honest than saying merely “it goes to framebuffer output,” because the first truly new structural fact is not export by itself; it is **export into a preserve-content source-alpha blend contract**.

## Why this does not widen into a fix

This slice only classifies the next downstream seam. It does **not** claim:

- that blend alone is root cause
- that preserving prior root contents is wrong
- that the correct fix is to disable blending, force overwrite, or change route selection

It only identifies the next narrow source-backed downstream step after packet-local `d` became irreducible.

## Conclusion

Once the exact first-L88 no-outline packet has reduced to irreducible emitted scalar `d`, the next honest outward trace is:

- packet-local `d` becomes shared/exported fragment alpha
- that exported alpha then enters the live `L88` canvas mix-blend contract
- because the live `UI_PASS` preserves prior root contents (`LOAD`) and blends with `BLEND_MODE_MIX`, the earliest structural difference outside the packet is the **attachment-level merge against destination/root contents**, not the intermediate shared shader-flow copies alone

So the next clear downstream seam after the exhausted packet-local ladder is the **source-alpha-driven preserve-content blend boundary** for the exact first-L88 packet.
