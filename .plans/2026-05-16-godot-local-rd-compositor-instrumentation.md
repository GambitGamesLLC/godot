# Godot

**Date:** 2026-05-16
**Status:** In Progress
**Agent:** Chip 🐱‍💻

---

## Goal

Instrument and progressively isolate the surviving GDGS repro around the local-RenderingDevice-in-compositor boundary, so we can determine whether the remaining fault comes from GDGS compute/resource misuse, unsupported boundary usage, or Godot/backend synchronization behavior.

---

## Overview

The cleaned repro is now much sharper than where we started. We removed one confirmed GDGS misuse: the radix push-constant contract mismatch. Godot 4.7-dev5 validation errors disappeared completely after that fix, but the later `BLIT_PASS` / Vulkan device-loss crash remained unchanged. That means the remaining bug is not the already-fixed push-constant issue.

The latest audit ranked the most likely surviving surface as the local-RD-in-compositor boundary: GDGS creates and uses a local `RenderingDevice` from inside the compositor callback, then the frame still later dies around `BLIT_PASS`, even in `No Present`. At the same time, we still cannot rule out remaining GDGS compute/resource misuse, such as OOB SSBO/image access, bad buffer bounds, or unsupported boundary behavior that only detonates after callback return.

So the next lane should be controlled instrumentation and staged simplification, not blind patching. We want to keep the compositor callback active while progressively trivializing the local RD workload, adding breadcrumbs around the callback, submit/wait points, and `BLIT_PASS`, and re-enabling passes in a deliberate order. The goal is to learn the first point where the crash becomes inevitable, and whether that point looks more like plugin misuse, unsupported API usage, or backend/engine fault.

---

## REFERENCES

| ID | Description | Path |
| --- | --- | --- |
| `REF-01` | Godot post-GDGS audit note | `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-post-gdgs-audit-2026-05-16.md` |
| `REF-02` | Godot source-debug lane memo | `/home/derrick/.openclaw/workspace/projects/openclaw-godot/docs/gdgs-godot-source-debug-lane-2026-05-16.md` |
| `REF-03` | GDGS push-constant fix plan/results | `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/.plans/2026-05-16-gdgs-push-constant-contract-fix.md` |
| `REF-04` | GDGS nightly repro note | `/home/derrick/.openclaw/workspace/projects/openclaw-godot/docs/gdgs-godot-47-dev5-nightly-repro-2026-05-16.md` |
| `REF-05` | Current Godot source checkout | `/home/derrick/.openclaw/workspace/projects/godot/` |
| `REF-06` | Instrumentation map note produced from source walk | `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-local-rd-compositor-instrumentation-map-2026-05-16.md` |
| `REF-07` | Staged QA results and artifact references for the compositor isolation run | `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md` |
| `REF-08` | Independent audit of the staged instrumentation package and QA evidence | `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-audit-2026-05-17.md` |

---

## Tasks

### Task 1: Map instrumentation points and staged isolation order in Godot/GDGS boundary

**Bead ID:** `oc-xzt`
**SubAgent:** `primary` (for `research`)
**Role:** `research`
**References:** `REF-01`, `REF-02`, `REF-03`, `REF-04`, `REF-05`, `REF-06`
**Prompt:** Using the current Godot source checkout and cleaned repro evidence, map the exact instrumentation points and staged workload-isolation order for the local-RD/compositor boundary. Specify where to add breadcrumbs/assertions and what order to re-enable workload slices (`no dispatch`, trivial dispatch, projection, radix, boundaries, render, etc.).

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-local-rd-compositor-instrumentation-map-2026-05-16.md`

**Status:** ✅ Complete

**Results:** Mapped the exact Godot and GDGS instrumentation seams in `REF-06`, including callback entry/exit, staged GDGS pass gates, RenderingDevice compute/BLIT breadcrumb sites, and the recommended re-enable order from “skip `render_for_compositor()` entirely” through compositor writeback. Important correction from the actual source walk: the surviving current GDGS repro is not using a local RD in the hot path right now. The helper still supports local-device creation, but the live raster path explicitly binds `RenderingServer.get_rendering_device()` in `addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd` and the compositor path also binds the global RD in `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd`. So the first coder pass should instrument the compositor callback + staged global-RD compute passes, while still mapping local-device `submit()` / `sync()` as a secondary path if the plugin is toggled back to default local-device creation. Validated against `REF-01`, `REF-02`, `REF-04`, `REF-05`, and recorded durably in `REF-06`.

---

### Task 2: Prepare the local instrumentation branch/package

**Bead ID:** `oc-jev`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-01`, `REF-02`, `REF-03`, `REF-04`, `REF-05`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-jev`, create a dedicated instrumentation branch from the current handoff state, and prepare the first diagnostic package for the surviving GDGS compositor repro. Use `REF-06` as the source of truth. Keep the changes diagnostic and reversible, not a speculative fix. Add callback entry/exit breadcrumbs in `RendererSceneRenderRD::_process_compositor_effects(...)`, add stage gates/logging in `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd` and `addons/gdgs/runtime/render/gaussian_renderer.gd`, add dispatch-name / push-constant-size / group-count logging in `addons/gdgs/runtime/render/gaussian_rendering_device_context.gd`, and add seam-correction logging proving the current repro is global/global rather than local/global. Run relevant repo-local validation you can for the touched areas, commit the instrumentation package, push the branch to the Gambit fork, and close bead `oc-jev` with a clear reason if the package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/renderer_scene_render_rd.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_render_manager.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_renderer.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_rendering_device_context.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd`

**Status:** ✅ Complete

**Results:** Created dedicated instrumentation branches named `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` in both the Godot repo and the companion GDGS repo because the requested package spans the engine-owned callback seam plus the plugin-owned raster/compositor runtime. In Godot, added reversible compositor callback breadcrumbs in `RendererSceneRenderRD::_process_compositor_effects(...)` that log callback batch begin/end, effect RID/index, callback type, view count, reflection-probe status, and per-callback duration. In GDGS, added exported compositor/raster debug stage gates plus explicit stage logs in `gaussian_compositor_effect.gd`, per-pass raster stage logs and stop points in `gaussian_renderer.gd`, dispatch-name / push-constant-size / group-count logging in `gaussian_rendering_device_context.gd`, and seam-correction logs proving the current repro path is global/global rather than local/global. Validation run: `git diff --check` in both repos, `python3 misc/scripts/file_format.py servers/rendering/renderer_rd/renderer_scene_render_rd.cpp` in Godot, `python3 .../file_format.py` across the touched GDGS scripts, and `godot --headless --path . --script <script> --check-only --quit` for each touched GDGS script in `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`. This pass stayed diagnostic/reversible and did not attempt a speculative fix.

---

### Task 3: Run staged GDGS compositor repro with the new instrumentation package

