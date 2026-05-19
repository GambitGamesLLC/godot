# GDGS compositor instrumentation audit — 2026-05-17

## Scope

Independent auditor review of bead `oc-76q` against:

- active plan: `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- instrumentation map: `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-local-rd-compositor-instrumentation-map-2026-05-16.md`
- staged QA findings: `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- instrumentation branches in:
  - `/home/derrick/.openclaw/workspace/projects/godot`
  - `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`

## Audit verdict

The staged QA package is sufficient to support a **narrow** conclusion:

- `projection_only` is the **first failing staged GDGS workload boundary** in the current repro.
- `callback_only` and `prepared_no_dispatch` surviving makes a pure callback-entry problem, pure no-dispatch setup problem, and compositor writeback/presentation-as-first-trigger unlikely.
- the current repro seam is correctly reclassified as **global/global**, not local/global.

## What is supported

The executed order matched the intended isolation logic closely enough:

1. callback only
2. prepared / no dispatch
3. projection only
4. stop once the first failing boundary is found

The missing trivial scratch dispatch is a real gap, but it does **not** invalidate the narrower claim above. It only limits how strongly we can separate:

- "projection shader / projection-owned resources are bad"
from
- "any nontrivial compute dispatch inside this compositor path is enough to poison the device"

The observed evidence is consistent across the normal and `--accurate-breadcrumbs` reruns:

- seam-correction logs show `compositor_path_uses_global_rd=true`
- raster seam logs show `raster_path_uses_global_rd=true`
- `local_device_submit_sync_exercised=false`
- `prepared_no_dispatch` repeats without device loss
- `projection_only` reaches dispatch, barrier, and projection-end logs before later device loss at `fence_wait`
- lost-device breadcrumbs still collapse to `BLIT_PASS`

## Critical caveat

The instrumentation package included new Godot-side callback breadcrumbs, but the staged QA evidence package was gathered with the managed Godot runtime, not a source-built binary from the Godot instrumentation branch.

So this audit does **not** accept the stronger claim that the package already cleanly separates plugin misuse from engine/backend failure at the engine-owned callback seam. The QA evidence only demonstrates stage isolation on the GDGS side plus preexisting Vulkan breadcrumb output from the runtime in use.

That means:

- the QA result **does** support "projection is the first failing staged boundary"
- the QA result does **not yet** prove whether the root cause is projection misuse, a broader compute-in-callback backend hazard, or a later engine-side synchronization/lifetime issue triggered by projection output

## Hidden setup / interpretation checks

I did not find evidence that a hidden local-RD setup issue explains the result:

- the audited code paths explicitly bind `RenderingServer.get_rendering_device()` in the current repro path
- the logs agree with that setup
- the writeback path was intentionally gated off during the failing `projection_only` stage

I also did not find a stage-order mistake that would undermine the first-failure reading:

- later raster stages were correctly not run after projection became the first failure
- callback-only and no-dispatch were both exercised first

The only material setup limitation is the missing trivial scratch dispatch stage.

## Next suspect list

1. projection pass output writes or bounds assumptions
2. projection pipeline layout / push-constant contract at the observed `128` bytes
3. resource lifetime / synchronization fallout that becomes visible only after projection dispatch completes
4. broader engine/backend handling of compositor-path compute dispatches, to be tested more cleanly once a source-built Godot binary with the new engine breadcrumbs is exercised

## Bottom line

Projection-first is supported as the first failing **staged GDGS boundary**.

The stronger claim that the package already separates plugin misuse from engine/backend failure is **not** fully supported yet because the Godot-source breadcrumbs were not part of the executed QA runtime.

## Addendum — Tonemap to L88 transition audit (2026-05-19, bead `oc-xdd`)

After the later source-built runs and the `4912ce8d` Vulkan instrumentation pass, the narrower Tonemap/L88 question can now be answered cleanly.

### Addendum verdict

- `Tonemap (L87) (Draw)` still remains the **first self-owned poisoned-boundary candidate** on failing `submit_serial=9`.
- The new evidence does **not** move the seam forward to `L88`.
- What it *does* add is a precise downstream transition answer: the **first meaningful ownership expansion after Tonemap** happens exactly at `Command Graph (L88) (Draw)`.

### Why this is supported

The source change in `4912ce8d` added exactly the right measurement points:

- per-label begin/end backend-command serial capture
- per-label begin/end state capture
- a post-label unlabeled-gap tracker for the most recently closed top-level label

The corresponding QA artifact at:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/`

shows the expected zero-gap result directly:

- `tonemap_end_state` and `l88_begin_state` match at `backend_command_serial=11`
- `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,...,first_meaningful_expansion="l88_label"}`
- no breadcrumb jump to `UI_PASS` occurs in the gap

That is internally consistent with the earlier ownership evidence:

- `tonemap_local_attachment=` still shows a small self-owned local draw payload with zero descendant leakage and exact scope alignment
- `l88_local_attachment=` still shows a heavier self-owned local draw packet, also without hidden descendant leakage

### What this does **not** prove

This addendum sharpens the **downstream ownership handoff**, not the root cause. It does not prove that `L88` is harmless, and it does not disprove that a poisoned state created inside Tonemap could simply become more visible once `L88` expands the workload.

### Next tight seam to inspect

If Tonemap remains the first surviving candidate, the next backend-owned seam to split is **inside Tonemap’s own local packet**, not the Tonemap→L88 gap.

Recommended Tonemap sub-seam order:

1. `begin_render_pass`
2. Tonemap pipeline / descriptor bind setup
3. the single Tonemap draw call
4. `end_render_pass`

That is the tightest remaining backend-owned seam because the gap after Tonemap has now been shown to be empty, while `L88` has been demoted to the first downstream expansion/amplification packet.

## Addendum — Tonemap vertex-input recipe audit (2026-05-19, bead `oc-2n1`)

After commit `e33c77f9` and the refreshed QA artifact at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/`, the surviving Tonemap-vs-`L88` recipe seam can be classified more sharply.

