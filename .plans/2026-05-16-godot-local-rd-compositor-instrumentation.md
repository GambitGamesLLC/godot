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

### Task 55: Inspect the Tonemap-to-L88 ownership transition on failing `submit_serial=9`

**Bead ID:** `oc-j8x`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-j8x` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` is the first self-owned surviving poisoned-boundary candidate and `Command Graph (L88) (Draw)` is the immediately following heavier downstream amplification packet. Add small, reversible, high-signal instrumentation that inspects the ownership transition between Tonemap and `L88` so we can see whether the first meaningful ownership expansion after Tonemap happens exactly at `L88`, at `UI_PASS`, or at some smaller backend-owned transition in between. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-j8x` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow Tonemap-to-L88 transition diagnostic on top of the existing `tonemap_l88_contrast=` payload without changing renderer behavior or reopening shader-side guesswork. The Vulkan backend now persists label begin/end state for direct labels, a per-command-buffer backend-command serial, and a tiny unlabeled post-label gap tracker for the most recently closed top-level label. The refreshed `tonemap_l88_contrast=` summary now emits `tonemap_end_state=` plus `tonemap_to_l88_transition=` so QA can answer the exact seam question on failing `submit_serial=9`: whether anything backend-owned happens after Tonemap closes and before `Command Graph (L88) (Draw)` begins, whether the first visible transition is only the breadcrumb jump to `UI_PASS`, or whether the first meaningful expansion is the `L88` local packet itself. Validation run stayed on the source-built Godot branch and kept the staged repro model intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "tonemap_to_l88_transition="` to confirm the refreshed binary contains the new transition payload. No repro was run in this coder pass; QA should use the refreshed binary to capture the actual seam classification. Landed on the active branch as commit `4912ce8d` (`debug: inspect tonemap to l88 transition`).

---

### Task 56: QA confirm the Tonemap-to-L88 ownership transition on failing `submit_serial=9`

**Bead ID:** `oc-bjx`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-bjx` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap-to-L88 transition payload on failing `submit_serial=9`. Confirm whether the first meaningful ownership expansion after Tonemap happens exactly at `Command Graph (L88) (Draw)`, at `UI_PASS`, or at some smaller backend-owned transition in between. Compare the result against the earlier `tonemap_l88_contrast=` / `tonemap_local_attachment=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-bjx` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan staged repro (`projection_only + disabled`) on 2026-05-19 using the refreshed source-built editor `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/`.

The failing `submit_serial=9` payload answered the new seam question directly. `tonemap_end_state=` and `l88_begin_state=` matched exactly (`render_pass_active=false`, `framebuffer_active=false`, `subpass=0`, `render_pipeline_bound=true`, `vertex_binding_count=0`, `index_buffer_bound=false`, breadcrumb `NONE`, `backend_command_serial=11`), and `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,post_gap_first_backend_command=none,post_gap_last_backend_command=none,post_gap_last_breadcrumb="NONE",state_delta={render_pass_active=same,framebuffer_active=same,subpass=same,render_pipeline_bound=same,vertex_binding_count=same,index_buffer_bound=same,breadcrumb=same},first_meaningful_expansion="l88_label"}`. So the first meaningful ownership expansion after Tonemap happens **exactly at `Command Graph (L88) (Draw)`** — not at `UI_PASS`, and not at a smaller backend-owned transition in between.

This new QA evidence preserves the earlier Tonemap-first interpretation instead of displacing it. `tonemap_local_attachment=` still reads as a self-owned local Tonemap packet with zero descendant payload and exact scope alignment, while `l88_local_attachment=` still reads as the next heavier self-owned local draw packet. The same rerun also preserved the prior contrast values (`tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}` and `l88_is_heavier_than_tonemap={draw_calls=true,draw_indexed_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,...}`), so Tonemap still remains the first self-owned poisoned-boundary candidate and `L88` still reads as downstream ownership expansion / amplification.

The outer failure envelope stayed unchanged in the same run (`submit_serial=8` transfer-worker handoff -> `submit_serial=9` failing frame-1 main submission -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`). One incidental runtime nuisance also surfaced again — repeated Godot `vformat` formatting errors from the compositor callback breadcrumb strings after callback return — but it did not change the Tonemap-to-L88 classification or block the QA answer. This closes bead `oc-bjx` because the requested transition classification is now durable, repo-documented, and internally consistent with the earlier Tonemap/L88 evidence.

---

### Task 57: Audit the Tonemap-to-L88 ownership transition findings and next seam

**Bead ID:** `oc-xdd`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-xdd` after beads `oc-j8x` and `oc-bjx` complete. Independently audit whether the new Tonemap-to-L88 transition evidence really advances the seam beyond the current Tonemap-first conclusion, whether the QA artifact supports the claimed ownership transition, and what the next tightest backend-owned seam should be if Tonemap remains the first poisoned-boundary candidate.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the prior audit in `REF-08`, the fresh source diff from commit `4912ce8d` (`debug: inspect tonemap to l88 transition`), and the captured source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/`. Verdict: the new evidence **does** advance the seam one notch beyond the earlier Tonemap-first conclusion, but only in the narrow downstream-transition sense. It does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`; instead it proves that the first meaningful backend-owned ownership expansion *after* Tonemap is exactly `Command Graph (L88) (Draw)`, with no smaller unlabeled backend gap and no `UI_PASS` breadcrumb jump in between. The QA artifact supports that claim directly: `tonemap_end_state=` and `l88_begin_state=` match at `backend_command_serial=11`, `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,...,first_meaningful_expansion="l88_label"}`, and the log still shows `l88_local_attachment=` as the next heavier self-owned local packet rather than hidden descendant leakage. Source inspection matches the artifact: commit `4912ce8d` added per-label begin/end backend-command serial capture plus post-label gap accounting in `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp`, so the observed zero-gap / same-state result is exactly what the new instrumentation was designed to measure. Important audit caveat: this sharpens the **downstream ownership transition** only; it does not add new first-cause evidence against Tonemap itself. Tonemap remains the first self-owned poisoned-boundary candidate because its own local attachment still carries the first real draw payload (`render_pass_begin=1`, `pipeline_binds=1`, `uniform_binds=1`, `draw_calls=1`, `render_pass_end=1`) with zero descendant leakage, while `L88` stays the first downstream amplification packet (`draw_calls=10`, `draw_indexed_calls=10`, `uniform_binds=11`, `vertex_buffer_binds=10`, `index_buffer_binds=1`). Recommended next tight backend-owned seam: inspect **inside Tonemap’s own five-command local packet** rather than the Tonemap→L88 gap. Specifically, split Tonemap’s `begin_render_pass -> bind pipeline/uniform -> single draw -> end_render_pass` sequence so QA can tell whether the poisoned boundary is created at Tonemap render-pass entry/setup, the lone Tonemap draw itself, or Tonemap render-pass teardown. The repeated compositor `vformat` errors remain a nuisance worth fixing for log hygiene, but they do not contradict the Tonemap-to-L88 classification.

---

### Task 58: Split the Tonemap local packet on failing `submit_serial=9`

**Bead ID:** `oc-5f9`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-5f9` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first self-owned poisoned-boundary candidate, and the Tonemap→L88 gap is now exhausted: the first meaningful ownership expansion after Tonemap happens exactly at `Command Graph (L88) (Draw)` with no smaller backend-owned transition in between. Add small, reversible, high-signal instrumentation that splits Tonemap’s own local packet so we can classify its internal backend-owned seam: `begin_render_pass`, pipeline/descriptor bind setup, the single Tonemap draw, and `end_render_pass`. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-5f9` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow Tonemap-local packet split on top of the existing `tonemap_pass_scope=` payload without changing renderer behavior or reopening the already-demoted Tonemap→L88 gap. `DebugLabelEntry` in `drivers/vulkan/rendering_device_driver_vulkan.h` now captures direct-command backend serials for the Tonemap-owned phase boundaries: first `begin_render_pass`, first/last bind-setup command (`bind_render_pipeline` / `bind_render_uniform_sets`), first/last direct draw, and first `end_render_pass`. `drivers/vulkan/rendering_device_driver_vulkan.cpp` now records those serials in `_debug_record_label_backend_command()` and emits a new `local_packet_split=` attachment inside `tonemap_local_attachment=` so QA can classify Tonemap’s internal seam with explicit phase counts, phase serials, setup-command names, backend-gap counts between begin→setup / setup→draw / draw→end, and a coarse `first_internal_expansion` tag.

Validation/build stayed on the refreshed source-built Godot branch and kept the staged repro model intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "local_packet_split={phase_counts="`; and headless binary sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.6fa5aed4c`). No staged repro was run in this coder pass, so there is no new artifact root yet; QA should use the refreshed binary to determine whether the tightest surviving Tonemap seam is render-pass begin, bind/setup, the single draw, or render-pass end. Implementation landed on the active branch as commit `d9d83920` (`debug: split tonemap local packet`).

---

### Task 59: QA classify the Tonemap local packet on failing `submit_serial=9`

**Bead ID:** `oc-19h`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-19h` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap-local instrumentation on failing `submit_serial=9`. Determine which internal Tonemap-owned boundary is the tightest surviving seam — render-pass begin, pipeline/descriptor bind setup, the single Tonemap draw, or render-pass end — while confirming whether Tonemap still remains the first poisoned-boundary candidate. Compare the result against the earlier `tonemap_local_attachment=` / `tonemap_l88_contrast=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-19h` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the minimum valid host-Vulkan source-built repro with the refreshed Tonemap-local instrumentation using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` plus `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` at `projection_only__disabled`, saving the durable artifact package under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/` (`context.txt`, `stdout.log`, `exit_status.txt`; process abort / exit `134`). The new `local_packet_split=` block on failing `submit_serial=9` cleanly classifies the internal Tonemap-owned seam: `phase_counts={render_pass_begin=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,render_pass_end=1}`, `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,draw_first=10,draw_last=10,end_render_pass=11}`, `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets"}`, `phase_gaps={begin_to_setup_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`, and `first_internal_expansion="bind_setup"`. That means the tightest surviving internal Tonemap seam is the bind/setup phase — not render-pass begin, the single Tonemap draw, or render-pass end — and the packet remains fully contiguous once it starts. The earlier Tonemap-first read also held unchanged on the same run: `tonemap_local_attachment=` still has zero descendant payload / exact scope alignment, `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`, and the broader failure envelope remains `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`. The only runtime noise on this pass was the previously seen repeated `vformat` formatting errors from callback breadcrumb text; they did not affect the Tonemap-local classification.

---

### Task 60: Audit the Tonemap local packet findings and next seam

**Bead ID:** `oc-kqh`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-kqh` after beads `oc-5f9` and `oc-19h` complete. Independently audit whether the new Tonemap-local evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed internal Tonemap transition, and what the next tightest backend-owned seam should be if one Tonemap sub-boundary survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-kqh` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the prior audit trail in `REF-08`, the fresh Tonemap-local source diff from commit `d9d83920` (`debug: split tonemap local packet`), and the captured source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/`. Verdict: the new evidence **does** advance the seam inside the current first poisoned-boundary candidate, but only one notch inward. It does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`; instead it shows that the first surviving internal Tonemap-owned expansion happens at the **bind/setup** phase, with the local packet still fully contiguous once it starts.

The QA artifact supports that claim directly. On failing `submit_serial=9`, `tonemap_local_attachment.local_packet_split=` reports `phase_counts={render_pass_begin=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,render_pass_end=1}`, `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,draw_first=10,draw_last=10,end_render_pass=11}`, `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets"}`, `phase_gaps={begin_to_setup_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`, and `first_internal_expansion="bind_setup"`. That matches the source instrumentation exactly: commit `d9d83920` added direct-command serial capture for `first_render_pass_begin_serial`, `first/last_setup_backend_command_serial`, `first/last_draw_backend_command_serial`, and `first_render_pass_end_serial`, then emitted `local_packet_split=` from those captured serials plus gap counts in `drivers/vulkan/rendering_device_driver_vulkan.cpp`. The observed QA output is therefore aligned with what the new instrumentation was designed to measure.

Important audit caveat: this is still a **phase-bucket** answer, not a proof that the first toxic backend command is already isolated to one exact setup call. The current data cleanly demotes render-pass begin, the lone draw, and render-pass end below the combined setup bucket, but `bind_setup` still contains two distinct backend commands at adjacent serials (`bind_render_pipeline` at `8`, `bind_render_uniform_sets` at `9`). So the conclusion is supported as stated — Tonemap remains the first poisoned-boundary candidate, and bind/setup is the tightest surviving internal Tonemap seam — but the bucket itself is not yet exhausted.

Recommended next tight backend-owned seam: split the surviving **Tonemap bind/setup bucket** itself, with the first priority on `bind_render_pipeline` vs `bind_render_uniform_sets` inside the same local packet. Because the packet is contiguous (`begin=7 -> setup=8..9 -> draw=10 -> end=11` with zero backend gap commands between phases), the best next question is whether the first meaningful internal ownership expansion survives exactly at pipeline bind, only after uniform-set bind, or only once the later draw executes despite the setup bucket being present. The repeated compositor `vformat` formatting errors remain log-noise worth fixing for hygiene, but they do not contradict the Tonemap-local seam classification.

---

### Task 61: Split the Tonemap bind/setup bucket on failing `submit_serial=9`

**Bead ID:** `oc-apv`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-apv` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, and the tightest surviving internal Tonemap seam is now the contiguous bind/setup bucket. Add small, reversible, high-signal instrumentation that splits the Tonemap bind/setup bucket so we can classify whether the surviving sub-boundary happens exactly at `bind_render_pipeline`, at `bind_render_uniform_sets`, or only after both setup commands complete. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-apv` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrower Tonemap bind/setup split on top of the existing `tonemap_local_attachment.local_packet_split=` payload without changing renderer behavior or reopening the already-demoted Tonemap→L88 lane. `drivers/vulkan/rendering_device_driver_vulkan.h` now persists dedicated serial slots for direct Tonemap `bind_render_pipeline` and `bind_render_uniform_sets` commands, and `drivers/vulkan/rendering_device_driver_vulkan.cpp` now records those serials in `_debug_record_label_backend_command()` while extending the Tonemap summary with per-command setup serials (`pipeline_first/last`, `uniform_first/last`), an explicit `pipeline_to_uniform_backend_gap_commands` counter, a `setup_sub_boundary=` classification tag, ordered `setup_sequence=` booleans, and packet-shape flags that distinguish pipeline bind from uniform-set bind instead of collapsing both into one setup bucket. This keeps the evidence backend-owned and pass-scope-local: the next QA pass can now answer whether the first surviving Tonemap sub-boundary lands exactly at `bind_render_pipeline`, at `bind_render_uniform_sets`, or only after both setup commands complete before the draw. Validation/build stayed on the refreshed source-built Godot branch and kept the staged repro model intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "setup_sub_boundary="`; and headless binary sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.01751e427`). No staged repro was run in this coder pass, so there is no new artifact root yet; QA should use the refreshed binary to capture the actual bind/setup classification. Implementation commit hash: `5cc35d16` (`debug: split tonemap bind setup bucket`).

---

### Task 62: QA classify the Tonemap bind/setup bucket on failing `submit_serial=9`

**Bead ID:** `oc-0nj`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-0nj` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap bind/setup instrumentation on failing `submit_serial=9`. Determine whether the tightest surviving Tonemap sub-boundary happens exactly at `bind_render_pipeline`, at `bind_render_uniform_sets`, or only after both setup commands complete, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Compare the result against the earlier `local_packet_split=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-0nj` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro with the refreshed Tonemap bind/setup split using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` plus `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` at `projection_only__disabled`, saving the durable artifact package under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/` (`context.txt`, `stdout.log`, `exit_status.txt`; process abort / exit `134`). Durable notes and artifact references were appended to `REF-07`. On the failing `submit_serial=9` command summary, the new Tonemap-local split now answers the tighter setup question directly: `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,pipeline_first=8,pipeline_last=8,uniform_first=9,uniform_last=9,draw_first=10,draw_last=10,end_render_pass=11}`, `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets",pipeline_first="bind_render_pipeline",pipeline_last="bind_render_pipeline",uniform_first="bind_render_uniform_sets",uniform_last="bind_render_uniform_sets"}`, `phase_gaps={begin_to_setup_backend_gap_commands=0,pipeline_to_uniform_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`, `first_internal_expansion="bind_render_pipeline"`, and `setup_sub_boundary="pipeline_then_uniform"`. That means the tightest surviving Tonemap setup sub-boundary now lands **exactly at `bind_render_pipeline` first**, then advances contiguously to `bind_render_uniform_sets`; it is not best described as “uniform bind first” or “only after both setup commands complete.” The earlier Tonemap-first interpretation also held unchanged on the same run: `tonemap_local_attachment=` still shows zero descendant payload / exact scope alignment, `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`, and the broader failure envelope remains `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`. Repeated compositor-callback `vformat` formatting errors still appeared as log noise, but they did not affect the setup-sub-boundary classification.

---

### Task 63: Audit the Tonemap bind/setup findings and next seam

**Bead ID:** `oc-ab8`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-ab8` after beads `oc-apv` and `oc-0nj` complete. Independently audit whether the new Tonemap bind/setup evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed bind/setup sub-boundary, and what the next tightest backend-owned seam should be if one setup command survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-ab8` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the earlier Tonemap-local audit in `REF-08`, the fresh Tonemap bind/setup source diff from commit `5cc35d16` (`debug: split tonemap bind setup bucket`), and the captured source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/`. Verdict: the new evidence **does** advance the seam inside the current first poisoned-boundary candidate, and it does so one notch beyond the earlier undifferentiated bind/setup bucket. It still does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`; instead it shows that the first surviving internal Tonemap-owned expansion now lands exactly at `bind_render_pipeline`, then advances contiguously to `bind_render_uniform_sets`, before the later Tonemap draw and render-pass end.

The QA artifact supports that claim directly. On failing `submit_serial=9`, `tonemap_local_attachment.local_packet_split=` reports `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,pipeline_first=8,pipeline_last=8,uniform_first=9,uniform_last=9,draw_first=10,draw_last=10,end_render_pass=11}`, `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets",pipeline_first="bind_render_pipeline",pipeline_last="bind_render_pipeline",uniform_first="bind_render_uniform_sets",uniform_last="bind_render_uniform_sets"}`, `phase_gaps={begin_to_setup_backend_gap_commands=0,pipeline_to_uniform_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`, `first_internal_expansion="bind_render_pipeline"`, and `setup_sub_boundary="pipeline_then_uniform"`. That matches the source instrumentation in commit `5cc35d16`: the new code records dedicated direct-command serials for Tonemap pipeline bind and uniform-set bind, then emits per-command setup serials plus `pipeline_to_uniform_backend_gap_commands`, `setup_sub_boundary`, and ordered `setup_sequence` booleans from those captured serials. The observed QA output therefore matches what the new backend-owned instrumentation was designed to measure.

Important audit caveat: this is now a **one-command-leading** answer, but not yet a complete one-command proof of causality. The evidence cleanly demotes `bind_render_uniform_sets`, the lone Tonemap draw, and render-pass end below the earlier first setup step because `bind_render_pipeline` is the first surviving internal expansion and there are zero backend-gap commands before uniform bind or draw. But the surviving setup packet is still contiguous from serial `8` through `9`, so this does not yet prove whether the toxic state is created exactly by the pipeline bind itself, by state that only becomes meaningful once the uniform bind lands, or by the two-command setup pair as an inseparable mini-packet.

Recommended next tight backend-owned seam: inspect the **pipeline-bind-owned seam inside Tonemap** rather than reopening the Tonemap→L88 lane or the broader setup bucket. The best next question is whether any smaller backend-owned attachment exists immediately before or after `bind_render_pipeline` inside the Tonemap local packet, or whether the next honest surviving seam is simply `bind_render_pipeline` as the current tightest boundary with `bind_render_uniform_sets` as the first downstream setup amplifier. The repeated compositor `vformat` formatting errors remain log-noise worth fixing for hygiene, but they do not contradict the Tonemap bind/setup sub-boundary classification.

---

### Task 64: Inspect the Tonemap pipeline-bind seam on failing `submit_serial=9`