**Bead ID:** `oc-hf4`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-01`, `REF-04`, `REF-05`, `REF-06`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-hf4` and run the staged GDGS compositor repro using the new instrumentation branches. Capture the first meaningful new evidence from the documented isolation order: callback-only, no-dispatch, trivial-dispatch if available, projection-only, radix-only, boundaries-only, render-last, then compositor writeback/presentation only if earlier stages stabilize. Use `--accurate-breadcrumbs` on at least one rerun per meaningful stage boundary. Record exactly which stage first reproduces the crash or survives, preserve logs/artifacts/notes in repo-owned docs as needed, update this plan with actual findings, and close bead `oc-hf4` with a clear reason if the QA evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA ran the staged compositor isolation order on the instrumentation branches in the existing repo worktrees without disturbing unrelated work. Because both repos were already checked out on `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`, no extra worktree was needed. The staged harness was driven from `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/`, with durable notes captured in `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Exact executed order/results: `callback_only` survived; `prepared_no_dispatch` survived; trivial dispatch was not available from the package and was explicitly skipped/noted; `projection_only` was the first failing stage in both the normal run and an `--accurate-breadcrumbs` rerun; later stages (`radix_only`, `boundaries_only`, `render_only`, and compositor writeback/presentation) were intentionally not run because the first failing boundary had already been isolated. The updated seam evidence stayed consistent with the coder pass: the live repro is still global/global (`compositor_path_uses_global_rd=true`, `raster_path_uses_global_rd=true`, `local_device_submit_sync_exercised=false`), so the current crash is not gated on the old local/global seam theory. The new high-signal finding is that the first meaningful compute dispatch alone is sufficient: `projection_begin` → `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)` → `rd barrier complete pipeline=gsplat_projection` → `projection_end` all log successfully, then the device later dies at `fence_wait`, with lost-device breadcrumbs still collapsing to `BLIT_PASS`. Artifact roots: normal run `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-20260517-090004/`; accurate rerun `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-accurate-20260517-090036/`. This narrows the next suspect list to the projection pass itself (writes/bounds/layout/push-constant contract) or synchronization/lifetime fallout immediately after that pass, while making callback-only handling, no-dispatch setup, radix, boundaries, render, and compositor writeback/presentation unlikely as the first trigger.

---

### Task 4: Audit whether the instrumentation package is the right first experiment

**Bead ID:** `oc-76q`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-01`, `REF-02`, `REF-03`, `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** Independently audit the instrumentation package and the staged QA findings after bead `oc-hf4` completes. Confirm that the experiment was run in the right order, that the new evidence actually separates plugin misuse from engine/backend failure, and that the next suspect list follows from the observed results rather than old assumptions.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-audit-2026-05-17.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the staged order against `REF-06`, reviewed the instrumentation branches directly, and spot-checked the durable QA artifacts in `REF-07`. Verdict: the package supports a narrow but real conclusion — `projection_only` is the first failing staged GDGS boundary in the current repro, while `callback_only` and `prepared_no_dispatch` survive and the live seam remains global/global rather than local/global. Important caveat captured in `REF-08`: the staged QA evidence was gathered with the managed Godot runtime, not a source-built binary from the Godot instrumentation branch, so this pass does **not** fully prove plugin-misuse vs engine/backend separation at the engine-owned callback seam. The missing trivial scratch-dispatch stage is also a known evidence gap, but it does not overturn the narrower projection-first finding. Next-suspect list after audit: projection output writes/bounds, projection pipeline/push-constant contract, post-projection resource lifetime/synchronization fallout, and broader compositor-path compute/backend handling once the source-built Godot breadcrumbs are exercised.

---

### Task 5: Deepen projection-pass diagnostics and add the missing trivial scratch dispatch control

**Bead ID:** `oc-dew`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-dew` and prepare the next diagnostic slice focused on the projection pass. Add the missing trivial scratch-dispatch control so QA can distinguish “projection-specific failure” from “any real compute dispatch in this compositor path is hazardous.” Then deepen projection-specific assertions/logging around output sizing, bounds assumptions, push-constant layout/size, and immediate post-dispatch lifetime/sync evidence. Keep the changes diagnostic and reversible, update this plan with actual results, run relevant validation, commit/push the instrumentation updates, and close bead `oc-dew` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_renderer.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/shaders/compute/gsplat_scratch_probe.glsl`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a dedicated trivial compute control named `scratch_only` without replacing the staged repro model. The new stage runs a one-workgroup scratch-buffer pipeline (`gsplat_scratch_probe`) before any real GDGS projection/radix/boundary/render work, then immediately CPU-readbacks the 16-byte probe buffer so QA can distinguish “any real compositor-path compute dispatch is hazardous” from “projection-specific work is the first bad stage.” Projection diagnostics were also deepened in the live projection path: CPU-side assertions now enforce positive output sizing, tile-grid derivation, point-count/state-capacity consistency, boundary-capacity assumptions, and the exact observed projection push-constant contract (`128` bytes / `32` floats / `mat4 view + mat4 projection`). Immediate post-dispatch evidence now logs a blocking readback of the projection histogram header (`sort_buffer_size`) plus RID-validity for the key projection outputs right after `compute_list_end()`, giving QA/audit a tighter handhold on whether the first dispatch completed and whether the written counts stayed within the allocated sort capacity. Validation run stayed repo-local and reversible: `git diff --check`, `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py` on the touched GDGS scripts/shader, and `godot --headless --path . --script ... --check-only --quit` for the touched GDScript entrypoints plus a headless project load. Landed as GDGS commit `e3be761` plus Godot/docs commit `d54d58ae`. No speculative fix was attempted; this pass is strictly instrumentation.

---

### Task 6: Rerun staged QA with scratch-only control and tightened projection diagnostics

**Bead ID:** `oc-sib`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-sib` and rerun the staged GDGS compositor QA using the updated instrumentation package. Prioritize `callback_only`, `prepared_no_dispatch`, `scratch_only`, and `projection_only`. Use `--accurate-breadcrumbs` on at least one rerun of the first failing meaningful boundary. Capture whether `scratch_only` survives or fails, whether `projection_only` still becomes the first failing stage, and what the new readback/assertion diagnostics say. Save durable notes/artifact references in repo-owned docs, update this plan with actual findings, and close bead `oc-sib` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the staged compositor isolation on the updated instrumentation package and wrote the follow-up evidence into `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Important runtime drift was encountered and documented exactly: the current managed `/home/derrick/.local/bin/godot` has drifted to `Godot 4.6.2.stable.official.71f334935`, which immediately reports `No loader found for resource: res://addons/gdgs/runtime/render/shaders/compute/gsplat_scratch_probe.glsl` on the updated GDGS branch, so QA used the preserved dev5 repro binary instead: `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`. Exact executed stages on that runtime: `callback_only` survived; `prepared_no_dispatch` survived; `scratch_only` exited cleanly; `projection_only` remained the first failing meaningful stage in both the normal and `--accurate-breadcrumbs` reruns. However, the new scratch control is only a partial win right now: the stage returns `0`, but the scratch probe shader/pipeline never actually materializes (`No loader found for resource: .../gsplat_scratch_probe.glsl`, later `Parameter "pipeline" is null`, `Parameter "uniform_set" is null`, and repeated zero readbacks `scratch_words=0x00000000,0x00000000,0x00000000,0x00000000`), so QA cannot yet claim that a valid known-safe trivial compute dispatch has been demonstrated safe. The projection diagnostics remained high-signal and consistent across both reruns: `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`; barrier completes; the immediate post-dispatch readback logs `sort_buffer_size=0`, `sort_capacity=2711230`, `sort_within_capacity=true`, and the key projection output RIDs stay valid before the later device loss at `fence_wait` / `BLIT_PASS`. Artifact roots: normal run `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-dev5-20260517-124210/`; accurate rerun `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-dev5-accurate-20260517-124210/`. This closes the QA task because the requested evidence package is complete for the current branch state: projection still fails first, scratch-only does not crash but is not yet a valid positive control, and the drift plus readback caveats are now durable.

---

### Task 7: Fix the scratch-only control so it becomes a real positive-control dispatch

