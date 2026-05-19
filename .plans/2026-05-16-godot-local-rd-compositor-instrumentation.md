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

### Task 26: QA verify `submit_serial=9` wait provenance from `submit_serial=8`

**Bead ID:** `oc-y6n`  
**SubAgent:** `primary` (for `qa`)  
**Role:** `qa`  
**References:** `REF-04`, `REF-05`, `REF-06`, `REF-07`, `REF-08`  
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-y6n` and keep the investigation projection-only. Run the refreshed source-built host-Vulkan repro (`projection_only + disabled`) and use the new transfer→frame provenance diagnostics to determine whether `submit_serial=9` is waiting on the exact semaphore last signaled by `submit_serial=8`, whether that semaphore already had a prior consumer, and whether the attached wait/signal provenance or command summary indicates stale/reused ownership rather than a pure projection workload failure. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-y6n` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA claimed bead `oc-y6n`, confirmed the refreshed source-built editor at `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` now contains the new provenance strings from `dc6b26d1`, and ran one minimum host-Vulkan repro (`projection_only + disabled`) into `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-provenance-sourcebuild-20260517-214532/`.

That run answered the semaphore question directly. `submit_serial=8` is the frame-1 transfer-worker handoff with `signal_summary=[{source=submit,index=0,vk=107352545131872}]`, and `submit_serial=9` is the following frame-1 main command submission with `wait_summary=[{source=external,index=0,vk=107352545131872,stage="ALL_COMMANDS"}]`. The backend wait provenance on submit 9 records `last_signal_submit_serial=8` for that exact handle and `last_wait_submit_serial=0`, so submit 9 is waiting on the exact semaphore last signaled by submit 8 and QA did **not** see a prior recorded consumer or stale/reused semaphore ownership on that chain.

The transfer payload ownership also stays coherent across the handoff: `transfer_submit_begin frame=1 transfer_worker=0 ... command_buffer_id=135588252477144 staging_in_use=63676352 ops_processed=85 ops_submitted=85 ops_recorded=98 ops_used_by_draw=98` matches `frame_execute_begin frame=1 ... wait_debug=[{source=transfer_worker,... semaphore_id=107352545131872,command_buffer_id=135588252477144,command_fence_id=107352559285712,staging_in_use=63676352,ops_processed=85,ops_submitted=85,ops_recorded=98,ops_used_by_draw=98}]`. On the command side, submit 8 carries a narrow unlabeled transfer command buffer, while submit 9 carries the larger frame-1 main command graph (`labels=102`, `first_label="Command Graph (L-1)"`, `last_label="Command Graph (L88) (Draw)"`, truncated `label_path`, `last_breadcrumb="UI_PASS"`).

The failure signature is unchanged: the first explicit error still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. QA saved the durable evidence package in `doc/gdgs-compositor-staged-qa-2026-05-17.md` and closed bead `oc-y6n` because the requested semaphore-provenance package is now complete.

---

### Task 27: Split the frame-1 main command graph behind failing `submit_serial=9`

**Bead ID:** `oc-1ux`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-1ux` and keep the investigation projection-only on the source-built Godot binary. Add small, reversible, high-signal instrumentation that splits or maps the large frame-1 main command graph behind failing `submit_serial=9` into more actionable backend segments. The goal is to identify what portion of that non-present frame-1 work is actually represented by the failing command buffer and what segment most plausibly leads to the later `fence_wait_error` / `BLIT_PASS` collapse. Prefer command-graph / draw-list / backend ownership evidence over more shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-1ux` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow Vulkan command-buffer classification layer on top of the existing submit-8 → submit-9 provenance logs, without changing renderer behavior. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now track per-command-buffer `label_tail` (the last labels rather than only the truncated front path) plus a compact `label_segments` summary that groups contiguous debug labels by their recorded command-graph operation tag (`Copy`, `Compute`, `Draw`, `Custom`, or mixed tags already emitted by `RenderingDeviceGraph`) and reports label-count / label-index / level ranges for each segment. The summary is reset at command-buffer begin, recorded at each `command_begin_label(...)`, and appended to the existing `command_summary=` payload that already rides through `queue_submit`, `fence_wait_begin`, and `fence_wait_error`. This keeps the staged repro model intact while making `submit_serial=9` actionable for the next QA pass: instead of only seeing `labels=102 ... last_breadcrumb=UI_PASS`, QA can now tell whether the failing frame-1 main command buffer is mostly copy-vs-draw work, where the major contiguous segment boundaries land, and what tail labels were active nearest the eventual fence failure / later `BLIT_PASS` collapse. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "label_segments="` to confirm the refreshed source-built binary contains the new segment-summary log string.

---

### Task 28: QA classify the failing `submit_serial=9` command graph segments

**Bead ID:** `oc-uvd`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-uvd` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `submit_serial=9` backend summaries. Determine whether `label_segments` shows a dominant Copy / Compute / Draw / Custom slice or a clearer handoff boundary, and whether `label_tail` places the likely hazard near late draw/UI work versus earlier setup/copy work. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-uvd` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan repro on the refreshed source-built editor and recorded the durable evidence in `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Exact runtime path used: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`, launched on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`) against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`, case `projection_only__disabled`. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-segments-sourcebuild-20260518-073823/`. The new `submit_serial=9` backend summary answers the classification question clearly: `label_segments` is dominated by Copy work rather than Draw/Compute (`Copy` blocks of `21` labels and then `73` labels, versus only a tiny late tail of `Draw(1)`, `Compute(1)`, `Copy+Compute(1)`, and final `Draw(2)`). `label_tail` places the nearest end-of-buffer hazard context late in the frame — `Render 3D Transparent Pass (L86) (Copy)`, `Render 3D Transparent Pass (L86) (Draw)`, `Command Graph (L86) (Compute)`, `Command Graph (L87) (Copy+Compute)`, `Tonemap (L87) (Draw)`, `Command Graph (L88) (Draw)` — so the best read is a long copy-heavy body handing off into a short late transparent/tonemap/final-draw epilogue. The broader provenance/failure signature stayed unchanged in the same run: `submit_serial=8` remains the transfer-worker handoff, `submit_serial=9` waits on that exact semaphore (`last_signal_submit_serial=8`, `last_wait_submit_serial=0`), the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. Next recommendation from this QA pass: split the backend investigation at the large copy-body -> late `L86`/`L87`/`L88` tail boundary instead of treating `submit_serial=9` as a generic draw/UI failure.

---

### Task 29: Instrument the copy-body -> late-tail seam inside failing `submit_serial=9`

**Bead ID:** `oc-fdg`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-fdg` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that splits the large Copy-dominated body of failing `submit_serial=9` from its short late `L86`/`L87`/`L88` transparent/compute/tonemap/draw tail. The goal is to identify what backend seam or subrange inside that command buffer is the most plausible hazard before the later `fence_wait_error` / `BLIT_PASS` collapse. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-fdg` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small Vulkan-side seam summary on top of the existing `label_segments` / `label_tail` diagnostics, without changing renderer behavior or broadening back into shader probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now retain per-command-buffer per-level label statistics and append a new `late_tail_split=` summary into `command_summary=` at submit / fence-wait time. The summary keys off the highest observed command-graph level in the command buffer, treats the last three levels (`max_level-2 .. max_level`) as the late tail, and reports: aggregate pre-tail label/op counts, aggregate tail label/op counts, and per-level details for the tail including first/last labels and Copy / Compute / Draw / Custom / Mixed / Unclassified counts. For the failing `submit_serial=9` lane this should turn the prior qualitative answer (“big Copy body, short `L86`/`L87`/`L88` tail”) into a backend-owned quantitative seam so QA can ask whether the plausible hazard sits in the pre-tail copy body or only in the final late-level epilogue. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "late_tail_split="` to confirm the new log string landed in the runnable source-built binary.

---

### Task 30: QA classify the pre-tail body versus late tail on failing `submit_serial=9`

**Bead ID:** `oc-bge`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-bge` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `late_tail_split=` backend summary on failing `submit_serial=9`. Determine whether the tighter backend-owned hazard seam is the dominant pre-tail copy body or the short late `L86` / `L87` / `L88` tail, compare the aggregate body-versus-tail counts with the earlier `label_segments` / `label_tail` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-bge` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan repro on the refreshed source-built editor and recorded the durable evidence in `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Exact runtime path used: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`, launched on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`) against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`, case `projection_only__disabled`. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-late-tail-sourcebuild-20260518-083603/`. The new `late_tail_split=` summary on failing `submit_serial=9` materially tightens the same classification established by bead `oc-uvd`: the plausible backend-owned seam is still the dominant pre-tail Copy body rather than the short late `L86` / `L87` / `L88` tail. Quantitatively, the command buffer splits into `pre_tail_labels=94` with `pre_tail_ops={copy=91,compute=0,draw=2,custom=0,mixed=0,unclassified=1}` versus only `tail_labels=8` with `tail_ops={copy=3,compute=1,draw=3,custom=0,mixed=1,unclassified=0}`. The per-level late tail stays tiny and matches the earlier `label_tail` evidence exactly: level `86` carries `5` labels (`copy=3, compute=1, draw=1`), level `87` carries `2` labels (`mixed=1, draw=1`), and level `88` carries the final single draw label. Compared against the earlier `label_segments` answer from `oc-uvd` (`Copy` blocks of `21` and `73` labels, then only `1+1+1+2` labels across the late draw/compute/mixed/draw epilogue), this new backend-owned split confirms that the frame-1 main command buffer is overwhelmingly copy-dominated before the final transparent / tonemap / draw tail. The broader provenance/failure signature is unchanged in the same run: `submit_serial=8` remains the transfer-worker handoff, `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`, the stable `render_for_compositor_sync_snapshot` still precedes the failing submit, the first explicit failure still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. Recommended next step from this QA pass: split or classify the large pre-tail Copy body itself before spending more time on the already-quantified late `L86` / `L87` / `L88` epilogue.