**Bead ID:** `oc-5nx`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-5nx` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, and the tightest surviving internal Tonemap seam has narrowed to `bind_render_pipeline` first, then `bind_render_uniform_sets`. Add small, reversible, high-signal instrumentation that inspects the Tonemap pipeline-bind-owned seam directly so we can classify whether the surviving boundary lives exactly at the pipeline bind itself, in any immediate backend-owned attachment around it, or only in the contiguous advance into uniform binding. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-5nx` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow Tonemap pipeline-bind seam payload to the source-built Vulkan debug lane without broadening the repro model. In `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp`, introduced lightweight command-state snapshots for the first Tonemap `bind_render_pipeline` and the first following `bind_render_uniform_sets`, then extended `tonemap_local_attachment.local_packet_split` with `pipeline_bind_seam={...}`. The new payload reports the first pipeline-bind serial, the first following uniform-bind serial, zero/nonzero backend gap between them, the tracked command-buffer state immediately before and after the pipeline bind, the tracked state at uniform-bind entry, a state-delta summary, and a compact `owned_attachment_class` that distinguishes `pipeline_bind_direct_state_flip` from a backend gap after pipeline bind, a missing downstream uniform bind, or an unexpected state mutation before uniform bind. This keeps the answer render-pass / command-buffer owned instead of shader-side. Validation/build run in the Godot repo: `git diff --check`, `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`, and `scons -j8 platform=linuxbsd target=editor dev_build=yes` (success; rebuilt `bin/godot.linuxbsd.editor.dev.x86_64`). No staged repro was run in this coder pass, so there is no new artifact root yet. Landed in commit `13a65d69` on branch `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`.

---

### Task 65: QA classify the Tonemap pipeline-bind seam on failing `submit_serial=9`

**Bead ID:** `oc-0mx`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-0mx` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap pipeline-bind instrumentation on failing `submit_serial=9`. Determine whether the tightest surviving Tonemap sub-boundary lives exactly at `bind_render_pipeline`, in an immediate backend-owned attachment around that bind, or only in the contiguous advance into `bind_render_uniform_sets`, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Compare the result against the earlier `setup_sub_boundary=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-0mx` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan repro on the refreshed source-built Godot binary, keeping the staging projection-only (`projection_only + disabled`) and using the same checkpoint harness at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`. New durable artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/`. Exact run command: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955 no_present compositor projection_only disabled 120` (artifact exit `134`). The new `pipeline_bind_seam=` payload inside `tonemap_local_attachment.local_packet_split` cleanly classifies the tightest surviving Tonemap sub-boundary as `owned_attachment_class="pipeline_bind_direct_state_flip"`: `pipeline_serial=8`, `uniform_serial=9`, `pipeline_to_uniform_backend_gap_commands=0`, `state_before.render_pipeline_bound=false`, `state_after.render_pipeline_bound=true`, and `uniform_begin_matches_pipeline_after=true`. So the seam no longer reads as an immediate backend-owned attachment around the bind or as something that only appears in the advance into uniform binding; it now lands exactly on the Tonemap `bind_render_pipeline` state flip itself, with uniform bind remaining the first contiguous downstream amplifier inside the same local packet. The broader source-built baseline did not move: `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate, `Command Graph (L88) (Draw)` remains the next heavier downstream local packet, `submit_serial=9` is still the first failing frame-1 main submission, and the later lost-device breadcrumb still collapses to `BLIT_PASS`. Durable notes and artifact references were appended to `REF-07`.

---

### Task 66: Audit the Tonemap pipeline-bind findings and next seam

**Bead ID:** `oc-x8s`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-x8s` after beads `oc-5nx` and `oc-0mx` complete. Independently audit whether the new Tonemap pipeline-bind evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed pipeline-bind seam, and what the next tightest backend-owned seam should be if a smaller pipeline-bind-owned attachment survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-x8s` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the earlier Tonemap-local audits in `REF-08`, the fresh Tonemap pipeline-bind source diff from commit `7ff68572` (`debug: inspect tonemap pipeline bind seam`), and the captured source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/`. Verdict: the new evidence **does** advance the seam inside the current first poisoned-boundary candidate, and it advances it one notch beyond the earlier `setup_sub_boundary="pipeline_then_uniform"` result. It still does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`; instead it shows that the tightest surviving internal Tonemap sub-boundary now lands exactly on the Tonemap `bind_render_pipeline` state flip itself, with `bind_render_uniform_sets` reduced to the first contiguous downstream amplifier inside the same local packet.

The QA artifact supports that claim directly. On failing `submit_serial=9`, `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=` reports `owned_attachment_class="pipeline_bind_direct_state_flip"`, `pipeline_serial=8`, `uniform_serial=9`, `pipeline_to_uniform_backend_gap_commands=0`, `state_before.render_pipeline_bound=false`, `state_after.render_pipeline_bound=true`, and `uniform_begin_matches_pipeline_after=true`. The broader attachment still keeps Tonemap first: `tonemap_local_attachment` remains a zero-descendant exact scope match, `next_meaningful_after_target.scope.owner_begin="Command Graph (L88) (Draw)"` still marks `L88` as the next heavier downstream packet, and `tonemap_l88_contrast.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion at `L88` instead of displacing Tonemap.

Source inspection matches the artifact. Commit `7ff68572` added `DebugCommandStateSnapshot`, captured state immediately before/after the first Tonemap `bind_render_pipeline` and immediately before the first following `bind_render_uniform_sets`, then emitted `pipeline_bind_seam={...}` with the `owned_attachment_class` classification in `drivers/vulkan/rendering_device_driver_vulkan.cpp`. The observed runtime payload matches exactly what that instrumentation was designed to measure, so the QA claim is supported by both branch source and saved artifact.

Important audit caveat: this is now a very tight **state-flip boundary** answer, but it still is not a full causal proof that the pipeline bind alone is the sole toxic operation in isolation from everything the newly bound pipeline implies. What the evidence *does* demote is the smaller competing seams: there is no surviving backend-owned gap between pipeline bind and uniform bind, and no unexpected tracked state mutation before uniform bind arrives. The surviving uncertainty is now inside the pipeline-bind-owned state itself — e.g. which exact bound pipeline object/provenance or pipeline-owned render state becomes toxic at serial `8` — with `bind_render_uniform_sets` downgraded to the first downstream amplifier rather than the next tightest seam.

Recommended next tight backend-owned seam: inspect the **pipeline-bind-owned state provenance** inside Tonemap rather than reopening the already-demoted pipeline→uniform gap or the broader Tonemap→L88 lane. Concretely, the next bead should capture which exact graphics pipeline object / pipeline compatibility context is being bound at Tonemap serial `8`, then compare that pipeline-owned state against the immediate neighboring pass packets to determine what about the bound pipeline makes this the first surviving backend-owned boundary. The repeated compositor `vformat` formatting errors remain log-noise worth fixing for hygiene, but they do not contradict the Tonemap pipeline-bind classification.

---

### Task 67: Inspect the Tonemap pipeline-state provenance on failing `submit_serial=9`

**Bead ID:** `oc-kbk`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-kbk` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, and the tightest surviving internal Tonemap seam is now the pipeline-bind-owned state flip at serial `8`. Add small, reversible, high-signal instrumentation that inspects the pipeline-state provenance made live by Tonemap’s `bind_render_pipeline` so we can classify what exact graphics pipeline object, compatibility context, or pipeline-owned state becomes active there and how it compares to neighboring pass packets. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-kbk` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small Vulkan-side provenance layer for render-pipeline binds without changing renderer behavior or reopening shader-side probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now persist a compact `DebugPipelineBindingProvenance` record for each graphics pipeline created by `render_pipeline_create()` (pipeline handle, pipeline-layout handle, render-pass handle, render subpass, and shader name), track the currently bound render pipeline on each command buffer, and copy that provenance into the existing command-state snapshots captured around Tonemap’s first `bind_render_pipeline` / `bind_render_uniform_sets` seam. The Tonemap-local `pipeline_bind_seam={...}` payload now includes that bound-pipeline provenance directly inside `state_before`, `state_after`, and `uniform_begin_state`, plus a new `neighboring_pass_pipeline_compare={previous=...,next=...}` block that compares Tonemap’s bound pipeline against the nearest meaningful neighboring pass packets by both exact pipeline identity and compatibility context (`pipeline_layout + render_pass + subpass`). This keeps the answer backend-owned and pass-scope-local: the next QA pass can now tell whether Tonemap’s poisoned boundary is a unique pipeline object, a different pipeline sharing the same compatibility context, or a broader compatibility/context delta relative to its neighboring pass packets. Validation/build stayed on the refreshed source-built editor and projection-only lane intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "neighboring_pass_pipeline_compare="`; and headless sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.7ff68572e`). No staged repro was run in this coder pass, so there is no new artifact root yet. The implementation landed on the active branch as commit `3b84ea95` (`debug: inspect tonemap pipeline state provenance`); this plan entry records that commit in the follow-up documentation update.

---

### Task 68: QA classify the Tonemap pipeline-state provenance on failing `submit_serial=9`

**Bead ID:** `oc-899`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-899` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap pipeline-state provenance instrumentation on failing `submit_serial=9`. Determine what exact pipeline-state provenance becomes live at Tonemap serial `8`, whether that provenance stays unique to Tonemap or matches neighboring pass packets, and whether Tonemap still remains the first poisoned-boundary candidate. Compare the result against the earlier `pipeline_bind_seam=` evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-899` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum host-Vulkan repro on the refreshed source-built Godot binary and appended the durable evidence package to `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Exact artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/`. Exact launch used: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121 no_present compositor projection_only disabled 120` (artifact exit code `134`). The new Tonemap provenance fields resolved the live serial-8 bind to a concrete Tonemap packet: `pipeline_provenance={pipeline_handle="0x61e17cfdfec0",pipeline_layout_handle="0x7f0990db06e0",render_pass_handle="0x61e17ce8dc10",render_subpass=0,shader_name="TonemapShaderRD:0"}`. `neighboring_pass_pipeline_compare=` then classified the comparison as `previous=none` and `next={label="Command Graph (L88) (Draw)",relation="different_pipeline_different_compatibility_context",... shader_name="CanvasShaderRD:0" ...}`. So the best exact read is: the first surviving live pipeline provenance appears exactly at Tonemap serial `8`; it is Tonemap-owned and effectively unique in this slice rather than merely a same-context neighboring pipeline with a different handle; and the next meaningful pass packet (`L88`) differs at both pipeline identity and compatibility/context level. This sharpens the earlier `pipeline_bind_seam=` result without displacing the broader classification: `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`, with the outer failure envelope unchanged (`fence_wait_error submit_serial=9 wait_result=-4` and later `BLIT_PASS`). Minor runtime noise persisted as repeated callback `vformat` formatting errors in the source-built binary logs, but it did not change the provenance classification.

---

### Task 69: Audit the Tonemap pipeline-state provenance findings and next seam

**Bead ID:** `oc-ju6`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-ju6` after beads `oc-kbk` and `oc-899` complete. Independently audit whether the new Tonemap pipeline-state provenance evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed pipeline-state provenance, and what the next tightest backend-owned seam should be if a smaller pipeline-state-owned boundary survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-ju6` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the fresh Tonemap pipeline-state provenance claim against both the active branch source (`3b84ea95`, `debug: inspect tonemap pipeline state provenance`) and the durable QA artifact package at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/`. The source changes do what the QA story depends on: `render_pipeline_create()` persists a `DebugPipelineBindingProvenance` packet per graphics pipeline (pipeline handle, pipeline-layout handle, render-pass handle, subpass, shader name); `command_bind_render_pipeline()` latches that packet onto the active command buffer; `_debug_capture_command_state_snapshot()` copies it into the Tonemap seam snapshots; and the Tonemap summary now compares the Tonemap packet against neighboring meaningful pass packets through `neighboring_pass_pipeline_compare=`. The QA artifact matches that instrumentation exactly: Tonemap serial `8` records `pipeline_provenance={pipeline_handle="0x61e17cfdfec0",pipeline_layout_handle="0x7f0990db06e0",render_pass_handle="0x61e17ce8dc10",render_subpass=0,shader_name="TonemapShaderRD:0"}`, `uniform_begin_matches_pipeline_after=true`, and the next meaningful neighboring packet at `Command Graph (L88) (Draw)` records a different packet (`pipeline_handle="0x7f098cf39dd0"`, `pipeline_layout_handle="0x7f09a412f440"`, same render-pass handle / subpass, `shader_name="CanvasShaderRD:0"`) so the relation honestly classifies as `different_pipeline_different_compatibility_context`. Audit verdict: this evidence **does advance the seam inside the current first poisoned-boundary candidate** and supports the narrower statement that the first surviving live pipeline provenance at serial `8` is effectively unique to Tonemap **in this local surviving slice**. It does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate, and it does **not** prove global uniqueness across every pass in the command buffer — only against the nearest meaningful surviving neighbor that the current comparison exposes (`previous=none`, `next=L88`). The next tightest backend-owned seam should therefore stay **inside Tonemap serial 8 itself**, with `bind_render_pipeline` treated as the current tightest surviving sub-boundary and `bind_render_uniform_sets` as the first downstream setup amplifier inside the same contiguous local packet. If follow-up instrumentation is added, it should stay pipeline-bind-owned: inspect whether any smaller backend-owned attachment survives around the Tonemap pipeline bind itself (for example pipeline object / layout compatibility / bind-time state ownership), before broadening back out to the later Tonemap draw or the heavier `L88` packet. Minor concern preserved for the record: the runtime still emits noisy callback `vformat` formatting errors, but they did not contradict the provenance result.

---

### Task 70: Inspect the bind-owned attachment at Tonemap serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-vhc`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-vhc` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, the tightest surviving internal seam is the pipeline-bind-owned state flip at serial `8`, and the live pipeline provenance there is a concrete Tonemap packet (`TonemapShaderRD:0`) that differs from the next meaningful neighboring pass at both pipeline identity and compatibility/context. Add small, reversible, high-signal instrumentation that inspects any smaller backend-owned attachment around Tonemap serial `8` itself so we can tell whether the surviving boundary is exhausted at the direct pipeline bind, or whether there is a narrower bind-owned attachment/state payload that should now become the next seam. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-vhc` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small, reversible bind-owned attachment refinement on top of the existing Tonemap `pipeline_bind_seam=` diagnostics without reopening broader Tonemap phases or the demoted Tonemap→L88 lane. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now extend the captured `DebugCommandStateSnapshot` with active render-pass / framebuffer handles, thread those handle snapshots through the first Tonemap pipeline-bind and first downstream uniform-bind seam capture, and emit a new `bind_owned_attachment={...}` block inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`. The new block stays backend-owned and pass-scope-local: it reports whether the active pass scope stayed stable across serial `8`, whether the newly-live pipeline provenance packet matches the active render-pass/subpass scope, whether the bind delta was pipeline-only versus coupled with other tracked state flips, and a compact `narrower_bind_owned_seam` hint (`none`, `active_pass_scope`, `pipeline_compatibility_context`, `cohort_state_delta`, or `backend_gap`). This is designed to let QA answer the exact Task 70 question from the refreshed source-built binary: whether the surviving seam is exhausted at the direct `bind_render_pipeline` state flip, or whether a smaller bind-owned attachment around serial `8` survives. Validation/build stayed projection-only and source-built: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "bind_owned_attachment={class="`; and headless sanity launch `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.61f778227`). No staged repro was run in this coder pass, so there is no new artifact root yet; QA should use the refreshed binary to classify whether the seam is now exhausted at the direct bind or whether `bind_owned_attachment=` exposes a narrower surviving attachment. Landed on branch `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` as commit `e0489432` (`debug: inspect tonemap bind owned attachment`) and pushed to remote `gambit`.

---

### Task 71: QA classify the bind-owned attachment at Tonemap serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-rea`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-rea` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new bind-owned-attachment instrumentation on failing `submit_serial=9`. Determine whether the surviving Tonemap seam is exhausted at the direct `bind_render_pipeline` state flip, or whether a smaller backend-owned attachment/state payload around serial `8` becomes the next tightest seam, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Compare the result against the earlier `pipeline_bind_seam=` / provenance evidence, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-rea` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the minimum valid host-Vulkan repro with the refreshed source-built editor using `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` and a fresh durable artifact root at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/`. Exact launch: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044 no_present compositor projection_only disabled 120` (exit `134`, as expected for the repro). The new serial-8 seam evidence does advance beyond the old `pipeline_bind_direct_state_flip` label-only read: inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`, QA observed `bind_owned_attachment={class="pipeline_packet_scope_mismatch",narrower_bind_owned_seam="pipeline_compatibility_context",active_scope_stable_across_bind=true,pipeline_packet_matches_active_scope=false,bind_delta_is_pipeline_only=true,...}`. That means the active pass scope stayed stable across `bind_render_pipeline`, the delta still remained pipeline-only, but the newly-live Tonemap pipeline packet carried a different render-pass compatibility context (`pipeline_packet.render_pass_handle`) than the active render-pass scope even before uniform binding or draw broadened the packet. So the surviving Tonemap seam is **not exhausted** at a pure direct state flip; the tighter surviving backend-owned attachment at serial `8` is the pipeline-compatibility-context mismatch itself. Tonemap still remains the first poisoned-boundary candidate: `tonemap_local_attachment` still has zero descendant payload / exact scope alignment, `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the next ownership expansion exactly at `Command Graph (L88) (Draw)`, and the outer failure envelope still remains `fence_wait_error submit_serial=9 wait_result=-4` followed later by `BLIT_PASS`. Notable runtime noise persisted but did not change the classification: repeated compositor callback `vformat` formatting errors still appeared in the log before the failing submit chain.

---

### Task 72: Audit the bind-owned attachment findings at Tonemap serial `8`

**Bead ID:** `oc-jdq`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-jdq` after beads `oc-vhc` and `oc-rea` complete. Independently audit whether the new bind-owned-attachment evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed serial-8 attachment classification, and what the next tightest backend-owned seam should be if a smaller pipeline-bind-owned attachment survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-jdq` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan plus `REF-07`, inspected the fresh source changes from commits `e0489432` (`debug: inspect tonemap bind owned attachment`) and `8bfa11ed` (docs landing), and re-checked the saved QA artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/`. Verdict: the new evidence only **partially** advances the seam inside the current first poisoned-boundary candidate. It does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate, and it does successfully tighten the question beyond the old bare `pipeline_bind_direct_state_flip` label by proving that the bind delta at serial `8` stays pipeline-only while the active pass scope remains stable across the bind. The QA artifact matches the new instrumentation on those points exactly: `bind_owned_attachment={class="pipeline_packet_scope_mismatch",narrower_bind_owned_seam="pipeline_compatibility_context",active_scope_stable_across_bind=true,pipeline_packet_matches_active_scope=false,bind_delta_is_pipeline_only=true,...}` with Tonemap `state_after.pipeline_provenance={...shader_name="TonemapShaderRD:0",render_pass_handle="0x5e29916b3630",render_subpass=0}` while the active scope on both sides of the bind stays `{render_pass_handle="0x5e2991c8aff0",framebuffer_handle="0x5e2991ba1270",subpass=0}`.

Important audit caveat: the stronger claimed conclusion — that the tighter surviving seam is now a Tonemap-local **pipeline compatibility/context mismatch** — is not yet fully proven by the current artifact. Source inspection shows `pipeline_packet_matches_active_scope` is currently a strict equality check on `render_pass_handle` plus `subpass`, not a richer Vulkan compatibility proof. The saved QA payload also shows the immediately following `Command Graph (L88) (Draw)` packet running under that same active scope handle while carrying a pipeline provenance packet with the **same** pipeline `render_pass_handle="0x5e29916b3630"` and `render_subpass=0`; so the observed handle mismatch is not yet shown to be unique to Tonemap, nor is it yet shown to distinguish a true compatibility break from a broader late-pass render-pass-wrapper/provenance aliasing pattern. So the artifact supports the narrower statement that serial `8` exposes a **pipeline-packet vs active-scope handle mismatch** under a stable pass scope, but it does not yet justify treating that as a fully exhausted Tonemap-specific compatibility seam.

Recommended next tight backend-owned seam: stay inside Tonemap serial `8`, but inspect the **active render-pass provenance / compatibility lineage** behind this mismatch instead of assuming the handle delta already names the cause. Concretely, the next bead should trace why the active pass scope handle (`0x5e2991c8aff0`) differs from the bound pipeline packet render-pass handle (`0x5e29916b3630`), whether those handles are intentionally compatibility-equivalent wrappers, and whether the same mismatch is already present for the immediately following `L88` packet or any earlier surviving draw packet. If that lineage proves the handles are merely compatible aliases, then the current seam has not actually narrowed beyond the direct pipeline bind and the next honest split should move to the smaller pipeline-layout / pipeline-object-owned delta inside the Tonemap packet. The repeated compositor `vformat` errors remain log-noise worth fixing for hygiene, but they do not contradict the bind-owned attachment read.

---

### Task 73: Inspect active render-pass provenance at Tonemap serial `8`

**Bead ID:** `oc-3jv`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-3jv` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, but the latest audit says the stronger `pipeline_compatibility_context` conclusion is not settled yet because the current mismatch check only proves a handle mismatch, not true Vulkan compatibility failure. Add small, reversible, high-signal instrumentation that inspects the active render-pass provenance / compatibility lineage at Tonemap serial `8` so we can explain why the active scope handle and the bound pipeline packet handle differ, whether they are expected compatible aliases/wrappers, and whether the same mismatch pattern appears for `L88` or earlier surviving draw packets. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-3jv` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `drivers/vulkan/rendering_device_driver_vulkan.h`
- `drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added small, reversible Vulkan-side render-pass provenance instrumentation so the Tonemap serial-8 seam can distinguish raw handle mismatch from likely compatible aliasing/wrapping. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now retain compact render-pass lineage fingerprints on both sides of the comparison: each `RenderPassInfo` records a creation serial plus an `exact` hash, a compatibility-oriented hash, and per-subpass compatibility hashes derived from the render-pass attachment/subpass recipe; `DebugPipelineBindingProvenance` and `DebugCommandStateSnapshot` carry those lineage fields through pipeline creation, active render-pass snapshots, and the existing Tonemap seam summaries. The `pipeline_bind_seam=` / `bind_owned_attachment=` payload now reports `pipeline_packet_scope_relation` as one of `same_handle`, `different_handle_same_exact_render_pass_recipe`, `different_handle_same_render_pass_compatibility`, or `different_handle_different_compatibility_context`, and emits a compact `scope_packet_lineage=` object with the active-scope vs packet hashes/serials. Neighboring draw-packet comparisons now also include each packet’s own `scope_packet_relation`, so the next QA pass can tell whether the Tonemap pattern is shared by `Command Graph (L88) (Draw)` or earlier surviving draw packets instead of only seeing Tonemap’s raw handle mismatch in isolation. Validation run: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check`; incremental object-target check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "scope_packet_lineage="`; and `timeout 20s ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version`. No staged repro was run in this coder pass, so there is no new artifact root yet; the next QA pass should generate that evidence on the refreshed source-built binary. Landed in commit `47799b38` (`debug: add render pass provenance lineage`).

