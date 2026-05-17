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

**Results:** Added a dedicated trivial compute control named `scratch_only` without replacing the staged repro model. The new stage runs a one-workgroup scratch-buffer pipeline (`gsplat_scratch_probe`) before any real GDGS projection/radix/boundary/render work, then immediately CPU-readbacks the 16-byte probe buffer so QA can distinguish “any real compositor-path compute dispatch is hazardous” from “projection-specific work is the first bad stage.” Projection diagnostics were also deepened in the live projection path: CPU-side assertions now enforce positive output sizing, tile-grid derivation, point-count/state-capacity consistency, boundary-capacity assumptions, and the exact observed projection push-constant contract (`128` bytes / `32` floats / `mat4 view + mat4 projection`). Immediate post-dispatch evidence now logs a blocking readback of the projection histogram header (`sort_buffer_size`) plus RID-validity for the key projection outputs right after `compute_list_end()`, giving QA/audit a tighter handhold on whether the first dispatch completed and whether the written counts stayed within the allocated sort capacity. Validation run stayed repo-local and reversible: `git diff --check`, `python3 /home/derrick/.openclaw/workspace/projects/godot/misc/scripts/file_format.py` on the touched GDGS scripts/shader, and `godot --headless --path . --script ... --check-only --quit` for the touched GDScript entrypoints plus a headless project load. No speculative fix was attempted; this pass is strictly instrumentation.

---

## Final Results

**Status:** ⚠️ Partial

**What We Built:** The instrumentation lane is now fully documented and independently audited. The package established the right staged repro controls on the GDGS side, corrected the seam description from the earlier local/global theory to the current global/global reality, and narrowed the first failing staged workload boundary to `projection_only`. The audit also captured the main remaining limitation: the new Godot-source callback breadcrumbs were prepared in the instrumentation branch, but the staged QA evidence package came from the managed runtime rather than a source-built engine binary, so the current evidence narrows the first failing GDGS stage more strongly than it separates plugin misuse from engine/backend failure.

**Reference Check:** `REF-06` documents the intended staged order and instrumentation seams, `REF-07` contains the executed QA evidence package, and `REF-08` records the independent auditor verdict and caveats. The projection-first conclusion is accepted only in that narrower audited sense.

**Commits:**
- `2e9a557` - `docs: map GDGS compositor instrumentation seams`
- `c111442` - `debug: add compositor callback breadcrumbs`
- `3b2ce44` - `debug: add GDGS compositor instrumentation gates`

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
