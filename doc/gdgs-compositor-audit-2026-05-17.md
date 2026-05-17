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