---

### Task 74: QA classify active render-pass provenance at Tonemap serial `8`

**Bead ID:** `oc-hge`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-hge` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new active render-pass provenance instrumentation on failing `submit_serial=9`. Determine whether the Tonemap serial-8 pipeline-packet vs active-scope mismatch reflects expected compatible aliasing/wrapping, a broader late-pass pattern shared by `L88` or other surviving draw packets, or a narrower Tonemap-local compatibility lineage issue. Confirm whether Tonemap still remains the first poisoned-boundary candidate, save durable notes/artifact references, update this plan with actual findings, and close bead `oc-hge` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro at `projection_only + disabled` using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` plus the staged harness `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`, saving the durable artifact package at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/`. The fresh source-built run preserved the established downstream envelope (`submit_serial=9` fence-wait failure, later `BLIT_PASS`) while answering the new render-pass lineage question directly. Inside `tonemap_pass_scope.target.tonemap_local_attachment.local_packet_split.pipeline_bind_seam.bind_owned_attachment`, Tonemap serial `8` now classifies as `pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility"` with `active_scope_stable_across_bind=true`, `pipeline_packet_matches_active_scope=false`, `pipeline_packet_shares_compatible_scope=true`, and `bind_delta_is_pipeline_only=true`. The paired `scope_packet_lineage=` block shows why: the active scope and Tonemap packet use different render-pass handles / create serials / exact hashes (`active=create_serial 13, exact 0x5c19e291`; `packet=create_serial 9, exact 0x547d0135`), but they share the same compatibility hash and subpass-compatibility hash (`0x425f4d3d` / `0x81df0cad`). So the old Tonemap packet-vs-active-scope mismatch narrows to expected compatible aliasing/wrapping, not a raw incompatible render-pass break. QA also confirmed that the next meaningful `Command Graph (L88) (Draw)` packet carries the same active-scope relation pattern (`scope_pipeline_relation="different_handle_same_render_pass_compatibility"`) under the same active scope, which means this specific packet-vs-active-scope compatibility pattern is broader than Tonemap alone across the late surviving local-packet slice. But the direct Tonemap-vs-L88 neighboring packet comparison still lands at `different_pipeline_different_compatibility_context`, because the two packets differ at pipeline identity / layout / shader (`TonemapShaderRD:0` vs `CanvasShaderRD:0`) even though both point at the same compatible render-pass lineage object. Earlier surviving draw labels before Tonemap still remain wrapper-only pass scopes rather than comparable self-owned local payload packets, so the strongest shared-pattern evidence is Tonemap plus `L88`, not an earlier surviving draw packet. Tonemap therefore still remains the first self-owned poisoned-boundary candidate: it is still the first meaningful surviving local payload packet, while `L88` remains the immediately following heavier downstream amplification packet. QA appended the durable evidence package and interpretation to `doc/gdgs-compositor-staged-qa-2026-05-17.md`. Minor runtime noise persisted (`vformat` formatting errors in the callback breadcrumb text), but it did not change the classification.

---

### Task 75: Audit active render-pass provenance findings at Tonemap serial `8`

**Bead ID:** `oc-3ta`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-3ta` after beads `oc-3jv` and `oc-hge` complete. Independently audit whether the new active render-pass provenance evidence really resolves or narrows the Tonemap serial-8 compatibility-lineage caveat, whether the QA artifact supports the claimed provenance/aliasing interpretation, and what the next tightest backend-owned seam should be if a narrower compatibility-lineage issue survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-3ta` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor independently re-checked the active plan/log, Godot source commit `47799b38` (`debug: add render pass provenance lineage`), and the saved QA artifact at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/`. The source change really does add the needed provenance fields on both sides of the comparison: render-pass create serial, exact hash, compatibility hash, and per-subpass compatibility hash are now captured for live active scope state (`_debug_capture_command_state_snapshot`) and for graphics-pipeline provenance (`render_pipeline_create`), then compared by the new `render_pass_scope_pipeline_relation(...)` / `scope_packet_lineage` logic inside the Tonemap pass summary. The artifact matches that implementation exactly. On Tonemap pipeline-bind serial `8`, the active scope records handle/create/exact lineage `{handle=0x56701a965130, create_serial=13, exact=0x5c19e291}` while the bound Tonemap packet records `{handle=0x56701a3e8bd0, create_serial=9, exact=0x547d0135}`, yet both sides share `render_pass_compatibility_hash=0x425f4d3d` and `render_subpass_compatibility_hash=0x81df0cad`, producing `pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility"` plus `pipeline_packet_shares_compatible_scope=true`. That means the prior caveat is now materially resolved: the surviving mismatch is a different-handle compatible alias / wrapper lineage, not evidence of an incompatible render-pass context break at Tonemap. The same active-scope-vs-packet relation also appears on the neighboring `Command Graph (L88) (Draw)` packet under the same active scope, which further demotes the raw handle mismatch as the first tight seam because the pattern is not unique to Tonemap. The direct Tonemap-vs-L88 packet comparison still reports `different_pipeline_different_compatibility_context`, but the evidence in the same payload shows that difference is driven by pipeline identity / layout / shader (`TonemapShaderRD:0` vs `CanvasShaderRD:0`), not by the active-scope compatible-alias handle split. Audit verdict: the expected-aliasing interpretation is supported, Tonemap still remains the first self-owned poisoned-boundary candidate, and the next tightest backend-owned seam should stay inside Tonemap’s already-isolated local packet—specifically the pipeline-bind-owned seam at serial `8` (`bind_render_pipeline`) with `bind_render_uniform_sets` at serial `9` treated as the first downstream setup amplifier. Concern noted: this still does not prove the pipeline bind alone is uniquely causal, because the Tonemap setup packet remains contiguous across serials `8 -> 9`; it only proves the older render-pass-handle mismatch caveat is no longer the best surviving explanation.

---

### Task 76: Inspect the Tonemap setup pair contract at serials `8->9` on failing `submit_serial=9`

**Bead ID:** `oc-aol`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-aol` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, the raw render-pass handle mismatch has been demoted to expected compatible aliasing, and the remaining honest uncertainty is whether the harmful setup requires the contiguous Tonemap pair `bind_render_pipeline` at serial `8` plus `bind_render_uniform_sets` at serial `9`. Add small, reversible, high-signal instrumentation that inspects the setup-pair contract across serials `8->9` so we can classify whether the next tight seam lives in the pipeline-owned state alone, the uniform-set-owned state alone, or the specific contract/handshake between them. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-aol` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow Tonemap `setup_pair_contract=` diagnostic on top of the existing serial-8 pipeline-bind / serial-9 uniform-bind seam instrumentation without changing renderer behavior or reopening the already-demoted render-pass-handle caveat. `drivers/vulkan/rendering_device_driver_vulkan.h` now persists lightweight uniform-set creation/bind provenance (`UniformSetInfo` debug fields plus `DebugUniformBindingProvenance` on the first Tonemap uniform bind), and `drivers/vulkan/rendering_device_driver_vulkan.cpp` now captures that provenance during `command_bind_render_uniform_sets()` and emits a new `setup_pair_contract={...}` block inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`. The payload is intentionally contract-focused: it records whether serial `9` carries forward the exact post-serial-8 pipeline state (`pipeline_after_matches_uniform_before`), whether the bound pipeline layout/shader matches the uniform-bind shader/layout, whether the descriptor-set packet itself matches the requested shader layout / declared set indices, and a coarse `next_seam_candidate` hint (`pipeline_owned_state`, `uniform_owned_state`, or `cross_pair_contract`). This keeps the evidence render-pass / command-buffer owned so the next QA pass can classify whether the surviving Tonemap seam is pipeline-owned alone, uniform-owned alone, or the specific 8->9 handshake.

Validation/build stayed on the refreshed source-built Godot branch and preserved the staged repro model: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "setup_pair_contract="`; and headless sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.18bd92b14`). No staged repro was run in this coder pass, so there is no new artifact root yet; QA should use the refreshed binary to capture the actual setup-pair classification on failing `submit_serial=9`. Landed as commit `f586b8e2` (`debug: inspect tonemap setup pair contract`) and pushed to `gambit/gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`.

---

### Task 77: QA classify the Tonemap setup pair contract at serials `8->9` on failing `submit_serial=9`

**Bead ID:** `oc-0cd`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-0cd` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap setup-pair instrumentation on failing `submit_serial=9`. Determine whether the tightest surviving Tonemap seam now lives in the pipeline-owned state alone, the uniform-set-owned state alone, or the specific contract/handshake between serial `8` and serial `9`, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-0cd` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on 2026-05-19 using `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/`.

On the failing `submit_serial=9` command summary, the new `setup_pair_contract=` payload answered the serial-`8 -> 9` seam question cleanly in favor of the **pipeline-owned state alone**, not the uniform-set-owned state alone and not the cross-pair handshake. The payload reports `contract_class="pair_contract_clean"`, `next_seam_candidate="pipeline_owned_state"`, `pair_contract_exact=true`, `pipeline_after_matches_uniform_before=true`, `pipeline_layout_matches_bind_shader=true`, `pipeline_shader_matches_bind_shader=true`, and `uniform_packet_matches_bind_shader=true`. The captured contract checks also stayed exact: `all_sets_match_bind_shader_layout=true`, `all_sets_match_bind_shader_pipeline_layout=true`, `all_sets_match_bind_shader_name=true`, and `all_sets_match_declared_set_index=true`, while `uniform_owner=` preserved the same Tonemap-owned layout/shader lineage already made live by serial `8`.

This moves the seam one notch beyond the earlier “contiguous setup pair” uncertainty without displacing the broader Tonemap-first conclusion. The refreshed run still shows `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility",pipeline_packet_shares_compatible_scope=true,bind_delta_is_pipeline_only=true}`, so the old render-pass-handle caveat remains demoted to expected compatible aliasing rather than a surviving contract problem. The first meaningful downstream ownership expansion after Tonemap also stayed unchanged: `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"`, so `Command Graph (L88) (Draw)` remains the next heavier downstream packet instead of displacing Tonemap. The outer failure envelope likewise stayed stable in the same artifact (`submit_serial=8` transfer-worker handoff -> `submit_serial=9` failing frame-1 main submission -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`). Repeated callback `vformat` formatting errors appeared again as runtime noise, but they did not change the setup-pair classification or block the QA answer.

---

### Task 78: Audit the Tonemap setup pair contract findings at serials `8->9`

**Bead ID:** `oc-0e7`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-0e7` after beads `oc-aol` and `oc-0cd` complete. Independently audit whether the new Tonemap setup-pair evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed serial-8->9 contract classification, and what the next tightest backend-owned seam should be if the pair contract survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-0e7` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan plus the living QA log in `REF-07`, inspected the fresh source changes from commit `f586b8e2` (`debug: inspect tonemap setup pair contract`), and re-checked the saved QA artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/`. Verdict: the new evidence **does** advance the seam inside the current first poisoned-boundary candidate, and this time the advancement is materially stronger than the earlier bind/setup and render-pass-lineage slices. It still does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate, but it does support the narrower conclusion that the surviving Tonemap uncertainty is no longer the contiguous serial-`8 -> 9` setup pair as a contract problem. The saved artifact supports the QA claim directly: `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",pair_contract_exact=true,pipeline_after_matches_uniform_before=true,pipeline_layout_matches_bind_shader=true,pipeline_shader_matches_bind_shader=true,uniform_packet_matches_bind_shader=true,...}` and the full descriptor-set checks stay exact (`all_sets_match_bind_shader_layout=true`, `all_sets_match_bind_shader_pipeline_layout=true`, `all_sets_match_bind_shader_name=true`, `all_sets_match_declared_set_index=true`). Source inspection matches the artifact exactly: commit `f586b8e2` adds `UniformSetInfo` debug lineage, captures `DebugUniformBindingProvenance` at `command_bind_render_uniform_sets()`, and emits the `setup_pair_contract=` classifier by comparing the post-serial-8 pipeline state against the pre-serial-9 uniform-bind state plus the uniform packet’s declared shader/layout/set-index provenance. So the artifact is measuring what the branch actually claims to measure.

Just as important, the same artifact also shows that the older serial-8 attachment caveat is now exhausted rather than revived: `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility",pipeline_packet_shares_compatible_scope=true,bind_delta_is_pipeline_only=true,...}`. That means the prior active-scope-vs-pipeline handle mismatch remains demoted to expected compatible aliasing, and no smaller bind-owned attachment survives around Tonemap serial `8` on the current evidence package. With the pair contract clean and the bind-owned attachment exhausted, the surviving honest seam is the **pipeline-owned state packet alone** made live at Tonemap serial `8`, not the uniform-set-owned state and not the `8 -> 9` handshake.

Audit verdict: the QA conclusion is supported as stated. Tonemap still remains the first self-owned poisoned-boundary candidate; the serial-`8 -> 9` setup pair contract is clean; and the remaining backend-owned seam is pipeline-owned state alone. Important scope caveat: this is a classification result, not yet a one-field causal proof inside the pipeline object. The current instrumentation proves the uniform bind is carrying forward the exact pipeline state and matching the expected shader/layout/set contract, but it does **not** yet isolate which sub-piece of the live Tonemap graphics-pipeline packet (shader/program identity, pipeline layout, render-state recipe, or other pipeline-owned immutable state) is the first toxic backend-owned ingredient. Recommended next tight backend-owned seam: stay **inside the Tonemap pipeline-owned state packet at serial `8`** and split the pipeline packet itself, preferably by contrasting the exact Tonemap graphics-pipeline provenance/state recipe against the next compatible late-pass packet(s) under the same active render-pass lineage. In practice, the next bead should inspect what pipeline-owned state is unique to `TonemapShaderRD:0` once the cross-pair contract and active-scope aliasing have both been exhausted, rather than reopening the uniform-bind lane or broadening back out to `L88`. Repeated compositor callback `vformat` formatting errors remain noisy but do not contradict the setup-pair audit verdict.

---

### Task 79: Inspect the Tonemap pipeline-owned state packet at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-8vs`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-8vs` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, the render-pass handle mismatch has been demoted to expected compatible aliasing, the `8->9` setup pair contract is clean, and the remaining honest seam is the pipeline-owned state packet alone made live at Tonemap serial `8` (`TonemapShaderRD:0`). Add small, reversible, high-signal instrumentation that inspects and contrasts the exact Tonemap graphics-pipeline recipe/provenance against neighboring compatible late-pass packets under the same active render-pass lineage so we can classify what pipeline-owned state is unique to Tonemap and whether a tighter sub-seam emerges inside that packet. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-8vs` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added small, reversible pipeline-owned-state instrumentation on top of the existing Tonemap serial-8 provenance lane without reopening the demoted render-pass-handle or setup-pair theories. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now persist a compact graphics-pipeline recipe fingerprint inside `DebugPipelineBindingProvenance` for each created graphics pipeline: aggregate `graphics_recipe_hash` plus component hashes for shader stages, vertex input, rasterization, multisample, depth/stencil, blending, dynamic state, and specialization constants, along with compact shape/provenance fields (stage mask/count, vertex binding+attribute counts, attachment mask/count, sample count, cull/front-face, depth flags, logic-op flags, and related booleans). The Tonemap summary path now emits those fields directly in `pipeline_provenance=` and extends `neighboring_pass_pipeline_compare=` with `tonemap_pipeline=` plus a `recipe_delta=` block for previous/next meaningful pass packets so QA can classify whether the Tonemap serial-8 packet is unique because of shader-stage recipe, vertex-input recipe, blend/depth/raster state, or some broader aggregate contrast under the same active compatible render-pass lineage.

Validation/build stayed on the refreshed source-built branch and kept the staged repro model intact: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "graphics_recipe_hash="`; and headless sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.0484f557d`). No staged repro was run in this coder pass, so there is no new artifact root yet; QA should use the refreshed source-built binary to decide whether the next seam narrows to a specific Tonemap-unique pipeline state field or remains a broader Tonemap-vs-neighbor recipe contrast. Implementation landed on the active branch as commit `d42bd313` (`debug: inspect tonemap pipeline recipe contrast`).

---

### Task 80: QA classify the Tonemap pipeline-owned state packet at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-i2z`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-i2z` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap pipeline-owned-state instrumentation on failing `submit_serial=9`. Determine what pipeline-owned state is unique to Tonemap’s serial-8 packet versus neighboring compatible late-pass packets, whether that uniqueness yields a tighter sub-seam, and whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-i2z` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan source-built repro on the refreshed pipeline-recipe instrumentation, keeping the staging projection-only (`projection_only + disabled`) and using the same checkpoint harness at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`. Exact artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113/`. Exact launch used: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113 no_present compositor projection_only disabled 120` (artifact abort / exit `134`; durable package includes `context.txt`, `env.txt`, `exact_command.txt`, `stdout.log`, `exit_status.txt`). Durable notes and artifact references were appended to `REF-07`.