**Bead ID:** `oc-x5q`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-x5q` and fix the `scratch_only` control so it becomes a real positive-control compute dispatch in the staged compositor repro. The goal is not a fix for the crash; it is to make the scratch probe shader/resource/pipeline/uniform path valid and observable, so QA can cleanly distinguish “any real compute dispatch here is hazardous” from “projection-specific dispatch is hazardous.” Keep changes diagnostic and reversible, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-x5q` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- scratch-control source files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Fixed the broken `scratch_only` positive control without changing the staged repro model. Root cause was not the scratch GLSL itself or the dispatch wrapper; it was packaging/resource plumbing: `addons/gdgs/runtime/render/shaders/compute/gsplat_scratch_probe.glsl` existed, but unlike the other compute shaders it had no companion `.glsl.import` remap, so Godot could not load it as an `RDShaderFile`. That left the scratch shader RID invalid, the scratch uniform set effectively unusable, and the `gsplat_scratch_probe` pipeline path observationally collapsing into a no-op with null-pipeline/null-uniform errors and all-zero readback. The fix was to add the missing `gsplat_scratch_probe.glsl.import` resource metadata in the GDGS repo so both the managed runtime and the preserved 4.7-dev5 repro binary can resolve the shader into SPIR-V, then tighten the diagnostic path with explicit validity assertions for the scratch shader/uniform set and a readback signature check (`GDGS`, workgroup count `1`, workgroup size `1`, sentinel tail word) so QA can tell the difference between “dispatch really executed” and “stage silently degraded.” Validation run: `git diff --check`; `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py` on the touched GDGS scripts/shader; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd --check-only --quit`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_renderer.gd --check-only --quit`; `godot --headless --path . --import` to materialize the new shader import; and a dedicated headless load probe run on both `/home/derrick/.local/bin/godot` (`4.6.2.stable`) and `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`, both of which now report `shader_file_null=false` and `spirv_null=false` for the scratch probe resource. This pass stays diagnostic/reversible and does not attempt to change the projection failure itself.

---

### Task 8: QA rerun after the scratch positive-control fix

**Bead ID:** `oc-6li`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-6li` and rerun the staged compositor QA after the scratch positive-control fix. Focus on `scratch_only`, `projection_only`, and `projection_only --accurate-breadcrumbs` if needed. Confirm whether the scratch probe now executes as a real compute dispatch with the expected nonzero signature/readback, and whether `projection_only` still remains the first meaningful failing stage. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-6li` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the staged compositor isolation after the scratch positive-control fix using the same updated instrumentation branches in both repos and wrote the follow-up evidence into `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). To keep the answer directly comparable to the earlier projection-first evidence package and avoid unnecessary runtime drift, QA used the preserved repro binary again: `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5.official.a8643700c`). Exact executed stages: `scratch_only`; `projection_only`; `projection_only --accurate-breadcrumbs`. New artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/`. The scratch control is now a real positive-control compute dispatch rather than a degraded no-op: logs repeatedly show `scratch_shader_valid=true`, `scratch_buffer_valid=true`, `rd dispatch pipeline=gsplat_scratch_probe ... group_count=(1, 1, 1)`, barrier completion, and stable nonzero readback `scratch_words=0x47534744,0x00000001,0x00000001,0x5a5aa5a5` with `scratch_signature_ok=true`. `scratch_only` exits cleanly (`0`). `projection_only` still remains the first meaningful failing stage in both normal and accurate reruns (`134`): the projection dispatch/breadcrumb chain is unchanged (`push_constant_bytes=128`, `group_count=(1060, 1, 1)`, barrier complete, projection end), the immediate post-dispatch readback still reports `sort_buffer_size=0`, `sort_capacity=2711230`, and `sort_within_capacity=true` with key RIDs valid, then the failure still surfaces at `fence_wait` and the last known lost-device breadcrumb still collapses to `BLIT_PASS`. This closes the QA evidence gap left by bead `oc-sib`: a valid compositor-path scratch dispatch can execute safely, so the broad theory that any real compute dispatch here is inherently hazardous is demoted, while projection-specific work or projection-triggered synchronization/lifetime fallout remains the leading suspect lane.

---

### Task 9: Deep-inspect projection dispatch contract and bounds/lifetime fallout

**Bead ID:** `oc-wz6`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-wz6` and prepare the next diagnostic slice focused narrowly on the projection pass. Deep-inspect the projection dispatch contract, projection output writes/bounds assumptions, and immediate post-dispatch lifetime/synchronization fallout. Prefer high-signal diagnostics/assertions/readbacks over speculative fixes. If possible, add instrumentation that can distinguish bad projection outputs from later consumption/sync failure without broadening back into later-stage guesswork. Keep changes diagnostic and reversible, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-wz6` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- projection-path source files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Kept this slice narrowly on projection and its immediate aftermath, without attempting a crash fix. The companion GDGS branch stayed on `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` and landed commit `ba83c1c` (`debug: deepen projection dispatch diagnostics`). New diagnostics/assertions added in the GDGS repo: (1) explicit CPU-side contract assertions for projection-owned upload/layout assumptions (`Splat` stride `240` bytes / `60` floats, `RasterizeData` stride `64` bytes / `16` floats, instance-id upload width, instance-transform upload width, existing `128`-byte / `32`-float push-constant contract, and projection-probe RID validity); (2) a dedicated projection probe SSBO bound only to `gsplat_projection`, seeded before dispatch and atomically filled by the shader with invocation/visibility/duplication counts, emitted sort-element totals, max exclusive sort end, max tile id, max tiles touched, and zero-tile splat count; and (3) stronger immediate post-dispatch readback that now logs the projection probe words plus first-word snapshots of `sort_keys`, `sort_values`, and `culled_splats` after projection, instead of only logging `sort_buffer_size` and RID validity. This matters because the next QA pass can now distinguish several cases that previously collapsed together: “projection emitted nothing but stayed within contract,” “projection duplicated splats and produced plausible bounded outputs,” “projection exceeded sort/tile bounds assumptions before later stages ever touched the data,” or “projection outputs look sane and the device is still lost later, which pushes suspicion toward post-projection lifetime/synchronization fallout instead of raw projection writes.” Validation run for the touched repo-local files: `git diff --check`; `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd addons/gdgs/runtime/render/gaussian_renderer.gd addons/gdgs/runtime/render/shaders/compute/gsplat_projection.glsl`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd --check-only --quit`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_renderer.gd --check-only --quit`; and `godot --headless --path . --import`. No Godot engine source files changed in this pass; the Godot repo changes are the plan/doc handoff updates only.

---

### Task 10: QA projection probe readback after deep projection diagnostics

