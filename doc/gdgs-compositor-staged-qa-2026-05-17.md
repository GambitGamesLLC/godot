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

## Follow-up QA pass for bead `oc-4wb` — projection probe/readback after deep projection diagnostics

### Scope

Run only the two requested projection-focused repros after bead `oc-wz6` added the new projection probe SSBO plus immediate post-dispatch readback of:

- histogram header / `sort_buffer_size`
- projection probe words
- first words of `sort_keys`, `sort_values`, and `culled_splats`

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `63a549658abba0ede8fc4260c3cf82dd53b46a86`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `ba83c1cbc67065b2be55de8bfaaa624d4afc2283`

### Runtime used

To keep the answer directly comparable to the prior staged evidence and avoid reopening the already-documented managed-runtime drift, QA again used the preserved repro binary:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`

This was a deliberate minimum-rerun choice. The task only needed the focused projection-probe answer on the already-established repro runtime.

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-probe-dev5-20260517-154354/`

Key files:

- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-probe-dev5-20260517-154354/run_summary.tsv`
- normal log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-probe-dev5-20260517-154354/logs/projection_only.normal.log`
- accurate log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-probe-dev5-20260517-154354/logs/projection_only.accurate.log`

### Exact runs performed

1. `projection_only` — exit `134` / abort
2. `projection_only --accurate-breadcrumbs` — exit `134` / abort

### Findings

#### The projection dispatch contract is still stable before readback begins

Both reruns still reach the same pre-readback projection chain successfully:

1. `renderer stage=prepared ... projection_push_constant_bytes=128 ... projection_group_count=1060 ... tile_bounds_capacity=2952 sort_capacity=2711230`
2. `renderer stage=projection_begin ... push_constant_floats=32 ... max_tile_id=2951 splat_stride_bytes=240 culled_stride_bytes=64`
3. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
4. `rd barrier complete pipeline=gsplat_projection`
5. `renderer stage=projection_end ... projection_group_count=1060`

So the updated branch still does **not** show drift in the intended projection launch contract.

#### The new failure point is now tighter: device loss surfaces inside the immediate projection readback path itself

Unlike the earlier QA passes, the new `projection_post_dispatch` diagnostic line never prints at all in either rerun.

Instead, both logs die while `_log_projection_post_dispatch_evidence(...)` is attempting the first CPU readbacks:

- GDScript backtrace hits line `322` (`buffer_get_data(... histogram ...)`)
- then line `325` (`buffer_get_data(... projection_probe ...)`)
- engine reports failure at `fence_wait`
- last known lost-device breadcrumb still collapses to `BLIT_PASS`

That is the main new evidence from this task: once the new blocking projection readbacks are enabled, the repro now consistently detonates **before any projection probe words, sort-buffer header values, or sentinel snapshots can be logged**.

#### What we can and cannot claim from this run

Because `projection_post_dispatch` never emits, this rerun does **not** provide concrete values for:

- `probe_duplicated_splats`
- `probe_emitted_sort_elements`
- `probe_max_sort_end`
- `probe_max_tile_id`
- `first_sort_keys`
- `first_sort_values`
- `first_culled_words`

So QA cannot honestly claim from this bead that duplicated splats were observed, that sort-key writes were observed, that `probe_max_sort_end` stayed within capacity, that `probe_max_tile_id` stayed within tile capacity, or that output-buffer sentinels were overwritten.

The stronger supported statement is narrower:

- the projection dispatch and post-dispatch barrier still complete
- the crash now surfaces immediately when the code tries to perform blocking CPU readback on the projection outputs
- therefore the evidence does **not** currently point to a cleanly observed immediate projection-bounds violation in the logged counters
- instead it points more strongly toward projection-triggered synchronization / lifetime / device-loss fallout that becomes visible at the first readback fence

That does **not** prove the projection shader writes are safe. It means the current diagnostic package fails before it can read back the counters that would prove or disprove that theory.

### Updated interpretation

This pass moves the failure boundary one step tighter than bead `oc-6li`:

- before: projection dispatch completed, then device loss surfaced later at `fence_wait`, with no probe/readback package yet
- now: projection dispatch still completes, but the immediate projection readback itself is the point where `fence_wait` trips before any counters or sentinels can be emitted

So the leading suspect lane remains:

1. projection-specific GPU writes that poison later synchronization/readback
2. projection-triggered resource lifetime or synchronization fallout visible at the first blocking readback

The lane that is **not** supported by this task is “we already saw out-of-capacity counters or overwritten sentinels proving an immediate projection bounds violation.” The counters never made it back to the CPU.

### Next recommendation

Keep the next step narrowly diagnostic and projection-focused. The most useful next move is to split the immediate readback package into smaller checkpoints so we can discover the first individual readback that trips the fence:

1. histogram header only
2. projection probe only
3. `sort_keys` sentinel snapshot only
4. `sort_values` sentinel snapshot only
5. `culled_splats` sentinel snapshot only

That should answer whether the device is already lost before any readback, whether a specific buffer readback is the first detonator, and whether any part of the projection output package can still be observed before the fence failure.

## Follow-up QA pass for bead `oc-15g` — projection readback checkpoints isolated

### Scope

Run the new minimal projection readback checkpoint sequence added by bead `oc-bjw`, in the requested order, and stop as soon as the first failing checkpoint was identified. If the first failure still needed breadcrumb confirmation, rerun only that checkpoint with `--accurate-breadcrumbs`.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `47c14d70f1e32f4aa02f820f6024df84bc4ba96e`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `9d7f436dbacb9724b6152a337d9a90089fc6e3af`

### Runtime used

To keep this directly comparable to the earlier staged QA evidence and avoid reopening the already-documented managed-runtime drift, QA again used the preserved repro binary:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`