On the failing `submit_serial=9` command summary, Tonemap serial `8` still resolves to a concrete `TonemapShaderRD:0` packet with `graphics_recipe_hash="0x9c44278c"`, and the nearest meaningful neighboring late-pass packet remains `Command Graph (L88) (Draw)` with `relation="different_pipeline_different_compatibility_context"`. The new `neighboring_pass_pipeline_compare.next.recipe_delta=` payload classifies the surviving contrast as `graphics_recipe_relation="different_graphics_recipe"` with `graphics_recipe_hash_changed=true` and `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`. So the seam does **not** collapse to one field/component hash; it remains broad across multiple recipe buckets even though other buckets stay unchanged in the same comparison (`shader_stage_recipe_hash`, `rasterization_recipe_hash`, `multisample_recipe_hash`, `depth_stencil_recipe_hash`, `dynamic_state_recipe_hash`, plus the same compatible render-pass lineage). This advances the read inside the pipeline-owned packet by narrowing the candidate space from “the whole pipeline packet” to a specific multi-bucket contrast, but it does **not** yet isolate a single toxic sub-field. The broader source-built baseline did not move: `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` still keeps the old render-pass-handle caveat demoted, `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}` still keeps the `8 -> 9` handshake clean, `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `L88`, and `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`. Repeated compositor-callback `vformat` formatting errors persisted as runtime noise, but they did not change the recipe classification.

---

### Task 81: Audit the Tonemap pipeline-owned state packet findings at serial `8`

**Bead ID:** `oc-cq6`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-cq6` after beads `oc-8vs` and `oc-i2z` complete. Independently audit whether the new Tonemap pipeline-owned-state evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed serial-8 pipeline-state classification, and what the next tightest backend-owned seam should be if a tighter Tonemap pipeline-state sub-seam survives. Update this plan with actual findings, add audit notes if useful, and close bead `oc-cq6` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the fresh coder commit `d42bd313` plus the saved QA artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113/` and found the claim materially supported.

The source change really does advance the seam *inside* Tonemap’s serial-`8` pipeline-owned packet instead of just renaming the old packet. In `render_pipeline_create()`, the new instrumentation now hashes/stores a concrete graphics-pipeline recipe packet (`graphics_recipe_hash`, per-bucket recipe hashes, and shape fields) into `DebugPipelineBindingProvenance`, then emits `recipe_delta=` through the Tonemap/L88 comparison lane in `_debug_command_buffer_tonemap_pass_scope_summary()`. That means the new evidence is derived directly from the live Vulkan pipeline creation inputs plus the already-tracked bind provenance, not from a guessed post-hoc label classification.

The QA artifact matches that source instrumentation exactly on the failing `submit_serial=9` slice. In `stdout.log`, Tonemap serial `8` still resolves to `TonemapShaderRD:0` with `graphics_recipe_hash="0x9c44278c"`; the contiguous `8 -> 9` setup pair still stays clean (`setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",pair_contract_exact=true,...}`); the packet-vs-active-scope caveat remains demoted to compatible aliasing (`bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",pipeline_packet_shares_compatible_scope=true,...}`); and the nearest meaningful downstream comparison is still `Command Graph (L88) (Draw)` with `relation="different_pipeline_different_compatibility_context"`. The new `recipe_delta=` payload on that same Tonemap→L88 contrast reports `graphics_recipe_relation="different_graphics_recipe"`, `graphics_recipe_hash_changed=true`, and `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the remaining buckets stay unchanged (`shader_stage_recipe`, `rasterization`, `multisample`, `depth_stencil`, `dynamic_state`, plus the same compatible render-pass lineage object). So the current evidence really does narrow the surviving seam from “whole pipeline-owned packet” to a smaller **multi-bucket** packet contrast, but it still does **not** collapse to a single toxic field.

Audit verdict: `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate, and the remaining pipeline-owned seam is still broad across multiple recipe buckets rather than one isolated field. The tightest next backend-owned seam to inspect from the surviving changed set should be **`vertex_input_recipe` first**: it is the most structural and least already-explained delta in the surviving contrast, while `pipeline_layout` was already exercised by the clean serial-`8 -> 9` handshake, and `blend_recipe` / `specialization_constants` remain broader shader-config buckets. In practice, the next pass should instrument the Tonemap serial-`8` pipeline creation lane deeply enough to explain *why* its zero-vertex-input/fullscreen-style packet differs from the L88 draw packet and whether that difference is still necessary/sufficient on the failing submit.

Concern discovered: the current classification is only as strong as the chosen nearest comparison packet (`L88`). That is enough to support the broad multi-bucket verdict, but it is **not** yet a proof that one of the four changed buckets is uniquely toxic by itself.

---

### Task 82: Inspect the Tonemap vertex-input recipe seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-c03`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-c03` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, the render-pass mismatch has been demoted to expected compatible aliasing, the `8->9` setup pair contract is clean, and the remaining pipeline-owned seam is still broad across multiple recipe buckets compared with `L88`. Auditor guidance says to split `vertex_input_recipe` first as the sharpest surviving structural delta. Add small, reversible, high-signal instrumentation that inspects and contrasts Tonemap’s vertex-input recipe/state against the nearest meaningful compatible late-pass packet so we can tell whether the seam collapses to a narrower vertex-input-owned sub-boundary or stays broad. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-c03` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added small reversible vertex-input-specific instrumentation on top of the existing Tonemap/L88 pipeline provenance lane in commit `e33c77f9` (`Add Tonemap vertex-input recipe instrumentation`). `DebugPipelineBindingProvenance` now captures a tighter vertex-input packet summary at pipeline creation time: binding/attribute counts, total binding stride, per-binding instance-rate mask, attribute location/binding masks, separate binding-layout / attribute-layout / attribute-format hashes, and a coarse packet class (`null_vertex_input`, `streamed_vertex_input`, `instanced_vertex_input`). `_debug_command_buffer_tonemap_pass_scope_summary()` now emits those fields in each pipeline provenance summary plus a dedicated `vertex_input_delta=` block inside the Tonemap→neighbor recipe comparison so QA can tell whether the surviving seam collapses to a vertex-input-only sub-boundary or stays broad.

**Validation / Repro:**
- `git diff --check`
- `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `scons -j8 platform=linuxbsd target=editor dev_build=yes bin/godot.linuxbsd.editor.dev.x86_64`
- Projection-only staged repro on the refreshed source-built binary:
  `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled <artifact_root> no_present compositor projection_only disabled 120`

**Artifact Root:** `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-coder-vulkan-sourcebuild-20260519-120705/`

**Observed Result:** The failing `submit_serial=9` artifact now makes the vertex-input seam explicit. Tonemap serial `8` stays `vertex_input_class="null_vertex_input"` with zero bindings/zero attributes and no instance-rate usage. The nearest meaningful late-pass compare at `Command Graph (L88) (Draw)` resolves to `vertex_input_class="instanced_vertex_input"` with `binding_count=1`, `attribute_count=8`, `binding_stride_total=128`, `binding_input_rate_mask="0x1"`, `attribute_location_mask="0xff00"`, and non-empty layout/format hashes. The new `vertex_input_delta=` payload classifies that contrast as `relation="null_vs_streamed_vertex_input"` while the surviving changed component set is still `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`.

That means the seam **did not collapse to a narrower vertex-input-only poisoned boundary yet**. The instrumentation narrows the structural story: the sharpest surviving vertex-input contrast is now clearly “Tonemap null/fullscreen-style packet vs L88 instanced canvas packet,” but the broad bucket still survives because blend, specialization constants, and pipeline layout remain changed in the same Tonemap→L88 packet compare. The next QA pass should therefore focus on proving whether `vertex_input_recipe` can be isolated further inside that packet, while keeping the current default expectation that the surviving bucket is still broad until the new evidence says otherwise.

---

### Task 83: QA classify the Tonemap vertex-input recipe seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-lb4`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-lb4` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap vertex-input instrumentation on failing `submit_serial=9`. Determine whether the remaining pipeline-owned seam collapses to a narrower vertex-input-owned sub-boundary or stays broad, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-lb4` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA independently reran the same minimum valid host-Vulkan staged repro on 2026-05-19 using the refreshed source-built editor `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`, still via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` on the host GPU path (`DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`). Durable notes and artifact references were appended to `REF-07` under artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/`.

Exact launch used: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213 no_present compositor projection_only disabled 120` (artifact abort / exit `134`).

The refreshed failing `submit_serial=9` artifact independently confirms the new vertex-input provenance slice without changing the broader seam read. Tonemap serial `8` still resolves to `shader_name="TonemapShaderRD:0"` with `vertex_input_class="null_vertex_input"`, zero bindings, zero attributes, zero stride, `binding_input_rate_mask="0x0"`, and no binding/attribute layout hashes. The nearest meaningful downstream compare at `Command Graph (L88) (Draw)` still resolves to `vertex_input_class="instanced_vertex_input"` with `binding_count=1`, `attribute_count=8`, `binding_stride_total=128`, `binding_input_rate_mask="0x1"`, `attribute_location_mask="0xff00"`, `attribute_binding_mask="0x1"`, and non-empty binding/attribute layout and format hashes. The new `vertex_input_delta=` block classifies that contrast as `relation="null_vs_streamed_vertex_input"` with the same runtime shape split the coder reported (`base_class="null_vertex_input"`, `other_class="instanced_vertex_input"`).

However, QA classified the surviving Tonemap-owned seam as still **broad multi-component rather than vertex-input-owned**. In the same Tonemap -> `L88` compare, `recipe_delta.changed_components` still remains `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, so the evidence does not collapse the poisoned boundary to vertex input alone. The older narrowing lanes also held: `tonemap_to_l88_transition={backend_gap_commands=0,...,first_meaningful_expansion="l88_label"}` still keeps `Tonemap (L87) (Draw)` as the first self-owned poisoned-boundary candidate, while `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",...}` show the prior pipeline-bind/setup caveats stayed reduced instead of re-expanding. Expected runtime noise persisted as repeated compositor callback `vformat` formatting errors plus the same outer failure envelope (`fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`), but those did not alter the classification.

---

### Task 84: Audit the Tonemap vertex-input recipe findings at serial `8`

**Bead ID:** `oc-2n1`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-2n1` after beads `oc-c03` and `oc-lb4` complete. Independently audit whether the new Tonemap vertex-input evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed vertex-input classification, and what the next tightest backend-owned seam should be if the vertex-input bucket still does not isolate it. Update this plan with actual findings, add audit notes if useful, and close bead `oc-2n1` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the earlier audit trail in `REF-08`, the fresh source diff from commit `e33c77f9` (`Add Tonemap vertex-input recipe instrumentation`), and the saved source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/`.

Verdict: the new evidence **does** advance the seam inside the current Tonemap-first poisoned-boundary candidate, but only as a clearer structural classification of the surviving `vertex_input_recipe` delta. It does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`, and it does **not** collapse the remaining recipe seam to a vertex-input-only explanation. Tonemap remains the first self-owned poisoned-boundary candidate, while the surviving Tonemap -> `L88` recipe contrast stays broad across multiple buckets.

The QA artifact supports the classification materially. On failing `submit_serial=9`, Tonemap serial `8` still resolves to `TonemapShaderRD:0` with a null/fullscreen-style vertex-input packet (`vertex_binding_description_count=0`, `vertex_attribute_count=0`, `vertex_binding_stride_total=0`, zero masks, no layout/format hashes), while the nearest meaningful downstream compare at `Command Graph (L88) (Draw)` still resolves to a populated instanced packet (`binding_count=1`, `attribute_count=8`, `binding_stride_total=128`, `binding_input_rate_mask="0x1"`, non-empty binding/attribute layout and format hashes). That matches the source instrumentation added in `e33c77f9`: `render_pipeline_create()` now captures those exact vertex-input shape fields in `DebugPipelineBindingProvenance`, and `_debug_command_buffer_tonemap_pass_scope_summary()` emits them through `vertex_input_delta=` inside the Tonemap/L88 recipe comparison.

Important audit caveat: the artifact and source agree on the **data**, but one label is imprecise. The compare block reports `vertex_input_delta.relation="null_vs_streamed_vertex_input"` even though the same payload also reports `other_class="instanced_vertex_input"` and `binding_input_rate_mask="0x1"`. Source inspection shows that this is a naming limitation in the current relation classifier rather than a contradiction in the captured state: the relation string uses the broad `null_vs_streamed_vertex_input` tag for any null-vs-non-null transition before it later exposes the more specific class fields. So the trustworthy read is still "Tonemap null vertex input vs L88 instanced vertex input," not a literal proof that L88 is non-instanced.

The broader seam therefore remains multi-component exactly as QA reported. In the same Tonemap -> `L88` compare, `recipe_delta.changed_components` still remains `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the earlier narrowing lanes stay reduced instead of reopening: `tonemap_to_l88_transition` still shows no backend gap before `L88`, `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` still keeps the pipeline-bind-owned lane exhausted, and `setup_pair_contract={contract_class="pair_contract_clean",...}` still shows the `8 -> 9` setup pair as internally clean.

Recommended next tight backend-owned seam: inspect **`blend_recipe` next** from the remaining changed set. After this pass, `vertex_input_recipe` is now explained structurally but not isolated causally; `pipeline_layout` has already been partially de-risked by the clean Tonemap `8 -> 9` bind/setup contract; and `specialization_constants` remains a broader shader-config bucket. `blend_recipe` is the tightest still-unexplained fixed-function pipeline-owned delta left in the surviving Tonemap -> `L88` contrast, so the next pass should split that bucket with the same provenance style used here to determine whether Tonemap’s minimal fullscreen/null-vertex-input packet is still merely the first visible candidate or whether a narrower blend-state-owned seam survives.

Concern discovered: the current `vertex_input_delta.relation` string is too coarse and can read as contradictory beside `other_class="instanced_vertex_input"`. That is a log-labeling/documentation issue worth tightening for future audits, but it does not invalidate the underlying Tonemap-vs-L88 vertex-input evidence.

---

### Task 85: Inspect the Tonemap blend-recipe seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-ivp`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-ivp` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, the seam does not collapse to vertex input alone, and auditor guidance says `blend_recipe` is now the tightest remaining unexplained fixed-function pipeline-owned bucket. Add small, reversible, high-signal instrumentation that inspects and contrasts Tonemap’s blend recipe/state against the nearest meaningful compatible late-pass packet so we can tell whether the seam collapses to a narrower blend-owned sub-boundary or stays broad. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-ivp` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small Tonemap-local blend diagnostic layer in the Vulkan backend without changing renderer behavior or widening the investigation. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now preserve richer blend provenance on each graphics pipeline created by `render_pipeline_create()`: alongside the existing aggregate `blend_recipe_hash`, the debug record now keeps `blend_attachment_recipe_hash`, `blend_constant_value_hash`, enabled/write-mask summaries, first-active-attachment blend factors/ops, and whether the pipeline expects dynamic blend constants or constant-factor blending. The command-buffer debug state now also tracks live `vkCmdSetBlendConstants` activity (`set_blend_constants` is recorded as a backend command, and command-state snapshots capture whether blend constants were set, their value hash, the last-set serial, and the live values). The Tonemap `pipeline_bind_seam={...}` / `neighboring_pass_pipeline_compare={...}` payloads now surface a dedicated `blend_delta={...}` block when contrasting Tonemap against the nearest meaningful neighboring late-pass packets, so QA can tell whether the surviving seam narrows specifically to attachment recipe vs constant values vs dynamic blend-state ownership instead of staying a broad pipeline-owned bucket.

Validation stayed on the refreshed source-built projection-only lane: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "blend_delta={relation="`; and headless sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.e727ae7d3`). No staged repro was run in this coder pass, so there is no new artifact root yet. The implementation landed on the active branch as commit `8732c258` (`debug: inspect tonemap blend state seam`).

---

### Task 86: QA classify the Tonemap blend-recipe seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-ifh`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-ifh` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap blend instrumentation on failing `submit_serial=9`. Determine whether the remaining pipeline-owned seam collapses to a narrower blend-owned sub-boundary or stays broad, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-ifh` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- QA notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same minimum valid host-Vulkan staged repro on the refreshed source-built Godot binary and captured the durable evidence in `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). Exact run path stayed comparable to the prior source-built passes: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655 no_present compositor projection_only disabled 120` (artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/`, exit `134`). The failure envelope stayed unchanged: frame-1 main submission `submit_serial=9` still queues, later hits `fence_wait_error ... wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`.

The new instrumentation meaningfully tightened the blend bucket without fully isolating it. In the Tonemap -> `Command Graph (L88) (Draw)` comparison, `blend_delta` now classifies exactly as `relation="different_attachment_recipe_same_constants"`: `attachment_recipe_hash_changed=true`, `constant_value_hash_changed=false`, `blend_enabled_attachment_mask_changed=true`, `dynamic_blend_constants_changed=false`, and `uses_constant_factors_changed=false`. Tonemap stays a no-blend packet (`blend_enabled_attachment_mask="0x0"`, `blend_attachment_recipe_hash="0xb9952359"`, `blend_constant_value_hash="0xf90490b7"`, first active attachment `blend_enable=false`), while the L88 neighbor stays blended with the same constant provenance but a different attachment recipe (`blend_enabled_attachment_mask="0x1"`, `blend_attachment_recipe_hash="0x4e489967"`, same `blend_constant_value_hash="0xf90490b7"`). The tracked live command state also stayed clean for blend constants (`blend_constants={set=false,hash=none,last_set_serial=0,...}`), so this pass demotes dynamic blend-constant ownership as the surviving seam.

Important classification boundary: the seam still does **not** collapse to a blend-only poisoned sub-boundary. `recipe_delta.changed_components` remains broad at `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, and the already-reduced ownership lanes stayed reduced (`bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` plus `setup_pair_contract={contract_class="pair_contract_clean",...}`). So the exact QA read is: the surviving blend-owned lane narrows to an **attachment-recipe-only blend delta with shared constant provenance**, but the overall poisoned boundary remains a broader multi-component Tonemap-owned pipeline seam. `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`. Runtime noise remained limited to repeated compositor callback `vformat` formatting errors (12 occurrences) plus startup Wayland decoration/icon warnings, with no evidence that they changed the classification.

---

### Task 87: Audit the Tonemap blend-recipe findings at serial `8`

**Bead ID:** `oc-55q`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-55q` after beads `oc-ivp` and `oc-ifh` complete. Independently audit whether the new Tonemap blend evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed blend classification, and what the next tightest backend-owned seam should be if the blend bucket still does not isolate it. Update this plan with actual findings, add audit notes if useful, and close bead `oc-55q` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan plus the living QA log (`REF-07`) and independently checked the fresh Tonemap-blend QA artifact at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/`. The audit agrees that commit `8732c258` materially advances the seam *inside* the current first poisoned-boundary candidate without isolating it outright. The new Tonemap -> `Command Graph (L88) (Draw)` compare now proves the surviving blend lane is specifically `blend_delta={relation="different_attachment_recipe_same_constants", ...}`: Tonemap stays a no-blend packet (`blend_enabled_attachment_mask="0x0"`, `blend_attachment_recipe_hash="0xb9952359"`) while the immediate L88 neighbor stays blended (`blend_enabled_attachment_mask="0x1"`, `blend_attachment_recipe_hash="0x4e489967"`), both sharing the same `blend_constant_value_hash="0xf90490b7"`. Live command state also stays clean for dynamic blend constants (`blend_constants={set=false,hash=none,last_set_serial=0,...}`), so this pass successfully demotes dynamic blend-constant ownership and narrows the blend bucket to an attachment-recipe-only contrast with shared constant provenance.

The audit also agrees with QA that this does **not** collapse the poisoned boundary to a blend-only seam. The same artifact still reports `recipe_delta.changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the already-reduced ownership lanes remain reduced: `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",...}`. So the broad classification remains: `Tonemap (L87) (Draw)` is still the first self-owned poisoned-boundary candidate on failing `submit_serial=9`, but the blend-owned sub-lane is now narrowed enough to rule out dynamic blend-constant ownership as the surviving culprit. The exact next recommended seam is the **Tonemap -> L88 specialization-constant delta inside the same immediate neighboring-pass pipeline compare**. Reason: vertex input is now structurally explained (`null_vertex_input` -> `instanced_vertex_input`), blend is now structurally explained (`different_attachment_recipe_same_constants`), and pipeline-layout risk is partially de-risked by the clean Tonemap bind/setup pair contract. That leaves specialization constants as the tightest still-unexplained backend-owned recipe lane inside the current poisoned-boundary candidate. Durable audit notes were appended to `doc/gdgs-compositor-audit-2026-05-17.md` and this plan entry was updated with the final finding.

---

### Task 88: Inspect the Tonemap specialization-constant seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-8c1`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-8c1` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, vertex input is now structurally explained, the blend lane has been narrowed to attachment-recipe-only contrast with shared constant provenance, and the remaining broad Tonemap -> `L88` pipeline-owned seam still includes `specialization_constants` plus `pipeline_layout`. Auditor guidance says to split `specialization_constants` next as the tightest still-unexplained neighboring-pass recipe lane. Add small, reversible, high-signal instrumentation that inspects and contrasts Tonemap’s specialization-constant recipe/state against the immediate meaningful neighboring late-pass packet so we can tell whether the seam collapses to a narrower specialization-constant-owned sub-boundary or stays broad. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-8c1` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small, reversible specialization-constant provenance slice on top of the existing Tonemap/L88 neighboring-pass pipeline compare without widening the repro model or dropping into shader-side probes. `drivers/vulkan/rendering_device_driver_vulkan.h/.cpp` now preserve a tighter specialization packet for each created graphics pipeline: aggregate specialization hash plus separate ID/value hashes, type-mask and per-type counts, min/max constant IDs, and a compact preview of the concrete constants captured at pipeline creation. `_debug_command_buffer_tonemap_pass_scope_summary()` now emits those fields in each `pipeline_provenance=` summary and adds a dedicated `specialization_delta={...}` block inside the Tonemap -> neighboring-pass recipe compare so QA can tell whether the surviving seam collapses to a specialization-owned sub-boundary (`same_ids_different_values`, `different_ids_same_values`, etc.) or stays broad alongside the already-surviving `pipeline_layout` / other recipe buckets.