**Bead ID:** `oc-4wb`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-4wb` and rerun the focused projection QA using the new projection-probe/readback instrumentation. Prioritize `projection_only` and `projection_only --accurate-breadcrumbs`, and capture whether duplicated splats or sort-key writes occur at all, whether `probe_max_sort_end` and `probe_max_tile_id` stay within capacity, whether sentinels show outputs were written, and whether the evidence points to immediate projection bounds violation versus later sync/lifetime fallout. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-4wb` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran only the two requested focused repros on the updated instrumentation branches, again using the preserved dev5 repro runtime for comparability and to avoid the already-documented managed-runtime drift: `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5.official.a8643700c`). Exact runs: `projection_only` and `projection_only --accurate-breadcrumbs`, both from artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-probe-dev5-20260517-154354/`, and both exited `134`. The important new result is tighter than the prior `oc-6li` evidence: the projection dispatch contract still looks stable before readback (`push_constant_bytes=128`, `push_constant_floats=32`, `projection_group_count=1060`, `tile_bounds_capacity=2952`, `sort_capacity=2711230`, dispatch + barrier + `projection_end` all log), but the new immediate post-dispatch readback package never successfully emits `projection_post_dispatch`. Instead, the failure now surfaces inside `_log_projection_post_dispatch_evidence(...)` itself while attempting the first blocking `buffer_get_data(...)` calls (backtrace hits lines `322` histogram readback and `325` projection-probe readback), with `fence_wait` failure and lost-device breadcrumbs still collapsing to `BLIT_PASS`. That means this QA pass did **not** obtain concrete probe/readback values for duplicated splats, emitted sort elements, `probe_max_sort_end`, `probe_max_tile_id`, or the first-word sentinel snapshots of `sort_keys`, `sort_values`, and `culled_splats`; the CPU never received them before the device-loss path tripped. So the evidence package is complete in the narrower sense requested by Task 10: it does **not** show a logged immediate projection bounds violation, and it points more strongly toward projection-triggered synchronization/lifetime/device-loss fallout that becomes visible at the first blocking readback fence. Durable notes and artifact references were appended to `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`).

---

### Task 11: Split projection post-dispatch readbacks into minimal checkpoints

**Bead ID:** `oc-bjw`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-bjw` and keep the investigation projection-only. Split the current post-projection readback package into minimal, individually togglable checkpoints so QA can test whether the device is already lost before any readback, or whether one specific readback is the first detonator. Prioritize isolated stages for: histogram header only, projection probe only, `sort_keys` sentinel only, `sort_values` sentinel only, and `culled_splats` sentinel only. Keep the changes diagnostic and reversible, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-bjw` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- projection-path source files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Kept this pass projection-only and split the immediate post-dispatch readback bundle into a dedicated, minimal checkpoint control surface instead of broadening instrumentation further. In the GDGS repo, added a new `ProjectionReadbackCheckpoint` debug enum in `addons/gdgs/runtime/render/gaussian_renderer.gd`, threaded it through `addons/gdgs/runtime/render/gaussian_render_manager.gd`, and exposed it on the compositor effect via `addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd` as `debug_projection_readback_checkpoint`. The control keeps the old `full_package` behavior available for comparison, adds an explicit `disabled` / no-readback mode so QA can prove whether projection still detonates before any CPU readback at all, and adds the five requested individually togglable minimal checkpoints: `histogram_header_only`, `projection_probe_only`, `sort_keys_sentinel_only`, `sort_values_sentinel_only`, and `culled_splats_sentinel_only`. Each checkpoint now logs a begin marker before the targeted `buffer_get_data(...)` call and an end marker only if that specific readback returns, which matters because QA can now isolate the first detonating fence/readback instead of losing all evidence inside the old combined package. Validation run for the touched files stayed repo-local and reversible: `git diff --check`; `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py addons/gdgs/runtime/render/gaussian_renderer.gd addons/gdgs/runtime/render/gaussian_render_manager.gd addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_renderer.gd --check-only --quit`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_render_manager.gd --check-only --quit`; `godot --headless --path . --script addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd --check-only --quit`; and `godot --headless --path . --import`. No crash fix was attempted; this pass is strictly diagnostic and reversible.

---

### Task 12: QA isolate the first toxic projection readback checkpoint

**Bead ID:** `oc-15g`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-15g` and keep the investigation projection-only. Run the minimal readback checkpoint sequence in this order: `disabled`, `histogram_header_only`, `projection_probe_only`, `sort_keys_sentinel_only`, `sort_values_sentinel_only`, `culled_splats_sentinel_only`, and only rerun the first failing one with `--accurate-breadcrumbs` if needed. Determine whether `projection_only` still crashes with readbacks disabled, and if not, which exact readback is the first detonator. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-15g` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA ran the new checkpoint-isolation pass on the same updated instrumentation branches in both repos, again using the preserved dev5 repro runtime for comparability and to avoid the already-documented managed-runtime drift: `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5.official.a8643700c`). Because the existing stage harness did not yet expose `debug_projection_readback_checkpoint`, QA used a temporary wrapper harness at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` that only threads the new checkpoint enum into the already-established repro scene without modifying repo code. Exact runs: `projection_only + disabled`; `projection_only + disabled + --accurate-breadcrumbs`, both from artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/`, and both exited `134`. Per the minimum-runs constraint, QA stopped there because the very first checkpoint in the required order already failed. The important answer is sharper than expected: `projection_only` still crashes even with the entire post-projection readback package disabled, so QA did **not** isolate a toxic readback checkpoint at all. Both logs show the disabled checkpoint path itself completing (`projection_post_dispatch_checkpoint_begin` → `projection_post_dispatch_checkpoint_disabled` → `projection_post_dispatch_checkpoint_end`) before the later failure, while the broader signature remains unchanged: projection dispatch still uses the stable `128`-byte / `32`-float contract with group count `(1060, 1, 1)`, compositor still returns through `raster_only_no_writeback_gate`, failure still first surfaces at `fence_wait`, and lost-device breadcrumbs still collapse to `BLIT_PASS`. That demotes `histogram_header_only`, `projection_probe_only`, `sort_keys_sentinel_only`, `sort_values_sentinel_only`, and `culled_splats_sentinel_only` as candidate first detonators in the current repro. Durable notes and artifact references were added to `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`).

---

### Task 13: Add projection-internal GPU-side guards and sentinel evidence

**Bead ID:** `oc-33u`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-33u` and keep the investigation projection-internal. Add high-signal GPU-side guards, sentinels, or shader-side diagnostics inside the projection lane so QA can determine whether projection is immediately violating bounds or producing obviously bad internal state before later fence/device-loss fallout. Prefer small reversible instrumentation over speculative fixes; keep the staged repro model intact; update this plan with actual results; run relevant validation; commit/push the updates; and close bead `oc-33u` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- projection shader/runtime files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added projection-only GPU-side guard/sentinel instrumentation on the active GDGS instrumentation branch rather than broadening the repro. The projection probe SSBO was expanded from 13 to 24 words so the shader can now persist extra evidence without depending on the full CPU readback package. New words record `error_flags`, total guard aborts, the first failing stage/id/value payload, `max_requested_sort_end`, `max_requested_tile_id`, and per-class counts for non-finite failures, sort-overflow guards, and tile/rect guards. Inside `gsplat_projection.glsl`, the projection lane now records and early-outs on non-finite view/clip/covariance/eigen/image/radius/conic/color/view-depth states, validates rect bounds, safely reserves `sort_buffer_size` via an atomic compare-exchange loop instead of blindly overrunning it, and refuses tile-id writes outside the seeded tile capacity while recording the offending values in the probe. On the GDScript side, the renderer seeds the expanded probe layout and now decodes/logs the new guard fields so future `projection_probe_only` or full-package runs can tell whether projection immediately hit guarded bad state even if later fence/device-loss fallout still happens. Validation run on the touched GDGS files: `git diff --check`; `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd addons/gdgs/runtime/render/gaussian_renderer.gd addons/gdgs/runtime/render/shaders/compute/gsplat_projection.glsl`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd --check-only --quit`; `godot --headless --path . --script addons/gdgs/runtime/render/gaussian_renderer.gd --check-only --quit`; `godot --headless --path . --import`; and `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64 --headless --path . --import`. This keeps the investigation projection-internal, adds small reversible guards instead of speculative fixes, and sets up the next QA pass to ask whether `projection_only` now survives long enough to emit guarded probe evidence or whether the device still dies with all new guard counters remaining clean.

---

### Task 14: QA rerun with projection GPU guard diagnostics

