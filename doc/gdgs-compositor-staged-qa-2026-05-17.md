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