Validation/build stayed source-built and projection-only on branch `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.h drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object check `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; refreshed editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "specialization_delta={relation="`; and headless sanity check `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --version` (`4.7.beta.custom_build.e398bda96`). No staged repro was run in this coder pass, so there is no fresh artifact root yet; QA should use the refreshed source-built binary to decide whether the remaining Tonemap -> `Command Graph (L88) (Draw)` seam narrows specifically to specialization constants or remains a broader multi-bucket contrast. Implementation landed on the active branch as `debug: inspect tonemap specialization seam` (current branch tip; see `git log -1`).

---

### Task 89: QA classify the Tonemap specialization-constant seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-76k`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-76k` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap specialization-constant instrumentation on failing `submit_serial=9`. Determine whether the remaining Tonemap -> `Command Graph (L88) (Draw)` seam collapses to a specialization-constant-owned sub-boundary or remains broad alongside `pipeline_layout` or other remaining recipe buckets, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-76k` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same source-built host-Vulkan `projection_only__disabled` staged repro with `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-specialization-seam-vulkan-sourcebuild-20260519-164911` (`stdout.log` under that root). The rerun reproduced the expected failure envelope (exit `134`, first failing frame-1 main submission still `submit_serial=9`, later collapse still reaches `BLIT_PASS`). The new Tonemap -> `Command Graph (L88) (Draw)` `specialization_delta={...}` payload proved specialization constants are a real surviving changed recipe bucket at the Tonemap-vs-`L88` seam: Tonemap/base carries zero specialization constants while `L88` carries one `int` specialization constant (`id=0`, `value=0`). But the same `recipe_delta.changed_components` list stayed broad — `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]` — and the comparison still classified as `different_pipeline_different_compatibility_context`, so the seam did **not** collapse to specialization constants alone. Tonemap therefore still classifies as the first poisoned-boundary candidate, with the tightest surviving answer now: a broad multi-bucket pipeline-owned seam that includes specialization constants alongside pipeline layout, vertex input, and blend recipe differences. Durable notes were added to `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`.

---

### Task 90: Audit the Tonemap specialization-constant findings at serial `8`

**Bead ID:** `oc-px0`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-px0` after beads `oc-8c1` and `oc-76k` complete. Independently audit whether the new Tonemap specialization-constant evidence really advances the seam inside the current first poisoned-boundary candidate, whether the QA artifact supports the claimed specialization classification, and what the next tightest backend-owned seam should be if the specialization bucket still does not isolate it. Update this plan with actual findings, add audit notes if useful, and close bead `oc-px0` with a clear reason when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- audit notes/docs/log references as needed
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-read the active plan, the living QA log in `REF-07`, the earlier audit notes in `REF-08`, the fresh source diff from commit `1c2d852d` (`debug: inspect tonemap specialization seam`), and the saved QA artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-specialization-seam-vulkan-sourcebuild-20260519-164911/`. Verdict: the new evidence **does** advance the seam one notch inside the current first poisoned-boundary candidate, but only as a sharper classification of the specialization-constant bucket — not as an isolation of the poisoned boundary to specialization constants alone. Source inspection matches that scope exactly: commit `1c2d852d` extends the Vulkan pipeline provenance with specialization-constant ID/value hashes, type/count summaries, min/max IDs, and a concrete preview payload, then emits `specialization_delta={...}` inside the existing Tonemap -> `Command Graph (L88) (Draw)` neighboring-pass compare. The QA artifact supports that claim directly on failing `submit_serial=9`: Tonemap/base reports an empty specialization recipe (`count=0`, `preview=[]`), while the immediate `L88` neighbor reports exactly one `int` specialization constant (`other={count=1,type_mask="0x2",int_count=1,min_id=0,max_id=0,preview=[{id=0,type="int",bits="0x0",value=0}]}`), and the compare correspondingly classifies `specialization_delta={relation="different_specialization_recipe",hash_changed=true,id_hash_changed=true,value_hash_changed=true,count_changed=true,type_mask_changed=true,...}`. That is real progress beyond the earlier opaque specialization hash, because the bucket is no longer just “different somehow”; it is now concretely “Tonemap has none, L88 has one trivial int specialization constant (id 0 -> value 0)."

However, the same artifact also shows that the poisoned boundary still does **not** collapse to specialization constants. The same immediate neighboring-pass compare remains broad at `recipe_delta.changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the older narrowing lanes remain reduced rather than reopening: `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` still keeps the serial-8 bind-owned lane exhausted, `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}` still keeps the `8 -> 9` handshake clean, and `tonemap_to_l88_transition={backend_gap_commands=0,...,first_meaningful_expansion="l88_label"}` still shows no hidden backend gap between Tonemap and `L88`. So the audit answer is: specialization-constant evidence now explains that bucket structurally, but it still does not isolate the poisoned boundary causally. Tonemap remains the first self-owned poisoned-boundary candidate, and `L88` remains the immediate heavier downstream packet.

Recommended next tight backend-owned seam: inspect the **Tonemap -> `Command Graph (L88) (Draw)` pipeline-layout delta** inside the same immediate neighboring-pass pipeline compare. After this pass, vertex input is structurally explained (`null_vertex_input` vs streamed/instanced input), blend is structurally explained (`different_attachment_recipe_same_constants`), and specialization constants are now structurally explained (`none` vs one trivial `int` constant). The remaining least-explained bucket is pipeline layout, and the earlier clean Tonemap bind/setup contract only de-risks Tonemap’s own `8 -> 9` handshake — it does not explain why Tonemap’s layout packet diverges from `L88`’s neighboring packet. That makes pipeline layout the tightest remaining backend-owned seam to split next if the investigation is to keep narrowing inside the current Tonemap-owned candidate.

---

### Task 91: Inspect the Tonemap pipeline-layout seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-mf8`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-mf8` and keep the investigation projection-only on the refreshed source-built Godot binary. The current best read is that `Tonemap (L87) (Draw)` remains the first poisoned-boundary candidate, vertex input / blend / specialization have each been structurally sharpened but do not isolate the seam, and auditor guidance says the next tightest remaining neighboring-pass recipe lane is the Tonemap -> `Command Graph (L88) (Draw)` `pipeline_layout` delta. Add small, reversible, high-signal instrumentation that inspects and contrasts Tonemap’s pipeline-layout provenance/state against the immediate meaningful neighboring late-pass packet so we can tell whether the seam collapses to a narrower pipeline-layout-owned sub-boundary or stays broad. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; run relevant validation; update this plan with actual results; commit/push the changes; and close bead `oc-mf8` with a clear reason if complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a small reversible Vulkan-side pipeline-layout instrumentation slice in `drivers/vulkan/rendering_device_driver_vulkan.{h,cpp}`. The new slice records per-pipeline layout provenance directly at pipeline creation/bind time — descriptor-set-layout count/hash/preview, first/last descriptor-set-layout handles, push-constant count/hash/stage-mask/size/offset, plus a summarized `pipeline_layout_recipe={...}` on each pipeline provenance packet. The immediate Tonemap -> `Command Graph (L88) (Draw)` neighboring-pass compare now emits `pipeline_layout_delta={...}` with an explicit relation classifier and per-field change flags so QA can tell whether the remaining seam collapses to layout ownership or stays broad.

Validation: source build passed with `scons -j8 platform=linuxbsd target=editor dev_build=yes`. A fresh projection-only staged repro was then run with the source-built Vulkan editor against `projection_only__disabled`, producing artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-layout-seam-vulkan-sourcebuild-20260519-184726/`. On failing `submit_serial=9`, the new evidence says the seam does **not** collapse to a narrow pipeline-layout-only boundary: the immediate Tonemap -> `L88` compare still reports `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, while the new layout lane sharpens from opaque handle drift to a concrete recipe mismatch: `pipeline_layout_delta={relation="different_pipeline_layout_recipe", handle_changed=true, descriptor_set_layout_hash_changed=true, descriptor_set_layout_count_changed=false, first_descriptor_set_layout_handle_changed=true, last_descriptor_set_layout_handle_changed=true, push_constant_hash_changed=true, push_constant_range_count_changed=false, push_constant_stage_mask_changed=true, push_constant_total_offset_changed=false, push_constant_total_size_changed=true, ...}`. Tonemap’s bound layout is now concretely `descriptor_set_layout_count=4`, `push_constant_stage_mask="0x10"`, `push_constant_total_size=112`, while the immediate `L88` neighbor is also count-4 but changes descriptor-set lineage and narrows to `push_constant_stage_mask="0x11"`, `push_constant_total_size=32`.

That means the remaining Tonemap -> `L88` seam stays broad even after the layout lane is structurally explained; pipeline layout is a real differing bucket, but not an isolating one. The Tonemap-owned bind/setup side still stays clean in the same artifact (`bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}`), so this pass strengthens the read that the failure remains attached to the broader Tonemap-vs-neighbor recipe boundary rather than collapsing to Tonemap’s local `8 -> 9` layout handshake alone. Landed in commit `9245ee8f` (`debug: inspect tonemap pipeline layout seam`) and pushed to `gambit/gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`.

---

### Task 92: QA classify the Tonemap pipeline-layout seam at serial `8` on failing `submit_serial=9`

**Bead ID:** `oc-bgd`
**SubAgent:** `primary` (for `qa`)
**Role:** `qa`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-bgd` and keep the investigation projection-only on the refreshed source-built Godot binary. Run the same minimum valid host-Vulkan repro (`projection_only + disabled`) and inspect the new Tonemap pipeline-layout instrumentation on failing `submit_serial=9`. Determine whether the remaining Tonemap -> `Command Graph (L88) (Draw)` seam collapses to a pipeline-layout-owned sub-boundary or remains broad alongside the other surviving recipe buckets, while confirming whether Tonemap still remains the first poisoned-boundary candidate. Save durable notes/artifact references, update this plan with actual findings, and close bead `oc-bgd` with a clear reason if the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** QA reran the same source-built host-Vulkan `projection_only__disabled` staged repro with `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-layout-qa-vulkan-sourcebuild-20260519-195448/` (`stdout.log` plus `exact_command.txt` under that root). The rerun reproduced the expected failure envelope (exit `134`, first failing frame-1 main submission still `submit_serial=9`, later collapse still reaches `BLIT_PASS`).

The new Tonemap -> `Command Graph (L88) (Draw)` `pipeline_layout_delta={...}` payload proved pipeline layout is a real surviving changed recipe bucket at the Tonemap-vs-`L88` seam, but it did **not** collapse the poisoned boundary to pipeline layout alone. The exact observed relation stayed `different_pipeline_layout_recipe`: both pipelines keep `descriptor_set_layout_count=4`, but Tonemap/base carries `descriptor_set_layout_hash="0xb45ace77"`, `push_constant_hash="0x8f2faf58"`, `push_constant_stage_mask="0x10"`, `push_constant_total_size=112`, while the immediate `L88` neighbor changes to `descriptor_set_layout_hash="0x23ba2460"`, `push_constant_hash="0x9b5fef81"`, `push_constant_stage_mask="0x11"`, `push_constant_total_size=32`. That structurally explains the layout bucket down to descriptor-set-layout lineage plus push-constant recipe drift.

Important classification boundary: the seam still remains broad. The same packet compare still reports `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, and the already-reduced ownership lanes stayed reduced (`bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` plus `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}`). So the exact QA read is: **the remaining seam does not collapse to pipeline layout; it remains a broad multi-bucket pipeline-owned seam that includes pipeline layout alongside vertex input, blend recipe, and specialization-constant differences.** `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`. Durable notes were added to `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`.

---

### Task 93: Audit whether Tonemap-vs-`L88` now requires a combined multi-bucket slice or a new compare target

**Bead ID:** `oc-cho`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-cho` and audit the current Tonemap-vs-`L88` seam state after the completed pipeline-layout QA on failing `submit_serial=9`. Use the current evidence package — especially `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-layout-qa-vulkan-sourcebuild-20260519-195448/` plus the living staged notes — to answer one narrow planning question: is the next honest cut (a) a deeper combined multi-bucket Tonemap-vs-`L88` interaction slice across the surviving recipe buckets, or (b) a new comparison target because `L88` is no longer the most informative neighbor? Do not widen back to already-demoted lanes unless the evidence forces it. Write the conclusion, the exact rationale, and the recommended next bead shape back into the plan, then close bead `oc-cho` when the audit package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the current evidence package, centered on `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-layout-qa-vulkan-sourcebuild-20260519-195448/stdout.log`, against the living staged notes in `doc/gdgs-compositor-staged-qa-2026-05-17.md`.

**Conclusion:** choose **(a)** — the next honest cut is a **deeper combined multi-bucket Tonemap-vs-`L88` interaction slice across the surviving recipe buckets**, not a new compare target.

Why this is the honest next move:
- `L88` still remains the **nearest meaningful downstream compare**. The live artifact still reports `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` and `next_meaningful_after_target.scope.owner_begin="Command Graph (L88) (Draw)"`, so there is no new evidence that another neighbor displaced it as the tightest downstream contrast.
- The Tonemap-owned local setup/bind lanes still stay **reduced rather than reopened**. In the same `submit_serial=9` payload, `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}` still demote the serial-`8 -> 9` local handshake as the surviving seam. That means a pivot back into those already-demoted lanes would be dishonest scope widening.
- The freshly added pipeline-layout lane made real progress, but it **did not** produce a better compare target. It only strengthened the already-stable read that the surviving Tonemap-vs-`L88` boundary stays broad: `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]` still spans four recipe buckets at the same exact neighbor boundary.
- `L88` is still informative because it cleanly answers two planning questions at once: (1) Tonemap is still the first poisoned-boundary candidate (`Tonemap (L87) (Draw)` remains the first self-owned meaningful local packet), and (2) the first downstream ownership expansion still begins immediately and gap-free at `L88` (`backend_gap_commands=0`, `gap_has_backend_commands=false`, `first_meaningful_expansion="l88_label"`). No competing compare target in the current evidence gives a tighter or earlier downstream contract.

Planning truth from this audit:
- the investigation should **not** pivot to a new neighbor yet
- the investigation should **not** keep peeling single buckets in isolation as if one remaining field has already won
- the next bead should instead ask how the surviving `vertex_input_recipe` + `blend_recipe` + `specialization_constants` + `pipeline_layout` combination interacts at the Tonemap-vs-`L88` seam while keeping the compare anchored on the same failing `submit_serial=9` path

**Recommended next bead title:** `Audit Tonemap-vs-L88 combined surviving-recipe interaction seam at submit_serial=9`

Recommended bead shape: stay on the same minimum valid source-built host-Vulkan repro (`projection_only + disabled`), keep Tonemap as the first poisoned-boundary candidate and `L88` as the immediate downstream compare, and add one layer of interaction-focused classification over the surviving buckets instead of opening a new neighbor hunt.

---

### Task 94: Audit the combined Tonemap-vs-`L88` surviving-recipe interaction seam at failing `submit_serial=9`

**Bead ID:** `oc-91u`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-91u` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: `L88` remains the best compare target, and no single bucket has isolated alone. Implement the next honest diagnostic cut to classify the **combined interaction** of the surviving Tonemap -> `L88` recipe buckets together — `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout` — at the same poisoned-boundary seam. Prefer the smallest instrumentation or comparison expansion that tells us whether the hazard is really attached to the interaction of those surviving buckets together at Tonemap -> `L88`, rather than to one bucket in isolation. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-91u` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Implemented a small interaction-focused expansion directly inside the existing Tonemap -> neighboring-pass recipe compare lane in `drivers/vulkan/rendering_device_driver_vulkan.cpp`, without reopening already-demoted local Tonemap bind/setup lanes or pivoting to a new neighbor. The new `surviving_bucket_interaction={...}` block reuses the locked surviving bucket set — `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout` — and classifies the compare structurally by checking whether any proper subset of those tracked buckets still matches across the Tonemap -> `Command Graph (L88) (Draw)` seam. The new payload reports the tracked changed-bucket count, whether the full combined bucket hash changes, match counts for 1-bucket / 2-bucket / 3-bucket proper subsets, the largest matching proper-subset size, the minimal distinguishing changed-bucket count, and a compact classification label. Validation/build on the source-built branch: `python3 misc/scripts/file_format.py drivers/vulkan/rendering_device_driver_vulkan.cpp`; `git diff --check -- drivers/vulkan/rendering_device_driver_vulkan.cpp`; targeted object build `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/obj/drivers/vulkan/rendering_device_driver_vulkan.linuxbsd.editor.x86_64.o`; full incremental editor rebuild `scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`; and binary string check `strings bin/godot.linuxbsd.editor.dev.x86_64 | grep -F "surviving_bucket_interaction={tracked_buckets"`.

Ran the refreshed source-built host-Vulkan staged repro on the same `projection_only + disabled` lane with `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64` against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs` via `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`, saving the durable artifact package under `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-combined-interaction-vulkan-sourcebuild-20260520-121325/` (`stdout.log`, `exact_command.txt`, `exit_status.txt`; process abort / exit `134`). The rerun preserved the expected failure envelope (`submit_serial=9` -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`) and the previously locked seam facts (`Tonemap (L87) (Draw)` still first self-owned poisoned-boundary candidate, `L88` still the immediate downstream meaningful compare, and the Tonemap -> `L88` transition still gap-free with `first_meaningful_expansion="l88_label"`).

The new classifier answered the Task 94 planning question directly on the failing `submit_serial=9` packet compare. Inside `recipe_delta=`, the surviving changed set still remains `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`, and the new `surviving_bucket_interaction=` block reports `tracked_changed_bucket_count=4`, `full_combo_hash_changed=true`, `proper_subset_match_counts={size1=0,size2=0,size3=0}`, `largest_matching_proper_subset_size=0`, `minimal_distinguishing_changed_bucket_count=4`, `classification="full_four_bucket_interaction"`, and `minimal_changed_bucket_candidates=[["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]]`. That means **no proper subset of the surviving buckets matches across the locked Tonemap -> `L88` seam** — not any single bucket, not any pair, and not any 3-of-4 subset. The smallest still-distinguishing structural delta at this seam is therefore the full four-bucket set together. Exact conclusion: the current poisoned-boundary contrast is most honestly classified as a **combined Tonemap-vs-`L88` four-bucket interaction**, not a single surviving recipe bucket in isolation. Durable notes were appended to `REF-07` (`/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`). No commit was made in this coder pass.

---

### Task 95: Audit whether the next seam split should classify ownership side of the Tonemap-vs-`L88` full four-bucket interaction

**Bead ID:** `oc-2wq`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-2wq` on start and audit the current Tonemap-vs-`L88` seam state after the completed full-four-bucket interaction classification on failing `submit_serial=9`. Use the current evidence package — especially the updated plan, `REF-07`, and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-combined-interaction-vulkan-sourcebuild-20260520-121325/` — to answer one narrow planning question: do we now have enough truth to stop trying to isolate recipe buckets and instead split **which side of the Tonemap -> `L88` boundary owns the full four-bucket interaction hazard**, or is there still a more honest intermediate cut before ownership-side classification? Do not widen back to demoted lanes unless the evidence forces it. Write the conclusion, rationale, and the recommended exact next bead title/shape back into the plan, then close bead `oc-2wq` when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the active plan, the living staged QA log in `REF-07`, and the fresh source-built artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-combined-interaction-vulkan-sourcebuild-20260520-121325/` (`stdout.log`, `exact_command.txt`, `exit_status.txt`). Conclusion: **yes, ownership-side classification is now the honest next move**. There is not a tighter intermediate recipe-isolation cut left on the current Tonemap -> `L88` seam evidence.

Grounded reasons from the current evidence:
- The new interaction classifier structurally locks the surviving Tonemap -> `L88` seam as `classification="full_four_bucket_interaction"` with `tracked_changed_bucket_count=4`, `proper_subset_match_counts={size1=0,size2=0,size3=0}`, and `minimal_distinguishing_changed_bucket_count=4`. So no single bucket, pair, or 3-of-4 subset survives as a more honest next recipe cut.
- The already-demoted Tonemap-local lanes stay demoted in the same failing artifact: `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` keeps the old bind-owned attachment lane exhausted, and `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}` keeps the serial-`8 -> 9` handshake clean. Reopening those lanes would be dishonest scope widening.
- The Tonemap -> `L88` transition itself is still gap-free and already sharply located: `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,first_meaningful_expansion="l88_label"}`. That means there is no newly-exposed smaller backend-owned transition sitting between Tonemap and `L88` that should be split before ownership-side classification.
- Both sides of the seam are already proven to be self-owned local packets rather than wrappers or hidden descendant scopes. Tonemap remains `target_class="draw_payload"` with `tonemap_local_attachment.scope_alignment={scope_matches_label_commands=true,scope_matches_label_plus_descendants=true,...}` and zero descendant payload; `L88` likewise remains a self-owned local packet with `l88_local_attachment.scope_alignment={scope_matches_label_plus_descendants=true,...}` and zero descendant payload. So the unresolved planning axis is no longer “is there hidden work under one side?” but rather **which side of this now-locked two-packet boundary owns the hazardous four-bucket interaction state**.
- The broader downstream envelope is stable rather than ambiguous: the same run still goes through `submit_serial=9` -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`, while `Tonemap (L87) (Draw)` stays the first self-owned poisoned-boundary candidate and `Command Graph (L88) (Draw)` stays the immediately following heavier downstream packet.