**Bead ID:** `oc-bl3`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-bl3` and keep the investigation projection-only. Rerun the staged repro with the new GPU-side projection guard diagnostics. Prioritize `projection_only` and, if useful, `projection_probe_only` or the smallest readback mode that can surface the probe. Determine whether the new guards stay clean or report an immediate guarded failure class before the later fence/device-loss path. Capture the most important probe fields, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-bl3` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the projection-only staged repro on the updated instrumentation branches and wrote the durable evidence into `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). To keep the answer comparable to the earlier dev5 evidence package, QA used the preserved repro binary again: `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5.official.a8643700c`). Important runtime note captured in the doc: an initial noninteractive `--headless` retry fell into the dummy renderer and was discarded as invalid, so the valid runs used the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000` with `--display-driver wayland --rendering-driver vulkan`). Exact valid runs: `projection_only + projection_probe_only`; then `projection_only + projection_probe_only + --accurate-breadcrumbs`, both exiting `134`, with artifacts under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-20260517-180114/` and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-accurate-20260517-180149/`. The new answer is a useful negative result: the smallest useful readback mode (`projection_probe_only`) now returns the expanded probe package, but every new GPU-side guard field stays clean in both reruns — `probe_error_flags_hex=0x00000000`, `probe_first_failure_stage_name=none`, `probe_first_failure_id=0`, `probe_max_requested_sort_end=0`, `probe_max_requested_tile_id=0`, `probe_sort_overflow_guard_count=0`, `probe_tile_guard_count=0`, with the broader `probe_words` array also all zeros. Despite that, the failure still surfaces at `fence_wait` during `_log_projection_probe_readback(...)`, and the later lost-device breadcrumbs still collapse to `BLIT_PASS`. This means QA did not observe an immediate guarded projection failure class before the later device-loss path; the current evidence keeps suspicion on projection-triggered synchronization/lifetime/backend fallout, or on the possibility that the probe buffer itself is not becoming observably coherent before the device-loss path trips.

---

### Task 15: Investigate projection probe visibility/coherency and post-dispatch sync hazards

**Bead ID:** `oc-lnp`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-lnp` and keep the investigation projection-only. Move the next diagnostic slice onto probe visibility/coherency and post-dispatch synchronization/lifetime hazards. Add small, reversible, high-signal instrumentation that can help distinguish: (1) projection produced outputs but probe/read visibility is incoherent, versus (2) projection outputs or resource usage are entering a sync/lifetime/backend hazard before later fence/device-loss fallout. Keep the staged repro model intact, avoid speculative fixes, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-lnp` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/compositor/gaussian_compositor_effect.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_renderer.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/shaders/compute/gsplat_projection.glsl`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Kept this slice projection-only and focused it specifically on the visibility/coherency-vs-sync-hazard fork, without attempting a crash fix. On the active GDGS instrumentation branch, added a second projection-side evidence path by binding the existing scratch probe buffer into `gsplat_projection` and mirroring a few high-signal counters/stage bits there (`invocations`, `visible_splats`, stage-bit milestones for entry / visible path / culled write / sort reservation / sort writes, and max requested sort end). This matters because the scratch probe path is already a known-good positive-control buffer/readback in this repro harness, so the next QA pass can compare `projection_probe_only` against a new `scratch_projection_mirror_only` checkpoint to tell whether the all-zero projection probe is a probe-visibility/coherency problem or whether projection is already poisoning broader post-dispatch state before either probe can be observed. To support the second half of that distinction, the pass also added lightweight post-dispatch lifetime breadcrumbs: per-state `last_projection_dispatch_serial` tracking logged at projection begin/end and again from GPU-state cleanup paths (`request_cleanup`, `flush_pending_cleanup`, `cleanup_state`) so QA/audit can see if resource teardown is racing suspiciously close to a failing projection dispatch. The full-package projection readback now includes the scratch-mirror fields too, and the compositor effect exposes the new `Scratch Projection Mirror Only` checkpoint without changing the staged repro model. Validation run: `git diff --check`; `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py` on the touched GDGS scripts/shader; `godot --headless --path . --script ... --check-only --quit` for the touched compositor/cache/renderer scripts on `/home/derrick/.local/bin/godot`; `godot --headless --path . --import` on both `/home/derrick/.local/bin/godot` (`4.6.2.stable`) and `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5`). No Godot engine source files changed in this pass; the Godot repo changes here are plan/doc handoff updates only.

---

### Task 16: QA compare projection probe visibility against scratch mirror visibility

**Bead ID:** `oc-2cr`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-2cr` and keep the investigation projection-only. Use the new `scratch_projection_mirror_only` checkpoint and compare it against `projection_probe_only`. Determine whether projection activity/stage bits become visible in the known-good scratch mirror path even when the main projection probe remains zero or unreadable. Capture the most important mirror/probe fields, preserve durable notes/artifact references, update this plan with actual findings, and close bead `oc-2cr` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA compared the two requested minimum projection-only checkpoints on the active instrumentation branches and recorded the durable evidence in `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). To stay directly comparable to the earlier valid Vulkan evidence and avoid the already-documented managed-runtime drift / dummy-renderer trap, the pass again used the preserved repro binary `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` (`4.7.dev5.official.a8643700c`) on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Exact runs: `projection_only + projection_probe_only`; `projection_only + scratch_projection_mirror_only`; both exited `134`, with artifacts under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-mirror-vulkan-dev5-20260517-18222280927/`. Important harness note: the temporary checkpoint runner in `.temp/` did not yet know about the new `scratch_projection_mirror_only` enum value, so QA patched only that temp harness mapping (not repo code) to run the second checkpoint. The answer is a useful negative result: both checkpoints preserve the same projection launch contract (`128`-byte / `32`-float push constants, `group_count=(1060, 1, 1)`, barrier complete), both still proceed through `projection_only_gate`, and the device still later fails at `fence_wait` with lost-device breadcrumbs collapsing first to `BLIT_PASS`. Crucially, `projection_probe_only` remained all zero / non-observing (`probe_words=[0, ...]`, `probe_invocations=0`, `probe_visible_splats=0`, `probe_error_flags_hex=0x00000000`), and the supposedly known-good scratch mirror path did **not** reveal hidden projection activity either: `scratch_words` was entirely zero, `scratch_signature_ok=false`, `scratch_projection_stage_bits=0x00000000`, `scratch_projection_entered=false`, `scratch_projection_visible_path=false`, `scratch_projection_sort_reserved=false`, and `scratch_projection_sort_written=false`. So the new mirror path does not presently show projection activity/stage bits becoming visible anywhere that the main probe missed; both evidence buffers remain zero-valued while the later failure signature stays unchanged. This closes the QA evidence package for bead `oc-2cr` and points the next recommendation back toward projection-triggered synchronization / lifetime / backend fallout or broader visibility/coherency failure affecting both probe paths.

---

### Task 17: Inspect projection post-dispatch resource lifetime and cleanup hazards

**Bead ID:** `oc-hm0`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-hm0` and keep the investigation projection-only. Focus on post-dispatch resource lifetime, cleanup timing, aliasing, and synchronization/backend hazard evidence around the projection outputs and associated buffers. Add small, reversible, high-signal instrumentation that can show whether cleanup/reuse/rebinding or lifetime transitions line up suspiciously close to the failing projection dispatch and later fence/device-loss fallout. Keep the staged repro model intact, avoid speculative fixes, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-hm0` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_renderer.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_render_manager.gd`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/addons/gdgs/runtime/render/gaussian_scene_registry.gd`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a projection-only lifetime / cleanup instrumentation pass in GDGS without changing staged behavior. `gaussian_gpu_state_cache.gd` now tracks per-state `gpu_generation`, remembers the last cleanup request serial + reason, snapshots the key projection RIDs (projection/scratch probe buffers, histogram, sort buffers, culled buffer, tile bounds, render/depth textures, projection descriptor set, scratch descriptor set, projection pipeline), and logs those snapshots at cleanup request, cleanup flush, cleanup_state, and rebuild time. `gaussian_renderer.gd` now includes the same projection resource snapshot plus cleanup-request metadata directly on `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`, and it flags unexpected RID aliasing across the critical projection-owned resources. `gaussian_render_manager.gd` now passes cleanup reasons through to the state cache, and `gaussian_scene_registry.gd` tags the empty-scene cleanup path as `scene_registry_empty`. This gives QA a way to correlate: (1) which exact projection-owned resources were bound when the failing dispatch launched, (2) whether those handles were later torn down or regenerated before the fence/device-loss fallout, and (3) whether any suspicious aliasing/rebinding shows up across buffers or outputs that should stay distinct. Validation: source parse/load check against the preserved repro binary `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` with a temporary `--headless -s` loader script; result `parse_ok`.

