# 2026-05-22 runtime LOAD reuse classification

## Scope

Task 147 follow-up to the source-built host-Vulkan GDGS investigation.

Question: inside the live runtime `LOAD` render-pass reuse path, what still differs between failing and healthy runs, and what is the best next crash seam?

## Diagnostic added

Added a small reversible `runtime_load_reuse_path={...}` block to the existing `tonemap_l88_contrast` payload in:

- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`

The block reports:

- whether Tonemap's live active render-pass scope is the same scope `L88` sees **before** its own pipeline rebind
- active runtime scope create serial / attachment exact hash / load-op hash / subpass
- Tonemap local workload shape
- `L88` local workload shape
- exact Tonemap→`L88` workload deltas inside that one reused runtime scope

## Artifacts

### Healthy control

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-runtime-load-reuse-control-vulkan-sourcebuild-20260522-1809/`
- exit: `0`

Key control facts from `stdout.log`:

- non-present draw submit is `submit_serial=7`
- command summary is shallow: `labels=17`
- tail is `... > Tonemap (L7) (Draw) > Command Graph (L8) (Draw)`
- runtime UI pass is still a single-attachment `LOAD` pass with the same attachment exact hash:
  - `begin_render_pass_scope create_serial=16 ... owner_label="Tonemap (L5) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - immediately followed by
  - `begin_render_pass_scope create_serial=16 ... owner_label="Command Graph (L6) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`

### Failing source-built repro

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-runtime-load-reuse-failing-vulkan-sourcebuild-20260522-1822/`
- exit: `134`

Key failing facts from `stdout.log`:

- crashing non-present draw submit is `submit_serial=9`
- command summary is deep/heavy: `labels=102`
- tail is `... > Tonemap (L87) (Draw) > Command Graph (L88) (Draw)`
- the new seam-local block reports:

```text
runtime_load_reuse_path={classification="same_runtime_load_scope_reused_before_l88_rebind", ... active_render_pass_create_serial="0xd", active_render_pass_attachment_exact_hash="0x6529dc72", active_render_pass_load_op_hash="0xc5247e53", subpass=0, tonemap_label={index=100,level=87,draw_calls=1,pipeline_binds=1,uniform_binds=1,...}, l88_label={index=101,level=88,draw_calls=10,pipeline_binds=1,uniform_binds=11,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}, tonemap_to_l88_scope_delta={label_index_delta=1,level_delta=1,draw_calls_delta=9,pipeline_binds_delta=0,uniform_binds_delta=10,vertex_buffer_binds_delta=10,vertex_buffer_binding_total_delta=10,index_buffer_binds_delta=1,...}}
```

## Classification

The remaining failing-vs-healthy difference is **not** whether Tonemap and the next UI draw share a live runtime `LOAD` scope.

Both runs do that.

What differs is the amount of command-graph payload routed through that shared scope:

- healthy control reaches the scope as a shallow `L7`→`L8` UI tail inside a `labels=17` non-present draw submit
- failing repro reaches the scope as a deep `L87`→`L88` UI tail inside a `labels=102` non-present draw submit
- inside the failing shared scope itself, `L88` adds:
  - `+9` draw calls
  - `+10` uniform binds
  - `+10` vertex-buffer binds
  - `+10` vertex-buffer binding total
  - `+1` index bind

So the live runtime `LOAD` scope is working as a shared scope in both lanes; the failing-only difference is the **late, much heavier `L88` payload** being assembled into that scope.

## Best next seam

Best next crash seam: the failing-only submit-assembly / graph-build path that inflates the frame-1 non-present UI draw lane before `L88` runs.

In plain terms:

- stop treating the `LOAD` scope reuse itself as the likely differentiator
- compare why the failing lane reaches that scope as a deep `L87`→`L88` tail with a heavy local draw payload
- against why the healthy lane reaches the equivalent scope as a shallow `L7`→`L8` UI tail

That points the next investigation at the upstream command-content / graph-build path feeding `L88`, not at the already-classified placeholder-`CLEAR` metadata and not at the mere fact of runtime `LOAD` scope reuse.

## Task 148 follow-up: exact graph-build condition behind the heavy `L88` tail

### Diagnostic added

Added one tiny, reversible classifier to the existing `GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1` payload in:

- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`

The new `graph_build_condition={...}` block only summarizes already-observable UI batch facts:

- `classifier`
- `expected_draw_calls_from_rendered_batches`
- whether all rendered batches are rect-like
- whether all rendered batches need preserved destination color
- whether all rendered batches are clipped

### Fresh minimum comparisons

#### Healthy control

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-batch-shape-control-vulkan-sourcebuild-20260522-1839/`
- exit: `0`

Key control facts:

- `ui_pass_origin` now reports:
  - `batch_summary={rendered=1,rect_like=1,polygon=0,primitive=0,clipped=0,lcd_blend=0,destination_color=1,blend_disabled=0,...}`
  - `graph_build_condition={classifier="single_rect_preserve_batch",expected_draw_calls_from_rendered_batches=1,all_rendered_batches_are_rect_like=true,all_rendered_batches_need_preserved_color=true,all_rendered_batches_are_clipped=false}`
- the same run keeps the shallow runtime scope tail:
  - `Tonemap (L7) (Draw)`
  - `Command Graph (L8) (Draw)`

#### Failing staged repro

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-batch-shape-failing-vulkan-sourcebuild-20260522-1840/`
- exit: `134`

Key failing facts:

- `ui_pass_origin` now reports:
  - `batch_summary={rendered=10,rect_like=10,polygon=0,primitive=0,clipped=10,lcd_blend=0,destination_color=10,blend_disabled=0,...}`
  - `graph_build_condition={classifier="multi_rect_preserve_clip_batch_chain",expected_draw_calls_from_rendered_batches=10,all_rendered_batches_are_rect_like=true,all_rendered_batches_need_preserved_color=true,all_rendered_batches_are_clipped=true}`
- the same run keeps the heavy runtime scope tail and crash seam:
  - `Tonemap (L87) (Draw)`
  - `Command Graph (L88) (Draw)`
  - `fence_wait_error submit_serial=9 wait_result=-4`

### Exact attribution

The upstream graph-build condition that makes the failing lane accumulate the heavier `L88` tail is now explicit:

- healthy control UI path is a **single preserved destination-color rect batch**
- failing staged repro UI path is a **ten-batch preserved destination-color clipped rect chain**
- the new `expected_draw_calls_from_rendered_batches` value matches the surviving runtime payload split exactly:
  - control: `1` rendered UI batch ↔ shallow `L8` local draw payload
  - failing: `10` rendered UI batches ↔ heavy `L88` local draw payload with `10` indexed draws

That means the extra workload family is not generic render-pass reuse metadata; it is the **canvas/UI clipped rect batch chain** emitted by `RendererCanvasRenderRD::_render_batch_items()` on the failing staged lane. This is the best next crash seam because it is the first exact failing-only content family that scales with the surviving `L88` deltas (`+9` draws, `+10` uniform binds, `+10` vertex-buffer binds, `+1` index bind`) while reusing the same runtime `LOAD` scope as the healthy control.

## Hygiene

- Diagnostic is reversible and scoped to existing text payloads.
- No commit made in this pass.