Audit verdict: the more honest next split is no longer another bucket-isolation pass. It is an **ownership-side audit of the full four-bucket interaction seam**: determine whether the hazardous packet ownership reads as primarily Tonemap/source-side, primarily `L88`/downstream-side, or only as a boundary-crossing interaction contract between those two already-self-owned local packets. That is a side-classification question, not a return to per-bucket isolation.

**Recommended next bead title:** `Classify ownership side of the Tonemap-vs-L88 full four-bucket interaction hazard at submit_serial=9`

Recommended bead shape: stay on the same minimum valid source-built host-Vulkan repro (`projection_only + disabled`), keep Tonemap as the first poisoned-boundary candidate and `L88` as the immediate downstream compare, and add the smallest ownership-focused diagnostic needed to classify whether the four-bucket hazard is owned by the Tonemap packet that first makes the state live, by the heavier `L88` packet that first amplifies/consumes it, or only by the exact boundary-crossing interaction between the two. Do not reopen already-demoted bind/setup or render-pass-alias lanes unless new evidence directly contradicts the current artifact.

---

### Task 96: Classify ownership side of the Tonemap-vs-`L88` full four-bucket interaction hazard at failing `submit_serial=9`

**Bead ID:** `oc-o9j`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-o9j` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: the Tonemap -> `L88` seam is structurally a full four-bucket interaction, and no honest intermediate recipe-isolation cut remains. Implement the smallest ownership-focused diagnostic needed to classify whether the hazardous four-bucket interaction reads as owned by (1) the Tonemap packet that first makes the state live, (2) the heavier `L88` packet that first amplifies/consumes it, or (3) only the exact boundary-crossing interaction between the two already-self-owned local packets. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-o9j` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a smallest-possible ownership classifier inside the existing `tonemap_l88_contrast` instrumentation in `drivers/vulkan/rendering_device_driver_vulkan.cpp`. The new `ownership_side_classifier` reuses the already-locked Tonemap-vs-`L88` recipe comparison and local packet/transition evidence to report: whether the four tracked buckets are still structurally locked as `full_four_bucket_interaction`, whether both packets are still self-owned local packets, whether the Tonemap -> `L88` transition remains gap-free, which packet first makes the state live, and which packet is the first immediate heavier consumer/amplifier.

Validation stayed on the same source-built host-Vulkan `projection_only + disabled` repro lane and produced `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-ownership-side-vulkan-sourcebuild-20260520-123925/` (`submit_serial=9`, `fence_wait_error submit_serial=9 wait_result=-4`, later `BLIT_PASS`). In that artifact, `tonemap_l88_contrast.ownership_side_classifier` resolves to `classification="boundary_crossing_interaction"` with `basis="tonemap_makes_state_live_then_l88_first_amplifies_without_gap"`, plus `full_four_bucket_interaction_locked=true`, `tonemap_self_owned_local_packet=true`, `l88_self_owned_local_packet=true`, `transition_gap_free=true`, `first_live_owner="tonemap_local_packet"`, `first_amplifier_owner="l88_local_packet"`, `tonemap_makes_state_live_before_l88=true`, `l88_is_immediate_heavier_consumer=true`, `surviving_changed_bucket_count=4`, and `largest_matching_proper_subset_size=0`.

That makes the current honest read: this hazard is **not** Tonemap-only and **not** `L88`-only. Tonemap still owns the first live-state activation, `L88` still owns the first heavy amplification/consumption, and the hazardous seam reads as the exact boundary-crossing interaction contract between those two already-self-owned local packets.

---

### Task 97: Audit whether the next split should classify the smallest Tonemap-vs-`L88` boundary state-handoff contract at failing `submit_serial=9`

**Bead ID:** `oc-fzh`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-fzh` on start and audit the current Tonemap-vs-`L88` seam state after the completed ownership-side classification on failing `submit_serial=9`. Use the current evidence package — especially the updated plan, `REF-07`, and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-ownership-side-vulkan-sourcebuild-20260520-123925/` — to answer one narrow planning question: is the next honest split now the **smallest state-handoff contract across the Tonemap -> `L88` boundary**, or is there still a more truthful intermediate cut before contract-level classification? Do not widen back to demoted lanes unless the evidence forces it. Write the conclusion, rationale, and the recommended exact next bead title/shape back into the plan, then close bead `oc-fzh` when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the active plan, the living staged QA log in `REF-07`, and the fresh ownership-side artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-ownership-side-vulkan-sourcebuild-20260520-123925/` (`stdout.log`, `exact_command.txt`, `exit_status.txt`). Conclusion: **yes, the next honest split is now the smallest state-handoff contract across the Tonemap -> `L88` boundary.** There is no more truthful intermediate cut left before contract-level classification on the current `submit_serial=9` seam.

Grounded reasons from the current evidence:
- The ownership-side classifier already resolves the surviving seam as `classification="boundary_crossing_interaction"` with basis `tonemap_makes_state_live_then_l88_first_amplifies_without_gap`, plus `full_four_bucket_interaction_locked=true`, `first_live_owner="tonemap_local_packet"`, `first_amplifier_owner="l88_local_packet"`, `tonemap_makes_state_live_before_l88=true`, and `l88_is_immediate_heavier_consumer=true`. That means the honest unresolved object is now the **handoff**, not one side in isolation.
- The bucket-isolation lane is structurally exhausted on this seam. The same evidence package still locks `surviving_bucket_interaction.classification="full_four_bucket_interaction"` with `tracked_changed_bucket_count=4`, `largest_matching_proper_subset_size=0`, and no matching 1-bucket / 2-bucket / 3-bucket proper subset. Reopening per-bucket classification would be dishonest backtracking.
- Both sides are already proven to be self-owned local packets rather than wrappers hiding a smaller ownership pivot. Tonemap still shows `tonemap_local_attachment.consumer_class="draw_payload"` with zero descendant payload and exact scope alignment; `L88` still shows `l88_local_attachment.consumer_class="draw_payload"` with zero descendant payload and exact scope alignment. So there is no newly-exposed hidden-owner cut between “ownership side” and “contract”.
- The Tonemap -> `L88` boundary itself is already as tight as the current instrumentation can honestly make it without becoming a contract classifier. The transition stays gap-free: `tonemap_end_state.backend_command_serial=11`, `l88_begin_state.backend_command_serial=11`, and `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,first_meaningful_expansion="l88_label"}`. There is no smaller backend-owned transition sitting in the gap that should be split first.
- Tonemap-local sub-splits are now demoted relative to the boundary question. The artifact still carries `setup_sub_boundary="pipeline_then_uniform"` and `first_internal_expansion="bind_render_pipeline"`, but the ownership classifier has already shown the hazard does **not** read as Tonemap-only. Those Tonemap-internal details remain useful context for the eventual contract, but they are no longer the next planning axis.
- The broader failure envelope remains stable rather than ambiguous: the same run still goes through `submit_serial=9` -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`, with no new evidence forcing a return to demoted projection, wrapper-only, aliasing, or semaphore-ownership lanes.

Audit verdict: the next bead should classify the **smallest boundary state-handoff contract** between the two already-self-owned local packets — i.e. which exact live state Tonemap leaves behind at the boundary and which exact state `L88` first consumes/amplifies immediately across that zero-gap transition. That is the honest next cut; there is no tighter intermediate planning split left before it.

**Recommended next bead title:** `Classify the smallest Tonemap-vs-L88 boundary state-handoff contract at submit_serial=9`

Recommended bead shape: stay on the same minimum valid source-built host-Vulkan repro (`projection_only + disabled`), keep Tonemap as the first self-owned live-state packet and `L88` as the first immediate heavier consumer, and add the smallest contract-focused instrumentation that records the exact carry-forward boundary state across `tonemap_end_state` -> `l88_begin_state` / `l88_local_attachment`. Prefer state-handoff facts over new ownership or recipe buckets: pipeline/layout provenance that remains live, descriptor/uniform-set binding carryover, render-pass/framebuffer compatibility state, vertex/index binding assumptions, breadcrumb/state deltas, and whether `L88` reuses versus re-establishes each carried field. Do not reopen already-demoted per-bucket, Tonemap-only, `L88`-only, wrapper-only, or transfer/semaphore lanes unless new evidence directly contradicts the current artifact.

---

### Task 98: Classify the smallest Tonemap-vs-`L88` boundary state-handoff contract at failing `submit_serial=9`

**Bead ID:** `oc-a9b`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-a9b` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: the Tonemap -> `L88` seam is a boundary-crossing interaction with no honest intermediate bucket or owner split left. Implement the smallest contract-focused diagnostic needed to classify the exact carried state across the zero-gap Tonemap -> `L88` boundary — especially what remains live from `tonemap_end_state` into `l88_begin_state` / `l88_local_attachment`, what `L88` immediately reuses vs re-establishes, and which state-handoff contract looks like the minimum still-distinguishing boundary hazard. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-a9b` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Claimed bead `oc-a9b`, extended the Vulkan command-label debug capture to persist full `DebugCommandStateSnapshot` records at label begin/end, and added a new `boundary_state_handoff_classifier` block under `tonemap_l88_contrast` so the zero-gap Tonemap -> `L88` seam can be classified from exact carried snapshots instead of only coarse booleans. Rebuilt the source editor and reran the same host-Vulkan `projection_only + disabled` source-built repro. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-contract-side-vulkan-sourcebuild-20260520-145520/` (`stderr.log` still ends in the expected `fence_wait_error submit_serial=9 wait_result=-4`, `stdout.log` now carries the classifier block).

The new evidence says the minimum still-distinguishing boundary hazard is a **carried pipeline packet**, not a broader pass/framebuffer/vertex/index ownership blob. At `tonemap_end_snapshot` and `l88_begin_snapshot`, the same Tonemap pipeline packet (`TonemapShaderRD:0`, same pipeline/layout/render-pass recipe provenance) remains live across a zero backend-command gap while render-pass scope is absent, vertex/index bindings are absent, and breadcrumb stays `NONE`. `L88` then immediately re-establishes its own active render-pass/framebuffer scope before its first pipeline bind, keeps the carried Tonemap packet live up to that bind, rebinds to its own `CanvasShaderRD:0` pipeline on the first pipeline bind, and only then adds vertex/index state before the first uniform bind. The classifier emits `classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index"` with `minimum_distinguishing_hazard="carried_pipeline_packet"`, which is the smallest honest contract that still separates the failing seam.

---

### Task 99: Audit whether the next split should classify the smallest carried Tonemap pipeline-packet contract before `L88` first rebind at failing `submit_serial=9`

**Bead ID:** `oc-752`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-752` on start and audit the current Tonemap-vs-`L88` seam state after the completed boundary state-handoff classification on failing `submit_serial=9`. Use the current evidence package — especially the updated plan, `REF-07`, and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-contract-side-vulkan-sourcebuild-20260520-145520/` — to answer one narrow planning question: is the next honest split now the **smallest carried Tonemap pipeline-packet contract before `L88` first pipeline rebind**, or is there still a more truthful intermediate cut before that contract-level slice? Do not widen back to demoted lanes unless the evidence forces it. Write the conclusion, rationale, and the recommended exact next bead title/shape back into the plan, then close bead `oc-752` when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the active plan, `REF-07`, and the fresh contract-side artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-contract-side-vulkan-sourcebuild-20260520-145520/` (`stdout.log`, `stderr.log`, `exact_command.txt`, `exit_status.txt`). Conclusion: **yes, the next honest split is now the smallest carried Tonemap pipeline-packet contract before `L88` first pipeline rebind.** There is not a more truthful intermediate cut left before that contract-level slice on the current failing `submit_serial=9` seam.

Grounding from the current evidence:
- The boundary-state classifier already collapsed the surviving Tonemap -> `L88` seam to `classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index"` with `minimum_distinguishing_hazard="carried_pipeline_packet"`. That directly demotes the broader pass/framebuffer/vertex/index handoff story and leaves the carried pipeline packet as the minimum still-distinguishing boundary hazard.
- The zero-gap boundary is exact and stable. `tonemap_end_state.backend_command_serial=11` and `l88_begin_state.backend_command_serial=11`, with `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,...}`. There is no hidden backend work between Tonemap and `L88` that would justify an intermediate split.
- The carried state is already proven narrower than a general render scope blob. `boundary_state_handoff_classifier.carried_state={pipeline_relation="same_pipeline",pipeline_packet_exact=true,packet_fields_match=true,scope_absent=true,vertex_index_absent=true,breadcrumb_absent=true,...}` shows the exact Tonemap pipeline packet survives across the boundary while render-pass/framebuffer scope, vertex/index bindings, and breadcrumb do **not**. So the honest next question is not “what else survives?” but “which fields inside that carried packet contract matter before rebind?”
- `L88`’s first meaningful divergence is already localized to its own pre-rebind reconstruction path. The same classifier reports `l88_pipeline_before_keeps_carried_packet=true`, `l88_reestablishes_scope_before_own_pipeline=true`, `l88_has_pipeline_bind=true`, and `carried_to_l88_pipeline_relation="different_pipeline_different_compatibility_context"`. That means the remaining seam is precisely the contract that stays live **up to** `L88`’s first pipeline bind, not an earlier hidden ownership or wrapper split.
- The already-demoted Tonemap-local sublanes remain demoted. The same artifact still carries `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}`. Reopening those lanes would be dishonest widening, not a truer intermediate cut.

Audit verdict: the next bead should classify the **smallest carried Tonemap pipeline-packet contract before `L88` first pipeline rebind** — i.e. which exact carried packet fields remain live from `tonemap_end_snapshot` through `l88_first_pipeline_bind_before_state`, which of those are merely compatible context versus exact carried identity, and which field set is the minimum still-distinguishing pre-rebind hazard. No tighter truthful planning split remains before that contract slice.

**Recommended next bead title:** `Classify the smallest carried Tonemap pipeline-packet contract before L88 first pipeline rebind at submit_serial=9`

Recommended bead shape: stay on the same minimum valid source-built host-Vulkan repro (`projection_only + disabled`), keep Tonemap as the packet that first makes the carried state live and `L88` as the first immediate downstream packet that re-establishes scope before rebinding, and add the smallest packet-contract-focused classification over the pre-rebind carried fields only. Prefer exact carried packet facts — pipeline identity vs compatibility, pipeline-layout / descriptor-set-layout provenance, push-constant-range contract, render-pass compatibility lineage attached to the carried pipeline, and any pre-rebind packet field that remains identical versus merely compatible up to `l88_first_pipeline_bind_before_state`. Do not reopen already-demoted Tonemap-local bind/setup, broader pass/framebuffer ownership, wrapper-chain, or downstream-post-rebind lanes unless new evidence directly contradicts the current artifact.

---

### Task 100: Classify the smallest carried Tonemap pipeline-packet contract before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-f9d`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-f9d` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: the smallest honest remaining seam is the carried Tonemap pipeline-packet contract that survives until `L88`’s first pipeline rebind. Implement the smallest contract-focused diagnostic needed to classify only the **pre-rebind carried packet contract fields** — exact vs merely compatible pipeline identity, pipeline-layout / descriptor-set-layout provenance, push-constant-range contract, and render-pass compatibility lineage attached to the carried packet — and determine which of those fields is the minimum still-distinguishing contract hazard. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-f9d` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Implemented a new `pre_rebind_carried_packet_contract={...}` classifier inside `drivers/vulkan/rendering_device_driver_vulkan.cpp`, then rebuilt the source `godot.linuxbsd.editor.dev.x86_64` binary and reran the same minimum valid host-Vulkan repro (`projection_only + disabled`) on the failing `submit_serial=9` lane. Fresh artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-contract-vulkan-sourcebuild-20260520-151506/`.

Actual evidence from `tonemap_pass_scope.target.tonemap_l88_contrast.pre_rebind_carried_packet_contract=` now classifies the carried packet as `classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage"` with `minimum_distinguishing_hazard="render_pass_compatibility_lineage_attached_to_carried_packet"`.

What the classifier proved on the live failing path:
- the Tonemap -> `L88` boundary is still zero-gap and still carries the exact same Tonemap pipeline packet up to `l88_first_pipeline_bind_before_state`
- `pipeline_identity={relation="same_pipeline",exact=true,...}`: the carried pipeline identity stays exact before rebind
- `pipeline_layout_descriptor_contract={exact=true,...}`: pipeline-layout handle plus descriptor-set-layout provenance stay exact before rebind
- `push_constant_range_contract={exact=true,...}`: the push-constant-range contract also stays exact before rebind
- `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,...}`: the only non-exact carried contract family is the packet’s attached render-pass lineage, which matches the re-established `L88` scope only by compatibility hash / subpass compatibility, not by exact render-pass handle or exact render-pass recipe

That is the smallest honest remaining pre-rebind cut. The minimum still-distinguishing hazard before `L88` performs its own pipeline rebind is **render-pass compatibility lineage attached to the carried Tonemap packet**, not pipeline identity, not pipeline-layout / descriptor-set-layout provenance, and not push-constant-range drift. Durable notes were added to `doc/gdgs-compositor-staged-qa-2026-05-17.md` with the same artifact reference and conclusion.

---

### Task 101: Audit whether the next split should classify the smallest carried render-pass compatibility-lineage contract before `L88` first rebind at failing `submit_serial=9`

**Bead ID:** `oc-hy8`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-hy8` on start and audit the current Tonemap-vs-`L88` seam state after the completed pre-rebind carried packet contract classification on failing `submit_serial=9`. Use the current evidence package — especially the updated plan, `REF-07`, and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-contract-vulkan-sourcebuild-20260520-151506/` — to answer one narrow planning question: is the next honest split now the **smallest carried render-pass compatibility-lineage contract** attached to the Tonemap packet before `L88` first rebind, or is there still a more truthful intermediate cut before that slice? Do not widen back to demoted lanes unless the evidence forces it. Write the conclusion, rationale, and the recommended exact next bead title/shape back into the plan, then close bead `oc-hy8` when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor re-checked the active plan, the living staged QA log in `REF-07`, and the fresh pre-rebind contract artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-contract-vulkan-sourcebuild-20260520-151506/` (`stdout.log`, `stderr.log`, `exact_command.txt`, `exit_status.txt`). Conclusion: **yes, the next honest split is now the smallest carried render-pass compatibility-lineage contract attached to the Tonemap packet before `L88` first rebind.** There is not a more truthful intermediate cut left before that slice on the current failing `submit_serial=9` seam.

- The completed `oc-f9d` classifier already reduced the pre-rebind carried packet to `classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage"` with `minimum_distinguishing_hazard="render_pass_compatibility_lineage_attached_to_carried_packet"`. That directly demotes pipeline identity, pipeline-layout / descriptor-set-layout provenance, and push-constant-range drift as the next planning axis because those contracts are already exact before `L88` rebinds.
- The zero-gap boundary is still exact and stable. `tonemap_end_state.backend_command_serial=11` and `l88_begin_state.backend_command_serial=11`, while `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,...}` shows there is no hidden backend work between Tonemap and `L88` that would justify an intermediate pre-rebind split.
- The carried packet facts are already as small as they can honestly get without splitting the render-pass lineage itself. `pipeline_identity={relation="same_pipeline",exact=true,...}`, `pipeline_layout_descriptor_contract={exact=true,...}`, and `push_constant_range_contract={exact=true,...}` all stay exact up to `l88_first_pipeline_bind_before_state`, while `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,...}` is the only surviving non-exact carried contract family.
- `L88`’s first pre-rebind state reconstruction is already localized enough to support that cut directly. Before its own pipeline bind, `L88` re-establishes active scope with `scope_pipeline_relation="different_handle_same_render_pass_compatibility"` while still keeping the carried Tonemap packet live (`l88_pipeline_before_keeps_carried_packet=true`, `l88_reestablishes_scope_before_own_pipeline=true`). That means the unresolved seam is no longer “which packet field survives?” but specifically **what the compatible-only render-pass lineage contract is across that carried-packet / rebuilt-scope relation**.
- The already-demoted local lanes remain demoted in the same artifact. `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}` still keep Tonemap-local bind/setup reopening off the honest path.