---

### Task 18: QA correlate projection resource lifetime and cleanup hazard evidence

**Bead ID:** `oc-x6n`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-x6n` and keep the investigation projection-only. Run a focused `projection_only` repro using the new lifetime/cleanup diagnostics and determine whether the projection resource snapshot remains stable from `projection_begin` through `projection_post_dispatch_checkpoint_end`, or whether any cleanup request, pending cleanup flush, rebuild, RID alias event, or resource-identity change involving those exact projection-owned resources occurs before the later `fence_wait` / `BLIT_PASS` device-loss collapse. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-x6n` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA ran the minimum valid host-Vulkan repro on the updated instrumentation branches using the preserved dev5 runtime (`/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`) with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`, stage `projection_only`, and readback checkpoint `disabled`. Durable notes/artifacts were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-vulkan-dev5-20260517-18492290849/`. The useful answer is partial but clear: no `request_cleanup`, `flush_pending_cleanup`, post-dispatch `cleanup_state`, or second `rebuild_gpu_state` event was logged between `projection_begin` and `projection_post_dispatch_checkpoint_end`; `gpu_generation` stayed `1`, `projection_dispatch_serial` stayed `1`, `cleanup_request_serial` stayed `0`, and `cleanup_request_reason` stayed `none` before the later `fence_wait` / `BLIT_PASS` device-loss collapse. However, the exact projection-resource identity comparison requested by this task is blocked on a new instrumentation defect discovered during QA: both `gaussian_gpu_state_cache.gd` and `gaussian_renderer.gd` currently throw `Invalid type in function '_rid_string' ... Cannot convert argument 1 from Callable to RID` inside `_projection_resource_snapshot()`, so every logged `projection_resource_snapshot` degraded to `{}` and QA could not truthfully compare the exact RID snapshot or alias set across `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`. This closes the QA evidence package for the current branch state because the cleanup-timing correlation question is answered as a negative result, the remaining gap is now explicitly narrowed to the broken snapshot helper itself, and the next recommendation is to repair that helper before spending more runs on the same lifetime lane.

---

### Task 19: Fix the projection resource snapshot helper and rerun lifetime correlation

**Bead ID:** `oc-8ao`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-8ao` and keep the investigation projection-only. Fix the broken `_projection_resource_snapshot()` / RID-string helper path so the resource snapshot and alias diagnostics become trustworthy, while keeping the work diagnostic and reversible. Update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-8ao` with a clear reason if complete. The immediate follow-up QA goal is to rerun the same `projection_only + disabled` lifetime correlation with a functioning snapshot path.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- projection runtime files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Root cause confirmed from the Task 18 QA evidence: the new projection snapshot helper was still passing `state.pipelines[...]` through `_rid_string(rid: RID)`, but in this codepath those entries are `Callable` dispatch closures returned by `state.context.create_pipeline(...)`, not `RID` values. That made `_projection_resource_snapshot()` throw `Cannot convert argument 1 from Callable to RID`, which collapsed every logged `projection_resource_snapshot` to `{}` and blocked the intended lifetime/alias comparison. The fix stayed diagnostic and reversible: in both `addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd` and `addons/gdgs/runtime/render/gaussian_renderer.gd`, coder split pipeline serialization from RID serialization by (1) keeping `_rid_string()` strictly RID-only, (2) adding `_descriptor_set_rid_string()` for the real descriptor-set RIDs, and (3) adding `_pipeline_snapshot_string()` that records pipeline presence as `Callable(valid=true|false)` instead of pretending pipeline closures are RIDs. The snapshot helper was also tightened so alias reporting groups the tracked projection-owned resource RIDs by member name and only reports duplicate groups, which is more truthful for the next lifetime rerun than the earlier flat duplicate list. Validation run: `timeout 15s /home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64 --headless --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --quit` exited `0` with no script parse/type errors from the touched files, plus a small static sanity check confirmed the new helper paths landed in both runtime files. Follow-up QA is still required to rerun the same `projection_only + disabled` Vulkan pass and compare the now-working snapshot payloads at `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`.

---

### Task 20: QA rerun projection lifetime correlation after the snapshot-helper fix

