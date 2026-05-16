# Godot

**Date:** 2026-05-16  
**Status:** Complete  
**Agent:** Chip 🐱‍💻

---

## Goal

Audit the remaining likely Godot-side causes of the surviving GDGS repro after the confirmed push-constant misuse is removed, and identify the next highest-signal engine-side investigation targets.

---

## Overview

We now have a cleaner repro than before. The confirmed GDGS radix push-constant misuse is fixed on the test branch, and QA proved that the 4.7-dev5 validation errors disappear completely. But the later failure still survives unchanged: valid compositor textures exist, `No Present` still crashes, and the run still ends in `BLIT_PASS` / Vulkan device loss.

That means the remaining problem is narrower and more credible as a later engine/backend/resource-state issue than it was earlier. But this audit also needed to stay honest about what is still unsupported or unproven on the GDGS side. In particular, `No Present` removes the final GDGS writeback/presentation path, but it does **not** remove the local `RenderingDevice` workload executed from inside the compositor callback. That surviving shared path is now the most important boundary.

This slice stayed source-level and audit-only. No engine patching was attempted. Instead, the audit re-checked the cleaned repro artifacts, walked the current Godot source anchors for compositor callback insertion and `BLIT_PASS`, and wrote a durable ranking note in this repo with recommended instrumentation/experiment order.

---

## REFERENCES

| ID | Description | Path |
| --- | --- | --- |
| `REF-01` | Nightly repro note with surviving crash | `/home/derrick/.openclaw/workspace/projects/openclaw-godot/docs/gdgs-godot-47-dev5-nightly-repro-2026-05-16.md` |
| `REF-02` | Godot source-debug lane memo | `/home/derrick/.openclaw/workspace/projects/openclaw-godot/docs/gdgs-godot-source-debug-lane-2026-05-16.md` |
| `REF-03` | GDGS push-constant contract fix plan/results | `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/.plans/2026-05-16-gdgs-push-constant-contract-fix.md` |
| `REF-04` | GDGS push-constant contract note | `/home/derrick/.openclaw/workspace/projects/aerobeat/aerobeat-vendor-gdgs/docs/gdgs-radix-push-constant-contract.md` |
| `REF-05` | Current Godot source checkout for audit | `/home/derrick/.openclaw/workspace/projects/godot/` |
| `REF-06` | Cleaned dev5 QA artifact summary after the GDGS fix | `/home/derrick/.openclaw/workspace/.temp/gdgs-push-constant-fix-qa-2026-05-16-dev5/summary.md` |
| `REF-07` | Exact `No Present` crash log after the GDGS fix | `/home/derrick/.openclaw/workspace/.temp/gdgs-push-constant-fix-qa-2026-05-16-dev5/logs/no_present.log` |

---

## Tasks

### Task 1: Audit remaining Godot-side candidate surfaces after the GDGS fix

**Bead ID:** `oc-cya`  
**SubAgent:** `primary` (for `auditor`)  
**Role:** `auditor`  
**References:** `REF-01`, `REF-02`, `REF-03`, `REF-04`, `REF-05`, `REF-06`, `REF-07`  
**Prompt:** Audit current Godot source and the cleaned repro evidence to identify the most plausible remaining engine-side or engine-facing failure surfaces after the confirmed GDGS push-constant misuse is removed. Rank the top candidates, explain why each is still plausible, and recommend the first instrumentation or experiment targets.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-godot-post-gdgs-audit.md`
- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-post-gdgs-audit-2026-05-16.md`

**Status:** ✅ Complete

**Results:**
- Claimed bead `oc-cya` and completed the audit as an auditor-only ranking pass.
- Re-checked the cleaned artifact evidence from `REF-06` and `REF-07`, confirming the key reduced facts: the push-constant validation failures are gone, `No Present` still crashes, valid compositor textures still exist, and the later failure still surfaces at fence wait / `BLIT_PASS` / Vulkan device loss.
- Re-walked the current Godot source anchors in `REF-05`, focusing on:
  - `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp::_process_compositor_effects(...)`
  - `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` frame order around compositor effects
  - `servers/rendering/rendering_device.cpp` `BLIT_PASS` tagging and compute-list validation
  - `doc/classes/RenderingServer.xml` local `RenderingDevice` contract
- Added durable audit note `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-post-gdgs-audit-2026-05-16.md` with the ranked remaining candidate surfaces and next experiments.
- Final ranking from the audit:
  1. **Local-RD-in-compositor boundary**: GDGS local compute workload plus Godot local-device submission/synchronization while the frame continues toward present. This is the highest-signal shared surface across all crashing modes, including `No Present`.
  2. **Remaining GDGS local compute/resource misuse**: still a strong explanation because valid textures do not rule out delayed GPU fault from OOB or otherwise unsafe shader/resource behavior.
  3. **Godot backend synchronization after callback return**: still plausible engine-side candidate because the fault surfaces later at the frame boundary, not necessarily at the original cause site.
  4. **Unsupported local-RD to global-RD texture sharing** in non-`No Present` modes: definitely still a GDGS contract problem, but demoted because `No Present` bypasses that path and still crashes.
  5. **Post-transparent / screen/depth compositor-state handling**: now lower-confidence because `No Present` removes the final GDGS composite path that originally made this lane look stronger.
- Ownership verdict after the audit: **mixed**, with a slight lean toward remaining GDGS-side compute/resource misuse or unsupported boundary usage rather than a clean pure-Godot bug. The top engine-side candidate is still local-RD/compositor-callback synchronization or backend integration.
- Recommended first instrumentation/experiment order from the audit:
  1. keep the compositor callback alive but progressively trivialize/disable the local RD workload
  2. add tighter Godot-side breadcrumbs around compositor callback entry/exit, local-device submit/wait, and `BLIT_PASS`
  3. add temporary provenance/assertion checks for local-RD resources crossing into global-RD paths
  4. inspect GDGS indirect-dispatch and buffer/image bounds pass by pass
- Committed the durable plan/doc updates in this repo.

---

## Final Results

**Status:** ✅ Complete

**What We Built:** A source-level audit note and updated plan documenting the strongest remaining candidate surfaces after the GDGS push-constant fix, plus a concrete next-step instrumentation order.

**Reference Check:**
- `REF-01`, `REF-06`, and `REF-07` were used to confirm the cleaned repro still crashes in `No Present` with valid textures and later `BLIT_PASS` / device loss.
- `REF-02` and `REF-05` were used to map the current Godot source anchors that still matter.
- `REF-03` and `REF-04` were used to verify that the earlier push-constant misuse was real but is no longer sufficient to explain the surviving crash.
- No deliberate deviations from the reference material.

**Commits:**
- `HEAD` - `docs: audit remaining post-GDGS repro surfaces`

**Lessons Learned:**
- Once `No Present` still crashes, any theory that depends on the final composite writeback/presentation path should drop sharply in priority.
- Godot's documented local-RD contract matters a lot for the non-`No Present` modes, but it does not by itself explain the surviving reduced repro.
- The next highest-value move is not blind engine patching; it is workload minimization and tighter instrumentation at the local-RD/compositor boundary.

---

*Completed on 2026-05-16*