Audit verdict: the next bead should classify the **smallest carried render-pass compatibility-lineage contract** attached to the Tonemap packet before `L88` first rebinds — i.e. which exact lineage fields are merely compatible rather than exact across the carried packet vs re-established active scope, and what the minimum still-distinguishing compatibility-only contract is inside that render-pass family.

**Recommended next bead title:** `Classify the smallest carried render-pass compatibility-lineage contract before L88 first pipeline rebind at submit_serial=9`

Recommended bead shape: stay on the same minimum valid source-built host-Vulkan repro (`projection_only + disabled`), keep Tonemap as the packet that makes the carried state live and `L88` as the immediate downstream scope-rebuilder before rebind, and add the smallest render-pass-lineage-focused classification over the pre-rebind carried contract only. Prefer exact compatibility-lineage facts — active render-pass recipe vs pipeline-attached render-pass recipe, exact-handle vs exact-recipe vs compatibility-hash agreement, subpass/attachment/dependency/view-count lineage, and any lineage field that remains merely compatible rather than exact before `L88` binds its own pipeline. Do not reopen already-demoted packet-identity, layout/descriptor, push-constant, Tonemap-local bind/setup, or post-rebind lanes unless new evidence directly contradicts the current artifact.

---

### Task 102: Classify the smallest carried render-pass compatibility-lineage contract before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-s5m`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-s5m` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: the smallest honest remaining seam is the carried render-pass compatibility-lineage contract attached to the Tonemap packet before `L88` first pipeline rebind. Implement the smallest lineage-focused diagnostic needed to classify which render-pass lineage fields are exact vs merely compatible across the carried Tonemap packet and the rebuilt active `L88` scope before that first rebind, and determine which render-pass lineage field is the minimum still-distinguishing contract hazard. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-s5m` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Coder pass `oc-s5m` implemented the smallest honest extension of the existing pre-rebind carried-packet classifier directly in `drivers/vulkan/rendering_device_driver_vulkan.{h,cpp}` and reran the same source-built host-Vulkan `projection_only + disabled` repro on the failing `submit_serial=9` lane. The new diagnostic persists three exact-only render-pass lineage family hashes onto both the carried Tonemap pipeline provenance and the active pre-rebind command-state snapshot — `render_pass_attachment_exact_hash`, `render_pass_dependency_hash`, and `render_pass_view_density_hash` — then reports a narrow `render_pass_lineage={field_classifier,minimum_distinguishing_field,...}` verdict inside `pre_rebind_carried_packet_contract=` before `L88` first rebinds.

Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-lineage-vulkan-sourcebuild-20260520-180122/` (`exact_command.txt`, `stdout.log`, `stderr.log`, `exit_status.txt`). The run reproduced the same expected failure envelope: abort / exit `134`, `fence_wait_begin submit_serial=9`, `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapsing to `BLIT_PASS`.

The new result answers the planning question directly. On the failing `submit_serial=9` handoff, `pre_rebind_carried_packet_contract={classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage",minimum_distinguishing_hazard="attachment_exact_recipe",...}` now refines the earlier coarse render-pass conclusion. The carried Tonemap packet still keeps exact pipeline identity, exact pipeline-layout / descriptor-set-layout provenance, and exact push-constant-range contract before `L88` rebinds. Inside the remaining render-pass lineage family, the new classifier reports `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,field_classifier="attachment_exact_recipe_is_minimum_hazard",minimum_distinguishing_field="attachment_exact_recipe",handle_match=false,exact_hash_match=false,compatibility_hash_match=true,subpass_compatibility_hash_match=true,attachment_exact_hash_match=false,dependency_hash_match=true,view_density_hash_match=true,subpass_match=true,subpass_count_match=true,attachment_count_match=true,dependency_count_match=true,view_count_match=true,fragment_density_usage_match=true,...}`.

That means the remaining pre-rebind carried-packet hazard is now classified one level deeper than the prior `oc-f9d` result: the carried Tonemap packet and rebuilt active `L88` scope differ by render-pass handle and exact render-pass recipe, but once the compatibility spine is held constant the only still-distinguishing exact-only lineage family is the **attachment exact recipe**. Dependency lineage is exact, view-density lineage is exact, subpass compatibility is exact, counts stay exact, and the minimum still-distinguishing contract hazard before `L88` first pipeline rebind is therefore the carried packet’s `attachment_exact_recipe` lineage rather than a broader render-pass compatibility bucket.

---

### Task 103: Audit whether the next split should classify the smallest carried render-pass attachment exact-recipe contract before `L88` first rebind at failing `submit_serial=9`

**Bead ID:** `oc-ni1`
**SubAgent:** `primary` (for `auditor`)
**Role:** `auditor`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-ni1` on start and audit the current Tonemap-vs-`L88` seam state after the completed pre-rebind render-pass-lineage classification on failing `submit_serial=9`. Use the current evidence package — especially the updated plan, `REF-07`, and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-lineage-vulkan-sourcebuild-20260520-180122/` — to answer one narrow planning question: is the next honest split now the **smallest carried render-pass attachment exact-recipe contract** attached to the Tonemap packet before `L88` first rebind, or is there still a more truthful intermediate cut before that slice? Do not widen back to demoted lanes unless the evidence forces it. Write the conclusion, rationale, and the recommended exact next bead title/shape back into the plan, then close bead `oc-ni1` when complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Auditor reviewed the updated plan, the staged QA notes, and the fresh artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-lineage-vulkan-sourcebuild-20260520-180122/` and the answer is now clean: **yes, the next honest split is the smallest carried render-pass attachment exact-recipe contract attached to the Tonemap packet before `L88` first rebind; there is no more truthful intermediate cut left on this lane.** The new `pre_rebind_carried_packet_contract=` block shows the carried packet still preserves exact pipeline identity, exact pipeline-layout / descriptor-set-layout provenance, and exact push-constant-range contract up to the zero-gap Tonemap -> `L88` boundary, while `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,field_classifier="attachment_exact_recipe_is_minimum_hazard",minimum_distinguishing_field="attachment_exact_recipe",compatibility_hash_match=true,subpass_compatibility_hash_match=true,attachment_exact_hash_match=false,dependency_hash_match=true,view_density_hash_match=true,...}` proves that the broader render-pass lineage family has already been reduced as far as the current evidence can honestly take it. Dependency lineage is exact, view-density lineage is exact, subpass compatibility is exact, counts remain exact, and the Tonemap -> `L88` handoff still stays gap-free with `l88_pipeline_before_keeps_carried_packet=true`, so there is no remaining pre-rebind lineage or handoff bucket to split before the attachment-exact-recipe slice itself. Auditor conclusion: the carried attachment exact-recipe contract is now the smallest truthful next move. Recommended exact next bead title/shape: **Classify the smallest carried render-pass attachment exact-recipe contract before `L88` first pipeline rebind at failing `submit_serial=9`** — specifically, keep the same source-built `projection_only + disabled` repro and instrument only the exact attachment-recipe subfields still attached to the carried Tonemap packet versus the rebuilt active pre-rebind `L88` scope, without reopening already-demoted dependency/view-density/compatibility lanes.

---

### Task 104: Classify the smallest carried render-pass attachment exact-recipe contract before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-sld`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-sld` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked: the smallest honest remaining seam is the carried render-pass attachment exact-recipe contract attached to the Tonemap packet before `L88` first pipeline rebind. Implement the smallest attachment-focused diagnostic needed to classify which render-pass attachment exact-recipe fields are exact vs non-exact across the carried Tonemap packet and the rebuilt active `L88` scope before that first rebind, and determine which attachment exact-recipe field is the minimum still-distinguishing contract hazard. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-sld` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Implemented the smallest attachment-focused diagnostic directly in `rendering_device_driver_vulkan.*` by threading per-field attachment exact-recipe hashes for `format`, `samples`, `load_op`, `store_op`, `stencil_load_op`, `stencil_store_op`, `initial_layout`, and `final_layout` through render-pass creation, carried pipeline provenance, and active pre-rebind scope snapshots. The pre-rebind `render_pass_lineage` report now emits `attachment_exact_recipe={...}` with per-field exact/non-exact classification and a minimum-hazard classifier.

Validation stayed on the same source-built host-Vulkan `projection_only__disabled` lane using artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-attachment-exact-vulkan-sourcebuild-20260520-182834/` (`stdout.log`, `stderr.log`, `exact_command.txt`, `context.txt`, `env.txt`, `exit_status.txt`). The repro still aborts at failing `submit_serial=9`, but the new evidence cleanly classifies the carried Tonemap packet’s remaining render-pass attachment contract hazard: `attachment_exact_recipe={exact=false, field_classifier="load_op_is_minimum_attachment_exact_hazard", minimum_distinguishing_field="load_op", mismatch_count=1, mismatch_fields=["load_op"], attachment_count_match=true, format_match=true, samples_match=true, load_op_match=false, store_op_match=true, stencil_load_op_match=true, stencil_store_op_match=true, initial_layout_match=true, final_layout_match=true, ...}`. That means the broader pre-rebind seam remains exactly where planning locked it, and within that seam the **only** still-distinguishing attachment exact-recipe field between the carried Tonemap packet (`render_pass_create_serial=9`) and the rebuilt active `L88` scope (`render_pass_create_serial=13`) is the attachment `load_op`.

---

### Task 105: Classify the smallest carried attachment `load_op` contract before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-7hi`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-7hi` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked one step deeper: the only still-distinguishing carried render-pass attachment exact-recipe field before `L88` first pipeline rebind is `load_op`. Implement the smallest `load_op`-focused diagnostic needed to classify the surviving carried attachment contract at that zero-gap Tonemap -> `L88` boundary: determine whether the differing `load_op` is isolated to the active pre-rebind scope versus the carried Tonemap packet, whether it tracks a broader attachment-family recipe cohort or a single attachment slot, and what the minimum still-distinguishing `load_op` contract hazard honestly is before `L88` binds its own pipeline. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-7hi` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/exact_command.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/stdout.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/stderr.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/context.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/env.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/exit_status.txt`

**Status:** ✅ Complete

**Results:** Implemented the smallest additional `load_op` diagnostic in `rendering_device_driver_vulkan.*` by threading per-attachment `load_op` values plus a per-slot non-`load_op` recipe cohort hash through render-pass creation, carried pipeline provenance, and active pre-rebind command-state snapshots. The existing `attachment_exact_recipe={...}` payload now also reports `load_op_slot_classifier`, `load_op_contract_scope`, `load_op_minimum_contract_hazard`, `active_load_ops`, `pipeline_load_ops`, and `load_op_mismatch_slots` so the same locked `submit_serial=9` seam can classify slot-vs-cohort without widening into already-demoted lanes.

Validation stayed on the same source-built host-Vulkan `projection_only__disabled` lane using artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/` (`exact_command.txt`, `stdout.log`, `stderr.log`, `context.txt`, `env.txt`, `exit_status.txt`). The repro still aborts at `submit_serial=9` with `exit_status=134`, and the new payload proves the remaining carried attachment contract is narrower than a family-wide recipe rule in this lane: `attachment_exact_recipe={... field_classifier="load_op_is_minimum_attachment_exact_hazard", minimum_distinguishing_field="load_op", load_op_slot_classifier="single_attachment_slot", load_op_contract_scope="single_slot_without_same_recipe_siblings", load_op_minimum_contract_hazard="single_slot_load_op_flip", active_load_ops=["0:LOAD"], pipeline_load_ops=["0:CLEAR"], load_op_mismatch_slots=["{index=0,active="LOAD",pipeline="CLEAR",family_hash_match=true,active_family_hash="0x2a58ca8c",pipeline_family_hash="0x2a58ca8c"}"] ...}`.

What actually happened vs. the original question:
- The differing `load_op` is **not** tracking a broader attachment-family cohort in this locked lane; there is only one surviving attachment slot before `L88` first binds its own pipeline, so the classifier is `single_attachment_slot`.
- The non-`load_op` recipe cohort hash still matches on that slot (`family_hash_match=true`), so the minimum honest hazard remains a **single-slot `CLEAR` vs `LOAD` flip** rather than any broader render-pass recipe change.
- Because there are no same-recipe sibling slots in this pass (`single_slot_without_same_recipe_siblings`), this evidence cannot honestly pin the difference as an active-pre-rebind-only cohort rule or a carried-Tonemap-packet-only cohort rule. It only proves the exact surviving contract at the zero-gap boundary: active pre-rebind scope slot 0 is `LOAD`, carried Tonemap packet slot 0 is `CLEAR`, and every other attachment-exact field still matches.

---

### Task 106: Classify slot-0 `load_op` ownership before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-a4g`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-a4g` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The planning truth is now locked one step deeper: the smallest surviving carried attachment exact-recipe hazard before `L88` first pipeline rebind is a single-slot `load_op` flip (`active slot 0 = LOAD`, carried Tonemap packet slot 0 = CLEAR`) with matching non-`load_op` family hash. Implement the smallest ownership-focused diagnostic needed to classify that slot-0 `load_op` difference: determine whether the surviving hazard is best explained as a carried-packet-owned slot-0 contract, an active pre-rebind scope-owned slot-0 contract, or only as a narrower carried-vs-active slot-local mismatch that cannot yet be attributed farther. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; save durable notes/artifact references; update this plan with what actually happened; and close bead `oc-a4g` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.h`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Claimed bead `oc-a4g`, kept the same source-built host-Vulkan `projection_only__disabled` repro lane, and added the smallest ownership-focused follow-up directly inside the existing carried attachment exact-recipe payload. `drivers/vulkan/rendering_device_driver_vulkan.cpp` now emits `load_op_ownership_classifier={...}` alongside the already-locked slot-0 `load_op` diff so the same `submit_serial=9` seam can say whether the mismatch is best explained by the carried Tonemap packet, the active pre-rebind scope, or only a narrower slot-local carried-vs-active mismatch.

Validation used artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-slot0-load-op-ownership-vulkan-sourcebuild-20260520-211039/` (`exact_command.txt`, `stdout.log`, `stderr.log`, `exit_status.txt`) with the same source-built binary and repro script. The run still aborts at `submit_serial=9` with `exit_status=134`, but the new ownership evidence is decisive at the requested scope: `load_op_ownership_classifier={classification="slot_local_carried_vs_active_mismatch_unattributed", best_explanation="carried_vs_active_slot_local_mismatch", basis="single_slot_mismatch_with_matching_non_load_recipe_and_no_same_recipe_siblings", slot_index=0, active_scope_reestablished_before_rebind=true, carried_packet_kept_live_before_rebind=true, family_hash_match=true, active_load_op="LOAD", pipeline_load_op="CLEAR", active_render_pass_create_serial="0xd", pipeline_render_pass_create_serial="0x9"}`.

What actually happened vs. the task question:
- The active pre-rebind scope really does own a re-established compatible render-pass instance before `L88` first binds its own pipeline (`active_render_pass_create_serial=13`).
- The carried Tonemap packet also really does survive the zero-gap boundary into that pre-rebind state (`pipeline_render_pass_create_serial=9`, `carried_packet_kept_live_before_rebind=true`).
- But there is still only one surviving attachment slot, its non-`load_op` family hash matches on both sides, and there are no same-recipe sibling slots to show a side-local cohort rule. So the honest conclusion is **only** the narrower `carried_vs_active_slot_local_mismatch` bucket — not a farther attribution to `carried_packet_owned_slot_0_contract` or `active_pre_rebind_scope_owned_slot_0_contract`.

---

### Task 107: Split slot-0 `load_op` attribution between active-scope rebuild and carried-packet persistence before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-1l7`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-1l7` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: the smallest surviving seam before `L88` first pipeline rebind is a single-slot `load_op` mismatch on slot 0 (`active=LOAD`, carried Tonemap packet=`CLEAR`), but ownership is still unattributed. Implement the smallest attribution-focused diagnostic needed to split whether that slot-0 mismatch first becomes attributable at the active pre-rebind scope reconstruction step or at the carried-packet persistence step, while keeping the zero-gap Tonemap -> `L88` boundary model intact. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; save durable notes/artifact references; update this plan with what actually happened; and close bead `oc-1l7` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a narrow `load_op_attribution_split` diagnostic beside the existing slot-local ownership classifier in `drivers/vulkan/rendering_device_driver_vulkan.cpp`, without widening the capture lane beyond the locked Tonemap -> `L88` zero-gap boundary model. The new output records the slot-0 `load_op` value at `tonemap_end_snapshot`, `l88_begin_snapshot`, the carried pre-rebind pipeline packet, and the active pre-rebind scope, then classifies which side first introduces the mismatch.

Validation stayed on the same source-built host-Vulkan `projection_only + disabled` repro lane. I rebuilt the source binary (`scons platform=linuxbsd target=editor dev_build=yes -j8 bin/godot.linuxbsd.editor.dev.x86_64`), confirmed the new marker string was present in the binary, then reran the staged case against `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`. Durable artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-slot0-load-op-attribution-vulkan-sourcebuild-20260520-213316/`.

What actually happened vs. the task question:
- The failure stayed locked to the same seam: `submit_serial=9` still ends in `fence_wait_error ... wait_result=-4` with last breadcrumb `BLIT_PASS`.
- The new classifier reported `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", first_attributable_step="active_pre_rebind_scope_reconstruction", boundary_packet_exact=true, pre_rebind_packet_exact=true, active_scope_available=true, tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD", slot_index=0}`.
- That means the carried Tonemap packet keeps slot 0 exactly `CLEAR` across `tonemap_end -> l88_begin -> pre_rebind pipeline packet`; the first new divergent value appears only when the active compatible render-pass scope is rebuilt before `L88` first rebinds its own pipeline, where slot 0 becomes `LOAD`.
- So the surviving hazard no longer stays only in the earlier undifferentiated `carried_vs_active_slot_local_mismatch` bucket. Temporally, it first becomes attributable to the **active pre-rebind scope reconstruction** side, while the carried packet persistence side remains exact across the zero-gap boundary.

---

### Task 108: Classify the active-scope rebuild recipe that flips slot-0 `load_op` to `LOAD` before `L88` first pipeline rebind at failing `submit_serial=9`

**Bead ID:** `oc-vpw`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/` and `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`, claim bead `oc-vpw` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: the carried Tonemap packet keeps slot-0 `load_op=CLEAR` across the zero-gap Tonemap -> `L88` handoff, and the first attributable divergence appears when the active compatible render-pass scope is rebuilt before `L88` first pipeline rebind. Implement the smallest rebuild-focused diagnostic needed to classify what exact active-scope reconstruction recipe input or scope-owned attachment contract causes slot 0 to become `LOAD`. Prefer render-pass / pass-scope / command-buffer ownership evidence over shader-side probes; keep the staged repro model intact; save durable notes/artifact references; update this plan with what actually happened; and close bead `oc-vpw` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/drivers/vulkan/rendering_device_driver_vulkan.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added a minimal debug-only RDG/Vulkan correlation seam on the locked source-built host-Vulkan `projection_only__disabled` repro lane, then reran the exact staged case and saved the artifact package at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/` (`exact_command.txt`, `stdout.log`, `stderr.log`, `exit_status.txt`). The new RDG line proved the Tonemap draw-list render-pass recipe for slot 0 is assembled with `source="non_discardable_default_load_contract"`, `tracker_discardable=false`, no clear/ignore override, and `load_op=LOAD`; the paired Vulkan scope-begin line proved that same `create_serial=13` render pass is already active for `Tonemap (L87) (Draw)` and is then reused unchanged by `Command Graph (L88) (Draw)` with `attachment_load_ops=[0:LOAD]`. That means the first attributable divergence is **not** `L88`’s own first pipeline rebind. It is the earlier RDG Tonemap draw-list recipe branch that treats slot 0 as a non-discardable preserve-content attachment and therefore assigns `RDD::ATTACHMENT_LOAD_OP_LOAD`, while the carried Tonemap pipeline packet still points at the compatible-but-different pipeline-side render-pass recipe whose slot 0 remains `CLEAR`. REF-07 was updated with the exact runtime lines and conclusion.

---

### Task 109: Split the Tonemap RDG active-scope slot-0 `LOAD` decision into `non_discardable` attribution vs default-load policy

**Bead ID:** `oc-nxv`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. The current stopping point is locked: the first attributable source of slot-0 `LOAD` is the Tonemap RDG active-scope recipe branch `source="non_discardable_default_load_contract"` with `tracker_discardable=false`, and `L88` only reuses that already-live compatible scope. Add the smallest honest diagnostic that splits this branch one step deeper: determine whether the surviving seam is best explained by **why slot 0 is classified as non-discardable**, or by **why the default non-discardable branch chooses `LOAD`** for this Tonemap attachment. Prefer render-pass / RDG recipe / command-buffer ownership evidence over widening back into demoted lanes. If `bd` is healthy, create/claim the next bead on start; if the local bead CLI remains wedged, proceed on this exact task and record the bead blockage clearly in your results. Save durable notes/artifact references, update this plan with what actually happened, and close the bead if one exists.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/exact_command.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/stdout.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/stderr.log`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/context.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/exit_status.txt`

