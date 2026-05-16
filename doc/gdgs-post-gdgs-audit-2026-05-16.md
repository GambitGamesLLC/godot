# GDGS post-push-constant audit in Godot source

Date: 2026-05-16
Repo: `/home/derrick/.openclaw/workspace/projects/godot`

## Scope

This is an audit/ranking pass after the confirmed GDGS radix push-constant misuse was removed. It is **not** an engine patch. The goal is to rank the remaining plausible Godot-side or Godot-facing failure surfaces that could still explain the surviving repro shape:

- valid compositor textures exist
- `No Present` still crashes
- the process still ends at `Last known breadcrumb: BLIT_PASS`
- Vulkan device loss still occurs on Godot 4.7-dev5 after the push-constant fix

Primary evidence inputs:

- cleaned QA artifact: `/home/derrick/.openclaw/workspace/.temp/gdgs-push-constant-fix-qa-2026-05-16-dev5/summary.md`
- exact `no_present` crash log: `/home/derrick/.openclaw/workspace/.temp/gdgs-push-constant-fix-qa-2026-05-16-dev5/logs/no_present.log`
- Godot-side source anchors in this checkout

## Cleaned repro facts that matter most

1. The push-constant contract bug was real, and it is now gone from the dev5 repro.
2. The crash matrix did **not** change after that fix.
3. `No Present` still reaches:
   - compositor callback entry
   - manager lookup
   - local RD raster work
   - valid GDGS color/depth compositor textures
4. `No Present` then explicitly skips GDGS writeback/presentation work and still dies later with:
   - `Condition "err != VK_SUCCESS" is true. Returning: FAILED` in Vulkan fence wait
   - `Last known breadcrumb: BLIT_PASS`
   - `Vulkan device was lost.`

That sharply demotes any theory that requires the GDGS final composite writeback path, overlay presentation path, or script-side presentation queue to execute.

## Source anchors checked

### 1) Compositor callback insertion in Godot

`servers/rendering/renderer_rd/renderer_scene_render_rd.cpp:298-317`

Godot calls compositor callbacks directly via `_process_compositor_effects(...)` by passing `RenderDataRD` to the registered effect callback. There is no extra guard here beyond compositor/effect existence.

### 2) Forward+ frame order around the callback

`servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:2102-2395`

The callback sits inside the normal Forward+ frame flow. The relevant post-opaque / post-sky / pre-transparent / post-transparent order matters because the surviving crash only shows up later, when the frame moves on toward the final screen path.

### 3) `BLIT_PASS` really is the swapchain/screen blit boundary

`servers/rendering/rendering_device.cpp:5508-5528`

`BLIT_PASS` is tagged when Godot begins the screen/swapchain draw list. That makes it a strong **failure boundary**, but not proof that the bug originates in the blit pass itself. A prior GPU fault can easily surface here.

### 4) Global RD validation gaps relevant to this repro

`servers/rendering/rendering_device.cpp:6763-6818`

Godot validates push-constant size and dispatch counts, which is why the earlier GDGS misuse became visible in 4.7-dev5. But those checks do **not** validate shader memory safety, indirect-dispatch contents, or whether a compute workload is semantically safe for the driver/device.

### 5) Local RenderingDevice contract note

`doc/classes/RenderingServer.xml:1132-1136`

Godot explicitly documents `create_local_rendering_device()` as a separate-thread device that cannot draw to the screen **nor share data with the global RenderingDevice**.

That matters a lot for non-`No Present` modes, because GDGS creates its raster textures on a local RD and later binds those RIDs in a global-RD compositor pass.

## Ranked remaining candidate surfaces

## 1) Local-RD-in-compositor boundary: GDGS local compute workload + Godot local-device submission/synchronization

**Rank:** 1
**Ownership read:** mixed, leaning boundary/misuse rather than clean engine bug

This is the highest-signal remaining surface because it is the last shared step across all crashing modes, including `No Present`.

What survives in `No Present`:

- GDGS enters the compositor callback
- GDGS calls into `GaussianRenderManager.render_for_compositor(...)`
- GDGS executes multiple local `RenderingDevice` compute lists
- valid local RD textures are produced
- GDGS returns without doing its final composite writeback
- the process still later dies at fence wait / `BLIT_PASS`

That means the root cause can still live in either:

- the local RD workload itself, or
- Godot's handling of local-device execution/synchronization while the main render frame continues toward present

This surface is stronger than any pure “final composite shader is wrong” theory, because `No Present` removes that path.

## 2) GDGS local compute shader/resource misuse that still causes a delayed GPU fault

**Rank:** 2
**Ownership read:** mostly GDGS-side until disproven

The push-constant fix removes one real misuse, but it does **not** prove the remaining local compute passes are safe.

Important nuance: “valid compositor textures exist” only proves the workload got far enough to write plausible outputs. It does **not** prove the workload avoided:

- out-of-bounds SSBO access
- bad indirect-dispatch dimensions
- image writes outside intended bounds
- format/layout assumptions that happen to run long enough to produce output before the device faults

The surviving `No Present` crash is fully compatible with “a GDGS local RD shader/resource bug poisons the device, and the fault is only observed later when Godot waits/presents.”

This is now a more credible remaining explanation than the old push-constant bug, because the current evidence no longer has a simple validation-layer explanation.

## 3) Godot backend synchronization around compositor callback return and later frame submission

**Rank:** 3
**Ownership read:** engine/backend-side candidate

The main engine-side candidate that still fits the evidence is a synchronization/queue-ordering problem after the callback returns.

Why it still fits:

- `_process_compositor_effects(...)` simply calls the effect callback inline.
- the frame then continues through later passes and eventually into the screen blit path.
- the observed failure is a Vulkan fence wait failure followed by device loss at a later boundary.

So a bug in how Godot sequences or fences local-device work relative to the global frame could still explain why the crash appears only when the frame reaches `BLIT_PASS`.

This is especially worth keeping alive because the repro is on Intel Iris Xe + Linux + Vulkan, which is a combination where backend/driver synchronization bugs are plausible.

Still, the evidence does **not** yet justify claiming a pure Godot bug. The engine-side case gets much stronger only if a trivial or sanitized local RD workload still reproduces the fault.

## 4) Unsupported local-RD-to-global-RD texture sharing in the non-`No Present` compositor paths

**Rank:** 4
**Ownership read:** unsupported GDGS contract pattern

In the non-`No Present` path, GDGS creates textures on a local RD (`RenderingServer.create_local_rendering_device()`) and then binds those texture RIDs into a compositor pass using the global RD.

Godot's docs explicitly say local RDs cannot share data with the global RD. So this remains a real unsupported pattern.

However, `No Present` sharply demotes this as the explanation for the **surviving baseline crash**, because `No Present` exits before the global compositor uniform set is built and before those local RD texture RIDs are consumed by the global RD.

So this is still a bug-shaped GDGS contract problem, but it is **not sufficient** to explain the current `No Present` crash by itself.

## 5) Post-transparent / screen-texture / scene-depth compositor-state handling in Godot

**Rank:** 5
**Ownership read:** lower-confidence engine-side candidate

This family includes:

- post-transparent callback timing
- screen/depth texture copies
- resource-state transitions around color/depth access
- attachment vs storage usage handling

These were stronger suspects before the `No Present` reduction. After the reduction, they drop because `No Present` bypasses the final GDGS composite dispatch and therefore bypasses most of the scene-texture/depth-texture mixing path that originally looked suspicious.

These are still worth instrumenting later if the local-RD boundary is cleared, but they are no longer the first bet.

## What still looks unsupported/misused by GDGS

1. **Local RD textures are treated as if they can be consumed by the global RD** in non-`No Present` modes.
   - Godot docs say that sharing is not supported.
   - This remains a genuine GDGS-side contract problem, even if it is not the whole `No Present` story.

