# GDGS first-L88 preserve-merge co-condition note (2026-05-26)

See also:

- `/home/derrick/.openclaw/workspace/projects/godot/doc/gdgs-first-l88-preserve-blend-merge-condition-2026-05-26.md`

This companion note records the same Task 189 conclusion in the plan-linked filename expected by the active plan.

## Conclusion

For the exact preserved `Command Graph (L88)` crash path:

- **source-alpha color weighting** remains the tightest active carrier *inside* the exact `BLEND_MODE_MIX` merge
- but **preserve/load dependency** must return as an independently live co-condition for the stronger preserved-path framing, because the destination-retention half of that exact merge only exists if prior root contents are still live under `attachment_load_ops=[0:LOAD]`
- **alpha writeback** remains a real downstream effect, but not the tighter co-condition distinguishing the preserved-content path

Best narrow wording:

> the preserved `L88` crash path requires the source-alpha-driven `BLEND_MODE_MIX` color merge over loaded preserved destination/root contents.