### Addendum verdict

- `Tonemap (L87) (Draw)` still remains the **first self-owned poisoned-boundary candidate** on failing `submit_serial=9`.
- The new evidence **does** sharpen the surviving `vertex_input_recipe` contrast.
- It still does **not** collapse the remaining recipe seam to a vertex-input-only boundary.

### Why this is supported

The source change in `e33c77f9` added concrete vertex-input provenance fields directly in `render_pipeline_create()` and surfaced them in the Tonemap/L88 comparison lane:

- vertex binding / attribute counts
- total binding stride
- per-binding instance-rate mask
- attribute location / binding masks
- binding-layout / attribute-layout / attribute-format hashes
- coarse runtime class (`null_vertex_input`, `streamed_vertex_input`, `instanced_vertex_input`)

The corresponding QA artifact shows that payload faithfully:

- Tonemap serial `8` still resolves to `shader_name="TonemapShaderRD:0"`
- Tonemap’s packet is `null_vertex_input` with zero bindings, zero attributes, zero stride, zero masks, and no layout/format hashes
- the nearest meaningful compare at `Command Graph (L88) (Draw)` is a populated instanced packet with `binding_count=1`, `attribute_count=8`, `binding_stride_total=128`, `binding_input_rate_mask="0x1"`, `attribute_location_mask="0xff00"`, `attribute_binding_mask="0x1"`, and non-empty hashes
- the same compare still reports `recipe_delta.changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`

That means the new evidence advances the seam from a generic multi-bucket recipe contrast to a more specific structural read: Tonemap’s first surviving packet is a null/fullscreen-style vertex-input shape, while the downstream `L88` packet is an instanced canvas-style shape.

### Important caveat

One label in the new compare block is too coarse. The artifact reports:

- `vertex_input_delta.relation="null_vs_streamed_vertex_input"`

while also reporting:

- `other_class="instanced_vertex_input"`
- `binding_input_rate_mask="0x1"`

Source inspection shows this is a naming limitation in the relation classifier, not a contradiction in the captured runtime data. The reliable read is therefore:

- **Tonemap null vertex input vs L88 instanced vertex input**

not a literal proof that the downstream packet is non-instanced.

### What this does **not** prove

This addendum does **not** prove that vertex input alone is the toxic state. The surviving seam remains broad because three other recipe buckets still differ in the same compare:

- `blend_recipe`
- `specialization_constants`
- `pipeline_layout`

The earlier narrowing lanes also stay intact rather than reopening:

- the Tonemap→`L88` gap remains empty
- the Tonemap pipeline-bind-owned lane remains exhausted
- the Tonemap `8 -> 9` setup pair contract still reads clean

### Next tight seam to inspect

The tightest remaining backend-owned seam to split next is **`blend_recipe`**.

Reasoning:

- `vertex_input_recipe` is now structurally explained, but still not isolated causally
- `pipeline_layout` has already been partially de-risked by the clean serial-`8 -> 9` bind/setup contract
- `specialization_constants` remains a broader shader-config bucket
- `blend_recipe` is the narrowest still-unexplained fixed-function pipeline-owned delta left in the surviving Tonemap→`L88` compare

So the next pass should instrument the Tonemap/L88 blend-state contrast with the same provenance style used for vertex input and pipeline state.

## 2026-05-19 audit addendum — Tonemap blend seam after commit `8732c258`

The fresh QA artifact at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/` supports the claimed blend classification. The new compare block does advance the seam *inside* the current Tonemap-owned poisoned-boundary candidate: `blend_delta` now narrows exactly to `relation="different_attachment_recipe_same_constants"`, with `attachment_recipe_hash_changed=true`, `constant_value_hash_changed=false`, `blend_enabled_attachment_mask_changed=true`, `dynamic_blend_constants_changed=false`, and `uses_constant_factors_changed=false`. Tonemap remains the no-blend packet while the immediate `Command Graph (L88) (Draw)` neighbor remains blended with the same constant provenance but a different attachment recipe. The tracked live command state also stays clean for dynamic blend constants (`blend_constants={set=false,hash=none,last_set_serial=0,...}`), so dynamic blend-constant ownership is now demoted.

This still does **not** isolate the poisoned boundary to blend alone. The same compare remains broad at `recipe_delta.changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the already-reduced ownership lanes stay reduced (`bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",...}`). Auditor conclusion: the exact next tightest backend-owned seam is the **Tonemap -> L88 specialization-constant delta in the immediate neighboring-pass pipeline compare**. Vertex input is now structurally explained, blend is now structurally explained, and pipeline-layout risk is partly de-risked by the clean Tonemap bind/setup pair contract; specialization constants are the narrowest remaining unexplained recipe-owned lane.
