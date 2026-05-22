# 2026-05-22 — frame-1 non-present submit seam classification

## Scope

Task 144 follow-up on the source-built host-Vulkan investigation. Keep the question strictly inside the earlier frame-1 non-present `_execute_frame(false)` submit that the failing lane dies in before any screen-blit/present work begins.

## Artifacts reused

- Failing source-built handoff artifact:
  - `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-failing-vulkan-sourcebuild-20260522-1715/`
- Healthy source-built control artifact:
  - `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-control-vulkan-sourcebuild-20260522-1716/`

These already contain the smallest honest non-present-submit diagnostic payload needed for this fork, so no extra rerun was required.

## Exact failing-vs-healthy difference before present-side work starts

### Healthy control
- Frame-1 non-present submit happens at `submit_serial=7`.
- It waits on one transfer-worker semaphore, completes successfully, and its `command_summary` is still small:
  - `labels=17`
  - `first_label="Command Graph (L-1)"`
  - `last_label="Command Graph (L8) (Draw)"`
  - `last_breadcrumb="UI_PASS"`
- After that, the control survives long enough to reach `draw_list_begin_for_screen frame=1` and then `swap_buffers_begin frame=1`, after which later submits move onto the `BLIT_PASS`/present lane.

### Failing lane
- Frame-1 non-present submit happens later at `submit_serial=9`.
- It also waits on one transfer-worker semaphore, but its `command_summary` is much larger **before any present work exists**:
  - `labels=102`
  - `first_label="Command Graph (L-1)"`
  - `last_label="Command Graph (L88) (Draw)"`
  - `label_tail` reaches `Tonemap (L87) (Draw) > Command Graph (L88) (Draw)`
  - `last_breadcrumb="UI_PASS"`
- The device loss then occurs while waiting on that same non-present submit:
  - `fence_wait_error submit_serial=9 wait_result=-4`

## What still distinguishes the failing non-present submit internally

The healthy non-present submit never reaches the failing-only `Tonemap (L87)` -> `Command Graph (L88)` tail. So the surviving pre-present difference is not a swapchain/present path at all; it is the extra late draw tail that only exists in the failing lane before present-side work even starts.

Inside that failing-only tail, the smallest still-distinguishing state seam is already classified by the current payload:

1. `boundary_state_handoff_classifier`
   - `classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index"`
   - `minimum_distinguishing_hazard="carried_pipeline_packet"`
   - meaning: Tonemap leaves behind an exact carried pipeline packet across a zero-gap boundary, and `L88` is the first heavier consumer that rebuilds scope and then rebinds.

2. `pre_rebind_carried_packet_contract`
   - `classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage"`
   - `minimum_distinguishing_hazard="attachment_exact_recipe"`
   - meaning: before `L88`'s first rebind, the carried packet stays exact on pipeline identity, descriptor/pipeline-layout contract, and push-constant-range contract; the remaining mismatch is in the carried packet's attached render-pass exact recipe.

3. `render_pass_lineage.attachment_exact_recipe`
   - `field_classifier="load_op_is_minimum_attachment_exact_hazard"`
   - `minimum_distinguishing_field="load_op"`
   - `active_load_ops=["0:LOAD"]`
   - `pipeline_load_ops=["0:CLEAR"]`
   - the mismatch is a single attachment slot (`slot 0`) and only the `load_op` field changes while the non-load recipe remains exact.

4. `load_op_attribution_split`
   - `classification="active_scope_rebuild_first_attributable_step"`
   - `first_attributable_step="active_pre_rebind_scope_reconstruction"`
   - meaning: the carried Tonemap packet keeps the `CLEAR` view of slot 0 through the boundary, and the first attributable place where the opposing `LOAD` view appears is the rebuilt active scope that exists before `L88` rebinding.

## Best next crash seam

The best next crash seam is **not** present/blit handoff anymore.

It is the failing-only late non-present tail's **pre-rebind carried-packet contract at Tonemap -> `L88`**, specifically:
- same carried Tonemap pipeline packet survives the zero-gap boundary,
- active scope is rebuilt before `L88`'s first pipeline rebind,
- that rebuilt active scope disagrees with the carried packet on exactly one attachment-exact subfield,
- the minimum surviving subfield is `slot 0 load_op` (`active=LOAD`, `pipeline=CLEAR`).

So the narrowest honest next fork is to classify why the rebuilt active scope entering `L88` acquires `slot 0 load_op=LOAD` while the carried pre-rebind Tonemap packet still preserves the exact `CLEAR` attachment recipe lineage.

## Task 145 follow-up — why the rebuilt active scope enters `L88` with `LOAD` while the carried Tonemap packet stays `CLEAR`

### Artifact roots reused

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-failing-vulkan-sourcebuild-20260522-1715/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-control-vulkan-sourcebuild-20260522-1716/`

No new rerun was needed for this fork. The runtime seam was already locked by the saved failing/control handoff artifacts, and the remaining question was exact source-lineage attribution.

### Exact codepath / lineage decision

The two pre-rebind views diverge because they are born from **different render-pass lineage sources** before `L88` binds its own pipeline:

1. **Carried Tonemap packet keeps the earlier pipeline-side recipe.**
   - Tonemap's carried packet still points at the render-pass recipe baked into the Tonemap pipeline provenance.
   - That provenance comes from `RenderingDevice::render_pipeline_create()` using `fb_format.render_pass`.
   - `fb_format.render_pass` is created in `RenderingDevice::framebuffer_format_create()` with placeholder attachment actions initialized to `CLEAR` for every attachment (`load_ops.push_back(RDD::ATTACHMENT_LOAD_OP_CLEAR)`), with the explicit comment `// Actions don't matter for this use case.`
   - So the carried packet preserves the older framebuffer-format / pipeline-side `CLEAR` recipe lineage until `L88` rebinds.

