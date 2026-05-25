# GDGS first-L88 MSDF fragment-path comparison note (2026-05-25)

## Goal

Compare, as narrowly as possible, what the first-L88 MSDF-specialized fragment path selected by `sc_use_msdf()` changes versus the non-MSDF fragment path on the exact first-L88 packet, and explain why that path still preserves the locked crash envelope at failing `submit_serial=9`.

## Scope lock

This note stays on the already-approved first-L88 specialization seam only:

- exact packet: the first clipped preserve-rect L88 packet already isolated by the prior tasks
- exact specialization flip: `constant_id=0`, `int`, `packed_0 0x0 -> 0x2`
- exact branch selected: `sc_use_msdf()` in `servers/rendering/renderer_rd/shaders/canvas.glsl`

It does **not** reopen specialization-cache theory, request-hash theory, or CPU-side vertex-format-cache theory, and it does **not** widen into a fix.

## Exact packet / selector facts already established

From the existing value-only artifact package:

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/`
- primary summaries: `summary.tsv`, `notes.md`, `results.json`

Already-approved facts for the exact first-L88 packet:

- `baseline_packed_0=0x0`
- `experiment_packed_0=0x2`
- `held_use_msdf=false`
- vertex surface stayed fixed: `vertex_input_recipe_hash=0xaf2a1c78`, first vertex bind `serial=104`, `binding_count=1`
- draw surface stayed fixed: first draw consumer `serial=108`, `index_count=6`, `instance_count=27`
- blend surface stayed fixed: `blend_recipe_hash=0xd22fca4d`
- outer crash envelope stayed fixed: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

So the question here is no longer whether the specialization fork exists. The question is what the `sc_use_msdf()` fragment path changes inside that same packet.

## Where the branch happens

On this first-L88 packet, the fragment-side branch lives in:

- `servers/rendering/renderer_rd/shaders/canvas.glsl`

and the specialization bit comes from:

- `servers/rendering/renderer_rd/shaders/canvas_uniforms_inc.glsl`

The branch is only in the non-`USE_ATTRIBUTES`, non-`USE_PRIMITIVE` rect path:

- the shader first computes the same `color`, `uv`, `vertex`, source-rect / region setup, and optional clip-rect UV clamp
- then it selects one of three texture-consumption branches:
  - `if (sc_use_msdf()) { ... }`
  - `else if (sc_use_lcd()) { ... }`
  - `else { color *= texture(...) }`
- after that branch, both paths rejoin and continue into the same later normal/specular/light/final-color flow

So the live seam is not “a different draw packet.” It is a different fragment texturing/coverage sub-branch inside the same first-L88 rect packet.

## What the non-MSDF path does

On the baseline first-L88 packet (`packed_0=0x0`), `sc_use_msdf()` is false and `sc_use_lcd()` is false, so the fragment shader takes the ordinary rect-texture path:

- `color *= texture(sampler2D(color_texture, texture_sampler), uv);`

That means the sampled texture contributes directly as ordinary color/alpha modulation:

- sampled `rgb` multiplies `color.rgb`
- sampled `a` multiplies `color.a`

No MSDF distance reconstruction happens in this branch.

## What the MSDF-specialized path changes

When the same packet is forced to `packed_0=0x2`, `sc_use_msdf()` becomes true and the shader instead takes the MSDF branch:

1. It still samples the **same** `color_texture` with the **same** `texture_sampler` at the **same** `uv`.
2. But it stops treating the sampled RGB as ordinary display color.
3. Instead, it interprets sampled channels as MSDF distance information:
   - `msdf_sample = texture(...)`
   - `msdf_size = textureSize(...)`
   - `dest_size = vec2(1.0) / fwidth(uv)`
   - `px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0)`
   - `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`
4. It then converts that distance value into coverage / outline alpha:
   - no outline: `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`
   - outline case: uses `params.msdf.y`, includes `msdf_sample.a`, and clamps an outline-adjusted alpha
5. The branch then applies that result only to fragment alpha:
   - `color.a = a * color.a`

That is the narrowest honest description of the semantic change.

## Precise contrast: what changes and what does not

### Changes inside the branch

Compared with the non-MSDF path, the MSDF-specialized path changes:

- **texture interpretation**
  - non-MSDF: sampled texel is treated as ordinary color/alpha
  - MSDF: sampled RGB is treated as a distance field
- **math performed before output**
  - non-MSDF: direct texture multiply
  - MSDF: median-distance reconstruction + derivative-based coverage sizing + optional outline logic
- **which color channels are used for what**
  - non-MSDF: RGB/A are display modulation inputs
  - MSDF: RGB are distance carriers; alpha may participate as outline clamp input
- **which output components are directly changed in this branch**
  - non-MSDF: sampled texture modulates both `color.rgb` and `color.a`
  - MSDF: this branch explicitly rewrites `color.a`; it does not directly multiply `color.rgb` by sampled RGB here

### Does not change on this packet

On the exact approved value-only contrast, the following stay materially fixed:

- same first-L88 packet / same draw lane
- same rect packet family (`TYPE_RECT` / quad path in `renderer_canvas_render_rd.cpp`)
- same descriptor topology and same bound texture/sampler roles
- same vertex-input recipe hash (`0xaf2a1c78`)
- same first vertex bind shape (`serial=104`, `binding_count=1`)
- same first draw consumer shape (`serial=108`, `index_count=6`, `instance_count=27`)
- same blend recipe hash (`0xd22fca4d`)
- same later outer failure envelope (`submit_serial=9` -> `fence_wait_error` -> later `BLIT_PASS`)

And source-wise, both branches still rejoin into the same later fragment pipeline after the texturing branch.

## Why this still preserves the locked crash envelope

The approved artifact evidence already answers the sufficiency part:

- `baseline` and `specialization_value_only` both preserve the same locked outer failure identity
- the value-only case proves the exact specialization payload change occurred while `held_use_msdf=false`
- the non-specialization comparison surface remained fixed

That means the envelope does **not** depend on a broader packet rewrite than this branch swap.

The narrow reason the envelope stays preserved is:

1. The forced specialization changes only the first-L88 pipeline's fragment texturing/coverage semantics.
2. It does **not** move execution to a different packet, different draw shape, different vertex-input recipe, different descriptor topology, or different blend recipe on this approved contrast.
3. The same packet still reaches the same downstream failing envelope at `submit_serial=9`.

So even though the fragment math changes meaningfully — ordinary texture modulation versus MSDF coverage reconstruction — that change remains *contained inside the same already-locked packet shell*. The evidence says that contained branch swap is still sufficient to keep the same crash envelope alive.

## Important interpretation boundary

This note supports a narrow conclusion only:

- on this exact first-L88 packet, the MSDF-specialized path changes the fragment texturing sub-branch from ordinary texture modulation to MSDF distance/coverage computation
- that branch-level specialization is still sufficient to preserve the same locked `submit_serial=9` failure envelope when the surrounding packet surface is held fixed

It does **not** prove a broad claim like “MSDF rendering is the root cause.”

## Conclusion

On the exact first-L88 packet, the non-MSDF path does a direct `color *= texture(...)` multiply, while the MSDF-specialized path still samples the same texture at the same UVs but reinterprets sampled RGB as a signed-distance field, computes derivative-scaled coverage/outline alpha, and rewrites `color.a` from that result.

That is the concrete shader-behavior change selected by `sc_use_msdf()`.

It still preserves the locked crash envelope because the approved value-only experiment changed that specialization branch while keeping the rest of the first-L88 packet surface materially fixed, and the run still reproduced the same outer identity: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`.
