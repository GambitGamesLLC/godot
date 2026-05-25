# GDGS first-L88 specialization value-only sufficiency note (2026-05-25)

## Goal

Verify whether the exact singleton specialization constant value flip at the first-L88 `bind_render_pipeline` recipe is by itself sufficient to preserve the locked crash envelope at failing `submit_serial=9`.

Target seam:
- first-L88 `bind_render_pipeline` recipe at `serial=103`
- specialization constant `id=0`
- type `int`
- value flip `0 -> 2`

## Narrowing change

A reversible diagnostic env gate was added in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`:

- `GODOT_GDGS_TEMP_DIAG_FIRST_CLIPPED_PRESERVE_RECT_SPECIALIZATION_VALUE_FORCE_2_EXPERIMENT=1`

For the targeted first clipped preserve-rect batch only, this gate overrides:
- `pipeline_key.shader_specialization.packed_0 = 0x2`

It does **not** semantically toggle `batch.use_msdf`, and it does **not** add a new diagnostic cache-identity bit to `PipelineKey` hashing. The existing `SPECIALIZATION_FORCE_MSDF_EXPERIMENT` path remains available for contrast.

## Validation artifact

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/`

Primary files:
- `summary.tsv`
- `results.json`
- `notes.md`
- `baseline/stdout.log`
- `specialization_only/stdout.log`
- `specialization_value_only/stdout.log`

Harness:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_specialization_value_only_qa.py`

Binary:
- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`

## Result summary

Three-case matrix:
- `baseline`
- `specialization_only` (`SPECIALIZATION_FORCE_MSDF_EXPERIMENT=1`)
- `specialization_value_only` (`SPECIALIZATION_VALUE_FORCE_2_EXPERIMENT=1`)

Outer crash envelope stayed locked in all three cases:
- `submit_serial=9` reached
- `fence_wait_error submit_serial=9` reproduced
- `Tonemap (L87)` present
- `Command Graph (L88)` present
- `BLIT_PASS` present
- exit status `-6`

Shared non-specialization surface between `baseline` and `specialization_value_only`:
- vertex input recipe hash stayed `0xaf2a1c78`
- blend recipe hash stayed `0xd22fca4d`
- first vertex bind stayed `serial=104`, `binding_count=1`
- first draw consumer stayed `serial=108`, `index_count=6`, `instance_count=27`

Specialization delta between `baseline` and `specialization_value_only`:
- `specialization_constant_hash`: `0x6273dcb1 -> 0xbfcbce98`
- `specialization_constant_value_hash`: `0xc5247e53 -> 0xa18293e9`

Selector proof for the value-only case:
- `requested=true`
- `applied=true`
- `baseline_packed_0=0x0`
- `experiment_packed_0=0x2`
- `held_use_msdf=false`

This proves the targeted singleton `packed_0` flip happened while the batch-level `use_msdf` selector stayed false.

## Important contrast vs the older force-msdf path

`specialization_only` and `specialization_value_only` share the same specialization hashes:
- `specialization_constant_hash=0xbfcbce98`
- `specialization_constant_value_hash=0xa18293e9`

But the selector traces differ:
- `specialization_only` logs `experiment_use_msdf=true`
- `specialization_value_only` logs `held_use_msdf=false`

So the value-only path is the narrower contrast: it preserves the locked crash envelope without semantically flipping `use_msdf` on the batch selector.

## Caveat

In this run, the dedicated packing/provenance marker lines did not emit even though the selector trace did. The sufficiency call therefore relies on:
- selector trace
- fence-wait boundary provenance hashes
- preserved vertex/draw payloads

That evidence is still sufficient for this seam because it directly captures the exact `0x0 -> 0x2` specialization payload flip and shows the locked outer crash envelope is unchanged.

## Conclusion

Yes: the exact singleton specialization constant value flip alone is sufficient to preserve the locked `submit_serial=9` crash envelope on this approved seam.
