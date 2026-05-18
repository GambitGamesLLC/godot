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

## Follow-up QA pass for bead `oc-bl3` — projection GPU guard diagnostics rerun

### Scope

Rerun the staged repro after bead `oc-33u` added the new projection-internal GPU guard counters and first-failure fields, while keeping the investigation projection-only and using the minimum readback mode that can still surface the probe.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `a5da0f979d717895073c820237defaca35ecbc4e`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `64287e1b260cf17e527578b036bbcea7ea30a13c`

### Runtime used

For comparability with the earlier dev5 repro evidence, QA again used the preserved repro binary:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`

Important runtime note: the first noninteractive retry using `--headless` fell into Godot's dummy renderer and never exercised Vulkan, so QA discarded that attempt as invalid. The durable artifact package below uses the corrected host-GPU launch path instead:

- `--display-driver wayland --rendering-driver vulkan`
- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000`

That corrected launch path preserved the real Vulkan repro but did change the observed render size from the earlier headless package (`1152x648`) to the live desktop size (`2304x1296`). The crash signature and probe result below were stable across both the normal and `--accurate-breadcrumbs` reruns on that corrected launch path.

### Artifact roots

- normal run: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-20260517-180114/`
- accurate rerun: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-accurate-20260517-180149/`

Key files:

- normal log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-20260517-180114/logs/projection_only__projection_probe_only.normal.log`
- accurate log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-accurate-20260517-180149/logs/projection_only__projection_probe_only.accurate.log`
- normal context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-20260517-180114/context.txt`
- accurate context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-guard-vulkan-dev5-accurate-20260517-180149/context.txt`

### Exact runs performed

1. invalid launch probe — `projection_only + projection_probe_only` via `--headless`; discarded because Godot fell into the dummy renderer instead of the Vulkan path
2. `projection_only + projection_probe_only` via `--display-driver wayland --rendering-driver vulkan` — exit `134`
3. `projection_only + projection_probe_only + --accurate-breadcrumbs` via the same Vulkan launch path — exit `134`

Per the minimum-runs constraint, QA did not broaden back out to other stages. `projection_probe_only` was already the smallest useful readback mode for surfacing the new guard package.

### Findings

#### The new GPU guard package stays completely clean before the later device-loss path

Both valid reruns reach the same projection chain successfully:

1. `renderer stage=prepared ... projection_push_constant_bytes=128 ... projection_group_count=1060`
2. `renderer stage=projection_begin ... push_constant_floats=32 ... sort_capacity=2711230`
3. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
4. `rd barrier complete pipeline=gsplat_projection`
5. `renderer stage=projection_end`
6. `renderer stage=projection_post_dispatch_checkpoint_begin ... checkpoint=projection_probe_only`
7. `renderer stage=projection_readback_projection_probe_begin ... projection_probe_valid=true`

At the probe readback itself, `fence_wait` still fails, but the probe buffer is returned and decoded. The important result is that every newly added guard field remains zero / unset:

- `probe_error_flags_hex=0x00000000`
- `probe_first_failure_stage_name=none`
- `probe_first_failure_id=0`
- `probe_max_requested_sort_end=0`
- `probe_max_requested_tile_id=0`
- `probe_sort_overflow_guard_count=0`
- `probe_tile_guard_count=0`
- `probe_guard_abort_count=0`
- `probe_non_finite_failure_count=0`

The broader probe words are also all zero in both reruns:

- `probe_words=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]`
- `probe_invocations=0`
- `probe_visible_splats=0`
- `probe_duplicated_splats=0`
- `probe_emitted_sort_elements=0`

So this QA pass did **not** observe an immediate guarded projection failure class. The new guards stay clean all the way up to the later fence/device-loss path.

#### The failure still surfaces at `fence_wait`, and the later breadcrumb signature still collapses to `BLIT_PASS`

The key failure signature is unchanged from the earlier projection-focused QA, just with the tighter probe-specific call site now identified:

- `ERROR: Condition "err != VK_SUCCESS" is true. Returning: FAILED`
- `at: fence_wait (drivers/vulkan/rendering_device_driver_vulkan.cpp:2983)`
- GDScript backtrace now pins the first blocking readback to `_log_projection_probe_readback (...)`
- the compositor still returns through `raster_only_no_writeback_gate`
- lost-device breadcrumbs still report `Last known breadcrumb: BLIT_PASS`

That means the new GPU-side guard counters do **not** provide evidence of an immediate projection-internal bounds/NaN/tile-overflow guard trip before the later device-loss path becomes visible.

### Updated interpretation

This is a useful negative result.

Compared with bead `oc-15g`, the answer is now sharper:

- disabling readbacks previously showed that CPU readback itself was not required to poison the device
- this new probe-only rerun shows that even when the smallest useful guard package is read back, the new GPU-side projection guards remain completely clean
- therefore the current evidence does **not** support the theory that the new shader-side projection guards are catching an immediate projection bounds violation before the crash

That does **not** prove the projection shader is correct. It means the newly instrumented guard classes stayed unset, so the failure still looks more like projection-triggered device-loss / synchronization / lifetime fallout than a promptly observed guarded projection error class.

### Next recommendation

Keep the next work narrowly on what happens after projection dispatch rather than on enumerating more projection-probe fields.

Best next suspects after this QA rerun:

1. whether the probe SSBO itself is actually visible/coherent when read back in this compositor path, since the all-zero probe may reflect “never observed” rather than “definitively no work happened”
2. post-projection resource lifetime / aliasing / synchronization hazards that are not covered by the current guard classes
3. deeper engine/backend investigation around why the projection lane can submit and barrier successfully, yet the device is still lost by the next fence and later collapses to `BLIT_PASS`

## Follow-up QA pass for bead `oc-2cr` — projection probe visibility vs scratch mirror visibility

### Scope

Compare the two minimum projection-only checkpoints requested after bead `oc-lnp` added the scratch-mirror instrumentation path:

- `projection_only + projection_probe_only`
- `projection_only + scratch_projection_mirror_only`

The question for this pass was whether the known-good scratch probe buffer would surface projection activity / stage bits even when the main projection probe remained zero or otherwise unhelpful.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `61ab50f3`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `7f569a8`

### Runtime used

To stay directly comparable to the earlier valid Vulkan repro package and avoid the already-documented managed-runtime drift / dummy-renderer trap, QA again used the preserved dev5 repro binary on the host GPU path:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Harness note

The existing temporary checkpoint harness at `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd` had not yet been updated with the new `scratch_projection_mirror_only` enum value. QA patched only that temp harness (not repo code) to map `scratch_projection_mirror_only` to checkpoint value `7`, then reused the same projection-only checkpoint runner flow as the earlier focused QA passes.

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-mirror-vulkan-dev5-20260517-18222280927/`

Key files:

- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-mirror-vulkan-dev5-20260517-18222280927/run_summary.tsv`
- probe log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-mirror-vulkan-dev5-20260517-18222280927/logs/projection_only__projection_probe_only.normal.log`
- mirror log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-mirror-vulkan-dev5-20260517-18222280927/logs/projection_only__scratch_projection_mirror_only.normal.log`

### Exact runs performed

1. `projection_only + projection_probe_only` — exit `134`
2. `projection_only + scratch_projection_mirror_only` — exit `134`

Per the minimum-runs constraint, QA did not add an `--accurate-breadcrumbs` rerun because the two requested checkpoints already produced a directly comparable answer.

### Findings

#### Both checkpoints still reproduce the same later failure signature

Both runs reach the same stable projection launch chain before the later failure:

1. `renderer stage=prepared ... projection_push_constant_bytes=128 ... projection_group_count=1060 ... tile_bounds_capacity=11664 ... sort_capacity=2711230`
2. `renderer stage=projection_begin ... push_constant_floats=32 ... max_tile_id=11663 ... projection_dispatch_serial=1`
3. `rd dispatch pipeline=gsplat_projection push_constant_bytes=128 direct=true group_count=(1060, 1, 1)`
4. `rd barrier complete pipeline=gsplat_projection`
5. `renderer stage=projection_end ... projection_dispatch_serial=1`
6. checkpoint-specific readback begin/end markers log
7. `renderer stage=projection_only_gate`
8. compositor returns through `raster_only_no_writeback_gate`
9. failure still later surfaces at `fence_wait`
10. lost-device breadcrumbs still collapse first to `BLIT_PASS`

So adding the scratch mirror checkpoint does **not** change the later failure location in this repro package.

#### `projection_probe_only` stays all-zero / non-observing

The main projection-probe path still returns an all-zero package even though the readback helper runs to completion:

- `probe_words=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]`
- `probe_invocations=0`
- `probe_visible_splats=0`
- `probe_duplicated_splats=0`
- `probe_emitted_sort_elements=0`
- `probe_error_flags_hex=0x00000000`
- `probe_first_failure_stage_name=none`
- `probe_guard_abort_count=0`
- `probe_sort_overflow_guard_count=0`
- `probe_tile_guard_count=0`

So this pass does not overturn the earlier observation that the main projection probe is still effectively unreadable / non-observing in the failing Vulkan path.

#### The scratch mirror path also stays all-zero; it does not reveal hidden projection activity

The new scratch-mirror-only checkpoint was supposed to answer whether projection activity became visible on the known-good scratch probe buffer even when the main projection probe stayed zero. In this run, it did **not**.

The mirror readback completes, but the scratch buffer is entirely zeroed rather than carrying either the normal scratch positive-control signature or the mirrored projection counters/stage bits:

- `scratch_words=0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000`
- `scratch_signature_ok=false`
- `scratch_projection_invocations=0`
- `scratch_projection_visible_splats=0`
- `scratch_projection_stage_bits=0`
- `scratch_projection_stage_bits_hex=0x00000000`
- `scratch_projection_entered=false`
- `scratch_projection_visible_path=false`
- `scratch_projection_culled_write=false`
- `scratch_projection_sort_reserved=false`
- `scratch_projection_sort_written=false`
- `scratch_projection_max_requested_sort_end=0`

That means the known-good scratch buffer path does **not** currently surface projection activity/stage bits that the main projection probe misses. In this failing path, both evidence buffers remain zero-valued.

### Updated interpretation

This is another useful negative result.

The new mirror path does **not** provide the hoped-for separation between “projection worked but the main probe is incoherent” and “projection poisoned broader post-dispatch state before either probe became observably useful.” Instead, both the dedicated projection probe and the scratch-mirror checkpoint return zero-valued evidence while the later failure signature remains unchanged.