2. **Rebuilt active scope is recomputed from a fresh UI draw-list begin, not copied from the carried packet.**
   - `RendererCanvasRenderRD::_render_batch_items()` enters the UI lane with `RD::DRAW_DEFAULT_ALL` and `clear=false`.
   - `RenderingDevice::draw_list_begin()` therefore leaves slot 0 at `RDG::ATTACHMENT_OPERATION_DEFAULT`; it does not import Tonemap's carried `load_op` recipe.
   - `RenderingDeviceGraph::_add_draw_list_begin()` / draw-list materialization then resolves the actual render-pass load op from the attachment operation plus tracker policy.
   - On the failing lane, slot 0 hits the runtime-proven branch `source="non_discardable_default_load_contract"`, with decision input `resource_tracker->is_discardable=false_after_clear_ignore_checks` and upstream seed `root_texture_create<-texture_format_is_discardable_flag:false`.
   - That branch unconditionally resolves slot 0 to `ATTACHMENT_LOAD_OP_LOAD` for the rebuilt active scope.

### Why the views diverge on the failing lane

The divergence is **not** an in-place mutation of one shared packet.

It is a lineage split between:
- the **pipeline-side framebuffer-format render pass** baked earlier with placeholder `CLEAR` load ops and still visible in the carried Tonemap packet, versus
- the **active UI scope render pass** rebuilt later from `DRAW_DEFAULT_ALL` plus the non-discardable tracker rule, which recomputes slot 0 as `LOAD`.

So the exact decision that makes the two pre-rebind views differ is:
- `framebuffer_format_create()` seeds pipeline provenance with placeholder `CLEAR` load ops,
- while the later UI draw-list reconstruction ignores that carried recipe and re-solves slot 0 through the non-discardable-default-load branch.

### Exact conclusion

- **Why does the rebuilt active scope entering `L88` acquire slot-0 `LOAD`?** Because the UI active scope is a fresh draw-list/render-pass reconstruction, and its slot-0 attachment falls through the RDG `resource_tracker->is_discardable == false` default-load rule.
- **Why does the carried pre-rebind Tonemap packet still preserve `CLEAR`?** Because it still reflects the earlier pipeline provenance render pass created from framebuffer-format placeholder `CLEAR` load ops.
- **What exact codepath/lineage decision makes them diverge?** The split between `RenderingDevice::framebuffer_format_create()`'s placeholder pipeline render-pass creation and the later `RendererCanvasRenderRD::_render_batch_items()` -> `RenderingDevice::draw_list_begin()` -> `RenderingDeviceGraph::_add_draw_list_begin()` active-scope recomputation path.

## Task 146 follow-up — does the earlier placeholder-`CLEAR` lineage become the hazardous half?

### New artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-placeholder-clear-runtime-role-vulkan-sourcebuild-20260522-174808/`

### Minimal diagnostic added

A narrow reversible classifier was added in `drivers/vulkan/rendering_device_driver_vulkan.cpp` inside the existing `pre_rebind_carried_packet_contract.render_pass_lineage.attachment_exact_recipe` payload:

- `load_op_runtime_role={...}`

It compares the Tonemap first pipeline-bind scope against the pre-rebind `L88` scope and the carried pipeline provenance for the same surviving slot-0 `load_op` mismatch.

### Exact result

The new failing-lane artifact classifies the placeholder pipeline-side `CLEAR` lineage as:

- `load_op_runtime_role={classification="pipeline_placeholder_clear_is_benign_metadata", ...}`

The decisive evidence in the same payload is:

- Tonemap already executes inside the runtime active render pass with `tonemap_active_load_op="LOAD"`.
- That same Tonemap bind still carries pipeline provenance with `tonemap_pipeline_load_op="CLEAR"`.
- The Tonemap active scope and the pre-rebind `L88` active scope match on the same runtime render-pass lineage:
  - `tonemap_active_render_pass_create_serial="0xd"`
  - `l88_pre_rebind_active_render_pass_create_serial="0xd"`
- The carried pipeline provenance stays on the earlier placeholder lineage instead:
  - `tonemap_pipeline_render_pass_create_serial="0x9"`
  - `carried_pipeline_render_pass_create_serial="0x9"`
- Compatibility remains shared while exact attachment recipe does not:
  - `relation="different_handle_same_render_pass_compatibility"`
  - `compatibility_hash_match=true`
  - `attachment_exact_hash_match=false`

### Exact conclusion

The earlier placeholder-`CLEAR` framebuffer-format / pipeline lineage is **benign metadata on this seam, not the hazardous runtime half**.

Why:
- Tonemap itself is already running successfully inside the runtime `LOAD` render-pass lineage while its bound pipeline provenance still remembers the older placeholder `CLEAR` lineage.
- `L88` then re-enters that same runtime `LOAD` scope before its own pipeline rebind.
- So the placeholder `CLEAR` lineage never becomes the live active render-pass contract on this failing lane; it survives only as compatible pipeline provenance metadata attached to the carried packet.

What remains hazardous is narrower:
- the runtime active-scope reconstruction / reuse path that re-establishes the `LOAD` lineage (`create_serial=13`) before `L88` rebinds,
- not the older placeholder-`CLEAR` pipeline lineage (`create_serial=9`) itself.

## Outcome

- Durable repo note added here.
- One narrow engine diagnostic edit was made in `drivers/vulkan/rendering_device_driver_vulkan.cpp` and validated with an incremental rebuild plus one failing rerun.
- No commit was made.