**Status:** ✅ Complete

**Results:** The bead create path was noisy because the local `bd` CLI lock handling was unhealthy in the parent session, but the create did in fact land as `oc-nxv` and was later reconciled/closed cleanly. I did **not** burn more time fighting Beads during the implementation slice itself and proceeded on the locked Task 109 scope while recording the transient bead blockage.

Implemented the smallest honest follow-up in `rendering_device_graph.*` plus `rendering_device.cpp`: the Tonemap draw-list attachment debug payload now records **discardable provenance** (`root_texture_create`, `shared_fallback_create`, `slice_tracker_create`, or explicit `texture_set_discardable_*`) and annotates the `non_discardable_default_load_contract` branch itself as `default_non_discardable_policy="always_load_without_extra_per_attachment_split"`. That split is intentionally narrow: it does not reopen broader render-pass, pipeline, or post-rebind lanes; it only distinguishes whether the remaining seam lives in the attachment’s non-discardable attribution or in a narrower Tonemap-local policy branch under default non-discardable handling.

Validation stayed on the same source-built host-Vulkan `projection_only__disabled` lane using artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/` (`exact_command.txt`, `stdout.log`, `stderr.log`, `context.txt`, `exit_status.txt`). The repro still aborts at `submit_serial=9` with `exit_status=134`, and the new evidence resolves the requested split cleanly:

- Tonemap runtime line: `[gdgs-rdg] draw_list_render_pass_create ... label="Tonemap" ... attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,...,discardable_provenance="root_texture_create",discardable_seed=false,non_discardable_basis="tracker_is_non_discardable",default_non_discardable_policy="always_load_without_extra_per_attachment_split"}]`
- Vulkan scope lines still show Tonemap and `L88` sharing the already-live active `create_serial=13` scope with `attachment_load_ops=[0:LOAD]`.

What actually happened vs. the Task 109 question:
- The surviving seam is **best explained by why slot 0 is classified as non-discardable**, not by any narrower Tonemap-specific explanation of why the default non-discardable branch chooses `LOAD`.
- In this locked lane, the Tonemap slot-0 tracker is already a **root texture tracker** with `discardable_seed=false` and `tracker_has_parent=false`; there is no slice/parent/shared-fallback nuance carrying the seam.
- Once the code reaches `non_discardable_default_load_contract`, there is **no additional per-attachment policy split left** here: the default policy is simply unconditional `LOAD`.
- So the next honest seam, if this lane continues, is upstream of the default-load rule: **who/what seeded this Tonemap attachment’s root tracker as non-discardable in the first place?**

---

### Task 110: Classify the Tonemap root-tracker non-discardable seed on slot 0 before the `L88` pre-rebind scope rebuild at failing `submit_serial=9`

**Bead ID:** `oc-7za`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-7za` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: the carried Tonemap packet keeps slot-0 `load_op=CLEAR`, the first attributable divergence appears when the active compatible scope is rebuilt, and the deeper split says that divergence is best explained by Tonemap slot 0 already being classified as `non_discardable`, not by any narrower default-load policy branch. Implement the smallest ownership-focused diagnostic needed to trace **who/what seeds that root tracker as non-discardable** for Tonemap slot 0 (`discardable_provenance="root_texture_create"`, `discardable_seed=false`, `tracker_has_parent=false`) and determine whether the earliest honest seam is the root texture creation path itself, an upstream usage/registration contract, or another pre-RDG seed rule that marks the tracker non-discardable before Tonemap’s active-scope recipe is built. Prefer RDG / texture-tracker / render-pass ownership evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-7za` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added the smallest honest root-seed breadcrumb on the RDG tracker path (`tracker_name`, `discardable_seed_contract`) plus a tiny render-target color naming attempt, rebuilt the source-built editor, and reran the same host-Vulkan `projection_only__disabled` repro twice. Fresh artifact roots: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-seed-vulkan-sourcebuild-20260521-0914/` and `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-seed-vulkan-sourcebuild-20260521-0920/`. Locked result: Tonemap slot 0 still arrives as `tracker_has_parent=false`, `discardable_provenance="root_texture_create"`, `discardable_seed=false`, and now also `discardable_seed_contract="texture_format_is_discardable_flag"`. The tracked attachment remains unnamed at runtime (`tracker_name="RID:8783208120351"`), but its `texture_usage=0x8b` exactly matches `TextureStorage::render_target_get_color_usage_bits(false)` (`sampling|color_attachment|storage|can_copy_from`), which is the render-target color contract used before Tonemap’s active-scope recipe is rebuilt. No later `texture_set_discardable` override appears on this lane. Conclusion: the earliest honest seed is not another pre-RDG rule; it is the root texture creation path applying an upstream render-target usage/creation contract that leaves `TextureFormat.is_discardable=false` by default, so the tracker is already non-discardable before Tonemap builds its active compatible scope. `bd` worked for claim and closure.

---

### Task 111: Classify the upstream render-target color contract that leaves Tonemap slot-0 texture non-discardable before the active-scope rebuild at failing `submit_serial=9`

**Bead ID:** `oc-h3g`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-h3g` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: Tonemap slot 0 becomes non-discardable because root texture creation materializes an upstream render-target color usage/creation contract with `TextureFormat.is_discardable=false`; this is earlier than Tonemap-local logic and earlier than any extra pre-RDG seed rule. Implement the smallest honest diagnostic needed to classify **which upstream render-target color contract choice** leaves `is_discardable=false` here — for example whether it is inherent to `render_target_get_color_usage_bits(false)`, a texture-format creation default, a usage-flag family decision, or another exact render-target creation rule on this lane. Prefer texture-storage / render-target creation / RDG ownership evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-h3g` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added the smallest honest upstream-contract diagnostic in `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp` and reran the same source-built host-Vulkan `projection_only__disabled` repro on the failing `submit_serial=9` lane. Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/`. The new runtime line proved that `TextureStorage::_update_render_target()` prepares the non-MSAA render-target color attachment with `usage_bits=0x8b` and `is_discardable=false` under `discardable_contract="texture_format_default_false_unset"`, i.e. this path leaves `RD::TextureFormat::is_discardable` at the struct default rather than explicitly setting it. The same run still reports the Tonemap slot-0 tracker as `discardable_provenance="root_texture_create"` with `discardable_seed_contract="texture_format_is_discardable_flag"`, which locks the exact classification: the upstream contract is the non-MSAA render-target color creation rule leaving `TextureFormat::is_discardable` untouched/default-false; `render_target_get_color_usage_bits(false)` explains the matching `0x8b` usage family but is not itself the non-discardable seed rule. Updated `doc/gdgs-compositor-staged-qa-2026-05-17.md` with the exact evidence and conclusion. Bead outcome: `bd update`/`bd close` worked.

---

### Task 112: Classify whether the non-MSAA render-target `is_discardable=false` contract is intentional or the surviving Tonemap bug seam at failing `submit_serial=9`

**Bead ID:** `oc-wvi`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-wvi` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: `TextureStorage::_update_render_target()` builds the non-MSAA render-target color `RD::TextureFormat` with `usage_bits=0x8b` and leaves `is_discardable` unset, so it stays at the struct default `false`; root texture creation copies that into the Tonemap slot-0 tracker, and Tonemap later reaches the non-discardable `LOAD` branch. The MSAA sibling path in the same function explicitly sets `rd_color_multisample_format.is_discardable = true`. Implement the smallest honest diagnostic needed to classify whether the non-MSAA `is_discardable=false` contract is an intentional render-target rule on this lane or the surviving bug seam for Tonemap’s render target — for example by tracing the creation-policy rationale, explicit branch ownership, or exact conditions that distinguish the MSAA and non-MSAA paths here. Prefer texture-storage / render-target creation / ownership evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-wvi` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added one narrow debug-only ownership breadcrumb in `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`, rebuilt the source-built editor, and reran the same host-Vulkan `projection_only__disabled` repro lane into `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-policy-vulkan-sourcebuild-20260521-0956/`. The new runtime line proves that the non-MSAA render-target color on this lane is the persistent sampled/shared root texture (`owner=persistent_root_color`, `policy_class=sampled_shared_root`, `shared_to_render_target_texture=true`, `shared_to_srgb_texture=true`) while the Tonemap render-pass record still inherits `discardable_seed=false` from `root_texture_create` and lands in `source="non_discardable_default_load_contract"` before the same `fence_wait_error submit_serial=9 wait_result=-4`. Cross-checking the owning code path keeps the branch split exact: when `msaa == disabled`, only `rt->color` exists and stays as the persistent root texture with default `is_discardable=false`; when `msaa != disabled`, `rt->color` remains the persistent resolve destination/root texture while `rt->color_multisample` is the explicit transient MSAA intermediate and is the branch that gets `is_discardable=true` (also documented by the in-code comment `Render into our MSAA buffer and resolve into our color buffer.`). Conclusion: on this locked failing lane, non-MSAA `is_discardable=false` is an intentional render-target ownership contract, not a newly isolated Tonemap-local bug seam by itself. `bd update oc-wvi --status in_progress --json` worked at start; closure result recorded separately when the evidence package finished. No commit was made because this repo already carries active investigation-only instrumentation and the task was to classify the contract/policy distinction, not land a durable upstream change.

---

### Task 113: Classify whether Tonemap is on the wrong persistent-root render-target lane or the persistent-root policy is wrong for this usage at failing `submit_serial=9`

**Bead ID:** `oc-pnc`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-pnc` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: the non-MSAA render-target `is_discardable=false` path is intentional persistent-root ownership policy, not a newly isolated Tonemap-local bug by itself. In this lane, `rt->color` is the persistent sampled/shared root texture; when MSAA is enabled, the transient/discardable branch is the multisample intermediate instead. Implement the smallest honest diagnostic needed to classify the next exact fork: **is Tonemap bound to the wrong persistent-root render-target lane for this usage, or is the persistent-root policy itself wrong for this specific Tonemap usage?** Prefer texture-storage / render-target ownership / usage-flow evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-pnc` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added the smallest honest Tonemap-lane breadcrumb across `servers/rendering/renderer_rd/storage_rd/texture_storage.{h,cpp}` and `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp`, rebuilt the source-built editor, and reran the same host-Vulkan `projection_only__disabled` repro lane into `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-lane-vulkan-sourcebuild-20260521-100508/`. The new runtime line is decisive on the requested fork: `tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,lane_rationale=tonemap_direct_to_render_target_framebuffer,msaa=0,view_count=1,override_active=false,...}` while the same run still logs the already-locked root ownership line `render_target_color_policy scope=non_msaa_color owner=persistent_root_color policy_class=sampled_shared_root ...` and still dies at the same `fence_wait_error submit_serial=9 wait_result=-4`. That means Tonemap is **not** bound to a wrong side lane, intermediate, override, or MSAA transient lane on this repro; it is writing directly to the intended non-MSAA persistent-root render-target lane (`rt->color`). The surviving fork therefore resolves to the second branch: the persistent-root lane binding itself is correct, but the **persistent-root policy/contract is wrong for this specific Tonemap usage** on the failing lane. The earlier locked pre-rebind classifier still applies in the same artifact: the carried packet keeps slot-0 `CLEAR` while the active rebuilt scope for that same lane still presents slot-0 `LOAD`, so the bug seam remains attached to the lane’s exact attachment/load-op policy rather than Tonemap selecting the wrong destination object. Updated `doc/gdgs-compositor-staged-qa-2026-05-17.md` with the exact artifact path, runtime lines, and conclusion. `bd update oc-pnc --status in_progress --json` worked at start; closure is recorded when the bead close finishes. No commit was made because this remains investigation-only instrumentation on the active debug branch.

---

### Task 114: Classify why Tonemap usage is treated as persistent sampled/shared root instead of discardable/clearable at failing `submit_serial=9`

**Bead ID:** `oc-0dj`
**SubAgent:** `primary` (for `coder`)
**Role:** `coder`
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-0dj` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: Tonemap is on the correct non-MSAA persistent-root render-target lane (`rt->color`, `path_class=persistent_root_direct`), so the surviving fork is the policy itself — why this Tonemap usage is treated as a persistent sampled/shared root that preserves contents instead of a discardable/clearable render-target usage. Implement the smallest honest diagnostic needed to classify that policy cause: whether it is driven by a specific sampled/shared consumer registration, a render-target texture sharing/view rule, a usage-bit family requirement, or another exact ownership rule on this lane. Prefer texture-storage / render-target ownership / usage-flow evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-0dj` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added one narrow debug-only classification line in `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`, rebuilt the source-built editor, and reran the same host-Vulkan `projection_only__disabled` repro lane into `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-policy-cause-vulkan-sourcebuild-20260521-101345/`. The decisive runtime line is: `render_target_policy_cause scope=non_msaa_color owner=persistent_root_color primary_cause=render_target_texture_shared_view_rule specific_sampled_consumer_registration=false render_target_texture_shared_view=true srgb_shared_view=true ... usage_bits=0x8b usage_family={sampling=true,color_attachment=true,copy_from=true,storage=true} discardable_override_on_root=false ... creation_note="shared root texture is created so transparent can be supported"`. Paired with the same run’s existing lane breadcrumb (`tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,...}`) and Tonemap render-pass record (`source="non_discardable_default_load_contract"`, `discardable_provenance="root_texture_create"`, `discardable_seed=false`, `texture_usage=0x8b`), this cleanly classifies the policy cause: **Tonemap is treated as persistent sampled/shared root because `rt->color` is the canonical non-MSAA render-target backing store that is immediately aliased into the render-target texture / sRGB shared views via `texture_create_shared(...)`; the broad `0x8b` usage-bit family supports that reusable root role, but the primary driver is the render-target shared-view ownership rule, not a later specific sampled-consumer registration.** Only the MSAA sibling path gets the explicit discardable override. The run still fails at the same `fence_wait_error submit_serial=9 wait_result=-4`, so the repro lane stayed locked. Updated `doc/gdgs-compositor-staged-qa-2026-05-17.md` with the exact artifact path, runtime lines, and conclusion. `bd update oc-0dj --status in_progress --json` worked at start and `bd close oc-0dj --reason "Classified the Tonemap persistent sampled/shared root policy cause" --json` succeeded at finish. No commit was made because this remains investigation-only instrumentation on the active debug branch.

---

### Task 115: Classify whether transparent-support shared-view ownership is actually required for this Tonemap lane or too broad at failing `submit_serial=9`

**Bead ID:** `oc-83p`  
**SubAgent:** `primary` (for `coder`)  
**Role:** `coder`  
**References:** `REF-05`, `REF-06`, `REF-07`, `REF-08`  
**Prompt:** In `/home/derrick/.openclaw/workspace/projects/godot/`, claim bead `oc-83p` on start and stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`. Do not widen into already-demoted lanes unless the evidence forces it. The current truth is now locked: Tonemap’s correct lane is treated as persistent sampled/shared root primarily because the non-MSAA render-target backing store `rt->color` is immediately aliased into shared render-target texture views (`texture_create_shared(...)`) so transparent support can be provided. Implement the smallest honest diagnostic needed to classify the next fork: **is that transparent-support/shared-view ownership actually required for this Tonemap lane on the failing repro, or is the shared-root ownership rule too broad here?** Prefer texture-storage / render-target ownership / usage-flow evidence over widening back into demoted packet or post-rebind lanes. Save durable notes/artifact references, update this plan with what actually happened, and close bead `oc-83p` with a clear reason when the evidence package is complete.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`
- `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_rd/renderer_scene_render_rd.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.h`
- `/home/derrick/.openclaw/workspace/projects/godot/servers/rendering/rendering_device_graph.cpp`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md`
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

**Status:** ✅ Complete

**Results:** Added the smallest honest texture-ownership diagnostic in `servers/rendering/renderer_rd/storage_rd/texture_storage.h` and `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`: a debug-only `render_target_get_texture()` request counter on the owning `RenderTarget`, surfaced through the existing `render_target_policy_cause` and `tonemap_render_target_lane` logs. Rebuilt the source editor and reran the locked host-Vulkan `projection_only__disabled` repro at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-transparent-shared-view-necessity-vulkan-sourcebuild-20260521-104055/`. Exact runtime result: `render_target_policy_cause` still reports `primary_cause=render_target_texture_shared_view_rule ... creation_note="shared root texture is created so transparent can be supported"`, but now also shows `transparent_bg=false viewport_texture_requests=0`; the Tonemap lane log confirms `shared_view_requirement={transparent_bg=false,viewport_texture_requests=0,requirement_class=eager_rule_without_observed_consumer}` before the unchanged `fence_wait_error submit_serial=9 wait_result=-4`. Conclusion: for this failing Tonemap lane, the eager transparent-support/shared-view ownership rule is broader than the observed usage. It still explains why `rt->color` stays persistent/non-discardable, but this repro does not show an actual transparent-background need or any observed render-target texture request before failure. Updated `doc/gdgs-compositor-staged-qa-2026-05-17.md` with the new artifact root and classification. `bd update oc-83p --status in_progress` worked; close-out status recorded separately at task completion.

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

1. **Stay on the same source-built Godot binary and keep the repro at `projection_only + disabled`**
   - do not reopen scratch-only, projection-first, semaphore identity, Tonemap-local bind/setup, wrapper-chain, or post-rebind lanes unless new evidence forces it
   - keep the staging narrow so every artifact remains directly comparable on the same failing lane: `submit_serial=9` -> `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`

2. **Start from the latest stopping-point artifact, not the older depth-prepass seam**
   - freshest high-signal artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-attachment-exact-vulkan-sourcebuild-20260520-182834/`
   - current locked seam summary:
     - Tonemap -> `L88` remains the first honest poisoned-boundary compare
     - the boundary is still zero-gap
     - the carried Tonemap packet remains exact on pipeline identity, layout/descriptor provenance, and push-constant-range contract before `L88` first pipeline rebind
     - the remaining carried render-pass attachment exact-recipe contract has narrowed to a single differing field: `load_op`

3. **Execute the exact next bead implied by Task 103 / Task 104**
   - next bead title: `Classify the smallest carried render-pass attachment exact-recipe contract before L88 first pipeline rebind at failing submit_serial=9`
   - focus only on the attachment exact-recipe subfields still attached to the carried Tonemap packet versus the rebuilt active pre-rebind `L88` scope
   - the current evidence already says:
     - `field_classifier="load_op_is_minimum_attachment_exact_hazard"`
     - `minimum_distinguishing_field="load_op"`
     - `mismatch_count=1`
   - so the next honest question is whether the carried `load_op` contract can itself be structurally explained or narrowed further without reopening broader demoted families

4. **Do not backslide into already-exhausted families unless the new artifact directly contradicts the current read**
   - already exact/exhausted on this lane:
     - pipeline identity
     - pipeline-layout / descriptor-set-layout provenance
     - push-constant-range contract
     - dependency lineage
     - view-density lineage
     - broader render-pass compatibility lineage
   - already demoted:
     - Tonemap-local bind/setup lanes
     - hidden wrapper/descendant ownership
     - broader pass/framebuffer ownership
     - post-rebind and downstream-only lanes

### Landing-the-plane handoff

**Stopping point:** The live failing seam is now pinned one step deeper: Tonemap slot 0 reaches the RDG recipe as a **root tracker seeded `is_discardable=false`**, and the graph’s `non_discardable_default_load_contract` then applies an unconditional `LOAD`, while the carried Tonemap pipeline packet still keeps slot 0 at `CLEAR` across the zero-gap handoff into `L88`.

**Best current one-line read:** the carried packet is still not the first source of the `LOAD` divergence; the earliest concrete cause is Tonemap’s root non-discardable attachment attribution feeding the unconditional RDG default-`LOAD` branch, and `L88` just reuses that already-live compatible scope.

**Best artifact to resume from next session:** `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/`

**Best next move:** stay on the same locked repro lane and walk one step upstream of the default-load rule: determine **who/what seeds the Tonemap slot-0 root tracker as non-discardable** on this path. Do not reopen the already-resolved question of why default non-discardable chooses `LOAD`; that branch is now evidenced as unconditional in this lane.

---

*Updated on 2026-05-21 (partial; stopping point advanced from the Tonemap RDG non-discardable default-load contract as the first attributable source of slot-0 `LOAD` to the narrower fact that the remaining seam lives in the root tracker’s non-discardable attribution, while the default non-discardable policy itself is unconditional `LOAD` on this lane)*