That keeps the leading suspicion on projection-triggered synchronization / lifetime / backend fallout or on broader probe/read visibility incoherency that affects both evidence paths under the failing projection workload.

### Next recommendation

Do not spend another QA pass repeating the same two checkpoints unless coder work materially changes the projection-side evidence path.

Best next lane from this result:

1. inspect why the projection dispatch can complete with barrier logging, yet both projection-owned and scratch-mirror readbacks remain zero-valued under the failing workload
2. keep investigating post-dispatch synchronization / lifetime hazards around the projection path
3. if a coder adds a third evidence path that avoids the current readback/coherency ambiguity, compare it against these two zero-valued baselines


## Follow-up QA pass for bead `oc-x6n` — projection lifetime / cleanup hazard correlation

### Scope

Run the minimum projection-only Vulkan repro after bead `oc-hm0` added projection resource snapshot, cleanup-request, cleanup-flush, cleanup-state, rebuild, and alias breadcrumbs. The question for this pass was whether the exact projection-owned resource set stays stable from `projection_begin` through `projection_post_dispatch_checkpoint_end`, or whether any cleanup / rebuild / alias / identity-change event lands before the later `fence_wait` / `BLIT_PASS` collapse.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `5c67b6be`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `e610c25`

### Runtime used

To stay comparable to the earlier valid Vulkan evidence and avoid the already-documented managed-runtime drift / dummy-renderer trap, QA again used the preserved dev5 repro binary on the host GPU path:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-vulkan-dev5-20260517-18492290849/`

Key files:

- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-vulkan-dev5-20260517-18492290849/logs/projection_only__disabled.normal.log`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-vulkan-dev5-20260517-18492290849/context.txt`

### Exact runs performed

1. `projection_only + disabled` via host Vulkan path — abort / device-loss (`signal 4`; later `fence_wait` + `BLIT_PASS` in log)

Per the minimum-runs constraint, QA did not add an `--accurate-breadcrumbs` rerun because the lifetime/cleanup answer was already clear from the first valid Vulkan pass.

### Findings

#### No cleanup request / flush / post-dispatch rebuild event appears before the later failure

The lifetime/control serials stay flat across the critical projection window:

1. `gpu_state_cache rebuild_gpu_state ... gpu_generation=1 projection_dispatch_serial=0`
2. `renderer stage=projection_begin ... gpu_generation=1 cleanup_request_serial=0 cleanup_request_reason=none projection_dispatch_serial=1`
3. `renderer stage=projection_end ... gpu_generation=1 cleanup_request_serial=0 cleanup_request_reason=none projection_dispatch_serial=1`
4. `renderer stage=projection_post_dispatch_checkpoint_begin ... checkpoint=disabled`
5. `renderer stage=projection_post_dispatch_checkpoint_disabled ... checkpoint=disabled`
6. `renderer stage=projection_post_dispatch_checkpoint_end ... checkpoint=disabled gpu_generation=1 projection_dispatch_serial=1`
7. later `fence_wait` still fails and lost-device breadcrumbs still collapse to `BLIT_PASS`

Within that run, QA did **not** observe:

- any `gpu_state_cache request_cleanup ...`
- any `gpu_state_cache flush_pending_cleanup ...`
- any `gpu_state_cache cleanup_state ...` after the initial pre-dispatch rebuild/setup
- any second `rebuild_gpu_state ...` before the later device-loss path

So the available serial/generation evidence does **not** show cleanup timing or a mid-flight rebuild racing the failing projection dispatch.

#### Exact projection resource snapshot comparison is blocked by an instrumentation bug on this branch state

This pass also found a new instrumentation defect that matters for interpretation.

The snapshot helper invoked from both `gaussian_gpu_state_cache.gd` and `gaussian_renderer.gd` throws before it can emit the intended RID inventory:

- `SCRIPT ERROR: Invalid type in function '_rid_string' ... Cannot convert argument 1 from Callable to RID.`
- first seen from `GaussianGpuStateCache._projection_resource_snapshot (...)` during `rebuild_gpu_state`
- repeated from `GaussianRenderer._projection_resource_snapshot (...)` at `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`

Because of that helper failure, every logged `projection_resource_snapshot` in this QA run degraded to `{}` instead of the intended RID map, so QA could **not** directly compare:

- projection descriptor-set RID
- scratch descriptor-set RID
- projection / scratch pipeline RIDs
- projection probe / scratch probe / histogram / sort / culled / tile / render / depth resource RIDs
- alias detection output across those exact projection-owned resources

So the run answers the cleanup/rebuild timing part more strongly than the exact resource-identity/alias part.

#### Failure signature remains unchanged

Even with the post-dispatch checkpoint disabled and no cleanup activity observed, the later failure signature is still the same:

- projection launch contract remains `128` bytes / `32` floats / group count `(1060, 1, 1)`
- `projection_only_gate` is reached
- compositor returns through `raster_only_no_writeback_gate`
- later `fence_wait` fails
- lost-device breadcrumbs still collapse to `BLIT_PASS`

### Updated interpretation

This pass gives a useful partial answer with an explicit caveat.

Supported by the run:

- there is no logged cleanup request, pending-cleanup flush, or second rebuild between `projection_begin` and `projection_post_dispatch_checkpoint_end`
- `gpu_generation` stays `1`
- `projection_dispatch_serial` stays `1`
- `cleanup_request_serial` stays `0`
- `cleanup_request_reason` stays `none`
- the later failure still surfaces at `fence_wait` / `BLIT_PASS`

Not supported because of the snapshot-helper bug:

- exact RID-by-RID stability comparison across `projection_begin` → `projection_end` → `projection_post_dispatch_checkpoint_end`
- direct alias detection / resource-identity change claims for the exact projection-owned resources listed by the new instrumentation

### Next recommendation

Fix the snapshot helper first. Right now the new lifetime lane already suggests that cleanup timing is **not** the first visible problem, but the resource-identity / alias question is only partially answered because the supposed RID snapshot path emits `{}` after a `Callable`→`RID` type error.

Best next step:

1. repair `_projection_resource_snapshot()` so descriptor-set / pipeline / buffer / texture RIDs serialize cleanly instead of faulting on the pipeline entries
2. rerun the same single `projection_only + disabled` Vulkan pass
3. compare the exact snapshot payloads at `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end` before broadening further

## Coder follow-up for bead `oc-8ao` — snapshot helper repair landed

### What changed

Coder confirmed the Task 18 failure mode: the new projection snapshot helper was still trying to stringify `state.pipelines[...]` through `_rid_string(rid: RID)`, but those pipeline entries are `Callable` dispatch closures in this GDGS codepath, not `RID` values. That is the direct reason the earlier QA run logged `Cannot convert argument 1 from Callable to RID` and emitted `{}` for every `projection_resource_snapshot`.

The fix landed in both runtime copies of the helper:

- `addons/gdgs/runtime/render/gaussian_gpu_state_cache.gd`
- `addons/gdgs/runtime/render/gaussian_renderer.gd`

The repair keeps the instrumentation diagnostic and reversible:

- `_rid_string()` stays RID-only
- `_descriptor_set_rid_string()` serializes the real descriptor-set RIDs
- `_pipeline_snapshot_string()` now records projection / scratch pipeline presence as `Callable(valid=true|false)` instead of pretending those entries are RIDs
- alias reporting now groups duplicate tracked resource RIDs by member name via `alias_groups` instead of the earlier flat duplicate list

### Validation recorded by coder

Coder ran the preserved dev5 binary against the GDGS project with a lightweight headless load check:



- `timeout 15s /home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64 --headless --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --quit`
- exit status: `0`

No script parse/type error from the touched snapshot helper path was reported during that validation run.

### QA impact

This coder pass does **not** replace the earlier QA result; it clears the instrumentation defect that blocked the exact resource-identity comparison. The next QA rerun should repeat the same minimum valid host-Vulkan pass:

1. `projection_only`
2. readback checkpoint `disabled`
3. same preserved dev5 runtime / host Vulkan launch path used in the earlier valid repro package

That rerun should now check whether the logged `projection_resource_snapshot` payloads at `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end` remain stable and whether any duplicate RID groups are reported before the later `fence_wait` / `BLIT_PASS` collapse.

## Follow-up QA pass for bead `oc-k2b` — projection lifetime correlation after snapshot-helper fix

### Scope

Rerun the same minimum valid host-Vulkan repro after bead `oc-8ao` repaired the projection snapshot helper so the exact resource snapshot and alias diagnostics could be trusted again. The question for this pass was whether the tracked projection-owned resources stay stable across:

- `projection_begin`
- `projection_end`
- `projection_post_dispatch_checkpoint_end`

and whether any duplicate `alias_groups` appear before the later `fence_wait` / `BLIT_PASS` collapse.

Branches / commits under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `6b696283`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `60bc52d`

### Runtime used

To stay directly comparable to the earlier valid Vulkan evidence and avoid the already-documented managed-runtime drift / dummy-renderer trap, QA again used the preserved dev5 repro binary on the host GPU path:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-godot-47-dev5-nightly-repro-2026-05-16/godot-dev5/Godot_v4.7-dev5_linux.x86_64`
- version: `4.7.dev5.official.a8643700c`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-snapshotfix-vulkan-dev5-20260517-191507/`

Key files:

- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-snapshotfix-vulkan-dev5-20260517-191507/run_summary.tsv`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-snapshotfix-vulkan-dev5-20260517-191507/logs/projection_only__disabled.normal.log`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-projection-lifetime-snapshotfix-vulkan-dev5-20260517-191507/context.txt`

### Exact run performed

1. `projection_only + disabled` via the preserved dev5 runtime on the host Wayland/Vulkan path — exit `134`

Per the minimum-runs constraint, QA stopped after this single valid repro because it answered the RID-stability / alias question directly.

### Findings

#### The projection resource snapshot is now populated and stays stable across all three checkpoints

The repaired helper now emits the tracked projection-owned resource snapshot instead of `{}`. Across `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`, the snapshot stays byte-for-byte stable for the tracked members:

- `culled_splats="RID(11171209936915)"`
- `depth_texture="RID(11227044511803)"`
- `histogram="RID(11179799871509)"`
- `projection_pipeline="Callable(valid=true)"`
- `projection_probe="RID(11218454577181)"`
- `projection_set="RID(11231339479061)"`
- `render_texture="RID(11222749544506)"`
- `scratch_pipeline="Callable(valid=true)"`
- `scratch_probe="RID(11214159609884)"`
- `scratch_probe_set="RID(11257109282843)"`
- `sort_keys="RID(11184094838806)"`
- `sort_values="RID(11188389806103)"`
- `tile_bounds="RID(11205569675290)"`

The serial / generation / cleanup fields also stay stable through that same window:

- `gpu_generation=1`
- `projection_dispatch_serial=1` at `projection_begin`, `projection_end`, and `projection_post_dispatch_checkpoint_end`
- `cleanup_request_serial=0`
- `cleanup_request_reason=none`

The pre-dispatch rebuild snapshot is consistent with the later per-stage snapshots as well, with only the expected pre-dispatch serial difference:

- `gpu_state_cache rebuild_gpu_state ... gpu_generation=1 projection_dispatch_serial=0 snapshot=...`

#### No duplicate `alias_groups` appear before the later collapse

The repaired alias reporting remains clean at every relevant snapshot site:

- `aliasing_detected=false`
- `alias_groups={}`

So this rerun did **not** observe any duplicate tracked RID group among the projection-owned resources before the later device-loss path.

#### No cleanup / flush / post-dispatch rebuild event appears before failure

The broader lifetime/cleanup timing answer from bead `oc-x6n` still holds on the repaired helper path. In this rerun, QA again observed:

- one initial `gpu_state_cache rebuild_gpu_state ...`
- no `request_cleanup`
- no `flush_pending_cleanup`
- no post-dispatch `cleanup_state`
- no second `rebuild_gpu_state` before the later failure

The disabled checkpoint path itself still completes:

1. `projection_post_dispatch_checkpoint_begin ... checkpoint=disabled`
2. `projection_post_dispatch_checkpoint_disabled ... checkpoint=disabled`
3. `projection_post_dispatch_checkpoint_end ... checkpoint=disabled`
4. `projection_only_gate`
5. `compositor stage=raster_only_no_writeback_gate`

#### Failure signature remains unchanged

Even with the now-working snapshot path, the later failure remains unchanged:

- projection launch contract still logs the stable `128`-byte / `32`-float push-constant contract
- `projection_begin` → dispatch → barrier → `projection_end` all complete
- `projection_post_dispatch_checkpoint_end` is reached with the stable snapshot above
- later `fence_wait` still fails
- lost-device breadcrumbs still collapse to `BLIT_PASS`

### Updated interpretation

This rerun closes the evidence gap left by bead `oc-x6n`.

The supported answer is now stronger and complete for this lifetime/alias question:

- the tracked projection-owned resources stay stable from `projection_begin` through `projection_post_dispatch_checkpoint_end`
- no duplicate `alias_groups` appear in the tracked resource set before the later `fence_wait` / `BLIT_PASS` collapse
- no cleanup request / flush / cleanup-state / second rebuild event is logged in that same window

So this QA pass does **not** support a pre-fence explanation based on obvious tracked-resource RID churn, duplicate aliasing, or cleanup timing within the instrumented projection-owned set. The failure still looks later than that snapshot window, which keeps suspicion on projection-triggered synchronization / backend / lifetime fallout that is not showing up as simple tracked-RID instability.

### Next recommendation

Do not spend more QA runs on the same snapshot-stability question. That lane is now answered for the current tracked resource set.

Best next step:

1. move the next diagnostic slice onto the later synchronization / backend / fence path that follows this stable snapshot window
2. if additional lifetime suspicion remains, instrument resource ownership or backend state beyond the current tracked RID set rather than repeating the same projection snapshot comparison

## Follow-up QA pass for bead `oc-zbz` — submit_serial 8 → 9 chain correlation

### Scope

Run the minimum valid host-Vulkan repro again on the updated instrumentation branches, keep the repro projection-only (`projection_only + disabled`), and answer the specific submit-chain question:

- is `submit_serial=8` the transfer-worker submission?
- is `submit_serial=9` the following frame-1 command submission that waits on that same semaphore chain?
- what command / breadcrumb metadata is attached when the later `fence_wait_error` fires?

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `e968db74`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the source-built Godot editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- timestamp on disk during the run: `2026-05-17 20:19`

Launch path:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000`
- `--display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-map-sourcebuild-20260517-205949/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-map-sourcebuild-20260517-205949/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-map-sourcebuild-20260517-205949/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-map-sourcebuild-20260517-205949/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved context/artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### `submit_serial=8` still maps to the transfer-worker handoff

The runtime log preserves the same high-level ordering previously seen around the stable post-projection snapshot:

1. frame-0 wait completes successfully at `submit_serial=5`
2. `queue_submit submit_serial=8 ... wait_semaphores=0 command_buffers=1 signal_semaphores=1 swap_chains=0 present_submission=false`
3. immediately after that, `frame_execute_begin frame=1 ... wait_semaphores=1 swap_chains=0`
4. then `queue_submit submit_serial=9 ... wait_semaphores=1 command_buffers=1 signal_semaphores=0 swap_chains=0 present_submission=false`

The updated engine source on this branch makes the ownership explicit:

- `RenderingDevice::_submit_transfer_worker(...)` logs `transfer_submit_begin ... signal_semaphores=%d` and then pushes those same semaphores into `frames[frame].semaphores_to_wait_on`
- `RenderingDevice::_execute_frame(...)` / `execute_chained_cmds(...)` consumes `frames[frame].semaphores_to_wait_on` as the wait list for the next main-queue command submission

So even though this binary did not yet emit the new `transfer_submit_begin` line at runtime, the valid repro plus the updated source path support the mapping cleanly: `submit_serial=8` is the transfer-worker submission that seeds the next frame wait chain.

#### `submit_serial=9` is the following frame-1 command submission waiting on that same semaphore chain

This same run shows the expected consumer side:

- `frame_execute_begin frame=1 ... wait_semaphores=1`
- `queue_submit submit_serial=9 ... wait_semaphores=1 command_buffers=1 signal_semaphores=0 swap_chains=0 present_submission=false`
- `frame_execute_submitted frame=1 ...`
- `fence_wait_begin submit_serial=9 ...`
- `fence_wait_error submit_serial=9 wait_result=-4`

That matches the updated main-queue execution code exactly:

- the first frame-1 command buffer waits on the accumulated external semaphore list
- the last/only command buffer in this `no_present` path signals the fence, not a new semaphore
- the later failing fence wait therefore belongs to that same frame-1 command submission

#### Command-label path / breadcrumb metadata caveat on this run

This run produced a valid Vulkan repro and answered the submit-chain ownership question, but it also exposed a build-artifact drift caveat that matters for the richer metadata fields.

The source on branch `e968db74` includes the new Vulkan log fields:

- `wait_summary=...`
- `signal_summary=...`
- `command_summary=...`
- command-buffer `label_path`, `first_label`, `last_label`, and `last_breadcrumb`

However, the source-built editor binary on disk at run time did **not** yet contain those strings, and the saved runtime log correspondingly emitted only the older shorter lines. QA verified that mismatch by checking the source and the binary contents after the run.

So for this bead, the durable evidence package supports:

- submit ownership (`8` = transfer handoff, `9` = following frame-1 wait/execute submit)
- the semaphore-chain relationship
- the later failure site (`fence_wait_error submit_serial=9`)
- the later lost-device breadcrumb collapse to `BLIT_PASS`

But it does **not** include a runtime-emitted `command_summary` / `label_path` payload for `submit_serial=9` from this specific artifact set.

#### Failure signature still surfaces at `fence_wait`, then later collapses to `BLIT_PASS`

The outer failure shape remains unchanged:

- `projection_end`
- `projection_post_dispatch_checkpoint_end ... checkpoint=disabled`
- `render_for_compositor_sync_snapshot`
- frame-0 wait on `submit_serial=5` succeeds
- `submit_serial=8` then `submit_serial=9`
- `fence_wait_error submit_serial=9 wait_result=-4`
- later lost-device breadcrumbs still report `Last known breadcrumb: BLIT_PASS`

### Updated interpretation

This pass is enough to close the ownership question that Task 24 asked.

Supported by the valid repro plus source-path correlation:

- `submit_serial=8` is the transfer-worker submission that signals the semaphore chain consumed by the next frame
- `submit_serial=9` is the subsequent frame-1 main command submission that waits on that same chain and later fails at `fence_wait`
- the later failure still first surfaces at `fence_wait` and still later collapses to `BLIT_PASS`

Explicit caveat:

- this artifact set does **not** yet carry the new runtime-emitted `command_summary` / `label_path` text for `submit_serial=9`, because the source-built editor binary used for the valid run lagged the newest logging strings even though the branch source already contained them

### Next recommendation

Do not spend more QA time re-proving the submit-8 → submit-9 ownership question. That answer is complete enough for this bead.

Best next step:

1. have the coder refresh the source-built editor binary so the new `wait_summary` / `signal_summary` / `command_summary` strings are actually present in the runnable artifact
2. if richer submit-9 command-label provenance is still needed, rerun the same single `projection_only + disabled` host-Vulkan pass once on that refreshed binary
3. otherwise treat the main QA answer as settled and keep the next diagnostic lane focused on why the frame-1 command submission later dies at `fence_wait` / `BLIT_PASS`

## Follow-up QA pass for bead `oc-y6n` — refreshed source-build wait provenance for `submit_serial=9`

### Scope

Run the refreshed source-built host-Vulkan repro once, keep it projection-only (`projection_only + disabled`), and answer the remaining semaphore-provenance questions for the failing frame-1 submit:

- does `submit_serial=9` wait on the exact semaphore last signaled by `submit_serial=8`?
- did that semaphore already have a prior recorded consumer (`last_wait_submit_serial != 0`)?
- do the new wait/signal provenance, transfer payload ownership, and command summaries suggest stale/reused semaphore ownership rather than a pure workload failure?

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `dc6b26d1`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- binary mtime during the run: `2026-05-17 21:40:12 -0400`
- binary contains the new provenance strings (`wait_provenance=`, `signal_provenance=`, `last_wait_submit_serial=`)

