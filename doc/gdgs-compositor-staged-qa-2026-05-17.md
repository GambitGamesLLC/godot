# GDGS compositor staged QA — 2026-05-17

## Scope

QA pass for bead `oc-hf4` against:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

The goal was to run the staged compositor isolation order and identify the first stage that reproduces the Vulkan device-loss crash.

## Branch / worktree state used

- Godot repo working tree: `/home/derrick/.openclaw/workspace/projects/godot`
- GDGS repo working tree: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- No extra worktree was needed; both repos were already on the instrumentation branches, so QA did not disturb unrelated work.

## Runner used

Temporary QA harness artifacts were staged under:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/`

Key run directories:

- normal run: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-20260517-090004/`
- accurate-breadcrumb rerun: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-accurate-20260517-090036/`

Note: the staged QA harness exercised the GDGS instrumentation package successfully using the installed Godot binary at `/home/derrick/.local/share/openclaw/godot/current/godot`. A source build of the Godot instrumentation branch was started in parallel, but the first failing boundary was already isolated before that build completed, so the evidence package below comes from the staged GDGS branch plus the managed Godot runtime.

## Stage order requested vs. executed

Requested order:

1. callback only
2. no dispatch
3. trivial dispatch if available
4. projection only
5. radix only
6. boundaries only
7. render last
8. compositor writeback / presentation only if earlier stages stabilize

Executed:

1. `callback_only` — completed, stable
2. `prepared_no_dispatch` — completed, stable
3. trivial dispatch — **not available in the package**; no dedicated scratch/known-safe dispatch stage was exposed by the branch, so QA noted the gap and continued per instructions
4. `projection_only` — **first failing stage**, reproduced crash in both normal and `--accurate-breadcrumbs` reruns
5. `radix_only` — not run, because projection was already the first failing boundary
6. `boundaries_only` — not run, because projection was already the first failing boundary
7. `render_only` — not run, because projection was already the first failing boundary
8. full compositor writeback / presentation — not run, because projection was already the first failing boundary

## Findings

### Stable boundary before failure

`callback_only` and `prepared_no_dispatch` both survived repeatedly.

Evidence:

- `callback_only` summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-20260517-090004/run_summary.tsv`
- `prepared_no_dispatch` log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-20260517-090004/logs/prepared_no_dispatch.normal.log`
- accurate rerun summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-accurate-20260517-090036/run_summary.tsv`

Notable seam evidence from the stable runs:

- compositor callback enters cleanly and can be repeatedly short-circuited at `callback_only`
- the current repro path still reports the corrected seam as global/global:
  - `compositor_path_uses_global_rd=true`
  - `raster_path_expected_global_rd=true`
  - `raster_path_uses_global_rd=true`
  - `local_device_submit_sync_exercised=false`
- `prepared_no_dispatch` reaches `renderer stage=prepared_no_dispatch_gate` and returns compositor textures without device loss

### First failing stage

`projection_only` is the first stage that reproduces the crash.

The crash occurs after the first meaningful compute dispatch is enabled, before radix, boundaries, render, or compositor writeback/presentation are needed.

Normal-run evidence:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-20260517-090004/logs/projection_only.normal.log`

Accurate-breadcrumb rerun evidence:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-accurate-20260517-090036/logs/projection_only.accurate.log`

Relevant chain from the accurate rerun:

1. `renderer stage=projection_begin`
2. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
3. `rd barrier complete pipeline=gsplat_projection`
4. `renderer stage=projection_end`
5. `renderer stage=projection_only_gate`
6. compositor returns through `raster_only_no_writeback_gate`
7. later `fence_wait` fails with `VK_SUCCESS` check failure
8. lost-device breadcrumbs still collapse to `BLIT_PASS`

That means the first failing boundary is now much tighter than before: the crash no longer requires radix sort, tile boundaries, the final render pass, or compositor writeback/present. Projection dispatch alone is enough.

## Interpretation

This QA pass materially reduces the suspect set.

Still supported by evidence:

- projection compute dispatch or data written by it
- synchronization / lifetime issue that becomes visible immediately after projection work is submitted
- engine/backend breadcrumb coverage still being too coarse once the device is already lost

Made less likely by this pass:

- pure compositor callback entry/exit handling
- compositor writeback / presentation path
- local RenderingDevice submit/sync seam mismatch in the current repro path
- radix / boundary / render passes as the *first* trigger

## Recommended next suspects

1. projection output buffer / image writes and bounds assumptions
2. projection pipeline layout / push constant contract (`128` bytes in the observed dispatch)
3. hazards around resources consumed after `projection_only_gate`, even though later GDGS passes are skipped
4. why device-loss breadcrumbs still flatten to `BLIT_PASS` after the fence failure, despite the tighter stage isolation

## Follow-up QA pass for bead `oc-sib` — scratch control + tighter projection diagnostics

### Scope

Rerun the staged compositor repro after bead `oc-dew` added:

- `scratch_only` trivial dispatch control
- projection precondition assertions
- immediate post-projection readback / RID-validity logging

### Runtime note / drift encountered

The current managed `godot` on this machine has drifted to:

- `/home/derrick/.local/bin/godot` → `Godot 4.6.2.stable.official.71f334935`

That runtime is not suitable for this rerun because the updated GDGS branch now tries to load the new scratch probe shader during GPU-state rebuild and immediately logs:

- `No loader found for resource: res://addons/gdgs/runtime/render/shaders/compute/gsplat_scratch_probe.glsl`

To avoid reporting a false stage regression from the wrong runtime, QA used the preserved repro binary instead:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`

### Artifact roots

- normal run: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-dev5-20260517-124210/`
- accurate rerun: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-dev5-accurate-20260517-124210/`

### Exact stages run

1. `callback_only` — exit `0`
2. `prepared_no_dispatch` — exit `0`
3. `scratch_only` — exit `0`
4. `projection_only` — exit `134` / device-loss crash
5. `projection_only` with `--accurate-breadcrumbs` — exit `134` / device-loss crash

### Findings

#### `callback_only` and `prepared_no_dispatch` still survive

The first two pre-dispatch boundaries remain stable on the dev5 runtime. That preserves the earlier narrowing: callback entry and no-dispatch setup are still not the first trigger.

#### `scratch_only` returns cleanly, but the trivial scratch dispatch did **not** actually execute a valid shader pipeline

This matters. The stage exits `0`, but the logs show the new scratch probe shader failed to load during GPU-state rebuild:

- `No loader found for resource: res://addons/gdgs/runtime/render/shaders/compute/gsplat_scratch_probe.glsl`
- `SCRIPT ERROR: Cannot call method 'get_spirv' on a null value.`
- later, during the staged scratch pass:
  - `rd dispatch pipeline=gsplat_scratch_probe push_constant_bytes=0 direct=true group_count=(1, 1, 1)`
  - `ERROR: Parameter "pipeline" is null.`
  - `ERROR: Parameter "uniform_set" is null.`
  - `ERROR: No compute pipeline was set before attempting to draw.`

The immediate scratch readback stays all zeros on repeated callbacks:

- `scratch_words=0x00000000,0x00000000,0x00000000,0x00000000`

So `scratch_only` is only a partial control result right now: it *survives*, but it does **not** yet prove that a known-good compositor-path compute dispatch can execute safely. The current scratch control is effectively a no-op because the shader/pipeline never materializes.

#### `projection_only` still remains the first failing meaningful stage

Despite the scratch-control limitation above, the first meaningful successful compute dispatch is still `projection_only`, and it still fails in both normal and accurate runs.

Key chain from both reruns:

1. `renderer stage=projection_begin`
2. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
3. `rd barrier complete pipeline=gsplat_projection`
4. `renderer stage=projection_end`
5. `fence_wait` fails during the immediate post-dispatch readback path
6. `renderer stage=projection_post_dispatch ... sort_buffer_size=0 sort_capacity=2711230 sort_within_capacity=true culled_buffer_valid=true sort_keys_valid=true sort_values_valid=true histogram_valid=true`
7. compositor returns through `raster_only_no_writeback_gate`
8. later lost-device breadcrumbs still collapse to `BLIT_PASS`

### Updated interpretation

This rerun strengthens the earlier projection-first finding, but with an important QA caveat:

- `projection_only` is still the first **meaningful** failing stage.
- `scratch_only` does not overturn that, but it also does not yet answer the intended “is any tiny compositor-path dispatch hazardous?” question because the scratch shader/pipeline failed to load and the readback stayed zero.

