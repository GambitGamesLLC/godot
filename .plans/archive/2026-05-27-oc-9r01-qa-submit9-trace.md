# GDGS Submit9 QA Trace

**Date:** 2026-05-27  
**Status:** Draft  
**Last Updated:** 2026-05-27 00:00 ET  
**Blocked Reason:** None  
**Agent:** chip

---

## Goal

Verify the submit9 error surface trace ordering for the three-rung projection ladder and capture a fresh artifact root for comparison.

---

## Overview

This QA slice reruns the already-approved three-rung ladder with the refreshed source-built Godot editor and GODOT_GDGS_DEBUG_SUBMIT9_ERROR_SURFACE_WINDOW=1 enabled. The focus is the submit9_error_surface_trace device_lost_edge ordering: determine whether wait_device_lost and status_device_lost flip true on the same final poll_ordinal or whether one flips first with the other lagging by an adjacent same-iteration observation.

The work stays inside the backend/barrier seam defined by the master plan and does not reopen shader ancestry or widen scope. Outputs include a dated artifact root under /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-27/ and an explicit comparison recorded back into the master plan.

---

## REFERENCES

| ID | Description | Path |
| --- | --- | --- |
| `REF-01` | GDGS master plan seam and constraints | `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-gdgs-gaussian-splat-godot-vendor-plugin-bug-hunt-master-plan.md` |

---

## Tasks

### Task 1: Re-run submit9 QA ladder

**Bead ID:** `oc-9r01`  
**SubAgent:** `primary` (for `qa` workflow role)  
**Role:** `qa`  
**References:** `REF-01`  
**Prompt:** Run the three-rung ladder (projection_non_footprint_immediate_return_only, projection_post_barrier_no_scratch_immediate_return_only, projection_post_barrier_immediate_return_only) using /home/derrick/.openclaw/workspace/projects/godot/bin/godot.linuxbsd.editor.dev.x86_64 with GODOT_GDGS_DEBUG_SUBMIT9_ERROR_SURFACE_WINDOW=1, capture a fresh artifact root under /home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-27/ (or current dated sibling), compare submit9_error_surface_trace device_lost_edge ordering for wait_device_lost and status_device_lost, update master plan with results, and close bead oc-9r01 when complete with the next seam explicit.

**Folders Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/.temp/gdgs-stage-repro-2026-05-27/`

**Files Created/Deleted/Modified:**
- `/home/derrick/.openclaw/workspace/projects/godot/.plans/2026-05-16-gdgs-gaussian-splat-godot-vendor-plugin-bug-hunt-master-plan.md`

**Status:** ⏳ Pending

**Results:** Pending.

---

## Final Results

**Status:** ⚠️ Partial

**What We Built:** Pending QA rerun and trace comparison.

**Reference Check:** Pending `REF-01`.

**Commits:**
- Pending

**Lessons Learned:** Pending.

---

*Completed on 2026-05-27*