**Bead ID:** `oc-k2b`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-k2b` and keep the investigation projection-only. Rerun the same minimum valid host-Vulkan repro (`projection_only + disabled`) after the snapshot-helper fix. Compare `projection_resource_snapshot` across `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`, and determine whether the tracked projection-owned resources stay stable and whether any duplicate `alias_groups` appear before the later `fence_wait` / `BLIT_PASS` collapse. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-k2b` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan repro after the snapshot-helper repair using the preserved dev5 runtime `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`) with stage `projection_only` and readback checkpoint `disabled`. Durable notes/artifacts were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-snapshotfix-vulkan-dev5-20260517-191507/`. This rerun closes the resource-snapshot evidence gap left by bead `oc-x6n`: the repaired helper now emits a full `projection_resource_snapshot`, and that snapshot stays stable from `projection_begin` through `projection_end` and `projection_post_dispatch_checkpoint_end`. Across those three points, the tracked projection-owned resources held the same identities (`culled_splats=RID(11171209936915)`, `depth_texture=RID(11227044511803)`, `histogram=RID(11179799871509)`, `projection_probe=RID(11218454577181)`, `projection_set=RID(11231339479061)`, `render_texture=RID(11222749544506)`, `scratch_probe=RID(11214159609884)`, `scratch_probe_set=RID(11257109282843)`, `sort_keys=RID(11184094838806)`, `sort_values=RID(11188389806103)`, `tile_bounds=RID(11205569675290)`, with both `projection_pipeline` and `scratch_pipeline` still `Callable(valid=true)`), `aliasing_detected` stayed `false`, and `alias_groups` stayed `{}`. The lifecycle/correlation fields also stayed flat in the same window: `gpu_generation=1`, `projection_dispatch_serial=1` at all three stage checkpoints, `cleanup_request_serial=0`, and `cleanup_request_reason=none`, with no `request_cleanup`, `flush_pending_cleanup`, post-dispatch `cleanup_state`, or second `rebuild_gpu_state` logged before the later failure. Despite that stable snapshot window, the broader failure signature is unchanged: the disabled checkpoint path completes, `projection_only_gate` and `raster_only_no_writeback_gate` still log, and the device is still later lost at `fence_wait` with breadcrumbs collapsing to `BLIT_PASS`. This closes the QA bead with a complete evidence package for the requested lifetime/alias question: the tracked projection-owned resource set is stable and non-aliased before the later collapse, so the next recommendation is to move the investigation farther down the synchronization/backend/fence path rather than rerunning the same snapshot-stability check.

---

### Task 21: Instrument the post-projection sync and backend hazard path

**Bead ID:** `oc-b8r`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-b8r` and keep the investigation projection-only. The current evidence says the projection dispatch is the first bad event, but not because of CPU readbacks, tracked cleanup churn, or tracked RID aliasing. Add small, reversible, high-signal instrumentation around the post-projection synchronization/backend path after `projection_post_dispatch_checkpoint_end` so QA can narrow where the GPU/backend path first becomes unhealthy before the later `fence_wait` / `BLIT_PASS` collapse. Prefer engine-adjacent or plugin-side sync/lifecycle evidence over repeating earlier bounds/probe work. Update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-b8r` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- sync/backend diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Kept the investigation projection-only and added two small diagnostic seams instead of more bounds/probe churn. On the GDGS side, `gaussian_renderer.gd` now returns a `debug_sync_snapshot` alongside the compositor textures, carrying `gpu_generation`, `projection_dispatch_serial`, `cleanup_request_serial`, `cleanup_request_reason`, and the full `projection_resource_snapshot` after `projection_post_dispatch_checkpoint_end`; `gaussian_compositor_effect.gd` logs that snapshot immediately at `render_for_compositor_returned`, which lets QA confirm whether the post-projection state that was stable inside the renderer still survives the callback-return seam right before the compositor exits at `raster_only_no_writeback_gate`. On the Godot side, `rendering_device.cpp` now logs `frame_execute_begin`, `frame_execute_submitted`, `frame_stall_begin`, and `frame_stall_end` with frame-local wait-semaphore, swapchain, and pending-download counts, while `rendering_device_driver_vulkan.{h,cpp}` records per-fence submission metadata (`submit_serial`, queue family/index, wait semaphore count, command buffer count, signal semaphore count, swapchain count, pending fence image semaphores, present-submission flag) and prints it at `queue_submit`, `fence_wait_begin`, `fence_wait_end`, and `fence_wait_error`. This gives QA a tighter chain from `projection_post_dispatch_checkpoint_end` -> compositor return seam -> frame submission -> fence wait, without changing sync behavior or introducing speculative fixes. Validation run: GDGS GDScript parse/load sanity via `timeout 20s .../Godot_v4.7-dev5_linux.x86_64 --headless --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --quit` exited `0`, and targeted `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp servers/rendering/rendering_device.cpp` passed for the Godot C++ edits. Recommended next QA question: in the first failing `projection_only` rerun with these logs enabled, does the returned `debug_sync_snapshot` stay identical through `render_for_compositor_sync_snapshot`, and if so, what is the first suspicious engine/backend transition — `queue_submit`, `frame_stall_begin`, or `fence_wait_begin/error` — before the familiar `BLIT_PASS` collapse?

---

### Task 22: QA correlate the post-projection submit/stall/fence transition

**Bead ID:** `oc-c1f`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-c1f` and keep the investigation projection-only. Run the minimum valid host-Vulkan repro (`projection_only + disabled`) using the new post-projection sync/backend instrumentation. Determine whether `render_for_compositor_sync_snapshot` stays stable, and identify the first suspicious transition after that snapshot: `queue_submit`, `frame_stall_begin`, `fence_wait_begin`, or `fence_wait_error`, before the later `BLIT_PASS` collapse. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-c1f` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA completed the minimum valid host-Vulkan source-built rerun and answered the post-projection transition question directly. Because the fresh Godot instrumentation worktree would not compile as-written (`VectorView<SwapChainID>` has no `is_empty()`), QA applied the minimum build-unblock change in `drivers/vulkan/rendering_device_driver_vulkan.cpp` (`!p_swap_chains.is_empty()` -> `p_swap_chains.size() > 0`) so the new engine-side logs could actually be exercised. QA then built `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` from the current Godot worktree and ran exactly one real-GPU repro: `projection_only + disabled` on the host Wayland/Vulkan path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`) via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`. Durable artifacts were saved under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-postprojection-sync-sourcebuild-20260517-201958/`, with findings documented in `doc/gdgs-compositor-staged-qa-2026-05-17.md`.

The returned `render_for_compositor_sync_snapshot` stayed stable: `gpu_generation=1`, `cleanup_request_serial=0`, `cleanup_request_reason=none`, `projection_dispatch_serial=1`, `aliasing_detected=false`, `alias_groups={}`, and the tracked `projection_resource_snapshot` remained byte-for-byte stable from `projection_begin` through `projection_post_dispatch_checkpoint_end` into the callback-return snapshot. The first suspicious transition after that stable snapshot is the next backend `queue_submit`, specifically `submit_serial=9` on frame 1 (`queue_family=0`, `queue_index=0`, `wait_semaphores=1`, `command_buffers=1`, `signal_semaphores=0`, `swap_chains=0`, `pending_fence_image_semaphores=0`, `present_submission=false`). The immediately preceding post-return wait on `submit_serial=5` succeeds cleanly; then `submit_serial=9` is queued, `frame_stall_begin frame=1` follows, `fence_wait_begin submit_serial=9` starts, and the first explicit failure appears at `fence_wait_error submit_serial=9 wait_result=-4` with the Vulkan debug callback reporting `GPU hung on one of our command buffers (VK_ERROR_DEVICE_LOST)`. The later lost-device breadcrumb collapse is unchanged and still reports `BLIT_PASS`. Recommended next step: inspect what work / inherited semaphores are bound into the first non-present post-snapshot submission (`submit_serial=9`) rather than spending more time on the already-stable snapshot seam.

---

### Task 23: Map the failing post-projection submissions `submit_serial=8` and `submit_serial=9`

**Bead ID:** `oc-atc`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-atc` and keep the investigation projection-only. The current evidence says the first suspicious backend transition after a stable projection return is `queue_submit submit_serial=9`, with a clean earlier wait on `submit_serial=5` and a nearby `submit_serial=8`. Add small, reversible, high-signal instrumentation that maps what work `submit_serial=8` and `submit_serial=9` actually correspond to, including relevant command-buffer / submission labels, wait-semaphore context, and frame-stage ownership, so QA can tell why `submit_serial=9` queues cleanly but later dies at fence wait. Keep the staged repro model intact, avoid speculative fixes, update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-atc` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- sync/backend diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added focused projection-only submission mapping diagnostics in the Godot Vulkan backend and `RenderingDevice` call sites without changing staged repro behavior or attempting a fix. In `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp`, fences now retain per-submit `wait_summary`, `signal_summary`, and `command_summary` strings so the existing `queue_submit`, `fence_wait_begin`, and `fence_wait_error` logs carry the same submission context all the way through the later stall/fence failure. Command buffers are now stamped at `command_buffer_begin*()` with the active frame segment (`frame` / `frames_drawn`), accumulate first/last label plus a bounded label path from `command_begin_label()`, and retain last breadcrumb/count from `command_insert_breadcrumb()`. At submit time, the driver now logs: acquired-image waits vs external waits, raw semaphore handles for waits/signals, swapchain/image ownership for acquire/present semaphores, and command-buffer summaries including frame ownership, label path, and last breadcrumb. In `servers/rendering/rendering_device.cpp`, small host-side markers were added at `transfer_submit_begin` and `frame_execute_cmd_submit` so QA can correlate ordering: whether a submit came from transfer-worker upload staging or the main frame execution loop, with frame index, command-buffer slot, and whether a signal fence/semaphore was attached.

Why this matters: QA can now directly answer whether `submit_serial=8` is the transfer-worker handoff that seeds `frames[frame].semaphores_to_wait_on`, whether `submit_serial=9` is the subsequent main frame execution submission that consumes that exact semaphore, and what labeled command buffer / stage ownership was attached when the later `VK_ERROR_DEVICE_LOST` fence wait fires. That turns the current “submit 8 / submit 9 / BLIT_PASS later” sequence into an explicit chain of ownership instead of an ordering guess.

Validation completed on the touched code paths with `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp servers/rendering/rendering_device.cpp`, `git diff --check`, and a targeted successful object build: `scons platform=linuxbsd target=editor dev_mode=yes bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o bin/obj/servers/rendering/rendering_device.linuxbsd.editor.x86_64.o -j8`.

