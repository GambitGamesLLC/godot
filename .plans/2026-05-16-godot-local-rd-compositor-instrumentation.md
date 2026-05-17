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

### Task 3: Audit whether the instrumentation package is the right first experiment

**Bead ID:** `oc-76q`  
**SubAgent:** `primary` (for `auditor`)  
**Role:** `auditor`  
**References:** `REF-01`, `REF-02`, `REF-03`, `REF-04`, `REF-05`  
**Prompt:** Independently audit the instrumentation plan/package. Confirm that it is the highest-signal next move and that it distinguishes plugin misuse from engine/backend failure better than alternative next steps.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
- notes/docs as needed

**Status:** ⏳ Pending

**Results:** Pending.

---

## Final Results

**Status:** ⚠️ Partial

**What We Built:** We finished the mapping/strategy half of the Godot instrumentation lane and clarified the surviving repro seam for the next session. The durable instrumentation map now identifies the exact Godot and GDGS files to instrument, the staged callback/compute isolation order, and the key assertions/breadcrumbs needed to separate plugin misuse from engine/backend failure. The critical correction from this pass is that the current surviving repro is not actually running on a local RenderingDevice in the hot path; it is using the global RenderingDevice in both the raster and compositor path, so the next coder pass should instrument the compositor callback plus staged global-RD compute path first.

**Reference Check:** `REF-01`, `REF-02`, `REF-04`, `REF-05`, and `REF-06` are now the key fresh-session starting points. `REF-06` is the most important handoff artifact.

**Commits:**
- `2e9a557` - `docs: map GDGS compositor instrumentation seams`

**Lessons Learned:**
- After removing one real plugin misuse, the next best step is diagnostic isolation, not another guess.
- The surviving seam changed meaning after source inspection: the immediate experiment should target global-RD-in-compositor behavior, not assume a local-RD sync bug.

## Fresh Session Start / Next Steps

Start the next session from this plan and `REF-06`, then execute in this order:

1. **Task 2 (`oc-jev`) — prepare the local instrumentation branch/package**
   - create a dedicated instrumentation branch in `/home/derrick/.openclaw/workspace/projects/godot/`
   - add callback entry/exit breadcrumbs in `RendererSceneRenderRD::_process_compositor_effects(...)`
   - add a small debug stage gate in `gaussian_compositor_effect.gd`
   - add per-pass stage gates/logs in `gaussian_renderer.gd`
   - add dispatch-name / push-constant-size / group-count logging in `gaussian_rendering_device_context.gd`
   - add seam-correction logging proving the current repro is global/global, not local/global
2. **Run the staged isolation experiments in the documented order**
   - callback only, skip `render_for_compositor()`
   - no dispatch
   - trivial dispatch
   - projection only
   - radix only
   - boundaries only
   - render pass last
   - compositor writeback/presentation only after raster stages are stable
3. **Use `--accurate-breadcrumbs` on at least one rerun per meaningful stage change**
4. **Only after stage-gating results are in, run Task 3 (`oc-76q`)**
   - audit whether the instrumentation package actually gives the highest-signal answer and whether the surviving culprit now looks plugin-side, unsupported-boundary-side, or engine/backend-side

Avoid reopening already-closed branches unless the new stage-gating evidence directly points back to them:
- the radix push-constant contract bug is fixed and validated as removed
- the earlier indirect-dispatch hypothesis no longer leads the suspect list
- final compositor writeback/presentation is demoted because `No Present` still crashes

---

*Completed on 2026-05-16 (partial; ready for fresh-session continuation)*