2. **GDGS relies on heavy local RD compute from inside a compositor callback running during the frame.**
   - Godot docs describe local RDs as separate-thread devices.
   - That does not automatically make current usage illegal, but it is far enough off the beaten path that it remains a mixed boundary risk.

3. **The remaining GDGS compute workloads are not yet proven memory-safe.**
   - The old push-constant violation is fixed.
   - The rest of the shader/buffer/image contract is still largely unvalidated by Godot.

## What now looks more likely engine/backend-side

1. **If** a trivial local RD workload inside the same callback still reproduces the device loss, then the strongest remaining explanation becomes a Godot backend synchronization or local-device integration bug.
2. The `BLIT_PASS` breadcrumb and fence-wait failure still support “the GPU fault is only observed when the frame reaches the later screen/present boundary,” which is compatible with an engine/backend synchronization issue.
3. The inline callback shape in `_process_compositor_effects(...)` leaves very little explicit isolation or diagnostic context, so Godot-side instrumentation there is the right first engine-side move.

## Recommended first instrumentation / experiment targets

### A. Highest-signal experiment: trivialize the local RD workload while keeping the compositor callback alive

Goal: determine whether **any** local RD activity inside the callback is sufficient to trigger the later device loss, or whether the fault requires the full GDGS workload.

Recommended variants:

1. callback entered, but skip `render_for_compositor(...)` entirely
2. callback entered, create/use local RD but do no dispatch
3. run one trivial local RD compute dispatch that only writes to a tiny scratch buffer/texture
4. progressively re-enable GDGS passes (`projection` -> `radix` -> `boundaries` -> `render`)

Interpretation:

- if variant 1 or 2 is stable, the bug needs local RD work
- if variant 3 is stable but a later GDGS pass breaks, the likely owner shifts toward GDGS shader/resource misuse
- if even variant 3 dies, the engine/backend boundary becomes the top suspect

### B. Add Godot-side breadcrumbs around compositor callback entry/exit and around local-device submit/wait points

Targets:

- `RendererSceneRenderRD::_process_compositor_effects(...)`
- local `RenderingDevice` submit/flush/fence paths
- the swapchain `BLIT_PASS` path

Goal: tighten the gap between “callback ran” and “device loss surfaced later.”

Also rerun with `--accurate-breadcrumbs` so the breadcrumb chain is less lossy than the current reverse dump.

### C. Add temporary validation/assertion for local/global RD resource provenance

Even though this is not the `No Present` root by itself, it is worth instrumenting because it is already a documented unsupported pattern.

Desired outcome:

- loudly detect when a RID created on a local RD is bound through the global RD path
- turn the current silent-UB shape into an early actionable error

### D. Inspect/guard GDGS indirect-dispatch and buffer extents pass by pass

First checks:

- max dispatch counts per radix/boundary/render pass
- SSBO sizes versus per-pass indexing math
- image write bounds versus `texture_size`
- any pass that can write based on point count or tile bounds without a hard cap

### E. Try the same minimized local-RD callback on another Vulkan stack if available

This is not required to rank the source surfaces, but it helps sort “engine contract bug” from “Intel Vulkan driver sensitivity to borderline usage.”

## Current verdict

The remaining culprit no longer looks cleanly “Godot-side.”

Best current read:

- **overall ownership:** mixed
- **lean:** slightly toward GDGS-side remaining compute/resource misuse or unsupported boundary usage
- **top engine-side candidate:** local-RD/compositor-callback synchronization or backend integration issue that only surfaces later at `BLIT_PASS`

So the next move should **not** be blind engine patching. The next move should be targeted instrumentation and workload-minimization around the local RD boundary.

## Bottom line

After the push-constant fix, the surviving repro says:

- the final GDGS composite writeback path is not required to crash
- the unsupported local-to-global texture-sharing contract is real, but it does not fully explain `No Present`
- the highest-value remaining boundary is now: **GDGS local RD work executed from a compositor callback, and how Godot/backend handles that work before the frame reaches `BLIT_PASS`**
