# GDGS first-L88 `a = d` bridge: first downstream non-packet-local consequence note (2026-05-26)

## Goal

Continue exactly from Task 196 without widening into a fix.

The narrow question here is:

- once the exact first-L88 packet-local `a` versus `d` split has been rejoined as `a = d`,
- what is the **first downstream consequence that is no longer packet-local**, and
- what is the tightest honest wording for that first outward step on the same preserved crash path?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact packet-local bridge inherited from Task 196: `a = d`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 196

Task 196 already closed the last honest packet-local split on this lane.

The live no-outline shader relation in `servers/rendering/renderer_rd/shaders/canvas.glsl` is still:

```glsl
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

And on this exact packet the earlier approved fact `px_size = 1.0` collapses that to:

```glsl
a = d
```

So the packet-local ladder is already rejoined. The next honest question cannot stay inside the `a`/`d` split; it has to step outward and ask where the first **non-packet-local** consequence begins.

## What still does **not** count as the first non-packet-local consequence

Immediately after the `a = d` bridge, the value still passes through a few narrower same-packet/shared-fragment carriers:

```glsl
color.a = d * color.a;
...
frag_color = color;
```

Those steps matter, but they are still not the first non-packet-local consequence.

Why not:

- `color.a` is still the packet-local/shared-fragment carrier already classified in Tasks 194–196
- `frag_color.a` is only the fragment export/writeback alias of that same packet-owned value
- neither step yet introduces a new external participant outside the packet

So the first outward consequence is **not** merely “`d` reaches `color.a`” and it is **not** merely “`d` is exported as `frag_color.a`.” Those are still transport/alias steps inside the same packet-owned fragment result.

## First downstream non-packet-local consequence

The first downstream consequence that is no longer packet-local begins when that exported alpha is consumed by the fixed-function blend stage of the live `Command Graph (L88)` UI pass.

The current source-backed blend contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` remains:

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

And the durable `REF-07` lane evidence still pins the exact pass context as:

- `owner_label="Command Graph (L88) (Draw)"`
- `breadcrumb=UI_PASS`
- `attachment_load_ops=[0:LOAD]`
- `first_blend_mode="mix"`
- `blend_enabled_attachment_mask="0x1"`

That means the first outward, non-packet-local consequence after `a = d` is:

```text
d
-> a (= d)
-> color.a
-> frag_color.a
-> source-alpha coefficient at the preserved L88 BLEND_MODE_MIX merge
```

This is the first non-packet-local step because it is the first place where a new external participant joins the computation: the already-loaded destination/root attachment contents.

## Tighter classification inside that first outward step

At that exact merge, several broader framings are true:

- the path is preserve-content because the attachment is loaded (`LOAD`)
- alpha is eventually written back too
- destination/root contents remain a live participant

But the **tightest first non-packet-local consequence** is narrower than those broader framings.

It is the exported source alpha descended from `d` becoming the active coefficient in the color merge:

```text
src.rgb * src.a + dst.rgb * (1 - src.a)
```

So the best exact wording is:

- broader context: preserved-content `L88` `BLEND_MODE_MIX` merge over loaded root contents
- tightest first non-packet-local consequence: **source-alpha-driven incoming packet-color admission at that merge**

This is tighter than calling the first outward seam merely “blend happens,” and tighter than calling it merely “destination contents are retained,” because the first active role played by the carried packet-local value is as the source-alpha coefficient that admits new packet color into the preserved merge.

## Exact classification

For the exact preserved first-L88 no-outline MSDF packet:

- Task 196 fully rejoined the packet-local ladder as **`a = d`**
- `color.a` and `frag_color.a` remain downstream carriers/aliases, but they are still packet-owned transport rather than the first non-packet-local consequence
- the **first downstream non-packet-local consequence** appears at the live `Command Graph (L88)` preserve-content `BLEND_MODE_MIX` boundary
- the tightest honest wording for that first outward step is:
  - **exported source alpha descended from `d` becomes the source-alpha coefficient for incoming packet-color admission over loaded preserved destination/root contents**

## Conclusion

Once the exact first-L88 packet-local split is rejoined as `a = d`, the first honest outward consequence is no longer another packet-local alias.

The first non-packet-local step is the fixed-function preserved `L88` blend boundary, and the tightest carrier there is the exported source alpha descended from `d` acting as the admission coefficient for incoming packet color under `BLEND_MODE_MIX`.

Best narrow wording after this slice:

> after the exact first-L88 `a = d` bridge, the first downstream non-packet-local consequence is not the export alias itself, but the exported source alpha descended from `d` becoming the `BLEND_MODE_MIX` source-alpha admission coefficient at the preserved `Command Graph (L88)` merge over loaded root contents.