Launch path:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000`
- `--display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-provenance-sourcebuild-20260517-214532/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-provenance-sourcebuild-20260517-214532/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-provenance-sourcebuild-20260517-214532/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-provenance-sourcebuild-20260517-214532/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`
- git head recorded in context: `dc6b26d173f8d80e0c67f87e9699be99320df43a`

### Findings

#### `submit_serial=9` waits on the exact same semaphore handle last signaled by `submit_serial=8`

The failing frame-1 main submit waits on Vulkan semaphore handle `107352545131872`:

- `queue_submit submit_serial=9 ... wait_summary=[{source=external,index=0,vk=107352545131872,stage="ALL_COMMANDS"}]`

The immediately preceding transfer-worker handoff signaled that same handle:

- `queue_submit submit_serial=8 ... signal_summary=[{source=submit,index=0,vk=107352545131872}]`
- `signal_provenance=[{source=submit,index=0,vk=107352545131872,signal_submit_serial=8,queue_family=0,queue_index=0}]`

The backend provenance table on submit 9 confirms the match directly:

- `wait_provenance=[{index=0,vk=107352545131872,last_signal_submit_serial=8,...}]`

So the answer is yes: `submit_serial=9` is waiting on the exact semaphore last signaled by `submit_serial=8`.

#### The semaphore did **not** have a prior recorded consumer before `submit_serial=9`

The same submit-9 wait provenance records:

- `last_wait_submit_serial=0`
- `last_wait_queue_family=0`
- `last_wait_queue_index=0`

That means the semaphore did not show a prior recorded wait/consumer before the failing submit-9 handoff. QA did not see stale/reused ownership signs on this semaphore chain.

#### Transfer payload ownership and frame wait provenance are internally consistent across submit 8 → 9

The transfer-worker payload attached to the signaling handoff is explicit:

- `transfer_submit_begin frame=1 transfer_worker=0 signal_semaphores=1 command_fence=true submitted=false command_buffer_id=135588252477144 staging_in_use=63676352 ops_processed=85 ops_submitted=85 ops_recorded=98 ops_used_by_draw=98`

The following frame-1 execution consumes that same payload as its sole wait source:

- `frame_execute_begin frame=1 ... wait_semaphores=1`
- `wait_debug=[{source=transfer_worker,frame=1,worker=0,signal_index=0,semaphore_id=107352545131872,command_buffer_id=135588252477144,command_fence_id=107352559285712,staging_in_use=63676352,ops_processed=85,ops_submitted=85,ops_recorded=98,ops_used_by_draw=98}]`

That matches the Vulkan-level handoff exactly: one transfer-worker signal on submit 8, then one external wait on submit 9 for the same semaphore and payload lineage.

#### Command summaries distinguish the submit-8 transfer handoff from the failing submit-9 main command graph

`submit_serial=8` command summary:

- `command_summary=[{index=0,vk=107352560225984,frame=1,frames_drawn=4,labels=0,breadcrumbs=0,last_breadcrumb="NONE"}]`

`submit_serial=9` command summary:

- `command_summary=[{index=0,vk=107352545824112,frame=1,frames_drawn=4,labels=102,first_label="Command Graph (L-1)",last_label="Command Graph (L88) (Draw)",label_path="Command Graph (L-1) > Command Graph (L0) (Copy) > ...",breadcrumbs=1,last_breadcrumb="UI_PASS"}]`

So the failing submit is not the transfer-worker command buffer itself. It is the subsequent frame-1 main command-graph submission, which waits on the transfer-worker semaphore and later fails at fence wait.

#### Failure signature still first surfaces at `fence_wait`, then later collapses to `BLIT_PASS`

The outer failure shape is unchanged:

- `queue_submit submit_serial=9 ...`
- `fence_wait_begin submit_serial=9 ...`
- `fence_wait_error submit_serial=9 wait_result=-4`
- later: `ERROR: Last known breadcrumb: BLIT_PASS`

The new provenance package narrows the handoff mechanics, but it does not move the first explicit failure site away from `fence_wait` or change the later lost-device breadcrumb collapse.

### Updated interpretation

This refreshed source-build pass completes the semaphore-provenance question cleanly.

Supported directly by runtime evidence:

- `submit_serial=9` waits on the exact Vulkan semaphore handle last signaled by `submit_serial=8`
- that semaphore had no prior recorded consumer (`last_wait_submit_serial=0`) before submit 9
- the transfer payload ownership (`command_buffer_id`, `command_fence_id`, `staging_in_use`, `ops_*`) propagates coherently from the transfer-worker handoff into `frame_execute_begin`
- `submit_serial=8` is a narrow transfer-worker handoff, while `submit_serial=9` is the broader frame-1 main command-graph submission
- the failure still first surfaces at `fence_wait` for submit 9, with later breadcrumbs still collapsing to `BLIT_PASS`

QA did **not** find evidence here of stale/reused semaphore ownership on the specific submit-8 → submit-9 handoff. That shifts suspicion away from “submit 9 waited on the wrong/previously-consumed semaphore” and back toward the workload or synchronization/lifetime fallout carried into the main frame command graph after a valid transfer-worker handoff.

### Next recommendation

Do not spend more QA time re-proving semaphore identity for submit 8 → 9. That answer is now complete.

Best next step:

1. investigate why the valid transfer-worker handoff feeds a frame-1 main command graph that later dies at `fence_wait`
2. focus on synchronization/lifetime fallout or workload poisoning inside the main frame command graph rather than stale semaphore reuse
3. if more engine-side narrowing is needed, add instrumentation that splits the large frame-1 command graph around the projection-following copy/draw/UI segments so the post-submit failure can be attributed more precisely than the later `BLIT_PASS` collapse

## Follow-up QA pass for bead `oc-uvd` — classify `submit_serial=9` command graph segments on refreshed source build

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-1ux` added `label_tail` and `label_segments` to the Vulkan command summary, then classify what kind of frame-1 work actually dominates failing `submit_serial=9`.

Branches / worktree state used:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `5f9c4e65`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-segments-sourcebuild-20260518-073823/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-segments-sourcebuild-20260518-073823/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-segments-sourcebuild-20260518-073823/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-submit-segments-sourcebuild-20260518-073823/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved context/artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### `submit_serial=9` is overwhelmingly copy-heavy, with a late draw/compute tail rather than one dominant pure draw slice

The new `label_segments` summary on the failing frame-1 main submission is:

- `Unclassified`: `1` label (`label_indexes=0..0`, `levels=-1`)
- `Copy`: `21` labels (`1..21`, `levels=0..15`)
- `Draw`: `2` labels (`22..23`, `levels=15..16`)
- `Copy`: `73` labels (`24..96`, `levels=16..86`)
- `Draw`: `1` label (`97..97`, `levels=86`)
- `Compute`: `1` label (`98..98`, `levels=86`)
- `Copy+Compute`: `1` label (`99..99`, `levels=87`)
- `Draw`: `2` labels (`100..101`, `levels=87..88`)

So the actionable answer is not “mostly draw” or “mostly compute.” It is a clearer handoff shape:

- a tiny unclassified root
- an early copy-heavy front block
- a very large middle copy block (`73` labels, far larger than any other segment)
- then only a small late tail containing one draw label, one compute label, one mixed copy+compute label, and two final draw labels

That makes `submit_serial=9` predominantly copy-oriented frame-graph work with a narrow late render/compute epilogue, not a command buffer dominated by the final UI/draw tail.

#### `label_tail` places the nearest visible hazard context late in the frame, but only as a short tail after the dominant copy body

The new `label_tail` for `submit_serial=9` is:

- `Render 3D Transparent Pass (L86) (Copy)`
- `Command Graph (L86) (Copy)`
- `Render 3D Transparent Pass (L86) (Draw)`
- `Command Graph (L86) (Compute)`
- `Command Graph (L87) (Copy+Compute)`
- `Tonemap (L87) (Draw)`
- `Command Graph (L88) (Draw)`

That places the end of the failing command buffer near late transparent/render, tonemap, and final draw work rather than near the early setup/copy labels at the front of the command graph. But the surrounding `label_segments` data matters: that late tail is short and sits after a much larger copy-heavy body. So the best classification is:

- **dominant workload class:** Copy
- **clearest handoff boundary:** large copy body -> tiny late draw/compute tail around `L86`/`L87`/`L88`
- **nearest visible end-of-buffer hazard context:** late render / tonemap / final draw work, not the earliest setup labels

#### Transfer-worker provenance and failure site remain unchanged

The broader submit-chain answer from the prior pass still holds in the same run:

- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

So this pass narrows the frame-1 command graph classification without overturning the already-established semaphore-provenance or later failure-site answers.

### Updated interpretation

This pass gives the first useful structural classification of failing `submit_serial=9`.

Supported by the runtime evidence:

- the failing frame-1 main submission is **not** dominated by a large late draw/UI block
- it is dominated by a long copy-heavy command-graph body, followed by a much smaller late tail containing transparent-pass draw, one compute segment, one mixed copy+compute segment, Tonemap draw, and final draw labels
- `label_tail` therefore places the nearest visible hazard context late in the frame, but `label_segments` says the command buffer as a whole is mostly copy-oriented work

### Next recommendation

Keep the investigation source-built and projection-only, but use this classification to narrow the next engine-side split:

1. prefer the boundary around the large copy body -> late `L86` / `L87` / `L88` draw/compute tail as the next breakpoint
2. inspect whether the first bad inherited work after projection feeds the long copy-heavy body, or whether the late transparent/tonemap/final-draw tail is only where the poisoned submission finally becomes observable
3. avoid reopening shader-side projection probes unless a new backend split points back upstream

## Follow-up QA pass for bead `oc-c1f` — source-built post-projection submit/stall/fence correlation

### Scope

Run the same minimum valid host-Vulkan repro after bead `oc-b8r` landed the new engine-side submit/stall/fence instrumentation, and correlate the first suspicious backend transition *after* the already-stable compositor sync snapshot.

Branches / worktree state under test:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `e9c89177`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

Important build note: the fresh Godot instrumentation worktree did not compile as-is because `VectorView<SwapChainID>` does not provide `is_empty()`. QA applied the minimum non-behavioral build unblock in `drivers/vulkan/rendering_device_driver_vulkan.cpp`:

- `fence->last_present_submission = !p_swap_chains.is_empty();`
- → `fence->last_present_submission = p_swap_chains.size() > 0;`

That one-line fix was required only so the new instrumentation binary could be built and exercised.

### Runtime used

