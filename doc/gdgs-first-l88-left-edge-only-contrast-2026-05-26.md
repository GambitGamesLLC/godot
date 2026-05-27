# GDGS first-L88 heading `G` packet: left-edge-only non-containment contrast on the preserved route (2026-05-26)

## Goal

Continue exactly from Task 219's explicit next seam without widening scope.

The narrowed question here is:

- for the same preserved first-`Command Graph (L88)` clipped preserve-rect lane,
- now that both sides of the exact left-edge fact are fixed (`G` packet global `(15,19,14,16)` vs owner clip origin `(16,16)`),
- does the clipped preserve-rect classification / crash identity actually depend on that **1 px left-edge-only partial overlap**, or does that overlap merely coexist with the real surviving seam?

This slice stays on the same preserved crash path and uses the smallest reversible contrast I could find: a runtime-only 1 px left content-margin override on `HudLabel`'s `normal` stylebox so the first heading `G` draw anchor moves right by exactly 1 px while the clip owner stays fixed.

## Scope lock

This note stays on the same already-approved preserved crash path:

- exact packet identity already fixed: first preserved clipped white no-outline MSDF rect packet = first shaped heading `G` packet
- exact glyph-local cause already fixed: the packet's original `x=-1` came from the glyph-local MSDF left bearing, not from upstream layout drift
- exact owner clip origin already fixed: direct `HudLabel` clip rect begins at global `(16,16)`
- exact overlap fact already fixed: baseline packet global `(15,19,14,16)` overlapped the owner clip only partially on the left edge

It does **not** reopen broader provenance links, caller-family identity, owner identity, font selection, destination-retention theory, specialization-cache theory, request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or turn into a broad fix attempt.

## Minimal reversible contrast

The contrast stayed runtime-only. I copied the existing staged-case harness and added one tiny override before attaching the scene root:

```gdscript
var label := scene_root.get_node_or_null("CanvasLayer/HudMargin/HudLabel") as RichTextLabel
var base := label.get_theme_stylebox("normal")
var override_style: StyleBox = base != null ? base.duplicate() : StyleBoxEmpty.new()
var current_left := base != null ? maxf(0.0, base.get_content_margin(SIDE_LEFT)) : 0.0
override_style.set_content_margin(SIDE_LEFT, current_left + 1.0)
label.add_theme_stylebox_override("normal", override_style)
```

That leaves the clip owner path alone while shifting the text content anchor right by exactly 1 px inside the same `HudLabel`.

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/official-first-l88-left-edge-contrast-vulkan-sourcebuild-20260526-201400/`

Harnesses used:

- baseline: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- contrast: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/run_stage_case_checkpoint_left_margin_1px.gd`
- runner: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/run_first_l88_left_edge_contrast.py`

## What changed — and what did not

Baseline first traced clipped batch:

```text
clip_rect={x=16,y=16,w=504,h=460}
command.rect={x=-1,y=3,w=14,h=16}
instance_count=27
matched=10 skipped=9
```

Contrast first traced clipped batch:

```text
clip_rect={x=16,y=16,w=504,h=460}
command.rect={x=0,y=3,w=14,h=16}
instance_count=27
matched=10 skipped=9
```

So the intended local contrast succeeded exactly:

- the owner clip stayed fixed at `(16,16,504,460)`
- the first traced `G` packet moved from local `x=-1` to local `x=0`
- that removes the packet's left-edge-only non-containment against the same clip edge
- the rest of the immediate packet shape stayed fixed (`y=3`, `w=14`, `h=16`)

At global coordinates, the packet therefore moved from:

```text
baseline global packet = (15,19,14,16)
```

to:

```text
contrast global packet = (16,19,14,16)
```

which is now fully left-edge-contained by the same owner clip.

## Outer crash identity stayed unchanged

Despite removing that exact left-edge-only non-containment, the preserved failing lane stayed the same in both fresh launches:

- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- command-summary tail still includes `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumb still reaches `BLIT_PASS`
- `temp_diag_clipped_preserve_rect_gate_result={matched=10,skipped=9}` still holds
- process exit remains `134` / signal `6` (`subprocess` return `-6`)

The pre-draw prerequisite marker also stayed semantically the same shape across both runs:

- same `setup_stage=pre_l88_draw`
- same clip/scissor rect `(16,16,504,460)`
- same `shader_variant=quad`, `render_primitive=triangles`, `vertex_format=2`, `instance_count=27`
- only run-local resource IDs changed, which is normal across fresh launches

## Tight interpretation

This is the honest one-rung conclusion from the minimal contrast:

```text
The first heading G packet's 1 px left-edge-only partial overlap is not required for either
(1) the preserved clipped preserve-rect family classification on this lane, or
(2) the surviving submit_serial=9 / Tonemap(L87)->L88 crash identity.
```

Removing just that 1 px non-containment changed the packet geometry exactly as intended, but **did not** change:

- the matched/skipped preserve-rect gate outcome
- the first traced batch ordinal / shape family
- the pre-draw setup-stage family
- or the outer crash signature

So the left-edge-only partial overlap is best classified now as a **coexisting packet-local fact**, not the dependency that explains why this lane is still the preserved clipped-family / crash lane.

## What this implies about the next honest seam

Because the first traced `G` packet became left-edge-contained while the lane still reported the same clipped preserve-rect family outcome (`matched=10, skipped=9`), the surviving seam is probably **not** "does this one `G` packet overhang left by 1 px?"

The next honest adjacent seam, if continuation is wanted, is narrower and different:

- decide whether the preserved clipped-family classification is fundamentally **batch-level** rather than this single traced packet's containment fact, and
- if so, identify what other instance / packet / batch-level condition inside that same first traced `instance_count=27` family still keeps the lane in the same clipped preserve-rect bucket after the first `G` packet itself becomes left-edge-contained.

## Conclusion

The contrast succeeded exactly as designed:

- same owner clip
- same preserved first-`L88` crash lane
- same gate result and crash identity
- first traced `G` packet moved from `x=-1` to `x=0`

Therefore the exact 1 px left-edge-only non-containment of the first heading `G` packet is **not the dependency** behind the preserved clipped preserve-rect classification or crash identity on this lane. It coexists with them, but removing it alone does not collapse either one.
