# GDGS compositor instrumentation map for surviving repro

Date: 2026-05-16
Repo: `/home/derrick/.openclaw/workspace/projects/godot`
Companion GDGS repo: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`

## Purpose

Map the exact instrumentation points and staged isolation order for the surviving GDGS repro around the compositor callback / RenderingDevice boundary, so the next coder pass can add breadcrumbs and assertions without re-deriving the path.

## Important correction: the current surviving repro is not actually using a local RD in the hot path

The earlier working theory was “local RenderingDevice inside compositor callback.” The helper still supports that path:

- `addons/gdgs/runtime/render/gaussian_rendering_device_context.gd:38`
  - `context.device = RenderingServer.create_local_rendering_device() if device_ == null else device_`
- `doc/classes/RenderingServer.xml:1135`
  - local devices are for separate-thread draw/compute and “Cannot draw to the screen nor share data with the global RenderingDevice.”

But the current surviving GDGS repro explicitly overrides that and binds the global RD instead:

- `addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd:85`
  - `state.context = RenderingDeviceContext.create(RenderingServer.get_rendering_device())`
- `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd:343`
  - `rd = RenderingServer.get_rendering_device()`

So the current repro seam is more precisely **global-RD compute inside the compositor callback, followed by later screen blit/present work**. Local-device `submit()` / `sync()` instrumentation is still worth mapping, but only as a staged follow-up if the plugin is toggled back to the default local-device path.

## Exact instrumentation points

### 1) Compositor callback processing in Godot

Primary entrypoint:

- `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp:298-316`
  - `RendererSceneRenderRD::_process_compositor_effects(...)`
  - exact callback invoke site is `callback.callv(arr);` at line 316

Add here:

1. A debug breadcrumb/log before entering each compositor callback with:
   - callback type
   - effect RID
   - frame number if available
   - view count
   - whether `p_render_data->reflection_probe` is valid
2. A matching exit breadcrumb/log after `callback.callv(arr)`.
3. Duration/timing around the callback so we can see whether the fault correlates with a slow or long-lived callback.
4. A one-shot warning if multiple compositor effects are chained, to avoid attributing all later damage to the wrong callback.

Why here:

- This is the narrowest engine-owned boundary between the frame and GDGS.
- It cleanly separates “Godot entered the callback” from “the GPU later died at BLIT_PASS.”

### 2) Frame-stage anchors around the compositor callback

Useful stage markers already exist in Forward+:

- `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:2265`
  - `Process Post Opaque Compositor Effects`
- `...:2326`
  - `Process Post Sky Compositor Effects`
- `...:2403`
  - `Process Pre Transparent Compositor Effects`
- `...:2453`
  - `Process Post Transparent Compositor Effects`

Add here:

- one extra debug label immediately before and after `_process_compositor_effects(...)` for the active callback type used by the repro
- include the callback type in the label text, not just the existing generic timestamp

Why here:

- It pins the callback to the exact stage of the frame and narrows whether the later failure happens before or after transparent/final passes continue.

### 3) GDGS compositor callback body

Primary plugin-side entrypoint:

- `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd:148`
  - `manager.render_for_compositor(...)`
- `...:176`
  - `if is_no_present_mode:` early-out proving writeback can be skipped
- `...:242`
  - global RD compositor compute list begins
- `...:343`
  - `rd = RenderingServer.get_rendering_device()`

Add here:

1. A stage enum or string field, updated monotonically through the callback:
   - `enter_callback`
   - `camera_data_ready`
   - `render_for_compositor_called`
   - `render_for_compositor_returned`
   - `no_present_early_out`
   - `writeback_uniform_set_created`
   - `writeback_dispatch_submitted`
   - `callback_return`
2. A hard assertion/log that the callback RD is global and not a local override, so the logs stop saying “local RD” when the current branch is actually global.
3. If the next coder wants ultra-high signal, add a temporary exported debug enum so the callback can short-circuit after any of the above stages without editing code between runs.

Why here:

- This is the narrowest place to keep the compositor callback alive while selectively skipping work.

### 4) GDGS raster pipeline staging points

Primary plugin-side workload entrypoint:

- `addons/gdgs/runtime/render/gaussian_renderer.gd:68`
  - `func _rasterize_state(state, point_count: int)`

Exact pass calls:

- `...:89` projection pass
  - `state.pipelines["gsplat_projection"].call(...)`
- `...:96` radix upsweep
- `...:104` radix spine
- `...:109` radix downsweep
- `...:121` boundaries pass
- `...:125` render pass

Add here:

1. A single debug stage gate before each pass call.
2. Breadcrumb/log before and after each pass group.
3. Per-pass metadata logging:
   - point count
   - texture size
   - tile dims
   - radix pass index
   - computed offsets for upsweep/downsweep
4. A final `rasterize_state_return` marker after the render pass or after the selected short-circuit point.

Why here:

- This is the exact staged isolation seam the next coder needs.

### 5) GDGS device-context dispatch wrapper

Primary wrapper:

- `addons/gdgs/runtime/render/gaussian_rendering_device_context.gd:101-125`
  - `create_pipeline(...)`
- `...:124`
  - `rd.compute_list_dispatch(...)`
- `...:125`
  - `rd.compute_list_add_barrier(compute_list)`

Add here:

1. A temporary debug name parameter for each created pipeline (`projection`, `radix_upsweep`, etc.).
2. Before each dispatch, print/log:
   - pipeline name
   - workgroup dimensions
   - whether push constants were supplied
   - push-constant byte size
   - whether this dispatch is direct vs indirect
3. After each `compute_list_add_barrier`, log that the pass barrier completed.
4. If local-device mode is re-enabled later, this is also the right place to log `submit()` / `sync()` boundaries.

Why here:

- This is the single plugin-side choke point that sees every compute dispatch.

### 6) RenderingDevice compute-list and barrier paths in Godot

Engine sites:

- `servers/rendering/rendering_device.cpp:6634`
  - `RenderingDevice::compute_list_begin()`
- `...:6786`
  - `RenderingDevice::compute_list_dispatch(...)`
- `...:7048`
  - `RenderingDevice::compute_list_add_barrier(...)`
- `...:7860`
  - `RenderingDevice::submit()`
- `...:7870`
  - `RenderingDevice::sync()`
- `servers/rendering/rendering_device_graph.cpp:2019`
  - `RenderingDeviceGraph::add_compute_list_begin(RDD::BreadcrumbMarker p_phase, uint32_t p_breadcrumb_data)`

Add here:

1. For the debugging branch, thread a non-zero breadcrumb phase/data into the compute-list begin path for compositor-owned compute lists, instead of leaving `draw_graph.add_compute_list_begin()` unannotated.
2. In `compute_list_dispatch(...)`, add a temporary debug print for group counts plus currently bound pipeline RID / reflected push-constant size when available.
3. In `compute_list_add_barrier(...)`, log pipeline/set rebinding after the barrier restore, because a bad restore could masquerade as a later pass failure.
4. In `submit()` / `sync()`, add breadcrumbs/logging even if they are not currently hit by the surviving repro. If the plugin switches back to default local RD, these become the first engine-owned points to verify.

Why here:

- Current `BLIT_PASS` breadcrumbs are good for the failure endpoint, but the compute side is under-labeled.

### 7) Vulkan / BLIT breadcrumbs and screen blit boundary

Engine sites:

- `servers/rendering/rendering_device.cpp:5527`
  - screen draw list begins with `RDD::BreadcrumbMarker::BLIT_PASS`
- `servers/rendering/renderer_rd/renderer_compositor_rd.cpp:42-123`
  - `RendererCompositorRD::blit_render_targets_to_screen(...)`
- `servers/rendering/renderer_rd/renderer_compositor_rd.cpp:51`
  - `draw_list_begin_for_screen(p_screen)`
- `servers/rendering/renderer_rd/renderer_compositor_rd.cpp:116`
  - screen blit push constants are set
- `servers/rendering/renderer_viewport.cpp:909`
  - viewport path calling `blit_render_targets_to_screen(...)`
- `drivers/vulkan/rendering_device_driver_vulkan.cpp:6772-6829`
  - `command_insert_breadcrumb(...)`
- `drivers/vulkan/rendering_device_driver_vulkan.cpp:6915-7019`
  - reverse breadcrumb dump and `--accurate-breadcrumbs` guidance

Add here:

1. Keep reruns on the diagnostic branch using `--accurate-breadcrumbs`.
2. Add one temporary breadcrumb/user-data value just before `RendererCompositorRD::blit_render_targets_to_screen(...)` loops the render targets, so we can distinguish:
   - died before screen prepare
   - died after screen prepare but before draw-list begin
   - died during the per-target blit loop
3. Add one log line with render-target RID validity/count before screen blit begins.

Why here:

- `BLIT_PASS` is still the observed fault boundary, so we want one step more precision inside the blit path.

## Staged isolation order

Keep the compositor callback active throughout. Only shrink the workload.

### Stage 0: callback only, skip GDGS render call entirely

Location:

- `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd:148`

Action:

- return or continue before `manager.render_for_compositor(...)`

Expected read:

- If this still crashes later, the fault is not in the GDGS raster workload at all; look harder at callback registration / state handling / downstream engine sequencing.
- If stable, the crash requires GDGS work beyond mere callback entry.

### Stage 1: call into GDGS, but no dispatch

Location:

- `addons/gdgs/runtime/render/gaussian_renderer.gd:68-129`

Action:

- let `render_for_compositor()` rebuild/upload state and prepare textures, but short-circuit `_rasterize_state()` before the first compute-list begin.

Expected read:

- Stable here means resource allocation / texture creation alone are not enough.
- Crash here would implicate allocation, descriptor creation, or RD resource ownership/setup.

### Stage 2: trivial scratch-buffer / scratch-texture dispatch only

Location:

- ideally injected via `gaussian_rendering_device_context.gd` or a temporary debug helper called from `_rasterize_state()` before the real passes

Action:

- execute one tiny known-safe compute dispatch against a scratch buffer or 1x1 scratch texture, then barrier, then stop.

Expected read:

- Stable here means “compute inside compositor callback” is not inherently fatal on the current branch.
- If this alone crashes, the engine/backend boundary becomes the top suspect.

### Stage 3: projection only

Location:

- `gaussian_renderer.gd:89`

Action:

- run only `gsplat_projection`, then stop.

Expected read:

- If this is the first failing stage, inspect camera push constants, point-count derived output sizes, and projection shader writes.

### Stage 4: radix only

Location:

- `gaussian_renderer.gd:96-117`

Action:

- re-enable the radix loop after projection.

Expected read:

- First failure here points at sort buffer sizing, radix offsets, push constants, or pass-to-pass barriers.

### Stage 5: boundaries only

Location:

- `gaussian_renderer.gd:121`

Action:

- re-enable `gsplat_boundaries` after projection + radix.

Expected read:

- First failure here points at tile-id bounds, tile buffer sizing, or invalid sort-buffer contents reaching the boundaries pass.

### Stage 6: render pass last

Location:

- `gaussian_renderer.gd:125`

Action:

- re-enable `gsplat_render` only after all prior stages are stable.

Expected read:

- If only this stage reintroduces the crash, the likely owner narrows to final image/depth writes or tile-boundary consumption by the render shader.

### Stage 7: compositor writeback/presentation last

Location:

- `gaussian_compositor_effect.gd:181-257`

Action:

- only after stage 6 is stable, re-enable the compositor writeback dispatch that composites GDGS output back into scene color.

Expected read:

- This separates GDGS raster safety from the later “consume GDGS result in compositor/global frame” path.

## Key assertions / experiments to add

### A. Correct the seam naming in logs

Because the current repro uses the global RD in the plugin hot path, add a one-shot log/assert showing:

- compositor RD source = global
- raster RD source = global
- local-device submit/sync not currently exercised

Without this, future logs will keep sending people after the wrong suspect.

### B. Resource provenance assertion

Add a temporary assertion wherever practical that all RIDs bound together in the compositor writeback path come from the same RD ownership model.

Even though the current branch is global/global, keep the assertion message explicit enough to catch a future accidental local/global mix.

### C. Boundary pass assertions in GDGS

Relevant files:

- `addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd:115`
  - tile-bounds buffer allocation
- `addons/gdgs/runtime/render/shaders/compute/gsplat_boundaries.glsl`

Add assertions before running boundaries:

1. `state.tile_dims.x * state.tile_dims.y > 0`
2. max tile id implied by the current render target equals allocated bounds-buffer length minus one
3. `num_sort_elements_max = point_count * MAX_SORT_ELEMENTS_PER_SPLAT` matches the sort/histogram/bounds assumptions used to size buffers
4. if practical on CPU side, clear tile-bounds to a sentinel pattern and verify only valid tile range is later consumed

Reason:

- the boundaries shader writes `bounds_buffer[tile_id]` directly from sorted keys; a bogus tile id becomes a very sharp failure discriminator.

### D. Radix pass assertions

Relevant file:

- `addons/gdgs/runtime/render/gaussian_renderer.gd:93-117`

Add assertions/logs for every radix pass:

- `radix_input_offset < num_sort_elements_max * 2`
- `radix_output_offset < num_sort_elements_max * 2`
- reflected push-constant sizes still match exact payload sizes (4 / 8 / 12 bytes)
- dispatch group counts equal the values used when the pipelines were created

### E. Projection/render bounds sanity

Relevant files:

- `addons/gdgs/runtime/render/shaders/compute/gsplat_projection.glsl`
- `addons/gdgs/runtime/render/shaders/compute/gsplat_render.glsl`

Add CPU-side assertions/logs for:

- texture dims > 0
- tile dims derived from texture size are consistent with render pipeline grid
- point count and instance count match uploaded buffer sizes

This does not prove shader memory safety, but it narrows obvious bad launches.

### F. Accurate breadcrumbs experiment

Always rerun the instrumented branch with `--accurate-breadcrumbs` at least once per stage boundary change.

Reason:

- Vulkan already tells us the current breadcrumb chain can be imprecise without that flag.

## Recommended first coder pass

1. Add callback entry/exit breadcrumbs in `RendererSceneRenderRD::_process_compositor_effects(...)`.
2. Add a small debug stage gate in `gaussian_compositor_effect.gd` so stages 0-7 can be toggled without rewriting code each run.
3. Add per-pass stage logs in `gaussian_renderer.gd`.
4. Add dispatch-name logging in `gaussian_rendering_device_context.gd`.
5. Add the seam-correction note to the logs: current surviving repro is global/global, not local/global.
6. Only after that, consider deeper engine-side compute breadcrumbs in `RenderingDevice` if stage 2 or stage 3 still dies.

## Bottom line

The surviving repro still dies near `BLIT_PASS`, but the current checkout changes the interpretation of the boundary:

- the plugin helper can use a local RD
- the current repro path does not; it explicitly uses the global RD both for raster work and compositor writeback setup
- the next diagnostic lane should therefore isolate **compositor-callback + staged GDGS compute passes first**, while keeping local-device `submit()` / `sync()` breadcrumbs ready as a secondary path if the plugin is toggled back to default local-device creation