QA used a freshly source-built editor from the Godot worktree on the real host Vulkan path:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- engine version logged by the crash: `Godot Engine v4.7.beta.custom_build (e9c8917768bb2e49a71cc8261ab23ea0dbca6ced)`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-postprojection-sync-sourcebuild-20260517-201958/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-postprojection-sync-sourcebuild-20260517-201958/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-postprojection-sync-sourcebuild-20260517-201958/logs/projection_only__disabled.normal.log`

### Exact run performed

1. `projection_only + disabled` via the freshly source-built Godot binary on the host Wayland/Vulkan path — exit `134`

Per the minimum-runs constraint, QA stopped after this one valid run because it answered the submit/stall/fence transition question directly.

### Findings

#### `render_for_compositor_sync_snapshot` stays stable

The returned sync snapshot matches the stable post-projection checkpoint state exactly:

- `gpu_generation=1`
- `cleanup_request_serial=0`
- `cleanup_request_reason=none`
- `projection_dispatch_serial=1`
- `projection_resource_snapshot.aliasing_detected=false`
- `projection_resource_snapshot.alias_groups={}`
- tracked resource identities remain unchanged from `projection_begin` → `projection_end` → `projection_post_dispatch_checkpoint_end` → `render_for_compositor_sync_snapshot`

So the callback-return seam is still clean. The tracked projection-owned resource set remains stable all the way through `render_for_compositor_sync_snapshot`.

#### The first suspicious post-snapshot transition is the next `queue_submit`, specifically `submit_serial=9`

The post-return ordering is now visible in the source-built engine logs:

1. `projection_post_dispatch_checkpoint_end`
2. `render_for_compositor_returned`
3. `render_for_compositor_sync_snapshot`
4. `raster_only_no_writeback_gate`
5. frame-0 cleanup wait completes successfully:
   - `frame_stall_begin frame=0`
   - `fence_wait_begin submit_serial=5 ... present_submission=true`
   - `fence_wait_end submit_serial=5`
6. new frame-1 backend work starts:
   - `queue_submit submit_serial=8 ... signal_semaphores=1 swap_chains=0 present_submission=false`
   - `frame_execute_begin frame=1 present_requested=false frame_can_present=false wait_semaphores=1 swap_chains=0`
   - `queue_submit submit_serial=9 ... wait_semaphores=1 command_buffers=1 signal_semaphores=0 swap_chains=0 pending_fence_image_semaphores=0 present_submission=false`
   - `frame_execute_submitted frame=1 ...`
   - `frame_stall_begin frame=1 ...`
   - `fence_wait_begin submit_serial=9 ...`
   - `fence_wait_error submit_serial=9 wait_result=-4`
   - later lost-device breadcrumbs still collapse to `BLIT_PASS`

Why QA calls `queue_submit` the first suspicious transition:

- the stable sync snapshot has already been emitted before it
- the immediately preceding post-return stall/wait on `submit_serial=5` succeeds cleanly
- `submit_serial=9` is the first post-snapshot submission whose fence later fails with `VK_ERROR_DEVICE_LOST`
- the later `frame_stall_begin`, `fence_wait_begin`, and `fence_wait_error` events all belong to that same already-doomed submission chain

So the first suspicious boundary after projection return is no longer a cleanup/rebuild/snapshot change; it is the first new backend submission after the clean snapshot window.

#### Key submit / fence metadata from the failing chain

Successful post-return cleanup wait:

- `fence_wait_begin submit_serial=5`
- `queue_family=0 queue_index=0`
- `wait_semaphores=1`
- `command_buffers=1`
- `signal_semaphores=1`
- `swap_chains=1`
- `pending_fence_image_semaphores=1`
- `present_submission=true`
- followed by `fence_wait_end submit_serial=5`

First suspicious post-snapshot submission:

- `queue_submit submit_serial=9`
- `queue_family=0 queue_index=0`
- `wait_semaphores=1`
- `command_buffers=1`
- `signal_semaphores=0`
- `swap_chains=0`
- `pending_fence_image_semaphores=0`
- `present_submission=false`

Failure surfacing on the matching wait:

- `frame_stall_begin frame=1 fence_signaled=true wait_semaphores=0 swap_chains=0 pending_buffer_downloads=0 pending_texture_downloads=0`
- `fence_wait_begin submit_serial=9 queue_family=0 queue_index=0 fence_status=1 wait_semaphores=1 command_buffers=1 signal_semaphores=0 swap_chains=0 pending_fence_image_semaphores=0 present_submission=false`
- Vulkan debug callback: `GPU hung on one of our command buffers (VK_ERROR_DEVICE_LOST)`
- `fence_wait_error submit_serial=9 queue_family=0 queue_index=0 wait_result=-4`

#### Failure signature is still `fence_wait` first, then later `BLIT_PASS`

This source-built pass preserves the same outer failure shape as the earlier dev5 QA, but now with the engine-side chain visible:

- the stable projection snapshot survives the callback return seam
- failure still first becomes explicit at `fence_wait_error`
- the later lost-device breadcrumb report still collapses to `BLIT_PASS`

### Updated interpretation

This closes the post-projection transition question for the current instrumentation package.

Supported by this run:

- the tracked sync snapshot really does stay stable through `render_for_compositor_sync_snapshot`
- no tracked resource churn / aliasing / cleanup event appears before the later failure
- the first suspicious backend transition after that stable snapshot is the next backend `queue_submit`, specifically `submit_serial=9`
- the fatal error still surfaces at the matching `fence_wait_error`, with the later breadcrumb collapse unchanged

### Next recommendation

Do not spend more QA time re-proving the snapshot stability seam. The next coder/audit slice should focus on what `submit_serial=9` actually contains / inherits, and why that first non-present post-snapshot submission can be queued successfully yet later dies at fence wait.

## Follow-up QA pass for bead `oc-bge` — classify the pre-tail body versus late tail on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan repro after bead `oc-fdg` added `late_tail_split=` to the Vulkan command summary, then classify whether the tighter backend-owned hazard seam inside failing `submit_serial=9` is the large pre-tail body or the short late `L86` / `L87` / `L88` tail.

Branches / worktree state used:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `68cad1f9`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-late-tail-sourcebuild-20260518-083603/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-late-tail-sourcebuild-20260518-083603/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-late-tail-sourcebuild-20260518-083603/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-late-tail-sourcebuild-20260518-083603/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved context/artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### `late_tail_split=` confirms the tighter seam is the dominant pre-tail copy body, not the short late tail

The failing `submit_serial=9` command summary now carries:

- `late_tail_split={max_level=88,tail_start_level=86,pre_tail_labels=94,pre_tail_ops={copy=91,compute=0,draw=2,custom=0,mixed=0,unclassified=1},tail_labels=8,tail_ops={copy=3,compute=1,draw=3,custom=0,mixed=1,unclassified=0},tail_levels=[{level=86,labels=5,ops={copy=3,compute=1,draw=1,custom=0,mixed=0,unclassified=0},first_label="Command Graph (L86) (Copy)",last_label="Command Graph (L86) (Compute)"}, {level=87,labels=2,ops={copy=0,compute=0,draw=1,custom=0,mixed=1,unclassified=0},first_label="Command Graph (L87) (Copy+Compute)",last_label="Tonemap (L87) (Draw)"}, {level=88,labels=1,ops={copy=0,compute=0,draw=1,custom=0,mixed=0,unclassified=0},first_label="Command Graph (L88) (Draw)",last_label="Command Graph (L88) (Draw)"}]}`

That turns the earlier qualitative split into a quantitative one:

- pre-tail body: `94` labels total, overwhelmingly `Copy` (`91`) with only `2` `Draw` and `1` `Unclassified`
- late tail: only `8` labels total across all of `L86` / `L87` / `L88`, split among `3` `Copy`, `1` `Compute`, `3` `Draw`, and `1` `Copy+Compute`

So the tighter backend-owned seam still points at the **dominant pre-tail Copy body** rather than the already-short late tail. The late `L86` / `L87` / `L88` epilogue is where the final visible context lives, but it is too small to overturn the much larger copy-dominated body that precedes it.

#### The new counts match and strengthen the earlier `label_segments` / `label_tail` evidence

This new split is consistent with the earlier `oc-uvd` classification instead of changing it.

Earlier `label_segments` answer:

- `Unclassified(1)`
- `Copy(21)`
- `Draw(2)`
- `Copy(73)`
- `Draw(1)`
- `Compute(1)`
- `Copy+Compute(1)`
- `Draw(2)`

Those earlier segment counts already implied a `94`-label front body before the late tail:

- `1 + 21 + 2 + 73 = 97` labels up through the start of level `86`, but once grouped by the new last-three-level rule the late tail cleanly captures the final `8` labels and leaves `94` labels in the pre-tail bucket
- the same late labels identified earlier in `label_tail` are exactly the labels now reported in `tail_levels[86..88]`

So `late_tail_split=` does not introduce a new competing story. It makes the previous one harder to hand-wave away: the command buffer is not just “copy-heavy overall”; the backend-owned seam still says the large pre-tail body dwarfs the short transparent / tonemap / final-draw tail.

#### Provenance and failure site remain unchanged in the same run

The broader frame-1 failure signature is unchanged:

- `render_for_compositor_sync_snapshot` still stays stable before the new frame-1 submissions
- `submit_serial=8` remains the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the first explicit failure still surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass tightens the next backend-investigation breakpoint.

Supported by the runtime evidence:

- the short late `L86` / `L87` / `L88` tail is real and remains the nearest visible end-of-buffer context
- but the newly quantified seam shows that tail is only `8` labels wide, while the pre-tail body is `94` labels and overwhelmingly copy-dominated (`91` copy labels)
- so the best next split is **inside the large pre-tail Copy body**, not by spending more time reclassifying the already-small late epilogue

### Next recommendation

Stay source-built and projection-only, but move the next engine-side split one step earlier into the large pre-tail Copy body behind failing `submit_serial=9`:

1. classify or split the big pre-tail Copy body itself rather than the already-quantified `L86` / `L87` / `L88` tail
2. treat the late tail as the last visible context, not as the dominant workload seam
3. keep the 8 → 9 semaphore handoff, stable projection snapshot, and projection-first staging conclusions as settled baselines unless new backend evidence directly contradicts them

## Follow-up QA pass for bead `oc-699` — classify the dominant 8-level pre-tail Copy bucket on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-699` and inspect the new `pre_tail_copy_body_split=` backend summary on failing `submit_serial=9`. The question for this pass was no longer whether the late `L86..L88` tail exists — that was already settled — but which fixed 8-level bucket inside the much larger pre-tail Copy body actually dominates the submission.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `62db7e64`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- engine banner from the run: `Godot Engine v4.7.beta.custom_build.296d8248c (2026-05-18 12:17:09 UTC)`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-body-vulkan-sourcebuild-corrected-20260518-093650/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### The dominant 8-level pre-tail bucket is `L8..L15`, not the late `L86..L88` tail

