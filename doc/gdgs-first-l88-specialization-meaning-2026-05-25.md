# GDGS first-L88 specialization constant meaning note (2026-05-25)

## Goal

Explain, as narrowly as possible, what the first-L88 specialization constant flip `id=0`, `int`, `0 -> 2` changes in shader behavior / pipeline specialization terms, and why that path still preserves the locked crash envelope at failing `submit_serial=9`.

## Source-backed code path

### 1) The first-L88 canvas pipeline packs three boolean specialization flags into `packed_0`

In `servers/rendering/renderer_rd/renderer_canvas_render_rd.h`, `ShaderSpecialization` defines:

- bit `0`: `use_lighting`
- bit `1`: `use_msdf`
- bit `2`: `use_lcd`

The `PipelineKey` hash includes `shader_specialization.packed_0`, so this value participates directly in selecting/creating the specialized render pipeline.

### 2) The first-L88 batch computes `packed_0` from batch booleans, then Task 177's narrow experiment can override only the packed value

In `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`:

- baseline value is computed as
  - `use_lighting ? 1 : 0`
  - `use_msdf ? 2 : 0`
  - `use_lcd ? 4 : 0`
- the narrow Task 177 env gate can force only
  - `pipeline_key.shader_specialization.packed_0 = 0x2`

That means the exact forced flip is:

- baseline `0x0` = lighting off, msdf off, lcd off
- forced `0x2` = lighting off, **msdf on**, lcd off

The selector trace from the value-only artifact proves the batch-level booleans stayed fixed while only the packed specialization payload changed:

- `baseline_packed_0=0x0`
- `experiment_packed_0=0x2`
- `held_use_msdf=false`

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/`

Primary evidence:
- `notes.md`
- `summary.tsv`
- `results.json`
- `specialization_value_only/stdout.log`

### 3) That packed value becomes specialization constant `constant_id = 0`

In `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`, pipeline creation converts `packed_0` into the pipeline specialization vector:

- `sc.constant_id = 0`
- `sc.int_value = p_pipeline_key.shader_specialization.packed_0`
- `sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT`

So `0 -> 2` is not just metadata; it is the exact value passed into Vulkan pipeline specialization for the canvas shader.

### 4) In the non-ubershader canvas path, shader-side `sc_packed_0()` comes from the pipeline specialization constant, not from push constants

In `servers/rendering/renderer_rd/shaders/canvas_uniforms_inc.glsl`:

- `layout(constant_id = 0) const uint pso_sc_packed_0 = 0;`
- `sc_packed_0()` returns `pso_sc_packed_0`
- `sc_use_lighting()` reads bit `0`
- `sc_use_msdf()` reads bit `1`
- `sc_use_lcd()` reads bit `2`

In `renderer_canvas_render_rd.cpp`, `_get_pipeline_specialization_or_ubershader()` also zeroes the push-constant shadow for the specialized path:

- non-ubershader: `r_push_constant.shader_specialization = {}`
- only ubershader copies the specialization flags into push constants

So on this first-L88 path, forcing `packed_0=0x2` means the specialized pipeline itself reports `sc_use_msdf()==true`; the draw cannot "fall back" to the batch's original `use_msdf=false` through push constants.

### 5) In the canvas fragment shader, bit 1 selects the MSDF sampling/coverage branch

In `servers/rendering/renderer_rd/shaders/canvas.glsl`, for the non-attribute/non-primitive rect path:

- `if (sc_use_msdf()) { ... }`
- `else if (sc_use_lcd()) { ... }`
- `else { color *= texture(...) }`

The MSDF branch changes fragment behavior by:

- sampling the color texture as an MSDF texture
- computing a median distance from RGB channels
- deriving coverage/outline alpha from `params.msdf`
- modulating `color.a` from that distance field result

So the exact `0 -> 2` flip changes the first-L88 rect pipeline from the ordinary texture-multiply branch to the MSDF coverage branch, while leaving lighting and LCD specialization bits off.

## What the flip means in pipeline-specialization terms

This is the narrowest honest reading:

- it does **not** change descriptor-set topology
- it does **not** change render-pass compatibility context
- it does **not** change vertex-input shape in the value-only contrast
- it does **not** change the draw arguments
- it **does** select a different specialized shader recipe for the same canvas draw packet

More concretely, the first surviving pipeline fork at first-L88 `bind_render_pipeline` stays the specialization bucket identified in Tasks 175-177, and inside that bucket the exact payload change is the singleton `constant_id=0` value `0 -> 2`, which flips the shader's `sc_use_msdf()` specialization branch on.

## Why this specialized path still preserves the locked crash envelope

The value-only experiment from Task 177 already showed that this exact specialization flip alone is sufficient to preserve the locked outer failure identity:

- `submit_serial=9`
- `fence_wait_error submit_serial=9`
- `Tonemap (L87)` then `Command Graph (L88)`
- later `BLIT_PASS`
- exit `-6`

The narrow reason it preserves the envelope is that the experiment changed only the first-L88 specialized pipeline recipe while holding the already-audited surrounding packet structure fixed:

- same first-L88 packet/draw lane
- same vertex input recipe hash in the value-only contrast
- same first vertex bind shape (`binding_count=1`)
- same descriptor topology / same first draw consumer shape from earlier tasks
- same pre-draw sync scope and lifetime controls from earlier tasks

So the crash envelope does not require a broader change than this pipeline-specialization fork. The exact `id=0` value flip is already enough to put execution onto a distinct specialized first-L88 pipeline path while leaving the rest of the locked packet envelope materially unchanged.

## Important boundary on interpretation

This note explains **what the value changes** and **why that specialized path still preserves the crash envelope**.

It does **not** claim that the root cause is "MSDF rendering is broken" in a broad sense.

The approved evidence only supports the narrower statement that, on this locked first-L88 packet:

- `packed_0=0x2` enables the shader's MSDF-specialized branch in the specialized canvas pipeline
- that specialized pipeline fork alone is sufficient to keep the same failing `submit_serial=9` envelope alive

## Conclusion

The exact first-L88 specialization constant flip `id=0`, `int`, `0 -> 2` means:

- bit `1` of canvas `sc_packed_0` changes from `0` to `1`
- the first-L88 non-ubershader canvas pipeline binds a shader variant where `sc_use_msdf()` is specialized true
- the fragment path switches from ordinary texture modulation to the MSDF distance-field coverage/outline branch

That change is enough to preserve the same locked crash envelope because it is consumed at the earliest surviving first-L88 pipeline bind fork while the rest of the already-audited execution envelope remains fixed.