---

### Task 31: Split the dominant pre-tail Copy body inside failing `submit_serial=9`

**Bead ID:** `oc-ynq`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-ynq` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that breaks apart or classifies the dominant pre-tail Copy body inside failing `submit_serial=9`. The goal is to identify whether that large Copy-dominated body contains a narrower backend-owned seam or subrange that better explains the later `fence_wait_error` / `BLIT_PASS` collapse. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-ynq` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added one more compact Vulkan backend summary on top of `late_tail_split=` without changing renderer behavior or reopening shader-side probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now append `pre_tail_copy_body_split=` into `command_summary=` at submit / fence-wait time. The new summary keeps the existing late-tail definition (`max_level-2 .. max_level`) but breaks only the pre-tail body into fixed 8-level bands, reporting per-band label/op counts plus first/last labels and highlighting the dominant pre-tail bucket. This keeps the staged repro model intact while giving QA a narrower backend-owned question than the old `pre_tail_labels=94` lump: whether the copy-heavy body concentrates into a smaller level subrange, and if so which first/last labels frame that bucket. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "pre_tail_copy_body_split="` to confirm the new log string landed in the runnable source-built binary. This pass stays small, reversible, source-built, and projection-only.

---

### Task 32: QA classify the dominant pre-tail Copy-body buckets on failing `submit_serial=9`

**Bead ID:** `oc-699`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-699` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `pre_tail_copy_body_split=` backend summary on failing `submit_serial=9`. Determine which 8-level pre-tail bucket dominates the Copy-heavy body, record that bucket’s label/op counts and first/last labels, compare the result against the earlier `late_tail_split=` and `label_segments` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-699` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650/`). The fresh `submit_serial=9` command summary preserved the settled seam (`submit_serial=8` transfer handoff -> `submit_serial=9` frame-1 main submit -> later `fence_wait_error submit_serial=9` -> eventual `BLIT_PASS` collapse) and added the requested `pre_tail_copy_body_split=` classification. The dominant 8-level pre-tail bucket is `levels=8..15` with `labels=14` and `ops={copy=13,compute=0,draw=1,custom=0,mixed=0,unclassified=0}`; its first/last labels are `Command Graph (L8) (Copy)` and `Render Depth Pre-Pass (L15) (Draw)`. That result sharpens the earlier evidence instead of contradicting it: `late_tail_split=` still shows the late `L86..L88` tail is only `8` labels wide (`copy=3, compute=1, draw=3, mixed=1`), while the older `label_segments=` summary still shows the overall frame-1 submission is dominated by Copy work (`21` early Copy + `73` middle Copy labels before the tiny late tail). The new bucket breakdown therefore says the first densest 8-level hotspot inside that already-dominant pre-tail body is the `L8..L15` copy band, not the demoted late transparent/tonemap epilogue. Durable evidence was recorded in `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`, and bead `oc-699` can close on this classification result.

---

### Task 33: Split the `L8..L15` Copy hotspot and the handoff into `Render Depth Pre-Pass (L15)`

**Bead ID:** `oc-vmd`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-vmd` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that splits the newly identified `L8..L15` Copy-dominated hotspot inside failing `submit_serial=9`, with particular attention to the handoff into `Render Depth Pre-Pass (L15) (Draw)`. The goal is to identify whether the tighter backend-owned seam lives inside the `L8..L14` copy chain itself or at the transition into the first `L15` draw consumer. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-vmd` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added one more compact Vulkan backend seam summary on top of `pre_tail_copy_body_split=` without changing renderer behavior or reopening shader-side probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now append `pre_tail_copy_handoff=` into `command_summary=` at submit / fence-wait time. The new summary reuses the existing dominant 8-level pre-tail bucket selection, then splits that bucket at the first level containing a draw label. For the current `submit_serial=9` lane this is designed to answer a tighter backend-owned question than the old bucket summary alone: whether the densest hotspot remains inside the copy-only prefix (`L8..L14` in the currently known shape) or at the first draw-consuming handoff into `Render Depth Pre-Pass (L15) (Draw)`. The payload reports the dominant bucket range, first draw level, last copy label, first draw label, aggregate copy-chain vs consumer counts, and per-level details inside that bucket so the next QA pass can classify the seam directly from the source-built binary logs. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object-target check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "pre_tail_copy_handoff="` to confirm the new log string landed in the runnable source-built binary. Commit/push details will be appended after landing the branch update.

---

### Task 34: QA classify the `L8..L15` copy-chain versus `L15` draw-consumer handoff on failing `submit_serial=9`

**Bead ID:** `oc-tz6`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-tz6` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `pre_tail_copy_handoff=` backend summary on failing `submit_serial=9`. Determine whether the tighter backend-owned seam resolves to the `L8..L14` copy-chain side or to the first `L15` draw consumer handoff (`Render Depth Pre-Pass (L15) (Draw)`), compare the result against the earlier `pre_tail_copy_body_split=` / `late_tail_split=` / `label_segments` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-tz6` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539/`). The new `pre_tail_copy_handoff=` summary on failing `submit_serial=9` tightens the earlier `L8..L15` hotspot into an even split between a pure copy-chain prefix and the first draw-containing consumer level: `copy_chain={levels=8..14,labels=7,ops={copy=7,compute=0,draw=0,...}}` versus `consumer={levels=15..15,labels=7,ops={copy=6,compute=0,draw=1,...}}`, with `first_draw_level=15`. Compared against the earlier evidence, this means the broad copy-dominated story still holds (`label_segments` still shows the overall frame-1 command buffer dominated by Copy work, and `late_tail_split=` / `pre_tail_copy_body_split=` still place the dominant backend seam before the late `L86..L88` tail), but the *tightest* backend-owned seam inside that dominant bucket now resolves to the first `L15` consumer handoff rather than the pure `L8..L14` copy-only prefix alone. The run preserved the existing baselines in the same artifact: `submit_serial=8` remains the transfer-worker handoff, `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`, `render_for_compositor_sync_snapshot` stays stable before the frame-1 submit chain, the first explicit failure still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. Recommended next step from this QA pass: split or classify level `15` itself — especially the handoff from the `Command Graph (L15) (Copy)` labels into `Render Depth Pre-Pass (L15) (Draw)` — before spending more effort on the already-demoted late tail or on the pure `L8..L14` copy prefix.

---

### Task 35: Split the `L15` copy-to-depth-prepass seam inside failing `submit_serial=9`