Because the existing stage harness did not yet expose `debug_projection_readback_checkpoint`, QA used a temporary harness wrapper script at:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`

This wrapper only threads the new debug enum into the already-established stage harness and does not modify repo code.

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/`

Key files:

- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/run_summary.tsv`
- normal log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/logs/projection_only__disabled.normal.log`
- accurate log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/logs/projection_only__disabled.accurate.log`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-readback-checkpoint-dev5-20260517-172304/context.txt`

### Exact runs performed

1. `projection_only + disabled` — exit `134`
2. `projection_only + disabled + --accurate-breadcrumbs` — exit `134`

Per the task constraint to prefer the minimum runs needed, QA stopped there. The first checkpoint in the ordered sequence already failed, so `histogram_header_only`, `projection_probe_only`, `sort_keys_sentinel_only`, `sort_values_sentinel_only`, and `culled_splats_sentinel_only` were not run.

### Findings

#### `projection_only` still crashes with readbacks disabled

This is the key result from bead `oc-15g`.

The logs show the projection dispatch and post-dispatch checkpoint-disable path both completing cleanly before the later failure:

1. `renderer stage=projection_begin`
2. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
3. `rd barrier complete pipeline=gsplat_projection`
4. `renderer stage=projection_end`
5. `renderer stage=projection_post_dispatch_checkpoint_begin ... checkpoint=disabled`
6. `renderer stage=projection_post_dispatch_checkpoint_disabled ... checkpoint=disabled`
7. `renderer stage=projection_post_dispatch_checkpoint_end ... checkpoint=disabled`
8. `renderer stage=projection_only_gate`
9. compositor returns through `raster_only_no_writeback_gate`
10. later `fence_wait` still fails and lost-device breadcrumbs still collapse to `BLIT_PASS`

That means the projection crash is **not** gated on the new CPU readback checkpoints at all. Disabling the entire post-projection readback package does not prevent the device-loss path.

#### There is no toxic projection readback checkpoint in the tested order

Because the very first `disabled` checkpoint still detonates, QA did **not** isolate a first toxic readback. The evidence now points one step earlier:

- the device is already being poisoned by `projection_only` before any of the newly split readbacks execute
- therefore `histogram_header_only`, `projection_probe_only`, `sort_keys_sentinel_only`, `sort_values_sentinel_only`, and `culled_splats_sentinel_only` are demoted as candidate *first detonators*

This does **not** prove those readbacks are harmless in every circumstance. It does prove they are not required to trigger the current first failure.

#### Failure signature remains unchanged

Both the normal and accurate reruns preserve the same high-signal signature seen in the earlier projection-focused QA:

- projection launch contract still looks stable (`128` push-constant bytes / `32` floats / group count `(1060, 1, 1)`)
- the compositor still returns through `raster_only_no_writeback_gate`
- failure still first surfaces at `fence_wait`
- lost-device breadcrumbs still collapse to `BLIT_PASS`

### Updated interpretation

This narrows the suspect lane again.

Before bead `oc-15g`, the leading theory was that one of the immediate post-projection blocking readbacks might be the first fence that exposed the loss.

After bead `oc-15g`, the evidence supports a stronger statement:

- projection-only work is sufficient to poison the device even when the entire new readback package is disabled
- so the first trigger is upstream of those CPU readbacks
- the leading suspects stay centered on projection-owned GPU writes, projection-specific resource/lifetime hazards, or synchronization fallout that becomes visible later at `fence_wait` / `BLIT_PASS`

### Next recommendation

Do not spend another QA pass enumerating the remaining readback checkpoints unless a coder specifically needs confirmation for a narrower hypothesis. The first-order answer is already complete.

The next useful lane should stay projection-internal, not readback-internal:

1. inspect projection shader writes / bounds assumptions directly
2. add projection-owned GPU-side sentinels or shader-side guardrails that do not require immediate CPU readback
3. inspect post-projection resource lifetime / aliasing / synchronization assumptions that can poison the device before the later `fence_wait`