The failing `submit_serial=9` command summary now carries:

- `pre_tail_copy_body_split={bucket_size=8,pre_tail_levels=0..85,dominant_bucket={levels=8..15,labels=14,ops={copy=13,compute=0,draw=1,custom=0,mixed=0,unclassified=0},first_label="Command Graph (L8) (Copy)",last_label="Render Depth Pre-Pass (L15) (Draw)"}, ...}`

That gives the requested bucket classification directly:

- dominant pre-tail bucket levels: `8..15`
- bucket labels: `14`
- bucket ops: `copy=13`, `draw=1`, `compute=0`, `mixed=0`, `custom=0`, `unclassified=0`
- first label: `Command Graph (L8) (Copy)`
- last label: `Render Depth Pre-Pass (L15) (Draw)`

The remaining pre-tail buckets are smaller:

- `0..7`: `8` labels, all `Copy`
- `16..23`: `9` labels (`8` `Copy`, `1` `Draw`)
- `24..31` through `72..79`: each `8` labels, all `Copy`
- `80..85`: `6` labels, all `Copy`

So the fresh backend split says the densest single 8-level hotspot inside the already-dominant pre-tail body is an early-mid Copy band around `L8..L15`, not any part of the demoted late transparent/tonemap/final-draw epilogue.

#### This sharpens — not overturns — the earlier `late_tail_split=` and `label_segments=` evidence

The new bucket result lines up with the previous two backend summaries:

- earlier `late_tail_split=` had already shown that the late `L86..L88` tail is only `8` labels wide with `tail_ops={copy=3,compute=1,draw=3,mixed=1}`
- earlier `label_segments=` had already shown the overall frame-1 submission is dominated by Copy work: `Copy(21)` before the first small draw seam, then `Copy(73)` before the late tail

`pre_tail_copy_body_split=` adds the missing detail inside that 94-label pre-tail body:

- the dominant bucket is not one of the flat all-copy 8-label bands
- it is `L8..L15`, where the command graph still stays overwhelmingly Copy-heavy but also reaches the first draw seam inside the pre-tail body (`Render Depth Pre-Pass (L15) (Draw)`)

So the evidence now forms a coherent stack:

1. `label_segments=`: submission is broadly Copy-dominated with only a tiny late tail
2. `late_tail_split=`: late `L86..L88` tail is real but small (`8` labels)
3. `pre_tail_copy_body_split=`: within the much larger pre-tail body, the densest 8-level hotspot is `L8..L15`

#### Provenance and outer failure signature remain unchanged in the same run

The same corrected repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass completes the next backend-owned narrowing step.

Supported by the runtime evidence:

- the late `L86..L88` tail remains a small end-of-buffer epilogue, not the dominant hotspot
- the dominant 8-level hotspot inside the large pre-tail Copy body is `L8..L15`
- that hotspot is still overwhelmingly Copy-heavy (`13` Copy labels), but it also reaches the first draw seam in that bucket at `Render Depth Pre-Pass (L15) (Draw)`

That makes `L8..L15` the best next backend breakpoint if coder work wants to split the pre-tail Copy body further.

### Next recommendation

Keep the investigation source-built and projection-only, but move the next backend split into or around the newly identified `L8..L15` hotspot rather than revisiting the already-demoted late tail:

1. inspect / split the `L8..L15` region first, especially the transition from the Copy chain into `Render Depth Pre-Pass (L15) (Draw)`
2. keep treating the late `L86..L88` tail as final visible context rather than the dominant workload seam
3. preserve the settled baselines: projection-only is still the first bad event, tracked projection resources are stable, and the `submit_serial=8` -> `submit_serial=9` semaphore handoff remains coherent

## Follow-up QA pass for bead `oc-tz6` — classify the `L8..L15` copy-chain versus `L15` consumer handoff on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-vmd` added `pre_tail_copy_handoff=` to the Vulkan command summary. The question for this pass was whether the tighter backend-owned seam inside the already-identified dominant `L8..L15` hotspot still belongs to the pure `L8..L14` copy chain, or whether it resolves to the first draw-containing `L15` consumer handoff.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `9964a253`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `eb3e53f`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539/logs/projection_only__disabled.normal.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-pre-tail-copy-handoff-vulkan-sourcebuild-20260518-111539/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### `pre_tail_copy_handoff=` splits the old `L8..L15` hotspot into an even copy-prefix / consumer-level handoff

The failing `submit_serial=9` command summary now carries:

- `pre_tail_copy_handoff={dominant_bucket_levels=8..15, first_draw_level=15, copy_chain={levels=8..14,labels=7,ops={copy=7,compute=0,draw=0,...},first_label="Command Graph (L8) (Copy)",last_label="Command Graph (L14) (Copy)"}, consumer={levels=15..15,labels=7,ops={copy=6,compute=0,draw=1,...},first_label="Command Graph (L15) (Copy)",last_label="Render Depth Pre-Pass (L15) (Draw)"}, ...}`

That gives the requested seam classification directly:

- pure copy-chain side: `L8..L14`, `7` labels, all `Copy`
- first consumer side: `L15`, `7` labels total, `6` `Copy` + `1` `Draw`
- first draw-containing level: `15`
- first draw-containing terminal label in the bucket: `Render Depth Pre-Pass (L15) (Draw)`

So the old dominant `L8..L15` bucket is no longer just “copy-heavy somewhere before the tail.” The new handoff split says the bucket resolves into two equally sized halves, with the actual draw-consuming transition concentrated entirely in level `15`.

#### Compared against earlier evidence, the broad copy story still holds, but the *tightest* seam now resolves to the `L15` handoff

This new split sharpens the earlier stack rather than overturning it:

- `label_segments=` still says the overall failing frame-1 command buffer is dominated by Copy work (`Copy(21)` + `Copy(73)` before the tiny late tail)
- `late_tail_split=` still says the late `L86..L88` epilogue is small (`tail_labels=8`) compared with the `94`-label pre-tail body
- `pre_tail_copy_body_split=` still says the densest 8-level hotspot inside that pre-tail body is `L8..L15` with `13` `Copy` labels and `1` `Draw` label

What `pre_tail_copy_handoff=` adds is the final split inside that hotspot:

- the pure copy-only prefix `L8..L14` is real, but it is only half the dominant bucket
- the first draw consumer is not diffused across several later levels; it is concentrated immediately at `L15`
- because the bucket divides evenly by label count and the only draw in the bucket lives in `L15`, the tightest backend-owned seam now points at the `L15` consumer handoff rather than at the pure `L8..L14` copy prefix alone

This does **not** mean the broader frame-1 workload stopped being copy-dominated. It means the next narrowing step should target the first draw-consuming handoff inside the dominant copy bucket, not the already-demoted late tail and not a generic “somewhere in the copy body” theory.

#### Provenance and outer failure signature remain unchanged in the same run

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass completes the next backend-owned narrowing step.

Supported by the runtime evidence:

- the dominant pre-tail hotspot remains `L8..L15`
- within that hotspot, the pure `L8..L14` copy prefix and the `L15` consumer level are equal in label count (`7` vs `7`)
- the only draw in the hotspot appears at `Render Depth Pre-Pass (L15) (Draw)`
- therefore the tighter backend seam now resolves to the first `L15` draw consumer handoff, not to the pure copy-only prefix by itself

### Next recommendation

Keep the investigation source-built and projection-only, but move the next backend split into level `15` itself:

1. split or classify the `L15` labels more finely, especially the handoff from `Command Graph (L15) (Copy)` into `Render Depth Pre-Pass (L15) (Draw)`
2. keep the late `L86..L88` tail demoted as final visible context rather than the dominant seam
3. keep the settled baselines unchanged unless new backend evidence directly contradicts them

## Follow-up QA pass for bead `oc-is3` — classify the `L15` copy-setup prefix versus first draw-consumer boundary

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-89z` added `level_draw_handoff=` to the Vulkan command summary. The question for this pass was whether the tightest remaining backend-owned seam inside the already-identified `L15` consumer level lives in the `Command Graph (L15) (Copy)` setup prefix itself or exactly at the first draw-consumer boundary `Render Depth Pre-Pass (L15) (Draw)`.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-level-draw-handoff-vulkan-sourcebuild-20260518-115554/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### `level_draw_handoff=` resolves level `15` to a six-label copy/setup prefix plus a one-label draw boundary

The failing `submit_serial=9` command summary now carries:

- `level_draw_handoff={level=15,labels=7,ops={copy=6,compute=0,draw=1,...},first_label="Command Graph (L15) (Copy)",last_label="Render Depth Pre-Pass (L15) (Draw)",copy_setup_prefix={labels=6,ops={copy=6,compute=0,draw=0,...},last_copy_label="Command Graph (L15) (Copy)"},draw_consumer_boundary={has_draw=true,first_draw_label="Render Depth Pre-Pass (L15) (Draw)",from_first_draw={labels=1,ops={copy=0,compute=0,draw=1,...}}}}`

That gives the requested split directly:

- `copy_setup_prefix`: `6` labels, all `Copy`, ending at `Command Graph (L15) (Copy)`
- `draw_consumer_boundary`: present, starts immediately at `Render Depth Pre-Pass (L15) (Draw)`, and contains only `1` draw label from the first draw onward

So the remaining seam inside level `15` is no longer a vague mixed level. The instrumentation resolves it to a clean copy/setup prefix followed by a single first draw-consumer boundary.

#### Compared against the earlier stack, the tightest seam resolves to the first `L15` draw consumer, not the copy/setup prefix alone

This result is consistent with and tighter than the previous backend summaries:

- `label_segments=` still says the overall failing frame-1 command buffer is dominated by Copy work, with only a tiny late tail
- `late_tail_split=` still says the late `L86..L88` epilogue is small (`tail_labels=8`) compared with the `94`-label pre-tail body
- `pre_tail_copy_body_split=` still identifies `L8..L15` as the dominant 8-level hotspot (`labels=14`, `copy=13`, `draw=1`)
- `pre_tail_copy_handoff=` already split that hotspot into an even `L8..L14` pure copy chain (`7` labels) and `L15` consumer level (`7` labels, `6` copy + `1` draw)

What `level_draw_handoff=` adds is the final intra-level answer: inside `L15`, the copy/setup prefix itself is still copy-only and ends cleanly, while the only draw work begins exactly at `Render Depth Pre-Pass (L15) (Draw)`. That means the tightest backend-owned seam now resolves to the **first draw-consumer boundary** rather than to the `L15` copy/setup prefix alone.

#### Provenance and outer failure signature remain unchanged in the same run

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass completes the next backend-owned narrowing step.

Supported by the runtime evidence:

- the dominant pre-tail hotspot remains `L8..L15`
- the tighter consumer-level seam remains `L15`
- inside `L15`, the copy/setup prefix is six pure-copy labels and the first draw work appears only at `Render Depth Pre-Pass (L15) (Draw)`
- therefore the tightest backend seam currently resolves to the **first `L15` draw-consumer boundary**, not to the copy/setup prefix by itself

### Next recommendation

Keep the investigation source-built and projection-only, but move the next backend split onto the first `L15` draw consumer itself rather than back into broader copy-body buckets:

1. classify what backend-owned work is attached to `Render Depth Pre-Pass (L15) (Draw)` and its immediate inherited setup/dependency chain
2. keep the late `L86..L88` tail and the pure `L8..L14` copy prefix demoted relative to this tighter seam
3. keep the settled baselines unchanged unless new backend evidence directly contradicts them

## Follow-up QA pass for bead `oc-y2c` — classify the depth-prepass draw-consumer seam on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-an8` added `depth_prepass_consumer_seam=` to the Vulkan command summary. The question for this pass was whether the tightest remaining backend-owned seam resolves to the exact first depth-prepass draw-consumer label or to the contiguous inherited non-draw feeder chain that reaches into that boundary.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- engine banner from the crash: `Godot Engine v4.7.beta.custom_build (becc15f72693cc92b14c9bb728e2e283fba28306)`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700/`

Key files:

- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command used:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-consumer-seam-vulkan-sourcebuild-20260518-122700 no_present compositor projection_only disabled 120`