**Bead ID:** `oc-89z`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-89z` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that splits level `15` inside failing `submit_serial=9`, with particular attention to the handoff from `Command Graph (L15) (Copy)` into `Render Depth Pre-Pass (L15) (Draw)`. The goal is to identify whether the tighter backend-owned seam lives in the `L15` copy/setup work itself or exactly at the first depth-prepass draw consumer boundary. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-89z` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small, reversible Vulkan-side refinement on top of `pre_tail_copy_handoff=` so the source-built runtime can now split the first draw-containing consumer level itself instead of stopping at per-level aggregate counts. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now extend `DebugLevelStats` with first-draw and last-copy-before-first-draw bookkeeping plus per-level before/after-first-draw op counts, and `command_summary=` appends a new `level_draw_handoff=` payload. For the current failing lane this is designed to resolve level `15` into two backend-owned slices without reopening shader probes: `copy_setup_prefix={... last_copy_label="Command Graph (L15) (Copy)"}` versus `draw_consumer_boundary={has_draw=true, first_draw_label="Render Depth Pre-Pass (L15) (Draw)", from_first_draw={...}}`. That keeps the staged repro model intact while giving QA a direct way to answer whether the remaining seam inside `submit_serial=9` lives in the `L15` copy/setup prefix itself or exactly at the first depth-prepass draw consumer boundary. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "level_draw_handoff="` to confirm the new summary string landed in the runnable source-built binary. Commit/push details appended after landing the branch update.

---

### Task 36: QA classify the `L15` copy-setup prefix versus first draw-consumer boundary

**Bead ID:** `oc-is3`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-is3` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `level_draw_handoff=` backend summary on failing `submit_serial=9`. Determine whether the tighter backend-owned seam inside level `15` resolves to the copy-setup prefix itself or to the first draw-consumer boundary (`Render Depth Pre-Pass (L15) (Draw)`), compare the result against the earlier `pre_tail_copy_handoff=` / `pre_tail_copy_body_split=` / `late_tail_split=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-is3` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554/`). The new `level_draw_handoff=` summary on failing `submit_serial=9` resolves level `15` directly into `copy_setup_prefix={labels=6, ops={copy=6,...}, last_copy_label="Command Graph (L15) (Copy)"}` versus `draw_consumer_boundary={has_draw=true, first_draw_label="Render Depth Pre-Pass (L15) (Draw)", from_first_draw={labels=1, ops={draw=1,...}}}`. Compared against the earlier `pre_tail_copy_handoff=` / `pre_tail_copy_body_split=` / `late_tail_split=` stack, that means the broad copy-dominated story still holds, but the *tightest* backend-owned seam now resolves to the first `L15` draw-consumer boundary rather than to the `L15` copy/setup prefix itself. The same run preserved the established baselines: `submit_serial=8` remains the transfer-worker handoff, `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`, `render_for_compositor_sync_snapshot` stays stable before the frame-1 submit chain, the first explicit failure still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. Durable notes and artifact references were appended to `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`).

---

### Task 37: Split the `Render Depth Pre-Pass (L15) (Draw)` consumer seam and its immediate dependency chain

**Bead ID:** `oc-an8`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-an8` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that splits or classifies the backend-owned work attached to `Render Depth Pre-Pass (L15) (Draw)` inside failing `submit_serial=9`, plus the immediate setup/dependency chain that feeds that first draw consumer boundary. The goal is to identify whether the remaining seam lives exactly on the depth-prepass draw consumer itself or on the immediately inherited setup/dependency work that precedes it. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-an8` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added one more compact Vulkan backend seam summary on top of `level_draw_handoff=` without changing renderer behavior or reopening shader-side probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now retain a compact per-label trace for each command buffer and append a new `depth_prepass_consumer_seam=` payload into `command_summary=` at submit / fence-wait time. The new summary targets the first exact `Render Depth Pre-Pass (L15) (Draw)` label and reports: the full contiguous non-draw feeder chain immediately preceding that draw boundary, the same-level `L15` setup prefix, the inherited prior-level dependency chain that still feeds that consumer, and the previous draw label before the feeder chain begins. This is designed to answer a tighter backend-owned question than the old level-only summaries: whether the remaining seam lives exactly on the first depth-prepass draw consumer, or on the contiguous inherited non-draw setup/dependency work that reaches into it from earlier labels/levels. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "depth_prepass_consumer_seam="`; and a quick rebuilt-binary sanity launch `timeout 20s ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version`. This pass stays small, reversible, source-built, and projection-only.

---

### Task 38: QA classify the depth-prepass draw-consumer seam on failing `submit_serial=9`