---

### Task 24: QA map `submit_serial=8` → `submit_serial=9` semaphore and command path

**Bead ID:** `oc-zbz`  
**SubAgent:** `primary` (for `qa`)  
**Role:** `qa`  
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`  
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-zbz` and keep the investigation projection-only. Run the minimum valid host-Vulkan repro (`projection_only + disabled`) using the new Vulkan submit/work mapping diagnostics. Determine whether `submit_serial=8` is the transfer-worker submission, whether `submit_serial=9` is the following frame-1 command submission waiting on that same semaphore chain, and what command-label path / breadcrumb context is attached to `submit_serial=9` when the later `fence_wait_error` fires. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-zbz` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA claimed bead `oc-zbz`, ran the minimum host-Vulkan repro on the source-built editor (`projection_only + disabled`) at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-map-sourcebuild-20260517-205949/`, and confirmed the submit-chain ownership question. The valid run again showed the stable post-projection snapshot, then a successful frame-0 wait on `submit_serial=5`, then `queue_submit submit_serial=8` with `wait_semaphores=0 command_buffers=1 signal_semaphores=1`, followed immediately by `frame_execute_begin frame=1 ... wait_semaphores=1` and `queue_submit submit_serial=9` with `wait_semaphores=1 command_buffers=1 signal_semaphores=0`, before the later `fence_wait_error submit_serial=9 wait_result=-4` and `BLIT_PASS` collapse. Cross-checking that runtime ordering against the updated engine source (`RenderingDevice::_submit_transfer_worker()` pushing signal semaphores into `frames[frame].semaphores_to_wait_on`, then `_execute_frame()` / `execute_chained_cmds()` consuming that same wait list) supports the intended answer: `submit_serial=8` is the transfer-worker handoff and `submit_serial=9` is the following frame-1 command submission waiting on the same semaphore chain. QA saved the durable notes in `doc/gdgs-compositor-staged-qa-2026-05-17.md`.

Important caveat: the source-built editor binary used for the valid repro still lagged the newest runtime log strings, so this artifact set does **not** include the newly added `wait_summary` / `signal_summary` / `command_summary` / `label_path` text for `submit_serial=9` even though that source is present on branch `e968db74`. QA therefore closed the bead on the strength of the answered ownership question, while explicitly documenting that richer command-label provenance would require the same single rerun on a refreshed binary if still needed.

---

### Task 25: Investigate the transfer-submit → frame-submit hazard on failing `submit_serial=9`

**Bead ID:** `oc-d79`  
**SubAgent:** `primary` (for `coder`)  
**Role:** `coder`  
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`  
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-d79` and keep the investigation projection-only. The current evidence says `submit_serial=8` is the transfer-worker submission and `submit_serial=9` is the following frame-1 main submission that waits on that semaphore chain, then later dies at `fence_wait`. Add small, reversible, high-signal instrumentation that helps determine what specific hazard lives in that transfer-submit → frame-submit handoff: semaphore provenance/consumption, transfer payload ownership, frame-submit work classification, or other backend-side state that could explain why submit 9 queues cleanly but later fails. Prefer failure-mechanics instrumentation over prettier logging. Update this plan with actual results, run relevant validation, commit/push the updates, and close bead `oc-d79` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow transfer-submit → frame-submit diagnostic slice in Godot only; GDGS stayed unchanged. On the RenderingDevice side, transfer-worker submission now logs the submitted command buffer identity plus transfer payload ownership state (`staging_in_use`, `ops_processed`, `ops_submitted`, `ops_recorded`, `ops_used_by_draw`) and stores per-frame wait provenance strings when transfer semaphores are pushed into `frames[frame].semaphores_to_wait_on`. The matching frame execution logs now print that same `wait_debug` payload when frame execution begins and when the main queue submits the next command buffer, so QA can see exactly which transfer worker payload/semaphore chain is being consumed by the failing follow-up submit.

On the Vulkan backend side, queue submission and later fence-wait diagnostics now carry `wait_provenance=` and `signal_provenance=` alongside the existing wait/signal/command summaries. A small backend semaphore-state table records, per Vulkan semaphore handle, the last signaling submit serial / queue / source / command summary and the last waiting submit serial / queue / command summary. That gives the next projection-only repro a direct causal readout for whether failing `submit_serial=9` is waiting on a semaphore last signaled by `submit_serial=8`, whether that semaphore had already been consumed elsewhere, and what command summary last owned each side of the handoff.

Validation completed with `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp servers/rendering/rendering_device.h servers/rendering/rendering_device.cpp` and an incremental rebuild via `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64` (after one compile-only correction from `VectorView::is_empty()` to `size() > 0`). A final `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "wait_provenance="` check confirmed the rebuilt editor now contains the new provenance log strings.

---

## Final Results

**Status:** ⚠️ Partial

**What We Built:** The instrumentation lane is now fully documented and independently audited. The package established the right staged repro controls on the GDGS side, corrected the seam description from the earlier local/global theory to the current global/global reality, and narrowed the first failing staged workload boundary to `projection_only`. The audit also captured the main remaining limitation: the new Godot-source callback breadcrumbs were prepared in the instrumentation branch, but the staged QA evidence package came from the managed runtime rather than a source-built engine binary, so the current evidence narrows the first failing GDGS stage more strongly than it separates plugin misuse from engine/backend failure.

**Reference Check:** `REF-06` documents the intended staged order and instrumentation seams, `REF-07` contains the executed QA evidence package, and `REF-08` records the independent auditor verdict and caveats. The projection-first conclusion is accepted only in that narrower audited sense.

**Commits:**
- `2e9a557` - `docs: map GDGS compositor instrumentation seams`
- `c111442` - `debug: add compositor callback breadcrumbs`
- `3b2ce44` - `debug: add GDGS compositor instrumentation gates`
- `e3be761` - `debug: add projection scratch dispatch diagnostics`
- `d54d58ae` - `docs: record compositor projection diagnostics lane`

**Lessons Learned:**
- After removing one real plugin misuse, the next best step is diagnostic isolation, not another guess.
- The surviving seam changed meaning after source inspection: the active repro is global/global, not local/global.
- Stage gating can isolate the first failing plugin workload boundary even when the engine-side instrumentation runtime is not yet the one under test, but that distinction has to be stated explicitly.

## Fresh Session Start / Next Steps

Start the next session from this plan plus `REF-07` and `REF-08`, then execute in this order:

1. **Add or expose the missing trivial scratch-dispatch stage**
   - prove whether any nontrivial compositor-path compute dispatch is sufficient to poison the device, or whether projection-specific work is required
2. **Run the same staged experiment on a source-built Godot binary from the instrumentation branch**
   - exercise the new Godot callback breadcrumbs in the actual runtime under test
   - verify whether the engine-owned callback seam and later `BLIT_PASS` path add any new separating evidence
3. **If projection remains the first failing boundary, inspect projection-specific suspects first**
   - output buffer / image write bounds
   - projection pipeline layout / push-constant contract at the observed 128-byte dispatch
   - post-projection synchronization or resource lifetime fallout
4. **Keep later stages demoted unless new evidence forces them back up**
   - radix, boundaries, render, and compositor writeback/presentation are no longer the leading first-trigger suspects

Avoid reopening already-closed branches unless the new stage-gating evidence directly points back to them:
- the radix push-constant contract bug is fixed and validated as removed
- the earlier indirect-dispatch hypothesis no longer leads the suspect list
- final compositor writeback/presentation is demoted because projection alone already reproduces the crash in the current staged package

---

*Completed on 2026-05-17 (partial; audit complete, deeper runtime separation still pending)*