### Findings

#### `depth_prepass_consumer_seam=` resolves the remaining backend-owned seam to the exact first draw consumer

The failing `submit_serial=9` command summary now carries:

- `depth_prepass_consumer_seam={draw_label_index=22,draw_label="Render Depth Pre-Pass (L15) (Draw)",draw_level=15,contiguous_non_draw_feeder={label_indexes=0..21,levels=-1..15,labels=22,ops={copy=21,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L15) (Copy)"},same_level_setup={label_indexes=16..21,labels=6,ops={copy=6,compute=0,draw=0,custom=0,mixed=0,unclassified=0},first_label="Command Graph (L15) (Copy)",last_label="Command Graph (L15) (Copy)"},inherited_dependency_chain={label_indexes=0..15,levels=-1..14,labels=16,ops={copy=15,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L14) (Copy)"},previous_draw_label=none}`

This matters because the feeder chain is now fully described and still remains entirely non-draw setup work. The same-level `L15` setup is only six copy labels, the inherited prior-level dependency chain is sixteen non-draw labels ending at `Command Graph (L14) (Copy)`, and there is no earlier draw boundary in that contiguous feeder (`previous_draw_label=none`). That means the feeder chain is real and larger in aggregate, but the *first backend-owned consumer transition* is still exactly the `Render Depth Pre-Pass (L15) (Draw)` label itself.

#### Compared against earlier evidence, the broad copy-dominated story still holds, but the tightest seam remains the exact draw-consumer boundary

This new split stays consistent with the earlier stack instead of changing direction:

- `pre_tail_copy_body_split=` still says the dominant pre-tail hotspot is `L8..L15`
- `pre_tail_copy_handoff=` still says that hotspot resolves into a pure `L8..L14` copy chain plus an `L15` consumer level
- `level_draw_handoff=` already showed that `L15` itself splits into a six-label `Command Graph (L15) (Copy)` setup prefix and a one-label draw boundary at `Render Depth Pre-Pass (L15) (Draw)`

`depth_prepass_consumer_seam=` adds the final dependency context around that draw boundary and still does not uncover a narrower competing handoff inside the feeder chain. So the best read remains: the inherited non-draw feeder chain is the contiguous setup that leads into the seam, but the tightest remaining backend-owned seam is the exact first depth-prepass draw consumer boundary, not the feeder chain by itself.

#### Provenance and outer failure signature remain unchanged in the same run

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass completes the current backend-owned narrowing step.

Supported by the runtime evidence:

- the inherited feeder chain into level `15` is now fully classified and remains entirely non-draw work
- the same-level `L15` setup prefix is still only copy/setup work
- the first actual consumer transition is still `Render Depth Pre-Pass (L15) (Draw)`
- therefore the tightest remaining backend-owned seam on failing `submit_serial=9` is the exact depth-prepass draw-consumer boundary, with the inherited feeder chain as the contiguous dependency path that leads into it rather than as the narrower seam itself

### Next recommendation

Keep the investigation source-built and projection-only, but stop spending cycles reclassifying the already-settled copy-chain ancestry. The next useful slice should inspect what backend-owned work or dependency attached to `Render Depth Pre-Pass (L15) (Draw)` makes that first consumer boundary the tightest surviving seam.

## Follow-up QA pass for bead `oc-c1s` — classify the immediate downstream attachment after `Render Depth Pre-Pass (L15) (Draw)`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-73t` extended `depth_prepass_consumer_seam=` with the immediate downstream attachment fields:

- `post_draw_non_draw_attachment`
- `same_level_followup`
- `higher_level_followup`
- `next_draw_label`