**Bead ID:** `oc-y2c`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-y2c` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `depth_prepass_consumer_seam=` backend summary on failing `submit_serial=9`. Determine whether the tightest remaining backend-owned seam is exactly the `Render Depth Pre-Pass (L15) (Draw)` consumer label or the contiguous inherited non-draw feeder chain that leads into that boundary, compare the result against the earlier `level_draw_handoff=` / `pre_tail_copy_handoff=` / `pre_tail_copy_body_split=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-y2c` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700/`). The new `depth_prepass_consumer_seam=` summary on failing `submit_serial=9` resolves the remaining backend-owned boundary in favor of the exact first depth-prepass draw consumer rather than the inherited non-draw feeder chain alone. The summary reports `draw_label="Render Depth Pre-Pass (L15) (Draw)"` at `draw_label_index=22`, preceded by `contiguous_non_draw_feeder={label_indexes=0..21,levels=-1..15,labels=22,ops={copy=21,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L15) (Copy)"}`. Inside that feeder, the same-level setup is only `same_level_setup={label_indexes=16..21,labels=6,ops={copy=6,...},first_label="Command Graph (L15) (Copy)",last_label="Command Graph (L15) (Copy)"}`, while the inherited prior-level dependency chain is `inherited_dependency_chain={label_indexes=0..15,levels=-1..14,labels=16,ops={copy=15,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L14) (Copy)"}` with `previous_draw_label=none`. Compared against the earlier `level_draw_handoff=` / `pre_tail_copy_handoff=` / `pre_tail_copy_body_split=` stack, this keeps the broad copy-dominated story intact but classifies the tightest seam as the exact `Render Depth Pre-Pass (L15) (Draw)` consumer boundary: the inherited feeder chain is larger, but it remains entirely non-draw setup feeding into the first observed draw consumer, and no narrower backend-owned handoff inside that feeder beats the consumer boundary itself. The broader baselines remain unchanged in the same artifact set: `submit_serial=8` is still the transfer-worker handoff, `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`, the stable `render_for_compositor_sync_snapshot` still precedes the frame-1 submit chain, the first explicit failure still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`.

---

### Task 39: Split the backend-owned work attached to `Render Depth Pre-Pass (L15) (Draw)`

**Bead ID:** `oc-73t`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-73t` and keep the investigation projection-only on the refreshed source-built Godot binary. Add small, reversible, high-signal instrumentation that splits or classifies the backend-owned work attached directly to `Render Depth Pre-Pass (L15) (Draw)` inside failing `submit_serial=9`. The feeder/setup ancestry is now well understood; the goal is to identify what backend-owned work or dependency attached to that first depth-prepass draw consumer boundary makes it the surviving seam. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-73t` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Kept the investigation source-built and projection-only, then tightened the existing `depth_prepass_consumer_seam=` runtime summary instead of inventing a broader new probe lane. In `drivers/vulkan/rendering_device_driver_vulkan.cpp`, the depth-prepass consumer classifier now also records the immediate contiguous non-draw follow-up chain *after* `Render Depth Pre-Pass (L15) (Draw)` until the next draw, split into aggregate `post_draw_non_draw_attachment`, same-level `same_level_followup`, higher-level `higher_level_followup`, and `next_draw_label`. That makes the next QA pass capable of answering the narrower surviving question left by bead `oc-y2c`: whether the seam is just the first depth-prepass draw consumer boundary itself, or backend-owned non-draw work attached directly downstream of that consumer before the next draw boundary appears. The change stays small and reversible, keeps the staged repro model intact, and prefers command-graph / backend ownership evidence over shader-side instrumentation. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.cpp`; incremental object-target check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "post_draw_non_draw_attachment="`; and rebuilt-binary sanity launch `timeout 20s ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.e4097105e`). Landed in commit `df73b144` on branch `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` and pushed to remote `gambit`.

---

### Task 40: QA classify the immediate downstream attachment after `Render Depth Pre-Pass (L15) (Draw)` on failing `submit_serial=9`

**Bead ID:** `oc-c1s`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-c1s` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the expanded `depth_prepass_consumer_seam=` backend summary on failing `submit_serial=9`. Determine whether the seam stays pinned exactly on `Render Depth Pre-Pass (L15) (Draw)` or whether there is meaningful immediate non-draw backend attachment after that draw (`post_draw_non_draw_attachment`, `same_level_followup`, `higher_level_followup`, `next_draw_label`) that is actually the tighter seam. Compare the result against the earlier `depth_prepass_consumer_seam=` / `level_draw_handoff=` / `pre_tail_copy_handoff=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-c1s` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680/`). The expanded `depth_prepass_consumer_seam=` payload on failing `submit_serial=9` answers the remaining downstream-attachment question cleanly: the seam stays pinned on the exact `Render Depth Pre-Pass (L15) (Draw)` consumer boundary. There is no immediate non-draw attachment after that draw before the next draw boundary appears — `post_draw_non_draw_attachment={label_indexes=none,levels=none,labels=0,...}`, `same_level_followup={label_indexes=none,labels=0,...}`, `higher_level_followup={label_indexes=none,levels=none,labels=0,...}`, and `next_draw_label="Render Opaque Pass (L16) (Draw)"`. Compared against the earlier `depth_prepass_consumer_seam=` / `level_draw_handoff=` / `pre_tail_copy_handoff=` evidence, this demotes the theory that a meaningful immediate non-draw backend attachment downstream of the first depth-prepass draw is the tighter seam. The established baselines also held in the same artifact: `render_for_compositor_sync_snapshot` stayed stable, `submit_serial=8` remained the transfer-worker handoff, `submit_serial=9` still waited on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`, the first explicit failure still surfaced at `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapsed to `BLIT_PASS`. This closes the QA evidence package for bead `oc-c1s`: the surviving backend-owned seam did not shift downstream and remains pinned exactly on the first `Render Depth Pre-Pass (L15) (Draw)` consumer boundary.

---

### Task 41: Inspect the exact depth-prepass draw boundary on failing `submit_serial=9`

**Bead ID:** `oc-agt`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-agt` and keep the investigation projection-only on the refreshed source-built Godot binary. The backend seam is now pinned exactly on `Render Depth Pre-Pass (L15) (Draw)` with no tighter immediate downstream non-draw attachment. Add small, reversible, high-signal instrumentation that inspects the backend-owned work attached to that exact depth-prepass draw consumer boundary itself. The goal is to identify what aspect of the first depth-prepass draw consumer makes it the surviving seam before the later `fence_wait_error submit_serial=9` / `BLIT_PASS` collapse. Prefer command-graph / backend ownership evidence over shader-side probes, keep the staged repro model intact, run relevant validation, update this plan with actual results, commit/push the changes, and close bead `oc-agt` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow per-label backend probe inside `drivers/vulkan/rendering_device_driver_vulkan.*` that snapshots inherited render state at label begin and counts backend commands actually recorded while the exact `Render Depth Pre-Pass (L15) (Draw)` label is active. The updated `depth_prepass_consumer_seam=` payload now emits `draw_boundary_backend_attachment={consumer_class=...,begin_state=...,label_commands=...}` so the seam can be classified by backend ownership instead of ancestry alone. On the refreshed source-built `projection_only__disabled` repro, the first failing `submit_serial=9` still reproduces the later `fence_wait_error ... wait_result=-4` / `BLIT_PASS` collapse, but the exact depth-prepass draw boundary no longer reads like a real draw packet: it currently classifies as a render-pass wrapper with `begin_state={render_pass_active=false,framebuffer_active=false,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,begin_breadcrumb="NONE"}` and `label_commands={render_pass_begin=1,render_pass_end=1,draw_calls=0,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,index_buffer_binds=0,execute_secondary_calls=0}`. That means the surviving seam is not “the first direct depth-prepass draw call” so much as “the first depth-prepass render-pass consumer boundary,” which is a tighter backend classification for the next QA/audit pass. Validation performed: local source build via `scons -j8 platform=linuxbsd target=editor dev_build=yes`, then the staged host-Vulkan source-built repro at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-boundary-wrapper-vulkan-sourcebuild-20260518-150439/`.

---

### Task 42: QA inspect the depth-prepass render-pass wrapper payload on failing `submit_serial=9`

**Bead ID:** `oc-124`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-124` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `draw_boundary_backend_attachment={consumer_class,begin_state,label_commands}` classification on failing `submit_serial=9`. Determine whether the seam truly stays on a tiny `render_pass_wrapper` boundary, and whether the real depth-prepass payload appears as nested / render-pass-owned work beneath that wrapper rather than as direct commands on the label itself. Compare the result against the earlier `depth_prepass_consumer_seam=` / `draw_boundary_backend_attachment=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-124` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035/`). The fresh `submit_serial=9` evidence preserved the settled baselines (`render_for_compositor_sync_snapshot` still stable, `submit_serial=8` still the transfer-worker handoff, `submit_serial=9` still waits on that exact semaphore, first explicit failure still `fence_wait_error submit_serial=9 wait_result=-4`, later breadcrumb collapse still `BLIT_PASS`) and answered the new wrapper-payload question cleanly. The seam truly stays pinned on a tiny backend `render_pass_wrapper`: `draw_boundary_backend_attachment.consumer_class="render_pass_wrapper"`, `begin_state={render_pass_active=false, framebuffer_active=false, subpass=0, render_pipeline_bound=false, vertex_binding_count=0, index_buffer_bound=false, index_format=none, begin_breadcrumb="NONE"}`, and `label_commands={render_pass_begin=1, render_pass_end=1, draw_calls=0, draw_indexed_calls=0, draw_indirect_calls=0, draw_indexed_indirect_calls=0, pipeline_binds=0, uniform_binds=0, vertex_buffer_binds=0, index_buffer_binds=0, execute_secondary_calls=0, secondary_command_buffers=0, secondary_labels=0, secondary_draw_labels=0, first_backend_command="begin_render_pass", last_backend_command="end_render_pass"}`. That means the rerun did **not** reveal any direct draw/bind payload or secondary-command payload on the exact `Render Depth Pre-Pass (L15) (Draw)` label itself. Compared against the earlier `depth_prepass_consumer_seam=` evidence, the wrapper remained the tightest visible seam, and any real depth-prepass payload is still only inferable as render-pass-owned / nested work beneath that wrapper rather than as direct commands recorded on the label.

---

### Task 43: Inspect lower-level ownership beneath the depth-prepass render-pass wrapper on failing `submit_serial=9`

**Bead ID:** `oc-023`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-023` and keep the investigation projection-only on the refreshed source-built Godot binary. The backend seam is now pinned on a tiny `render_pass_wrapper` at `Render Depth Pre-Pass (L15) (Draw)` with no direct draw/bind/secondary payload visible on the label itself. Add small, reversible, high-signal instrumentation that inspects lower-level ownership beneath that wrapper so we can see where the real depth-prepass payload is emitted relative to this label. Prefer render-pass ownership / nested command recording evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-023` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small reversible descendant/pass-owned attribution layer in `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` so the exact `Render Depth Pre-Pass (L15) (Draw)` seam can now report lower-level ownership beneath its current `render_pass_wrapper` classification without reopening shader-side probes. The Vulkan debug label tracker now (1) records descendant label metadata for active ancestor labels, including descendant draw-label counts and whether descendants begin while a render pass is already active; (2) records descendant backend-command ownership for ancestor labels while nested labels are active; and (3) propagates executed secondary-command-buffer counts/labels into ancestor-descendant fields. `depth_prepass_consumer_seam=` now extends `draw_boundary_backend_attachment=` with a new `nested_scope={...}` payload that summarizes descendant labels, inside-render-pass descendants, descendant backend-command counts, and descendant secondary-command provenance beneath the exact wrapper label.

Validation completed with the usual source-built projection-only lane intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "nested_scope={descendant_labels="`; rebuilt-binary sanity launch `timeout 20s ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version`; and a fresh host Wayland/Vulkan staged repro saved at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-nested-vulkan-sourcebuild-20260518-164742319304969/`.

Actual runtime result on the failing `submit_serial=9` seam: the wrapper remained a tiny `render_pass_wrapper`, and the new descendant summary also came back empty — `nested_scope={descendant_labels=0, descendant_draw_labels=0, descendant_begin_inside_render_pass_labels=0, descendant_begin_inside_render_pass_draw_labels=0, ... descendant_commands={... all zero ...}}`. So the real depth-prepass payload still does not surface as direct commands, nested labels, or secondary-command descendants on this exact label-local wrapper. The next QA slice should widen one rung to render-pass ownership outside this exact label or broader pass-level ownership that survives into the later `fence_wait_error submit_serial=9 wait_result=-4` / `BLIT_PASS` envelope. Landed on branch `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` and ready for QA/audit follow-up.

---

### Task 44: Inspect broader pass-level ownership around the depth-prepass wrapper on failing `submit_serial=9`

**Bead ID:** `oc-iml`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-iml` and keep the investigation projection-only on the refreshed source-built Godot binary. The exact `Render Depth Pre-Pass (L15) (Draw)` wrapper is now exhausted as a label-local seam: it shows no direct payload, nested labeled payload, or secondary-command payload. Add small, reversible, high-signal instrumentation that widens one rung to broader render-pass / pass-level ownership around that wrapper so we can see what pass/container work survives around it before the later `fence_wait_error submit_serial=9 wait_result=-4` / `BLIT_PASS` collapse. Prefer render-pass ownership / command-buffer/pass-level attribution over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-iml` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added command-buffer render-pass scope tracking in `drivers/vulkan/rendering_device_driver_vulkan.{h,cpp}` and surfaced it in a new `depth_prepass_pass_scope=` summary beside the existing `depth_prepass_consumer_seam=` payload. The new scope tracker records per-render-pass owner labels, labels-started-inside-scope counts, pass-local backend command counts, and immediate neighboring pass scopes so the exhausted `Render Depth Pre-Pass (L15) (Draw)` wrapper can be viewed one rung wider at pass/container scope. Rebuilt the source editor with `scons platform=linuxbsd target=editor dev_build=yes -j8` and reran the projection-only staged repro on the host Wayland/Vulkan path using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`; the refreshed log at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-vulkan-sourcebuild-20260518-17052753266/stdout.log` still aborts with `fence_wait_error submit_serial=9 wait_result=-4` then `BLIT_PASS`, but now shows `depth_prepass_pass_scope={scope_count=5,target={index=0,owner_begin="Render Depth Pre-Pass (L15) (Draw)",owner_end="Render Depth Pre-Pass (L15) (Draw)",labels_started=0,draw_labels_started=0,...},previous=none,next={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",...}}`. In other words: the exact L15 wrapper is still a bare begin/end render-pass scope with no labels or direct payload started inside it, and the next surviving pass/container boundary after it is already `Render Opaque Pass (L16) (Draw)`. This keeps the staged repro intact while widening the attribution seam from label-local ownership to pass-scope ownership.

---

### Task 45: QA inspect whether the first meaningful pass after the empty L15 wrapper is the L16 opaque scope on failing `submit_serial=9`

**Bead ID:** `oc-8ie`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-8ie` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `depth_prepass_pass_scope=` payload on failing `submit_serial=9`. Determine whether the first meaningful surviving pass/container after the empty `Render Depth Pre-Pass (L15) (Draw)` wrapper is consistently the neighboring `Render Opaque Pass (L16) (Draw)` scope, or whether another non-label-owned pass-level seam appears between those scopes. Compare the result against the earlier `depth_prepass_pass_scope=` / `depth_prepass_consumer_seam=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-8ie` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-18 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` with `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050 no_present compositor projection_only disabled 120` (artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050/`). The refreshed `depth_prepass_pass_scope=` payload on failing `submit_serial=9` reported `scope_count=5`, `target={index=0,owner_begin="Render Depth Pre-Pass (L15) (Draw)",owner_end="Render Depth Pre-Pass (L15) (Draw)",labels_started=0,draw_labels_started=0,...}`, `previous=none`, and `next={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",owner_end="Render Opaque Pass (L16) (Draw)",labels_started=0,draw_labels_started=0,...}`. That confirms the first meaningful surviving pass/container after the empty L15 wrapper is still the neighboring `Render Opaque Pass (L16) (Draw)` scope; QA did **not** observe any intervening non-label-owned pass-level seam between those scopes. This stays consistent with the earlier `depth_prepass_consumer_seam=` evidence that the L15 boundary is an empty `render_pass_wrapper` and with the coder-side validation that originally surfaced the same L16 adjacency. The outer repro envelope was unchanged in the same artifact: stable `render_for_compositor_sync_snapshot`, coherent `submit_serial=8` → `submit_serial=9` semaphore provenance, explicit `fence_wait_error submit_serial=9 wait_result=-4`, and later `BLIT_PASS`. Durable notes were appended to `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`, and the evidence package is complete enough to close bead `oc-8ie`.

---

### Task 46: Inspect ownership and workload inside the L16 opaque pass after the empty L15 wrapper on failing `submit_serial=9`

**Bead ID:** `oc-20r`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-20r` and keep the investigation projection-only on the refreshed source-built Godot binary. The empty `Render Depth Pre-Pass (L15) (Draw)` wrapper has now been demoted, and `Render Opaque Pass (L16) (Draw)` is the first meaningful surviving neighboring pass/container. Add small, reversible, high-signal instrumentation that inspects ownership and workload inside or around that L16 opaque pass so we can see whether the first meaningful surviving seam now actually lives there. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-20r` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow `opaque_pass_scope=` command-buffer summary in `drivers/vulkan/rendering_device_driver_vulkan.{h,cpp}` that centers the failing `submit_serial=9` inspection on `Render Opaque Pass (L16) (Draw)`. The new payload reuses render-pass ownership breadcrumbs already captured by the Vulkan backend, classifies the L16 scope (`render_pass_wrapper` vs payload-bearing), reports its immediate previous/next pass scopes, tracks the contiguous wrapper-only chain starting at L16, and reports the first meaningful pass scope at or after L16 if one exists. Validation stayed on the refreshed source-built editor (`scons platform=linuxbsd target=editor dev_build=yes -j8`, then `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`) and reran the same host-Wayland Vulkan repro with `projection_only__disabled` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`; the durable artifact root is `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-vulkan-sourcebuild-20260518-182242/`. On the failing `submit_serial=9` command summary, the new evidence showed `opaque_pass_scope={scope_count=5,target={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",owner_end="Render Opaque Pass (L16) (Draw)",...},target_class="render_pass_wrapper",previous={index=0,owner_begin="Render Depth Pre-Pass (L15) (Draw)",...},next={index=2,owner_begin="Render 3D Transparent Pass (L86) (Draw)",...},wrapper_chain_from_target={start_index=1,end_index=2,scope_count=2,owner_begin_chain=["Render Opaque Pass (L16) (Draw)", "Render 3D Transparent Pass (L86) (Draw)"]},first_meaningful_at_or_after_target={distance_scopes=2,class="draw_payload",scope={index=3,owner_begin="Tonemap (L87) (Draw)",commands={render_pass_begin=1,render_pass_end=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,...}}}}`. So the answer for bead `oc-20r` is that L16 does **not** currently own the first meaningful surviving payload seam: the opaque pass is still an empty render-pass wrapper, the wrapper-only chain continues through the `Render 3D Transparent Pass (L86) (Draw)` scope, and the first pass-scope workload that survives this slice is the `Tonemap (L87) (Draw)` scope two pass scopes later. The outer failure signature stayed unchanged in the same run (`fence_wait_error submit_serial=9 wait_result=-4` followed by later `BLIT_PASS` lost-device breadcrumbs). The implementation and documentation handoff for this bead landed in commit `b6cadca4` (`debug: inspect opaque pass scope ownership`), and bead `oc-20r` can close as complete on that basis.

---

### Task 47: QA confirm `Tonemap (L87) (Draw)` is the first meaningful surviving pass-scope workload on failing `submit_serial=9`

**Bead ID:** `oc-7io`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-7io` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `opaque_pass_scope=` payload on failing `submit_serial=9`. Confirm whether `Render Opaque Pass (L16) (Draw)` remains wrapper-only, whether the wrapper chain continues through `Render 3D Transparent Pass (L86) (Draw)`, and whether `Tonemap (L87) (Draw)` is consistently the first meaningful surviving pass-scope workload before the later `fence_wait_error submit_serial=9 wait_result=-4` / `BLIT_PASS` collapse. Compare the result against the earlier `opaque_pass_scope=` / `depth_prepass_pass_scope=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-7io` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan repro on the refreshed source-built editor `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still using the staged `projection_only + disabled` harness on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-qa-vulkan-sourcebuild-20260518-190101561855320/`. The fresh QA run reproduced the coder-observed `opaque_pass_scope=` package exactly on the failing `submit_serial=9` command summary: `Render Opaque Pass (L16) (Draw)` still classifies as `target_class="render_pass_wrapper"`, the wrapper-only chain still continues through `Render 3D Transparent Pass (L86) (Draw)` via `wrapper_chain_from_target={start_index=1,end_index=2,scope_count=2,...}`, and `Tonemap (L87) (Draw)` still lands as `first_meaningful_at_or_after_target={distance_scopes=2,class="draw_payload",...}` with the first surviving pass-scope payload (`pipeline_binds=1`, `uniform_binds=1`, `draw_calls=1`). This agrees with the earlier `depth_prepass_pass_scope=` evidence instead of moving the seam: L15 still hands forward into an empty L16 wrapper, L16 still hands forward into an empty L86 wrapper, and the first meaningful surviving pass-scope workload remains Tonemap before the unchanged `fence_wait_error submit_serial=9 wait_result=-4` / later `BLIT_PASS` collapse. This closes bead `oc-7io` because the requested QA confirmation package is complete and internally consistent with the prior source-built pass-scope evidence.

---

### Task 48: Inspect ownership and workload inside `Tonemap (L87) (Draw)` as the first meaningful surviving pass-scope workload on failing `submit_serial=9`

**Bead ID:** `oc-8tk`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-8tk` and keep the investigation projection-only on the refreshed source-built Godot binary. The current pass-scope evidence demotes the empty `Render Depth Pre-Pass (L15) (Draw)` wrapper, the empty `Render Opaque Pass (L16) (Draw)` wrapper, and the empty `Render 3D Transparent Pass (L86) (Draw)` wrapper; `Tonemap (L87) (Draw)` is now the first meaningful surviving pass-scope workload. Add small, reversible, high-signal instrumentation that inspects ownership and workload inside or around that Tonemap pass so we can see whether the first meaningful surviving seam now actually lives there. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-8tk` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow `tonemap_pass_scope=` command-buffer summary in `drivers/vulkan/rendering_device_driver_vulkan.{h,cpp}` so the first meaningful surviving pass-scope workload can now be inspected directly at `Tonemap (L87) (Draw)` instead of only inferred from the earlier `opaque_pass_scope=` chain. The new payload reuses existing render-pass scope breadcrumbs and reports: the full target Tonemap scope summary, whether Tonemap itself is the first meaningful surviving pass scope (`first_meaningful_scope_is_target`), the full contiguous wrapper-only chain feeding into Tonemap (`previous_wrapper_chain_into_target`), whether any earlier meaningful pass survives before Tonemap (`previous_meaningful_before_target`), a compact direct workload signature for the Tonemap scope (`draw/pipeline/uniform/vertex/index/secondary` ownership counts), and the next meaningful surviving pass after Tonemap for contrast.

Kept the investigation source-built and projection-only, then rebuilt and exercised the refreshed editor on the staged host-Wayland Vulkan repro. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; incremental object-target check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "tonemap_pass_scope="`; sanity launch `timeout 20s ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version`; and one staged repro run using `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` with case `projection_only__disabled` on the host GPU path, artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-vulkan-sourcebuild-20260518-192431/`.

Actual runtime result on the failing `submit_serial=9` seam: Tonemap now classifies as a real local `draw_payload`, and the new summary says the first meaningful surviving pass-scope seam does in fact live there. The fresh `tonemap_pass_scope=` payload reports `target_class="draw_payload"`, `first_meaningful_scope_is_target=true`, `previous_meaningful_before_target=none`, and `previous_wrapper_chain_into_target={start_index=0,end_index=2,scope_count=3,owner_begin_chain=["Render Depth Pre-Pass (L15) (Draw)", "Render Opaque Pass (L16) (Draw)", "Render 3D Transparent Pass (L86) (Draw)"]}`. Tonemap’s direct workload signature is small but real and backend-owned on the target scope itself: `direct_draw_calls=1`, `pipeline_binds=1`, `uniform_binds=1`, zero vertex/index/secondary payload, and `begin_breadcrumb="NONE"`, `end_breadcrumb="NONE"`. The next meaningful surviving pass is already the much heavier `Command Graph (L88) (Draw)` scope one step later (`draw_calls=10`, `draw_indexed_calls=10`, `uniform_binds=11`, `vertex_buffer_binds=10`, `index_buffer_binds=1`, breadcrumbs `UI_PASS`). So this pass narrows the seam again: the empty wrapper chain truly ends at Tonemap, and the first surviving meaningful pass/container workload is not merely “somewhere after L86” but the Tonemap pass scope itself. The broader failure envelope stayed unchanged in the same artifact (`fence_wait_error submit_serial=9 wait_result=-4`, later `BLIT_PASS`).

---

### Task 49: QA confirm `Tonemap (L87) (Draw)` is the first meaningful surviving pass-scope seam on failing `submit_serial=9`

**Bead ID:** `oc-set`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-set` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `tonemap_pass_scope=` payload on failing `submit_serial=9`. Confirm whether `Tonemap (L87) (Draw)` remains `target_class="draw_payload"`, whether `first_meaningful_scope_is_target=true` still holds, whether the wrapper-only chain into Tonemap still covers `Render Depth Pre-Pass (L15) (Draw)` -> `Render Opaque Pass (L16) (Draw)` -> `Render 3D Transparent Pass (L86) (Draw)`, and whether `Command Graph (L88) (Draw)` remains the next heavier meaningful scope. Compare the result against the earlier `tonemap_pass_scope=` / `opaque_pass_scope=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-set` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan staged repro (`projection_only + disabled`) on the refreshed source-built editor and saved the durable evidence package under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-qa-vulkan-sourcebuild-20260518-20002814547/` with the corresponding repo-owned notes appended to `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`.

On the failing `submit_serial=9` command summary, the new `tonemap_pass_scope=` payload reproduced the earlier source-built answer exactly enough to close the seam question: `Tonemap (L87) (Draw)` remained `target_class="draw_payload"`, `first_meaningful_scope_is_target=true` still held, `previous_meaningful_before_target=none` still held, and the wrapper-only chain into Tonemap still covered `Render Depth Pre-Pass (L15) (Draw)` -> `Render Opaque Pass (L16) (Draw)` -> `Render 3D Transparent Pass (L86) (Draw)` via `previous_wrapper_chain_into_target={start_index=0,end_index=2,scope_count=3,...}`.

Tonemap also remained a small but real local workload-bearing pass scope (`direct_draw_calls=1`, `pipeline_binds=1`, `uniform_binds=1`, zero vertex/index/secondary payload, breadcrumbs `NONE`), while `Command Graph (L88) (Draw)` remained the next meaningful surviving scope one step later and stayed materially heavier (`draw_calls=10`, `draw_indexed_calls=10`, `uniform_binds=11`, `vertex_buffer_binds=10`, `index_buffer_binds=1`, breadcrumbs `UI_PASS`). The broader failure envelope stayed unchanged in the same rerun (`fence_wait_error submit_serial=9 wait_result=-4`, later `BLIT_PASS`).

---

### Task 50: Create an HTML visual map of the current GDGS/Godot pipeline investigation

**Bead ID:** `oc-8fk`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-8fk` and create a durable HTML file that visually explains the current GDGS/Godot investigation to a technically literate reader who is unfamiliar with the engine internals. The page should clearly show the simplified frame pipeline, highlight which scopes were demoted as empty wrappers (`Render Depth Pre-Pass (L15) (Draw)`, `Render Opaque Pass (L16) (Draw)`, `Render 3D Transparent Pass (L86) (Draw)`), show that `Tonemap (L87) (Draw)` is the first meaningful surviving pass-scope seam, and note that `Command Graph (L88) (Draw)` is the next heavier scope. Include a plain-English explanation of where GDGS projection sits, where Vulkan submit `submit_serial=9` fits, and what `fence_wait_error submit_serial=9 wait_result=-4` plus later `BLIT_PASS` means at a high level. Put the file somewhere durable in the repo docs, keep it self-contained (HTML/CSS/JS inline if needed), update this plan with actual results, and close bead `oc-8fk` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- durable HTML doc under repo docs
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Created a durable self-contained HTML explainer at `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-godot-investigation-visual-map-2026-05-18.html`. The page visually maps the simplified GDGS → Godot → Vulkan frame path for a technically literate reader who does not already know the engine internals. It shows where the GDGS projection dispatch sits as the first failing staged boundary, where the later backend-visible `submit_serial=9` fence failure fits, which pass scopes were demoted as empty wrappers (`Render Depth Pre-Pass (L15) (Draw)`, `Render Opaque Pass (L16) (Draw)`, `Render 3D Transparent Pass (L86) (Draw)`), why `Tonemap (L87) (Draw)` is now the first meaningful surviving pass-scope seam, and why `Command Graph (L88) (Draw)` is the next heavier meaningful scope. It also includes plain-English explanations of `fence_wait_error submit_serial=9 wait_result=-4` and the later `BLIT_PASS` breadcrumb so the current investigation state can be understood quickly without re-reading the entire QA log.

---

### Task 51: Deepen the `Tonemap (L87) (Draw)` seam and classify whether its local workload is the first real poisoned boundary on failing `submit_serial=9`

**Bead ID:** `oc-n3o`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-n3o` and keep the investigation projection-only on the refreshed source-built Godot binary. The current pass-scope evidence demotes the empty wrapper chain ending at `Render 3D Transparent Pass (L86) (Draw)` and places the first meaningful surviving pass-scope seam at `Tonemap (L87) (Draw)`. Add small, reversible, high-signal instrumentation that deepens the Tonemap seam itself so we can classify whether Tonemap’s local workload is the first real poisoned boundary on failing `submit_serial=9`, versus merely the first visible surviving payload before a later heavier scope like `Command Graph (L88) (Draw)`. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-n3o` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a deeper `tonemap_pass_scope=` payload in `drivers/vulkan/rendering_device_driver_vulkan.cpp` that now splits the surviving `Tonemap (L87) (Draw)` seam into two tighter ownership views without changing the staged repro model: (1) `tonemap_local_attachment=` follows the exact Tonemap label entry inside the command buffer and records its begin-state, direct backend commands, descendant/nested payload, and whether the enclosing pass scope matches the Tonemap label commands exactly; (2) the expanded `next_meaningful_after_target=` now includes `workload_delta_from_target=` plus `next_scope_is_heavier=` so the immediately following `Command Graph (L88) (Draw)` scope can be contrasted against Tonemap on the same failing submit.

Validation stayed on the refreshed source-built editor: `scons platform=linuxbsd target=editor dev_build=yes -j8`, then `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` on the host Wayland/Vulkan path against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still using `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` with case `projection_only__disabled`. Durable artifact root for this pass: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-deepen-sourcebuild-20260518-204905/`.

Actual runtime result on failing `submit_serial=9`: the deeper Tonemap seam currently reads as a real, fully self-owned local boundary rather than a hollow wrapper. The new summary reports `tonemap_local_attachment={label_index=100,entry_index=100,level=87,consumer_class="draw_payload",begin_state={render_pass_active=false,framebuffer_active=false,subpass=0,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,index_format=none,begin_breadcrumb="NONE"},label_commands={render_pass_begin=1,render_pass_end=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,...},nested_scope={descendant_labels=0,...,descendant_commands={render_pass_begin=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,draw_calls=0,...}},scope_alignment={scope_matches_label_commands=true,scope_matches_label_plus_descendants=true,scope_minus_label_plus_descendants={render_pass_begin=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,draw_calls=0,...}}}`. That means the Tonemap pass scope is not merely surfacing inherited or nested work from some hidden child label: on this failing submit the exact Tonemap label itself owns the full surviving local packet, with zero descendant payload and zero residual pass-scope commands left unexplained.

The contrast block also sharpened the Tonemap-vs-`L88` distinction without moving the seam. `next_meaningful_after_target=` still lands one scope later at `Command Graph (L88) (Draw)`, but now records the concrete expansion from Tonemap’s tiny self-owned packet to the later heavier UI scope: `workload_delta_from_target={draw_calls=9,draw_indexed_calls=10,pipeline_binds=0,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}` with `next_scope_is_heavier={draw_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,...}` and breadcrumbs `UI_PASS`. So the best current read is: Tonemap is the first real surviving local poisoned boundary candidate on `submit_serial=9`, not just an empty wrapper or a mislabeled envelope for nested work, while `Command Graph (L88) (Draw)` remains the next materially heavier scope that QA should inspect next for downstream amplification versus first-cause evidence. The outer failure signature stayed unchanged in the same artifact (`fence_wait_error submit_serial=9 wait_result=-4`, later `BLIT_PASS`).

---

### Task 52: QA confirm `Tonemap (L87) (Draw)` local attachment is the first real surviving poisoned-boundary candidate on failing `submit_serial=9`

**Bead ID:** `oc-x14`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-x14` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new `tonemap_local_attachment=` block on failing `submit_serial=9`. Confirm whether Tonemap still has zero descendant payload, zero residual scope mismatch, and an exact label-to-scope ownership match; then compare that result against `next_meaningful_after_target=` to decide whether `Command Graph (L88) (Draw)` remains only downstream amplification rather than displacing Tonemap as the first real poisoned-boundary candidate. Save durable notes/artifact references into the repo-owned QA log, update this plan with actual findings, and close bead `oc-x14` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan staged repro (`projection_only + disabled`) on 2026-05-18 using the refreshed source-built editor `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-local-attachment-qa-vulkan-sourcebuild-20260518-2104/`. The failing `submit_serial=9` command summary reproduced the deeper Tonemap-local answer exactly enough to close the bead: `tonemap_local_attachment=` still shows a fully self-owned local Tonemap packet with zero descendant payload (`descendant_labels=0`, `descendant_draw_labels=0`, descendant command counts all zero), and `scope_alignment=` still shows an exact label-to-scope match with zero residual scope mismatch (`scope_matches_label_commands=true`, `scope_matches_label_plus_descendants=true`, `scope_minus_label_plus_descendants={... all zero ...}`). The same payload also preserved the earlier contrast against the next heavier scope: `next_meaningful_after_target=` still lands one scope later at `Command Graph (L88) (Draw)`, and the recorded `workload_delta_from_target=` / `next_scope_is_heavier=` values still read as downstream amplification (`draw_calls +9`, `draw_indexed_calls +10`, `uniform_binds +10`, `vertex_buffer_binds +10`, `index_buffer_binds +1`) rather than displacing Tonemap as the first real surviving poisoned-boundary candidate. The outer failure envelope stayed unchanged in the same run (`submit_serial=8` transfer-worker handoff -> `submit_serial=9` failing frame-1 main submission -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`). One incidental runtime nuisance also surfaced in this pass — repeated Godot `vformat` string-formatting errors from the compositor callback breadcrumb text — but they did not change the Tonemap-local ownership answer or the failing submit classification. This closes `oc-x14` because the requested evidence package is complete and remains internally consistent with the earlier Tonemap pass-scope findings.

---

### Task 53: Deepen `Tonemap (L87)` and explicitly contrast it against `Command Graph (L88)` on failing `submit_serial=9`

**Bead ID:** `oc-t58`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-t58` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` is the first real surviving poisoned-boundary candidate, while `Command Graph (L88) (Draw)` is heavier downstream amplification. Add small, reversible, high-signal instrumentation that both deepens the Tonemap seam itself and explicitly contrasts it against `L88` in the same pass so we can better separate first-cause evidence from downstream amplification. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-t58` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- backend seam diagnostic files and docs as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a focused Tonemap/L88 contrast block to `drivers/vulkan/rendering_device_driver_vulkan.cpp` on the source-built branch so the failing `submit_serial=9` payload now emits `tonemap_l88_contrast=` beside the existing `tonemap_local_attachment=` / `next_meaningful_after_target=` evidence. The new block keeps the staged `projection_only + disabled` repro intact while explicitly recording whether `Command Graph (L88) (Draw)` is the very next meaningful scope, both labels' begin-state ownership, the full `L88` local attachment summary, exact `L88` scope-vs-label-plus-descendants alignment, and workload deltas from Tonemap into `L88`.

Validated with an incremental source build (`scons -j8 platform=linuxbsd target=editor dev_build=yes`) and a fresh host Wayland/Vulkan staged repro using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `projection_only__disabled`. On the reproduced `submit_serial=9` failure, the new payload reports `next_meaningful_scope_is_l88=true`, `scope_distance=1`, Tonemap beginning with no active render pass/pipeline/index state, `L88` beginning with `render_pipeline_bound=true` but still no active render pass/index buffer, `l88_local_attachment.consumer_class="draw_payload"`, `scope_matches_label_plus_descendants=true`, and `tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}`. That keeps Tonemap as the first surviving self-owned poisoned-boundary candidate while making the `L88` downstream amplification packet explicit in the same pass. Fresh artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-t58-tonemap-l88-contrast/`.


### Task 54: QA confirm the refreshed `tonemap_l88_contrast=` block on failing `submit_serial=9`

**Bead ID:** `oc-nw5`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-nw5` and keep the investigation projection-only on the refreshed source-built Godot binary. Use the active plan and the living QA log as source of truth. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs` using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`. Inspect the new `tonemap_l88_contrast=` block on failing `submit_serial=9`. Confirm whether `next_meaningful_scope_is_l88=true` and `scope_distance=1` remain stable, whether Tonemap still reads as the first self-owned poisoned-boundary candidate, and whether `L88` still reads as heavier downstream amplification rather than a seam that displaces Tonemap. Save durable notes/artifact references into the repo-owned QA log, update this plan with actual findings, close bead `oc-nw5` with a clear reason if complete, and report back with artifact root plus exact finding.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan staged repro (`projection_only + disabled`) on 2026-05-18 using the refreshed source-built editor `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-nw5-tonemap-l88-qa/`. The failing `submit_serial=9` payload reproduced the refined Tonemap-vs-L88 answer cleanly: `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` remained stable, `Tonemap (L87) (Draw)` still stayed the first self-owned poisoned-boundary candidate, and `Command Graph (L88) (Draw)` still stayed the immediately following heavier self-owned local packet rather than displacing Tonemap as the first surviving seam. Supporting details stayed internally consistent: Tonemap still begins with no active render pass/pipeline/index state, `L88` still begins with `render_pipeline_bound=true`, `tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}` still marks a materially heavier downstream packet, and `l88_local_attachment` / `scope_alignment` still show a self-owned local draw payload with zero descendant leakage or scope residual mismatch. The outer failure envelope stayed unchanged in the same run (`submit_serial=8` transfer-worker handoff -> `submit_serial=9` failing frame-1 main submission -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`). This closes `oc-nw5` because the requested refreshed-source-build confirmation package is complete and preserves the Tonemap-first read.

---

## Final Results

**Status:** ⚠️ Partial

**What We Built:** This session turned the GDGS/Godot bug hunt from a broad compositor mystery into a narrow backend handoff investigation. The earlier staged work already proved that `projection_only` is the first meaningful failing stage, that a real scratch-only compute dispatch survives, that CPU readbacks are not the root trigger, and that the tracked projection-owned resources remain stable across `projection_begin` → `projection_end` → `projection_post_dispatch_checkpoint_end` with no cleanup churn or RID aliasing. This session extended that by moving onto a source-built Godot binary, instrumenting the post-projection submit/stall/fence path, mapping the exact submissions after projection return, and proving that the failing `submit_serial=9` is the frame-1 main command-graph submission waiting on the semaphore signaled by the transfer-worker `submit_serial=8`.

The key current read is: the projection dispatch still appears to be the first bad event, but the tracked projection resources themselves stay stable and the transfer-worker → frame-1 semaphore handoff appears valid. `submit_serial=8` is a narrow transfer submission with `signal_semaphores=1`; `submit_serial=9` is the following frame-1 main submission with `wait_semaphores=1`, `command_buffers=1`, `present_submission=false`, and a large command graph (`labels=102`, first label `Command Graph (L-1)`, last label `Command Graph (L88) (Draw)`, last breadcrumb `UI_PASS`). That submit queues cleanly, then later dies at `fence_wait_error submit_serial=9 wait_result=-4`, after which the broader lost-device breadcrumb trail still collapses to `BLIT_PASS`. The investigation has now progressively narrowed the backend-owned seam from the broad copy-dominated frame-1 command graph, to the `L8..L15` hotspot, to the first `L15` draw-consumer boundary, and finally to the exact `Render Depth Pre-Pass (L15) (Draw)` consumer label. The newest source-built instrumentation pushed one step deeper into that exact label and showed that it currently behaves as a backend `render_pass_wrapper`, not a direct draw packet: the label begins with no active render pass/pipeline/index state, records `render_pass_begin=1` and `render_pass_end=1`, and records zero direct draw/bind/secondary-execute commands before the later `submit_serial=9` / `BLIT_PASS` collapse.

**Reference Check:** `REF-06` still defines the instrumentation seam map, `REF-07` is now the primary living evidence log for the entire staged repro campaign, and `REF-08` records the earlier independent audit that locked in the projection-first conclusion. The freshest high-signal artifacts in `REF-07` now include the source-built submit/stall/fence correlation run and the submit-8 → submit-9 semaphore provenance run.

**Commits:**
- `2e9a557` - `docs: map GDGS compositor instrumentation seams`
- `c111442` - `debug: add compositor callback breadcrumbs`
- `3b2ce44` - `debug: add GDGS compositor instrumentation gates`
- `e3be761` - `debug: add projection scratch dispatch diagnostics`
- `4523691` - `debug: make scratch probe a real positive control`
- `ba83c1c` - `debug: deepen projection dispatch diagnostics`
- `64287e1` - `Add projection GPU guard diagnostics`
- `7f569a8` - `debug: add projection visibility mirror diagnostics`
- `e610c25` - `debug: add projection lifetime cleanup diagnostics`
- `60bc52d4` - `Fix projection snapshot callable diagnostics`
- `eb3e53f` - `Add post-projection compositor sync snapshots`
- `e9c89177` - `Instrument post-projection fence and submit path`
- `e968db74` - `Add Vulkan submit mapping diagnostics for GDGS compositor stall`
- `dc6b26d1` - `Add transfer-to-frame semaphore provenance diagnostics`
- `68cad1f9` - `debug: add submit 9 late-tail seam diagnostics`
- `c9d03c87` - `debug: classify GDGS depth prepass boundary`
- `63e70bc0` - `debug: add render pass scope breadcrumbs`
- `b6cadca4` - `debug: inspect opaque pass scope ownership`
- `9ae01868` - `debug: deepen tonemap seam ownership`

**Lessons Learned:**
- The live repro is definitively on the global/global RenderingDevice path, not the earlier local/global theory.
- A valid compositor-path scratch dispatch can succeed, so the failure is not “any compute here is cursed”; it is tied to the projection lane and/or the downstream frame work it feeds.
- CPU readbacks, tracked RID aliasing, and tracked cleanup/rebuild churn are not the root trigger.
- The first suspicious backend transition after a stable projection return is the frame-1 main submission `submit_serial=9`, and the submit-8 → submit-9 semaphore handoff currently looks valid rather than stale or reused.

## Fresh Session Start / Next Steps

Start the next session from this plan plus `REF-07`, then execute in this order:

1. **Stay on the source-built Godot binary and keep the repro at `projection_only + disabled`**
   - do not reopen the earlier scratch/projection-first questions unless new evidence forces it
   - keep the staging narrow so the backend handoff logs remain comparable

2. **Run QA once on the refreshed source-built binary and inspect the expanded `depth_prepass_consumer_seam=` payload on failing `submit_serial=9`**
   - the seam currently stays pinned exactly on `Render Depth Pre-Pass (L15) (Draw)`
   - but the new backend classification says that exact label is a `render_pass_wrapper`, not a direct draw packet
   - artifact expectation: the most recent coder pass now emits `draw_boundary_backend_attachment={consumer_class,begin_state,label_commands}` alongside the older `post_draw_non_draw_attachment` / followup fields

3. **Split the backend-owned work immediately under that render-pass wrapper instead of re-widening to the full ancestry**
   - prefer render-pass ownership / nested work evidence around the depth-prepass consumer boundary itself
   - only widen the scope if new QA evidence proves the true surviving seam lives somewhere other than the wrapper currently exposed by `Render Depth Pre-Pass (L15) (Draw)`

4. **If the seam shifts downstream, split only that immediate attached backend work — not the whole ancestry again**
   - the broad copy-chain ancestry is already well classified and should stay demoted unless new evidence contradicts it

Avoid reopening already-closed branches unless the new backend evidence directly points back to them:
- the radix push-constant contract bug is fixed and no longer leads the suspect list
- scratch-only already proved a safe compositor-path compute dispatch exists
- projection-owned tracked resources stayed stable across the critical post-dispatch window
- the 8 → 9 semaphore identity/reuse question is answered and no stale consumer was seen
- the broad `L8..L14` copy prefix and the late `L86..L88` tail have both been demoted below the current depth-prepass seam

---

*Completed on 2026-05-17 (partial; projection-first locked, backend handoff narrowed to failing frame-1 submit `submit_serial=9`)*
