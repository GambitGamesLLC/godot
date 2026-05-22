# Submit Serial 9 lane classification after Tonemap/UI recipe alignment

Date: 2026-05-22

## Artifact roots

- Failing, source-built, lazy shared-view gate disabled:
  `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-no-lazy-gate-vulkan-sourcebuild-20260522-160300/`
- Healthy control:
  `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-control-vulkan-sourcebuild-20260522-145842/`

## What survived after recipe alignment

Disabling the lazy shared-view debug gate did realign the failing lane to the healthy Tonemap/UI preserve-content recipe on the active UI render pass (`attachment_load_ops=[0:LOAD]`, `attachment_exact_hash=0x6529dc72`). That removes the earlier active-scope `LOAD` vs carried-pipeline `CLEAR` mismatch as the best first seam.

The surviving submit-9 difference is not another Tonemap/UI attachment recipe mismatch. It is the *submission content and role* of `submit_serial=9` itself:

- **Failing `submit_serial=9`** is the heavy frame-1 draw submission that hangs.
  - `present_submission=false`
  - `wait_semaphores=1`, `swap_chains=0`, `signal_semaphores=0`
  - `labels=102`
  - `label_tail=... Tonemap (L87) (Draw) > Command Graph (L88) (Draw)`
  - `last_breadcrumb="UI_PASS"`
  - command workload reaches `Tonemap` + `L88` and then fence wait fails with `VK_ERROR_DEVICE_LOST`
- **Healthy control `submit_serial=9`** is not the matching draw packet.
  - It is the later present/blit lane, with `BLIT_PASS` as the last breadcrumb
  - It carries the light present-side work instead of the full 102-label draw graph
  - The Tonemap/UI draw packet completed on an earlier submit in the healthy lane

## Best next crash seam

The best next seam is therefore **submit partitioning / queue-role divergence around the failing draw submit**, not another attachment recipe inside the aligned UI preserve-content pass.

Plainly: after alignment, the failing run still puts the heavy `Tonemap (L87)` -> `Command Graph (L88)` draw packet on `submit_serial=9`, while the healthy control's `submit_serial=9` is already down to the later `BLIT_PASS` / present-side work. That surviving difference is exact, durable, and upstream of the unchanged `fence_wait_error submit_serial=9` crash.

## Why this is the best next seam

Because it is the smallest surviving exact diff that still separates crash vs no-crash after the render-pass recipe was normalized:

1. Active UI render-pass recipe alignment succeeded.
2. The crash remained unchanged.
3. The remaining exact difference is that the two runs are no longer doing the same kind of work on `submit_serial=9`.

That makes the next honest question:

- why does the failing source-built lane still assemble / retain the heavy `Tonemap` -> `L88` draw command buffer on `submit_serial=9`,
- while the healthy control has already advanced that work off submit 9 and is only waiting/presenting `BLIT_PASS` there?

The next diagnostic should stay focused on submission assembly / partitioning / handoff provenance for the failing draw submit, rather than re-opening the already-demoted attachment-load-op seam.

## Task 143 addendum: exact handoff / partitioning condition

### New artifact roots

- Failing handoff rerun:
  `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-failing-vulkan-sourcebuild-20260522-1715/`
- Healthy control handoff rerun:
  `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-submit-assembly-handoff-control-vulkan-sourcebuild-20260522-1716/`

### Minimal diagnostic added

A narrow RD-side breadcrumb was added under the existing `GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1` gate:

- `swap_buffers_begin frame=... present_requested=... pending_swap_chains=...`
- `draw_list_begin_for_screen frame=... split_swapchain_cmd_buffer=true ...`

This makes the queue-role handoff visible without changing the actual submission policy.

### Exact classification

The surviving difference is now pinned to **which `swap_buffers()` phase `submit_serial=9` belongs to**.

#### Failing lane

At the moment `submit_serial=9` is assembled, the failing run is still in the earlier **non-present** frame-1 execute:

- `frame_execute_begin frame=1 present_requested=false frame_can_present=false swap_chains=0`
- `frame_execute_cmd_submit frame=1 command_buffer_count=1 ... present_swap_chain=false`
- `queue_submit submit_serial=9 ... present_submission=false ... label_tail=... Tonemap (L87) (Draw) > Command Graph (L88) (Draw)`

Only **after** the fence-wait failure does the failing run reach the screen handoff path:

- `draw_list_begin_for_screen frame=1 ... split_swapchain_cmd_buffer=true`
- `swap_buffers_begin frame=1 present_requested=true pending_swap_chains=1`
- `frame_execute_begin frame=1 present_requested=true frame_can_present=true swap_chains=1`
- `frame_execute_cmd_submit frame=1 command_buffer_count=2 ... present_swap_chain=true`

So the heavy Tonemap→L88 graph stays attached to failing `submit_serial=9` because that submit is still the pre-present `_execute_frame(false)` packet. The BLIT/present split command buffer has not been handed off yet.

#### Healthy lane

The healthy control shows the same two-phase structure, but it survives the non-present frame-1 submit and then advances into the present handoff before `submit_serial=9`:

- non-present frame-1 pass first:
  - `frame_execute_begin frame=1 present_requested=false frame_can_present=false swap_chains=0`
  - `frame_execute_cmd_submit frame=1 command_buffer_count=1 ... present_swap_chain=false`
  - `queue_submit submit_serial=7 ... present_submission=false`
- then the screen path is recorded and split:
  - `draw_list_begin_for_screen frame=1 ... split_swapchain_cmd_buffer=true`
  - `swap_buffers_begin frame=1 present_requested=true pending_swap_chains=1`
  - `frame_execute_begin frame=1 present_requested=true frame_can_present=true swap_chains=1`
  - `frame_execute_cmd_submit frame=1 command_buffer_count=2 ... present_swap_chain=true`
  - `queue_submit submit_serial=8 ... present_submission=true ... last_breadcrumb="BLIT_PASS"`

By the time the healthy run reaches `submit_serial=9`, it is already on the later present-side lane for the next frame:

- `queue_submit submit_serial=9 ... present_submission=true ... last_breadcrumb="BLIT_PASS"`

### Conclusion

The exact upstream condition is:

- **Heavy Tonemap/UI work remains on submit 9 when that serial is still the non-present `_execute_frame(false)` submission** (`present_requested=false`, `frame_can_present=false`, no `draw_list_begin_for_screen()`, no pending swapchain, `command_buffer_count=1`).
- **The healthy lane advances to the later BLIT/present role only after `draw_list_begin_for_screen()` has run and `swap_buffers(true)` re-enters `_execute_frame(true)` with a pending swapchain**, which activates the split swapchain command buffer path (`command_buffer_count=2`, `present_submission=true`, `BLIT_PASS`).

So this is not a mysterious repartition happening inside `submit_serial=9` itself. The divergence is upstream: the failing run dies on the earlier non-present frame submit before the later screen-blit/present handoff can replace that serial role, while the healthy control survives long enough to cross into the split present path.
