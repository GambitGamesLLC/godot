# GDGS first-L88 rect-command modulation identity versus already-cull-composed item modulation (2026-05-26)

## Goal

Continue exactly from Task 203's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` no-outline MSDF rect packet,
- once Task 203 fixed the packet-local RGB source as `rect->modulate.rgb * p_item->final_modulate.rgb -> instance_data->modulation.rgb -> attrib_C.rgb -> read_draw_data_modulation.rgb -> color_interp.rgb`,
- should that CPU-side modulation product remain the tightest honest RGB identity as a whole,
- or does the exact packet already split one rung deeper between the rect-command contribution and the already-cull-composed item contribution?

This slice stays documentation-only and source-backed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact prior stop point inherited from Task 203: `rect->modulate.rgb * p_item->final_modulate.rgb -> instance_data->modulation.rgb -> attrib_C.rgb -> read_draw_data_modulation.rgb -> color_interp.rgb`

It does **not** reopen destination-retention theory, specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes.

## Exact packet evidence: the rect-command contribution is multiplicative identity

The existing preserved first-L88 packet artifact already logs the exact rect command that feeds this packet. In the approved first-L88 command-emission and specialization-value artifacts, the matching rect command is:

```text
command={rect={x=-1.000000,y=3.000000,w=14.000000,h=16.000000},source={x=1.000000,y=1.000000,w=14.000000,h=16.000000},flags=0x1,texture=...,outline=0.000000,px_range=1.000000,modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}}
```

Representative locked-lane artifact paths:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-command-emission-boundary-vulkan-sourcebuild-20260524-234738/baseline/stdout.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-command-emission-boundary-vulkan-sourcebuild-20260524-234738/specialization_only/stdout.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/baseline/stdout.log`

So for the exact preserved packet, the rect-command-local multiplicand is not an unknown colored factor. It is the neutral multiplicative identity:

```text
rect->modulate.rgb = (1, 1, 1)
```

## Source-backed CPU composition

The owning CPU-side rect-path composition in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` is:

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

And the cull-stage item contribution in `servers/rendering/renderer_canvas_cull.cpp` is:

```cpp
ci->final_modulate = p_modulate * ci->self_modulate;
```

For this exact packet, substituting the logged rect-command identity gives:

```text
rect->modulate.rgb * p_item->final_modulate.rgb
= (1,1,1) * p_item->final_modulate.rgb
= p_item->final_modulate.rgb
```

So the broader CPU-side product does **not** remain the tightest honest identity on this slice. It cleanly splits one rung deeper, and the rect-command side immediately collapses away.

## Classification

For the preserved first-`L88` packet, the CPU-side modulation product should **not** remain whole.

It splits one rung deeper as:

```text
rect->modulate.rgb | p_item->final_modulate.rgb
```

and the exact packet evidence/source-backed result is:

- `rect->modulate.rgb` is the neutral identity `(1,1,1)` on this packet
- the surviving non-identity RGB carrier is therefore the already-cull-composed item contribution `p_item->final_modulate.rgb`

So the best narrow wording after this slice is:

> on the exact preserved first-`L88` rect packet, the CPU-side modulation product does not need to stay whole; the rect-command contribution is logged as identity white, so the tighter surviving RGB identity is the already-cull-composed item contribution `p_item->final_modulate.rgb`, with the broader product only acting as the immediate composition shell that collapses on this packet.

## Why this does not widen scope

This is still a one-rung inward classification only.

It does **not** yet reopen where `p_modulate` comes from upstream in the tree, nor does it introduce a new runtime contrast. It only uses:

1. the already-approved exact first-L88 rect-command artifact showing `rect->modulate={1,1,1,1}`
2. the already-read CPU composition in `renderer_canvas_render_rd.cpp`
3. the already-read cull composition in `renderer_canvas_cull.cpp`

That is enough to decide the split honestly for this packet.

## Conclusion

For the exact preserved first-`Command Graph (L88)` no-outline MSDF rect packet, the CPU-side modulation product

```text
rect->modulate.rgb * p_item->final_modulate.rgb
```

should be split one rung deeper rather than kept whole.

Because the packet's exact rect-command contribution is logged as identity white, the effective surviving RGB carrier on this slice is:

```text
p_item->final_modulate.rgb
```

not the broader two-factor product.

## Exact next seam now materialized

If continuation is still wanted on the same preserved lane, the next honest inward split is the cull-composed item modulation itself:

```text
p_item->final_modulate.rgb
= ci->final_modulate.rgb
= p_modulate.rgb * ci->self_modulate.rgb
```

So the next tight question would be whether the surviving first-`L88` RGB identity should stay at `p_item->final_modulate.rgb` as a whole, or split one rung deeper between the inherited cull input `p_modulate.rgb` and the item-local contribution `ci->self_modulate.rgb`.