### Next recommendation

Fix or expose the scratch probe resource so the trivial control becomes a *real* dispatch, then rerun only:

1. `scratch_only`
2. `projection_only`
3. `projection_only --accurate-breadcrumbs` if projection still fails first

Until that is repaired, the evidence package supports:

- `callback_only` survives
- `prepared_no_dispatch` survives
- `projection_only` still fails first
- the new projection readback/assertion evidence shows `sort_buffer_size=0` within allocated capacity before the later device-loss path

But it does **not yet** support the stronger claim that a valid scratch dispatch has been demonstrated safe.

## Follow-up QA pass for bead `oc-6li` — scratch positive-control fix verified

### Scope

Rerun the minimum staged compositor isolation after bead `oc-x5q` fixed the scratch positive-control packaging/resource path.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `559b31ec`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `4523691`

### Runtime used

To keep the result directly comparable to the earlier projection-first evidence package, QA used the preserved repro binary again:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`

This was a deliberate minimum-rerun choice, not a new runtime-drift problem. Bead `oc-x5q` had already validated that the scratch probe resource now resolves on both the managed runtime and the preserved dev5 binary; this QA pass only needed the comparable repro binary to answer the scratch-vs-projection question cleanly.

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/`

Key files:

- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/run_summary.tsv`
- scratch log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/logs/scratch_only.normal.log`
- projection log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/logs/projection_only.normal.log`
- accurate projection log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-scratch-fixed-dev5-20260517-151908/logs/projection_only.accurate.log`

### Exact stages run

1. `scratch_only` — exit `0`
2. `projection_only` — exit `134` / device-loss crash
3. `projection_only --accurate-breadcrumbs` — exit `134` / device-loss crash

### Findings

#### `scratch_only` is now a real compute-dispatch positive control

The scratch stage no longer degrades into a null-pipeline/no-op path. The fresh logs show a valid dispatch, barrier, and nonzero CPU readback signature:

- `renderer stage=scratch_dispatch_begin ... scratch_probe_bytes=16 scratch_shader_valid=true scratch_buffer_valid=true`
- `rd dispatch pipeline=gsplat_scratch_probe push_constant_bytes=0 direct=true group_count=(1, 1, 1)`
- `rd barrier complete pipeline=gsplat_scratch_probe`
- `renderer stage=scratch_post_dispatch ... scratch_words=0x47534744,0x00000001,0x00000001,0x5a5aa5a5 scratch_signature_ok=true scratch_probe_valid=true histogram_valid=true`

That confirms the intended positive-control answer: a real compositor-path compute dispatch can execute successfully in this staged harness, and the expected scratch readback is nonzero and stable.

#### `projection_only` still remains the first meaningful failing stage

The projection pass still reproduces the failure in both the normal and accurate reruns.

Shared high-signal chain from both logs:

1. `renderer stage=projection_begin ... push_constant_bytes=128 ... expected_group_count=1060 ... sort_capacity=2711230`
2. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
3. `rd barrier complete pipeline=gsplat_projection`
4. `renderer stage=projection_end ... projection_group_count=1060`
5. immediate post-dispatch evidence still logs `sort_buffer_size=0 sort_capacity=2711230 sort_within_capacity=true culled_buffer_valid=true sort_keys_valid=true sort_values_valid=true histogram_valid=true`
6. the failure still surfaces at `fence_wait`
7. the last known lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This rerun closes the earlier positive-control gap.

The evidence package now supports the stronger staged conclusion:

- `scratch_only` is a valid known-safe compositor-path compute dispatch on this branch state and runtime.
- `projection_only` still remains the first meaningful failing stage.
- therefore the current failure is no longer well-explained by the broad theory that *any* real compute dispatch inside this compositor path is hazardous; the leading suspect list should stay centered on projection-specific work or on synchronization/lifetime fallout triggered specifically after projection.

### Next recommendation

Do not broaden the repro again yet. The next lane should stay projection-focused:

1. projection output writes / bounds assumptions
2. projection pipeline layout / push-constant contract at the observed `128` bytes / `32` floats
3. synchronization or resource-lifetime fallout immediately after projection dispatch / readback
4. only after that, revisit broader engine/backend compositor-path handling if new evidence forces it
