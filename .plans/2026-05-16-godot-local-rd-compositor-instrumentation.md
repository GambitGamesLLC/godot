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