The question for this pass was whether the surviving backend-owned seam stayed pinned exactly on `Render Depth Pre-Pass (L15) (Draw)` or whether there was meaningful immediate non-draw backend work after that draw that was actually the tighter seam.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command used:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-downstream-vulkan-sourcebuild-20260518-14412705680 no_present compositor projection_only disabled 120`

### Findings

#### The seam does **not** shift downstream; it stays pinned on `Render Depth Pre-Pass (L15) (Draw)`

The expanded `depth_prepass_consumer_seam=` payload on failing `submit_serial=9` now carries the direct downstream answer:

- `draw_label="Render Depth Pre-Pass (L15) (Draw)"`
- `post_draw_non_draw_attachment={label_indexes=none,levels=none,labels=0,ops={copy=0,compute=0,draw=0,custom=0,mixed=0,unclassified=0}}`
- `same_level_followup={label_indexes=none,labels=0,ops={copy=0,compute=0,draw=0,custom=0,mixed=0,unclassified=0}}`
- `higher_level_followup={label_indexes=none,levels=none,labels=0,ops={copy=0,compute=0,draw=0,custom=0,mixed=0,unclassified=0}}`
- `next_draw_label="Render Opaque Pass (L16) (Draw)"`

That means there is no immediate non-draw backend attachment after the first depth-prepass draw before the next draw boundary appears. The surviving seam therefore does **not** tighten further onto a downstream non-draw follow-up; it stays exactly on the first `Render Depth Pre-Pass (L15) (Draw)` consumer boundary.

#### Earlier feeder-chain and handoff evidence stays intact, but nothing beats the draw boundary itself

The same payload still preserves the earlier ancestry context:

- `contiguous_non_draw_feeder={label_indexes=0..21,levels=-1..15,labels=22,ops={copy=21,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L15) (Copy)"}`
- `same_level_setup={label_indexes=16..21,labels=6,ops={copy=6,...},first_label="Command Graph (L15) (Copy)",last_label="Command Graph (L15) (Copy)"}`
- `inherited_dependency_chain={label_indexes=0..15,levels=-1..14,labels=16,ops={copy=15,compute=0,draw=0,custom=0,mixed=0,unclassified=1},first_label="Command Graph (L-1)",last_label="Command Graph (L14) (Copy)"}`
- `previous_draw_label=none`

So the older `level_draw_handoff=` / `pre_tail_copy_handoff=` / `pre_tail_copy_body_split=` story is still correct: a copy-dominated inherited feeder chain leads into the first `L15` draw consumer. But this new pass removes the remaining ambiguity about downstream work: there is no meaningful immediate non-draw backend attachment after that draw that would be a tighter seam than the draw boundary itself.

#### Provenance and outer failure signature remain unchanged in the same run

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass closes the remaining downstream-attachment question for the current backend seam.

Supported by the runtime evidence:

- the inherited feeder/setup ancestry into `Render Depth Pre-Pass (L15) (Draw)` is still real and already well classified
- there is **no** immediate non-draw attachment after that draw before the next draw boundary
- `same_level_followup` and `higher_level_followup` are both empty
- the next boundary after the seam is simply the next draw label, `Render Opaque Pass (L16) (Draw)`
- therefore the tightest surviving backend-owned seam remains pinned exactly on `Render Depth Pre-Pass (L15) (Draw)` and does not shift downstream

### Next recommendation

Keep the investigation source-built and projection-only, but stop spending more QA cycles on ancestry or immediate-followup reclassification for this seam. The next useful slice should inspect backend-owned work, dependencies, or synchronization behavior attached directly to the `Render Depth Pre-Pass (L15) (Draw)` consumer boundary itself, because both the upstream feeder chain and the immediate downstream non-draw attachment theory are now demoted relative to that exact draw boundary.

## Follow-up QA pass for bead `oc-agt` — classify the exact `Render Depth Pre-Pass (L15) (Draw)` backend boundary on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-agt` and inspect the expanded `depth_prepass_consumer_seam=` payload on failing `submit_serial=9`. This pass was specifically checking whether the exact `Render Depth Pre-Pass (L15) (Draw)` seam behaves like a real draw packet, an inherited-state consumer, or some tighter backend wrapper around the eventual collapse.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- source-built binary rebuilt locally from the active worktree before the run

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-boundary-wrapper-vulkan-sourcebuild-20260518-150439/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-boundary-wrapper-vulkan-sourcebuild-20260518-150439/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-boundary-wrapper-vulkan-sourcebuild-20260518-150439/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-boundary-wrapper-vulkan-sourcebuild-20260518-150439/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### The exact surviving depth-prepass seam behaves like a render-pass wrapper, not a direct draw packet

The updated failing `submit_serial=9` command summary now carries an expanded:

- `depth_prepass_consumer_seam={...,draw_boundary_backend_attachment={consumer_class="render_pass_wrapper",begin_state={...},label_commands={...}}}`

The backend-owned classification from the run is:

- `consumer_class="render_pass_wrapper"`
- `begin_state={render_pass_active=false,framebuffer_active=false,subpass=0,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,index_format=none,begin_breadcrumb="NONE"}`
- `label_commands={render_pass_begin=1,next_subpass=0,render_pass_end=1,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,draw_indirect_calls=0,draw_indexed_indirect_calls=0,execute_secondary_calls=0,secondary_command_buffers=0,secondary_labels=0,secondary_draw_labels=0,first_backend_command="begin_render_pass",last_backend_command="end_render_pass"}`

So the exact label currently pinned as the first surviving seam is **not** a place where this command buffer records a direct indexed/non-indexed/indirect draw, a graphics-pipeline bind, a descriptor-set bind, a vertex/index-buffer bind, or a secondary-command execution. In this repro, that label is acting as a tiny backend wrapper that opens and closes a render pass around deeper work rather than carrying the draw payload itself.

#### The broader failure signature is unchanged

This tighter classification does **not** move the eventual crash site:

- the failing submission is still `submit_serial=9`
- the first explicit failure is still `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

So the new value here is not “the seam moved.” The value is that the seam is now classified more precisely: the earliest surviving depth-prepass consumer boundary is a render-pass-owned wrapper, not a direct draw command packet.

### Updated interpretation

This pass demotes the old mental model of “first depth-prepass draw” as though it were necessarily the first concrete draw call. The surviving seam is better understood as:

- the first **depth-prepass render-pass consumer boundary** inside the dominant `L8..L15` hotspot
- with no direct backend draw/bind commands recorded on the label itself in the primary command buffer
- and therefore likely requiring the next split to look at render-pass ownership / pass setup / work issued beneath that wrapper rather than only counting immediate draw commands on the label itself

### Next recommendation

Keep the source-built, projection-only staging exactly as-is and inspect the backend work immediately under this render-pass wrapper next:

1. determine where the actual depth-prepass draw payload is recorded relative to this wrapper (for example: inside pass-scoped work beneath the label rather than on the label itself)
2. keep command-graph / render-pass ownership evidence ahead of shader-side probes
3. continue treating `submit_serial=9` and the later `BLIT_PASS` collapse as the stable downstream failure envelope

## Follow-up QA pass for bead `oc-124` — inspect the depth-prepass render-pass wrapper payload on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-agt` and inspect the `draw_boundary_backend_attachment={consumer_class,begin_state,label_commands}` classification on failing `submit_serial=9`. The question for this pass was whether the surviving seam really stays pinned to a tiny `render_pass_wrapper` boundary, and whether the real depth-prepass payload shows up as nested / render-pass-owned work beneath that wrapper rather than as direct commands recorded on the label itself.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-payload-vulkan-sourcebuild-20260518-16032731035/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### The seam still resolves to a tiny `render_pass_wrapper` boundary

The fresh `submit_serial=9` payload reproduces the same exact boundary classification already suggested by bead `oc-agt`:

- `draw_boundary_backend_attachment.consumer_class="render_pass_wrapper"`
- `begin_state={render_pass_active=false, framebuffer_active=false, subpass=0, render_pipeline_bound=false, vertex_binding_count=0, index_buffer_bound=false, index_format=none, begin_breadcrumb="NONE"}`
- `label_commands={render_pass_begin=1, next_subpass=0, render_pass_end=1, pipeline_binds=0, uniform_binds=0, vertex_buffer_binds=0, vertex_buffer_binding_total=0, index_buffer_binds=0, draw_calls=0, draw_indexed_calls=0, draw_indirect_calls=0, draw_indexed_indirect_calls=0, execute_secondary_calls=0, secondary_command_buffers=0, secondary_labels=0, secondary_draw_labels=0, first_backend_command="begin_render_pass", last_backend_command="end_render_pass"}`

So the seam does **not** widen back out into a direct draw packet on this rerun. At the exact `Render Depth Pre-Pass (L15) (Draw)` label, the primary command buffer still records only a tiny render-pass wrapper: begin render pass, then end render pass, with zero direct draw/bind/secondary-execute commands attributed to the label itself.

#### The real depth-prepass payload does **not** appear as direct commands on the label itself

This rerun makes the second question answerable in a limited but still useful way.

What QA can now say confidently:

- the exact seam remains pinned on the first depth-prepass consumer boundary
- the label itself still does **not** own direct payload commands in the primary-command-buffer summary
- there is also no evidence that the payload is exposed through secondary command buffers at this exact label, because `execute_secondary_calls=0`, `secondary_command_buffers=0`, `secondary_labels=0`, and `secondary_draw_labels=0`

So the surviving seam still looks like a **render-pass-owned wrapper boundary**, not a direct recorded draw packet. If there is real depth-prepass payload attached to this consumer, it currently appears to live beneath broader render-pass ownership / nested pass machinery that this label-local summary does not unfold, rather than as direct draw/bind commands emitted on the label itself.

That is slightly sharper than the prior pass: the wrapper theory held, and this rerun did **not** reveal a hidden direct payload or secondary-command payload on the label.

#### Compared against earlier seam evidence, the wrapper stayed the tightest boundary

This rerun does not contradict the earlier narrowing stack:

- `pre_tail_copy_body_split=` still demotes the broad late tail in favor of the `L8..L15` hotspot
- `pre_tail_copy_handoff=` still resolves that hotspot toward the first `L15` consumer handoff
- `level_draw_handoff=` still resolves level `15` to a copy/setup prefix plus the first depth-prepass draw consumer boundary
- `depth_prepass_consumer_seam=` still keeps the seam pinned on `Render Depth Pre-Pass (L15) (Draw)` with no tighter immediate downstream non-draw follow-up
- the fresh `draw_boundary_backend_attachment=` readout now confirms that this exact surviving seam remains only a wrapper-level render-pass boundary

So the wrapper stayed the tightest visible seam. The rerun did **not** reveal a lower-level payload owner directly on the label itself.

#### Provenance and outer failure signature remain unchanged

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass closes the QA question for bead `oc-124`.

Supported by the runtime evidence:

- the seam truly does stay on a tiny `render_pass_wrapper` boundary at the exact `Render Depth Pre-Pass (L15) (Draw)` label
- the label-local backend summary still shows no direct draw/bind payload and no secondary-command payload on that label
- therefore the real depth-prepass work, if any, is still only inferable as render-pass-owned / nested work beneath the wrapper rather than as direct commands emitted on the label itself
- the wrapper remained the tightest visible seam; this rerun did **not** expose a lower-level payload owner directly on the label

### Next recommendation

Keep the investigation source-built and projection-only, but stop trying to force this exact label-local summary to behave like a direct draw packet. The next useful slice should look at render-pass-owned work beneath this wrapper or at broader pass-level ownership/state that survives past the wrapper into the later `submit_serial=9` / `BLIT_PASS` failure envelope.

---

## 2026-05-18 — bead `oc-023` coder validation (`nested_scope=` beneath the depth-prepass wrapper)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-nested-vulkan-sourcebuild-20260518-164742319304969/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-nested-vulkan-sourcebuild-20260518-164742319304969/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-nested-vulkan-sourcebuild-20260518-164742319304969/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-wrapper-nested-vulkan-sourcebuild-20260518-164742319304969/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — exit `134`

Exact command shape from the saved artifact package:

- runtime: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- script: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- display mode: `no_present`
- compositor stage: `compositor`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Findings

#### The new nested descendant summary landed in the rebuilt source binary

The refreshed editor contains the new string payload:

- `nested_scope={descendant_labels=`

That confirms the source-built runtime now includes the new descendant/pass-owned wrapper summary added by bead `oc-023`.

#### The exact `Render Depth Pre-Pass (L15) (Draw)` wrapper still shows no nested payload ownership

On the failing `submit_serial=9` command buffer, the new payload expands the existing wrapper classification to:

- `draw_boundary_backend_attachment.consumer_class="render_pass_wrapper"`
- `label_commands={render_pass_begin=1, next_subpass=0, render_pass_end=1, pipeline_binds=0, uniform_binds=0, vertex_buffer_binds=0, vertex_buffer_binding_total=0, index_buffer_binds=0, draw_calls=0, draw_indexed_calls=0, draw_indirect_calls=0, draw_indexed_indirect_calls=0, execute_secondary_calls=0, secondary_command_buffers=0, secondary_labels=0, secondary_draw_labels=0, first_backend_command="begin_render_pass", last_backend_command="end_render_pass"}`
- `nested_scope={descendant_labels=0, descendant_draw_labels=0, descendant_begin_inside_render_pass_labels=0, descendant_begin_inside_render_pass_draw_labels=0, levels=none, label_indexes=none, descendant_commands={render_pass_begin=0, next_subpass=0, render_pass_end=0, pipeline_binds=0, uniform_binds=0, vertex_buffer_binds=0, vertex_buffer_binding_total=0, index_buffer_binds=0, draw_calls=0, draw_indexed_calls=0, draw_indirect_calls=0, draw_indexed_indirect_calls=0, execute_secondary_calls=0, secondary_command_buffers=0, secondary_labels=0, secondary_draw_labels=0}}`

So this pass answers the new question pretty directly: the wrapper does **not** hide a nested label tree, nested draw labels, or secondary-command descendants in the primary-command-buffer trace either. The label still looks like a bare begin/end render-pass wrapper with no unfolded child payload at this instrumentation seam.

#### The outer failure envelope remains unchanged

The same source-built projection-only repro still keeps the previously established failure signature:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore
- the explicit failure still first appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass closes the coder question for bead `oc-023`.

Supported by the runtime evidence:

- the surviving seam still stays pinned on a tiny `render_pass_wrapper` at `Render Depth Pre-Pass (L15) (Draw)`
- the wrapper still owns no direct draw/bind payload on the label itself
- the new descendant summary also shows no nested labels, no nested draw labels, and no nested secondary-command payload beneath that exact label in the primary-command-buffer trace
- the next useful slice therefore needs to move one rung wider than this exact label-local wrapper: render-pass ownership outside the label, or broader pass-level / command-buffer ownership that survives past the wrapper into the later `submit_serial=9` / `BLIT_PASS` failure envelope
