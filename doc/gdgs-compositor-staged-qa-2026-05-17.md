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

## 2026-05-20 source-build follow-up — Task 105 (`oc-7hi`)

A source-built Godot follow-up stayed on the same host-Vulkan `projection_only__disabled` repro lane and added one more diagnostic step inside the carried Tonemap -> `L88` zero-gap boundary: per-attachment `load_op` values plus a per-slot non-`load_op` recipe cohort hash.

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130/`

Validation command:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-load-op-contract-vulkan-sourcebuild-20260520-202130 no_present compositor projection_only disabled 120`

Outcome:

- Repro still aborts at `submit_serial=9` (`exit_status=134`; see `stdout.log`, `stderr.log`, `exit_status.txt`).
- The carried render-pass exact-recipe delta is still narrowed to `load_op` only.
- The new slot-level classification is:
  - `load_op_slot_classifier="single_attachment_slot"`
  - `load_op_contract_scope="single_slot_without_same_recipe_siblings"`
  - `load_op_minimum_contract_hazard="single_slot_load_op_flip"`
  - `active_load_ops=["0:LOAD"]`
  - `pipeline_load_ops=["0:CLEAR"]`
  - `load_op_mismatch_slots=["{index=0,active="LOAD",pipeline="CLEAR",family_hash_match=true,active_family_hash="0x2a58ca8c",pipeline_family_hash="0x2a58ca8c"}"]`

Interpretation:

- The differing `load_op` is **not** spread across a broader attachment-family cohort in this lane; the active pre-rebind scope and carried Tonemap packet each expose only one attachment slot here.
- The non-`load_op` recipe cohort hash still matches on that slot, so the surviving carried attachment contract hazard before `L88` first binds its own pipeline is honestly just a **single-slot `CLEAR` vs `LOAD` flip**.
- Because there are no same-recipe sibling slots in this pass (`single_slot_without_same_recipe_siblings`), this run cannot honestly pin that flip as an active-scope-only cohort rule or a carried-packet-only cohort rule; it only proves the slot-local carried-vs-active `load_op` difference.

## 2026-05-20 source-build follow-up — Task 106 (`oc-a4g`)

A second source-built follow-up stayed on the same host-Vulkan `projection_only__disabled` repro lane and added the smallest ownership-focused classifier on top of the existing slot-0 `load_op` diff. Instead of widening into new lanes, the instrumentation now records whether the surviving slot-0 mismatch can honestly be attributed to one side's attachment-family ownership or only to a narrower carried-vs-active slot-local mismatch.

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-slot0-load-op-ownership-vulkan-sourcebuild-20260520-211039/`

Validation command:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-slot0-load-op-ownership-vulkan-sourcebuild-20260520-211039 no_present compositor projection_only disabled 120`

Outcome:

- Repro still aborts at `submit_serial=9` (`exit_status=134`; see `stdout.log`, `stderr.log`, `exit_status.txt`).
- The carried render-pass exact-recipe delta is still narrowed to `load_op` only.
- The new ownership payload reports:
  - `load_op_ownership_classifier={classification="slot_local_carried_vs_active_mismatch_unattributed", best_explanation="carried_vs_active_slot_local_mismatch", basis="single_slot_mismatch_with_matching_non_load_recipe_and_no_same_recipe_siblings", slot_index=0, active_scope_reestablished_before_rebind=true, carried_packet_kept_live_before_rebind=true, family_hash_match=true, active_load_op="LOAD", pipeline_load_op="CLEAR", active_render_pass_create_serial="0xd", pipeline_render_pass_create_serial="0x9"}`

Interpretation:

- The active pre-rebind scope does re-establish its own compatible render pass before `L88` first rebinds (`active_render_pass_create_serial=13`), and the carried Tonemap packet also remains live across the zero-gap boundary (`pipeline_render_pass_create_serial=9`, `carried_packet_kept_live_before_rebind=true`).
- But because the exact surviving difference is still only one slot with a matching non-`load_op` family hash and no same-recipe sibling slots, the new classifier still **cannot honestly attribute slot-0 `load_op` ownership farther** than a carried-vs-active slot-local mismatch.
- So the best current explanation is **not** `carried_packet_owned_slot_0_contract` and **not** `active_pre_rebind_scope_owned_slot_0_contract`; it is the narrower `carried_vs_active_slot_local_mismatch` bucket.

## 2026-05-21 source-build follow-up — Task 120 (`oc-246`)

Instead of widening instrumentation again, this follow-up compared the already-locked Task 119 control vs. lazy-shared-view experiment artifacts directly, because those runs already contained the Tonemap/L88 pre-rebind payload needed to answer the narrower question.

Artifact roots:

- control: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-control-vulkan-sourcebuild-20260521-1411/`
- experiment: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-on-vulkan-sourcebuild-20260521-1410/`

What changed across those runs was only the shared-view policy/materialization lane contract:

- control: `lane_policy=persistent_root_sampled_shared`, `shared_view_materialized=true`
- experiment: `lane_policy=persistent_root_direct_lazy_shared_view`, `shared_view_materialized=false`

What stayed unchanged on the failing seam — and therefore remains the stronger crash-trigger candidate — was the Tonemap -> `L88` pre-rebind attachment/state contract:

- `boundary_state_handoff_classifier="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index"`
- `pre_rebind_carried_packet_contract="exact_pipeline_packet_with_compatible_only_render_pass_lineage"`
- surviving minimum hazard still narrows to `attachment_exact_recipe`, then to slot-0 `load_op`
- carried Tonemap packet still preserves `CLEAR` through `tonemap_end_load_op`, `l88_begin_load_op`, and `pre_rebind_pipeline_load_op`
- rebuilt active pre-rebind scope still flips that same slot to `pre_rebind_active_load_op="LOAD"`
- `ownership_side_classifier="boundary_crossing_interaction"` stays locked with `first_live_owner="tonemap_local_packet"`, `first_amplifier_owner="l88_local_packet"`, `first_meaningful_expansion="l88_label"`, and `full_four_bucket_interaction_locked=true`

Interpretation:

- The lazy/on-demand shared-view experiment successfully removed eager shared-view materialization from this lane without changing the submit-9 crash.
- That means shared-view materialization policy is no longer the leading trigger candidate on this locked repro lane.
- The stronger unchanged candidate is still the zero-gap Tonemap/L88 handoff where an exact carried Tonemap pipeline packet survives, `L88` re-establishes an active compatible-only render-pass scope, and the surviving exact recipe mismatch remains a single-slot `load_op` flip (`pipeline=CLEAR`, rebuilt active scope=`LOAD`) before the unchanged `fence_wait_error submit_serial=9 wait_result=-4` and later `BLIT_PASS`.

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

## 2026-05-26 documentation follow-up — Task 209 (`oc-h5t9`)

Stayed on the same preserved first-`L88` lane after Task 208 fixed the packet owner to the direct `HudLabel` `RichTextLabel` canvas item, and narrowed the next inward seam source-first instead of widening into more runtime changes.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-hudlabel-richtextlabel-emission-step-2026-05-26.md`

Concrete finding:

- the first clipped white no-outline rect packet inside the direct `HudLabel` owner path is best classified as a **direct RichTextLabel `_draw_line(...)` `DRAW_STEP_TEXT` glyph emission**, not a hidden internal owner, not a list-prefix helper, not `TextServer::shaped_text_draw(...)` on this route, and not an outline/shadow/background lane
- the exact direct emission call on this route is `TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)`
- source draw order plus the authored `HudLabel` content narrow the first matching body to the opening line-0 heading `[b]GDGS render-path tweak harness[/b]`

Next seam materialized by this stop point:

- if continuation is still wanted on the same preserved lane, split the newly fixed `line-0 heading -> DRAW_STEP_TEXT -> font_draw_glyph(...)` packet one rung deeper into the exact first heading glyph emission and/or its immediate `font_color` / `frid` selection, without reopening owner identity or broader theories

## 2026-05-26 — Task 210: split the fixed line-0 heading `DRAW_STEP_TEXT -> font_draw_glyph(...)` packet into first heading glyph emission versus immediate `font_color` / `frid` selection

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-glyph-vs-font-selection-2026-05-26.md`

Source-backed conclusion:

- the already-fixed direct branch remains `RichTextLabel::NOTIFICATION_DRAW -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)`
- the next honest inward split is the immediate draw-call input provenance, not stopping at the broader prose label of the first heading glyph
- `font_color` resolves directly through `_find_color(it, p_base_color)` to `theme_cache.default_color`, because the heading route has no `[color]`/`[fgcolor]` override and the default runtime RichTextLabel theme sets `default_color = Color(1, 1, 1)`
- the heading begins with `[b]...[/b]`, and RichTextLabel parses that with `_push_def_font(RTL_BOLD_FONT)`, so the `frid` lane for this packet is the shaped glyph RID coming from the heading's active bold-font selection path (`RTL_BOLD_FONT -> theme_cache.bold_font` and its effective size)
- the first visible heading glyph is still source-backed as the leading `G` of `GDGS render-path tweak harness`, but that is a slightly broader semantic label than the immediate draw-call input provenance itself
- the tighter rung for this slice is therefore the immediate `font_color` / `frid` split, with `font_color` already collapsed to default white and `frid` still one step less terminal because it remains the shaped RID outcome of the heading's bold-font selection lane

Validation for this documentation slice:

- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1350,1368p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1482,1492p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '3767,3779p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '5614,5624p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '3440,3466p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/theme/default_theme.cpp | sed -n '1221,1238p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scenes/gdgs_happy_path_control.tscn | sed -n '67,72p'`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-glyph-vs-font-selection-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

Exact next seam now materialized:

- if continuation is still wanted on the same preserved lane, stay on this immediate input split and classify the still-unfinished bold-font `frid` lane itself — for example, resolving the exact theme font/size path behind the heading's `[b]` selection — while keeping the already-collapsed white `font_color` side closed.

## 2026-05-26 documentation follow-up — Task 214 (`oc-ol8e`)

Stayed on the same preserved first-`L88` lane after Task 213 fixed the exact first heading shaped-glyph pair on the preserved runtime, and answered the remaining bridge question source-first instead of widening into another runtime trace.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-vs-first-clipped-packet-2026-05-26.md`

Concrete finding:

- the first preserved clipped white no-outline MSDF rect packet can now be pinned directly to the first shaped heading `G` emission on the fixed direct `HudLabel` RichTextLabel route
- no extra packet-bridge probe is needed, because `_draw_line(...)` already fixes first-line / first-step / first-glyph order, and the MSDF `font_draw_glyph(...)` implementation emits a single `draw_msdf_rect_region(...)` packet for that glyph on this route
- best narrow wording after this slice is:
  - `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> first visible heading glyph G -> preserved-runtime shaped pair (font_rid RID(687194767361), font_size 16, glyph index 42) -> TS->font_draw_glyph(...) -> single MSDF draw_msdf_rect_region(...) -> first preserved clipped packet`

Next seam materialized by this stop point:

- if continuation is still wanted on the same preserved lane, move to a genuinely new adjacent packet-local fact on the now-fixed first `G` packet (for example, a deeper geometric/clip relation), rather than reopening provenance links already closed

## 2026-05-26 documentation follow-up — Task 215 (`oc-lx21`)

Stayed on the same preserved first-`L88` lane after Task 214 fixed the exact first heading `G` packet identity, and classified the adjacent packet-local geometry instead of reopening provenance.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-packet-clip-geometry-2026-05-26.md`

Concrete finding:

- the exact first heading `G` packet is a **single-edge left clip** case, not a broader multi-edge clip or a renewed packet-identity ambiguity
- the preserved packet artifact already fixed `clip_rect={x=16,y=16,w=504,h=460}` and `command.rect={x=-1,y=3,w=14,h=16}` for this exact packet
- a tiny reversible headless probe confirmed the direct `HudLabel` anchor point as global `(16,16)` with local position `(0,0)` under `HudMargin`
- best narrow wording after this slice is:
  - `HudLabel anchor (16,16) + packet rect local (-1,3,14,16) -> packet global (15,19,14,16) against clip rect global (16,16,504,460) -> only the leftmost 1 px column is clipped`

Next seam materialized by this stop point:

- if continuation is still wanted on the same preserved lane, split inside this now-fixed clip geometry itself — for example, whether the `1 px` left overhang comes directly from the glyph's own MSDF rect/bearing versus an upstream line/layout offset, and/or the exact source of the packet's `y=3` inset

## 2026-05-26 documentation follow-up — Task 218 (`oc-rw5f`)

Stayed on the same preserved first-`L88` lane after Task 217 fixed the packet-local x/y inputs, and resolved the next honest clip-intersection rung instead of widening back out into broader provenance or fix theories.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-clip-overlap-fact-2026-05-26.md`

Concrete finding:

- the direct `HudLabel` `RichTextLabel` route is a real clip-owner route on the preserved runtime: `clip_contents=true`, `global_rect=(16,16,504,460)`, matching the preserved packet artifact's `clip_rect={x=16,y=16,w=504,h=460}`
- the fixed first heading `G` packet is translated from local `(-1,3,14,16)` to global `(15,19,14,16)` against that owner clip rect
- exact containment fails only on the left edge (`15 < 16`), while the right/top/bottom edges remain contained (`29 <= 520`, `19 >= 16`, `35 <= 476`)
- the surviving visible overlap is the exact intersection `(16,19,13,16)`, so the packet is not fully contained but still positively overlaps by `13x16` and remains drawable under the owner clip instead of disappearing
- the renderer-side preserve-rect gate still classifies the batch by `rect-like && current_batch->clip != nullptr && gdgs_canvas_blend_mode_uses_prior_color(...)`; this slice closes the deeper packet-local fact that makes the now-fixed `G` packet **actually clipped** on that preserved route rather than merely clip-capable
- best narrow wording after this slice is:
  - `owner clip rect (16,16,504,460)` vs `packet global rect (15,19,14,16)` -> `intersection (16,19,13,16)` -> non-empty partial-overlap, not full containment and not zero-overlap drop

Next seam materialized by this stop point:

- the immediate packet/clip intersection seam is now closed; any continuation on the same preserved lane would need a genuinely new adjacent fact rather than reopening already-closed owner/caller/glyph-local/clip-overlap steps

## 2026-05-26 documentation follow-up — Task 217 (`oc-r0qa`)

Stayed on the same preserved first-`L88` lane after Task 216 fixed the left overhang as glyph-local, and resolved the adjacent baseline-anchor rung behind the packet's local `y=3` inset instead of reopening broader provenance.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-y-inset-origin-2026-05-26.md`

Concrete finding:

- on the direct `HudLabel -> RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(...)` route, the first line's glyph anchor is built from the line ascent before the MSDF glyph rect-position term is applied
- a smallest reversible preserved-runtime probe fixed the exact metrics for the same preserved pair (`font_rid RID(687194767361)`, `font_size 16`, `glyph index 42`): `shaped_text_get_ascent(...) = 18`, `font_get_glyph_offset(...) = (-1,-15)`, `font_get_glyph_size(...) = (14,16)`, `font_baseline_offset = 0`, `font_spacing_top = 0`, `label.get_line_offset(0) = 0`
- that closes the local packet-y decomposition exactly: `18 + (-15) = 3`
- the stricter source-only tie is also now fixed: on the MSDF path `font_get_glyph_offset(...)` returns the same scaled `fgl.rect.position` term that `_font_draw_glyph(...)` adds into `cpos` before `draw_msdf_rect_region(...)`
- best narrow wording after this slice is:
  - `packet local y = 3` = `first-line shaped ascent 18` + `glyph-local MSDF rect-position y term -15`

Next seam materialized by this stop point:

- the immediate packet-local geometry seam is now closed; any continuation on the same preserved lane would need a genuinely new adjacent fact rather than reopening the already-resolved owner/caller/left-overhang/baseline-anchor steps

## 2026-05-26 documentation follow-up — Task 216 (`oc-zo3z`)

Stayed on the same preserved first-`L88` lane after Task 215 fixed the packet's 1-pixel left-edge clip geometry, and resolved the origin of that overhang rather than reopening broader packet provenance.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-left-overhang-origin-2026-05-26.md`

Concrete finding:

- the first heading `G` packet's `1 px` left overhang comes directly from the glyph's own MSDF rect/bearing, not from an upstream line/layout offset
- a preserved-runtime probe showed the first shaped glyph has `offset=(0,0)`, while `font_get_glyph_offset(...) = (-1,-15)` and `font_get_glyph_size(...) = (14,16)` for the same preserved-runtime pair (`font_rid RID(687194767361)`, `font_size 16`, `glyph index 42`)
- those glyph-local metrics line up exactly with the preserved packet-local rect's horizontal side: local packet `x=-1`, `w=14`
- best narrow wording after this slice is:
  - `first shaped glyph layout offset = (0,0)`, glyph-local bearing/rect = `(-1,-15)` with size `(14,16)`, packet local rect = `(-1,3,14,16)` -> left overhang is glyph-local, while `y=3` is the baseline-side draw anchor plus glyph-local vertical bearing

Next seam materialized by this stop point:

- if continuation is still wanted on the same preserved lane, move to the remaining adjacent geometry rung: the exact source-backed origin of the baseline-side draw anchor behind packet local `y=3` on the same first-`G` route, and/or a stricter source-only tie between `font_get_glyph_offset(...)` and internal `fgl.rect.position` for this MSDF glyph

## 2026-05-26 documentation follow-up — Task 215 (`oc-lx21`)

Stayed on the same preserved first-`L88` lane after Task 214 fixed the first clipped packet to the first heading `G` emission, and split the next seam into packet geometry versus clip state instead of reopening provenance.

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-g-packet-geometry-vs-clip-2026-05-26.md`

Concrete finding:

- the next honest adjacent packet-local fact is that the fixed first heading `G` packet is still submitted as a **full no-outline MSDF glyph quad**, while the visible clipping happens later through the owner-side `HudLabel` / `RichTextLabel` clip-owner scissor path
- source plus the preserved artifact show this cleanly:
  - packet still logs a full `command.rect` / `command.source` pair (`14x16` on both)
  - clipping is logged separately as `clip_rect={x=16,y=16,w=504,h=460}`
  - the draw path uses one `draw_msdf_rect_region(...)` packet for the glyph, then the batch re-establishes scissor from `current_batch->clip->final_clip_rect`
- best narrow wording after this slice is:
  - full packet first, owner-side scissor clip second

Next seam materialized by this stop point:

- if continuation is still wanted on the same preserved lane, split this geometry/clip relation one rung deeper — for example, classify whether the first visible clipping on the fixed `G` packet is best described by the packet's own destination-rect placement versus the later scissor boundary

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

## Follow-up QA pass for bead `oc-8ie` — inspect whether the first meaningful pass after the empty L15 wrapper is the L16 opaque scope

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-iml` added `depth_prepass_pass_scope=` to the Vulkan command summary. The question for this pass was whether the first meaningful surviving pass/container after the empty `Render Depth Pre-Pass (L15) (Draw)` wrapper stayed the neighboring `Render Opaque Pass (L16) (Draw)` scope, or whether some other non-label-owned pass-level seam appeared between them.

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `c7c0508b`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-depth-prepass-pass-scope-qa-vulkan-sourcebuild-20260518-172050/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

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

#### The first meaningful surviving pass/container after the empty L15 wrapper is still `Render Opaque Pass (L16) (Draw)`

The refreshed `depth_prepass_pass_scope=` payload on failing `submit_serial=9` reports:

- `scope_count=5`
- `target={index=0,owner_begin="Render Depth Pre-Pass (L15) (Draw)",owner_end="Render Depth Pre-Pass (L15) (Draw)",labels_started=0,draw_labels_started=0,label_indexes=none,levels=none,commands={render_pass_begin=1,next_subpass=0,render_pass_end=1,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,draw_indirect_calls=0,draw_indexed_indirect_calls=0,execute_secondary_calls=0,secondary_command_buffers=0,secondary_labels=0,secondary_draw_labels=0},first_backend_command="begin_render_pass",last_backend_command="end_render_pass",begin_breadcrumb="NONE",end_breadcrumb="NONE"}`
- `previous=none`
- `next={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",owner_end="Render Opaque Pass (L16) (Draw)",labels_started=0,draw_labels_started=0,label_indexes=none,levels=none,commands={render_pass_begin=1,next_subpass=0,render_pass_end=1,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,draw_indirect_calls=0,draw_indexed_indirect_calls=0,execute_secondary_calls=0,secondary_command_buffers=0,secondary_labels=0,secondary_draw_labels=0},first_backend_command="begin_render_pass",last_backend_command="end_render_pass",begin_breadcrumb="NONE",end_breadcrumb="NONE"}`

So the answer for bead `oc-8ie` is consistent with the earlier coder-side validation: after the empty `Render Depth Pre-Pass (L15) (Draw)` wrapper, the first surviving neighboring pass/container is still the `Render Opaque Pass (L16) (Draw)` scope. This rerun did **not** surface any intermediate non-label-owned pass-level seam between those two scopes.

#### The surviving seam remains wrapper-shaped rather than revealing a hidden pass-local payload between L15 and L16

The same run still shows:

- `depth_prepass_consumer_seam.draw_boundary_backend_attachment.consumer_class="render_pass_wrapper"`
- no direct label-local payload on the L15 wrapper
- no nested-scope payload under that exact wrapper
- no immediate downstream non-draw attachment after L15 before the next draw boundary

Combined with the new pass-scope summary, that means the widened pass/container view did not uncover an unseen pass-level bridge between L15 and L16. The first meaningful survivor after the exhausted L15 wrapper is simply the next opaque pass wrapper at L16.

#### Provenance and outer failure signature remain unchanged

The same valid repro keeps the established source-built baseline intact:

- `render_for_compositor_sync_snapshot` still stays stable before the frame-1 submit chain
- `submit_serial=8` is still the transfer-worker handoff with one signaled semaphore
- `submit_serial=9` still waits on that exact semaphore with `last_signal_submit_serial=8` and `last_wait_submit_serial=0`
- the explicit failure still first surfaces at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass closes the QA question for bead `oc-8ie`.

Supported by the runtime evidence:

- the exact `Render Depth Pre-Pass (L15) (Draw)` seam still resolves to an empty render-pass wrapper scope
- the widened pass-scope view does **not** reveal an intermediate pass-level seam between L15 and L16
- the first meaningful surviving neighboring pass/container after the empty L15 wrapper is still the adjacent `Render Opaque Pass (L16) (Draw)` scope
- both L15 and that neighboring L16 scope currently present as wrapper-shaped pass scopes rather than as direct payload-bearing labels in this command-summary slice

### Next recommendation

Keep the investigation source-built and projection-only, but stop expecting the exhausted L15 wrapper to reveal a hidden between-pass seam. The next useful slice should move either onto broader pass-scope ownership around the neighboring opaque-pass container or onto another backend attribution seam that survives outside these wrapper-only pass scopes while still leading into the later `submit_serial=9` / `BLIT_PASS` failure envelope.

---

## 2026-05-18 — bead `oc-7io` QA validation (`opaque_pass_scope=` on the refreshed source-built binary)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-qa-vulkan-sourcebuild-20260518-190101561855320/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-qa-vulkan-sourcebuild-20260518-190101561855320/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-qa-vulkan-sourcebuild-20260518-190101561855320/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-opaque-pass-scope-qa-vulkan-sourcebuild-20260518-190101561855320/exit_status.txt`

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

#### The new `opaque_pass_scope=` payload reproduces cleanly in QA on the refreshed source-built runtime

On the failing `submit_serial=9` command summary, QA observed the same five-scope package introduced by the coder pass:

- `opaque_pass_scope={scope_count=5,...}`
- `target={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",owner_end="Render Opaque Pass (L16) (Draw)",...}`
- `target_class="render_pass_wrapper"`
- `previous={index=0,owner_begin="Render Depth Pre-Pass (L15) (Draw)",...}`
- `next={index=2,owner_begin="Render 3D Transparent Pass (L86) (Draw)",...}`
- `wrapper_chain_from_target={start_index=1,end_index=2,scope_count=2,owner_begin_chain=["Render Opaque Pass (L16) (Draw)", "Render 3D Transparent Pass (L86) (Draw)"]}`
- `first_meaningful_at_or_after_target={distance_scopes=2,class="draw_payload",scope={index=3,owner_begin="Tonemap (L87) (Draw)",commands={render_pass_begin=1,render_pass_end=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,...}}}`

That means the opaque pass is still wrapper-only in the failing command-buffer slice, the wrapper chain still continues through `Render 3D Transparent Pass (L86) (Draw)`, and `Tonemap (L87) (Draw)` still lands as the first meaningful surviving pass-scope workload two scopes later.

#### This matches the earlier source-built pass-scope evidence instead of moving the seam

Compared against the immediately earlier source-built evidence package:

- the `depth_prepass_pass_scope=` slice still leaves `Render Depth Pre-Pass (L15) (Draw)` as a wrapper-shaped scope with `next={index=1,owner_begin="Render Opaque Pass (L16) (Draw)",...}`
- the new `opaque_pass_scope=` slice sharpens that same seam rather than changing it: L16 remains wrapper-only, and the first payload-bearing surviving pass scope still does not appear until `Tonemap (L87) (Draw)`
- no contradictory pass-scope ownership showed up in the QA rerun; this pass reproduced the coder-observed ownership map verbatim on a fresh Vulkan run

#### The outer failure envelope still remains unchanged

The same source-built projection-only repro preserved the established failure signature:

- `submit_serial=8` remains the transfer-worker handoff submission
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore
- the explicit failure still first appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This QA pass closes bead `oc-7io` with a complete evidence package.

Supported by the runtime evidence:

- `Render Opaque Pass (L16) (Draw)` remains an empty render-pass wrapper on the refreshed source-built binary
- the wrapper-only chain continues through `Render 3D Transparent Pass (L86) (Draw)`
- `Tonemap (L87) (Draw)` remains the first meaningful surviving pass-scope workload before the later `submit_serial=9` fence wait failure and `BLIT_PASS` collapse
- the source-built QA rerun agrees with the coder-observed `opaque_pass_scope=` payload and with the earlier `depth_prepass_pass_scope=` interpretation rather than shifting the suspicious seam earlier or later

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

## 2026-05-18 — bead `oc-set` QA validation (`tonemap_pass_scope=` on refreshed source-built binary)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-qa-vulkan-sourcebuild-20260518-20002814547/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-qa-vulkan-sourcebuild-20260518-20002814547/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-qa-vulkan-sourcebuild-20260518-20002814547/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-pass-scope-qa-vulkan-sourcebuild-20260518-20002814547/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

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

#### `Tonemap (L87) (Draw)` still stays the first meaningful surviving pass-scope seam

On the failing `submit_serial=9` command summary, QA observed:

- `tonemap_pass_scope={scope_count=5,...}`
- `target={index=3,owner_begin="Tonemap (L87) (Draw)",owner_end="Tonemap (L87) (Draw)",...}`
- `target_class="draw_payload"`
- `first_meaningful_scope_is_target=true`
- `previous_meaningful_before_target=none`
- `previous_wrapper_chain_into_target={start_index=0,end_index=2,scope_count=3,owner_begin_chain=["Render Depth Pre-Pass (L15) (Draw)", "Render Opaque Pass (L16) (Draw)", "Render 3D Transparent Pass (L86) (Draw)"]}`

So the refreshed source-built repro keeps the same pass-scope answer as the earlier source-built evidence: the surviving wrapper-only chain still runs through `L15 -> L16 -> L86`, and the first meaningful workload-bearing pass scope after that chain is still `Tonemap (L87) (Draw)` itself.

#### Tonemap still carries a small real local draw payload, and `Command Graph (L88) (Draw)` remains the next heavier meaningful scope

The same payload reports Tonemap’s direct local workload as:

- `direct_draw_calls=1`
- `direct_draw_indexed_calls=0`
- `pipeline_binds=1`
- `uniform_binds=1`
- `vertex_buffer_binds=0`
- `index_buffer_binds=0`
- `secondary_command_buffers=0`
- `begin_breadcrumb="NONE"`
- `end_breadcrumb="NONE"`

And the next meaningful surviving scope still lands immediately after Tonemap:

- `next_meaningful_after_target={distance_scopes=1,class="draw_payload",scope={index=4,owner_begin="Command Graph (L88) (Draw)",...}}`
- `Command Graph (L88) (Draw)` workload remains much heavier: `draw_calls=10`, `draw_indexed_calls=10`, `uniform_binds=11`, `vertex_buffer_binds=10`, `index_buffer_binds=1`, `begin_breadcrumb="UI_PASS"`, `end_breadcrumb="UI_PASS"`

So this rerun did not move the seam earlier or later. Tonemap still marks the first meaningful surviving pass scope, and `Command Graph (L88) (Draw)` still stays the next heavier meaningful scope for contrast.

#### The outer failure envelope remains unchanged

The same source-built projection-only repro preserved the already-established downstream failure signature:

- `submit_serial=8` remains the transfer-worker handoff submission
- `submit_serial=9` remains the failing frame-1 main submission waiting on that semaphore
- the explicit failure still first appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This QA pass closes bead `oc-set` with a complete refreshed evidence package.

Supported by the runtime evidence:

- `Tonemap (L87) (Draw)` still remains `target_class="draw_payload"`
- `first_meaningful_scope_is_target=true` still holds on the refreshed source-built binary
- the wrapper-only chain into Tonemap still covers `Render Depth Pre-Pass (L15) (Draw)` -> `Render Opaque Pass (L16) (Draw)` -> `Render 3D Transparent Pass (L86) (Draw)`
- `Command Graph (L88) (Draw)` still remains the next meaningful surviving scope and is materially heavier than Tonemap
- the seam did not move relative to the earlier `opaque_pass_scope=` / `tonemap_pass_scope=` source-built evidence; this rerun reproduced it cleanly on a fresh host-Vulkan pass

## Follow-up QA pass for bead `oc-x14` — confirm Tonemap local attachment is the first real surviving poisoned-boundary candidate on failing `submit_serial=9`

### Goal

Run the same minimum valid host-Vulkan source-built repro one more time, keep the investigation at `projection_only + disabled`, and answer the refined seam question introduced by the deeper Tonemap instrumentation:

- does `tonemap_local_attachment=` still show zero descendant payload?
- does `scope_alignment=` still show zero residual scope mismatch and exact label-to-scope ownership?
- does `next_meaningful_after_target=` keep `Command Graph (L88) (Draw)` in the downstream-amplification lane rather than displacing Tonemap as the first real poisoned-boundary candidate?

### Artifacts

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-local-attachment-qa-vulkan-sourcebuild-20260518-2104/`
- runtime path: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project path: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- staged harness: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Run executed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Launch used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/official-tonemap-local-attachment-qa-vulkan-sourcebuild-20260518-2104 no_present compositor projection_only disabled 120`

### Findings

#### `tonemap_local_attachment=` still shows a fully self-owned local Tonemap packet

On the failing `submit_serial=9` command summary, QA observed the same deeper Tonemap-local block introduced by the coder pass:

- `tonemap_local_attachment={label_index=100,entry_index=100,level=87,consumer_class="draw_payload",...}`
- `label_commands={render_pass_begin=1,next_subpass=0,render_pass_end=1,pipeline_binds=1,uniform_binds=1,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=1,draw_indexed_calls=0,...}`
- `nested_scope={descendant_labels=0,descendant_draw_labels=0,descendant_begin_inside_render_pass_labels=0,descendant_begin_inside_render_pass_draw_labels=0,... descendant_commands={render_pass_begin=0,next_subpass=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,...}}`
- `scope_alignment={scope_matches_label_commands=true,scope_matches_label_plus_descendants=true,scope_minus_label_plus_descendants={render_pass_begin=0,next_subpass=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,...}}`

That means the refreshed source-built repro still shows exactly what this bead was asked to verify:

- zero descendant payload
- zero residual scope mismatch
- exact label-to-scope ownership match for the Tonemap-local packet

So Tonemap still does **not** read like a hollow wrapper, inherited envelope, or mislabeled parent for hidden child work. On this failing submit it still owns the full local surviving packet itself.

#### `Command Graph (L88) (Draw)` still reads as downstream amplification, not the first surviving poisoned boundary

The same Tonemap block still carries:

- `next_meaningful_after_target={distance_scopes=1,class="draw_payload",scope={index=4,owner_begin="Command Graph (L88) (Draw)",...}}`
- `workload_delta_from_target={draw_calls=9,draw_indexed_calls=10,draw_indirect_calls=0,draw_indexed_indirect_calls=0,pipeline_binds=0,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}`
- `next_scope_is_heavier={draw_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,secondary_draw_labels=false}`

So `Command Graph (L88) (Draw)` is still the immediately following heavier meaningful scope, but the evidence still reads as *expansion after Tonemap*, not as a tighter first-cause seam that displaces Tonemap. Tonemap remains the first surviving meaningful local packet; `L88` remains the next heavier downstream payload that could amplify or reveal the poisoned state after the fact.

#### The outer failure envelope still did not move

The same repro kept the established source-built failure signature intact:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the failing frame-1 main submission waiting on that exact semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This QA pass closes bead `oc-x14` with the requested confirmation package complete.

Supported by the runtime evidence:

- `Tonemap (L87) (Draw)` still remains the first real surviving local poisoned-boundary candidate on failing `submit_serial=9`
- `tonemap_local_attachment=` still shows zero descendant payload and exact label-owned local work
- `scope_alignment=` still shows zero residual scope mismatch
- `Command Graph (L88) (Draw)` is still heavier one scope later, but it still reads as downstream amplification rather than a seam that displaces Tonemap as the first real surviving candidate
- the broader downstream envelope (`fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`) remains unchanged
## Follow-up QA pass for bead `oc-t58` — contrast Tonemap local ownership directly against `Command Graph (L88)` on failing `submit_serial=9`

### Goal

Run the refreshed source-built host-Vulkan staged repro again at `projection_only + disabled`, keep the repro envelope unchanged, and answer the new Tonemap-vs-L88 questions introduced by bead `oc-t58`:

- does the failing `submit_serial=9` payload now prove that `Command Graph (L88) (Draw)` is the very next meaningful scope after Tonemap?
- does the new same-pass contrast preserve Tonemap as the first self-owned poisoned-boundary candidate while making the heavier `L88` packet explicit?
- does `L88` still align as a self-owned local draw packet rather than exposing hidden descendant/render-pass-owned work that would invalidate the Tonemap-first read?

### Artifacts

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-t58-tonemap-l88-contrast/`
- runtime path: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- project path: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- staged harness: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- case: `projection_only__disabled`
- raster stage: `projection_only`
- checkpoint: `disabled`

### Run executed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Launch used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-t58-tonemap-l88-contrast no_present compositor projection_only disabled 120`

### Findings

#### `tonemap_l88_contrast=` now proves the same-pass handoff from Tonemap into the heavier `L88` packet

On the failing `submit_serial=9` command summary, QA observed the new contrast block introduced by bead `oc-t58`:

- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}`
- `tonemap_begin_state={render_pass_active=false,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,begin_breadcrumb="NONE"}`
- `l88_begin_state={render_pass_active=false,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,begin_breadcrumb="NONE"}`
- `tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,pipeline_binds=0,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,secondary_command_buffers=0,secondary_draw_labels=0}`
- `l88_is_heavier_than_tonemap={draw_calls=true,draw_indexed_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,secondary_draw_labels=false}`

So the new payload now nails down the intended contrast in one place:

- Tonemap is still the first meaningful surviving scope
- `Command Graph (L88) (Draw)` is exactly one meaningful scope later
- the `L88` packet is materially heavier on draws, indexed draws, uniform binds, vertex-buffer binds, and index-buffer binds
- the seam still reads as Tonemap first, then heavier downstream amplification at `L88`

#### `L88` still reads like a fully self-owned local draw packet, not hidden render-pass-owned descendant work

The same new block also observed:

- `l88_scope_summary={index=4,owner_begin="Command Graph (L88) (Draw)",owner_end="Command Graph (L88) (Draw)",... commands={render_pass_begin=1,... pipeline_binds=1,uniform_binds=11,vertex_buffer_binds=10,index_buffer_binds=1,draw_calls=10,draw_indexed_calls=10,...},begin_breadcrumb="UI_PASS",end_breadcrumb="UI_PASS"}`
- `l88_local_attachment={label_index=101,entry_index=101,level=88,consumer_class="draw_payload",...}`
- `label_commands={render_pass_begin=1,next_subpass=0,render_pass_end=1,pipeline_binds=1,uniform_binds=11,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,draw_calls=10,draw_indexed_calls=10,...}`
- `nested_scope={descendant_labels=0,descendant_draw_labels=0,... descendant_commands={render_pass_begin=0,next_subpass=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,...}}`
- `scope_alignment={scope_matches_label_plus_descendants=true,scope_minus_label_plus_descendants={render_pass_begin=0,next_subpass=0,render_pass_end=0,pipeline_binds=0,uniform_binds=0,vertex_buffer_binds=0,vertex_buffer_binding_total=0,index_buffer_binds=0,draw_calls=0,draw_indexed_calls=0,...}}`

So `L88` does **not** reveal hidden descendant payload or a broader render-pass-owned wrapper undercutting the current read. It is itself a self-owned local draw packet — just a much heavier one than Tonemap.

#### The surrounding failure envelope still did not move

The staged repro stayed stable:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This QA pass closes bead `oc-t58` with the requested Tonemap-vs-L88 contrast package complete.

Supported by the runtime evidence:

- `Tonemap (L87) (Draw)` still remains the first surviving self-owned poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` is now explicitly proven to be the very next meaningful scope one slot later
- `L88` is materially heavier than Tonemap, but it still reads as downstream amplification rather than a tighter first-cause seam
- both Tonemap and `L88` currently align as self-owned local packets with zero descendant payload and zero scope-vs-label-plus-descendants residual mismatch
- the broader downstream envelope (`fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`) remains unchanged


## 2026-05-18 — bead `oc-nw5` QA validation (`tonemap_l88_contrast=` on refreshed source-built binary)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-nw5-tonemap-l88-qa/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-nw5-tonemap-l88-qa/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-nw5-tonemap-l88-qa/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-18/oc-nw5-tonemap-l88-qa/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

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

#### `tonemap_l88_contrast=` stays stable and preserves the Tonemap-first read

On the failing `submit_serial=9` command summary, QA observed:

- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}`
- `tonemap_begin_state={render_pass_active=false,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,begin_breadcrumb="NONE"}`
- `l88_begin_state={render_pass_active=false,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,begin_breadcrumb="NONE"}`
- `tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,pipeline_binds=0,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,secondary_command_buffers=0,secondary_draw_labels=0}`
- `l88_is_heavier_than_tonemap={draw_calls=true,draw_indexed_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,secondary_draw_labels=false}`

That confirms the three questions for this pass cleanly:

- `next_meaningful_scope_is_l88=true` remains stable
- `scope_distance=1` remains stable
- `Tonemap (L87) (Draw)` still reads as the first self-owned poisoned-boundary candidate rather than being displaced by `Command Graph (L88) (Draw)`

#### `L88` still reads as heavier downstream amplification, not a seam that displaces Tonemap

The same payload still shows `L88` as a self-owned local packet rather than hidden descendant work:

- `l88_local_attachment.consumer_class="draw_payload"`
- `nested_scope={descendant_labels=0,descendant_draw_labels=0,...}`
- `scope_alignment={scope_matches_label_plus_descendants=true,scope_minus_label_plus_descendants={... all zero ...}}`

And the workload contrast still stays one-sided in the downstream direction:

- `l88_scope_summary.commands={render_pass_begin=1,render_pass_end=1,pipeline_binds=1,uniform_binds=11,vertex_buffer_binds=10,index_buffer_binds=1,draw_calls=10,draw_indexed_calls=10,...}`
- compared with Tonemap’s local packet: `pipeline_binds=1`, `uniform_binds=1`, `draw_calls=1`, `draw_indexed_calls=0`

So the refreshed source-built repro still supports the same interpretation as bead `oc-t58`: `Command Graph (L88) (Draw)` is the immediately following heavier local draw packet, but it still reads as downstream amplification after Tonemap rather than a tighter first surviving seam.

#### The outer failure envelope remains unchanged

The same valid repro keeps the established failure chain intact:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore lineage
- the explicit failure still first appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This pass closes bead `oc-nw5` with a refreshed source-built confirmation package.

Supported by the runtime evidence:

- `next_meaningful_scope_is_l88=true` remains stable
- `scope_distance=1` remains stable
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` still remains the next heavier self-owned local packet and still reads as downstream amplification rather than a seam that displaces Tonemap
- the broader downstream failure envelope (`fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`) remains unchanged

## 2026-05-19 — auditor addendum for bead `oc-kqh` (Tonemap local packet split)

Auditor re-checked the fresh Tonemap-local evidence against both the branch source and the saved artifact package:

- source change under audit: Godot commit `d9d83920` (`debug: split tonemap local packet`)
- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/`

Audit verdict:

- the new evidence **does** advance the seam inside the current first poisoned-boundary candidate
- it does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate
- it narrows the first surviving internal Tonemap seam to the combined **bind/setup** phase

Why that verdict holds:

- the artifact reports `local_packet_split.first_internal_expansion="bind_setup"`
- the recorded phase serials are contiguous: `begin_render_pass=7`, `setup_first=8`, `setup_last=9`, `draw_first=10`, `draw_last=10`, `end_render_pass=11`
- all inter-phase backend gaps stay zero: `begin_to_setup_backend_gap_commands=0`, `setup_to_draw_backend_gap_commands=0`, `draw_to_end_backend_gap_commands=0`
- the phase-command names align with the source instrumentation: `setup_first="bind_render_pipeline"`, `setup_last="bind_render_uniform_sets"`

Important caveat:

- this is still a **phase-bucket** classification, not a final one-command culprit
- the surviving setup bucket still contains two distinct backend commands at adjacent serials (`bind_render_pipeline` then `bind_render_uniform_sets`)
- so the evidence supports the current claim as written, but it does **not** yet prove which of those two setup commands is the tightest surviving sub-boundary

Recommended next seam:

1. split the Tonemap setup bucket itself
2. inspect `bind_render_pipeline` vs `bind_render_uniform_sets` as the next tightest backend-owned seam
3. only after that, revisit whether the later Tonemap draw at serial `10` still needs to be considered as the first visible amplifier rather than the first internal trigger

## Follow-up QA pass for bead `oc-bjx` — classify the exact Tonemap-to-L88 ownership transition on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-j8x` added `tonemap_end_state=` plus `tonemap_to_l88_transition=` inside `tonemap_l88_contrast=`. The question for this pass was whether the first meaningful ownership expansion after the self-owned Tonemap packet happens:

- exactly at `Command Graph (L88) (Draw)`
- at `UI_PASS`
- or at some smaller backend-owned transition in between

### Branch / worktree state used

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`
- Godot repo HEAD during the run: `6fa5aed4ce598bc378783b70ffa4851bc6669dda`
- GDGS repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs`

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Exact command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-l88-transition-vulkan-sourcebuild-20260519-055328 no_present compositor projection_only disabled 120`

### Findings

#### The first meaningful ownership expansion after Tonemap happens exactly at the `L88` label

On the failing `submit_serial=9` command summary, QA observed the new transition payload:

- `tonemap_end_state={render_pass_active=false,framebuffer_active=false,subpass=0,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,index_format=none,end_breadcrumb="NONE",backend_command_serial=11}`
- `l88_begin_state={render_pass_active=false,framebuffer_active=false,subpass=0,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,index_format=none,begin_breadcrumb="NONE",backend_command_serial=11}`
- `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,post_gap_first_backend_command=none,post_gap_last_backend_command=none,post_gap_last_breadcrumb="NONE",state_delta={render_pass_active=same,framebuffer_active=same,subpass=same,render_pipeline_bound=same,vertex_binding_count=same,index_buffer_bound=same,breadcrumb=same},first_meaningful_expansion="l88_label"}`

That answers the bead question directly:

- there is **no** smaller backend-owned transition between Tonemap and `Command Graph (L88) (Draw)`
- the breadcrumb does **not** jump to `UI_PASS` in the gap
- the first meaningful ownership expansion after Tonemap lands exactly at the `L88` local packet itself

#### This preserves the earlier Tonemap-first interpretation instead of displacing it

The same rerun still shows:

- `tonemap_local_attachment=` as a self-owned local Tonemap packet with zero descendant payload and exact scope alignment
- `l88_local_attachment=` as a heavier self-owned local draw packet
- `tonemap_to_l88_delta={draw_calls=9,draw_indexed_calls=10,pipeline_binds=0,uniform_binds=10,vertex_buffer_binds=10,vertex_buffer_binding_total=10,index_buffer_binds=1,...}`
- `l88_is_heavier_than_tonemap={draw_calls=true,draw_indexed_calls=true,pipeline_binds=false,uniform_binds=true,vertex_buffer_binds=true,index_buffer_binds=true,secondary_draw_labels=false}`

So the new transition evidence sharpens the seam without moving it: Tonemap still remains the first self-owned poisoned-boundary candidate, and `L88` still reads as the first downstream ownership expansion / amplification packet rather than a tighter seam that displaces Tonemap.

#### The outer failure envelope remains unchanged

The same valid repro keeps the established failure chain intact:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore lineage
- the explicit failure still first appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

#### Incidental runtime drift / nuisance observed

This rerun still emitted repeated Godot `vformat` formatting errors from the compositor callback breadcrumb text after `callback_return`, but they did not change the Tonemap-to-L88 transition answer or the failing submit classification.

### Updated interpretation

This pass closes bead `oc-bjx` with the requested transition answer.

Supported by the runtime evidence:

- the first meaningful ownership expansion after Tonemap happens exactly at `Command Graph (L88) (Draw)`
- there is no smaller backend-owned transition in the gap and no `UI_PASS` breadcrumb jump before `L88` begins
- Tonemap still remains the first self-owned poisoned-boundary candidate
- `L88` remains the next heavier self-owned local packet and still reads as downstream amplification / expansion rather than the earlier first-cause seam
- the broader downstream failure envelope (`fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`) remains unchanged

## Follow-up QA pass for bead `oc-19h` — classify the Tonemap local packet on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-5f9` added `local_packet_split=` inside `tonemap_local_attachment=`. The question for this pass was which internal Tonemap-owned boundary is the tightest surviving seam on failing `submit_serial=9`: render-pass begin, bind/setup, the single Tonemap draw, or render-pass end.

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Exact command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-local-packet-split-vulkan-sourcebuild-20260519-070538-rerun no_present compositor projection_only disabled 120`

### Findings

#### `local_packet_split=` resolves the tightest internal Tonemap seam to bind/setup

On the failing `submit_serial=9` command summary, QA observed:

- `local_packet_split={phase_counts={render_pass_begin=1,pipeline_binds=1,uniform_binds=1,draw_calls=1,render_pass_end=1}, ...}`
- `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,draw_first=10,draw_last=10,end_render_pass=11}`
- `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets"}`
- `phase_gaps={begin_to_setup_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`
- `first_internal_expansion="bind_setup"`
- `packet_shape={has_begin_render_pass=true,has_bind_setup=true,has_draw=true,has_end_render_pass=true}`

That is the direct classification result for this bead. The Tonemap-local packet is present end-to-end, but the first internal expansion away from the empty pre-pass begin state happens at the bind/setup phase, not at the single draw and not at render-pass end. The packet is also tightly packed: begin -> setup -> draw -> end are all contiguous backend serial steps with zero gap commands between each phase.

#### Tonemap still remains the first poisoned-boundary candidate

The refreshed source-built repro also preserved the earlier Tonemap-first ownership story:

- `tonemap_local_attachment=` still shows zero descendant payload and exact scope alignment
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` still holds
- `tonemap_to_l88_transition={backend_gap_commands=0,...,first_meaningful_expansion="l88_label"}` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`

So the new Tonemap-local split sharpens the internal seam without displacing Tonemap as the first real poisoned-boundary candidate. `Command Graph (L88) (Draw)` remains the next heavier downstream amplification packet, not a tighter first-cause seam.

#### Outer failure envelope and runtime nuisance remain unchanged

The same valid repro keeps the established downstream failure signature intact:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the failing frame-1 main submission waiting on that semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

Incidental runtime drift / noise observed again on this pass:

- repeated Godot `vformat` formatting errors from the compositor callback breadcrumb text appeared in the log before the failing submit chain

Those formatting errors were noisy but did not change the Tonemap-local classification or the failing-submit interpretation.

### Updated interpretation

This pass closes bead `oc-19h` with the requested Tonemap-local classification.

Supported by the runtime evidence:

- `local_packet_split.first_internal_expansion="bind_setup"`
- the tightest surviving internal Tonemap-owned seam is the bind/setup phase (`bind_render_pipeline` -> `bind_render_uniform_sets`), not render-pass begin, the single Tonemap draw, or render-pass end
- all three internal phase gaps (`begin->setup`, `setup->draw`, `draw->end`) stayed at zero backend commands on this repro
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` still remains the next heavier downstream local packet and still reads as amplification after Tonemap rather than a tighter first seam

## Follow-up QA pass for bead `oc-0nj` — classify the Tonemap bind/setup sub-boundary on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-apv` split the Tonemap bind/setup bucket itself. The question for this pass was whether the tightest surviving Tonemap setup sub-boundary lands exactly at `bind_render_pipeline`, at `bind_render_uniform_sets`, or only after both setup commands complete, while confirming whether Tonemap still remains the first poisoned-boundary candidate.

### Runtime used

QA used the refreshed source-built editor already present in the Godot worktree:

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/context.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Exact command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647 no_present compositor projection_only disabled 120`

### Findings

#### `setup_sub_boundary=` resolves the surviving Tonemap setup seam to pipeline bind first, then uniform bind

On the failing `submit_serial=9` command summary, QA observed the new split inside `tonemap_local_attachment.local_packet_split=`:

- `phase_serials={begin_render_pass=7,setup_first=8,setup_last=9,pipeline_first=8,pipeline_last=8,uniform_first=9,uniform_last=9,draw_first=10,draw_last=10,end_render_pass=11}`
- `phase_commands={setup_first="bind_render_pipeline",setup_last="bind_render_uniform_sets",pipeline_first="bind_render_pipeline",pipeline_last="bind_render_pipeline",uniform_first="bind_render_uniform_sets",uniform_last="bind_render_uniform_sets"}`
- `phase_gaps={begin_to_setup_backend_gap_commands=0,pipeline_to_uniform_backend_gap_commands=0,setup_to_draw_backend_gap_commands=0,draw_to_end_backend_gap_commands=0}`
- `first_internal_expansion="bind_render_pipeline"`
- `setup_sub_boundary="pipeline_then_uniform"`
- `setup_sequence={pipeline_before_uniform=true,uniform_before_draw=true,setup_complete_before_draw=true}`

That is the direct classification answer for bead `oc-0nj`. The surviving setup seam no longer collapses to an undifferentiated bind bucket: the first internal Tonemap expansion now lands **exactly at `bind_render_pipeline`**, then advances contiguously to `bind_render_uniform_sets`, and only after both setup commands does the single Tonemap draw execute. So the tightest surviving setup sub-boundary is **pipeline bind first**, not “uniform bind first” and not “only after both setup commands complete.”

#### Tonemap still remains the first poisoned-boundary candidate

The same rerun preserved the earlier Tonemap-first ownership story:

- `tonemap_local_attachment=` still shows zero descendant payload and exact scope alignment
- `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` still keeps `L88` in the downstream-amplification lane rather than displacing Tonemap

So this new split advances the seam *inside* Tonemap, but it does not move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`.

#### Outer failure envelope and runtime noise remain unchanged

The same valid repro keeps the established downstream failure signature intact:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the failing frame-1 main submission waiting on that semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

Incidental runtime noise persisted in this pass as well:

- repeated Godot `vformat` formatting errors still appeared from the compositor callback breadcrumb text

Those formatting errors were noisy but did not change the Tonemap setup-sub-boundary classification or the failing-submit interpretation.

### Updated interpretation

This pass closes bead `oc-0nj` with the requested Tonemap setup-sub-boundary classification.

Supported by the runtime evidence:

- `setup_sub_boundary="pipeline_then_uniform"`
- `first_internal_expansion="bind_render_pipeline"`
- the surviving Tonemap setup seam now resolves to `bind_render_pipeline` first, then `bind_render_uniform_sets`, with zero backend-gap commands between those setup commands and the later draw
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` still remains the next heavier downstream local packet and still reads as amplification after Tonemap rather than a tighter first seam

## 2026-05-19 — auditor addendum for bead `oc-ab8` (Tonemap bind/setup sub-boundary)

Auditor re-checked the fresh Tonemap bind/setup evidence against both the active branch source and the saved artifact package:

- source change under audit: Godot commit `5cc35d16` (`debug: split tonemap bind setup bucket`)
- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-sub-boundary-vulkan-sourcebuild-20260519-073647/`

Audit verdict:

- the new evidence **does** advance the seam inside the current first poisoned-boundary candidate
- it does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate
- it narrows the first surviving internal Tonemap seam from the old combined bind/setup bucket to **`bind_render_pipeline` first, then `bind_render_uniform_sets`**

Why that verdict holds:

- the artifact reports `setup_sub_boundary="pipeline_then_uniform"`
- the recorded phase serials are contiguous: `begin_render_pass=7`, `pipeline_first=8`, `uniform_first=9`, `draw_first=10`, `end_render_pass=11`
- all inter-phase backend gaps stay zero: `begin_to_setup_backend_gap_commands=0`, `pipeline_to_uniform_backend_gap_commands=0`, `setup_to_draw_backend_gap_commands=0`, `draw_to_end_backend_gap_commands=0`
- the phase-command names align with the source instrumentation: `pipeline_first="bind_render_pipeline"`, `uniform_first="bind_render_uniform_sets"`, and `first_internal_expansion="bind_render_pipeline"`

Important caveat:

- this is a **leading-command** classification, not a proof that the pipeline bind alone is already the unique causal instruction
- the surviving Tonemap setup packet is still contiguous across serials `8 -> 9`, so the uniform bind still remains the first downstream setup amplifier inside the same local packet

Recommended next seam:

1. inspect the pipeline-bind-owned seam inside Tonemap directly
2. only if no smaller backend-owned attachment appears around `bind_render_pipeline`, treat pipeline bind as the current tightest surviving seam with uniform bind as the first downstream amplifier
3. keep the Tonemap→L88 transition and the broader Tonemap-first ownership story as settled baselines unless new backend evidence directly contradicts them

## 2026-05-19 — bead `oc-0mx` QA validation (`pipeline_bind_seam=` on refreshed source-built binary)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Launch used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955 no_present compositor projection_only disabled 120`

### Findings

#### `pipeline_bind_seam=` now pins the tightest surviving Tonemap sub-boundary exactly at the pipeline bind itself

On the failing `submit_serial=9` command summary, QA observed the new payload inside `tonemap_local_attachment.local_packet_split`:

- `setup_sub_boundary="pipeline_then_uniform"`
- `pipeline_bind_seam={owned_attachment_class="pipeline_bind_direct_state_flip",pipeline_serial=8,uniform_serial=9,pipeline_to_uniform_backend_gap_commands=0,...}`
- `state_before={render_pass_active=true,framebuffer_active=true,subpass=0,render_pipeline_bound=false,vertex_binding_count=0,index_buffer_bound=false,index_format=none,breadcrumb="NONE"}`
- `state_after={render_pass_active=true,framebuffer_active=true,subpass=0,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,index_format=none,breadcrumb="NONE"}`
- `state_delta={render_pass_active=same,framebuffer_active=same,subpass=same,render_pipeline_bound=changed,vertex_binding_count=same,index_buffer_bound=same,index_format=same,breadcrumb=same}`
- `uniform_begin_state={render_pass_active=true,framebuffer_active=true,subpass=0,render_pipeline_bound=true,vertex_binding_count=0,index_buffer_bound=false,index_format=none,breadcrumb="NONE"}`
- `uniform_begin_matches_pipeline_after=true`

That answers the bead question cleanly:

- the smallest surviving Tonemap sub-boundary is **not** a backend-owned attachment between `bind_render_pipeline` and `bind_render_uniform_sets`
- it is **not** a seam that only appears once execution advances into uniform binding
- the surviving seam classifies as a direct label-owned state flip at `bind_render_pipeline` itself, with the following uniform bind entering the exact post-pipeline state and no intervening backend commands

#### The older `setup_sub_boundary=` interpretation still holds, but the new seam is tighter

Compared against the earlier 2026-05-19 `setup_sub_boundary=` run:

- the older answer said the first internal Tonemap setup expansion was `bind_render_pipeline`, followed contiguously by `bind_render_uniform_sets`
- the new answer keeps that ordering, but now proves the tighter surviving class is `owned_attachment_class="pipeline_bind_direct_state_flip"`
- `pipeline_to_uniform_backend_gap_commands=0` and `uniform_begin_matches_pipeline_after=true` demote both the “immediate backend-owned attachment after pipeline bind” theory and the “only poisoned once uniform binding begins” theory

So the classification moved from “pipeline then uniform” to the sharper answer “pipeline bind itself, with uniform bind as the first downstream contiguous amplifier inside the same local packet.”

#### Tonemap still remains the first self-owned poisoned-boundary candidate

The rest of the failing `submit_serial=9` payload stayed aligned with the already-settled Tonemap-first baseline:

- `tonemap_local_attachment.consumer_class="draw_payload"`
- `nested_scope={descendant_labels=0,descendant_draw_labels=0,...}`
- `scope_alignment={scope_matches_label_commands=true,scope_matches_label_plus_descendants=true,... all zero residual ...}`
- `next_meaningful_after_target={distance_scopes=1,class="draw_payload",scope.owner_begin="Command Graph (L88) (Draw)",...}`
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}`

That means the refreshed source-built repro still reads the same way at the broader boundary level:

- `Tonemap (L87) (Draw)` is still the first self-owned meaningful poisoned-boundary candidate
- `Command Graph (L88) (Draw)` remains the immediately following heavier downstream packet
- the new seam split advances *inside* Tonemap rather than displacing Tonemap with `L88`

#### The outer failure envelope remained stable

The same run preserved the already-established downstream failure signature:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

### Updated interpretation

This QA pass closes bead `oc-0mx` with the requested pipeline-bind seam classification complete.

Supported by the runtime evidence:

- `pipeline_bind_seam.owned_attachment_class="pipeline_bind_direct_state_flip"`
- the tightest surviving Tonemap sub-boundary now lives exactly at `bind_render_pipeline`
- there is no surviving immediate backend-owned attachment between pipeline bind and uniform bind (`pipeline_to_uniform_backend_gap_commands=0`)
- the uniform bind begins in the exact state produced by the pipeline bind (`uniform_begin_matches_pipeline_after=true`)
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` still remains the next heavier downstream packet rather than a tighter first seam

## 2026-05-19 — auditor addendum for bead `oc-x8s` (Tonemap pipeline-bind seam)

Auditor re-checked the fresh Tonemap pipeline-bind evidence against both the active branch source and the saved artifact package:

- source change under audit: Godot commit `7ff68572` (`debug: inspect tonemap pipeline bind seam`)
- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-bind-seam-vulkan-sourcebuild-20260519-090955/`

Audit verdict:

- the new evidence **does** advance the seam inside the current first poisoned-boundary candidate
- it does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate
- it narrows the tightest surviving internal Tonemap seam from `bind_render_pipeline`-then-`bind_render_uniform_sets` ordering to the **direct `bind_render_pipeline` state flip itself**

Why that verdict holds:

- the artifact reports `pipeline_bind_seam.owned_attachment_class="pipeline_bind_direct_state_flip"`
- the recorded setup serials stay contiguous: `pipeline_serial=8`, `uniform_serial=9`, `pipeline_to_uniform_backend_gap_commands=0`
- the tracked command state flips exactly once at pipeline bind: `state_before.render_pipeline_bound=false` and `state_after.render_pipeline_bound=true`
- the first following uniform bind begins in the exact post-pipeline state: `uniform_begin_matches_pipeline_after=true`
- the broader Tonemap-first evidence remains unchanged: `tonemap_local_attachment` still has zero descendant payload / exact scope alignment, `tonemap_l88_contrast.first_meaningful_expansion="l88_label"` still points to `Command Graph (L88) (Draw)` as the next downstream ownership expansion, and the outer failure envelope still runs `fence_wait_error submit_serial=9 wait_result=-4` -> later `BLIT_PASS`

What is proven vs. not yet proven:

- proven: the surviving smaller seam is no longer an unlabeled backend gap after pipeline bind and no longer something that first appears only when uniform binding begins
- not yet proven: which exact pipeline object / pipeline-owned state property bound at serial `8` is the toxic ingredient inside that surviving pipeline-bind-owned boundary

Next seam recommendation:

- inspect the **pipeline-bind-owned state provenance** itself at Tonemap serial `8` — e.g. which exact graphics pipeline object / compatibility context is bound there and how it differs from neighboring pass packets
- treat `bind_render_uniform_sets` as the first downstream amplifier, not the next tightest seam to reopen unless pipeline provenance evidence collapses

## Follow-up QA pass for bead `oc-899` — classify the Tonemap pipeline-state provenance on failing `submit_serial=9`

### Scope

Run the same minimum valid host-Vulkan source-built repro after bead `oc-kbk` added Tonemap pipeline provenance capture inside `pipeline_bind_seam=` plus `neighboring_pass_pipeline_compare=`. The question for this pass was:

- what exact pipeline-state provenance becomes live at Tonemap serial `8`?
- is that provenance unique to Tonemap, or does it only differ from neighboring pass packets at the compatibility/context level?
- does Tonemap still remain the first poisoned-boundary candidate after the new provenance data lands?

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Launch used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121 no_present compositor projection_only disabled 120`

### Findings

#### Tonemap serial `8` now resolves to a concrete `TonemapShaderRD:0` graphics pipeline provenance packet

On the failing `submit_serial=9` command summary, QA observed the new provenance fields inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`:

- `owned_attachment_class="pipeline_bind_direct_state_flip"`
- `pipeline_serial=8`
- `uniform_serial=9`
- `state_before.pipeline_provenance=none`
- `state_after.pipeline_provenance={pipeline_handle="0x61e17cfdfec0",pipeline_layout_handle="0x7f0990db06e0",render_pass_handle="0x61e17ce8dc10",render_subpass=0,shader_name="TonemapShaderRD:0"}`
- `uniform_begin_state.pipeline_provenance={pipeline_handle="0x61e17cfdfec0",pipeline_layout_handle="0x7f0990db06e0",render_pass_handle="0x61e17ce8dc10",render_subpass=0,shader_name="TonemapShaderRD:0"}`
- `uniform_begin_matches_pipeline_after=true`

That means the Tonemap bind no longer resolves only to an abstract `render_pipeline_bound=true` flip. The new live provenance at serial `8` is a specific Tonemap graphics pipeline object plus its concrete compatibility context:

- graphics pipeline handle `0x61e17cfdfec0`
- pipeline layout handle `0x7f0990db06e0`
- render pass handle `0x61e17ce8dc10`
- render subpass `0`
- shader name `TonemapShaderRD:0`

#### In this surviving slice, the provenance is effectively unique to Tonemap rather than merely “same context, different handle”

The new `neighboring_pass_pipeline_compare=` block answers the neighboring-pass question directly:

- `previous=none`
- `next={label="Command Graph (L88) (Draw)",pipeline_serial=13,relation="different_pipeline_different_compatibility_context",provenance={...,pipeline_provenance={pipeline_handle="0x7f098cf39dd0",pipeline_layout_handle="0x7f09a412f440",render_pass_handle="0x61e17ce8dc10",render_subpass=0,shader_name="CanvasShaderRD:0"}}}`

That classification matters in two ways:

1. **No previous meaningful neighboring pass packet contributes an earlier pipeline provenance.** The prior three pass scopes are still wrapper-only (`L15 -> L16 -> L86`), so there is no earlier surviving pass-local pipeline object to compare against at all.
2. **The next meaningful neighboring pass does not just reuse Tonemap’s compatibility context with a different pipeline handle.** `Command Graph (L88) (Draw)` shares the same render pass handle and subpass, but the comparison still classifies the relation as `different_pipeline_different_compatibility_context` because both the pipeline identity and the compatibility-defining layout/context packet differ (`pipeline_handle`, `pipeline_layout_handle`, and shader all change).

So the best exact read from this repro is:

- the first surviving live pipeline provenance appears exactly at Tonemap serial `8`
- it is a concrete Tonemap-owned `TonemapShaderRD:0` pipeline packet
- in this command-buffer slice it is **unique to Tonemap** rather than a reused/same-context neighboring pipeline with only superficial identity differences
- the next meaningful pass (`L88`) is a distinct downstream `CanvasShaderRD:0` packet that differs at both pipeline identity and compatibility/context level

#### Tonemap still remains the first poisoned-boundary candidate

The provenance expansion does not move the broader seam away from Tonemap. The same rerun preserved the already-settled Tonemap-first evidence:

- `tonemap_local_attachment.consumer_class="draw_payload"`
- zero descendant payload / exact scope alignment still hold
- `previous_wrapper_chain_into_target={... "Render Depth Pre-Pass (L15) (Draw)", "Render Opaque Pass (L16) (Draw)", "Render 3D Transparent Pass (L86) (Draw)" ...}` still demotes the earlier passes as wrappers
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` still keeps `L88` in the downstream-amplification lane
- `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first post-Tonemap ownership expansion exactly at `Command Graph (L88) (Draw)`

So the new provenance payload sharpens *what* becomes live at the Tonemap seam, but it does **not** displace Tonemap as the first surviving poisoned-boundary candidate.

#### The outer failure envelope remained stable, with the same incidental runtime noise

The same run preserved the broader failure signature:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

The previously seen compositor callback formatting noise also recurred:

- repeated `vformat` errors such as `Formatting error in string "[gdgs][godot] compositor callback batch begin ... view_count=%u": incomplete format.`
- repeated `unsupported format character` errors on the callback breadcrumb strings

Those log-hygiene issues were noisy but did not change the Tonemap provenance classification or the failing-submit interpretation.

### Updated interpretation

This QA pass closes bead `oc-899` with the requested Tonemap pipeline-state provenance classification complete.

Supported by the runtime evidence:

- Tonemap serial `8` makes a concrete `TonemapShaderRD:0` graphics pipeline packet live: pipeline `0x61e17cfdfec0`, layout `0x7f0990db06e0`, render pass `0x61e17ce8dc10`, subpass `0`
- the prior meaningful neighboring pipeline provenance is still absent (`previous=none`) because the earlier pass scopes remain wrapper-only
- the next meaningful neighboring pass packet (`Command Graph (L88) (Draw)`) is classified as `different_pipeline_different_compatibility_context`, not as a same-context reuse of Tonemap’s packet
- the provenance therefore reads as Tonemap-unique in this surviving slice rather than merely a compatibility-only delta against an already-live neighboring pass pipeline
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

## 2026-05-19 — auditor addendum for bead `oc-ju6` (Tonemap pipeline-state provenance)

Auditor re-checked the fresh Tonemap pipeline-state provenance evidence against both the active branch source and the saved QA artifact package:

- source change under audit: Godot commit `3b84ea95` (`debug: inspect tonemap pipeline state provenance`)
- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-provenance-vulkan-sourcebuild-20260519-102121/`

Audit verdict:

- the new evidence **does** advance the seam inside the current first poisoned-boundary candidate
- it does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate
- it supports the narrower statement that the first surviving live pipeline provenance at serial `8` is effectively unique to Tonemap **in this local surviving slice**

Why that verdict holds:

- the source now persists `DebugPipelineBindingProvenance` at `render_pipeline_create()`, latches it on `command_bind_render_pipeline()`, and copies it into the Tonemap seam snapshots used by `pipeline_bind_seam=`
- the artifact shows Tonemap serial `8` with a concrete packet: `pipeline_handle="0x61e17cfdfec0"`, `pipeline_layout_handle="0x7f0990db06e0"`, `render_pass_handle="0x61e17ce8dc10"`, `render_subpass=0`, `shader_name="TonemapShaderRD:0"`
- `uniform_begin_matches_pipeline_after=true`, so the same Tonemap packet survives from the pipeline bind into the uniform-bind start state
- `neighboring_pass_pipeline_compare=` shows `previous=none` and `next=Command Graph (L88) (Draw)` with a different packet: `pipeline_handle="0x7f098cf39dd0"`, `pipeline_layout_handle="0x7f09a412f440"`, same render pass/subpass, `shader_name="CanvasShaderRD:0"`
- because the layout handle changes while render pass/subpass stay the same, the reported relation `different_pipeline_different_compatibility_context` is supported by the actual packet fields

Important caveat:

- this is still a **slice-local** uniqueness result, not a proof that Tonemap’s pipeline is globally unique across every pass in the command buffer
- the current comparison window is only the first poisoned-boundary candidate versus its nearest meaningful surviving neighbor (`previous=none`, `next=L88`)

Recommended next seam:

1. keep the next backend split **inside Tonemap serial `8` itself**
2. treat `bind_render_pipeline` as the current tightest surviving backend-owned seam
3. treat `bind_render_uniform_sets` as the first downstream setup amplifier inside the same contiguous local packet unless a smaller bind-owned attachment is exposed around the pipeline bind itself

## 2026-05-19 — bead `oc-rea` QA validation (`bind_owned_attachment=` on refreshed source-built binary)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Exact command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044 no_present compositor projection_only disabled 120`

### Findings

#### `bind_owned_attachment=` exposes a smaller serial-8 seam than the old direct bind-state-flip classification

On the failing `submit_serial=9` command summary, QA observed the new block inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`:

- `owned_attachment_class="pipeline_bind_direct_state_flip"`
- `bind_owned_attachment={class="pipeline_packet_scope_mismatch",narrower_bind_owned_seam="pipeline_compatibility_context",active_scope_stable_across_bind=true,pipeline_packet_matches_active_scope=false,bind_delta_is_pipeline_only=true,...}`
- `active_scope_before={render_pass_handle="0x5e2991c8aff0",framebuffer_handle="0x5e2991ba1270",subpass=0}`
- `active_scope_after={render_pass_handle="0x5e2991c8aff0",framebuffer_handle="0x5e2991ba1270",subpass=0}`
- `pipeline_packet={render_pass_handle="0x5e29916b3630",render_subpass=0,pipeline_layout_handle="0x743e8c10dbd0",shader_name="TonemapShaderRD:0"}`
- `pipeline_to_uniform_backend_gap_commands=0`
- `uniform_begin_matches_pipeline_after=true`

That gives the bead answer directly:

- the surviving Tonemap seam is **not exhausted** at a pure direct `bind_render_pipeline` state flip
- the active pass scope stays stable across serial `8`
- the delta at the bind still remains pipeline-only
- but the newly-live Tonemap pipeline packet does **not** match the active render-pass scope
- the tighter surviving backend-owned seam is therefore the bind-owned **pipeline compatibility/context mismatch** itself, not an unlabeled backend gap after the bind and not a seam that first appears only when uniform binding begins

#### Tonemap still remains the first poisoned-boundary candidate

The broader source-built ownership story stayed unchanged in the same rerun:

- `tonemap_local_attachment.consumer_class="draw_payload"`
- zero descendant payload still holds
- exact `scope_alignment` still holds
- `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` still keeps `L88` in the downstream-amplification lane

So the new bind-owned attachment advances the seam *inside* Tonemap serial `8`, but it does **not** displace `Tonemap (L87) (Draw)` as the first surviving poisoned-boundary candidate.

#### The outer failure envelope remained stable, with the same incidental runtime noise

The same valid repro preserved the already-established downstream failure signature:

- `submit_serial=8` remains the transfer-worker handoff
- `submit_serial=9` remains the first failing frame-1 main submission waiting on that semaphore lineage
- the first explicit failure still appears at `fence_wait_error submit_serial=9 wait_result=-4`
- the later lost-device breadcrumb still collapses to `BLIT_PASS`

Incidental runtime noise persisted in this pass as well:

- repeated compositor callback `vformat` formatting errors still appeared in the log before the failing submit chain

Those formatting errors were noisy but did not change the bind-owned attachment classification or the failing-submit interpretation.

### Updated interpretation

This QA pass closes bead `oc-rea` with the requested bind-owned-attachment classification complete.

Supported by the runtime evidence:

- `bind_owned_attachment.class="pipeline_packet_scope_mismatch"`
- `bind_owned_attachment.narrower_bind_owned_seam="pipeline_compatibility_context"`
- the active pass scope remains stable across the Tonemap pipeline bind at serial `8`
- the newly-live `TonemapShaderRD:0` pipeline packet does **not** match that active render-pass scope, so the tighter surviving backend-owned seam is the pipeline-compatibility-context mismatch itself
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`
- `Command Graph (L88) (Draw)` still remains the next heavier downstream packet rather than a tighter first seam

## 2026-05-19 — auditor addendum for bead `oc-jdq` (Tonemap bind-owned attachment)

Auditor re-checked the fresh bind-owned-attachment evidence against both the active branch source and the saved QA artifact package:

- source commits checked: `e0489432` (`debug: inspect tonemap bind owned attachment`) plus the docs landing `8bfa11ed`
- artifact root checked: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-bind-owned-attachment-vulkan-sourcebuild-20260519-1044/`

Audit verdict:

- the new evidence does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate
- it **does** sharpen the serial-8 seam beyond the old bare `pipeline_bind_direct_state_flip` label by proving that the bind delta is still pipeline-only while the active pass scope stays stable across the bind
- but it only **partially** proves the stronger claim that the next seam is already a Tonemap-local `pipeline_compatibility_context` mismatch

What is directly supported by source + artifact:

- source now captures active render-pass / framebuffer handles inside the Tonemap pipeline-bind snapshots and emits `bind_owned_attachment={...}` from those captured states
- the artifact shows `active_scope_stable_across_bind=true`, `pipeline_packet_matches_active_scope=false`, and `bind_delta_is_pipeline_only=true`
- the artifact also shows the exact handle mismatch the new code is classifying: active scope handle `0x5e2991c8aff0` vs bound Tonemap pipeline packet render-pass handle `0x5e29916b3630`, both at subpass `0`

Important audit concern:

- the current `pipeline_packet_matches_active_scope` test is still a **raw render-pass-handle + subpass equality check**, not a richer Vulkan compatibility proof
- the same saved payload shows the immediately following `Command Graph (L88) (Draw)` packet running under that same active scope handle while carrying a pipeline provenance packet with the **same** pipeline render-pass handle `0x5e29916b3630`
- so the evidence currently proves a **pipeline-packet vs active-scope handle mismatch**, but it does **not** yet prove that the mismatch is uniquely Tonemap-local or that it names a true compatibility failure rather than a broader compatible-wrapper / provenance-alias pattern in this late pass lane

Recommended next seam:

1. stay inside Tonemap serial `8`
2. inspect the **active render-pass provenance / compatibility lineage** directly — why the live active scope handle differs from the bound pipeline packet handle, and whether those objects are intentionally compatibility-equivalent wrappers
3. compare that same lineage against `L88` (and, if needed, the nearest earlier surviving draw packet) before concluding that Tonemap’s tighter surviving seam is already a genuine compatibility-context break
4. if the handles turn out to be expected compatible aliases, demote this new mismatch classification and continue inward on the smaller pipeline-layout / pipeline-object-owned delta instead

Minor concern preserved for the record:

- repeated compositor callback `vformat` formatting errors remain noisy in the runtime logs, but they did not contradict the bind-owned-attachment evidence

## 2026-05-19 — bead `oc-hge` QA validation (active render-pass provenance at Tonemap serial `8`)

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/`

Key files:

- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/exit_status.txt`

### Exact run performed

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Exact command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656 no_present compositor projection_only disabled 120`

### Findings

#### Tonemap serial `8` now classifies as compatible render-pass aliasing / wrapping, not a broader incompatibility break

Inside `tonemap_pass_scope.target.tonemap_local_attachment.local_packet_split.pipeline_bind_seam.bind_owned_attachment`, QA observed:

- `class="direct_pipeline_bind_exhausted"`
- `pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility"`
- `active_scope_stable_across_bind=true`
- `pipeline_packet_matches_active_scope=false`
- `pipeline_packet_shares_compatible_scope=true`
- `bind_delta_is_pipeline_only=true`

The paired `scope_packet_lineage=` payload keeps the same compatibility hash / subpass-compatibility hash on both sides while the exact render-pass handle and exact hash differ:

- active scope: `render_pass_handle="0x56701a965130"`, `active_render_pass_create_serial=13`, `active_render_pass_exact_hash="0x5c19e291"`, `active_render_pass_compatibility_hash="0x425f4d3d"`, `active_render_subpass_compatibility_hash="0x81df0cad"`
- pipeline packet: `render_pass_handle="0x56701a3e8bd0"`, `pipeline_render_pass_create_serial=9`, `pipeline_render_pass_exact_hash="0x547d0135"`, `pipeline_render_pass_compatibility_hash="0x425f4d3d"`, `pipeline_render_subpass_compatibility_hash="0x81df0cad"`

So the raw Tonemap packet-vs-active-scope handle mismatch now reads as expected compatible render-pass aliasing/wrapping: different render-pass objects / exact recipes, but the same compatibility-context lineage for the active subpass.

#### The same active-scope relation pattern also appears on the next meaningful `L88` packet, but the Tonemap↔L88 packet comparison still stays narrower than a broad late-pass compatibility issue

The same saved payload shows the immediately following `Command Graph (L88) (Draw)` packet running under that same active scope with:

- `scope_pipeline_relation="different_handle_same_render_pass_compatibility"`
- `pipeline_provenance={pipeline_handle="0x729084180db0",pipeline_layout_handle="0x72909c2d4b60",render_pass_handle="0x56701a3e8bd0",render_pass_create_serial=9,render_pass_exact_hash="0x547d0135",render_pass_compatibility_hash="0x425f4d3d",render_subpass_compatibility_hash="0x81df0cad",render_subpass=0,shader_name="CanvasShaderRD:0"}`

That matters because it means Tonemap is **not** the only late surviving local packet whose bound pipeline packet points at a distinct-but-compatible render-pass object while the active scope stays on the later wrapper-owned handle. So the packet-vs-active-scope relation itself is a broader late-pass pattern shared by the next meaningful `L88` packet.

But the direct neighboring packet comparison still stays narrower than a broad compatibility-lineage break:

- `neighboring_pass_pipeline_compare.previous=none`
- `neighboring_pass_pipeline_compare.next.label="Command Graph (L88) (Draw)"`
- `neighboring_pass_pipeline_compare.next.relation="different_pipeline_different_compatibility_context"`

That comparison does **not** mean Tonemap and `L88` disagree about active-scope compatibility. It means the two self-owned packets are still materially different packets — different pipeline handle, different pipeline-layout handle, and different shader (`TonemapShaderRD:0` vs `CanvasShaderRD:0`) — even though both packets individually point at the same compatible render-pass lineage object (`create_serial=9`, compatibility hash `0x425f4d3d`, subpass-compatibility hash `0x81df0cad`) while the active scope stays on the later wrapper-owned handle (`create_serial=13`).

#### Earlier surviving draw labels before Tonemap still do not surface as comparable self-owned local payload packets

This rerun did **not** overturn the earlier pass-scope ownership story:

- `Render Depth Pre-Pass (L15) (Draw)` remains a wrapper-only pass scope
- `Render Opaque Pass (L16) (Draw)` remains a wrapper-only pass scope
- `Render 3D Transparent Pass (L86) (Draw)` remains a wrapper-only pass scope
- the first meaningful surviving local payload packet is still `Tonemap (L87) (Draw)`
- the next meaningful surviving local payload packet is still `Command Graph (L88) (Draw)`

So the new provenance answer is strongest on the Tonemap-vs-`L88` slice. There still is **not** an earlier surviving self-owned draw packet before Tonemap in this summary slice that would let QA claim the same local-packet relation pattern stretches back earlier than the wrapper chain.

#### Tonemap still remains the first poisoned-boundary candidate

The broader source-built envelope did not move in this rerun:

- `tonemap_pass_scope.target_class="draw_payload"`
- `first_meaningful_scope_is_target=true`
- `tonemap_local_attachment` still shows zero descendant payload and exact scope alignment
- `tonemap_l88_contrast={status=ok,scope_distance=1,next_meaningful_scope_is_l88=true,...}` still places `L88` exactly one meaningful scope later as a heavier downstream packet
- `fence_wait_error submit_serial=9 wait_result=-4` remains the first explicit failure site
- later lost-device breadcrumbs still collapse first to `BLIT_PASS`

So the new provenance/lineage fields narrow the old handle-mismatch caveat without displacing Tonemap. The best current read is:

1. Tonemap serial `8` is **not** a raw incompatible render-pass mismatch; it is a compatible aliasing/wrapping case.
2. That same packet-vs-active-scope compatibility pattern is also present on the next meaningful `L88` packet, so it is a broader late-pass wrapper/pipeline-lineage pattern rather than a Tonemap-only active-scope anomaly.
3. Tonemap still remains the first self-owned poisoned-boundary candidate because it is still the first meaningful surviving local payload packet, while `L88` remains the next heavier downstream amplification packet.

### Runtime noise / drift

This rerun again emitted repeated Godot callback `vformat` formatting errors before the failing submit chain. They were noisy but did not contradict the render-pass provenance / lineage classification above.

## Auditor addendum — 2026-05-19

Independent audit of commit `47799b38` and artifact `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-render-pass-provenance-qa-vulkan-sourcebuild-20260519-110656/` confirms the new provenance fields are wired through source and reflected faithfully in the saved run.

Key audit conclusions:

- Tonemap pipeline-bind serial `8` now has enough lineage data to demote the old render-pass caveat from “possible incompatible mismatch” to **expected compatible aliasing**:
  - active scope lineage: create serial `13`, exact hash `0x5c19e291`, compatibility hash `0x425f4d3d`, subpass compatibility hash `0x81df0cad`
  - Tonemap packet lineage: create serial `9`, exact hash `0x547d0135`, compatibility hash `0x425f4d3d`, subpass compatibility hash `0x81df0cad`
  - resulting relation: `different_handle_same_render_pass_compatibility`
- The same scope-vs-packet relation also appears on `Command Graph (L88) (Draw)` under the same active scope, so the raw handle split is not the tightest surviving seam.
- Tonemap still stays first in line as the poisoned-boundary candidate because it is the first self-owned late payload packet, but the next backend-owned inspection seam should remain **inside Tonemap’s local packet**, with the sharpest focus on:
  1. serial `8` `bind_render_pipeline` as the first owned state flip, and
  2. serial `9` `bind_render_uniform_sets` as the first immediate downstream setup amplifier.
- Remaining caution: this evidence narrows the cause; it does **not** yet prove whether serial `8` alone is sufficient, because the harmful setup may still require the contiguous `8 -> 9` Tonemap setup pair.

## 2026-05-19 — bead `oc-0cd` QA validation (Tonemap setup pair contract at serials `8 -> 9`)

Run the same minimum valid host-Vulkan source-built repro after bead `oc-aol` added `setup_pair_contract=` inside `tonemap_local_attachment.local_packet_split.pipeline_bind_seam=`. The question for this pass was whether the tightest surviving Tonemap seam now lives in the pipeline-owned state alone, the uniform-set-owned state alone, or the specific serial-`8 -> 9` handshake, while confirming whether Tonemap still remains the first poisoned-boundary candidate.

### Artifact

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/context.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924/exit_status.txt`

### Exact run

1. `projection_only + disabled` via the refreshed source-built editor on the host Wayland/Vulkan path — process abort / artifact exit code `134`

Command used:

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-setup-pair-contract-vulkan-sourcebuild-20260519-112924 no_present compositor projection_only disabled 120`

### `setup_pair_contract=` now classifies the surviving Tonemap seam as pipeline-owned state alone

On the failing `submit_serial=9` command summary, QA observed:

- `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",pair_contract_exact=true,...}`
- `pipeline_after_matches_uniform_before=true`
- `pipeline_layout_matches_bind_shader=true`
- `pipeline_shader_matches_bind_shader=true`
- `uniform_packet_matches_bind_shader=true`
- `contract_checks={all_sets_match_bind_shader_layout=true,all_sets_match_bind_shader_pipeline_layout=true,all_sets_match_bind_shader_name=true,all_sets_match_declared_set_index=true,...}`

That is the direct classification answer for bead `oc-0cd`. The serial-`8 -> 9` handshake is currently **clean and exact** on the failing submit: the uniform bind inherits the exact post-pipeline-bind state, the pipeline layout / shader match the uniform bind, and the descriptor-set packet matches the requested shader layout plus declared set indices. So the surviving Tonemap seam no longer reads as a cross-pair contract bug or as a uniform-owned-only break. The best current answer is that the next seam lives in the **pipeline-owned state alone** at serial `8`, with serial `9` reduced to a clean contiguous downstream amplifier.

### This preserves the earlier Tonemap-first read instead of displacing it

The same rerun kept the broader Tonemap story intact:

- `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility",pipeline_packet_shares_compatible_scope=true,bind_delta_is_pipeline_only=true,...}`
- `tonemap_to_l88_transition={...,first_meaningful_expansion="l88_label"}`
- `next_meaningful_after_target.scope.owner_begin="Command Graph (L88) (Draw)"`
- `fence_wait_error submit_serial=9 wait_result=-4`
- later lost-device breadcrumb still collapsed to `BLIT_PASS`

So this pass advances the seam one notch past the old “maybe the harmful setup requires the contiguous pair” uncertainty, but it does **not** move the first poisoned-boundary candidate away from `Tonemap (L87) (Draw)`. Tonemap still remains the first self-owned meaningful local packet, and the next meaningful downstream ownership expansion still begins exactly at `Command Graph (L88) (Draw)`.

### Runtime noise

This rerun again emitted repeated Godot callback `vformat` formatting errors before the failing submit chain. They were noisy but did not contradict the setup-pair classification or the broader failing-submit interpretation.

### Conclusion

- the serial-`8 -> 9` Tonemap setup-pair contract currently classifies as **clean/exact**, not as the surviving seam
- the exact classification result is `contract_class="pair_contract_clean"` with `next_seam_candidate="pipeline_owned_state"`
- the tightest surviving Tonemap seam therefore stays on the **pipeline-owned state** made live at serial `8`, not on uniform-owned state alone and not on the cross-pair handshake
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

## 2026-05-19 — bead `oc-i2z` QA validation (Tonemap pipeline-owned state packet at serial `8`)

Run the same minimum valid host-Vulkan source-built repro after bead `oc-8vs` added `graphics_recipe_hash`, recipe component hashes, and `recipe_delta.changed_components` to the Tonemap pipeline provenance lane. The question for this pass was whether Tonemap’s serial-`8` pipeline-owned packet narrows to one unique field/component hash or stays broader across multiple pipeline recipe buckets, while confirming whether Tonemap still remains the first poisoned-boundary candidate.

### Artifact package

- Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113/`
- Exact command: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113 no_present compositor projection_only disabled 120`
- Durable files: `context.txt`, `env.txt`, `exact_command.txt`, `stdout.log`, `exit_status.txt`
- Observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)

### Tonemap pipeline-owned packet classification

On the failing `submit_serial=9` command summary, the new pipeline-owned-state payload keeps the earlier narrowed seam intact and now classifies the surviving Tonemap packet contrast directly:

- `pipeline_bind_seam.state_after.pipeline_provenance={shader_name="TonemapShaderRD:0",graphics_recipe_hash="0x9c44278c",shader_stage_recipe_hash="0x2b93d818",vertex_input_recipe_hash="0xae289b01",rasterization_recipe_hash="0xa23144e0",multisample_recipe_hash="0xd5984934",depth_stencil_recipe_hash="0x1c6fa349",blend_recipe_hash="0x2d67c100",dynamic_state_recipe_hash="0xfa756ac3",specialization_constant_hash="0x208ccbee",...}`
- `neighboring_pass_pipeline_compare.previous=none`
- `neighboring_pass_pipeline_compare.next={label="Command Graph (L88) (Draw)",pipeline_serial=13,relation="different_pipeline_different_compatibility_context",...}`
- `neighboring_pass_pipeline_compare.next.recipe_delta={graphics_recipe_relation="different_graphics_recipe",graphics_recipe_hash_changed=true,changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"],...}`

That is the direct classification answer for bead `oc-i2z`. Tonemap’s serial-`8` graphics-pipeline packet is still the first surviving self-owned pipeline-owned state packet, and the nearest meaningful compatible late-pass comparison (`L88`) does **not** collapse the seam to a single field or one component hash. Instead, the surviving packet contrast stays broad across **multiple recipe buckets at once**:

1. `vertex_input_recipe`
2. `blend_recipe`
3. `specialization_constants`
4. `pipeline_layout`

Just as important, several other buckets stayed unchanged in the same comparison (`shader_stage_recipe_hash`, `rasterization_recipe_hash`, `multisample_recipe_hash`, `depth_stencil_recipe_hash`, `dynamic_state_recipe_hash`, plus the same compatible render-pass lineage). So the new instrumentation narrows the candidate set meaningfully, but the seam still does **not** collapse to one unique Tonemap-only field.

### Tonemap still remains the first poisoned-boundary candidate

The same rerun preserved the already-settled Tonemap-first ownership story:

- `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",pipeline_packet_scope_relation="different_handle_same_render_pass_compatibility",pipeline_packet_shares_compatible_scope=true,...}` still keeps the old render-pass-handle caveat demoted to compatible aliasing rather than a surviving smaller seam
- `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",pair_contract_exact=true,...}` still keeps the `8 -> 9` handshake clean, so the surviving seam remains pipeline-owned rather than uniform-owned or contract-owned
- `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still places the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`
- the outer failure envelope still stays stable on the same run: `fence_wait_error submit_serial=9 wait_result=-4` followed later by `BLIT_PASS`

So this pass sharpens **what about the Tonemap packet differs** from the next meaningful late-pass packet, but it still does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate.

### Runtime noise

This rerun again emitted repeated Godot callback `vformat` formatting errors before the failing submit chain. They were noisy but did not change the pipeline-recipe classification or the broader failing-submit interpretation.

### Conclusion

- Tonemap serial `8` still resolves to a concrete `TonemapShaderRD:0` pipeline-owned packet with `graphics_recipe_hash="0x9c44278c"`
- the nearest meaningful neighboring late-pass packet (`Command Graph (L88) (Draw)`) stays classified as `relation="different_pipeline_different_compatibility_context"`
- the surviving recipe contrast does **not** collapse to one field/component hash; it remains broad across `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout`
- several other recipe buckets remain unchanged in the same comparison, so the new seam is narrower than “everything about the pipeline,” but broader than a one-field proof
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

## 2026-05-19 — auditor addendum for bead `oc-cq6` (Tonemap pipeline recipe contrast)

Auditor re-checked the fresh source changes in `d42bd313` against the saved QA artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-recipe-qa-vulkan-sourcebuild-20260519-115113/`.

### Verdict

The new evidence is real progress, but it is still a **broad multi-bucket** Tonemap-local pipeline-owned seam rather than a one-field answer.

What the audit confirmed:

- source now records concrete pipeline recipe hashes and shape metadata at `render_pipeline_create()` and emits `recipe_delta=` in the Tonemap ↔ neighboring-pass comparison lane
- the QA artifact matches that instrumentation exactly on failing `submit_serial=9`
- Tonemap serial `8` still resolves to `TonemapShaderRD:0` with `graphics_recipe_hash="0x9c44278c"`
- the nearest meaningful downstream comparison is still `Command Graph (L88) (Draw)`
- that comparison still classifies as:
  - `relation="different_pipeline_different_compatibility_context"`
  - `graphics_recipe_relation="different_graphics_recipe"`
  - `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- the already-reduced seams remain reduced:
  - `bind_owned_attachment` still demotes the render-pass-handle mismatch to compatible aliasing
  - `setup_pair_contract` still keeps the serial-`8 -> 9` handshake clean
  - `tonemap_to_l88_transition.first_meaningful_expansion="l88_label"` still keeps Tonemap as the first poisoned-boundary candidate

### Recommended next seam

If the next pass stays inside the surviving changed-component set, inspect **`vertex_input_recipe` first**.

Why this is the tightest next backend-owned seam:

- `pipeline_layout` already looks less suspicious after the clean serial-`8 -> 9` contract proof
- `blend_recipe` and `specialization_constants` are still real deltas, but they stay broader shader-configuration buckets
- `vertex_input_recipe` is the most structural surviving difference in the Tonemap-vs-L88 contrast and should be easiest to instrument down to a necessary/sufficient backend-owned contract

### Remaining caution

This classification is strong for the Tonemap-vs-`L88` comparison slice, but it is not yet proof that any single changed component is uniquely toxic by itself.

## 2026-05-19 — bead `oc-lb4` QA validation (Tonemap vertex-input recipe seam at serial `8`)

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213/exit_status.txt`

### Exact run

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-vertex-input-qa-vulkan-sourcebuild-20260519-121213 no_present compositor projection_only disabled 120`

### What QA re-verified

- the refreshed source-built repro still fails on the same envelope: frame-1 main submission `submit_serial=9` queues, later hits `fence_wait_error ... wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate in `tonemap_pass_scope` and `tonemap_local_attachment`
- the downstream Tonemap -> `Command Graph (L88) (Draw)` transition still stays gap-free: `backend_gap_commands=0`, `gap_has_backend_commands=false`, `post_gap_backend_command_count=0`, and `first_meaningful_expansion="l88_label"`

### Vertex-input classification

- Tonemap pipeline-bind serial `8` still resolves to Tonemap-owned provenance with `shader_name="TonemapShaderRD:0"`
- Tonemap serial `8` still carries `vertex_input_class="null_vertex_input"` with:
  - `binding_count=0`
  - `attribute_count=0`
  - `binding_stride_total=0`
  - `binding_input_rate_mask="0x0"`
  - `attribute_location_mask="0x0"`
  - `attribute_binding_mask="0x0"`
  - no binding/attribute layout or format hashes
- the nearest meaningful compare at `Command Graph (L88) (Draw)` resolves to `vertex_input_class="instanced_vertex_input"` with:
  - `binding_count=1`
  - `attribute_count=8`
  - `binding_stride_total=128`
  - `binding_input_rate_mask="0x1"`
  - `attribute_location_mask="0xff00"`
  - `attribute_binding_mask="0x1"`
  - `binding_layout_hash="0x93ad1991"`
  - `attribute_layout_hash="0xad06536e"`
  - `attribute_format_hash="0x5eb7e4a9"`
- the new compare block classifies that exact delta as `vertex_input_delta={relation="null_vs_streamed_vertex_input", ... base_class="null_vertex_input", other_class="instanced_vertex_input" ...}`

### QA conclusion

- the instrumentation **does** sharpen the recipe story, but the remaining pipeline-owned seam still **does not collapse to a vertex-input-only boundary**
- the same `recipe_delta.changed_components` set remains broad: `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- so the correct classification is:
  - `vertex_input_delta` itself is a real Tonemap-vs-L88 contrast (`null_vertex_input` -> `instanced_vertex_input`)
  - but the overall surviving recipe seam still stays **broad multi-component**, not a narrowed vertex-input-owned poisoned boundary
- useful side read from the same artifact: the older pipeline-bind-owned lane stayed reduced rather than growing. `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}` and `setup_pair_contract={contract_class="pair_contract_clean",...}` still leave the Tonemap-owned packet isolated without turning vertex input into the sole surviving culprit

### Runtime noise

- repeated compositor callback `vformat` formatting errors still appeared in the log
- they did not prevent the repro or change the vertex-input classification

## 2026-05-19 — bead `oc-ifh` QA validation (Tonemap blend-recipe seam at serial `8`)

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/`
- context: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/context.txt`
- exact command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/exact_command.txt`
- stdout/log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/stdout.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655/exit_status.txt`

### Exact run

- `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-blend-qa-vulkan-sourcebuild-20260519-123655 no_present compositor projection_only disabled 120`

### What QA re-verified

- the refreshed source-built repro still fails on the same envelope: frame-1 main submission `submit_serial=9` queues, later hits `fence_wait_error ... wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate in `tonemap_pass_scope` and `tonemap_local_attachment`
- the downstream Tonemap -> `Command Graph (L88) (Draw)` transition still stays gap-free: `backend_gap_commands=0`, `gap_has_backend_commands=false`, `post_gap_backend_command_count=0`, and `first_meaningful_expansion="l88_label"`

### Blend classification

- Tonemap pipeline-bind serial `8` still resolves to Tonemap-owned provenance with `shader_name="TonemapShaderRD:0"`
- the new compare block now classifies the blend lane specifically as:
  - `blend_delta={relation="different_attachment_recipe_same_constants", ...}`
  - `attachment_recipe_hash_changed=true`
  - `constant_value_hash_changed=false`
  - `blend_enabled_attachment_mask_changed=true`
  - `dynamic_blend_constants_changed=false`
  - `uses_constant_factors_changed=false`
- Tonemap’s own blend recipe remains a no-blend packet:
  - `blend_enabled_attachment_mask="0x0"`
  - `blend_attachment_recipe_hash="0xb9952359"`
  - `blend_constant_value_hash="0xf90490b7"`
  - `blend_summary.first_active_attachment.blend_enable=false`
- the nearest meaningful compare at `Command Graph (L88) (Draw)` stays a blended neighbor with the same constant provenance but a different attachment recipe:
  - `blend_enabled_attachment_mask="0x1"`
  - `blend_attachment_recipe_hash="0x4e489967"`
  - `blend_constant_value_hash="0xf90490b7"`
  - `blend_summary.first_active_attachment={blend_enable=true,src_color=6,dst_color=7,color_op=0,src_alpha=1,dst_alpha=7,alpha_op=0,uses_constant_factors=false}`
- the live command state also stays clean for blend constants on the Tonemap packet:
  - `blend_constants={set=false,hash=none,last_set_serial=0,values=[0.0, 0.0, 0.0, 0.0]}`
  - no `set_blend_constants` activity appears between Tonemap bind serial `8` and the later failing `submit_serial=9`

### QA conclusion

- the instrumentation **does** narrow the surviving blend-owned bucket to a smaller classification: the Tonemap-vs-L88 contrast is now specifically an **attachment-recipe-only blend delta with shared constant provenance**, not a dynamic blend-constant or constant-factor seam
- however, the overall surviving pipeline-owned seam still **does not collapse to a blend-only poisoned sub-boundary**
- `recipe_delta.changed_components` remains broad: `["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- so the correct classification is:
  - `blend_delta` itself narrows to `different_attachment_recipe_same_constants`
  - but the surviving Tonemap-owned packet still remains a **broad multi-component pipeline-owned seam**, not an isolated blend-only culprit
- the already-reduced ownership lanes stay reduced rather than regrowing:
  - `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}`
  - `setup_pair_contract={contract_class="pair_contract_clean",...}`
- therefore `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

### Runtime noise

- repeated compositor callback `vformat` formatting errors still appeared in the log (`12` occurrences in this rerun)
- Wayland decoration/icon warnings appeared at startup
- none of that changed the blend classification or the broader failing-submit interpretation

## 2026-05-19 — bead `oc-76k` QA validation (Tonemap specialization-constant seam at serial `8`)

Run the same minimum valid host-Vulkan source-built repro after the refreshed branch added the new `specialization_delta={...}` payload to the Tonemap -> neighboring-pass comparison lane. The question for this pass was whether the surviving Tonemap -> `Command Graph (L88) (Draw)` seam now collapses specifically to specialization constants or still remains broad alongside `pipeline_layout` and other recipe buckets, while confirming whether Tonemap still remains the first poisoned-boundary candidate.

### Repro / artifact

- Binary: `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- Project: `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs`
- Harness: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd`
- Mode: `projection_only__disabled` with `no_present compositor projection_only disabled 120`
- Artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-specialization-seam-vulkan-sourcebuild-20260519-164911`
- Log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-specialization-seam-vulkan-sourcebuild-20260519-164911/stdout.log`
- Observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)

### Specialization-constant classification

On the failing `submit_serial=9` command summary, QA observed the new Tonemap -> `Command Graph (L88) (Draw)` packet comparison inside `tonemap_pass_scope.target.tonemap_local_attachment.local_packet_split.pipeline_bind_seam.neighboring_pass_pipeline_compare.next.recipe_delta=`:

- `relation="different_pipeline_different_compatibility_context"`
- `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- `specialization_delta={relation="different_specialization_recipe",hash_changed=true,id_hash_changed=true,value_hash_changed=true,count_changed=true,type_mask_changed=true,int_count_changed=true,min_id_changed=true,...}`
- Tonemap/base specialization packet:
  - `count=0`
  - `type_mask="0x0"`
  - `id_hash="0x208ccbee"`
  - `value_hash="0x208ccbee"`
  - `preview=[]`
- `L88`/other specialization packet:
  - `count=1`
  - `type_mask="0x2"`
  - `int_count=1`
  - `id_hash="0xc5247e53"`
  - `value_hash="0xc5247e53"`
  - `preview=[{id=0,type="int",bits="0x0",value=0}]`

That proves the specialization lane is now a real surviving Tonemap-vs-`L88` contrast rather than an uninstrumented unknown. But it does **not** collapse the seam to specialization constants alone. The same exact payload still reports a broad multi-bucket contrast at the same boundary:

- `vertex_input_recipe` still changes (`null_vertex_input` -> `instanced_vertex_input`)
- `blend_recipe` still changes (`different_attachment_recipe_same_constants`)
- `pipeline_layout` still remains in `changed_components`
- the overall packet relation still stays `different_pipeline_different_compatibility_context`

So the exact classification answer for bead `oc-76k` is:

1. specialization constants are a **confirmed surviving changed component** in the Tonemap -> `L88` contrast
2. the seam does **not** reduce to a specialization-only sub-boundary on this evidence package
3. the surviving Tonemap-owned packet remains a **broad multi-component pipeline-owned seam** spanning at least `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout`

### Tonemap still remains the first poisoned-boundary candidate

The same rerun preserved the already-settled Tonemap-first ownership story:

- `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",pair_contract_exact=true,...}` still keeps the surviving seam on serial-`8` pipeline-owned state rather than the serial-`8 -> 9` handshake
- `tonemap_to_l88_transition={backend_gap_commands=0,gap_has_backend_commands=false,post_gap_backend_command_count=0,first_meaningful_expansion="l88_label",...}` still keeps the first downstream ownership expansion exactly at `Command Graph (L88) (Draw)`
- `submit_serial=9` remains the first failing frame-1 main submission
- `fence_wait_error submit_serial=9 wait_result=-4` still remains the first explicit failure site

So the new specialization payload sharpens one more surviving recipe bucket, but it does **not** displace `Tonemap (L87) (Draw)` as the first poisoned-boundary candidate.

### QA verdict

- Tonemap serial `8` still resolves to the first surviving self-owned pipeline-owned packet on the failing `submit_serial=9` path
- the new specialization payload confirms a real Tonemap-vs-`L88` specialization contrast: Tonemap has zero specialization constants while `L88` carries one `int` specialization constant (`id=0`, `value=0`)
- however the same boundary still remains broad across multiple buckets, with `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- exact classification: **the remaining seam does not collapse to specialization constants; it remains a broad multi-bucket pipeline-owned seam that includes specialization constants alongside pipeline layout, vertex input, and blend recipe differences**
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

### Runtime noise

- the usual compositor callback `vformat` formatting noise still appeared in the log
- Wayland decoration/icon warnings still appeared at startup
- none of that changed the specialization classification or the broader failing-submit interpretation

## 2026-05-19 — bead `oc-bgd` QA validation (Tonemap pipeline-layout seam at serial `8`)

Run the same minimum valid host-Vulkan source-built repro after the refreshed branch added the new `pipeline_layout_delta={...}` payload to the Tonemap -> neighboring-pass comparison lane. The question for this pass was whether the surviving Tonemap -> `Command Graph (L88) (Draw)` seam now collapses specifically to pipeline layout or still remains broad alongside the other surviving recipe buckets, while confirming whether Tonemap still remains the first poisoned-boundary candidate.

### Artifact root and failure envelope

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-19/official-tonemap-pipeline-layout-qa-vulkan-sourcebuild-20260519-195448/`
- exact command saved at `exact_command.txt` under that root
- observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)
- the first failing frame-1 main submission still queues as `submit_serial=9`, later hits `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate in `tonemap_pass_scope` and `tonemap_local_attachment`
- the downstream Tonemap -> `Command Graph (L88) (Draw)` transition still stays gap-free: `backend_gap_commands=0`, `gap_has_backend_commands=false`, `post_gap_backend_command_count=0`, and `first_meaningful_expansion="l88_label"`

### Tonemap pipeline-layout classification

On the failing `submit_serial=9` command summary, QA observed the new Tonemap -> `Command Graph (L88) (Draw)` packet comparison inside `tonemap_pass_scope.target.tonemap_local_attachment.local_packet_split.pipeline_bind_seam.neighboring_pass_pipeline_compare.next.recipe_delta=`:

- `pipeline_layout_delta={relation="different_pipeline_layout_recipe",handle_changed=true,descriptor_set_layout_hash_changed=true,descriptor_set_layout_count_changed=false,first_descriptor_set_layout_handle_changed=true,last_descriptor_set_layout_handle_changed=true,push_constant_hash_changed=true,push_constant_range_count_changed=false,push_constant_stage_mask_changed=true,push_constant_total_offset_changed=false,push_constant_total_size_changed=true,...}`
- Tonemap/base pipeline-layout recipe:
  - `descriptor_set_layout_count=4`
  - `descriptor_set_layout_hash="0xb45ace77"`
  - `push_constant_hash="0x8f2faf58"`
  - `push_constant_stage_mask="0x10"`
  - `push_constant_total_size=112`
- immediate `L88` neighbor pipeline-layout recipe:
  - `descriptor_set_layout_count=4`
  - `descriptor_set_layout_hash="0x23ba2460"`
  - `push_constant_hash="0x9b5fef81"`
  - `push_constant_stage_mask="0x11"`
  - `push_constant_total_size=32`

That proves pipeline layout is a **real surviving changed recipe bucket** in the Tonemap-vs-`L88` contrast, and it is now structurally explained down to descriptor-set-layout lineage plus push-constant recipe drift. But it does **not** collapse the seam to pipeline layout alone. The same exact payload still reports a broad multi-bucket contrast at the same boundary:

- `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- the compare relation still stays `different_pipeline_different_compatibility_context`
- the already-reduced ownership lanes remain reduced rather than reopening:
  - `bind_owned_attachment={class="direct_pipeline_bind_exhausted",narrower_bind_owned_seam="none",...}`
  - `setup_pair_contract={contract_class="pair_contract_clean",next_seam_candidate="pipeline_owned_state",...}`

### Exact classification

- pipeline layout is now a **confirmed, structurally explained surviving bucket**, not an opaque handle-only drift
- however the remaining Tonemap -> `L88` seam still remains broad across multiple buckets, with `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]`
- exact classification: **the remaining seam does not collapse to pipeline layout; it remains a broad multi-bucket pipeline-owned seam that includes pipeline layout alongside vertex input, blend recipe, and specialization-constant differences**
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

### Runtime noise

- the usual compositor callback `vformat` formatting noise still appeared in the log (`12` occurrences)
- startup Wayland warnings still appeared (`XDG decoration manager`, `xdg-toplevel-icon`, and `FIFO protocol`)
- none of that changed the pipeline-layout classification or the broader failing-submit interpretation

## 2026-05-20 — bead `oc-91u` coder validation (Tonemap-vs-`L88` combined surviving-recipe interaction seam)

Run the same minimum valid host-Vulkan source-built repro after the refreshed branch added the new `surviving_bucket_interaction={...}` classifier to the Tonemap -> neighboring-pass recipe comparison lane. The narrow question for this pass was whether the surviving Tonemap -> `Command Graph (L88) (Draw)` seam can now be honestly described as a **combined interaction** of the remaining changed buckets — `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout` — rather than as one surviving bucket in isolation.

### Artifact root and failure envelope

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-combined-interaction-vulkan-sourcebuild-20260520-121325/`
- exact command saved at `exact_command.txt` under that root
- observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)
- the first failing frame-1 main submission still queues as `submit_serial=9`, later hits `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- `Tonemap (L87) (Draw)` still remains the first self-owned poisoned-boundary candidate in `tonemap_pass_scope` / `tonemap_local_attachment`
- the downstream Tonemap -> `Command Graph (L88) (Draw)` transition still stays gap-free: `backend_gap_commands=0`, `gap_has_backend_commands=false`, `post_gap_backend_command_count=0`, and `first_meaningful_expansion="l88_label"`

### Combined surviving-bucket interaction classification

On the failing `submit_serial=9` command summary, QA/coder observed the new Tonemap -> `Command Graph (L88) (Draw)` packet comparison inside `tonemap_pass_scope.target.tonemap_local_attachment.local_packet_split.pipeline_bind_seam.neighboring_pass_pipeline_compare.next.recipe_delta=`:

- `changed_components=["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]` still remains the exact surviving changed set
- the new `surviving_bucket_interaction={...}` block now reports:
  - `tracked_changed_bucket_count=4`
  - `full_combo_hash_changed=true`
  - `proper_subset_match_counts={size1=0,size2=0,size3=0}`
  - `largest_matching_proper_subset_size=0`
  - `minimal_distinguishing_changed_bucket_count=4`
  - `classification="full_four_bucket_interaction"`
  - `matching_largest_proper_subsets=[]`
  - `minimal_changed_bucket_candidates=[["vertex_input_recipe", "blend_recipe", "specialization_constants", "pipeline_layout"]]`

This is the first honest structural cut that answers the planning question directly. Within the locked Tonemap-vs-`L88` neighbor comparison, **no proper subset of the surviving buckets matches across the seam** — not any single bucket, not any pair, and not any 3-of-4 subset. So the surviving Tonemap -> `L88` recipe boundary is now best classified as a **full four-bucket interaction** rather than as a single-bucket survivor that just had not been named yet.

### Exact classification

- the seam still does **not** collapse to one isolated surviving bucket
- the new interaction classifier tightens the read beyond the older per-bucket passes: the smallest still-distinguishing structural delta at the locked Tonemap -> `L88` compare is the **entire four-bucket set together**
- exact classification: **the surviving poisoned-boundary contrast is currently a full four-bucket interaction across `vertex_input_recipe`, `blend_recipe`, `specialization_constants`, and `pipeline_layout` at the Tonemap -> `Command Graph (L88) (Draw)` seam**
- this remains a structural classification on the same failing `submit_serial=9` path, not yet a causal proof of which field inside that four-bucket packet is toxic
- `Tonemap (L87) (Draw)` still remains the first poisoned-boundary candidate on failing `submit_serial=9`

## 2026-05-20 — bead `oc-f9d` coder validation (pre-rebind carried Tonemap pipeline-packet contract at `L88`)

Stay on the same minimum valid host-Vulkan source-built repro after the refreshed branch added the new `pre_rebind_carried_packet_contract={...}` classifier inside the existing Tonemap -> `L88` boundary handoff lane. The narrow question for this pass was no longer which broad Tonemap-vs-`L88` recipe buckets diverge after rebinding — that was already locked — but which **carried Tonemap packet contract fields** remain live *before* `L88` performs its first pipeline rebind, and whether the minimum still-distinguishing pre-rebind hazard is exact packet identity/layout/push-constant carry or only the carried packet’s render-pass lineage.

### Result summary

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-contract-vulkan-sourcebuild-20260520-151506/`
- observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)
- the first failing frame-1 main submission still queues as `submit_serial=9`, later hits `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- the Tonemap -> `L88` handoff still stays gap-free: `backend_gap_commands=0`, `gap_has_backend_commands=false`, `post_gap_backend_command_count=0`, and `l88_pipeline_before_keeps_carried_packet=true`
- `L88` still re-establishes render-pass/framebuffer scope before its own first rebind: `l88_reestablishes_scope_before_own_pipeline=true`

### Pre-rebind carried packet classification

On the failing `submit_serial=9` command summary, coder observed the new `tonemap_pass_scope.target.tonemap_l88_contrast.pre_rebind_carried_packet_contract=` payload:

- `classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage"`
- `minimum_distinguishing_hazard="render_pass_compatibility_lineage_attached_to_carried_packet"`
- `pipeline_identity={relation="same_pipeline",exact=true,...}`
- `pipeline_layout_descriptor_contract={exact=true,layout_handle_match=true,descriptor_set_layout_hash_match=true,descriptor_set_layout_count_match=true,first_descriptor_set_layout_handle_match=true,last_descriptor_set_layout_handle_match=true,...}`
- `push_constant_range_contract={exact=true,hash_match=true,range_count_match=true,stage_mask_match=true,total_offset_match=true,total_size_match=true,...}`
- `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,...}`

That is the narrow answer the prior planning slice asked for. Before `L88` does its own pipeline bind, the carried Tonemap packet is still the **exact same pipeline object** with the **exact same pipeline-layout / descriptor-set-layout provenance** and the **exact same push-constant-range contract**. The only tracked field family that is no longer exact at that pre-rebind moment is the packet’s attached render-pass lineage: the active `L88` scope has a different render-pass handle and different exact render-pass hash, but it still matches the carried packet by render-pass compatibility hash, subpass compatibility hash, and subpass index.

### Conclusion

This narrows the remaining pre-rebind seam one step further without reopening older demoted lanes:

- the minimum still-distinguishing hazard **before** `L88` first pipeline rebind is **not** exact pipeline identity drift
- it is **not** pipeline-layout / descriptor-set-layout provenance drift
- it is **not** push-constant-range drift
- the smallest surviving pre-rebind contract hazard is the **carried packet’s render-pass compatibility lineage**: exact packet identity/layout/push constants are preserved, while the carried pipeline remains attached to Tonemap’s render-pass lineage and only matches `L88`’s re-established active scope at the compatibility level

### Runtime noise

- the usual compositor callback `vformat` formatting noise still appeared in the log
- startup Wayland warnings still appeared
- none of that changed the combined-interaction classification or the broader failing-submit interpretation

## 2026-05-20 — bead `oc-s5m` coder validation (smallest pre-rebind render-pass lineage field before `L88` first rebind)

Stay on the same minimum valid host-Vulkan source-built repro after the refreshed branch added one narrower render-pass-lineage cut inside the existing `pre_rebind_carried_packet_contract={...}` lane. The question for this pass was no longer whether the carried Tonemap packet vs rebuilt pre-rebind `L88` scope differs only by render-pass compatibility-lineage in the broad sense — that was already locked — but which **exact render-pass lineage field family** is still the minimum still-distinguishing contract hazard once compatibility-hash, subpass-compatibility, pipeline identity, layout provenance, and push-constant-range contract are all held fixed.

### Result summary

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-pre-rebind-lineage-vulkan-sourcebuild-20260520-180122/`
- exact command saved at `exact_command.txt` under that root
- observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)
- the first failing frame-1 main submission still queues as `submit_serial=9`, later hits `fence_wait_error submit_serial=9 wait_result=-4`, and the later lost-device breadcrumb still collapses to `BLIT_PASS`
- the Tonemap -> `L88` handoff still stays gap-free and still preserves the carried Tonemap packet up to `L88`’s first bind (`l88_pipeline_before_keeps_carried_packet=true`)

### New lineage-field classifier

The refreshed lane persists three exact-only render-pass lineage family hashes onto both the carried pipeline provenance and the active pre-rebind scope snapshot:

- `render_pass_attachment_exact_hash`
- `render_pass_dependency_hash`
- `render_pass_view_density_hash`

Those fields now feed a new `render_pass_lineage={field_classifier,minimum_distinguishing_field,...}` block nested inside `pre_rebind_carried_packet_contract=` on the failing `submit_serial=9` summary.

### Classification outcome

On the failing `submit_serial=9` command summary, coder observed:

- `pre_rebind_carried_packet_contract={classification="exact_pipeline_packet_with_compatible_only_render_pass_lineage",minimum_distinguishing_hazard="attachment_exact_recipe",...}`
- `render_pass_lineage={relation="different_handle_same_render_pass_compatibility",exact=false,compatible_only=true,field_classifier="attachment_exact_recipe_is_minimum_hazard",minimum_distinguishing_field="attachment_exact_recipe",...}`
- `compatibility_hash_match=true`
- `subpass_compatibility_hash_match=true`
- `attachment_exact_hash_match=false`
- `dependency_hash_match=true`
- `view_density_hash_match=true`
- `subpass_match=true`
- `subpass_count_match=true`
- `attachment_count_match=true`
- `dependency_count_match=true`
- `view_count_match=true`
- `fragment_density_usage_match=true`

This is the narrowest honest answer currently available on the locked pre-rebind lane. The carried Tonemap packet and rebuilt active `L88` scope still disagree on render-pass handle and exact render-pass recipe, but once the compatibility spine is held fixed, the remaining exact-only family split is **not** dependency lineage and **not** view-density lineage. The minimum still-distinguishing render-pass contract hazard is the **attachment exact recipe**.

### Exact conclusion

- exact pipeline identity still survives before `L88` rebinds
- exact pipeline-layout / descriptor-set-layout provenance still survives before `L88` rebinds
- exact push-constant-range contract still survives before `L88` rebinds
- exact render-pass dependency lineage also survives across the carried Tonemap packet vs rebuilt pre-rebind `L88` scope
- exact render-pass view-density lineage also survives across that same seam
- the smallest surviving render-pass-lineage hazard is therefore the **attachment exact recipe** attached to the carried Tonemap packet before `L88` first pipeline rebind

### Runtime noise

- the usual compositor callback `vformat` formatting noise still appeared in the log
- startup Wayland warnings still appeared
- none of that changed the attachment-exact-recipe classification or the broader failing-submit interpretation

## 2026-05-20 — bead `oc-sld` coder validation (minimum attachment exact-recipe field before `L88` first rebind)

Stay on the same minimum valid host-Vulkan source-built repro after the refreshed branch narrowed the carried Tonemap packet vs rebuilt pre-rebind `L88` scope seam all the way down to the render-pass attachment exact recipe. The question for this pass was no longer which broad render-pass lineage family survives — that was already locked to attachment exactness — but which **specific attachment exact-recipe field** is still the minimum still-distinguishing contract hazard once attachment count, compatibility lineage, dependency lineage, view-density lineage, pipeline identity, layout provenance, and push-constant-range contract are all held fixed.

### Result summary

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-attachment-exact-vulkan-sourcebuild-20260520-182834/`
- exact command saved at `exact_command.txt` under that root
- observed repro result: abort / exit `134` on the expected failing path (`submit_serial=9` -> later `BLIT_PASS`)
- the Tonemap -> `L88` handoff still stays gap-free and still preserves the carried Tonemap packet up to `L88`’s first bind (`l88_pipeline_before_keeps_carried_packet=true`)

### New attachment-field classifier

The refreshed lane now persists per-field attachment exact-recipe hashes onto both the carried pipeline provenance and the active pre-rebind scope snapshot for:

- `format`
- `samples`
- `load_op`
- `store_op`
- `stencil_load_op`
- `stencil_store_op`
- `initial_layout`
- `final_layout`

Those fields now feed a nested `attachment_exact_recipe={field_classifier,minimum_distinguishing_field,...}` block inside `pre_rebind_carried_packet_contract.render_pass_lineage=` on the failing `submit_serial=9` summary.

### Classification outcome

On the failing `submit_serial=9` command summary, coder observed:

- `attachment_exact_recipe={exact=false,field_classifier="load_op_is_minimum_attachment_exact_hazard",minimum_distinguishing_field="load_op",mismatch_count=1,mismatch_fields=["load_op"],...}`
- `attachment_count_match=true`
- `format_match=true`
- `samples_match=true`
- `load_op_match=false`
- `store_op_match=true`
- `stencil_load_op_match=true`
- `stencil_store_op_match=true`
- `initial_layout_match=true`
- `final_layout_match=true`

This is the narrowest honest answer currently available on the locked pre-rebind lane. The carried Tonemap packet and rebuilt active `L88` scope still disagree on render-pass handle and whole exact render-pass recipe, but once the seam is reduced only to attachment exactness, **every tracked attachment exact-recipe field matches except `load_op`**.

### Exact conclusion

- exact attachment count still survives across the carried Tonemap packet vs rebuilt pre-rebind `L88` scope
- exact attachment `format`, `samples`, `store_op`, `stencil_load_op`, `stencil_store_op`, `initial_layout`, and `final_layout` all survive across that same seam
- the **only** still-distinguishing attachment exact-recipe field is `load_op`
- the smallest surviving pre-rebind contract hazard is therefore the carried Tonemap packet’s attachment **`load_op` exactness** before `L88` first pipeline rebind

## Follow-up QA pass for bead `oc-vpw` — classify the active-scope rebuild recipe behind slot-0 `LOAD` before `L88`

### Scope

Stay on the same source-built host-Vulkan `projection_only + disabled` repro lane around failing `submit_serial=9`, then add the smallest render-pass/graph diagnostic needed to classify which active-scope reconstruction input turns slot 0 into `LOAD` before the first `Command Graph (L88) (Draw)` pipeline bind.

Branches / worktree state used:

- Godot repo branch: `gambit/instrumentation/2026-05-17-gdgs-compositor-breadcrumbs` @ `31d1308cd022`
- GDGS repo branch: local repro target unchanged for the locked `projection_only__disabled` case

### Runtime used

- `/home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64`
- launch path: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 --display-driver wayland --rendering-driver vulkan`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/stderr.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-20/official-tonemap-active-scope-rebuild-recipe-vulkan-sourcebuild-20260520-223754/exit_status.txt`

### Diagnostic added

Two debug-only traces were added in the owning Godot source tree:

- `servers/rendering/rendering_device_graph.h`
- `servers/rendering/rendering_device_graph.cpp`
- `drivers/vulkan/rendering_device_driver_vulkan.cpp`

They do two things:

1. record a per-attachment load-op source classifier when the draw-list render-pass recipe is assembled in `RenderingDeviceGraph`
2. print the actual active Vulkan render-pass scope begin packet so the graph-side recipe can be correlated to the scope that survives into `Tonemap` / `L88`

The relevant runtime lines from `stdout.log` were:

- `[gdgs-rdg] draw_list_render_pass_create key=0x0 render_pass_id=0x76a1166b32e8 framebuffer_id=0x5c63aa0e9ce0 label="Tonemap" breadcrumb=0 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,parent_write_index=-1,texture_usage=0x8b}]`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 render_pass_id=0x76a1166b32e8 framebuffer_id=0x5c63aa0e9ce0 owner_label="Tonemap (L87) (Draw)" owner_level=87 breadcrumb=NONE attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 compatibility_hash=0x425f4d3d`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 render_pass_id=0x76a1166b32e8 framebuffer_id=0x5c63aa0e9ce0 owner_label="Command Graph (L88) (Draw)" owner_level=88 breadcrumb=UI_PASS attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 compatibility_hash=0x425f4d3d`

### What this proves

The slot-0 `LOAD` does **not** first appear at `L88` itself. The active scope is already using the same `create_serial=13` Tonemap/L88-compatible render pass with slot 0 set to `LOAD`, and `L88` simply reuses that active scope.

The first attributable divergence is the graph-side load-op recipe selection for the Tonemap draw-list attachment:

- there is no `ATTACHMENT_OPERATION_CLEAR`
- there is no `ATTACHMENT_OPERATION_IGNORE`
- the attachment tracker is present and `is_discardable=false`
- the code therefore falls into the default `non_discardable_default_load_contract` branch and assigns `RDD::ATTACHMENT_LOAD_OP_LOAD`

In other words, the earliest exact recipe field/contract that flips slot 0 is the **scope-owned non-discardable attachment contract** in `RenderingDeviceGraph`, not any later pipeline rebind detail. The rebuilt active scope that survives into `L88` inherits `LOAD` from the Tonemap draw-list render-pass recipe because the tracked color attachment is treated as preserve/restore content rather than clear/discard content.

### Updated interpretation

This narrows the seam one level further than the earlier “active pre-rebind scope reconstruction” finding.

More precise statement now supported by direct evidence:

- the carried Tonemap pipeline packet still references the pipeline-side render pass recipe whose slot 0 is `CLEAR`
- the active scope that Tonemap actually begins on the command buffer is a different-but-compatible render pass whose slot 0 is `LOAD`
- that active scope is born at Tonemap render-pass begin from the graph-side recipe branch `non_discardable_default_load_contract`
- `Command Graph (L88) (Draw)` does not create a newer slot-0 `LOAD` variant; it enters the already-live Tonemap-owned `create_serial=13` scope and then performs its own pipeline/vertex/index rebind work inside it

So the first attributable divergence is now classified as: **RDG Tonemap draw-list slot 0 chose `LOAD` because the attachment tracker contract said “non-discardable target with no clear/ignore override.”**

## 2026-05-21 — Task 109 follow-up: split `non_discardable` attribution vs default-`LOAD` policy

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/stderr.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-nondiscardable-vs-default-load-vulkan-sourcebuild-20260521-082001/exit_status.txt`

### Diagnostic added

A smaller debug-only follow-up was added in the owning Godot source tree:

- `servers/rendering/rendering_device_graph.h`
- `servers/rendering/rendering_device_graph.cpp`
- `servers/rendering/rendering_device.cpp`

It does two narrowly targeted things on the same locked repro lane:

1. records discardable provenance for the tracker feeding the Tonemap draw-list attachment (`root_texture_create`, `shared_fallback_create`, `slice_tracker_create`, or explicit `texture_set_discardable_*` override)
2. annotates the `non_discardable_default_load_contract` branch itself as an unconditional policy (`default_non_discardable_policy="always_load_without_extra_per_attachment_split"`)

The relevant runtime lines from `stdout.log` were:

- `[gdgs-rdg] draw_list_render_pass_create key=0x0 render_pass_id=0x7077c26b32e8 framebuffer_id=0x6503122abcd0 label="Tonemap" breadcrumb=0 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,parent_write_index=-1,texture_usage=0x8b,discardable_provenance="root_texture_create",discardable_seed=false,non_discardable_basis="tracker_is_non_discardable",default_non_discardable_policy="always_load_without_extra_per_attachment_split"}]`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 render_pass_id=0x7077c26b32e8 framebuffer_id=0x6503122abcd0 owner_label="Tonemap (L87) (Draw)" owner_level=87 breadcrumb=NONE attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 compatibility_hash=0x425f4d3d`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 render_pass_id=0x7077c26b32e8 framebuffer_id=0x6503122abcd0 owner_label="Command Graph (L88) (Draw)" owner_level=88 breadcrumb=UI_PASS attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 compatibility_hash=0x425f4d3d`

### What this proves

This split resolves the branch one step deeper without reopening demoted lanes:

- the Tonemap slot-0 tracker is already a **root texture tracker** with `discardable_seed=false`
- there is no parent/slice/shared-fallback nuance in this lane (`tracker_has_parent=false`, `discardable_provenance="root_texture_create"`)
- once the code lands in the `non_discardable_default_load_contract` branch, there is **no further per-attachment policy split** for this case; the branch is just the unconditional default `LOAD`

So the surviving seam is best explained by **why slot 0 is classified as non-discardable**, not by any deeper Tonemap-specific explanation of why the default non-discardable branch picked `LOAD`. On this lane, the default branch is boring and global; the meaningful remaining ownership fact is that the Tonemap attachment reaches it as a root texture tracker seeded non-discardable.

### Updated interpretation

The smallest honest current statement is now:

- carried Tonemap pipeline packet still keeps slot 0 at `CLEAR`
- Tonemap’s active-scope render-pass recipe still flips slot 0 to `LOAD`
- that flip now cleanly decomposes into:
  - **attribution seam:** slot 0 arrived as a root tracker seeded `is_discardable=false`
  - **policy seam:** the non-discardable default branch is unconditional `LOAD`, with no narrower Tonemap-local sub-branch left to split here

So the best next question, if the lane continues, is no longer “why does default non-discardable choose `LOAD`?” The better question is: **who/what made this Tonemap attachment root tracker non-discardable in the first place?**

## Follow-up coder pass for bead `oc-7za` — trace the Tonemap root-tracker non-discardable seed before the `L88` scope rebuild

### Artifact roots

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-seed-vulkan-sourcebuild-20260521-0914/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-seed-vulkan-sourcebuild-20260521-0920/`

### What changed

The coder pass added one narrow RDG breadcrumb at root-tracker creation / reporting time:

- `tracker_name`
- `discardable_seed_contract`

It also tried a tiny runtime name assignment for render-target color attachments so the attachment owner would print as a name instead of a raw RID where possible.

### Locked result

On the same failing source-built host-Vulkan `projection_only__disabled` lane, the Tonemap render-pass attachment still reports:

- `tracker_has_parent=false`
- `discardable_provenance="root_texture_create"`
- `discardable_seed=false`
- `discardable_seed_contract="texture_format_is_discardable_flag"`
- `texture_usage=0x8b`

The runtime name did not resolve past `tracker_name="RID:8783208120351"` on this lane, but the usage bits do resolve the ownership seam statically and honestly:

- `0x8b = TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_CAN_COPY_FROM_BIT`
- that exactly matches `TextureStorage::render_target_get_color_usage_bits(false)` in `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`
- that render-target color contract is the upstream texture-creation contract applied before Tonemap’s active-scope render-pass recipe is rebuilt

### Conclusion

The Tonemap slot-0 root tracker is **not** being flipped non-discardable by a later pre-RDG rule. The earliest honest seam is:

1. an upstream render-target color usage/creation contract leaves `TextureFormat.is_discardable=false`
2. the root texture creation path copies that flag into the root tracker (`discardable_seed_contract="texture_format_is_discardable_flag"`)
3. the tracker therefore reaches Tonemap already classified as non-discardable, which is why the later active-scope rebuild falls into the unconditional non-discardable default-`LOAD` branch

So the seed is best described as **root texture creation materializing an upstream render-target contract**, not a later Tonemap-local or extra pre-RDG seed rule.

## 2026-05-21 — Task 111 follow-up: classify the exact upstream render-target color contract behind Tonemap slot-0 `is_discardable=false`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/stderr.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-contract-vulkan-sourcebuild-20260521-0945/exit_status.txt`

### Diagnostic added

One narrow debug-only breadcrumb was added in the owning Godot source tree:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It prints the exact render-target color `RD::TextureFormat` contract at the point where `TextureStorage::_update_render_target()` prepares the non-MSAA color attachment, including the usage bits and whether `is_discardable` was left at the struct default versus explicitly overridden.

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_color_format scope=non_msaa_color create_path=TextureStorage::_update_render_target usage_bits=0x8b is_discardable=false discardable_contract="texture_format_default_false_unset" discardable_basis="TextureFormat::is_discardable default remains false on non_msaa_color_path" resolve_buffer=false msaa=0`
- `[gdgs-rdg] draw_list_render_pass_create key=0x0 render_pass_id=0x7f9fa26b32e8 framebuffer_id=0x5789dc583ff0 label="Tonemap" breadcrumb=0 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,parent_write_index=-1,texture_usage=0x8b,tracker_name="RID:8783208120351",discardable_provenance="root_texture_create",discardable_seed=false,discardable_seed_contract="texture_format_is_discardable_flag",non_discardable_basis="tracker_is_non_discardable",default_non_discardable_policy="always_load_without_extra_per_attachment_split"}]`
- later failure remains the same lane: `fence_wait_error submit_serial=9 wait_result=-4`

### What this proves

This answers the remaining upstream-contract question narrowly and directly:

- the decisive choice is **not** hidden inside `render_target_get_color_usage_bits(false)` itself
- the decisive choice is **not** a later usage-family reinterpretation of `0x8b`
- the decisive choice is the **non-MSAA render-target color creation rule in `TextureStorage::_update_render_target()` leaving `RD::TextureFormat::is_discardable` untouched**, so the field stays at its struct default `false`
- root texture creation then honestly materializes that untouched `false` into the tracker via `discardable_seed_contract="texture_format_is_discardable_flag"`

The contrast inside the same function is useful and exact: the MSAA sibling path explicitly sets `rd_color_multisample_format.is_discardable = true`, but the non-MSAA color attachment path does not set `is_discardable` at all.

### Updated conclusion

The upstream contract behind Tonemap slot-0 non-discardability on this lane is now classified as:

**`TextureStorage::_update_render_target()` non-MSAA render-target color creation leaves `RD::TextureFormat::is_discardable` at the `TextureFormat` default `false`; `render_target_get_color_usage_bits(false)` only explains the matching `0x8b` usage family, not the non-discardable seed.**

So the earliest honest seam is now fully pinned:

1. non-MSAA render-target color creation builds a format with usage bits `0x8b`
2. that path leaves `TextureFormat::is_discardable` unset, so it remains default `false`
3. root texture creation copies that `false` into the tracker
4. Tonemap later inherits the already-non-discardable root tracker and therefore falls into the unconditional non-discardable default-`LOAD` branch

## 2026-05-21 — Task 112 follow-up: classify whether the non-MSAA render-target `is_discardable=false` contract is intentional or the surviving Tonemap bug seam

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-policy-vulkan-sourcebuild-20260521-0956/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-policy-vulkan-sourcebuild-20260521-0956/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-policy-vulkan-sourcebuild-20260521-0956/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-render-target-policy-vulkan-sourcebuild-20260521-0956/stderr.log`

### Diagnostic added

One additional debug-only ownership breadcrumb was added in the owning Godot source tree:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It does not widen the repro lane. It only prints the exact policy/ownership split that already exists in `TextureStorage::_update_render_target()`:

- non-MSAA `rt->color` is later shared back out through `texture_create_shared(...)` to the render-target root texture / optional sRGB view
- MSAA `rt->color_multisample` is an intermediate resolve source that resolves into `rt->color`

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_color_format scope=non_msaa_color create_path=TextureStorage::_update_render_target usage_bits=0x8b is_discardable=false discardable_contract="texture_format_default_false_unset" discardable_basis="TextureFormat::is_discardable default remains false on non_msaa_color_path" resolve_buffer=false msaa=0`
- `[gdgs-ts] render_target_color_policy scope=non_msaa_color owner=persistent_root_color policy_class=sampled_shared_root sampled_by_root_texture=true shared_to_render_target_texture=true shared_to_srgb_texture=true resolve_buffer=false msaa=0`
- `[gdgs-rdg] draw_list_render_pass_create ... label="Tonemap" ... source="non_discardable_default_load_contract" ... discardable_provenance="root_texture_create" discardable_seed=false discardable_seed_contract="texture_format_is_discardable_flag" ...`
- later failure remains the same lane: `fence_wait_error submit_serial=9 wait_result=-4`

The MSAA breadcrumb does not appear in this runtime because this locked repro lane is `msaa=0`, but the same source branch is still explicit in code:

- `// Render into our MSAA buffer and resolve into our color buffer.`
- `rd_color_multisample_format.is_discardable = true;`
- `[gdgs-ts] render_target_color_policy scope=msaa_color owner=msaa_intermediate policy_class=transient_resolve_source sampled_by_root_texture=false shared_to_render_target_texture=false resolve_destination=rt->color ...`

### What this proves

This narrows the intent question enough to classify the contract honestly:

- the non-MSAA path is **not** behaving like an orphaned transient attachment that Tonemap accidentally inherited
- on this lane, the non-MSAA color texture is the **persistent render-target root resource**: it is created first as `rt->color`, then reused as the backing store for `tex->rd_texture` / `tex->rd_texture_srgb` via `texture_create_shared(...)`
- the MSAA sibling path is the intentionally transient one: it only exists when `rt->msaa != VIEWPORT_MSAA_DISABLED`, is tagged `is_discardable=true`, and is documented in-code as the source that resolves into the persistent `rt->color`

So the exact condition split is:

1. **`msaa == disabled`** → only `rt->color` exists; it is the sampled/shared root render-target color texture, so the default `is_discardable=false` persists into the root tracker
2. **`msaa != disabled`** → `rt->color` still exists as the persistent resolve destination/root texture, while `rt->color_multisample` is the explicit transient MSAA intermediate and therefore gets `is_discardable=true`

### Updated conclusion

For this locked `submit_serial=9` lane, the non-MSAA `is_discardable=false` contract reads as an **intentional render-target ownership rule**, not as a newly isolated Tonemap-local bug seam by itself.

The surviving bug seam is narrower than “non-MSAA should have been discardable”: Tonemap is consuming a render-target root texture whose upstream creation policy intentionally treats it as persistent/shared root state. That persistent-root contract is what later drives the Tonemap slot-0 tracker into the default non-discardable `LOAD` branch.

## 2026-05-21 — Task 113 follow-up: classify wrong persistent-root lane vs wrong persistent-root policy for Tonemap at failing `submit_serial=9`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-lane-vulkan-sourcebuild-20260521-100508/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-lane-vulkan-sourcebuild-20260521-100508/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-lane-vulkan-sourcebuild-20260521-100508/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-lane-vulkan-sourcebuild-20260521-100508/stderr.log`

### Diagnostic added

A minimal debug-only Tonemap-lane breadcrumb was added in the owning Godot source tree:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.{h,cpp}`
- `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp`

It only classifies which render-target lane Tonemap actually binds for the already-locked repro shape (`direct render-target`, `intermediate/scaling`, `MSAA resolve`, or `override`), without reopening demoted packet or post-rebind lanes.

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_color_policy scope=non_msaa_color owner=persistent_root_color policy_class=sampled_shared_root sampled_by_root_texture=true shared_to_render_target_texture=true shared_to_srgb_texture=true resolve_buffer=false msaa=0`
- `[gdgs-ts] tonemap_render_target_lane submit_focus=submit_serial_9 path=tonemapper tonemap_dest={dest_is_msaa_2d=false,using_scaling_pass=false,use_smaa=false,can_use_storage=true} lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,lane_rationale=tonemap_direct_to_render_target_framebuffer,msaa=0,view_count=1,override_active=false,...}`
- the same run still fails on the same lane: `fence_wait_error submit_serial=9 wait_result=-4`

The already-locked pre-rebind seam evidence still matches the same run too:

- `pre_rebind_carried_packet_contract={... minimum_distinguishing_hazard="attachment_exact_recipe", ... attachment_exact_recipe={... minimum_distinguishing_field="load_op", ... active_load_ops=["0:LOAD"], pipeline_load_ops=["0:CLEAR"] ...}}`

### What this proves

This closes the lane-vs-policy fork cleanly for the current failing lane:

- Tonemap is **not** accidentally bound to a side lane, intermediate, override chain, or the MSAA transient lane here
- with `dest_is_msaa_2d=false`, `using_scaling_pass=false`, and `use_smaa=false`, Tonemap writes **directly** to the render-target framebuffer backed by the persistent non-MSAA root color (`rt->color`)
- that means the “wrong lane” hypothesis fails on this exact repro: Tonemap is on the same persistent-root direct lane that the owning render-target policy intentionally provides for the non-MSAA root texture
- the surviving mismatch therefore remains the **policy/contract** attached to that lane for this specific usage, not Tonemap choosing the wrong destination object

### Updated conclusion

For the locked failing `submit_serial=9` source-built host-Vulkan `projection_only__disabled` lane, Tonemap is bound to the **intended persistent-root direct render-target lane** (`rt->color`), so the surviving fork resolves to **persistent-root policy wrong for this specific Tonemap usage**, not “Tonemap picked the wrong persistent-root lane.”

More precisely: the lane binding is correct, but the active-scope recipe rebuilt for that lane still ends up with slot-0 `LOAD` while the carried Tonemap packet keeps slot-0 `CLEAR`, so the bug seam stays attached to the persistent-root lane’s exact attachment/load-op policy for this usage.

## 2026-05-21 — Task 114 follow-up: classify the exact cause of Tonemap’s persistent sampled/shared-root policy at failing `submit_serial=9`

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-policy-cause-vulkan-sourcebuild-20260521-101345/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-policy-cause-vulkan-sourcebuild-20260521-101345/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-policy-cause-vulkan-sourcebuild-20260521-101345/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-persistent-root-policy-cause-vulkan-sourcebuild-20260521-101345/stderr.log`

### Diagnostic added

One extra debug-only policy-classification line was added in the owning Godot source tree:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It stays on the already-locked lane and only prints the exact root-policy causes already visible in `TextureStorage::_update_render_target()`:

- whether the root color is immediately aliased into the render-target texture / sRGB shared view
- the exact non-MSAA usage-bit family on that root texture
- whether any explicit discardable override exists on the root lane

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_color_policy scope=non_msaa_color owner=persistent_root_color policy_class=sampled_shared_root sampled_by_root_texture=true shared_to_render_target_texture=true shared_to_srgb_texture=true resolve_buffer=false msaa=0`
- `[gdgs-ts] render_target_policy_cause scope=non_msaa_color owner=persistent_root_color primary_cause=render_target_texture_shared_view_rule specific_sampled_consumer_registration=false render_target_texture_shared_view=true srgb_shared_view=true shareable_formats={base=true,srgb=true} usage_bits=0x8b usage_family={sampling=true,color_attachment=true,copy_from=true,storage=true} discardable_override_on_root=false policy_contract="rt->color is the canonical backing store for eager shared viewport/render-target texture views; only the MSAA intermediate branch is explicitly discardable" creation_note="shared root texture is created so transparent can be supported"`
- `[gdgs-ts] tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,...}`
- `[gdgs-rdg] draw_list_render_pass_create ... label="Tonemap" ... source="non_discardable_default_load_contract" ... texture_usage=0x8b ... discardable_provenance="root_texture_create" discardable_seed=false ...`
- later failure remains unchanged on the same lane: `fence_wait_error submit_serial=9 wait_result=-4`

### What this proves

This resolves the requested fork narrowly and honestly:

- the policy is **not** being driven by a one-off downstream sampled-consumer registration discovered later in the graph
- the primary driver on this lane is the **render-target texture sharing/view ownership rule** in `_update_render_target()`: `rt->color` is created as the canonical non-MSAA root color, then immediately aliased into `tex->rd_texture` and optional `tex->rd_texture_srgb` via `texture_create_shared(...)`
- the code comment right on that path states why the aliasing exists: `create shared textures to the color buffer, so transparent can be supported`
- the non-MSAA root also carries the broad usage-bit family `0x8b` (`sampling + color_attachment + can_copy_from + storage`), which is consistent with that shared-root role, but those bits are **supporting capability requirements**, not the direct discardable switch
- the discardable split remains explicit elsewhere: only the MSAA sibling path (`rt->color_multisample`) gets `is_discardable=true`

So the best exact classification for this failing lane is:

1. **Primary cause:** render-target texture shared-view/root-ownership rule (`rt->color` must persist because it backs the render-target texture aliases)
2. **Secondary supporting requirement:** the non-MSAA root is provisioned with the broad sampled/storage/copy usage-bit family needed for that reusable root role
3. **Not the cause:** a late-discovered specific sampled/shared consumer registration

### Updated conclusion

For the locked failing Tonemap lane, the persistent sampled/shared-root treatment is driven primarily by **root render-target ownership plus eager shared texture/view aliasing**, not by a late consumer registration and not merely by usage bits in isolation.

In plain terms: Tonemap writes into `rt->color`, and `rt->color` is intentionally the persistent backing store for the render-target texture views Godot exposes and reuses. Because that root texture is not the transient MSAA intermediate, it never gets the explicit discardable override, so Tonemap inherits the root texture’s default non-discardable `LOAD` contract on this lane.

## 2026-05-21 — Task 115 follow-up: classify whether the transparent-support shared-view ownership is actually required on the failing Tonemap lane

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-transparent-shared-view-necessity-vulkan-sourcebuild-20260521-104055/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-transparent-shared-view-necessity-vulkan-sourcebuild-20260521-104055/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-transparent-shared-view-necessity-vulkan-sourcebuild-20260521-104055/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-transparent-shared-view-necessity-vulkan-sourcebuild-20260521-104055/stderr.log`

### Diagnostic added

A tiny debug-only usage-flow counter was added on the owning render target:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.h`
- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It does only three things:

- counts `render_target_get_texture()` requests on the owning `RenderTarget`
- prints that count in the existing `render_target_policy_cause` line
- prints the same count plus `transparent_bg` in the existing Tonemap lane summary

This stays exactly on the narrow fork in question: was the eager shared-view ownership needed here because of transparent support or an observed viewport/render-target texture consumer, or was it broader than the failing lane actually used?

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_policy_cause ... transparent_bg=false viewport_texture_requests=0 ... creation_note="shared root texture is created so transparent can be supported"`
- `[gdgs-ts] tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,...,shared_view_requirement={transparent_bg=false,viewport_texture_requests=0,requirement_class=eager_rule_without_observed_consumer},attachments={color=RID:...,render_target_texture=RID:...}}`
- later failure is still the same locked one: `fence_wait_error submit_serial=9 wait_result=-4`

### What this proves

This classifies the fork directly:

- on the failing Tonemap lane, the owning render target is **not** transparent (`transparent_bg=false`)
- up to the failing `submit_serial=9` Tonemap lane, there were **zero observed `render_target_get_texture()` requests** (`viewport_texture_requests=0`)
- yet the root is still forced into the sampled/shared-root policy because the shared render-target texture view is created eagerly in `_update_render_target()`

So for this exact repro lane, the transparent-support/shared-view ownership rule is **too broad for the observed usage**.

More precisely:

- the eager shared-view rule still explains why `rt->color` stays persistent/non-discardable
- but the failing Tonemap lane did **not** demonstrate an actual transparent-background requirement
- and it also did **not** demonstrate an observed viewport/render-target texture consumer before the failure

### Updated conclusion

For the locked failing `submit_serial=9` Tonemap lane, transparent-support shared-view ownership is **not shown to be required by this repro’s actual usage flow**. The current rule is broader: it eagerly upgrades the root to a persistent sampled/shared owner even when this lane is non-transparent and has no observed render-target texture requests before the crash.

## 2026-05-21 — Task 116 follow-up: classify the exact eager shared-view enable trigger on the failing Tonemap lane

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-trigger-vulkan-sourcebuild-20260521-121350/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-trigger-vulkan-sourcebuild-20260521-121350/exact_command.txt`
- log: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-trigger-vulkan-sourcebuild-20260521-121350/run.log`
- exit marker: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-trigger-vulkan-sourcebuild-20260521-121350/exit_status.txt`

### Diagnostic added

A tiny debug-only ownership-trigger breadcrumb was added on the owning render target:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.h`
- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It does only two new things:

- records the last `_update_render_target()` trigger reason on the owning `RenderTarget`
- prints that reason plus a monotonic generation counter when the root texture and its shared render-target views are recreated

This keeps the question narrow: **what exact event eagerly turns on the shared-view ownership for the failing Tonemap lane?**

### Relevant runtime lines

From `run.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_update_entry generation=1 reason=render_target_set_size size=1152x648 view_count=1 transparent_bg=false use_hdr=false msaa=0 override_active=false`
- `[gdgs-ts] render_target_policy_cause ... trigger_reason=render_target_set_size trigger_generation=1 ... transparent_bg=false viewport_texture_requests=0 ...`
- `[gdgs-ts] render_target_update_entry generation=2 reason=render_target_set_size size=2304x1296 view_count=1 transparent_bg=false use_hdr=false msaa=0 override_active=false`
- `[gdgs-ts] render_target_policy_cause ... trigger_reason=render_target_set_size trigger_generation=2 ... transparent_bg=false viewport_texture_requests=0 ...`
- there are **no** `render_target_set_transparent`, `render_target_set_use_hdr`, or `render_target_set_msaa` trigger entries before the same later failure at `fence_wait_error submit_serial=9 wait_result=-4`

### What this proves

This narrows the ownership trigger exactly:

- the eager shared render-target views are **not** first enabled by a transparent-background mutation on this lane
- they are **not** first enabled by an observed render-target texture request on this lane
- they are **not** waiting on a later sampled-consumer registration
- instead, the root/shared ownership is turned on by the normal **render-target size allocation path**: once `render_target_set_size()` gives the RT a non-zero extent, `_update_render_target()` allocates `rt->color` and immediately creates the shared render-target texture aliases from it

So the correct classification for this failing lane is:

- **not** a special transparent-only trigger
- **not** a late consumer-driven trigger
- **not** a separate creation-time `render_target_create()` policy in isolation
- **yes:** an **unconditional shared-view allocation path inside `_update_render_target()` that is reached from `render_target_set_size()` for ordinary RT allocation**

### Updated conclusion

For the locked failing `submit_serial=9` Tonemap lane, the exact eager-enable condition is the first non-zero-size render-target allocation/update path. In practice, `RendererViewport::_viewport_set_size()` calls `render_target_set_size()`, which calls `_update_render_target()`, which unconditionally creates the shared render-target texture views from `rt->color` on that path. That is why Tonemap still lands on `lane_policy=persistent_root_sampled_shared` even though this repro shows `transparent_bg=false` and `viewport_texture_requests=0` before the crash.

## 2026-05-21 — Task 117 follow-up: classify whether eager shared-view creation is the surviving seam or a required Tonemap invariant

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-invariant-vulkan-sourcebuild-20260521-124956/`

Key files:

- command: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-invariant-vulkan-sourcebuild-20260521-124956/exact_command.txt`
- stdout: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-invariant-vulkan-sourcebuild-20260521-124956/stdout.log`
- stderr: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-invariant-vulkan-sourcebuild-20260521-124956/stderr.log`
- exit status: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-eager-shared-view-invariant-vulkan-sourcebuild-20260521-124956/exit_status.txt`

### Diagnostic added

A tiny debug-only access-counter extension was added on the owning render target:

- `servers/rendering/renderer_rd/storage_rd/texture_storage.h`
- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp`

It records only the narrow ownership question needed for this fork:

- how many times the user-facing shared render-target texture was requested (`render_target_get_texture()`)
- how many times Tonemap / late render code accessed the direct root attachments instead (`render_target_get_rd_framebuffer()`, `render_target_get_rd_texture()`, `render_target_get_rd_texture_slice()`, `render_target_get_rd_texture_msaa()`)
- a lane-local classifier printed in the existing `tonemap_render_target_lane` line

This keeps the fork honest: **does the failing Tonemap lane itself require the eager shared view, or does it actually run against the root attachment path while the shared view stays unobserved?**

### Relevant runtime lines

From `stdout.log` on the same failing source-built host-Vulkan `projection_only__disabled` lane:

- `[gdgs-ts] render_target_policy_cause ... trigger_reason=render_target_set_size ... transparent_bg=false viewport_texture_requests=0 ...`
- `[gdgs-ts] tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,... shared_view_requirement={transparent_bg=false,viewport_texture_requests=0,requirement_class=eager_rule_without_observed_consumer,invariant_classifier=no_observed_tonemap_lane_shared_view_invariant,direct_root_accesses={framebuffer=1,rd_texture=0,rd_texture_slice=0,rd_texture_msaa=0,total=1}} ...}`
- the same run still ends at `fence_wait_error submit_serial=9 wait_result=-4` and later `BLIT_PASS`

### What this proves

For this locked Tonemap repro lane, the eager shared-view creation does **not** look like a required Tonemap invariant:

- the render target still reaches Tonemap as `path_class=persistent_root_direct` with `lane_owner=rt->color`
- before the failure, the lane records one direct root-framebuffer access (`framebuffer=1`) and **zero** observed shared render-target texture requests (`viewport_texture_requests=0`)
- the new lane classifier therefore lands on `invariant_classifier=no_observed_tonemap_lane_shared_view_invariant`

That means the surviving fact pattern is:

- eager shared-view creation still happens earlier at `render_target_set_size()`
- the broader engine rule still keeps `rt->color` persistent/non-discardable because the shared aliases are created eagerly
- but on this exact failing Tonemap lane, the observed execution path uses the root attachment/framebuffer directly and never shows a Tonemap-local need for the shared render-target texture alias before the crash

### Updated conclusion

For the failing host-Vulkan `projection_only__disabled` Tonemap lane at `submit_serial=9`, the smallest honest classification is:

- **not** a proven Tonemap-local invariant requiring eager shared-view creation
- **yes:** the eager shared-view creation at `render_target_set_size()` remains the surviving broader ownership seam for this lane

Important scope boundary: this does **not** prove the engine can globally delete eager shared-view creation without consequences. It proves the narrower thing we needed here: on this failing Tonemap lane, the shared view is created eagerly by policy, but the observed Tonemap path itself still runs through the direct root framebuffer path and shows no actual shared-view consumer before the unchanged failure.

## 2026-05-21 — Task 118 follow-up: classify lazy/on-demand shared-view fix shape vs an unobserved broader pre-failure consumer

Artifact root:
`/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-classifier-vulkan-sourcebuild-20260521-134232/`

### What changed

I kept the diagnostic as narrow as possible inside `TextureStorage` and only added counters for **actual render-target texture-RID consumers** that could use the eagerly-created shared aliases without going back through the already-measured `render_target_get_texture()` request path:

- `texture_get_rd_texture()` now counts base-vs-sRGB requests when the queried texture belongs to a render target
- `texture_get_native_handle()` now counts base-vs-sRGB native-handle requests for render-target textures
- the existing Tonemap lane summary now reports these as `shared_view_entrypoints`

This keeps the fork honest: **if some broader pre-failure consumer already holds the render-target texture RID and asks for its RD/shared alias before the failing submit, we should now see it even when `viewport_texture_requests=0`.**

### Runtime result

The rebuilt source editor reproduced the same failing lane and printed:

- `[gdgs-ts] tonemap_render_target_lane ... lane={path_class=persistent_root_direct,lane_owner=rt->color,lane_policy=persistent_root_sampled_shared,... shared_view_requirement={transparent_bg=false,shared_view_entrypoints={viewport_texture_requests=0,texture_rd={base=0,srgb=0,total=0},native_handle={base=0,srgb=0,total=0},total=0},requirement_class=eager_rule_without_observed_consumer,invariant_classifier=lazy_on_demand_shared_view_candidate,direct_root_accesses={framebuffer=1,rd_texture=0,rd_texture_slice=0,rd_texture_msaa=0,total=1}} ...}`
- the same run still ends at `fence_wait_error submit_serial=9 wait_result=-4` and later `BLIT_PASS`

### What this proves

For the locked failing Tonemap lane, the newly-added pre-failure consumer checks stayed completely dark:

- no `render_target_get_texture()` requests before failure (`viewport_texture_requests=0`)
- no render-target texture RID → RD alias requests before failure (`texture_rd={base=0,srgb=0,total=0}`)
- no render-target texture native-handle requests before failure (`native_handle={base=0,srgb=0,total=0}`)
- Tonemap still reaches the failure through the direct root framebuffer path (`framebuffer=1`, other direct-root counters `0`)

So the evidence now favors the narrower fix shape:

- **yes:** this lane looks like a `lazy/on-demand shared-view creation` candidate
- **no observed evidence:** of a broader pre-failure shared-view consumer outside Tonemap on this repro before the unchanged `submit_serial=9` failure

### Updated conclusion

For this exact host-Vulkan `projection_only__disabled` repro lane, the most honest current classification is:

- the broader ownership seam is still the eager shared-view creation policy in `render_target_set_size()` / `_update_render_target()`
- but after also checking render-target texture RID → RD/native-handle entrypoints, there is still **no observed pre-failure consumer** forcing those shared aliases to exist before Tonemap reaches the unchanged crash
- therefore the fix shape for **this lane** now looks more like **lazy/on-demand shared-view creation** than "some broader pre-failure consumer exists but we just have not seen it yet"

Scope boundary: this is still a lane-local classification, not a proof that all eager shared-view creation can be removed engine-wide.

## 2026-05-21 — Task 119 follow-up: narrow debug-gated lazy shared-view creation experiment on the locked Tonemap lane

Artifact roots:
- control (env off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-control-vulkan-sourcebuild-20260521-1411/`
- experiment (env on): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-on-vulkan-sourcebuild-20260521-1410/`

### What changed

I kept the experiment narrowly scoped inside `TextureStorage` and made it reversible behind a debug/dev-only environment gate:

- `GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1`
- applies only to the current non-MSAA, non-transparent, single-view, non-overridden render-target lane
- skips eager `texture_create_shared()` creation for the render-target texture alias during `_update_render_target()`
- still preserves on-demand materialization if some later caller actually asks for the render-target texture's RD/native-handle shared alias
- updates the Tonemap lane summary so the lane contract can honestly report whether the shared view was actually materialized

### Runtime result

Control run (env off) stayed on the old contract and reproduced:

- `lane_policy=persistent_root_sampled_shared`
- `shared_view_materialized=true`
- `lazy_shared_view_experiment=false`
- `exit_status=134`
- unchanged `fence_wait_error submit_serial=9 wait_result=-4`

Experiment run (env on) changed the Tonemap lane contract before failure to:

- `lane_policy=persistent_root_direct_lazy_shared_view`
- `lane_rationale=tonemap_direct_to_render_target_framebuffer_without_materialized_shared_view`
- `shared_view_materialized=false`
- `lazy_shared_view_experiment=true`
- `shared_view_entrypoints={viewport_texture_requests=0,texture_rd={base=0,srgb=0,total=0},native_handle={base=0,srgb=0,total=0},total=0}`
- `direct_root_accesses={framebuffer=1,rd_texture=0,rd_texture_slice=0,rd_texture_msaa=0,total=1}`

Notably, the experiment run never logged `render_target_shared_view_create`, so no deferred shared-view demand appeared before failure.

But the failure envelope itself did **not** move:

- `exit_status=134`
- same `fence_wait_error submit_serial=9 wait_result=-4`
- later breadcrumbs still collapse to `BLIT_PASS`

### What this proves

This narrow experiment successfully changed the render-target ownership contract on the locked failing Tonemap lane from an **eager sampled/shared-root policy** to a **direct-root lazy-shared-view policy** without observing any real shared-view demand before failure.

So for this exact repro lane:

- **yes:** the Tonemap lane really can be reclassified as a lazy/on-demand shared-view candidate in practice, not just in theory
- **yes:** eager shared-view creation was removable on this lane without breaking any observed pre-failure consumer contract
- **no:** changing that contract alone did **not** change the failing `submit_serial=9` / `BLIT_PASS` envelope

Updated conclusion: eager shared-view creation is now demoted from an active suspect on this locked lane. It was a real policy mismatch worth testing, but the device-loss trigger survives even after the lane is forced onto the narrower direct-root/no-materialized-shared-view contract.

## 2026-05-21 — Task 121: why the rebuilt active scope still insists on slot-0 `LOAD` after shared-view demotion

Artifact roots:
- reused comparison roots:
  - control: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-control-vulkan-sourcebuild-20260521-1411/`
  - experiment: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-lazy-shared-view-experiment-on-vulkan-sourcebuild-20260521-1410/`
- fresh serial-mapping rerun (env on): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-active-scope-load-cause-vulkan-sourcebuild-20260521-2037/`

### Smallest added diagnostic

I added one tiny Vulkan-side creation log:

- `[gdgs-vk] render_pass_create create_serial=... render_pass_id=... vk_render_pass=... attachment_load_ops=...`

That bridges the existing RDG `draw_list_render_pass_create` evidence to the later backend `active_render_pass_create_serial=13` / `begin_render_pass_scope` evidence, so the active scope can be traced back to its exact graph-side render-pass recipe.

### Fresh runtime result

The new env-on rerun ties the whole chain together directly:

- RDG still creates the Tonemap render pass as:
  - `label="Tonemap"`
  - `source="non_discardable_default_load_contract"`
  - `tracker_discardable=false`
  - `tracker_has_parent=false`
  - `discardable_provenance="root_texture_create"`
  - `discardable_seed=false`
  - `discardable_seed_contract="texture_format_is_discardable_flag"`
  - `default_non_discardable_policy="always_load_without_extra_per_attachment_split"`
  - `load_op=LOAD`
- the new Vulkan creation bridge shows that same RDG object becoming:
  - `render_pass_create create_serial=13 render_pass_id=0x7ae3d2e30828 ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 compatibility_hash=0x425f4d3d`
- Tonemap then begins on that same object:
  - `begin_render_pass_scope create_serial=13 render_pass_id=0x7ae3d2e30828 ... owner_label="Tonemap (L87) (Draw)" ... attachment_load_ops=[0:LOAD]`
- and `L88` immediately reuses that exact active scope:
  - `begin_render_pass_scope create_serial=13 render_pass_id=0x7ae3d2e30828 ... owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD]`

The pre-rebind classifier is still unchanged in the same run:

- active scope `create_serial=13` / `active_load_ops=["0:LOAD"]`
- carried Tonemap pipeline packet `create_serial=9` / `pipeline_load_ops=["0:CLEAR"]`
- `load_op_attribution_split.classification="active_scope_rebuild_first_attributable_step"`

### Exact conclusion

Shared-view demotion did **not** remove the input that actually forces slot-0 `LOAD`.

The unchanged forcing contract is still the **Tonemap RDG active-scope render-pass recipe itself**:

- the attachment is still a **root, non-parented, non-discardable** tracker
- that non-discardability still comes from **root texture creation inheriting `TextureFormat.is_discardable=false`**
- once the tracker reaches RDG in that state, the draw-list render-pass recipe still takes the unchanged branch:
  - `source="non_discardable_default_load_contract"`
  - `default_non_discardable_policy="always_load_without_extra_per_attachment_split"`
- that branch creates the actual live active render pass (`create_serial=13`) with slot 0 already set to `LOAD`
- `L88` does not newly decide `LOAD`; it only re-enters the already-created Tonemap/L88-compatible active scope

So the rebuilt active scope still insists on slot-0 `LOAD` because the **graph-side non-discardable root attachment contract survived the shared-view experiment unchanged**. The experiment changed the lane-policy surface, but not the root tracker discardability contract that RDG uses when constructing the live Tonemap/L88-compatible render pass.

## 2026-05-21 — Task 122: classify the unchanged RDG attachment/tracker input still feeding `non_discardable_default_load_contract`

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-rdg-input-vulkan-sourcebuild-20260521-210609/`

### Smallest added diagnostic

I kept the change inside the existing RDG `draw_list_render_pass_create` debug payload and added two explicit per-attachment fields for the non-discardable default branch:

- `load_branch_decision_input`
- `load_branch_upstream_input`

That makes the RDG log name the exact field that selects the branch, instead of only implying it from the surrounding tracker dump.

### Fresh runtime result

The fresh env-on shared-view-demotion rerun still lands on the same crash envelope (`exit_status=134`, `fence_wait_error submit_serial=9 wait_result=-4`) and now names the unchanged RDG-side forcing input directly on the Tonemap attachment:

- `label="Tonemap"`
- `source="non_discardable_default_load_contract"`
- `tracker_discardable=false`
- `tracker_has_parent=false`
- `discardable_provenance="root_texture_create"`
- `discardable_seed=false`
- `discardable_seed_contract="texture_format_is_discardable_flag"`
- `load_branch_decision_input="resource_tracker->is_discardable=false_after_clear_ignore_checks"`
- `load_branch_upstream_input="root_texture_create<-texture_format_is_discardable_flag:false"`

The same run still bridges straight into the live Tonemap/L88 scope:

- `[gdgs-vk] render_pass_create create_serial=13 ... attachment_load_ops=[0:LOAD]`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 ... owner_label="Tonemap (L87) (Draw)" ... attachment_load_ops=[0:LOAD]`
- `[gdgs-vk] begin_render_pass_scope create_serial=13 ... owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD]`

And the pre-rebind classifier is still unchanged in the same artifact:

- `pre_rebind_carried_packet_contract.minimum_distinguishing_hazard="attachment_exact_recipe"`
- `load_op_attribution_split.classification="active_scope_rebuild_first_attributable_step"`
- carried Tonemap packet still `CLEAR`, rebuilt active scope still `LOAD`

### Exact conclusion

The unchanged RDG-side input is **not** a shared-view ownership bit. It is the attachment tracker field `resource_tracker->is_discardable`, which still arrives at Tonemap as `false` after the clear/ignore checks.

More precisely, the surviving forcing chain on this lane is:

1. `TextureFormat.is_discardable=false`
2. root tracker creation records that as `discardable_seed=false` with `discardable_seed_contract="texture_format_is_discardable_flag"`
3. the attachment remains a root tracker (`discardable_provenance="root_texture_create"`, `tracker_has_parent=false`)
4. RDG therefore sees `resource_tracker->is_discardable=false`
5. the draw-list recipe takes `source="non_discardable_default_load_contract"`
6. the live Tonemap/L88-compatible render pass is created with slot 0 already set to `LOAD`

So Task 122 narrows the answer one step further than Task 121: the exact unchanged RDG branch input is the **tracker discardability field itself** (`resource_tracker->is_discardable=false`), and its unchanged upstream source on this repro lane is still the root texture's `TextureFormat.is_discardable=false` seed inherited at root creation.

## 2026-05-21 — Task 123: classify whether the root texture `is_discardable=false` contract is itself the true Tonemap bug seam

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-contract-classifier-vulkan-sourcebuild-20260521-220037/`

### Smallest added diagnostic

I kept the change inside the existing `tonemap_render_target_lane` payload in `TextureStorage::render_target_debug_describe_tonemap_lane()` and added one narrow ownership block:

- `ownership_contract={class=...,basis=...,contrast=...}`

This does not reopen the demoted shared-view policy lane. It only states, for the exact Tonemap destination path already being logged, whether the destination is:

- the persistent non-discardable render-target root,
- the explicit discardable Tonemapper intermediate, or
- the explicit discardable MSAA intermediate.

### Fresh runtime result

The fresh shared-view-demotion rerun keeps the same failure envelope (`exit_status=134`, `fence_wait_error submit_serial=9 wait_result=-4`) and now classifies the failing Tonemap destination path directly:

- `path_class=persistent_root_direct`
- `lane_owner=rt->color`
- `lane_policy=persistent_root_direct_lazy_shared_view`
- `ownership_contract={class=persistent_root_non_discardable,basis=non_msaa_render_target_color_texture_format_default_false_root_tracker_seed,contrast=direct_path_bypasses_discardable_tonemapper_destination}`
- `shared_view_materialized=false`
- `transparent_bg=false`
- `shared_view_entrypoints.total=0`

The same run still names the RDG-side forcing branch exactly as before:

- `label="Tonemap"`
- `source="non_discardable_default_load_contract"`
- `tracker_discardable=false`
- `discardable_provenance="root_texture_create"`
- `discardable_seed=false`
- `load_branch_decision_input="resource_tracker->is_discardable=false_after_clear_ignore_checks"`
- `load_branch_upstream_input="root_texture_create<-texture_format_is_discardable_flag:false"`

### Exact conclusion

This makes the fork cleaner:

- the root texture `is_discardable=false` contract still looks like a **correct persistent-root ownership rule** for `rt->color`
- the failing Tonemap lane is specifically the **direct-root path**, not the explicit discardable Tonemapper intermediate path and not the explicit discardable MSAA intermediate path
- therefore the surviving seam is deeper than the root seed itself: Tonemap’s direct-root/full-target path is still being expressed to RDG only as a write to a persistent root attachment, so the graph rebuild keeps applying the generic non-discardable `LOAD` contract to that root attachment before `L88`

In plain English: the root texture contract is doing what the render-target ownership code says it should do — preserve the persistent root. The narrower surviving problem is that this exact Tonemap lane bypasses the already-existing discardable intermediate paths, yet the pre-rebind active-scope reconstruction still lacks a narrower “first-write / overwrite / safe-discard-for-this-pass” signal for the persistent root attachment. That is why the live Tonemap/L88-compatible scope is still born with slot 0 as `LOAD` even after the shared-view demotions.

## 2026-05-22 — Task 124: classify whether the surviving seam is the direct-path choice itself or the missing overwrite hint on that path

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-contract-classifier-vulkan-sourcebuild-20260521-220037/`

### Smallest added diagnostic

I kept the change inside the existing `tonemap_render_target_lane` payload in `TextureStorage::render_target_debug_describe_tonemap_lane()` and added two narrow source-routing blocks:

- `route_gate={...}`
- `direct_write_contract={...}`

The intent is to make the existing Tonemap lane log say, in one place, **why** the route was selected and whether the route carries any explicit RDG first-write/discard hint.

The new fields are source-derived and reversible. They do not reopen already-demoted families or alter renderer behavior.

### Source-routed classification

The exact route choice on this repro is now explicit in source:

- `RendererSceneRenderRD::render_buffers_post_process_and_tonemap()` takes the direct render-target branch when:
  - `using_scaling_pass=false`
  - `use_smaa=false`
  - `dest_is_msaa_2d=false`
- that branch selects `dest_fb = texture_storage->render_target_get_rd_framebuffer(render_target)`
- the existing discardable alternatives only appear on the other branches:
  - scaling/SMAA: `Tonemapper.destination`
  - MSAA: `rt->color_multisample`

The Tonemap draw itself also stays source-visible and unchanged:

- `ToneMapper::tonemapper()` / `tonemapper_mobile()` call `RD::draw_list_begin(p_dst_framebuffer)` with the default draw flags
- there is no Tonemap-side `DRAW_CLEAR_*`
- there is no Tonemap-side `DRAW_IGNORE_*`
- the packet is still a fullscreen draw into the selected destination framebuffer

### Exact conclusion

This answers the fork more cleanly than Task 123:

- **The path choice itself is not currently the strongest bug seam.** On this locked repro lane, Tonemap is taking the direct persistent-root path because the current source routing says it should: no scaling pass, no SMAA, no MSAA intermediate.
- **The stronger surviving seam is that the correct direct-root path carries no narrower RDG overwrite hint.** Because Tonemap begins with `RD::DRAW_DEFAULT_ALL` on a persistent non-discardable root attachment, RDG receives no explicit `CLEAR` / `IGNORE` / discardable-first-write signal for slot 0 on that path.
- So RDG’s existing rule remains internally consistent: root tracker is non-discardable → draw-list recipe selects `non_discardable_default_load_contract` → live Tonemap/L88-compatible scope is rebuilt as slot-0 `LOAD`.

In plain English: Tonemap is not mysteriously taking the wrong branch; it is taking the branch the current post-process routing requests. The surviving gap is that this branch still describes the pass to RDG as a write into a persistent root framebuffer **without** a narrower “this pass overwrites color 0” contract. That keeps the direct path looking like a normal non-discardable attachment reuse, so RDG conservatively rebuilds the active scope as `LOAD`.

## 2026-05-22 — Task 125: classify whether the direct-root Tonemap path can express a narrower RDG overwrite hint

Artifact root:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-contract-classifier-vulkan-sourcebuild-20260521-220037/`

### Smallest added diagnostic

I kept the change inside `TextureStorage::render_target_debug_describe_tonemap_lane()` and tightened the existing `direct_write_contract={...}` payload so it now states the RDG-side contract limits explicitly for this lane:

- `rdg_attachment_override_options=clear_or_ignore_only`
- `rdg_non_discardable_default=load_and_store`
- `rdg_safe_discard_signal=unavailable_without_clear_ignore_or_discardable_tracker`

This is diagnostic-only and reversible. It does not change renderer behavior.

### Source evidence used for the classification

The locked route-selection conclusion still holds on this repro:

- `RendererSceneRenderRD::render_buffers_post_process_and_tonemap()` takes the direct render-target branch when `using_scaling_pass=false`, `use_smaa=false`, and `dest_is_msaa_2d=false`
- that branch selects `texture_storage->render_target_get_rd_framebuffer(render_target)`
- the existing discardable alternatives stay on different branches (`Tonemapper.destination` for scaling/SMAA, `rt->color_multisample` for MSAA)

The direct-root write itself still uses the default draw-list contract:

- `ToneMapper::tonemapper()` calls `RD::draw_list_begin(p_dst_framebuffer)` with default flags
- `RenderingDevice::draw_list_begin()` only turns a color attachment into a narrower RDG attachment operation when the caller sets `DRAW_CLEAR_COLOR_*` or `DRAW_IGNORE_COLOR_*`
- `RenderingDeviceGraph::_run_draw_list_command()` then resolves non-discardable default attachments to `ATTACHMENT_LOAD_OP_LOAD` and `ATTACHMENT_STORE_OP_STORE`

### Exact conclusion

On this locked `projection_only + disabled` direct-root Tonemap lane, there is **no narrower per-attachment overwrite / first-write / safe-discard signal currently being expressed to RDG** without changing one of the three owning facts:

- the draw-list flags (`CLEAR` / `IGNORE`)
- the attachment tracker discardable state
- the route itself onto one of the already-existing discardable intermediate paths

Because this lane is intentionally the persistent root and Tonemap currently begins with `RD::DRAW_DEFAULT_ALL`, RDG is forced to keep the generic non-discardable slot-0 contract:

- `load_op=LOAD`
- `store_op=STORE`
- source branch `non_discardable_default_load_contract`

So the answer to the fork is: **the direct-root path is correct, but under the current API/contract it cannot express a narrower overwrite hint to RDG without violating the persistent-root rule or introducing an explicit new signal.**

## 2026-05-22 — Task 126: classify whether the direct-root Tonemap pass is semantically a full overwrite

Artifact root used:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-contract-classifier-vulkan-sourcebuild-20260521-220037/`

### Source evidence used for the overwrite question

I kept this slice on the same locked source-built host-Vulkan `projection_only + disabled` lane and answered the fork from exact source behavior rather than reopening route selection.

The relevant source path is:

- `RendererSceneRenderRD::render_buffers_post_process_and_tonemap()` still selects the direct-root framebuffer branch on this repro (`using_scaling_pass=false`, `use_smaa=false`, `dest_is_msaa_2d=false`)
- `ToneMapper::tonemapper()` begins a draw list directly on that framebuffer with `RD::draw_list_begin(p_dst_framebuffer)`
- `RenderingDevice::draw_list_begin()` uses the full framebuffer viewport/scissor when no custom `Rect2` region is supplied
- `ToneMapper::tonemapper()` binds a render pipeline created with `RD::PipelineColorBlendState::create_disabled()` and default depth/stencil state, then issues `draw_list_draw(draw_list, false, 1u, 3u)`
- the Tonemap vertex shader (`servers/rendering/renderer_rd/shaders/effects/tonemap.glsl`) emits the standard fullscreen triangle vertices `(-1,-1)`, `(-1,3)`, `(3,-1)`
- the Tonemap fragment shader writes exactly one color output (`frag_color = color`) and contains no `discard`, no conditional early-return that skips writes, and no depth/stencil output path

### Classification

On the direct-root lane, the Tonemap pass is **semantically a full-screen color overwrite of slot 0**:

- fullscreen geometry covers the whole framebuffer
- viewport/scissor default to the full framebuffer
- blending is disabled, so the previous color value is not read for blend composition
- the fragment shader always produces a color for the covered pixels
- the pass has no source-level dependence on preserving the previous contents of slot 0 in order to compute the new output

So the source answer to the fork is:

- **yes:** this pass behaves like a full overwrite of the destination color attachment
- **no:** I did not find source behavior, shader behavior, or attachment usage showing that preserving prior slot-0 contents is semantically required for Tonemap itself on this lane

### Important contract caveat

That does **not** mean the current RDG contract can already truthfully infer `IGNORE`/discard from the existing callsite.

Under the current API shape, RDG still only sees:

- a draw list begun with default flags
- a persistent non-discardable root attachment
- no explicit `DRAW_CLEAR_COLOR_*`
- no explicit `DRAW_IGNORE_COLOR_*`
- no discardable tracker on this route

So the source-level classification is now sharper than the currently expressed RDG contract:

- **semantic truth:** Tonemap direct-root is a full overwrite
- **current encoded contract:** generic non-discardable default `LOAD`/`STORE`

That leaves the next fork very narrow: if we want RDG to use a narrower slot-0 contract here, it must come from a new explicit signal or route-specific contract change, not from pretending the existing default draw-list call already says it.

## 2026-05-22 source-build follow-up — Task 127 (`oc-95k`)

This follow-up stayed on the same locked host-Vulkan `projection_only__disabled` repro lane and tested the smallest truthful overwrite-style contract experiment that did **not** reopen route selection or broader ownership policy.

Locked starting artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-21/official-tonemap-root-contract-classifier-vulkan-sourcebuild-20260521-220037/`

Fresh experiment artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-overwrite-contract-vulkan-sourcebuild-20260522-090000/`

Validation command:

- `env DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1 GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-overwrite-contract-vulkan-sourcebuild-20260522-090000 no_present compositor projection_only disabled 120`

What changed in code:

- `servers/rendering/renderer_rd/effects/tone_mapper.cpp` now accepts a debug-only env-gated experiment (`GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1`) that flips the direct-root Tonemap draw-list call from the default flags to `RD::DRAW_IGNORE_COLOR_0`.
- `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp` mirrors that experiment state into the existing Tonemap lane payload so the artifact explicitly records the contract expression that RDG was asked to honor.

What the artifact proves:

- The experiment really ran. `stdout.log` contains:
  - `[gdgs-ts] tonemap_overwrite_contract_experiment={enabled=true,draw_list_begin_flags=RD::DRAW_IGNORE_COLOR_0,attachment_overwrite_contract=explicit_ignore_slot0,fullscreen_draw=true}`
- The lane log also records the narrower contract at the routing seam:
  - `route_vs_overwrite_classifier=path_choice_matches_current_policy_with_explicit_overwrite_experiment`
  - `direct_write_contract={draw_list_begin_flags=RD::DRAW_IGNORE_COLOR_0,attachment_ignore=true,rdg_first_write_hint=explicit_ignore_slot0,...}`

Outcome:

- The repro still aborts at the same failing seam: `submit_serial=9` with `exit_status=134`, followed by `fence_wait_error submit_serial=9 wait_result=-4`.
- The surviving carried-vs-active attachment mismatch also stays put. The same pre-rebind payload still reports:
  - `active_load_ops=["0:LOAD"]`
  - `pipeline_load_ops=["0:CLEAR"]`
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", ... pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD"}`

Interpretation:

- Giving the direct-root Tonemap pass a truthful explicit overwrite-style contract at the draw-list callsite was a valid, reversible experiment.
- On this locked repro lane, that narrower contract did **not** move the crash, did **not** narrow the failing `submit_serial=9` handoff further, and did **not** dislodge the unchanged active-scope-rebuild vs carried-packet slot-0 `LOAD`/`CLEAR` mismatch before `L88`.
- So the next honest fork is no longer “would a truthful overwrite hint matter at all?” — this run says **not on its own, in this lane**.

## 2026-05-22 source-build follow-up — Task 128 (`oc-xkz`)

This follow-up did **not** add more code or rerun a wider family. Instead, it re-read the fresh overwrite-experiment artifact against the existing RDG/Vulkan attribution diagnostics to classify why slot 0 still resolves as `LOAD` in the rebuilt active scope.

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-overwrite-contract-vulkan-sourcebuild-20260522-090000/`

Key runtime lines from the same artifact:

- Tonemap’s own overwrite experiment really took effect on the Tonemap draw-list render pass:
  - `[gdgs-rdg] draw_list_render_pass_create ... label="Tonemap" ... attachments=[{index=0,load_op=2,store_op=0,source="attachment_operation_ignore",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,...,tracker_name="Render Target Color",discardable_provenance="root_texture_create",discardable_seed=false,...}]`
- But a later RDG draw-list render-pass creation on the same lane still rebuilds slot 0 through the default non-discardable path:
  - `[gdgs-rdg] draw_list_render_pass_create ... label="none" breadcrumb=720896 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,parent_write_index=-1,texture_usage=0x8b,tracker_name="Render Target Color",discardable_provenance="root_texture_create",discardable_seed=false,discardable_seed_contract="texture_format_is_discardable_flag",non_discardable_basis="tracker_is_non_discardable",default_non_discardable_policy="always_load_without_extra_per_attachment_split",load_branch_decision_input="resource_tracker->is_discardable=false_after_clear_ignore_checks",load_branch_upstream_input="root_texture_create<-texture_format_is_discardable_flag:false"}]`
- The paired Vulkan pre-rebind attribution still says the active-side divergence first appears at scope reconstruction, not at carried-packet survival:
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", basis="carried_packet_slot_value_stays_exact_from_tonemap_end_through_l88_begin_into_pre_rebind_while_active_scope_rebuild_first_introduces_the_opposing_slot_0_load_op", first_attributable_step="active_pre_rebind_scope_reconstruction", ... tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD", slot_index=0}`

Interpretation:

- The explicit Tonemap overwrite contract only changed the **Tonemap-local** draw-list render-pass recipe.
- The later active scope that `L88` re-enters is rebuilt from a **different** RDG draw-list render-pass creation (`label="none"`, `breadcrumb=720896`) that still consumes the same root, non-parented, non-discardable `Render Target Color` tracker (`tracker_write_index=132`).
- That later rebuild still sees the exact upstream forcing signal:
  - `resource_tracker->is_discardable=false_after_clear_ignore_checks`
  - upstream source: `root_texture_create<-texture_format_is_discardable_flag:false`
- Because that signal is unchanged on the rebuild path, the later active scope still falls back to `source="non_discardable_default_load_contract"` and therefore reintroduces slot-0 `LOAD` before `L88` first rebinds.

Exact conclusion:

- The overwrite experiment did **not** “fail to stick” on Tonemap. It stuck there.
- What stayed unchanged is the later active-scope rebuild input: a separate UI-pass / active-scope draw-list creation still reconsumes the same root non-discardable tracker contract and independently resolves slot 0 as `LOAD`.
- So the surviving forcing signal is still the root tracker’s non-discardable contract, not the absence of the Tonemap-local overwrite hint.

## 2026-05-22 source-build follow-up — Task 129 (`oc-sq0`)

This follow-up added the smallest honest source-side attribution needed to name the later unlabeled UI rebuild directly instead of only inferring it from `breadcrumb=720896`.

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`

Validation command:

- `env DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1 GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400 no_present compositor projection_only disabled 120`

Code change:

- `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` now emits an env-gated `[gdgs-canvas] ui_pass_origin=...` line from `RendererCanvasRenderRD::_render_batch_items()` right before the root render-target UI pass calls `draw_list_begin()`.
- The diagnostic records the exact caller codepath, framebuffer source, breadcrumb, draw-list flags, clear state, and the fact that this path has no surrounding `draw_command_begin_label`.

Key runtime lines from the new artifact:

- Exact UI draw-list origin:
  - `[gdgs-canvas] ui_pass_origin={codepath="RendererCanvasRenderRD::_render_batch_items",framebuffer_source="render_target_get_rd_framebuffer",breadcrumb=720896,draw_list_begin_flags=RD::DRAW_DEFAULT_ALL,clear_requested=false,msaa_resolve_disabled_before_ui=true,label_state="none_without_draw_command_begin_label"}`
- The same later RDG rebuild immediately follows and still resolves through the default non-discardable branch:
  - `[gdgs-rdg] draw_list_render_pass_create ... label="none" breadcrumb=720896 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract",tracker_discardable=false,tracker_has_parent=false,tracker_write_index=132,parent_write_index=-1,texture_usage=0x8b,tracker_name="Render Target Color",discardable_provenance="root_texture_create",discardable_seed=false,discardable_seed_contract="texture_format_is_discardable_flag",non_discardable_basis="tracker_is_non_discardable",default_non_discardable_policy="always_load_without_extra_per_attachment_split",load_branch_decision_input="resource_tracker->is_discardable=false_after_clear_ignore_checks",load_branch_upstream_input="root_texture_create<-texture_format_is_discardable_flag:false"}]`
- The broader failure envelope stayed locked:
  - `fence_wait_error submit_serial=9 wait_result=-4`
  - `exit_status=134`

Interpretation:

- The later `label="none"` active-scope rebuild before `L88` is created by the canvas UI root draw-list begin in `RendererCanvasRenderRD::_render_batch_items()`.
- It is unlabeled because this path does **not** wrap the UI pass in `draw_command_begin_label(...)`; the breadcrumb is `UI_PASS`, but the RDG label field remains `none`.
- It inherits the generic non-discardable `LOAD` contract because that codepath enters `draw_list_begin()` with `RD::DRAW_DEFAULT_ALL` on the root render-target framebuffer while `clear_requested=false`, so RDG sees the same root non-discardable `Render Target Color` tracker and falls back to `source="non_discardable_default_load_contract"` from `resource_tracker->is_discardable=false_after_clear_ignore_checks`.

Exact conclusion:

- The later active-scope rebuild is **not** an invisible Tonemap continuation and not a hidden backend-only gap.
- It is the root canvas/UI pass draw-list created by `RendererCanvasRenderRD::_render_batch_items()`.
- The surviving forcing signal remains the unchanged root non-discardable tracker contract, and the immediate reason slot 0 becomes `LOAD` again is that this UI path requests the default preserve-content contract (`RD::DRAW_DEFAULT_ALL`) rather than an overwrite-style attachment operation.

## 2026-05-22 source-build follow-up — Task 130 (`oc-tu4`)

This follow-up stayed on the same locked host-Vulkan `projection_only__disabled` repro lane and answered the next narrower fork: whether the UI root pass itself is semantically a full overwrite of slot 0, or whether it truthfully needs the prior root contents preserved.

Locked starting artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`

Fresh classifier artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-overwrite-classifier-vulkan-sourcebuild-20260522-100702/`

Validation command:

- `env DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1 GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-overwrite-classifier-vulkan-sourcebuild-20260522-100702 no_present compositor projection_only disabled 120`

Code change:

- `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` now extends the existing env-gated `[gdgs-canvas] ui_pass_origin=...` line with a per-batch classifier recorded directly at the `RendererCanvasRenderRD::_render_batch_items()` draw-list begin site.
- The diagnostic stays reversible and source-truthful: it records only observable batch facts that matter to the overwrite question — batch count, clip/scissor usage, blend-mode usage, and the first live blend mode.

Key runtime lines from the fresh artifact:

- The UI root pass still begins exactly on the same root framebuffer with the default preserve-content draw-list flags:
  - `[gdgs-canvas] ui_pass_origin={codepath="RendererCanvasRenderRD::_render_batch_items",framebuffer_source="render_target_get_rd_framebuffer",breadcrumb=720896,draw_list_begin_flags=RD::DRAW_DEFAULT_ALL,clear_requested=false,...}`
- The new batch summary classifies the live UI workload as preserve-content rather than overwrite:
  - `batch_summary={rendered=10,rect_like=10,polygon=0,primitive=0,clipped=10,lcd_blend=0,destination_color=10,blend_disabled=0,first_blend_mode="mix",first_destination_color_blend_mode="mix"},overwrite_classifier="preserve_prior_root_contents"`
- The downstream Vulkan/RDG evidence remains aligned with that reading:
  - `owner_label="Command Graph (L88) (Draw)" owner_level=88 breadcrumb=UI_PASS attachment_load_ops=[0:LOAD]`
  - the `L88` canvas pipeline provenance still reports `shader_name="CanvasShaderRD:0"` with `blend_enabled_attachment_mask="0x1"` and `first_active_attachment={... blend_enable=true, src_color=6, dst_color=7, ...}`
- The broader failure envelope stayed unchanged:
  - `queue_submit submit_serial=9 ... last_label="Command Graph (L88) (Draw)"`
  - `fence_wait_error submit_serial=9 wait_result=-4`

Interpretation:

- This UI root pass is **not** semantically a full overwrite of slot 0 on the locked repro lane.
- The live workload consists of **10** rect-like canvas draws, all **10** clipped/scissored, and all **10** using a destination-dependent blend mode (`mix`) rather than `disabled`.
- That means the UI path is truthfully acting as an overlay on top of prior root contents, not replacing them wholesale. Preserving the tonemapped scene under the UI is part of the actual draw contract, not an accidental default.
- The Vulkan-side pipeline provenance agrees: the `L88` canvas pass uses `CanvasShaderRD:0` with blend enabled, while the preceding Tonemap packet is the no-blend fullscreen write.

Exact conclusion:

- The later UI root pass before `L88` genuinely needs prior root contents preserved on this lane.
- A blanket overwrite/ignore contract for that pass would **not** be truthful.
- So the surviving slot-0 `LOAD` before `L88` is not just a missing optimization hint; for the UI pass, preserve-content behavior is semantically correct.

## 2026-05-22 source-build follow-up — Task 135 (`oc-8on`)

This follow-up stayed narrow and answered the fork by exact source/policy comparison against comparable engine lanes, without reopening route selection or rerunning a broader runtime family.

Locked artifact roots used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-overwrite-classifier-vulkan-sourcebuild-20260522-100702/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`

Source/policy comparison used:

- `RendererCanvasRenderRD::_render_batch_items()` in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`
- `RenderingDevice::draw_list_begin()` in `servers/rendering/rendering_device.cpp`
- the default-load classification in `servers/rendering/rendering_device_graph.cpp`
- comparable preserve-content callers in `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp`
- comparable default draw-list callers in `servers/rendering/renderer_rd/effects/copy_effects.cpp`

What the comparison shows:

- The failing UI root pass is **not** using a one-off or repro-only contract. On this lane `RendererCanvasRenderRD::_render_batch_items()` enters `draw_list_begin()` with `RD::DRAW_DEFAULT_ALL` when `clear_requested=false`, which is the ordinary preserve-content entrypoint for a fresh draw list.
- `RenderingDevice::draw_list_begin()` applies that policy generically: every attachment starts as `RDG::ATTACHMENT_OPERATION_DEFAULT`, and only explicit `DRAW_CLEAR_*` / `DRAW_IGNORE_*` flags narrow it before the graph sees the request.
- `RenderingDeviceGraph` then resolves that default generically as well: if the attachment tracker is non-discardable, the default path becomes `source="non_discardable_default_load_contract"` / `ATTACHMENT_LOAD_OP_LOAD`.
- That same structural pattern is used elsewhere in normal engine lanes. A close preserve-content comparator is the clustered opaque color pass, which explicitly chooses `RD::DRAW_DEFAULT_ALL` when `load_color` is true (`_render_list_with_draw_list(... RD::DRAW_DEFAULT_ALL ...)`) because prior color contents must be preserved. Other draw-list callers likewise rely on the same generic default path unless they explicitly request clear/ignore.
- The locked runtime artifacts remain consistent with that policy read instead of contradicting it: the UI pass is a blended/clipped overlay (`overwrite_classifier="preserve_prior_root_contents"`), it enters with `RD::DRAW_DEFAULT_ALL`, and the rebuilt active scope then lands on the normal default non-discardable `LOAD` branch.

Interpretation:

- The UI-root non-discardable default-load policy on the failing lane looks **normal and structurally consistent** with comparable engine lanes, not unusual in a way that makes the generic policy itself suspect.
- What is lane-specific is not the generic policy, but the exact juxtaposition at this seam: the carried Tonemap packet still describes slot 0 as `CLEAR`, while the fresh UI draw-list reconstruction truthfully asks to preserve prior root contents and therefore recomputes slot 0 as `LOAD` from the root non-discardable tracker.
- In other words, this repro lane is unusual because it exposes a sharp carried-vs-fresh contract contrast at the Tonemap -> UI boundary, **not** because the UI/root default-load policy is behaving abnormally compared to the rest of the engine.

Exact conclusion:

- The generic UI/root `non_discardable_default_load_contract` on this locked lane is a **normal engine policy**, and the comparable-lane evidence supports it as structurally expected.
- The surviving suspect seam therefore stays with the boundary-specific `CLEAR` vs `LOAD` contrast across Tonemap-carried state versus fresh UI-scope reconstruction, rather than with a claim that UI/root preserve-content policy is inherently anomalous on this repro.

### Task 131 — Last valid writer + pre-UI root attachment state classification on the locked lane (`oc-64s`)

Artifact roots used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-overwrite-classifier-vulkan-sourcebuild-20260522-100702/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`

What the locked lane now says:

- The **last valid writer before UI re-enters root** is still the `Tonemap (L87) (Draw)` payload, not the later UI pass. The carry-over evidence stays zero-gap and exact at the packet boundary:
  - `tonemap_end_state={render_pass_active=false, framebuffer_active=false, render_pipeline_bound=true, vertex_binding_count=0, index_buffer_bound=false, end_breadcrumb="NONE", backend_command_serial=11}`
  - `l88_begin_state={render_pass_active=false, framebuffer_active=false, render_pipeline_bound=true, vertex_binding_count=0, index_buffer_bound=false, begin_breadcrumb="NONE", backend_command_serial=11}`
  - `boundary_state_handoff_classifier.classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index"`
- In other words, **immediately before the UI pass begins**, the surviving root-side state is only the carried Tonemap pipeline packet. No active render pass/framebuffer is live yet; the UI pass has not rebuilt its own active scope.
- The carried Tonemap packet remains exact at that boundary:
  - `pipeline_identity={relation="same_pipeline", exact=true}`
  - `pipeline_layout_descriptor_contract={exact=true, ...}`
  - `push_constant_range_contract={exact=true, ...}`
  - `l88_begin_load_op="CLEAR"`
- The exact root-attachment mismatch appears only after UI starts reconstructing its own active scope. The same artifact pins the first attributable divergence here:
  - `load_op_attribution_split.classification="active_scope_rebuild_first_attributable_step"`
  - `first_attributable_step="active_pre_rebind_scope_reconstruction"`
  - `tonemap_end_load_op="CLEAR"`
  - `l88_begin_load_op="CLEAR"`
  - `pre_rebind_pipeline_load_op="CLEAR"`
  - `pre_rebind_active_load_op="LOAD"`
- So the preserved pre-UI state is **not already poisoned before UI ever re-enters it**. The pre-UI carry from Tonemap survives intact up to `L88` entry. The first conflicting root attachment state is introduced when the UI pass rebuilds its active scope with slot 0 as `LOAD`.

Tracker / attachment lineage that stays locked:

- The originating root tracker from the earlier attribution run remains the same singleton lineage into UI: `tracker_name="Render Target Color"`, `tracker_write_index=132`, `parent_write_index=-1`, `write_dep_count=0`, `non_discardable_root_texture_create={classification="texture_format_is_discardable_flag", texture_format_is_discardable=false}`.
- Tonemap’s local active scope before it writes root still carried its own attachment recipe:
  - active render pass create: `render_pass_create_serial=13`
  - active attachment exact hash: `render_pass_attachment_exact_hash="0xa40355c3"`
- The carried Tonemap pipeline packet references its compatible-only pipeline render-pass lineage:
  - pipeline render pass create: `render_pass_create_serial=9`
  - pipeline attachment exact hash: `render_pass_attachment_exact_hash="0x63c10583"`
- UI’s rebuilt active scope is the first place the opposing root attachment recipe appears:
  - active render pass create: `active_render_pass_create_serial=14`
  - active attachment exact hash: `active_render_pass_attachment_exact_hash="0x6529dc72"`
  - active slot-0 load op: `LOAD`

Exact conclusion:

- **Last valid writer before UI begins:** `Tonemap (L87) (Draw)`.
- **Exact root state immediately before UI begins:** no active render pass/framebuffer, but an exact carried Tonemap pipeline packet is still live across the zero-gap boundary, and that carried packet still encodes slot-0 `CLEAR` / tonemap-side lineage.
- **Was the preserved pre-UI state already poisoned before UI re-entered root?** No. The first attributable conflicting root attachment state appears only when UI rebuilds its own active scope (`active_pre_rebind_scope_reconstruction`) and introduces slot-0 `LOAD`.

### Task 132 — Minimum attachment subfield difference between the carried Tonemap packet and the rebuilt UI active scope (`oc-40v`)

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`

Validation command:

- `env DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1 GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057 no_present compositor projection_only disabled 120`

Code change:

- `drivers/vulkan/rendering_device_driver_vulkan.cpp` now extends the existing env-gated `attachment_exact_recipe={...}` payload with one narrower classifier: `minimum_attachment_subfield_diff={...}`.
- The new block does not widen the lane or add a new capture family. It simply records the minimum surviving attachment subfield difference already implied by the carried-vs-active exact-recipe comparison: classification, first differing field, slot index, both side values, and the non-`load_op` family hash relation.

Key runtime lines from the new artifact:

- The carried-vs-active attachment exact-recipe comparison stayed locked to the same two hashes immediately before `L88` first binds its own pipeline:
  - `pipeline_render_pass_attachment_exact_hash="0x63c10583"`
  - `active_render_pass_attachment_exact_hash="0x6529dc72"`
- The existing exact-recipe seam still says only one field differs:
  - `attachment_exact_recipe={exact=false, field_classifier="load_op_is_minimum_attachment_exact_hazard", minimum_distinguishing_field="load_op", mismatch_count=1, mismatch_fields=["load_op"], format_match=true, samples_match=true, load_op_match=false, store_op_match=true, stencil_load_op_match=true, stencil_store_op_match=true, initial_layout_match=true, final_layout_match=true, ...}`
- The new minimum-subfield classifier makes the smallest surviving difference explicit:
  - `minimum_attachment_subfield_diff={classification="single_slot_single_field_difference", basis="the attachment exact recipe differs only on load_op and only slot 0 changes while the non-load recipe hash stays exact", first_differing_field="load_op", slot_index=0, active_value="LOAD", pipeline_value="CLEAR", active_family_hash="0x2a58ca8c", pipeline_family_hash="0x2a58ca8c"}`
- The first attributable step is still unchanged and still belongs to the rebuilt active scope rather than the carried packet:
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", first_attributable_step="active_pre_rebind_scope_reconstruction", tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD", slot_index=0}`

Interpretation:

- The minimum attachment subfield difference between Tonemap’s carried packet (`0x63c10583`) and the rebuilt UI active scope (`0x6529dc72`) is now classified directly, not just inferred indirectly from the broader exact-recipe hash split.
- It is a **single-slot, single-field** difference: only attachment slot `0` changes, only the `load_op` subfield differs, and the non-`load_op` family hash remains exact on both sides (`0x2a58ca8c`).
- So the first differing field that actually causes the visible `CLEAR` → `LOAD` flip is exactly `load_op` itself, not `format`, `samples`, `store_op`, stencil ops, or either layout field.
- That keeps the previously locked ownership result intact: the carried Tonemap packet still preserves `CLEAR` across the zero-gap boundary, while the rebuilt active UI scope is the first place that reintroduces `LOAD`.

Exact conclusion:

- **Minimum attachment subfield difference:** `slot 0 / load_op` only.
- **First differing field that causes the `CLEAR` → `LOAD` flip:** `load_op`.
- **Did any non-`load_op` attachment subfield differ at this seam?** No — the non-`load_op` family hash stayed exact (`active_family_hash == pipeline_family_hash == 0x2a58ca8c`).
- **Did the broader failure move?** No — the run still aborts on the same locked lane at `fence_wait_error submit_serial=9 wait_result=-4` with `exit_status=134`.

## Follow-up coder pass for bead `oc-uzv` — trace the exact branch that chooses UI slot-0 `LOAD` before `L88`

Scope:

- Stayed on the same source-built host-Vulkan `projection_only + disabled` lane.
- Started from the locked artifact root `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`.
- Kept the pass narrow and reversible: no new engine diagnostic and no new staged rerun were needed because the exact runtime breadcrumb already existed, and the remaining question was source-attribution.

Exact runtime/source decision chain:

1. The UI rebuild before `L88` originates in `RendererCanvasRenderRD::_render_batch_items()`.
   - Source: `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp`
   - On this lane, `clear=false`, so the code sets `ui_draw_flags = RD::DRAW_DEFAULT_ALL` and calls `RD::get_singleton()->draw_list_begin(framebuffer, ui_draw_flags, ..., RDD::BreadcrumbMarker::UI_PASS)`.
   - That matches the saved runtime line:
     - `[gdgs-canvas] ui_pass_origin={codepath="RendererCanvasRenderRD::_render_batch_items", ... draw_list_begin_flags=RD::DRAW_DEFAULT_ALL, clear_requested=false, ... overwrite_classifier="preserve_prior_root_contents" ...}`

2. `RenderingDevice::draw_list_begin()` does **not** preserve the carried Tonemap packet's attachment load op.
   - Source: `servers/rendering/rendering_device.cpp`
   - For color attachments, it only changes the per-attachment operation when the new draw-list flags explicitly request `DRAW_CLEAR_COLOR_*` or `DRAW_IGNORE_COLOR_*`.
   - With `RD::DRAW_DEFAULT_ALL`, slot 0 stays at `RDG::ATTACHMENT_OPERATION_DEFAULT`.

3. The actual `LOAD` choice is then made later when RDG materializes the new draw-list command.
   - Source: `servers/rendering/rendering_device_graph.cpp`
   - In the load-op selection branch for each attachment:
     - `ATTACHMENT_OPERATION_CLEAR` => `ATTACHMENT_LOAD_OP_CLEAR`
     - `ATTACHMENT_OPERATION_IGNORE` => `ATTACHMENT_LOAD_OP_DONT_CARE`
     - else if `resource_tracker->is_discardable` => `LOAD` if modified this frame, otherwise `DONT_CARE`
     - else => unconditional `ATTACHMENT_LOAD_OP_LOAD`
   - On this lane the UI slot-0 tracker is the root `Render Target Color` tracker with `resource_tracker->is_discardable == false`, so execution falls into that final unconditional non-discardable branch.

4. The saved runtime artifact already proves that this is the branch taken for the rebuilt UI scope.
   - `[gdgs-rdg] draw_list_render_pass_create ... label="none" breadcrumb=720896 attachments=[{index=0,load_op=0,store_op=0,source="non_discardable_default_load_contract", tracker_discardable=false, tracker_has_parent=false, tracker_write_index=132, tracker_name="Render Target Color", discardable_provenance="root_texture_create", discardable_seed=false, discardable_seed_contract="texture_format_is_discardable_flag", non_discardable_basis="tracker_is_non_discardable", default_non_discardable_policy="always_load_without_extra_per_attachment_split", load_branch_decision_input="resource_tracker->is_discardable=false_after_clear_ignore_checks", load_branch_upstream_input="root_texture_create<-texture_format_is_discardable_flag:false"}]`
   - The paired Vulkan scope line shows the resulting active scope:
     - `[gdgs-vk] begin_render_pass_scope create_serial=14 ... owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72 ...`

Why the carried Tonemap `CLEAR` contract is not preserved:

- The carried Tonemap packet's slot-0 `CLEAR` contract belongs to the earlier pipeline-side render-pass recipe (`create_serial=9`, `attachment_exact_hash=0x63c10583`).
- The UI path does **not** clone or carry forward that packet's exact attachment recipe when it reconstructs its active scope.
- Instead, UI starts a fresh draw-list on the same framebuffer/tracker lane with `RD::DRAW_DEFAULT_ALL`, and RDG recomputes load/store ops from only:
  - the new draw-list attachment operation (`DEFAULT` here), and
  - the current tracker discardability contract (`is_discardable=false` from the root texture seed).
- Because no explicit clear/ignore override is supplied on the UI draw-list and the root tracker remains non-discardable, the recomputation rule resolves slot 0 back to `LOAD`.
- So the Tonemap-side exact packet is not being "overridden" by a later preservation step; it is simply **not an input** to the UI active-scope rebuild branch.

Exact conclusion:

- The exact codepath that chooses UI slot-0 `LOAD` before `L88` is:
  - `RendererCanvasRenderRD::_render_batch_items()`
  - -> `RD::draw_list_begin(..., RD::DRAW_DEFAULT_ALL, ..., UI_PASS)`
  - -> `RenderingDevice::draw_list_begin()` leaves slot 0 at `ATTACHMENT_OPERATION_DEFAULT`
  - -> `RenderingDeviceGraph` load-op selection falls through to the final `resource_tracker->is_discardable == false` branch
  - -> `ATTACHMENT_LOAD_OP_LOAD`
- The precise forcing rule is the RDG non-discardable default-load policy, evidenced at runtime as `source="non_discardable_default_load_contract"` / `default_non_discardable_policy="always_load_without_extra_per_attachment_split"`.

## 2026-05-22 — Task 134: classify whether UI active-scope reconstruction is supposed to recompute generic defaults or preserve Tonemap’s carried packet

Artifact roots used:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-pass-origin-attribution-vulkan-sourcebuild-20260522-093400/`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`

No new runtime rerun was needed for this fork. The locked artifacts already proved the live seam facts we needed to classify against source:

- UI re-entry comes from `RendererCanvasRenderRD::_render_batch_items()` with `draw_list_begin_flags=RD::DRAW_DEFAULT_ALL` and `clear_requested=false`.
- the carried Tonemap packet stays exact through `tonemap_end_state == l88_begin_state` with slot 0 still `CLEAR`.
- the first and only active-vs-carried attachment exact mismatch before `L88` is slot-0 `load_op`, introduced by `active_pre_rebind_scope_reconstruction`.

### Smallest added diagnostic

No new engine-side probe was necessary. The smallest honest diagnostic for this fork was exact source-chain attribution of whether any carry-forward/preserve mechanism exists on the UI draw-list path at all.

### Exact source classification

The codepath answers the fork directly:

1. `RendererCanvasRenderRD::_render_batch_items()` creates a **fresh** UI draw list with:
   - `RD::draw_list_begin(framebuffer, ui_draw_flags, ... , RDD::BreadcrumbMarker::UI_PASS)`
   - on this lane `ui_draw_flags = RD::DRAW_DEFAULT_ALL`
   - there is no argument carrying Tonemap’s prior per-attachment `load_op`, render-pass recipe hash, or any prior active-scope packet

2. `RenderingDevice::draw_list_begin()` (`servers/rendering/rendering_device.cpp`) constructs a brand-new `operations[]` array per framebuffer attachment.
   - each attachment starts as `RDG::ATTACHMENT_OPERATION_DEFAULT`
   - color attachments only change away from `DEFAULT` when the caller explicitly sets `DRAW_CLEAR_COLOR_*` or `DRAW_IGNORE_COLOR_*`
   - with `RD::DRAW_DEFAULT_ALL`, slot 0 stays `ATTACHMENT_OPERATION_DEFAULT`
   - the function then hands only these freshly computed operations, clear values, and stage bits to `draw_graph.add_draw_list_begin(...)`
   - it does **not** consult or import the currently carried Tonemap-side packet / prior active render-pass recipe when forming the new draw-list begin state

3. `RenderingDeviceGraph::_add_draw_list_begin()` / `_run_draw_list_command()` then materialize the render pass from that new attachment-operation vector.
   - for slot 0 on this lane, the runtime-proven branch is `source="non_discardable_default_load_contract"`
   - because the root tracker is non-discardable and no explicit clear/ignore override was supplied, the graph resolves slot 0 to `ATTACHMENT_LOAD_OP_LOAD`

### Exact conclusion

On this locked UI lane, active-scope reconstruction is **supposed to recompute attachment ops from the new draw-list’s generic/default attachment operations**, not preserve the carried Tonemap packet automatically.

There is no existing carry-forward/preserve mechanism on this path for Tonemap’s carried exact packet:

- no `draw_list_begin()` parameter for “preserve prior recipe/load-op packet”
- no code in `draw_list_begin()` that seeds `operations[]` from current command-buffer/render-pass state
- no graph-side branch that prefers a carried prior attachment recipe over the freshly supplied `ATTACHMENT_OPERATION_DEFAULT` + tracker-discardability policy

So the UI-side `LOAD` is not caused by a missing copy of Tonemap’s `CLEAR` inside an otherwise preserve-aware path. It is caused by the path being designed around **fresh draw-list reconstruction**, where `RD::DRAW_DEFAULT_ALL` means “leave attachment ops at default and let RDG solve them from explicit flags + tracker policy.” On this lane that generic solve truthfully falls through to the non-discardable default-load rule, yielding slot-0 `LOAD`.
- The carried Tonemap `CLEAR` contract is not preserved because the UI active-scope reconstruction is a separate draw-list/render-pass creation that recomputes attachment ops from draw flags plus tracker discardability, not from the carried Tonemap packet's exact render-pass recipe.

## 2026-05-22 — Task 136: classify whether comparable normal Tonemap-to-UI handoffs also show the same `CLEAR`-to-`LOAD` contrast without crashing

Artifact root used:
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-normal-tonemap-ui-compare-vulkan-sourcebuild-20260522-1335/`

Validation/control setup:
- Built the smallest honest non-failing comparison lane on the same source-built Vulkan editor instead of reopening the crashing GDGS repro.
- The control was a tiny vanilla Forward+ 3D project with a `WorldEnvironment`, opaque mesh, and blended `CanvasLayer` / `ColorRect` overlay, configured to quit after a few dozen frames.
- Exact launch used the existing UI-origin diagnostic only: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 ./bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/.temp/tonemap-ui-compare-20260522`
- Result: `exit_status=0`

What the normal control lane says:

- The UI pass is still a preserve-content lane in the normal control, not a full-overwrite shortcut:
  - `ui_pass_origin={... overwrite_classifier="preserve_prior_root_contents", overwrite_classifier_basis="blend_or_clip_or_multi_batch_requires_prior_contents"}`
- The late Tonemap -> UI scope pair stays on the same attachment recipe instead of showing the failing repro's sharp contrast:
  - `owner_label="Tonemap (L5) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `owner_label="Command Graph (L6) (Draw)" breadcrumb=UI_PASS ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
- The same aligned pattern also appears on the first warmup frame:
  - `owner_label="Tonemap (L7) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `owner_label="Command Graph (L8) (Draw)" breadcrumb=UI_PASS ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
- Repeated frame stalls remain healthy throughout the run:
  - `frame_stall_end ... fence_wait_error=0`

Comparison against the failing repro lane:

- The comparable normal lane still uses the same generic preserve-content UI policy (`RD::DRAW_DEFAULT_ALL` + non-discardable root tracker semantics), so Task 135's policy read stays intact.
- But the normal lane does **not** reproduce the failing lane's Tonemap-carried `CLEAR` versus UI-rebuilt `LOAD` contrast.
- In the normal control, Tonemap itself is already using the same active/root `LOAD` recipe that the UI pass rebuild later reuses, so the Tonemap -> UI handoff stays `LOAD` -> `LOAD` on the shared late-pass render-pass recipe.
- The crashing repro remains unusual at exactly this boundary because its carried Tonemap packet still reaches the UI seam with slot 0 `CLEAR`, while the fresh UI active-scope reconstruction on the same root tracker flips back to `LOAD` before `L88`.

Exact conclusion:

- Comparable normal Tonemap-to-UI handoffs in this control do **not** show the same `CLEAR`-to-`LOAD` contrast without crashing.
- The generic UI/root non-discardable default-`LOAD` policy remains normal, but the sharp carried-`CLEAR` versus rebuilt-`LOAD` juxtaposition is **specific to the failing repro lane**, not a routine engine-wide Tonemap -> UI handoff pattern.

## 2026-05-22 — Task 137: classify why the failing Tonemap packet still reaches the UI handoff as slot-0 `CLEAR`

Artifact roots used:
- Failing repro lane: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-ui-attachment-subfield-vulkan-sourcebuild-20260522-105057/`
- Healthy comparable control (`overwrite` experiment off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-normal-tonemap-ui-compare-vulkan-sourcebuild-20260522-1335/`
- Healthy comparable control (`overwrite` experiment on): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-normal-tonemap-ui-compare-overwrite-on-vulkan-sourcebuild-20260522-1402/`

Minimum extra comparison run:
- Reused the same tiny healthy-control project from Task 136 and enabled `GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1` alongside `GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1`.
- Exact launch: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 GODOT_GDGS_DEBUG_TONEMAP_OVERWRITE_CONTRACT=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/.temp/tonemap-ui-compare-20260522`
- Result: `exit_status=0`

What the Tonemap-side attribution now says:

- The failing repro lane explicitly enabled the direct-root overwrite experiment, so Tonemap was not on the ordinary preserve/load contract:
  - `tonemap_overwrite_contract_experiment enabled=true draw_flags=0x20 note="forcing DRAW_IGNORE_COLOR_0 for direct-root tonemap overwrite experiment"`
- That same failing Tonemap draw begins on an ignore-family active render-pass scope:
  - `draw_list_render_pass_create ... attachment_load_ops=[0:DONT_CARE] ... attachment_operation_source="attachment_operation_ignore" ... owner_label="Tonemap (L87) (Draw)"`
- Across the zero-gap Tonemap -> `L88` boundary, only the Tonemap pipeline packet survives live; the UI pass is not carrying an already-rebuilt `LOAD` scope:
  - `boundary_state_handoff_classifier={classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index" ...}`
- The surviving carried packet is still tied to a compatible-only Tonemap pipeline lineage whose exact attachment recipe was baked with slot-0 `CLEAR`, and the UI pass is the first place that rebuilds the active scope back to `LOAD`:
  - `pre_rebind_carried_packet_contract={... pipeline_load_ops=["0:CLEAR"], active_load_ops=["0:LOAD"], load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD" ...}}`
- The healthy comparable control without the overwrite experiment stays on the normal load-preserving Tonemap contract the whole time:
  - `owner_label="Tonemap (L7) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `owner_label="Command Graph (L8) (Draw)" breadcrumb=UI_PASS ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
- Enabling the same overwrite experiment on that healthy comparable control flips Tonemap onto the same ignore-family active contract, proving the Tonemap-side contract change is experiment-driven rather than inherently tied to the crashing scene:
  - `tonemap_overwrite_contract_experiment enabled=true draw_flags=0x20 ...`
  - `draw_list_render_pass_create ... attachment_load_ops=[0:DONT_CARE] ... attachment_operation_source="attachment_operation_ignore" ... owner_label="Tonemap (L7) (Draw)"`

Exact conclusion:

- The failing lane's slot-0 carried `CLEAR` is a **Tonemap-owned packet-lineage artifact** introduced by the direct-root overwrite experiment path.
- It is **not** a UI-pass-originated contract choice: the UI handoff is the first place that reconstructs the active scope back to slot-0 `LOAD` on the preserved root tracker.
- The practical fork answer is: the failing packet still arrives at the UI seam as `CLEAR` because the Tonemap pipeline packet stays live across the zero-gap boundary and its compatible-only render-pass lineage still carries the older slot-0 `CLEAR` exact recipe; healthy comparable lanes only reach that seam as `LOAD` when Tonemap stays on the default preserve/load contract instead of the overwrite experiment path.

## 2026-05-22 — Task 138: re-measure the failing-vs-healthy Tonemap-to-UI handoff with the overwrite experiment disabled

Artifact roots used:
- Failing repro lane (`overwrite` off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-vulkan-sourcebuild-20260522-145814/`
- Healthy comparable control (`overwrite` off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-control-vulkan-sourcebuild-20260522-145842/`

Exact launches:
- Failing repro: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW=1 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --script /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-17/run_stage_case_checkpoint.gd -- projection_only__disabled /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-vulkan-sourcebuild-20260522-145814 no_present compositor projection_only disabled 120`
- Healthy control: `DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1 /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 --display-driver wayland --rendering-driver vulkan --path /home/derrick/.openclaw/workspace/.temp/tonemap-ui-compare-20260522`

Results:
- The overwrite experiment really was off on the failing rerun: there are **no** `tonemap_overwrite_contract_experiment` lines in the failing artifact.
- The healthy control stayed on the already-known ordinary preserve/load handoff:
  - `ui_pass_origin={... overwrite_classifier="preserve_prior_root_contents" ...}`
  - `owner_label="Tonemap (L5) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `owner_label="Command Graph (L6) (Draw)" breadcrumb=UI_PASS ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `fence_wait_error=0`, `exit_status=0`
- With the overwrite experiment disabled, the failing lane's Tonemap/UI scope pair also presents as `LOAD` -> `LOAD` at the late handoff itself:
  - `ui_pass_origin={... overwrite_classifier="preserve_prior_root_contents" ...}`
  - `owner_label="Tonemap (L87) (Draw)" ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
  - `owner_label="Command Graph (L88) (Draw)" breadcrumb=UI_PASS ... attachment_load_ops=[0:LOAD] attachment_exact_hash=0x6529dc72`
- The failing artifact still preserves a narrower internal contrast inside the `L88` rebuild path rather than at the outer Tonemap->UI scope pair:
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", ... tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD", slot_index=0}`
  - `boundary_state_handoff_classifier={classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index", ... minimum_distinguishing_hazard="carried_pipeline_packet"}`
- The crash signature itself did not disappear on the failing lane: it still ends at `fence_wait_error submit_serial=9 wait_result=-4` and later `frame_stall_end frame=1 fence_wait_error=1`.

Exact conclusion:

- Once the overwrite experiment is honestly disabled, the **sharp outer Tonemap-to-UI `CLEAR` vs `LOAD` contrast does not survive cleanly**.
- The failing and healthy lanes now agree at the visible Tonemap/UI scope pair itself: both are `LOAD` -> `LOAD` with the same late-pass attachment hash (`0x6529dc72`) and the same preserve-content UI classifier.
- The surviving failing-only evidence shifts inward: the remaining `CLEAR` vs `LOAD` split is now an **internal `L88` active-scope reconstruction contrast** (`tonemap_end_load_op="CLEAR"` / `pre_rebind_active_load_op="LOAD"`) carried through the zero-gap handoff, not a clean outer Tonemap-scope-vs-UI-scope contrast.
- So Task 137's earlier “Tonemap reaches UI as `CLEAR` while healthy lanes reach it as `LOAD`” reading was contaminated by the overwrite experiment. The honest overwrite-off answer is narrower: the crash lane still carries a poisoned packet into `L88`, but the clean scope-level Tonemap-to-UI handoff no longer differs from the healthy control.

## 2026-05-22 — Task 139: classify the exact internal `L88` pre-rebind reconstruction step that flips `tonemap_end_load_op=CLEAR` to `pre_rebind_active_load_op=LOAD`

Artifact roots used (reused from Task 138; no additional reruns were needed):
- Failing repro lane (`overwrite` off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-vulkan-sourcebuild-20260522-145814/`
- Healthy comparable control (`overwrite` off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-control-vulkan-sourcebuild-20260522-145842/`

What the already-captured failing artifact proves:

- The zero-gap Tonemap -> `L88` boundary itself preserves only the carried Tonemap pipeline packet; there is no active render-pass scope live at `l88_begin`:
  - `boundary_state_handoff_classifier={classification="carried_pipeline_packet_then_l88_reestablishes_scope_rebinds_and_adds_vertex_index", ...}`
  - `tonemap_end_snapshot={render_pass_active=false, ... pipeline_provenance.render_pass_create_serial=9, ... render_pass_attachment_exact_hash="0x63c10583"}`
  - `l88_begin_snapshot={render_pass_active=false, ... pipeline_provenance.render_pass_create_serial=9, ... render_pass_attachment_exact_hash="0x63c10583"}`
- The first exact step that reintroduces the opposing active `LOAD` is **`L88`'s own `begin_render_pass` using the already-created `LOAD` render-pass object before the first `L88` pipeline bind**:
  - `reuse_vs_reestablish={... l88_reestablishes_scope_before_own_pipeline=true, l88_first_pipeline_bind_before_state={render_pass_active=true, breadcrumb="UI_PASS", render_pass_create_serial=13, render_pass_attachment_exact_hash="0x6529dc72", ...}}`
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", first_attributable_step="active_pre_rebind_scope_reconstruction", tonemap_end_load_op="CLEAR", l88_begin_load_op="CLEAR", pre_rebind_pipeline_load_op="CLEAR", pre_rebind_active_load_op="LOAD" ...}`
- That `LOAD` scope is not freshly invented by the `L88` pipeline bind itself. It comes from the cached render-pass object that the frame already materialized on the same root tracker with the default non-discardable policy:
  - `draw_list_render_pass_create key=0x0 ... label="Tonemap" ... source="non_discardable_default_load_contract" ... tracker_write_index=132 ... tracker_name="Render Target Color" ...`
  - `begin_render_pass_scope create_serial=13 ... owner_label="Tonemap (L87) (Draw)" ... attachment_load_ops=[0:LOAD] ...`
  - `begin_render_pass_scope create_serial=13 ... owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD] ...`
- So the flip is specifically: **the carried Tonemap packet keeps render-pass create-serial `9` / attachment hash `0x63c10583` (`CLEAR`), then `L88`'s pre-rebind `begin_render_pass` reactivates cached render-pass create-serial `13` / attachment hash `0x6529dc72` (`LOAD`) before `L88` binds its own pipeline.**

How the healthy comparable lane differs:

- The healthy control uses the same UI-side preserve-content policy and the same non-discardable default-`LOAD` render-pass creation path:
  - `ui_pass_origin={... overwrite_classifier="preserve_prior_root_contents" ...}`
  - `draw_list_render_pass_create key=0x0 ... label="Tonemap" ... source="non_discardable_default_load_contract" ...`
  - `begin_render_pass_scope create_serial=16 ... owner_label="Tonemap (L5) (Draw)" ... attachment_load_ops=[0:LOAD] ...`
  - `begin_render_pass_scope create_serial=16 ... owner_label="Command Graph (L6) (Draw)" ... attachment_load_ops=[0:LOAD] ...`
- The important difference is **not** that healthy `L6` avoids the pre-rebind `begin_render_pass` reconstruction step. It does the same kind of `LOAD`-flavored scope reactivation. The difference is that the healthy lane arrives at that step already aligned: Tonemap's carried packet is also on the same `LOAD` recipe, so there is no internal `CLEAR` -> `LOAD` split to expose.

Exact conclusion:

- The exact internal pre-rebind reconstruction step is **`L88`'s `begin_render_pass` scope reactivation that reuses the cached non-discardable-default-`LOAD` render-pass object (`create_serial=13`, attachment hash `0x6529dc72`) before the first `L88` pipeline bind.**
- The failing lane differs from the healthy comparable lane only in what survives into that step: failing still carries Tonemap pipeline packet render-pass lineage `create_serial=9` / attachment hash `0x63c10583` / slot-0 `CLEAR`, while healthy arrives with Tonemap already aligned to the same `LOAD` recipe that the UI-side scope reactivation uses.
- No engine code changed and no commit was made in this pass; the existing Task 138 diagnostics were already sufficient to classify the step honestly, so I kept the work reversible and limited to durable note updates.
## 2026-05-22 — Task 140: classify why the overwrite-off failing Tonemap lane still carries the `CLEAR`-side recipe before `L88`

Artifact roots reused:
- failing repro lane (`overwrite` off, lazy shared-view debug gate still on): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-vulkan-sourcebuild-20260522-145814/`
- healthy comparable control (`overwrite` off, lazy shared-view debug gate off): `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-control-vulkan-sourcebuild-20260522-145842/`

### Smallest diagnostic used

No new engine edits or reruns were required for this fork. The existing overwrite-off artifact pair already contains the needed Tonemap-side routing evidence:

- `[gdgs-ts] render_target_policy_cause ... lazy_shared_view_experiment=true|false ... render_target_texture_shared_view=true|false`
- `[gdgs-ts] tonemap_render_target_lane ... lane_policy=... shared_view_materialized=...`
- `[gdgs-rdg] draw_list_render_pass_create ... label="Tonemap" ... tracker_name=... source="non_discardable_default_load_contract"`
- the Task 139 zero-gap carry / `L88` pre-rebind backend lineage snapshots

### Runtime classification

The overwrite-off failing lane is **not** the same Tonemap-side route as the healthy overwrite-off control before `L88`.

Failing overwrite-off lane:
- `render_target_policy_cause` reports `primary_cause=render_target_texture_shared_view_rule_lazy_debug_gate`
- `lazy_shared_view_experiment=true`
- `render_target_texture_shared_view=false`, `srgb_shared_view=false`
- `tonemap_render_target_lane` reports `lane_policy=persistent_root_direct_lazy_shared_view`
- `shared_view_materialized=false`
- `shared_view_entrypoints={viewport_texture_requests=0,texture_rd={base=0,srgb=0,total=0},native_handle={base=0,srgb=0,total=0},total=0}`
- `attachments={color=RID:8748848381987,render_target_texture=RID:3672197038081}`
- visible Tonemap draw-list creation is still a root non-discardable default-`LOAD` recipe (`label="Tonemap"`, `tracker_name="Render Target Color"`), but the carried zero-gap packet stays tied to earlier Tonemap pipeline lineage `create_serial=9`, attachment hash `0x63c10583`, slot-0 `CLEAR`.

Healthy overwrite-off control:
- `render_target_policy_cause` reports `primary_cause=render_target_texture_shared_view_rule`
- `lazy_shared_view_experiment=false`
- `render_target_texture_shared_view=true`, `srgb_shared_view=true`
- `tonemap_render_target_lane` reports `lane_policy=persistent_root_sampled_shared`
- `shared_view_materialized=true`
- visible Tonemap draw-list creation already lands on the non-discardable default-`LOAD` recipe with tracker `RID:4037269258270`
- Task 139's comparison already showed the healthy comparable lane reaches the same UI-side `L88` reactivation with Tonemap aligned to the `LOAD` recipe instead of carrying the older `CLEAR` lineage.

### Exact conclusion

The exact upstream Tonemap-side divergence is the **lazy shared-view debug gate on the failing overwrite-off lane**. That gate leaves the render target on the narrower `persistent_root_direct_lazy_shared_view` contract with no materialized shared view before failure, while the healthy control stays on the eager `persistent_root_sampled_shared` contract.

Why the failing lane still carries the `CLEAR`-side recipe before `L88`:
- Tonemap is routed through the direct-root lazy-shared-view lane (`shared_view_materialized=false`), so the zero-gap carry keeps the already-live Tonemap pipeline packet that was baked earlier against the root-side `CLEAR` render-pass lineage (`create_serial=9`, attachment hash `0x63c10583`).
- `L88` does not inherit that packet's attachment exact recipe. It reactivates the cached UI/root preserve-content render pass on the same non-discardable root tracker, which is why Task 139 still sees `begin_render_pass` switch back to the cached `LOAD` lineage (`create_serial=13`, attachment hash `0x6529dc72`) before the first `L88` bind.
- The healthy control differs upstream because its Tonemap lane is already on the eager shared-view / sampled-shared policy, so the comparable Tonemap packet is created on the same default preserve-content `LOAD` recipe that the UI-side reactivation later uses.

So the next-fork answer is: the failing overwrite-off lane does **not** align to the cached UI-side `LOAD` recipe before `L88` because its Tonemap packet comes from the lazy-shared-view direct-root branch, where the live carried Tonemap pipeline lineage remains the earlier root-side `CLEAR` recipe; the healthy lane differs exactly because that lazy shared-view condition is absent and Tonemap is already created under the shared preserve-content `LOAD` contract.

## 2026-05-22 — Task 141: rerun the same failing overwrite-off lane with the lazy shared-view debug gate disabled

Artifact roots:
- prior failing overwrite-off lane with lazy shared-view gate still on: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-vulkan-sourcebuild-20260522-145814/`
- healthy overwrite-off control already captured earlier: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-control-vulkan-sourcebuild-20260522-145842/`
- new exact failing-lane rerun with the lazy shared-view debug gate removed: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-22/official-tonemap-ui-compare-overwrite-off-failing-no-lazy-gate-vulkan-sourcebuild-20260522-160300/`

### Smallest honest rerun

I reran the exact source-built host-Vulkan failing lane command line, still with `GODOT_GDGS_DEBUG_UI_PASS_ORIGIN=1`, but **without** `GODOT_GDGS_DEBUG_LAZY_RT_SHARED_VIEW`. No engine code changed.

### What changed in the Tonemap-side recipe

With the lazy shared-view debug gate removed, the former failing lane realigns to the same Tonemap/UI preserve-content path as the healthy overwrite-off control:

- `render_target_policy_cause` flips from `primary_cause=render_target_texture_shared_view_rule_lazy_debug_gate` to the normal `primary_cause=render_target_texture_shared_view_rule`
- `lazy_shared_view_experiment=false`
- `tonemap_render_target_lane` flips from `lane_policy=persistent_root_direct_lazy_shared_view` to `lane_policy=persistent_root_sampled_shared`
- `shared_view_materialized=true`
- Tonemap and the following UI scope both use the same `LOAD` render-pass lineage:
  - `begin_render_pass_scope create_serial=13 ... owner_label="Tonemap (L87) (Draw)" ... attachment_load_ops=[0:LOAD] ... attachment_exact_hash=0x6529dc72`
  - `begin_render_pass_scope create_serial=13 ... owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD] ... attachment_exact_hash=0x6529dc72`
- The no-gate rerun also keeps the same Task 139-style pre-rebind explanation once it reaches `L88`:
  - `load_op_attribution_split={classification="active_scope_rebuild_first_attributable_step", ... pre_rebind_active_load_op="LOAD" ...}`

So the Tonemap-side recipe **does** realign to the healthy `LOAD` path when the lazy shared-view debug gate is disabled.

### What did not change: crash outcome

The crash survives the recipe realignment and stays on the same submit/wait path:

- `fence_wait_error submit_serial=9 ... wait_result=-4`
- `frame_stall_end frame=1 fence_wait_error=1`
- process exit status: `134`

In other words, removing the lazy shared-view debug gate fixes the Tonemap-side `CLEAR` vs `LOAD` divergence, but it does **not** remove or relocate the host-Vulkan crash. The failure still dies on the frame-1 main command-graph wait after the same `submit_serial=9` submission.

### Exact conclusion

Task 140's conclusion was real: the lazy shared-view debug gate was the honest reason the overwrite-off failing lane carried the wrong Tonemap-side recipe upstream. But Task 141 shows that this recipe divergence is **not** the device-loss root cause by itself. Once the gate is removed, the lane matches the healthy `persistent_root_sampled_shared` / `LOAD` Tonemap path and still crashes at the same `submit_serial=9` fence-wait failure.

That means the next investigation fork should treat the lazy shared-view gate as a recipe confounder that is now eliminated, not as the surviving crash trigger.

## 2026-05-24 — first-L88 post-create / driver-pipeline ownership provenance QA

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-post-create-provenance-qa-vulkan-sourcebuild-20260524-171825/`
- harness: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_post_create_provenance_qa.py`
- binary proof: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-post-create-provenance-qa-vulkan-sourcebuild-20260524-171825/runtime_binary_proof.txt`
- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-post-create-provenance-qa-vulkan-sourcebuild-20260524-171825/post_create_provenance_summary.tsv`
- comparison: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-post-create-provenance-qa-vulkan-sourcebuild-20260524-171825/post_create_provenance_compare.txt`

### What changed in the instrumentation

- `RenderingDevice::render_pipeline_create(...)` now emits `driver_pipeline_id` alongside the existing first-L88 RD create fingerprint.
- The same first-L88 target now arms a second default-off marker at `draw_list_bind_render_pipeline()`:
  - `temp_diag_first_clipped_preserve_rect_post_create_provenance=`
- That second marker records the create→bind ownership handoff for the same pipeline:
  - created vs bound render-pipeline RID
  - created vs bound driver-pipeline ID
  - created vs bound shader-driver ID
  - carried vertex-format ID and specialization constant `0`

### What the three approved reruns showed

Across `baseline`, `specialization_only`, and `vertex_input_only`:

- the outer crash identity stayed locked:
  - `submit_serial=9` present
  - `fence_wait_error submit_serial=9` present
  - tail still includes `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
  - later breadcrumb still reaches `BLIT_PASS`
  - exit status stays `-6`
- the process-local renderer RID still recycles numerically in all three fresh launches:
  - `allocated_render_pipeline_rid=11343008628747`
  - `created_render_pipeline_rid=11343008628747`
  - `bound_render_pipeline_rid=11343008628747`
- but the earliest backend-owned identity after that RID does **not** collapse:
  - baseline: `driver_pipeline_id=123590973675520`
  - specialization-only: `driver_pipeline_id=131215950987168`
  - vertex-input-only: `driver_pipeline_id=137637126010544`
- in every case, the new bind marker proves the ownership handoff is internally consistent inside that run:
  - `render_pipeline_rid_matches_create=true`
  - `driver_pipeline_id_matches_create=true`
  - `shader_driver_id_matches_create=true`

### Exact conclusion

The new provenance seam answers the post-create question cleanly: the repeated `RID` is only a process-local recycled handle, while the first bound/consumed backend-owned pipeline identity still diverges per case and preserves the same case-specific distinctions already seen at RD create time.

So the locked crash lane now stays divergent through:

1. request hash / selector recipe
2. `RenderingDevice::render_pipeline_create(...)`
3. the first bound driver-pipeline ownership path at `draw_list_bind_render_pipeline()`

That means there is still no honest convergence point before first consumption of the graphics pipeline on this seam.

## 2026-05-24 — first-L88 execution-packet QA

- artifact root: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/`
- harness: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_execution_packet_qa.py`
- binary proof: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/runtime_binary_proof.txt`
- summary: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/execution_packet_summary.tsv`
- comparison: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/execution_packet_compare.txt`

### What changed in the instrumentation

- The first-L88 post-create / first-bind provenance seam now arms a one-shot execution marker at the first `RenderingDevice::draw_list_draw(...)` that uses that exact bound pipeline:
  - `temp_diag_first_clipped_preserve_rect_execution_packet=`
- That marker records the immediate submission bundle for the first executed L88 packet:
  - descriptor / uniform state (`set_count`, expected/dirty masks, batched bind count, and exact uniform-set RID/driver-ID pairs)
  - vertex binding state (direct-bind vs vertex-array source, bound driver buffer IDs, and offsets)
  - index binding state
  - first draw submission shape (`draw_indexed`, `instances`, `to_draw`, `draw_count_before`)
- To keep the vertex slice honest on the direct-bind path, draw-list state now preserves the currently bound vertex-buffer IDs and offsets even when the packet does not come from a cached vertex-array RID.

### What the three approved reruns showed

Across `baseline`, `specialization_only`, and `vertex_input_only`:

- the outer crash identity stayed locked:
  - `submit_serial=9` present
  - `fence_wait_error submit_serial=9` present
  - tail still includes `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
  - later breadcrumb still reaches `BLIT_PASS`
  - exit status stays `-6`
- the first real executed packet still carries the same per-case identity split already seen upstream:
  - baseline: `exec_driver_pipeline_id=127052114409296`, `exec_vertex_format_id=2`, `exec_specialization_constant_0=0x0`
  - specialization-only: `exec_driver_pipeline_id=124354939642144`, `exec_vertex_format_id=2`, `exec_specialization_constant_0=0x2`
  - vertex-input-only: `exec_driver_pipeline_id=135993698385776`, `exec_vertex_format_id=3`, `exec_specialization_constant_0=0x0`
- the descriptor topology converges even while the identities differ:
  - `expected_mask=dirty_mask=0xd`
  - sets `0`, `2`, and `3` are the expected / initially dirty descriptor sets
  - descriptor binding stays batched with `descriptor_bind_call_count=2`
- the draw submission shape also converges:
  - `draw_indexed`
  - `index_count=6`
  - `instances=27`
  - `to_draw=6`
  - `draw_count_before=0`
- the vertex-input-only case now proves the direct vertex-binding payload itself diverges:
  - baseline / specialization-only: `buffer_bind_count=1`
  - vertex-input-only: `buffer_bind_count=2`
  - both vertex-input-only bindings point at the same driver buffer ID with offset `0`, i.e. duplicated direct-bind payload for bindings `0` and `1`

### Exact conclusion

The first honest execution-packet seam still does **not** show convergence before the crash. The same case split survives all the way into the first real executed L88 draw packet: backend pipeline identity still differs, specialization-only still carries its specialization bit, and vertex-input-only still carries both its distinct vertex format and its duplicated direct vertex-buffer bind payload.

What *does* converge by this seam is the higher-level packet shape around those identities: all three cases bind the same descriptor-set topology, issue the same indexed draw shape, and then die inside the same locked `submit_serial=9` fence-wait crash envelope.

## 2026-05-24 audit follow-up — Task 164 (`oc-ins8`)

Auditor re-checked the same package directly rather than widening the lane.

Decisive artifacts:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/execution_packet_summary.tsv`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/execution_packet_compare.txt`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-packet-qa-vulkan-sourcebuild-20260524-221841/outer_crash_identity.json`
- per-case `marker_lines.json` files under `baseline/`, `specialization_only/`, and `vertex_input_only/`

Audit findings:

- Divergence still survives *inside* the first real executed packet, not just upstream of it.
- Truly converged fields are only the execution topology / shape:
  - descriptor topology: `set_count=4`, `expected_set_count=3`, `dirty_set_count=3`, `expected_mask=0xd`, `bound_mask=0x0`, `dirty_mask=0xd`, `descriptor_bind_call_count=2`, `descriptor_bind_mode=batched`, same active slots `0/2/3`
  - draw shape: `draw_indexed`, `index_count=6`, `instances=27`, `to_draw=6`, `draw_count_before=0`, `index_format=uint16`, `index_offset_bytes=0`
  - outer crash envelope: same `submit_serial=9` -> `fence_wait_error submit_serial=9` -> later `BLIT_PASS`, exit `-6`
- Still-divergent execution payload fields are backend-owned resource identities and lane-specific packet contents:
  - `exec_driver_pipeline_id`
  - `exec_shader_driver_id`
  - `exec_specialization_constant_0` (specialization-only only)
  - `exec_vertex_format_id`
  - `exec_vertex_buffer_bind_count` plus the actual bound driver-buffer payload
  - `exec_index_driver_id`
  - per-set `uniform_set_driver_id` values, even though the slot topology matches

The honest next slice therefore stays inside this same packet: inspect the exact resource-consumption boundary where those already-divergent descriptor-set / vertex-buffer / index-buffer identities are handed into backend command emission for the first `draw_indexed` packet, rather than reopening upstream pipeline-selection theories or widening into a fix.

## 2026-05-24 — First L88 backend command-emission boundary (submit_serial=9, source build)

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-command-emission-boundary-vulkan-sourcebuild-20260524-234738/`

Key files:
- `command_emission_boundary_compare_v2.txt`
- `command_emission_boundary_results.json`
- `runtime_binary_proof.txt`
- per-case `stdout.log`, `stderr.log`, `marker_lines.json`, `exact_command.txt`

### Locked cases rerun

- `baseline`
- `specialization_only`
- `vertex_input_only`

All three preserved the same outer crash identity:
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- process exit still `-6`

### Backend command-emission finding

The new Vulkan-driver marker `l88_command_emission_boundary=` shows that convergence still does **not** appear at backend command recording. The first bound-resource payloads and the first `draw_indexed` consumer state remain case-divergent when the backend records them.

Observed boundary facts:
- descriptor binds stay split into the same two calls in every case (`set 0`, then `sets 2+3`), but the actual descriptor-set driver IDs differ per case
- `specialization_only` already diverges at the first descriptor-set driver IDs even though its vertex-format shape still matches `baseline`
- `vertex_input_only` diverges at the descriptor-set driver IDs **and** changes the first vertex bind shape from one binding to two mirrored bindings; both bindings carry offset `2097152`
- index-buffer format/offset stay stable (`uint16`, offset `0`), but the actual backend index-buffer driver ID still differs per case
- the first `draw_indexed` consumer boundary (`serial=18`) consumes those same case-divergent descriptor / vertex / index payloads directly; no convergence appears at the handoff into `vkCmdDrawIndexed`
- the consumer still sees a carried set-1 descriptor-set driver ID in addition to the newly emitted set `0`, `2`, and `3` payloads, and that carried set-1 driver ID also differs per case

### Practical conclusion

The locked crash seam remains upstream of any hypothetical convergence inside backend command recording. By the time the first failing L88 packet reaches the actual descriptor-bind / vertex-bind / index-bind / `draw_indexed` emission boundary, the three approved cases are still materially different in the concrete backend resources being recorded.

### Audit confirmation

Auditor re-checked the package against `command_emission_boundary_summary.tsv`, `command_emission_boundary_compare_v2.txt`, `command_emission_boundary_results.json`, `outer_crash_identity.json`, and each case's `marker_lines.json`. The honest reading is:
- divergence is still present at the first emitted descriptor payloads, first emitted vertex payload, first emitted index payload, and the first `draw_indexed` consumer boundary
- convergence exists only in command scaffolding: descriptor-call split/order (`set 0`, then `sets 2+3`), bind serials (`14/15/16/17`), consumer serial (`18`), draw arguments (`index_count=6`, `instance_count=27`, `first_index=0`, `vertex_offset=0`, `first_instance=0`), index format/offset (`uint16`, `0`), and the shared vertex-buffer offset (`2097152`)
- the still-divergent backend payload fields are the actual descriptor-set driver IDs for sets `0`, `2`, `3`, the carried set-`1` descriptor-set driver ID at draw time, the concrete vertex-buffer driver ID payloads (plus `vertex_input_only`'s second mirrored binding), and the concrete index-buffer driver ID

That means the next honest seam remains inside the same backend resource-payload family: trace where those exact emitted descriptor-set and buffer driver objects are first realized / sourced, instead of widening back out to upstream request-hash, specialization-cache, or CPU-side vertex-format-cache theories.

## 2026-05-25 — First-L88 earliest backend realization boundary (`submit_serial=9`)

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-015220/`
- corrected comparison: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-015220/resource_realization_boundary_compare_corrected.txt`
- corrected parsed payloads: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-015220/resource_realization_boundary_corrected.json`

### Outer crash identity

The same three approved cases were rerun and all preserved the same outer crash identity:
- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status still lands at `-6`

### Earliest backend realization findings

New driver-side marker: `l88_resource_realization_boundary=`.

Descriptor-set realization (`vkAllocateDescriptorSets` / `uniform_set_create`) findings for the exact set `0/1/2/3` objects consumed by the first emitted L88 packet:
- all three cases hit the same realization ordinals for these exact slots: set `2` at create ordinal `3`, set `1` at `32`, set `0` at `35`, set `3` at `36`
- all three cases keep the same declared set indices, binding counts, write counts, pool-key hashes, and binding-signature hashes for sets `0/1/2/3`
- despite that structural convergence, the exact descriptor-set payload already diverges at this earliest backend realization boundary:
  - `resource_object_hash` differs across cases for sets `0`, `1`, `2`, and `3`
- buffer-backed subpayload realization at descriptor-set creation splits more narrowly:
  - set `2` keeps the same `buffer_realization_hash` across all three cases, so its buffer-backed realization recipe is converged even though the exact descriptor-set object still differs
  - sets `0`, `1`, and `3` already differ in `buffer_realization_hash`, so those descriptor-set payload families are structurally divergent as soon as the backend realizes them

First vertex-buffer and index-buffer realization (`buffer_create`) findings for the exact L88-consumed buffer objects:
- baseline and specialization-only consume one vertex binding; vertex-input-only consumes two bindings, but both bindings point at the same realized buffer object/recipe
- the first vertex-buffer object keeps the same create ordinal (`385`), usage mask (`0x81`), requested size (`2097152`), allocation size (`4194304`), frames-drawn (`5`), dynamic flag (`true`), frame slot (`1`), and realization recipe hash (`0x78fdd30e`) across all three cases
- the first index-buffer object keeps the same create ordinal (`25`), usage mask (`0x43`), requested size (`16`), allocation size (`16`), frames-drawn (`3`), dynamic flag (`false`), frame slot (`4294967295`), and realization recipe hash (`0x5e1dfda9`) across all three cases
- `vertex_input_only`'s only new buffer-side divergence at this boundary is the extra mirrored binding `1`; it does **not** realize a different underlying first vertex-buffer object

### Practical conclusion

For the exact concrete descriptor-set objects consumed by the first failing L88 packet, case divergence is already present at the earliest backend realization boundary. Convergence there survives only in the structural shell (same create ordinals / set indices / binding signatures), not in the exact realized descriptor payloads.

For the first vertex-buffer and index-buffer objects, the earliest backend realization boundary still converges structurally across all three cases. Their first backend recipes match exactly; the only vertex-input-only delta at that boundary is the second mirrored binding that reuses the same realized vertex buffer.

So the locked seam narrows cleanly to this statement:
- descriptor-set divergence is already backend-realized by `uniform_set_create`
- first vertex/index buffer realization does **not** introduce new structural divergence; the first buffer recipes converge there
- the next honest seam, if continued later, would stay inside the descriptor-set payload family rather than widening back into request hashes, specialization caches, or CPU-side vertex-format caches

## 2026-05-25 — Descriptor-set realization contents / backing-resource composition follow-up (`submit_serial=9`)

### Artifact root

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-024754/`
- slot-level comparison: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-024754/resource_realization_binding_details.txt`
- parsed slot-level payload: `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-024754/resource_realization_binding_details.json`

### Outer crash identity

The same three approved cases were rerun again and still preserved the exact same outer crash identity:
- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status still lands at `-6`

### Slot-level descriptor-set realization findings

New follow-up payload: `binding_realizations=` inside the existing `l88_resource_realization_boundary=` marker.

This follow-up answers the narrower question left by the prior hash-only pass: **which concrete bound slots first distinguish the already-divergent sets, and whether that distinction is recipe/state-level or only object-identity-level.**

Control set `2` stays converged in recipe/state terms:
- set `2` still has only binding `0` (`StorageBuffer`)
- across all three cases, its stable backing fields remain the same: `requested_size=16`, `allocation_size=16`, `usage_mask=0x23`, `dynamic=false`, `frame_slot=4294967295`, `realization_recipe_hash=0x922906c0`
- the only differences are fresh object-identity fields (`driver_id`, `buffer_handle`), confirming that new runs naturally allocate fresh backend objects even when the realized buffer recipe is unchanged

Divergent sets `0`, `1`, and `3` split the same way: **their first distinguishing fields are raw bound-object identity fields, not backing recipe/state fields.**

Set `0` (`20` bindings):
- first differing bound slot is binding `1` (`UniformBuffer`)
- binding `1` keeps the same stable buffer realization fields across all three cases (`requested_size=320`, `allocation_size=320`, `usage_mask=0x12`, `dynamic=false`, `frame_slot=4294967295`, `realization_recipe_hash=0x737d4ff4`)
- binding `2` (`StorageBuffer`) behaves the same way, with stable recipe `realization_recipe_hash=0x488b2968`
- bindings `3`, `4`, `6`, and `7` (`Texture`) keep the same texture recipe fields (`rd_format`, `usage`, `view_type`, `layout`, `recipe_hash`) and differ only in `driver_id` / `image_handle` / `view_handle`
- bindings `5` and `10..21` (`Sampler`) keep the same sampler `create_ordinal` / `state_hash` and differ only in sampler `driver_id`
- net: set `0`'s earliest distinguishing contents start at binding `1`, but the split is object identity only; no texture recipe hash, buffer realization recipe hash, or sampler state hash changes across the three cases

Set `1` (`CombinedSampler`, binding `0` only):
- the sampler half keeps `create_ordinal=4` and `state_hash=0x251d4581` in all three cases; only sampler `driver_id` changes
- the texture half keeps `rd_format=36`, `usage=0x6`, `view_type=1`, `layout=5`, `recipe_hash=0xacb5d5d9` in all three cases; only `driver_id` / `image_handle` / `view_handle` change
- net: set `1` diverges immediately at binding `0`, but again only by concrete backend object identity, not by stable sampler/texture realization fields

Set `3` (`4` bindings):
- all four bindings diverge only in object identity
- binding `0` texture keeps `rd_format=15`, `usage=0x7`, `view_type=1`, `layout=5`, `recipe_hash=0x31a55154`
- bindings `1` and `2` textures keep `rd_format=36`, `usage=0x6`, `view_type=1`, `layout=5`, `recipe_hash=0xacb5d5d9`
- binding `3` sampler keeps `create_ordinal=4`, `state_hash=0x251d4581`
- net: set `3`'s first differing slot is binding `0`, but every changed field is still object identity only (`driver_id`, `image_handle`, `view_handle`, or sampler `driver_id`)

### Practical conclusion

This narrows the descriptor-set realization seam one more step:
- the earlier set-level divergence hashes for sets `0`, `1`, and `3` do **not** correspond to any newly changed stable backing recipe/state field at `uniform_set_create`
- instead, the concrete descriptor contents first distinguish those sets only by which freshly realized backend objects were bound into the descriptor set (`driver_id`, buffer handle, image handle, view handle, sampler handle lineage)
- stable backing composition stays converged across the three approved cases: buffer realization recipe hashes, texture recipe hashes, and sampler state hashes all remain unchanged at this boundary
- set `2` remains the useful control because it proves the reruns naturally reallocate fresh backend objects even when the stable realization recipe is unchanged

## 2026-05-25 source-build follow-up — Task 168 (`oc-xmd8`)

This follow-up stayed on the same locked `projection_only__disabled` / host-Vulkan / failing `submit_serial=9` seam and did **not** widen into a fix. Instead of reopening request-hash or cache questions, it extended the existing `l88_resource_realization_boundary=` payload just enough to classify the concrete per-binding realization contents for the already-divergent first-L88 descriptor sets `0`, `1`, and `3`, while keeping set `2` as the converged control.

Artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-024754/`

Key durable artifacts inside that root:

- `resource_realization_contents_compare.txt`
- `resource_realization_contents.json`
- `resource_realization_boundary_compare.txt`
- `resource_realization_boundary_corrected.json`

Auxiliary analysis script staged for this slice:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_realization_contents.py`

Validation command:

- `python3 /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_resource_realization_boundary_qa.py`
- `python3 /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_realization_contents.py /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-024754`

### Outer crash identity stayed locked

All three approved cases were rerun again and preserved the same outer crash identity:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status still lands at `-6`

### Descriptor-set realization contents classification

The new `binding_realizations=` payload shows that the already-divergent set-level `buffer_realization_hash` split for sets `0`, `1`, and `3` is **not** caused by differing backing resource recipes. Across `baseline`, `specialization_only`, and `vertex_input_only`, the per-binding backing recipes remain converged; what changes is which resource families are still represented by raw driver-object identity fields at descriptor-set realization time.

#### Set `0`

Set `0` stays structurally identical across the three cases, and its buffer-backed bindings still keep matching realized recipes:

- binding `1` (`UniformBuffer`) keeps `realization_recipe_hash=0x737d4ff4`
- binding `2` (`StorageBuffer`) keeps `realization_recipe_hash=0x488b2968`

The **first** bound slot that actually distinguishes set `0` across the three cases is binding `3` (`Texture`):

- binding `3` keeps the same texture recipe (`recipe_hash=0x731579ab`)
- but its `driver_id`, `image_handle`, and `view_handle` differ per case

After that, the same identity-only pattern continues through the rest of the non-buffer resources in set `0`:

- binding `4` texture keeps `recipe_hash=0xdfe837f3`, but object identity fields differ
- binding `5` sampler keeps `state_hash=0xd69d4fbf`, but sampler `driver_id` differs
- binding `6` texture keeps `recipe_hash=0xacb5d5d9`, but object identity fields differ
- binding `7` texture and sampler-only bindings `10`..`21` follow the same identity-only split

Interpretation: set `0` first diverges only once the realized payload reaches the first non-buffer slot whose contribution still depends on exact object identity rather than only on buffer realization recipe.

#### Set `1`

Set `1` has only binding `0`, a `CombinedSampler`, and its backing recipe also stays converged across all three cases:

- sampler subpart keeps `state_hash=0xd69d4fbf`
- texture subpart keeps `recipe_hash=0xacb5d5d9`

What distinguishes the cases is again only object identity:

- sampler `driver_id` differs
- texture `driver_id`, `image_handle`, and `view_handle` differ

So for set `1`, the first distinguishing slot is immediately binding `0`, but the distinguishing fields are identity fields, **not** sampler-state or texture-recipe fields.

#### Set `3`

Set `3` follows the same pattern as set `1`, but spread across four bindings:

- binding `0` texture is the first distinguishing slot
- binding `0`'s texture recipe stays converged, while `driver_id`, `image_handle`, and `view_handle` differ per case
- binding `1` and binding `2` textures also keep converged recipes with identity-only splits
- binding `3` sampler keeps a converged sampler `state_hash` with identity-only `driver_id` differences

So set `3` diverges immediately at its first texture slot, but again only through object identity / handle fields rather than through differing realized texture or sampler recipes.

#### Set `2` control

Set `2` remains the converged control for this slice:

- its only binding `0` is a `StorageBuffer`
- that buffer keeps the same `realization_recipe_hash` across all three cases
- only `driver_id` / `buffer_handle` differ, which does **not** perturb the earlier set-level `buffer_realization_hash` because this path is already recipe-hashed for buffer-backed resources

### Practical conclusion

This closes the next locked seam cleanly:

- the earlier Task 167 set-level divergence for sets `0`, `1`, and `3` does **not** hide a deeper backing-recipe split at descriptor-set realization time
- instead, those sets first distinguish themselves at the earliest non-buffer slots whose realized payload still carries raw object identity / handle fields (`Texture`, `Sampler`, `CombinedSampler`)
- set `0` first distinguishes at texture binding `3`
- set `1` first distinguishes at combined-sampler binding `0`
- set `3` first distinguishes at texture binding `0`
- set `2` remains converged because its control payload is only a storage buffer whose contribution is already reduced to a stable realization recipe hash

So the descriptor-set realization family is now narrowed one step further: the cross-case split is identity-backed for non-buffer resources, not recipe-backed for the underlying texture/sampler/buffer realizations on this locked failing lane.

## 2026-05-25 source-build follow-up — Task 169 (`oc-67se`)

This follow-up stayed on the exact same locked failing lane (`projection_only__disabled`, host Vulkan, `submit_serial=9`) and deliberately did **not** widen into a fix. Rather than reopening request-hash or cache questions, it traced one rung farther behind the already-divergent first-L88 descriptor sets `0`, `1`, and `3` to decide whether their backing-resource provenance actually diverges before descriptor-set realization, while keeping set `2` as the converged control.

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-133804/`

Key durable artifacts inside that root:

- `resource_backing_provenance.txt`
- `resource_backing_provenance.json`
- `resource_realization_binding_details.txt`
- `resource_realization_binding_details.json`
- `outer_crash_identity.json`
- `resource_realization_boundary_compare.txt`

Analyzer / rerun entrypoints used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_resource_realization_boundary_qa.py`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_backing_provenance.py`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_realization_binding_details.py`

### Outer crash identity stayed locked

All three approved reruns preserved the same crash envelope:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status stayed `-6`

### Backing-provenance classification

The new backing-provenance comparison shows the descriptor-set split does **not** deepen into a pre-realization recipe/signature divergence for the target sets.

Across both comparisons (`baseline` -> `specialization_only`, `baseline` -> `vertex_input_only`):

- set `0` classifies as `raw_backend_identity_diverged_while_stable_provenance_converged`
- set `1` classifies as `raw_backend_identity_diverged_while_stable_provenance_converged`
- set `3` classifies as `raw_backend_identity_diverged_while_stable_provenance_converged`
- control set `2` also classifies as `raw_backend_identity_diverged_while_stable_provenance_converged`

The important stable hashes do **not** change for any of these sets:

- `stable_resource_provenance_hash_changed=false`
- `binding_signature_hash_changed=false`

Only the raw descriptor-object identity changes per rerun:

- `resource_object_hash_changed=true`
- first diffs remain identity fields like buffer/image/view handles or `driver_id`
- there are no semantic binding diffs (`first_semantic_binding_diff=null` in every compared set)

### Immediate provenance behind the already-divergent sets

The stable backing provenance still converges immediately behind the descriptor bindings that diverge at first L88:

- **set `0`** still first differs at binding `1` (`UniformBuffer`), then binding `2` and later non-buffer slots, but the compared fields remain identity-only (`buffer_handle`, `driver_id`, later texture/sampler handles). The stable buffer realization recipe stays converged (`0x737d4ff4` for binding `1`, `0x488b2968` for binding `2`).
- **set `1`** still first differs at binding `0` (`CombinedSampler`), but the sampler retains `create_ordinal=4` and `state_hash=0x251d4581`, while the texture retains `recipe_hash=0xacb5d5d9`; only sampler/texture identity fields split.
- **set `3`** still first differs at binding `0` (`Texture`), with the same texture recipe (`0x31a55154`) preserved across the three cases; the later texture bindings keep `0xacb5d5d9` and the sampler keeps `state_hash=0x251d4581`, again with only identity/handle fields changing.
- **set `2` control** remains the proof point that fresh reruns naturally reallocate backend objects even when the stable backing provenance is unchanged: its storage buffer keeps `realization_recipe_hash=0x922906c0`, while only `buffer_handle` / `driver_id` vary.

### Practical conclusion

This closes the Task 169 question cleanly:

- the already-divergent first-L88 descriptor sets `0`, `1`, and `3` do **not** first diverge through backing resource recipes, creation signatures, or stable provenance hashes before descriptor-set realization
- instead, the divergence remains a raw backend object-identity split all the way through the traced backing-resource provenance layer on this locked failing lane
- set `2` behaves the same way as the converged control, which reinforces that the reruns are simply allocating fresh backend objects while the stable provenance remains converged

So the descriptor-set realization family stays narrowed to an identity-only split for these resources at failing `submit_serial=9`; there is still no evidence here of a stable recipe/provenance fork hiding behind sets `0`, `1`, or `3`.


## 2026-05-25 source-build follow-up — Task 170 (`oc-rayk`)

This follow-up stayed on the exact same locked failing lane (`projection_only__disabled`, host Vulkan, `submit_serial=9`) and deliberately did **not** widen into a fix. Rather than reopening request-hash, specialization-cache, or CPU-side vertex-format-cache questions, it traced one rung farther forward from descriptor-set realization to the **semantic descriptor payload actually consumed by the first failing L88 packet**, while keeping set `2` as the converged control.

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-resource-realization-boundary-vulkan-sourcebuild-20260525-135340/`

Key durable artifacts inside that root:

- `resource_realization_contents_compare.txt`
- `resource_realization_contents.json`
- `resource_realization_binding_details.txt`
- `resource_realization_binding_details.json`
- `resource_backing_provenance.txt`
- `outer_crash_identity.json`

Analyzer / rerun entrypoints used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_resource_realization_boundary_qa.py`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_realization_contents.py`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_realization_binding_details.py`
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/analyze_resource_backing_provenance.py`

### Outer crash identity stayed locked

All three approved reruns preserved the same crash envelope:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status stayed `-6`

### First consumer-boundary semantic payload verdict

The first failing L88 packet still consumes the **same semantic descriptor payload** across all three approved cases.

Across both comparisons (`baseline` -> `specialization_only`, `baseline` -> `vertex_input_only`):

- set `0`: `first_recipe_diff_binding=None`, `semantic_recipe_changed=False`
- set `1`: `first_recipe_diff_binding=None`, `semantic_recipe_changed=False`
- set `3`: `first_recipe_diff_binding=None`, `semantic_recipe_changed=False`
- control set `2`: `first_recipe_diff_binding=None`, `semantic_recipe_changed=False`
- `first_semantic_binding_diff=null` for every compared set

So a true semantic split does **not** first appear at the first consumer boundary either. The remaining cross-case differences are still only raw backend identity fields (`driver_id`, `buffer_handle`, `image_handle`, `view_handle`).

### Concrete effective payload that stayed converged

The consumed payload remained content-stable at the exact fields that matter for each focus set:

- **set `0`** still carries the same buffer contract at the first distinguishing slots: binding `1` (`UniformBuffer`) stays `requested_size=320`, `allocation_size=320`, `usage_mask=0x12`, `dynamic=false`, `frame_slot=4294967295`, `realization_recipe_hash=0x737d4ff4`; binding `2` (`StorageBuffer`) keeps `realization_recipe_hash=0x488b2968`. Later texture/sampler slots also keep their same recipe/state payloads while only identity fields change.
- **set `1`** still carries the same combined-sampler payload at binding `0`: sampler `create_ordinal=4`, `state_hash=0x251d4581`; texture `recipe_hash=0xacb5d5d9`, `rd_format=36`, `usage=0x6`, `view_type=1`, `layout=5`, `has_allocation=true`, `is_subsampled=false`.
- **set `3`** still carries the same texture/sampler payload: binding `0` texture `recipe_hash=0x31a55154`, `rd_format=15`, `usage=0x7`, `view_type=1`, `layout=5`; bindings `1/2` textures `recipe_hash=0xacb5d5d9`; sampler slot keeps `state_hash=0x251d4581`.
- **set `2` control** remains converged with storage-buffer `realization_recipe_hash=0x922906c0`, confirming that fresh reruns naturally rotate backend identities even when the effective payload is semantically unchanged.

### Practical conclusion

This closes the Task 170 question cleanly:

- the first failing L88 packet does **not** first split semantically at the descriptor payload it actually consumes
- the effective descriptor contents / resource payload remain converged at content level across `baseline`, `specialization_only`, and `vertex_input_only`
- the surviving cross-case difference is still only backend object identity / handle churn, with set `2` behaving as the expected converged control

So the locked `submit_serial=9` seam is now narrowed one step farther: even at the first consumer boundary for the first failing L88 packet, the descriptor payload is still semantically the same across the approved contrast cases.


## 2026-05-25 source-build follow-up — Task 171 (`oc-6az1`)

This follow-up stayed on the same locked failing lane (`projection_only__disabled`, host Vulkan, `submit_serial=9`) and moved exactly one rung lower than Task 170: from semantically converged descriptor payloads to the **first L88 backend execution / synchronization / lifetime boundary that actually consumes that payload**. It did not widen into a fix and reran only the same approved three cases.

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-execution-sync-lifetime-boundary-vulkan-sourcebuild-20260525-143018/`

Key durable artifacts inside that root:

- `notes.md`
- `parsed_execution_sync_lifetime_summary.json`
- `comparison.txt`
- `execution_sync_lifetime_boundary_summary.tsv`
- per-case `stdout.log` / `stderr.log` / `marker_lines.json`

Rerun entrypoint used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_execution_sync_lifetime_boundary_qa.py`

### Outer crash identity stayed locked

All three approved reruns preserved the same crash envelope:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status stayed `-6`

### New driver-side consume-boundary trace

The Vulkan driver now records `l88_execution_sync_lifetime_boundary` beside the existing first-L88 summaries. That trace captures three things for the first L88 `draw_indexed` consumer:

- the exact pre-draw command-state snapshot actually consumed by the draw
- the last pipeline barrier seen before that draw
- control lifetime / realization summaries for descriptor set `2`, the first vertex buffer, and the index buffer

### What stayed converged

The first L88 draw consumed the same synchronization scope in all three cases:

- `render_pass_create_serial=13`
- `render_pass_compatibility_hash=0x425f4d3d`
- `render_subpass_compatibility_hash=0x81df0cad`
- `render_pass_attachment_exact_hash=0x6529dc72`
- `render_pass_dependency_hash=0x208ccbee`
- `subpass=0`
- `breadcrumb=UI_PASS`
- `scope_pipeline_relation=different_handle_same_render_pass_compatibility`

The last pipeline barrier before that first L88 draw also stayed structurally the same across the three reruns:

- `serial=101`
- `src_stage_mask=0x1489`
- `dst_stage_mask=0x248c`
- `memory_barriers=1`
- `buffer_barriers=5`
- `texture_barriers=4`
- `acceleration_structure_barriers=0`
- first traced texture layout remained `old=2 -> new=2`
- serial gaps remained `uniform_to_draw=1`, `vertex_to_draw=4`, `index_to_draw=3`, `pipeline_barrier_to_draw=7`

The lifetime / realization controls also stayed converged:

- descriptor set `2` kept `buffer_realization_hash=0xccee0b09` and `stable_resource_provenance_hash=0xbed89158`
- the first vertex buffer kept `create_ordinal=385`, `requested_size=2097152`, `allocation_size=4194304`, `dynamic=true`, `frame_slot=1`, `realization_recipe_hash=0x78fdd30e`
- the index buffer kept `create_ordinal=25`, `requested_size=16`, `allocation_size=16`, `dynamic=false`, `realization_recipe_hash=0x5e1dfda9`

Across those layers, only raw driver identities / handles rotated per rerun.

### Where the first true split appears

The first meaningful split beyond semantic descriptor content therefore appears in **backend execution state**, not synchronization scope or resource lifetime / aliasing state.

- `specialization_only` keeps the same draw synchronization context and lifetime controls, but changes the bound pipeline execution recipe (`graphics_recipe_hash=0xc10455e7`, `specialization_constant_hash=0xbfcbce98` versus baseline `0xb3c95184` / `0x6273dcb1`)
- `vertex_input_only` keeps the same draw synchronization context and lifetime controls, but changes the execution packet shape (`vertex_binding_count=2`, `vertex_input_recipe_hash=0xb9920205` versus baseline `vertex_binding_count=1`, `vertex_input_recipe_hash=0xaf2a1c78`)

### Practical conclusion

This closes the Task 171 question cleanly:

- the first failing L88 draw still enters under a converged render-pass / synchronization context
- it also still consumes converged lifetime / realization recipes for the traced control resources
- so the earliest stable cross-case fork beyond semantic descriptor payloads is the **backend execution packet itself** (pipeline execution recipe / vertex-input packet shape), not a driver-visible synchronization or lifetime boundary on this locked lane

## 2026-05-25 — Task 172: first driver-consumptive execution recipe boundary after the converged pre-draw sync state

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-driver-consumptive-execution-boundary-vulkan-sourcebuild-20260525-150238/`

Key durable artifacts inside that root:

- `notes.md`
- `comparison.txt`
- `results.json`
- `driver_consumptive_execution_boundary_summary.tsv`
- per-case `stdout.log` / `stderr.log` / `marker_lines.json`

Rerun entrypoint used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_driver_consumptive_execution_boundary_qa.py`

### Outer crash identity stayed locked

All three approved reruns preserved the same crash envelope:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status stayed `-6`

### Converged pre-draw sync state really does survive to the first L88 pipeline bind

The first L88 draw still entered under the same already-proven synchronized render-pass context in all three reruns:

- `render_pass_create_serial=13`
- `render_pass_compatibility_hash=0x425f4d3d`
- `render_subpass_compatibility_hash=0x81df0cad`
- `render_pass_attachment_exact_hash=0x6529dc72`
- `render_pass_dependency_hash=0x208ccbee`
- `subpass=0`
- `breadcrumb=UI_PASS`
- `scope_pipeline_relation=different_handle_same_render_pass_compatibility`

That state remained converged right up to the first L88 pipeline-consumption seam; no new sync or lifetime split appeared before the packet started consuming its bound execution recipe.

### Exact first driver-consumptive boundaries

The first L88 `bind_render_pipeline` is the earliest driver-facing execution-recipe consumer in all three lanes:

- `pipeline_serial=103`
- same compatibility context at consume time: `render_pass_compatibility_hash=0x425f4d3d`, `render_subpass_compatibility_hash=0x81df0cad`, `render_pass_dependency_hash=0x208ccbee`, `render_subpass=0`
- `baseline` pipeline recipe: `graphics_recipe_hash=0x7f28b49f`, `vertex_input_recipe_hash=0xaf2a1c78`, `specialization_constant_hash=0x6273dcb1`
- `specialization_only` pipeline recipe: `graphics_recipe_hash=0x0a87b22e`, `vertex_input_recipe_hash=0xaf2a1c78`, `specialization_constant_hash=0xbfcbce98`
- `vertex_input_only` pipeline recipe: `graphics_recipe_hash=0xaf15fdc4`, `vertex_input_recipe_hash=0xb9920205`, `specialization_constant_hash=0x6273dcb1`

That means the **first divergent execution recipe is consumed at `bind_render_pipeline`**, not later at uniform binding, pipeline barriers, or draw submission. `specialization_only` first diverges there by swapping only the specialization-driven portion of the recipe while holding the vertex-input recipe constant. `vertex_input_only` also already diverges there because the bound pipeline provenance carries a different vertex-input recipe into the driver at the same serial.

The first driver-visible **draw-shape** split appears one command later at the first L88 `bind_vertex_buffers`:

- `serial=104` in all three lanes
- `baseline`: `binding_count=1`
- `specialization_only`: `binding_count=1`
- `vertex_input_only`: `binding_count=2`

So the packet’s bound execution recipe diverges first at `bind_render_pipeline`, while the packet’s bound vertex-stream shape diverges first at `bind_vertex_buffers`.

### What still converges later

The later indexed draw consumer still preserves the same outer draw arguments across the three reruns:

- first `draw_indexed` consumer at `serial=108`
- `index_count=6`
- `instance_count=27`
- `first_index=0`
- `vertex_offset=0`
- `first_instance=0`

### Practical conclusion

This closes the Task 172 question cleanly:

- the converged pre-draw sync / lifetime state survives up to the first L88 driver-consumptive boundary
- the earliest consumed **execution-recipe** split is the first L88 `bind_render_pipeline` at `serial=103`
- the earliest consumed **draw-shape** split is the first L88 `bind_vertex_buffers` at `serial=104`
- the final `draw_indexed` call still converges on the same user-visible draw arguments, so the fault seam remains earlier in the backend-bound recipe / stream shape than in the final draw-call argument tuple

## 2026-05-25 — Task 174: first-L88 early-bind sufficiency matrix

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-bind-sufficiency-matrix-vulkan-sourcebuild-20260525-170250/`

Key durable artifacts inside that root:

- `notes.md`
- `comparison.txt`
- `results.json`
- `bind_sufficiency_summary.tsv`
- per-case `stdout.log` / `stderr.log` / `marker_lines.json`

Rerun entrypoint used:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/run_bind_sufficiency_matrix_qa.py`

### Outer crash identity stayed locked

All three approved reruns preserved the same crash envelope:

- `baseline`
- `specialization_only`
- `vertex_input_only`
- `fence_wait_begin submit_serial=9`
- `fence_wait_error submit_serial=9`
- tail still contains `Tonemap (L87) (Draw)` then `Command Graph (L88) (Draw)`
- later breadcrumbs still reach `BLIT_PASS`
- exit status stayed `-6`

### Narrow sufficiency proof for `bind_render_pipeline`

The narrowest approved comparison is still `baseline` vs `specialization_only`.

- both runs keep the first vertex bind unchanged at `serial=104`
- both runs keep `binding_count=1` at that vertex-bind consumer
- both runs keep the same first-L88 vertex-input recipe hash `0xaf2a1c78`
- only the pipeline-owned recipe changes at `serial=103`
  - `baseline`: `graphics_recipe_hash=0x92ea9d42`, `specialization_constant_hash=0x6273dcb1`
  - `specialization_only`: `graphics_recipe_hash=0xf16ef2b6`, `specialization_constant_hash=0xbfcbce98`

Because that single-fork comparison still preserves the exact locked crash envelope, `bind_render_pipeline` at `serial=103` is by itself sufficient to preserve the failing `submit_serial=9` lane.

### What this says about the `bind_vertex_buffers` fork

The third approved case still shows the combined packet split:

- `vertex_input_only`: `graphics_recipe_hash=0x69a72955`, `vertex_input_recipe_hash=0xb9920205`, `specialization_constant_hash=0x6273dcb1`
- the first vertex bind still lands at `serial=104`, but now with `binding_count=2`

That confirms the combined `serial=103` + `serial=104` divergent packet also preserves the same crash envelope. But the approved three-case matrix does **not** independently isolate a pure `bind_vertex_buffers`-only fork while holding the `serial=103` pipeline recipe at baseline, so this slice intentionally does not overclaim that `bind_vertex_buffers` alone is sufficient.

### Practical conclusion

This closes Task 174 cleanly without widening the seam:

- `bind_render_pipeline` alone is sufficient to preserve the locked L88 crash envelope
- the failure therefore does **not** require the combination of the `serial=103` pipeline fork plus the `serial=104` vertex-buffer fork
- `bind_vertex_buffers`-alone sufficiency remains unproven by this narrow three-case matrix, which is exactly the boundary this slice was asked to respect

## 2026-05-25 — Task 175: isolate the `bind_render_pipeline` recipe component that still carries the locked `submit_serial=9` L88 crash envelope

Artifact root reused from Task 174:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-bind-sufficiency-matrix-vulkan-sourcebuild-20260525-170250/`

Evidence consulted inside that root:

- `comparison.txt`
- `results.json`
- per-case `marker_lines.json`
- per-case `stdout.log` first-L88 `serial=103` pipeline-provenance payloads

No new engine instrumentation was added for this slice. The analysis stayed inside the already-approved three-case matrix and only compared the first-L88 `bind_render_pipeline` recipe subfields that were already captured in Task 174.

### Component contrast at `serial=103`

Across the three approved reruns, the first-L88 `bind_render_pipeline` packet kept the same outer crash envelope (`submit_serial=9`, `fence_wait_error submit_serial=9`, later `BLIT_PASS`, exit `-6`) while exposing this component split:

- `baseline`
  - `graphics_recipe_hash=0x92ea9d42`
  - `vertex_input_recipe_hash=0xaf2a1c78`
  - `blend_recipe_hash=0xd22fca4d`
  - `specialization_constant_hash=0x6273dcb1`
  - `descriptor_set_layout_hash=0x258b1e0e`
  - `push_constant_hash=0x9b5fef81`, `push_constant_stage_mask=0x11`, `push_constant_total_size=32`
- `specialization_only`
  - `graphics_recipe_hash=0xf16ef2b6`
  - `vertex_input_recipe_hash=0xaf2a1c78`
  - `blend_recipe_hash=0xd22fca4d`
  - `specialization_constant_hash=0xbfcbce98`
  - `descriptor_set_layout_hash=0x173e1535`
  - `push_constant_hash=0x9b5fef81`, `push_constant_stage_mask=0x11`, `push_constant_total_size=32`
- `vertex_input_only`
  - `graphics_recipe_hash=0x69a72955`
  - `vertex_input_recipe_hash=0xb9920205`
  - `blend_recipe_hash=0xd22fca4d`
  - `specialization_constant_hash=0x6273dcb1`
  - `descriptor_set_layout_hash=0xa83914bf`
  - `push_constant_hash=0x9b5fef81`, `push_constant_stage_mask=0x11`, `push_constant_total_size=32`

### Narrowest locked component classification

This leaves a tighter component story than Task 174:

- `blend_recipe` is ruled out for this seam because it stayed identical across all three approved reruns while the crash envelope stayed locked.
- `vertex_input_recipe` is not the minimal `serial=103` carrier because the narrow sufficiency contrast from Task 174 (`baseline` vs `specialization_only`) preserved the crash while holding `vertex_input_recipe_hash=0xaf2a1c78` fixed and also holding the first `serial=104` vertex-bind payload at `binding_count=1`.
- The captured `pipeline_layout` payload did not provide a stable discriminant in this methodology: its `descriptor_set_layout_hash` drifted across all three fresh-launch cases, while the stable push-constant contract (`push_constant_hash=0x9b5fef81`, stage mask `0x11`, total size `32`) stayed fixed across all three. That means this slice cannot honestly promote `pipeline_layout` to the locked carrier without widening into a new experiment.
- The only recipe component that cleanly matches the minimal preserved `serial=103` contrast is the `specialization_constants` bucket: `baseline` and `specialization_only` keep the same first-L88 vertex-input and blend sub-recipes, keep the same outer crash identity, yet split exactly on `specialization_constant_hash` (`0x6273dcb1` vs `0xbfcbce98`).

### Practical conclusion

Within the approved Task 174/175 methodology, the locked L88 crash envelope is currently carried by the `bind_render_pipeline` **specialization-constant sub-recipe** at `serial=103`, not by the blend sub-recipe, and not by any independently isolated `serial=104` vertex-buffer fork. This is a recipe-component classification only; it does **not** reopen specialization-cache theory or widen into a fix.

## 2026-05-25 — Task 176: isolate the exact first-L88 specialization-constant change that still carries the locked `submit_serial=9` crash envelope

Artifact root reused again from Tasks 174-175:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-bind-sufficiency-matrix-vulkan-sourcebuild-20260525-170250/`

Evidence consulted inside that root:

- `notes.md`
- `comparison.txt`
- per-case `stdout.log` first-L88 `serial=103` pipeline-provenance payloads
- per-case `stdout.log` neighboring-pass `specialization_delta` payloads

No new engine instrumentation was added for this slice. I stayed inside the already-approved three-case matrix and only tightened the specialization-constant comparison that Task 175 had already isolated.

### Exact specialization-constant contrast at `serial=103`

The narrowest preserved contrast remains `baseline` vs `specialization_only` because both reruns keep the same outer crash identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87) (Draw)` -> `Command Graph (L88) (Draw)` -> later `BLIT_PASS`, exit `-6`) while also keeping the same first-L88 vertex-input side fixed:

- `vertex_input_recipe_hash=0xaf2a1c78`
- first `serial=104` vertex bind `binding_count=1`
- same first-L88 singleton specialization shape: `count=1`, `type_mask=0x2`, `bool_count=0`, `int_count=1`, `float_count=0`, `id_hash=0xc5247e53`, `min_id=0`, `max_id=0`

What changes is only the payload of that singleton specialization entry:

- `baseline`
  - `specialization_constant_hash=0x6273dcb1`
  - `specialization_constant_value_hash=0xc5247e53`
  - preview `[{id=0,type="int",bits="0x0",value=0}]`
- `specialization_only`
  - `specialization_constant_hash=0xbfcbce98`
  - `specialization_constant_value_hash=0xa18293e9`
  - preview `[{id=0,type="int",bits="0x2",value=2}]`

The control case confirms that this is the exact specialization fork and not a hidden ID/count reshuffle:

- `vertex_input_only` keeps the same specialization payload as `baseline`
  - `specialization_constant_hash=0x6273dcb1`
  - `specialization_constant_value_hash=0xc5247e53`
  - preview `[{id=0,type="int",bits="0x0",value=0}]`
- while changing only the first-L88 vertex-input recipe (`vertex_input_recipe_hash=0xb9920205`)

### Narrowest exact carrier

That makes the Task 176 seam stricter than Task 175:

- it is **not** a change in specialization-constant count
- it is **not** a change in the specialization constant ID set
- it is **not** a change in specialization type mix (`int_count` stays `1`)
- it is **not** a broader shape change across multiple specialization entries
- it **is** the singleton `id=0` integer specialization payload changing from `0` to `2`

### Practical conclusion

Within the approved methodology, the exact first-L88 specialization-constant change that carries the locked crash envelope at failing `submit_serial=9` is the singleton `id=0` integer specialization value flipping from `0` to `2` while the rest of the specialization contract stays fixed. This remains a seam-classification result only; it does **not** reopen specialization-cache theory, request-hash theory, or CPU-side vertex-format-cache theory, and it does not widen into a fix.

## 2026-05-25 — Task 178: explain what the exact first-L88 specialization value flip `id=0`, `int`, `0 -> 2` changes in shader/pipeline terms and why it still preserves the locked crash envelope

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-specialization-meaning-2026-05-25.md`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-fragment-path-2026-05-25.md`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-derivative-coverage-math-2026-05-26.md`

The narrow source-backed conclusion is:

- first-L88 `constant_id=0` is canvas `sc_packed_0`
- bit layout is `use_lighting`=`bit0`, `use_msdf`=`bit1`, `use_lcd`=`bit2`
- the exact `0x0 -> 0x2` flip turns on only `sc_use_msdf()` in the specialized non-ubershader pipeline
- shader-side, that switches the fragment path from ordinary texture modulation to the MSDF coverage/outline branch in `servers/rendering/renderer_rd/shaders/canvas.glsl`
- pipeline-side, it creates/binds a different specialized render pipeline while leaving the already-audited surrounding packet shape fixed

Why the crash envelope stays locked:

- the Task 177 value-only run already proved this exact specialization payload flip alone preserves the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)` then `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- the value-only contrast held the rest of the approved comparison surface fixed enough to show the surviving fork is the specialized pipeline recipe itself, not a widened descriptor/sync/draw-argument change
- `_get_pipeline_specialization_or_ubershader()` zeroes the push-constant specialization shadow for the specialized path, so the forced `0x2` value is actually consumed as the bound pipeline specialization and not overridden by the batch's original `use_msdf=false` selector

## 2026-05-26 — Task 180: exact first-L88 MSDF derivative / coverage / outline math

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-derivative-coverage-math-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- the live specialized branch is still in `servers/rendering/renderer_rd/shaders/canvas.glsl` under `if (sc_use_msdf())`
- the exact packet inputs already established by approved artifacts are `px_range=1.0`, `outline=0.0`, and `texpixel_size=(1/256, 1/256)` with `held_use_msdf=false` in the value-only forcing run
- because `outline_thickness == 0.0`, the outline-capable sub-branch is **dormant** on this packet; the preserved-crash seam does not require outline-specific behavior
- the exact live math therefore reduces to:
  - same `texture(color_texture, uv)` sample as baseline
  - `dest_size = 1.0 / fwidth(uv)`
  - `px_size = max(0.5 * dot((vec2(1.0) / msdf_size), dest_size), 1.0)` with this packet's effective `msdf_size=256x256`
  - `d = median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`
  - `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`
  - `color.a = a * color.a`
- compared with the non-MSDF `color *= texture(...)` path, the semantic fork is local fragment ALU only: derivative-scaled coverage reconstruction plus alpha rewrite, not a new descriptor / draw / bind shell

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only contrast already proved the exact `packed_0 0x0 -> 0x2` branch swap preserves the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- the exact first-kept batch/prereq artifacts still keep the same surrounding first-L88 shell: same prereq scissor/uniform-set/draw-binding package, same indexed draw family, same packet lane
- so the most specific preserved-crash classification is that the live first-L88 specialized seam is the **no-outline MSDF derivative/coverage alpha path inside the same already-locked packet shell**, not a widened structural fork elsewhere

## 2026-05-26 — Task 181: exact first-L88 no-outline MSDF `px_size` versus median-distance split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-px-size-vs-median-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- the already-approved packet facts keep the same exact no-outline MSDF lane: `px_range=1.0`, `outline=0.0`, `texpixel_size=(1/256, 1/256)`, source rect `14x16`, destination rect `14x16`
- with that exact geometry, the nominal UV advance is one texel per destination pixel, so `fwidth(uv) ≈ (1/256, 1/256)` and `dest_size ≈ (256, 256)`
- substituting those packet facts into the live branch reduces the derivative-driven term to a neutral floor value:
  - `px_size = max(0.5 * dot((1/256, 1/256), (256, 256)), 1.0) = 1.0`
- once `px_size` collapses to `1.0`, the no-outline alpha formula simplifies from `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)` to `a = clamp(d, 0.0, 1.0)` and therefore, for normalized sampled channels, effectively to `a = d`
- that demotes the derivative-driven `px_size` term on this exact packet and leaves the median-distance reconstruction `d = median(r, g, b)` as the surviving live MSDF-specific semantic fork inside the already-locked packet shell

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only contrast already proved the exact `packed_0 0x0 -> 0x2` branch swap preserves the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior by proving `outline=0.0` on this packet
- this Task 181 split now demotes the derivative-driven scale term as well, because the exact packet geometry reduces `px_size` to `1.0`
- so the narrowest surviving preserved-crash classification is that the exact first-L88 no-outline MSDF seam tracks the **median-distance reconstruction / alpha rewrite**, not the derivative gain term

## 2026-05-26 — Task 182: exact first-L88 median-distance versus final alpha rewrite split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-median-vs-alpha-rewrite-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 181 already reduced the packet to the no-outline live form `a = d` and therefore effectively `color.a = d * color.a`
- in `servers/rendering/renderer_rd/shaders/canvas.glsl`, the surviving branch-local reinterpretation step is `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`
- the final application shell `color.a = a * color.a` uses inherited fragment alpha from the already-shared upstream path (`vec4 color = color_interp;` with `color_interp = color;` emitted from the vertex side), so the trailing `* color.a` is not the specialization-only semantic fork
- with `px_size` already demoted to `1.0`, the remaining branch-only factor inside the exact packet is therefore the median-distance value `d`, while the alpha rewrite is the generic carrier that applies that branch-local result into the shared fragment color state

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only contrast already proved the exact `packed_0 0x0 -> 0x2` branch swap preserves the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted the outline sub-branch on this packet because `outline=0.0`
- Task 181 already demoted the derivative gain term on this packet because the exact `14x16 -> 14x16` geometry and `1/256` texel size reduce `px_size` to `1.0`
- so this Task 182 split leaves the **median-distance reconstruction** as the tightest surviving preserved carrier inside the same already-locked first-L88 packet shell, not the generic final alpha-application shell

## 2026-05-26 — Task 183: exact first-L88 raw sampled RGB relationship versus `median(r, g, b)` collapse split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-rgb-vs-median-collapse-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 182 already reduced the live packet to the effective form `msdf_sample = texture(...)`, `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`, `color.a = d * color.a`
- the raw sampled RGB triplet is a **shared sampled input surface**, not the tightest branch-only seam, because the baseline non-MSDF path also samples the same texture at the same `uv` via `color *= texture(...)`
- the tighter specialization-only transform is the `msdf_median(...)` collapse itself: it is the exact operation that turns the shared RGB triplet into the surviving scalar carrier `d` used by the already-reduced no-outline packet
- put differently: the raw sampled RGB relationship is still required input, but the branch-local preserved divergence does not tighten onto “RGB exists”; it tightens onto the **collapse of that shared RGB payload into the median-distance scalar**

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only contrast already proved the exact `packed_0 0x0 -> 0x2` branch swap preserves the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted the outline sub-branch on this packet because `outline=0.0`
- Task 181 already demoted the derivative gain term on this packet because the exact `14x16 -> 14x16` geometry and `1/256` texel size reduce `px_size` to `1.0`
- Task 182 already demoted the generic final alpha-application shell because `color.a` is inherited shared fragment state
- so this Task 183 split leaves the **`median(r, g, b)` collapse** as the tightest surviving preserved carrier inside the same already-locked first-L88 packet shell, not the broader raw sampled RGB relationship alone

## 2026-05-26 — coder note for bead `oc-eymk` (first-L88 no-outline MSDF `d` versus final alpha rewrite)

Stayed strictly on the exact first-L88 no-outline MSDF packet and did a documentation-only split of the remaining live seam instead of widening into new runtime work.

Durable note added:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-d-vs-alpha-rewrite-2026-05-26.md`

What this slice locked:

- Task 181 had already reduced the exact packet to `px_size = 1.0`, so the no-outline formula on this packet simplifies from `a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)` to `a = d`.
- That means the exact surviving branch is `color.a = d * color.a`.
- The tighter preserved MSDF-specific carrier is therefore `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`, not the downstream alpha rewrite.
- Source reason: `d` is the first irreversible MSDF-owned collapse from the sampled RGB relationship to one scalar, while `color.a = d * color.a` only applies that scalar through the already-existing inherited `color.a` carrier (`vec4 color = color_interp;`).

Why this is the tighter seam:

- upstream raw sampled RGB is broader than the resolved scalar actually used by the exact no-outline packet
- downstream `color.a = d * color.a` is broader than `d` because it mixes the MSDF-owned scalar with preexisting fragment alpha
- so the surviving live seam stays pinned on the median-distance scalar itself

This preserves the same already-locked outer envelope only as a classification statement:

- exact packet: first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- same outer identity: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`
- no widening into a fix and no reopening of specialization-cache, request-hash, or CPU-side vertex-format-cache theories

## 2026-05-26 — Task 184: exact first-L88 `msdf_median(...)` ordering relationship versus selected middle-value split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-median-ordering-vs-selected-middle-value-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 183 already demoted the broader raw sampled RGB relationship in favor of the branch-only `median(r, g, b)` collapse itself
- the remaining honest split is therefore internal to `msdf_median(...)`: broader three-channel ordering relationship versus the exact selected middle-value scalar output returned as `d`
- reading the exact helper source one rung farther shows that the ordering relationship is only the broader internal decision structure used to choose the median; it does not survive as an exported packet value
- what actually leaves the helper and is forwarded by the already-reduced no-outline packet is only the selected scalar `d`, so the tighter preserved seam inside the helper is the **selected middle-value scalar output**, not the broader ordering relationship

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the median collapse itself
- so this Task 184 slice only tightens that same packet-local classification one rung farther: inside `msdf_median(...)`, the surviving seam is the exact selected middle-value scalar output, not the broader internal ordering relationship

## 2026-05-26 — Task 183: exact first-L88 raw sampled RGB relationship versus median-collapse split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-raw-rgb-vs-median-collapse-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- prior packet-local reductions already fixed this branch to the no-outline live form `a = d` and therefore `color.a = d * color.a`
- the remaining honest split is therefore upstream raw sampled RGB relationship (`msdf_sample.r/g/b`) versus the branch-local scalar collapse `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`
- reading the exact shader source one rung farther shows that `msdf_median(...)` is the first irreversible MSDF-only collapse on this packet: it resolves the three sampled channels to one order-statistic scalar and discards channel identity once the middle value is chosen
- that makes the raw sampled RGB relationship the broader upstream input bundle, while `d` is the tighter preserved carrier actually forwarded by the live packet

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`, so this Task 183 slice only tightens that same packet-local classification one rung farther: the surviving seam is the **median collapse**, not the full upstream raw sampled RGB relationship

## 2026-05-26 — Task 184: exact first-L88 `msdf_median(...)` ordering relationship versus selected middle-value split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-median-ordering-vs-selected-middle-value-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 183 already reduced the live packet to `msdf_sample = texture(...)`, `d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b)`, `color.a = d * color.a`
- inside `msdf_median(...)`, the broader three-channel ordering relationship is only the internal comparison structure used to determine which sampled channel value is the middle order statistic
- what actually leaves the helper and survives forward into the already-reduced packet is only the exact selected middle-value scalar `d`
- that makes the ordering relationship a broader internal selection mechanism, while the selected scalar output is the tighter preserved carrier actually forwarded by the live packet

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the branch-local median collapse, so this Task 184 slice only tightens the same packet-local classification one rung farther: the surviving seam inside `msdf_median(...)` is the **selected scalar output**, not the broader ordering relationship

## 2026-05-26 — Task 185: exact first-L88 selected median source-channel identity versus emitted scalar split

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-selected-channel-identity-vs-emitted-scalar-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 184 already demoted the broader three-channel ordering relationship in favor of the exact selected middle-value scalar emitted by `msdf_median(...)`
- reading the same helper one rung farther shows that the exact contributing source-channel identity (`r`, `g`, or `b`) remains only internal provenance behind the selected value; the helper exports no channel label or sideband identity bit
- what actually leaves `msdf_median(...)` and survives into the already-reduced packet is only the emitted scalar `d`
- that makes the source-channel identity the broader internal provenance fact, while the emitted scalar value is the tighter preserved carrier actually forwarded by the live packet

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the branch-local median collapse
- Task 184 already demoted the broader three-channel ordering relationship in favor of the exact selected scalar, so this Task 185 slice only tightens the same packet-local classification one rung farther: the surviving seam is the **emitted scalar value `d`**, not the contributing source-channel identity

## 2026-05-26 — Task 186: exact first-L88 emitted scalar `d` irreducibility check

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-d-irreducible-carrier-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 185 already demoted the exact contributing source-channel identity and reduced the surviving packet-local handoff to `float d = msdf_median(...); color.a = d * color.a;`
- re-reading the exact no-outline packet one rung farther shows there is no smaller packet-local sideband attached to `d`: outline is already dormant, derivative gain is already neutral, `msdf_median(...)` exports no ordering witness or channel label, and the packet does not collapse `d` into a later boolean/enum before rejoining shared flow
- the downstream multiply result is not a tighter MSDF-only carrier because inherited `color.a` is shared pre-branch fragment state; Task 182 already demoted that shell in favor of `d`
- so the emitted scalar `d` is now the **irreducible preserved packet-local carrier** on this exact branch; any further honest narrowing would require widening beyond the current exact packet rather than continuing the same packet-local reduction ladder

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 180 already demoted outline behavior on this packet because `outline = 0.0`
- Task 181 already demoted derivative scaling on this packet because `px_size = 1.0`
- Task 182 already demoted the downstream alpha shell in favor of `d`
- Task 183 already demoted the broader raw sampled RGB relationship in favor of the branch-local median collapse
- Task 184 already demoted the broader three-channel ordering relationship in favor of the exact selected scalar
- Task 185 already demoted the contributing source-channel identity in favor of emitted scalar `d`, so this Task 186 slice only closes the packet-local reduction ladder: there is no smaller honest carrier left on this exact packet

## 2026-05-26 — Task 187: first downstream use/consequence of exact first-L88 emitted scalar `d` beyond packet-local classification

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-msdf-d-downstream-blend-seam-2026-05-26.md`

Narrow source-backed downstream classification for the exact first-L88 packet:

- Task 186 exhausted the packet-local ladder at irreducible emitted scalar `d`, so the next honest trace has to step outward from the exact no-outline packet rather than trying to split `d` further inside the same branch
- source re-read of `servers/rendering/renderer_rd/shaders/canvas.glsl` shows the first downstream carry beyond that packet-local handoff is `color.a = d * color.a` -> shared fragment `color` -> `vec4 base_color = color` -> optional shared `canvas_modulation` multiply -> exported `frag_color = color`
- that means the first downstream use/consequence of `d` beyond packet-local classification is: `d` survives only as shared/exported fragment alpha (`color.a` then `frag_color.a`), not as a new smaller MSDF-local sideband
- the earliest point where the preserved crash path becomes **structurally different outside the packet** is the live `L88` preserve-content blend boundary, not the earlier shared-flow carries, because this is the first step where a new external participant enters: prior root attachment contents loaded under `UI_PASS`
- the same durable `REF-07` evidence already matches that source read: `owner_label="Command Graph (L88) (Draw)" ... attachment_load_ops=[0:LOAD]`, `batch_summary={...,first_blend_mode="mix",first_destination_color_blend_mode="mix"}`, and `CanvasShaderRD:0` with `blend_enabled_attachment_mask="0x1"`
- the corresponding source blend contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` confirms why this is the first structural downstream seam: `BLEND_MODE_MIX` uses `src_color_blend_factor = SRC_ALPHA` and `dst_color_blend_factor = ONE_MINUS_SRC_ALPHA`, so the exported alpha carrying `d` immediately becomes part of the source-alpha weighting that merges the exact packet with already-present root contents

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Tasks 180–186 already exhausted the packet-local MSDF ladder down to emitted scalar `d`
- this Task 187 slice only widens one rung farther into the next source-backed downstream seam: exported alpha feeding the preserve-content `L88` mix-blend boundary against loaded root contents

## 2026-05-26 — Task 188: classify the narrowest structural consequence at the exact `L88` preserve-content blend boundary

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-preserve-blend-boundary-consequence-2026-05-26.md`

Narrow source-backed downstream classification at the exact attachment-level merge:

- Task 187 already located the first widened downstream seam at the live `Command Graph (L88)` preserve-content blend boundary, where exported packet alpha descended from `d` meets already-loaded root contents under `UI_PASS`
- re-reading the exact `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` shows the merge equations implied by the live attachment factors: color uses `src.rgb * src.a + dst.rgb * (1 - src.a)`, while alpha uses `src.a * 1 + dst.a * (1 - src.a)`
- the preserve/load dependency (`attachment_load_ops=[0:LOAD]` in the durable `REF-07` lane) is therefore the exact external prerequisite/context for the merge, because it keeps a live destination participant available at all — but it is broader than the tightest downstream carrier inside the merge itself
- alpha writeback is a real downstream consequence because the same exported source alpha also participates in the stored post-merge alpha channel, but that is still one step broader/later than the first active structural role played by the packet-owned carry at the boundary
- the tightest consequence is the **source-alpha color weighting** itself: the exported source alpha descended from `d` is the first active coefficient that simultaneously determines how much new packet color enters and how much preserved destination/root color remains at the exact preserve-content `BLEND_MODE_MIX` merge
- so the best narrow seam wording after Task 187 is: the live boundary is the first-L88 preserve-content `BLEND_MODE_MIX` **source-alpha color merge over loaded root contents**, with alpha writeback and preserve/load retained as broader consequence/context rather than the tightest carrier

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 187 already proved that the first widened external structural difference is this preserve-content blend boundary rather than an earlier shared-flow carry
- this Task 188 slice only classifies that exact merge more tightly; it does not widen into a fix, reopen cache theories, or claim that blend alone is root cause

## 2026-05-26 — Task 189: determine whether preserve/load remains independently live at the exact preserved `L88` merge

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-preserve-blend-merge-condition-2026-05-26.md`

Narrow source-backed merge-condition classification:

- Task 188 already ranked the exact merge roles honestly: source-alpha color weighting is the tightest carrier *inside* the merge, alpha writeback is a broader/later downstream effect, and preserve/load is broader prerequisite/context
- this Task 189 slice asks a different but adjacent question: whether the **preserved-path framing itself** can reduce to source-alpha weighting alone, or whether the attachment `LOAD` / preserve contract must come back as an independently live condition at the exact merge
- re-reading the same live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` keeps the merge equations fixed: color uses `src.rgb * src.a + dst.rgb * (1 - src.a)`, and alpha uses `src.a * 1 + dst.a * (1 - src.a)`
- under that contract, source-alpha weighting alone explains the packet-owned carry's active coefficient role once a destination side exists, but it does **not** by itself preserve the stronger fact that this exact path is a **preserve-content** merge against already-loaded root contents
- so for the exact preserved `L88` crash path, preserve/load dependency must return as an **independently live condition**: it is not the tightest carrier *inside* the merge, but it is still required to keep the destination participant identified as prior root contents rather than as an unspecified generic `dst` side
- alpha writeback still does not become the better framing, because it remains a later stored consequence of the merge rather than the sharper condition distinguishing the preserved-path family

Said plainly: the best two-level read is now `source-alpha color weighting` as the tightest active merge carrier, plus `preserve/load dependency` as the independently live condition that keeps this the exact preserved-content merge family.

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 188 already fixed the tighter inside-the-merge carrier as source-alpha weighting
- this Task 189 slice only restores preserve/load to its narrower honest role as an independently live preserved-path condition; it still does not widen into a fix, reopen cache theories, or claim that either side alone is proven root cause

## 2026-05-26 — Task 190: determine whether the returned preserve/load condition is only destination retention or carries a narrower `LOAD` consequence

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-preserve-load-consequence-2026-05-26.md`

Narrow source-backed preserve/load consequence classification:

- Task 189 already restored attachment `LOAD` as an independently live co-condition for the exact preserved `L88` merge, but only because it keeps the stronger preserved-path framing alive alongside the tighter source-alpha merge carrier
- re-reading the same live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` shows that the returned `LOAD` state contributes only one merge-visible fact: the destination side consumed as `dst.rgb` / `dst.a` is retained prior root content rather than a discarded/cleared/undefined destination participant
- no smaller source-backed `LOAD`-specific sideband appears at this exact merge: `LOAD` does not introduce a new coefficient, packet-local selector, alpha-only micro-carrier, or narrower attachment witness beyond preserving the destination participant itself
- so the returned preserve/load condition is required **purely as destination-retention prerequisite**, not as a narrower independent attachment-level `LOAD` consequence that would replace that wording
- source-alpha weighting remains the tighter active carrier *inside* the merge, and alpha writeback remains a broader/later downstream effect; neither changes the exact role played by `LOAD` here

Said plainly: after Task 190, the best wording is that the preserved `L88` crash path still needs source-alpha-driven `BLEND_MODE_MIX` color weighting, and its returned `LOAD` condition matters only by retaining prior root contents as the destination participant at the merge.

Why the locked crash envelope stays preserved:

- the approved Task 177 value-only branch swap already preserved the same outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- Task 189 already fixed the preserve/load role as independently live for the preserved-path framing
- this Task 190 slice only narrows what that preserve/load role *is*: destination retention, not a smaller hidden `LOAD`-side carrier; it still does not widen into a fix, reopen cache theories, or claim that `LOAD` policy itself is root cause

## 2026-05-26 — Task 189: determine whether preserve/load must return as an independently live condition at the exact `L88` preserve-content merge

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-preserve-merge-co-condition-2026-05-26.md`

Narrow source-backed downstream classification at the same exact attachment-level merge:

- Task 188 remains correct about the **tightest active carrier** inside the merge: source-alpha color weighting is still the first active coefficient at the live `BLEND_MODE_MIX` boundary
- but Task 189 asks a different question: whether that active carrier alone is enough to describe the exact **preserved-content** crash path, or whether the preserve/load side must come back as a separately live condition because this is specifically a merge over already-present root contents
- re-reading the exact live contract keeps both required pieces visible at once: `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` still gives `src_color_blend_factor = SRC_ALPHA` / `dst_color_blend_factor = ONE_MINUS_SRC_ALPHA`, while the durable `REF-07` lane still pins the same failing draw to `attachment_load_ops=[0:LOAD]`
- that means the exact merge is still `src.rgb * src.a + dst.rgb * (1 - src.a)`, where the source-alpha term is the tightest active packet-owned carrier **and** the destination-retention term only remains a preserved-root-content term if the destination participant is actually kept live by `LOAD`
- so source-alpha weighting alone is too narrow for the full preserved-path wording: without the `LOAD`-preserved destination side, the path would still have a source-alpha coefficient, but it would no longer be the same exact preserve-content merge against already-loaded root contents
- the honest minimum for the exact preserved `L88` path is therefore two-layered: the tightest active carrier is source-alpha-driven color weighting, and the independently live co-condition is the preserve/load dependency that keeps the destination/root participant alive at that same merge
- alpha writeback stays demoted: it is still a real downstream effect of the same merge, but it is not the returned co-condition that makes this specifically a preserve-content path
- best current wording: the preserved `L88` crash path requires the source-alpha-driven `BLEND_MODE_MIX` color merge **over loaded preserved destination/root contents**

Why the locked crash envelope stays preserved:

- this slice reuses the already-approved Task 177 outer identity (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it stays on the same exact preserve-content `L88` merge identified by Tasks 187-188
- it only tightens the wording from “tightest active carrier” to “full preserved-path minimum,” without adding runtime instrumentation, reopening cache theories, or widening into a speculative fix


## 2026-05-26 — Task 191: classify the surviving source-alpha side as packet-color admission vs preserved destination retention

Artifact / note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-alpha-admission-vs-destination-retention-2026-05-26.md`

Scope lock:

- stayed on the same preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- reused the same fixed lane context already carried in `REF-07`: `owner_label="Command Graph (L88) (Draw)"`, `attachment_load_ops=[0:LOAD]`, `first_blend_mode="mix"`, `blend_enabled_attachment_mask="0x1"`
- no new runtime instrumentation, repro widening, or speculative fix work

What was re-read:

- `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` for the live `BLEND_MODE_MIX` color equation (`SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA`)
- `servers/rendering/renderer_rd/shaders/canvas.glsl` for the no-outline MSDF carry `d -> a -> color.a -> frag_color`

Conclusion:

- after Task 190 exhausted the returned `LOAD` side as destination-retention-only context, the surviving source-alpha-driven seam is tighter at **incoming packet-color admission** than at preserved destination-retention
- the exact narrow reason is that the packet-local carrier first becomes active as `src.rgb * src.a`; the retained destination half remains real as `dst.rgb * (1 - src.a)`, but it stays the broader already-restored preserved participant/context rather than the tighter surviving side of the source-alpha seam
- preserve/load is therefore still required for the preserved-path framing, but this slice does not reopen it as the tighter side; the best wording now is: the preserved `L88` crash path still hinges on source-alpha-driven `BLEND_MODE_MIX` color weighting over loaded preserved destination/root contents, and inside that weighting seam the tightest surviving side is incoming packet-color admission


## 2026-05-26 — heartbeat continuation: split the source-alpha admission side one rung deeper

Artifact / note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-alpha-admission-export-vs-color-term-2026-05-26.md`

Scope lock:

- recovered directly from Task 191's explicit next seam, with no newly materialized pending task block present
- stayed on the same preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- no new runtime instrumentation or speculative fix work

What was re-read:

- `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` for the live `SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA` color equation
- `servers/rendering/renderer_rd/shaders/canvas.glsl` for the no-outline MSDF carry `d -> a -> color.a -> frag_color`

Conclusion:

- one rung deeper than Task 191, the admission-side seam is tightest at the **exported source-alpha carrier itself** (`frag_color.a` / `src.a`) rather than at the later composite incoming color term `src.rgb * src.a`
- the full incoming color term is still real, but it is broader because it already bundles packet RGB payload with the already-live exported alpha coefficient
- this stays inside the same preserved merge framing and does not reopen the already-demoted destination-retention side


## 2026-05-26 — heartbeat continuation: split the exported source-alpha carrier one rung deeper

Artifact / note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-alpha-export-vs-blend-consumer-alias-2026-05-26.md`

Scope lock:

- recovered directly from Task 192's explicit next seam, with no newly materialized pending task block present
- stayed on the same preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- no new runtime instrumentation or speculative fix work

What was re-read:

- `servers/rendering/renderer_rd/shaders/canvas.glsl` for the exact export `frag_color = color`
- `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` for the fixed-function consume side `SRC_ALPHA`

Conclusion:

- one rung deeper than Task 192, the tightest surviving source-backed carrier naming lands at **`frag_color.a`** rather than the later fixed-function alias `src.a`
- there is no new value transform between those names on the current approved evidence; `src.a` is simply the immediate merge-side consumer alias of the same exported carrier
- this stays inside the same preserved merge framing and does not reopen the later composite color term or the already-demoted destination-retention side

## 2026-05-26 — heartbeat continuation: split the exported shader-alpha carrier one rung deeper from packet-local/writeback facts

Artifact / note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-alpha-packet-carrier-vs-export-writeback-2026-05-26.md`

Scope lock:

- recovered directly from Task 193's explicit next seam, with the preserved `Command Graph (L88)` merge framing kept fixed
- stayed documentation-only; no new runtime instrumentation or speculative fix work
- used only exact packet-local/writeback source facts from `servers/rendering/renderer_rd/shaders/canvas.glsl`

What was re-read:

- the exact no-outline packet-local alpha write in `canvas.glsl` (`color.a = a * color.a`)
- the terminal fragment output writeback in `canvas.glsl` (`frag_color = color`)

Conclusion:

- one rung deeper than Task 193, the tighter surviving carrier is the last packet-local/shared-fragment alpha **`color.a`**
- `frag_color.a` is only the immediate fragment-output/writeback alias created by the final whole-vector copy
- the later blend-side alias `src.a` and the broader admitted color term remain demoted on the current approved evidence

## 2026-05-26 — coder note for bead `oc-oflq` (first-L88 packet-local `color.a`: inherited alpha versus local multiplier)

Stayed on the same preserved crash path and kept this slice documentation-only.

Durable note added:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-packet-alpha-inherited-vs-local-multiplier-2026-05-26.md`

What this slice locked:

- Task 194 had already reduced the surviving export-side carrier to packet-local/shared-fragment `color.a`
- re-reading the exact no-outline lane in `servers/rendering/renderer_rd/shaders/canvas.glsl` shows that this carrier is formed from `vec4 color = color_interp;` followed by `color.a = a * color.a;`
- that splits the carrier one rung deeper into an inherited pre-MSDF alpha operand versus the packet-local multiplier `a`
- the tighter preserved seam is **`a`**, because the inherited `color.a` already exists before the MSDF rewrite while `a` is the exact branch-local ingredient applied by that rewrite
- on the exact packet facts already established earlier, `px_size = 1.0`, so this narrower side immediately reconnects to the existing packet-local reduction `a = d` rather than creating a new broader carrier

This keeps the later export/writeback alias, later blend-side alias, and broader admitted color term demoted on the current approved evidence.

## 2026-05-26 — coder note for bead `oc-fhus` (first-L88 packet-local `a = d` bridge)

Stayed on the same preserved crash path and kept this slice documentation-only.

Durable note added:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-a-equals-d-bridge-2026-05-26.md`

What this slice locked:

- Task 195 had already isolated the tighter side of the packet-local `color.a` product as the branch-local multiplier `a`
- re-reading the same exact no-outline lane in `servers/rendering/renderer_rd/shaders/canvas.glsl` keeps the live source relation fixed as `float d = msdf_median(...)`, `float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0)`, `color.a = a * color.a`
- on the already-established exact packet facts, `px_size = 1.0`, so the no-outline multiplier relation collapses directly back to `a = d`
- that means the narrowest honest bridge from Task 195's local multiplier seam back into the already-established packet-local ladder is the exact alias **`a = d`**
- and on this exact packet that alias closes the local split cleanly, because no narrower packet-local distinction survives between `a` and `d` once the fixed `px_size = 1.0` fact is applied

This keeps later export/writeback aliases, blend-consumer framing, and speculative fix theories demoted on the current approved evidence. Any further continuation from here would need to decide whether to step outward from the rejoined packet-local `d` ladder into the first downstream non-packet-local consequence, or stop here as the completed local rejoin.

## 2026-05-26 — coder note for bead `oc-w9fp` (first downstream non-packet-local consequence after the exact first-L88 `a = d` bridge)

Stayed on the same preserved crash path and kept this slice documentation-only.

Durable note added:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-a-equals-d-first-downstream-non-packet-local-consequence-2026-05-26.md`

What this slice locked:

- Task 196 had already fully rejoined the packet-local ladder on the exact first-L88 no-outline MSDF packet as **`a = d`**
- re-reading `servers/rendering/renderer_rd/shaders/canvas.glsl` keeps the immediate downstream packet-owned carry fixed as `color.a = a * color.a` followed by `frag_color = color`, but those remain packet-local/shared-fragment transport plus export/writeback aliasing rather than the first non-packet-local consequence
- the first downstream consequence that is no longer packet-local begins at the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` boundary described in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp`, with the already-fixed `REF-07` lane evidence kept intact (`owner_label="Command Graph (L88) (Draw)"`, `attachment_load_ops=[0:LOAD]`, `first_blend_mode="mix"`, `blend_enabled_attachment_mask="0x1"`)
- the tightest honest wording for that first outward step is: the exported source alpha descended from `d` becomes the **source-alpha admission coefficient for incoming packet color** at the preserved `L88` merge over loaded root contents

## 2026-05-26 — coder note for bead `oc-mn52` (tightest surviving non-packet-local source-alpha admission seam after the `a = d` rejoin)

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-non-packet-local-source-alpha-admission-seam-2026-05-26.md`

What stayed fixed from the already-approved seam:

- Task 196 had already fully rejoined the packet-local ladder as **`a = d`**
- Task 197 had already stepped outward and fixed the first non-packet-local consequence as source-alpha-driven incoming packet-color admission at the preserved `Command Graph (L88)` merge
- this slice therefore did **not** reopen `color.a`, `frag_color.a`, or the broader packet-local/export ladder; it stayed inside the preserved `L88` merge framing only

Exact source-backed classification:

- the live merge contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` still uses `BLEND_MODE_MIX` with `src_color_blend_factor = SRC_ALPHA` and `dst_color_blend_factor = ONE_MINUS_SRC_ALPHA`
- once the question is constrained to the **non-packet-local** admission seam, the tightest surviving carrier is the immediate merge-side source-alpha coefficient **`src.a`**
- the later admitted incoming color term **`src.rgb * src.a`** is already broader because it bundles the packet RGB payload with that already-live coefficient
- packet/export-side names (`color.a`, `frag_color.a`) remain true provenance, but they are no longer the right seam once the packet-local ladder is explicitly closed and the classification stays inside the merge side

Best current wording after this slice:

- after the exact first-L88 `a = d` rejoin, the tightest surviving **non-packet-local** source-alpha admission seam at the preserved `Command Graph (L88)` merge is the immediate blend-side admission coefficient `src.a`
- `src.rgb * src.a` remains the first broader composite consequence of that coefficient
- broader preserve/load framing, destination retention, and later alpha writeback remain real context/consequences, but they are not tighter than that first active non-packet-local admission role

This keeps specialization-cache, request-hash, CPU-side vertex-format-cache, and speculative-fix theories demoted on the current approved evidence. The next honest seam, if the plan wants to continue outward, is inside that same preserved `L88` merge framing rather than back inside the rejoined packet-local `a` / `d` ladder.

## 2026-05-26 — coder note for bead `oc-mmaw` (next narrower source-backed consequence of the preserved `L88` merge-side `src.a` admission coefficient)

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-src-a-source-backed-consequence-2026-05-26.md`

What stayed fixed from the already-approved seam:

- Task 198 had already fixed the tightest surviving non-packet-local source-alpha admission seam at the preserved `Command Graph (L88)` merge as the immediate coefficient `src.a`
- this slice therefore did **not** reopen `color.a`, `frag_color.a`, or the earlier packet/export ladder
- it also did **not** broaden back to the already-demoted destination-retention side (`dst.rgb * (1 - src.a)`, `dst.a * (1 - src.a)`)

Exact source-backed classification:

- re-reading the live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` keeps the merge equations fixed as `color_out = src.rgb * src.a + dst.rgb * (1 - src.a)` and `alpha_out = src.a * 1 + dst.a * (1 - src.a)`
- once Task 198 has already fixed the seam at the coefficient `src.a`, the next honest continuation must stay on the source side and ask what that coefficient first actively admits
- the next narrower source-backed consequence is therefore the admitted incoming source-color term **`src.rgb * src.a`**
- the source-side alpha term **`src.a * 1`** remains real, but it is the broader sibling post-merge alpha consequence rather than the tightest continuation on the already-selected source-admission lane

Best current wording after this slice:

- after the preserved `Command Graph (L88)` merge-side admission seam is fixed at `src.a`, the next narrower source-backed consequence is the admitted incoming source-color term `src.rgb * src.a`
- `src.a * 1` remains a real sibling post-merge effect, but it is not the tightest continuation on the already-selected source-admission lane
- destination-retention remains demoted on this slice and should stay closed unless new evidence contradicts the earlier Tasks 189-191 reductions

## 2026-05-26 — coder note for bead `oc-xwyv` (split the preserved `L88` admitted source-color term `src.rgb * src.a` one rung deeper)

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-color-carrier-vs-admitted-color-term-2026-05-26.md`

What stayed fixed from the already-approved seam:

- Task 198 had already fixed the tightest surviving non-packet-local source-alpha admission seam at the preserved `Command Graph (L88)` merge as the immediate coefficient `src.a`
- Task 199 had already fixed the next narrower source-backed consequence of that coefficient as the admitted incoming source-color term `src.rgb * src.a`
- this slice therefore did **not** reopen packet-local RGB composition, the earlier alpha/export ladder, or the already-demoted destination-retention side

Exact source-backed classification:

- re-reading the live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` keeps the color-side merge equation fixed as `color_out = src.rgb * src.a + dst.rgb * (1 - src.a)`
- once the admitted source-color continuation has already been fixed at `src.rgb * src.a`, the next honest one-rung-deeper split on that same lane lands at the merge-visible source-color carrier **`src.rgb`**
- `src.rgb * src.a` is already broader at this rung because it bundles that carrier with the already-fixed admission coefficient `src.a`
- this keeps the source-admission lane honest without stepping backward into packet-local RGB composition or outward into destination-retention

Best current wording after this slice:

- inside the preserved `Command Graph (L88)` admitted source-color lane, the next honest one-rung-deeper split lands at the merge-visible source-color carrier `src.rgb`
- the broader admitted term `src.rgb * src.a` is the first composite consequence of that carrier under the already-fixed admission coefficient `src.a`
- destination-retention remains demoted on this slice and should stay closed unless new evidence contradicts the earlier reductions

## 2026-05-26 — coder note for bead `oc-gmzv` (classify a narrower source-backed consequence of the preserved `L88` merge-visible source-color carrier `src.rgb`)

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-merge-visible-src-rgb-terminal-stop-2026-05-26.md`

What stayed fixed from the already-approved seam:

- Task 198 had already fixed the tightest surviving non-packet-local source-alpha admission seam as the immediate coefficient `src.a`
- Task 199 had already fixed the next narrower source-backed consequence as the admitted incoming source-color term `src.rgb * src.a`
- Task 200 had already split that admitted term one rung deeper at the merge-visible source-color carrier `src.rgb`
- this slice therefore did **not** reopen packet-local RGB composition, the earlier alpha/export ladder, or the already-demoted destination-retention side

Exact source-backed classification:

- re-reading `servers/rendering/renderer_rd/shaders/canvas.glsl` keeps the merge-visible export handoff fixed at `frag_color = color`
- re-reading the live `BLEND_MODE_MIX` contract in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` keeps the color-side merge equation fixed as `color_out = src.rgb * src.a + dst.rgb * (1 - src.a)`
- under those fixed facts, there is **no still-narrower source-backed consequence** of merge-visible `src.rgb` available under the current constraints
- any next inward split would reopen forbidden packet-local RGB composition, while any next outward continuation returns to the already-broader admitted composite term `src.rgb * src.a`

Best current wording after this slice:

- inside the preserved `Command Graph (L88)` admitted source-color lane, `src.rgb` is the terminal merge-visible source-color carrier on the current approved evidence
- no still-narrower source-backed consequence survives without either reopening packet-local RGB composition or broadening back out to the already-composite admitted term `src.rgb * src.a`
- destination-retention remains demoted on this slice and should stay closed unless new evidence contradicts the earlier reductions

## 2026-05-26 — Task 202: exact packet-local RGB composition immediately upstream of merge-visible `src.rgb`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-packet-local-rgb-upstream-of-src-rgb-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 packet:

- Task 201 terminated the old source-admission lane at merge-visible `src.rgb` only because further inward motion would have reopened packet-local RGB composition
- reopening that seam one rung inward shows the immediate packet-local chain is `color_interp.rgb -> color.rgb -> frag_color.rgb -> src.rgb`
- on the exact no-outline MSDF rect lane in `canvas.glsl`, the MSDF branch mutates only `color.a`; it does not rewrite `color.rgb`
- that means the exact packet-local RGB composition immediately upstream of merge-visible `src.rgb` is the unchanged inherited packet RGB `color_interp.rgb`, carried through packet-local `color.rgb` and exported as `frag_color.rgb`
- the sampled MSDF texture RGB remains upstream only of the already-classified alpha-distance ladder (`msdf_sample.r/g/b -> d -> a -> color.a`), not of the live RGB carrier on this slice

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, CPU-side vertex-format-cache theory, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward split is `color.rgb -> color_interp.rgb`, i.e. tracing the exact rect-path source that populates `color_interp` for this packet rather than jumping back to sampled MSDF RGB or outward to the merge composite

## 2026-05-26 — Task 203: exact rect-path source that populates first-`L88` packet-local `color_interp.rgb`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-rect-path-source-of-color-interp-rgb-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 rect packet:

- on the non-attribute rect path in `servers/rendering/renderer_rd/shaders/canvas.glsl`, the vertex shader sets `vec4 color = read_draw_data_modulation;` and then `color_interp = color;`
- on that same rect path, `read_draw_data_modulation` is `attrib_C`, so packet-local `color_interp.rgb` comes directly from the rect instance-buffer modulation payload rather than from sampled MSDF RGB
- the CPU-side rect-path producer in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` composes `Color modulated = rect->modulate * base_color;` with `base_color = p_item->final_modulate`, then writes `modulated.r/g/b/a` into `instance_data->modulation[0..3]`
- the matching `InstanceData` layout in `servers/rendering/renderer_rd/renderer_canvas_render_rd.h` stores `float modulation[4]`, which feeds shader `attrib_C` on the static rect instance format
- so the exact population chain for this slice is `rect->modulate.rgb * p_item->final_modulate.rgb -> instance_data->modulation.rgb -> attrib_C.rgb -> read_draw_data_modulation.rgb -> color_interp.rgb`
- `p_item->final_modulate` itself is already a cull-stage item modulation product (`ci->final_modulate = p_modulate * ci->self_modulate` in `servers/rendering/renderer_canvas_cull.cpp`), but this slice stops there rather than reopening farther ancestor modulation provenance

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, CPU-side vertex-format-cache theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward split is the CPU-side modulation product `rect->modulate.rgb * p_item->final_modulate.rgb`, i.e. determine whether the tightest surviving first-`L88` RGB identity should remain at that product as a whole or split one rung deeper between the rect command contribution and the already-cull-composed item contribution

## 2026-05-26 — Task 204: split the first-L88 rect-path modulation product feeding packet-local `color_interp.rgb`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-rect-modulation-product-split-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 rect packet:

- the renderer-side producer in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` composes `Color modulated = rect->modulate * base_color;` with `base_color = p_item->final_modulate`, so the Task 203 source product is already composite rather than irreducible
- once the question moves one rung inward, that CPU-side product should not remain whole; the honest split is `rect->modulate.rgb × p_item->final_modulate.rgb`
- `rect->modulate.rgb` is the exact rect-command-owned contribution on this rung; `renderer_canvas_cull.cpp` writes it directly into the packet command via `rect->modulate = p_modulate` for the rect/MSDF texture-rect path
- `p_item->final_modulate.rgb` is the broader inherited item-side sibling on this rung; the cull stage composes it earlier as `ci->final_modulate = p_modulate * ci->self_modulate`, and the render path later consumes that pre-composed carrier as `base_color`
- so the tighter surviving exact packet-owned RGB factor is `rect->modulate.rgb`, while `p_item->final_modulate.rgb` remains a live but broader inherited contributor to the same renderer-side product

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the tightest next inward continuation is to stay on the exact rect-command-owned side and classify whether `rect->modulate.rgb` is terminal for this lane or has a still-earlier source identity worth tracing, rather than jumping outward to the broader inherited `p_item->final_modulate.rgb` side

## 2026-05-26 — Task 205: classify whether first-L88 rect-command `rect->modulate.rgb` is terminal or has a still-earlier source identity

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-rect-command-modulate-source-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 MSDF rect packet:

- `rect->modulate.rgb` is **not** terminal for this lane
- the exact owning write in `servers/rendering/renderer_canvas_cull.cpp` is `rect->modulate = p_modulate`, including on the relevant `canvas_item_add_msdf_texture_rect_region(..., const Color &p_modulate, ...)` path
- so the next honest inward provenance step is the direct alias `p_modulate.rgb -> rect->modulate.rgb`
- there is no additional renderer-internal RGB composition between those two identities on this path
- live source also keeps that provenance intact at the higher API layers: `CanvasItem::draw_msdf_texture_rect_region(..., p_modulate, ...)` forwards the same modulation argument to `RenderingServer::canvas_item_add_msdf_texture_rect_region(...)`, and `ImageTexture::draw_msdf_rect_region(..., p_modulate, ...)` does the same

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is the caller-side source of `p_modulate.rgb` feeding `canvas_item_add_msdf_texture_rect_region(..., p_modulate, ...)`, while keeping the broader inherited `p_item->final_modulate.rgb` sibling lane closed

## 2026-05-26 — Task 206: classify the exact caller-side source of first-L88 MSDF draw `p_modulate.rgb`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-caller-side-source-of-p-modulate-rgb-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 no-outline MSDF packet:

- the next honest caller-side source is `p_color.rgb`, not another renderer-side factor
- on the live MSDF glyph path in `modules/text_server_adv/text_server_adv.cpp`, the code does `Color modulate = p_color;` and then, on the no-outline MSDF branch, calls `draw_msdf_rect_region(..., modulate, 0, ...)`
- `ImageTexture::draw_msdf_rect_region(..., const Color &p_modulate, ...)` then forwards that modulation unchanged to `RenderingServer::canvas_item_add_msdf_texture_rect_region(...)`
- so the exact caller-side provenance chain for this slice is `p_color.rgb -> modulate.rgb -> p_modulate.rgb -> rect->modulate.rgb`
- broader public text/font APIs preserve the same identity rather than recomposing it: `TextServer::font_draw_glyph(..., p_color, ...)`, `Font::draw_char(..., p_modulate, ...)` forwarding to `font_draw_glyph`, and `TextServer::shaped_text_draw(..., p_color, ...)` forwarding `p_color` into `font_draw_glyph`

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is the exact higher caller-side source of `p_color.rgb` for the preserved packet, while keeping the broader inherited `p_item->final_modulate.rgb` sibling lane closed

## 2026-05-26 — Task 207: classify the exact higher caller-side source of first-L88 glyph-draw `p_color.rgb`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-higher-caller-side-source-of-p-color-rgb-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 no-outline MSDF packet:

- the next honest higher caller-side source on the shared text path is `TextServer::shaped_text_draw(..., p_color, ...)`, not another renderer-side factor
- `servers/text/text_server.cpp` forwards its own `p_color` directly into repeated `font_draw_glyph(..., p_color, ...)` submissions while drawing shaped glyphs
- `modules/text_server_adv/text_server_adv.cpp` then receives that same `p_color` in `_font_draw_glyph(...)`, where the already-classified no-outline MSDF path does `Color modulate = p_color;` before forwarding it into the MSDF rect draw
- so the exact higher shared provenance chain for this slice is `TextServer::shaped_text_draw(..., p_color, ...) -> TextServer::font_draw_glyph(..., p_color, ...) -> TextServerAdvanced::_font_draw_glyph(..., p_color, ...) -> modulate.rgb -> p_modulate.rgb -> rect->modulate.rgb`
- static source does **not** yet justify collapsing that farther to one unique widget/control caller for the preserved packet, because above this rung the engine fans out into multiple callers such as direct widget `font_draw_glyph(..., font_color)` paths and `Font::draw_char(..., p_modulate, ...)`

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is to classify the exact higher caller-side source of `TextServer::shaped_text_draw(..., p_color, ...)` for the preserved packet, or explicitly prove that the packet instead comes from one of the direct widget/control `font_draw_glyph(..., font_color)` call sites

## 2026-05-26 — Task 208: classify the exact higher caller-side source above `TextServer::shaped_text_draw(..., p_color, ...)` or prove a direct widget glyph caller

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-source-above-shaped-text-draw-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 no-outline MSDF packet:

- static source does **not** justify collapsing the preserved packet one rung farther to one unique higher caller above `TextServer::shaped_text_draw(..., p_color, ...)`
- live `shaped_text_draw(..., p_color, ...)` callers include multiple distinct families such as `TextLine::draw(...)`, `TextParagraph::draw(...)`, and `CodeEdit::_draw_line_numbers()`
- live direct widget/control `font_draw_glyph(..., font_color)` callers also exist in parallel, including `Label`, `RichTextLabel`, `LineEdit`, and `TextEdit`
- static source alone does **not** prove that the preserved packet comes from one of those direct widget glyph paths instead of the already-identified shared `shaped_text_draw(...)` path
- so the honest result for this rung is a precise ambiguity: the provenance remains valid up to `TextServer::shaped_text_draw(..., p_color, ...)`, but above that point the code fans into multiple live caller families and no unique winner is proven by static source alone

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest seam is not another static-source-only collapse; it is a narrow evidence step to distinguish the higher caller family for the preserved packet, such as source-backed breadcrumbing or the smallest reversible contrast that can separate the shared `shaped_text_draw(..., p_color, ...)` family from the direct widget/control `font_draw_glyph(..., font_color)` family

## 2026-05-26 — Task 209 follow-through: resolve the higher caller family using the later narrow owner-path evidence

Durable note for this resolution slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-caller-family-shaped-vs-direct-font-draw-glyph-2026-05-26.md`

Resolved classification for the exact first-L88 no-outline MSDF packet:

- Task 208's static-source stop point is now resolved by combining the later narrow owner attribution and owner-path step classification on the same preserved lane
- the packet owner was fixed to the direct `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel` RichTextLabel canvas item route
- inside that owner path, the matching emission surface was fixed to `RichTextLabel::_draw_line(...) -> DRAW_STEP_TEXT -> TS->font_draw_glyph(..., font_color)`
- so the packet's higher caller family is the direct widget/control `font_draw_glyph(..., font_color)` family, not the shared `TextServer::shaped_text_draw(..., p_color, ...)` family

Why the locked crash envelope stays preserved:

- this slice adds no new runtime instrumentation and no new speculative contrast; it reuses the smallest already-collected distinguishing evidence from the same preserved lane
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is no longer which caller family won; it is to stay on the fixed direct `HudLabel` RichTextLabel route and split one rung deeper into the exact first line-0 heading glyph emission itself and/or the immediate `font_color` / `frid` selection received by that `DRAW_STEP_TEXT` call

## 2026-05-26 — Task 211: classify the heading bold-font `frid` lane behind the first-L88 direct RichTextLabel packet

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-bold-font-frid-lane-2026-05-26.md`

Resolved classification for the exact first-L88 direct `HudLabel` heading packet:

- the heading `[b]` tag selects `_push_def_font(RTL_BOLD_FONT)`, creating an `ItemFont` with `def_font = RTL_BOLD_FONT` and `def_size = true`
- `_find_font(...)` then resolves that lane directly to `theme_cache.bold_font`, and because `def_size` is true it also resolves the size side to `theme_cache.bold_font_size`
- `RichTextLabel` binds both of those as regular theme items (`bold_font` and `bold_font_size`), so this is a normal Control theme-resolution path rather than a GDGS-specific font branch
- the default theme assigns the RichTextLabel `bold_font` slot explicitly and stores `bold_font_size = -1`, which source-backed theme lookup means falls through to the theme default font size on this route
- the repro scene/script do not author any `HudLabel` theme/font/font-size overrides, so `_shape_line(...).add_string(tx, font, font_size, ...)` feeds the stock resolved bold font + default-size path into shaping before draw later consumes `glyphs[i].font_rid`

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward owner identity, shadow/outline branches, destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is no longer the broad bold-font theme path; it is to resolve one rung deeper into the exact concrete font resource that populates `theme_cache.bold_font` and/or the exact numeric theme-default size that the `-1` `bold_font_size` slot falls through to on the preserved runtime path

## 2026-05-26 — Task 212: resolve the exact concrete bold-font resource and exact default size behind the first-L88 heading `frid` lane

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-bold-font-resource-and-size-2026-05-26.md`

Resolved classification for the exact first-L88 direct `HudLabel` heading packet:

- the repro scene/project still show no custom GUI font or `HudLabel` theme/font/font-size override path, so the packet stays on the engine default-theme route
- on that route, `theme_cache.bold_font` is not a mystery label-local object; `default_theme.cpp` constructs it as a `FontVariation` with `set_base_font(default_font)` and `set_variation_embolden(1.2)`
- if no custom project font is supplied, that `default_font` is an instantiated `FontFile` loaded from the embedded `_font_OpenSans_SemiBold` payload
- RichTextLabel stores `bold_font_size = -1` in the default theme, and source-backed theme lookup means that falls through to the theme default font size
- the default theme sets that theme default font size to `Math::round(16 * scale)`, so on the ordinary default-scale path the exact numeric fallback is `16 px`

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward `font_color`, caller-family identity, owner identity, destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is to resolve the final step from this now-concrete theme resource/size lane into the shaped-glyph side itself, such as where the resolved `FontVariation` + `16 px` inputs become the eventual `glyphs[i].font_rid` / glyph index pair for the first visible heading `G`

## 2026-05-26 — Task 213: resolve the final shaped-glyph transition from the concrete bold theme inputs to the first heading glyph's `font_rid` / glyph-index pair

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-heading-glyph-shaping-font-rid-and-index-2026-05-26.md`

Resolved classification for the exact first-L88 direct `HudLabel` heading route:

- source already fixed the path up to shaping, but source alone stopped short of the exact runtime numeric `RID(...)` value and glyph index for the first visible heading `G`
- a smallest reversible headless probe shaped the heading body with the actual `HudLabel` `bold_font` / `bold_font_size` route and inspected the first shaped glyph entry
- on the preserved repro runtime (`4.7.dev5.official.a8643700c`), the first shaped heading glyph covers `start=0`, `end=1` and resolves to `font_rid = RID(687194767361)`, `font_size = 16`, `index = 42`
- a direct `font_get_glyph_index(..., 'G', 0)` check on that same `font_rid` / size also returned `42`, confirming the first visible heading `G` mapping

Why the locked crash envelope stays preserved:

- this slice used the smallest reversible runtime trace needed to close the exact numeric shaping pair that source alone could not provide
- it kept the same direct `HudLabel` heading route and did not widen back into caller-family identity, owner identity, destination-retention, specialization/request-hash theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward seam is to decide whether the first preserved clipped MSDF rect packet can now be pinned directly to this exact shaped heading `G` emission, or whether one last minimal packet-bridge trace is still needed

## 2026-05-26 — Task 204: classify whether the first-`L88` CPU-side modulation product stays whole or splits deeper between the rect-command contribution and the already-cull-composed item contribution

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-rect-modulate-identity-vs-item-final-modulate-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 rect packet:

- the already-approved exact packet artifact logs the rect command with `modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}`, so for this packet the rect-command-local multiplicand is exact identity white rather than an unknown colored factor
- the CPU-side rect path in `servers/rendering/renderer_rd/renderer_canvas_render_rd.cpp` still composes `Color modulated = rect->modulate * base_color` with `base_color = p_item->final_modulate` and writes that result into `instance_data->modulation[0..3]`
- substituting the exact packet-local rect command identity collapses the broader product to `rect->modulate.rgb * p_item->final_modulate.rgb = (1,1,1) * p_item->final_modulate.rgb = p_item->final_modulate.rgb`
- the cull-stage item contribution in `servers/rendering/renderer_canvas_cull.cpp` remains `ci->final_modulate = p_modulate * ci->self_modulate`, so the surviving non-identity RGB carrier after this one-rung split is the already-cull-composed item contribution `p_item->final_modulate.rgb`
- therefore, for this exact preserved packet, the CPU-side modulation product should not remain whole; it honestly splits one rung deeper, and the rect-command side immediately collapses away as identity

Why the locked crash envelope stays preserved:

- this slice is documentation-only and uses already-approved first-L88 packet artifacts plus source reads only; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, CPU-side vertex-format-cache theory, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward split is the cull-composed item modulation itself: `p_item->final_modulate.rgb = ci->final_modulate.rgb = p_modulate.rgb * ci->self_modulate.rgb`, i.e. determine whether the tightest surviving first-`L88` RGB identity should remain at `p_item->final_modulate.rgb` as a whole or split one rung deeper between the inherited cull input `p_modulate.rgb` and the item-local contribution `ci->self_modulate.rgb`

## 2026-05-26 — Task 205: classify whether first-`L88` cull-side item modulation stays whole or splits deeper between inherited cull input and item-local `self_modulate`

Durable note for this slice:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-item-final-modulate-inherited-vs-self-2026-05-26.md`

Narrow source-backed classification for the exact first-L88 rect packet:

- the cull stage composes the current item's draw-time modulation as `ci->final_modulate = p_modulate * ci->self_modulate` in `servers/rendering/renderer_canvas_cull.cpp`, so the Task 204 survivor `p_item->final_modulate.rgb` is already composite rather than irreducible
- the same cull function also shows the broader inherited carrier relationship: `Color modulate = ci->modulate * p_modulate;` and child recursion passes that inherited result onward with `_cull_canvas_item(..., modulate, ...)`, so `p_modulate` is the already-inherited modulation input arriving from the parent/ancestor chain
- the docs keep the ownership split explicit: `CanvasItem.modulate` affects the item and its children, `CanvasItem.self_modulate` affects only the node itself, and `canvas_item_set_parent()` states that a child inherits modulation from its parent
- therefore, if this surviving cull-side modulation is pushed one rung inward, it should not remain whole: it honestly splits as `p_modulate.rgb | ci->self_modulate.rgb`
- on that split, `p_modulate.rgb` is the broader inherited cull input, while `ci->self_modulate.rgb` is the tighter exact item-local RGB factor for the preserved packet's owning canvas item

Why the locked crash envelope stays preserved:

- this slice is documentation-only and source-backed; it adds no new runtime instrumentation and no new contrast run
- it keeps the same exact packet (`first clipped preserve-rect Command Graph (L88) no-outline MSDF packet`) and the same outer identity only as context (`submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`)
- it does not widen back toward destination-retention, specialization/request-hash theory, CPU-side vertex-format-cache theory beyond this exact modulation seam, sampled-MSDF-alpha questions already settled elsewhere, or speculative fixes

Next seam now made explicit:

- if continuation is still wanted on the same preserved lane, the next honest inward continuation is to stay on the exact item-local side and classify whether `ci->self_modulate.rgb` is terminal for this lane or has a still-earlier exact source identity worth tracing for the preserved packet, rather than jumping back out to the broader inherited `p_modulate.rgb` side

## 2026-05-26 — Task 206: classify whether first-`L88` item-local `ci->self_modulate.rgb` is terminal or traces to a still-earlier exact source identity

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-self-modulate-terminal-vs-caller-source-2026-05-26.md`

Source-backed conclusion:

- the cull-stage composition still uses `ci->final_modulate = p_modulate * ci->self_modulate`, so this slice stayed on the tighter item-local side only
- `ci->self_modulate` is **not** the terminal origin for the lane's full provenance; it is only the last renderer-cull storage slot before composition
- the exact write into that renderer field is `RendererCanvasCull::canvas_item_set_self_modulate(..., p_color) { canvas_item->self_modulate = p_color; }`
- that setter is itself just the RenderingServer-side alias of the scene/API-side CanvasItem property write: `CanvasItem::set_self_modulate(const Color &p_self_modulate) { self_modulate = p_self_modulate; RenderingServer::get_singleton()->canvas_item_set_self_modulate(canvas_item, self_modulate); }`
- the owning earlier source identity is therefore `CanvasItem.self_modulate` / `set_self_modulate(p_self_modulate)`, not the copied renderer-cull field alone
- best narrow wording after this slice: `ci->self_modulate.rgb` is terminal only as the last renderer-cull storage slot, but not terminal for the lane's full source path because it is forwarded from the scene/API-side `CanvasItem.self_modulate.rgb` value

Validation for this documentation slice:

- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-self-modulate-terminal-vs-caller-source-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

Exact next seam now materialized:

- if continuation is still wanted on the same preserved lane, stay on this item-local side and classify the still-earlier owner/caller of the preserved packet's `CanvasItem.self_modulate.rgb` value — i.e. whether the owning CanvasItem simply retains the default white property or receives a specific non-default `set_self_modulate(...)` / RenderingServer write from an identifiable scene-side caller

## 2026-05-26 — Task 207: classify whether first-`L88` `CanvasItem.self_modulate.rgb` stays at the default white property or comes from a concrete non-default scene-side write

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-self-modulate-default-vs-scene-write-2026-05-26.md`

Source-backed conclusion:

- the engine-side property lane still starts from `CanvasItem::self_modulate = Color(1, 1, 1, 1)` and only changes if `CanvasItem::set_self_modulate(...)` forwards a new value through `RenderingServer::canvas_item_set_self_modulate(...)`
- a project-wide scan of `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/` over `*.gd`, `*.tscn`, and `*.cs` finds **no** occurrences of `self_modulate`, `set_self_modulate`, or `canvas_item_set_self_modulate`
- the reproducer's scene-authored UI text path is just `CanvasLayer/HudMargin/HudLabel` (`RichTextLabel`), and neither `scenes/gdgs_happy_path_control.tscn` nor `scripts/build_control_scene.gd` serializes or writes a non-default self-modulate value for that label
- the preserved packet artifact still logs `modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}`, which is consistent with the default-property reading even though that packet field is broader than `self_modulate` alone
- best narrow classification: for this preserved lane, `CanvasItem.self_modulate.rgb` remains the default white property value, not an identifiable non-default scene-side write

Validation for this documentation slice:

- `grep -RIn "self_modulate\|set_self_modulate\|canvas_item_set_self_modulate" /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --include='*.tscn' --include='*.gd' --include='*.cs' || true`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scenes/gdgs_happy_path_control.tscn | sed -n '56,78p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scripts/build_control_scene.gd | sed -n '68,88p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/main/canvas_item.h | sed -n '84,94p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/main/canvas_item.cpp | sed -n '580,592p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_canvas_cull.cpp | sed -n '696,706p'`
- `grep -RIn "temp_diag_first_clipped_preserve_rect_batch=\|modulate={r=1.000000,g=1.000000,b=1.000000,a=1.000000}" /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/baseline/stdout.log | head -n 5`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-self-modulate-default-vs-scene-write-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

Exact next seam now materialized:

- if stricter packet-owner proof is still wanted on this same item-local side, the next honest seam is to attribute the exact first clipped preserve-rect packet to its owning CanvasItem/RID/path and decide whether that packet comes from the `HudLabel` CanvasItem directly or a RichTextLabel-internal item on the same label path; that seam would strengthen owner identity only and is not required for the current default-white classification

## 2026-05-26 — Task 208: attribute the exact first clipped preserve-rect packet to its owning CanvasItem/RID/path

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-packet-owner-hudlabel-vs-richtext-internal-2026-05-26.md`

Source-backed conclusion:

- the reproducer scene's authored UI text owner on this lane is `CanvasLayer/HudMargin/HudLabel`, and that node is a `RichTextLabel`
- the decisive engine-side ownership trace is in `scene/gui/rich_text_label.cpp`: both `NOTIFICATION_DRAW` and `_draw_line(...)` begin from `RID ci = get_canvas_item()`
- the emitted text/glyph path then uses that same `ci` for actual packet emission, including `TS->font_draw_glyph(...)`, `TS->font_draw_glyph_outline(...)`, and label-side `canvas_item_add_rect(...)`
- the text-server implementation keeps that same owner RID all the way to `texture->draw_rect_region(p_canvas, ...)`, so the glyph packet stays on the `ci` passed by `RichTextLabel`
- the smallest reversible runtime trace on the control scene resolved the live owner to `/root/GdgsHappyPathControl/CanvasLayer/HudMargin/HudLabel` with `HudLabel.get_canvas_item().get_id() == 128849018881`
- the only internal child `CanvasItem` surfaced by `HudLabel.get_children(true)` was the internal `VScrollBar`, and it had a different RID (`146028888066`)
- therefore the preserved first clipped no-outline MSDF rect packet is best classified as emitted onto the direct `HudLabel` canvas item RID (`HudLabel.get_canvas_item()`), not onto a separate RichTextLabel-internal canvas item owner

Validation for this documentation slice:

- `grep -nE "RID ci = get_canvas_item\(|font_draw_glyph\(|font_draw_glyph_outline\(|canvas_item_add_rect\(" /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1,40p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1064,1080p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1624,1640p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '2712,2810p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/modules/text_server_adv/text_server_adv.cpp | sed -n '4316,4362p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/modules/text_server_fb/text_server_fb.cpp | sed -n '2958,3004p'`
- temporary validation trace, then reverted: `~/.local/bin/godot --headless --path /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs --quit-after 3`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scenes/gdgs_happy_path_control.tscn | sed -n '56,72p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scripts/build_control_scene.gd | sed -n '58,84p'`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-packet-owner-hudlabel-vs-richtext-internal-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

Exact next seam now materialized:

- if continuation is still wanted on the same preserved lane, stay on the now-fixed direct `HudLabel` owner route and classify which exact RichTextLabel text/glyph emission step produces the first clipped white no-outline MSDF rect packet inside that direct owner path, rather than reopening owner identity or broader theories.

## 2026-05-26 — Task 209: classify which exact RichTextLabel text/glyph emission step produces the first clipped white no-outline MSDF rect packet inside the direct `HudLabel` owner path

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-richtextlabel-emission-step-2026-05-26.md`

Source-backed conclusion:

- inside `RichTextLabel::_draw_line(...)`, the relevant glyph-emission families are `DRAW_STEP_TEXT -> font_draw_glyph(...)`, `DRAW_STEP_SHADOW -> font_draw_glyph(...)`, and the outline families `DRAW_STEP_SHADOW_OUTLINE` / `DRAW_STEP_OUTLINE -> font_draw_glyph_outline(...)`
- the preserved packet artifact remains `outline=0.000000` with white modulation, so the packet does not match the outline families
- those outline branches are also skipped on this reproducer path because the default runtime RichTextLabel theme sets `outline_size = 0`, and the scene/script do not introduce outline BBCode or theme overrides
- the shadow branch is likewise skipped on this reproducer path because the default runtime RichTextLabel theme sets `font_shadow_color = Color(0, 0, 0, 0)`, so `font_shadow_color.a == 0`
- the surviving exact emission step is therefore the normal visible-text branch: `DRAW_STEP_TEXT -> TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color)`
- this also matches the preserved packet's white modulation because the default runtime RichTextLabel theme sets `default_color = Color(1, 1, 1)` and the reproducer's `HudLabel` BBCode uses bold tags only, not color tags

Validation for this documentation slice:

- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1348,1372p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '1600,1642p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '2790,2810p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/theme/default_theme.cpp | sed -n '1221,1238p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scenes/gdgs_happy_path_control.tscn | sed -n '67,80p'`
- `grep -n "temp_diag_first_clipped_preserve_rect_batch=" /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-24/official-first-l88-specialization-value-only-vulkan-sourcebuild-20260525-183916/baseline/stdout.log | head -n 1`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-richtextlabel-emission-step-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

Exact next seam now materialized:

- if continuation is still wanted on the same preserved lane, stay on the now-fixed direct `DRAW_STEP_TEXT -> font_draw_glyph(...)` route and classify the tightest useful next rung inside that branch — either the `font_color` identity after `_find_color(...)` / bold-format resolution, or the exact first glyph/subspan within the `HudLabel` BBCode text that maps to the preserved clipped packet.

## 2026-05-26 — Task 219: resolve the exact source-backed origin of the direct `HudLabel` clip rect left/top edge `(16,16)`

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-hudlabel-clip-origin-16-16-2026-05-26.md`

Source-backed conclusion:

- the direct `HudLabel` clip edge does not pick up its `(16,16)` left/top from glyph-local geometry or a hidden late draw offset
- the repro scene and builder script both author the parent `HudMargin` `MarginContainer` at `offset_left = 16`, `offset_top = 16`
- because `HudMargin` sits directly under `CanvasLayer`, its anchorable parent rect on this route is the viewport visible rect; the `CanvasLayer` itself contributes no authored transform shift here
- the default theme gives `MarginContainer` zero margins on all four sides, so `MarginContainer::_notification(NOTIFICATION_SORT_CHILDREN)` lays out `HudLabel` into an inner rect that begins at `(0,0)`
- `HudLabel` is therefore placed at local `(0,0)` inside `HudMargin`, and `Control::_update_canvas_item_transform()` plus `Control::get_global_rect()` expose the resulting owner global origin as `(16,16)`
- because `RichTextLabel` has `clip_contents=true`, that same owner global-rect origin becomes the direct clip rect left/top edge behind the already-fixed packet/clip overlap fact

Validation for this documentation slice:

- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scenes/gdgs_happy_path_control.tscn | sed -n '59,70p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/scripts/build_control_scene.gd | sed -n '68,85p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/control.cpp | sed -n '708,726p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/control.cpp | sed -n '755,769p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/control.cpp | sed -n '1573,1594p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/container.cpp | sed -n '109,150p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/margin_container.cpp | sed -n '117,132p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/theme/default_theme.cpp | sed -n '1268,1271p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/main/canvas_layer.cpp | sed -n '90,103p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/scene/gui/rich_text_label.cpp | sed -n '8453,8458p'`
- `nl -ba /home/derrick/.openclaw/workspace/projects/godot/servers/rendering/renderer_canvas_cull.cpp | sed -n '413,424p'`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-hudlabel-clip-origin-16-16-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`

## 2026-05-26 — Task 220: run a minimal reversible contrast that removes only the first heading `G` packet's 1 px left-edge non-containment

Durable note:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-left-edge-only-contrast-2026-05-26.md`

Fresh artifact root:

- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/official-first-l88-left-edge-contrast-vulkan-sourcebuild-20260526-201400/`

Tight result:

- the smallest successful reversible contrast was a runtime-only `HudLabel` `normal` stylebox override that increased the effective left content margin from `0` to `1`, leaving the direct owner clip rect fixed at `clip_rect={x=16,y=16,w=504,h=460}` while shifting the first traced heading-`G` packet from `command.rect={x=-1,y=3,w=14,h=16}` to `command.rect={x=0,y=3,w=14,h=16}`
- that means the exact 1 px left-edge-only non-containment was removed: the packet moved from global `(15,19,14,16)` to global `(16,19,14,16)` against the same owner clip origin `(16,16)`
- despite that geometry change, the preserved failing lane stayed unchanged in both fresh launches: `temp_diag_clipped_preserve_rect_gate_result={matched=10,skipped=9}`, `fence_wait_error submit_serial=9`, command-summary tail still ending on `Tonemap (L87) (Draw) -> Command Graph (L88) (Draw)`, later breadcrumb still reaching `BLIT_PASS`, and process exit still `134` / signal `6`
- the honest read is therefore that the first heading `G` packet's 1 px left-edge-only partial overlap is **not required** for either the clipped preserve-rect family classification or the surviving crash identity on this lane; it is a coexisting packet-local fact, not the dependency that explains the lane

Validation for this documentation slice:

- `python3 /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/run_first_l88_left_edge_contrast.py`
- `cat /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/official-first-l88-left-edge-contrast-vulkan-sourcebuild-20260526-201400/comparison.txt`
- `cat /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-26/official-first-l88-left-edge-contrast-vulkan-sourcebuild-20260526-201400/results.json`
- `git diff --check -- /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-left-edge-only-contrast-2026-05-26.md /home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-compositor-staged-qa-2026-05-17.md /home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-local-rd-compositor-instrumentation.md`
