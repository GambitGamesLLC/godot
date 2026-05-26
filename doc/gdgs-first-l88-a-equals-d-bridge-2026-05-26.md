# GDGS first-L88 packet-local `a = d` bridge note (2026-05-26)

## Goal

Continue exactly from Task 195 without widening scope.

The narrow question here is:

- after Task 195 reduced the surviving packet-local `color.a` split to the tighter local multiplier `a`,
- what is the narrowest honest bridge from that `a` back into the already-established packet-local `d` ladder,
- and does restating the exact `a = d` alias on this packet fully close the local split without widening into broader theories or later aliases?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary context: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 195

Task 195 already split the surviving packet-local/shared-fragment alpha carrier one rung deeper:

- inherited pre-MSDF packet alpha was demoted as the broader operand
- the tighter surviving local side was the packet-local multiplier `a`
- the same note also kept earlier packet-local reductions fixed: on this exact packet, `px_size = 1.0`

The relevant live no-outline source remains:

```glsl
float px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0);
float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

So this task only asks whether the exact surviving local multiplier `a` still has a meaningful split from the already-established packet-local `d` carrier on the exact first-L88 packet.

## Exact packet-local bridge from `a` back to `d`

Task 181 already fixed the exact packet fact that this first-L88 lane runs with:

- `px_size = 1.0`

Substituting only that already-established packet-local fact into the exact live no-outline relation gives:

```glsl
a = clamp((d - 0.5) * 1.0 + 0.5, 0.0, 1.0)
  = clamp(d, 0.0, 1.0)
```

And on this exact packet's already-established normalized sampled-channel lane, that is the same surviving relation earlier tasks recorded as:

```glsl
a = d
```

So the narrowest honest bridge from the Task 195 multiplier `a` back into the packet-local ladder is not a new intermediate carrier. It is the already-known exact alias:

- `a = d`

## Does this close the local split?

Yes — on this exact packet, it closes the split cleanly.

Why:

1. **Task 195 already isolated `a` as the tighter side of the `color.a` product.**
   - inherited pre-MSDF alpha stayed broader
   - `a` was the last genuinely packet-local branch-owned side of that split

2. **The only remaining question is whether `a` differs meaningfully from the prior packet-local `d` carrier.**
   - on this exact packet, it does not
   - the already-fixed `px_size = 1.0` fact collapses the no-outline formula directly back to `a = d`

3. **That means no further honest local fork survives between `a` and `d`.**
   - there is no narrower packet-local bridge term between them on this lane
   - there is also no need to widen outward into export/writeback, blend-consumer, or speculative fix framing just to connect them

So the local split introduced in Task 195 is now fully closed on exact packet-local facts alone.

## Exact classification

For the exact preserved `Command Graph (L88)` no-outline MSDF packet:

- Task 195's tighter surviving local side was **packet-local multiplier `a`**
- on this same packet, the narrowest honest bridge from `a` back into the prior packet-local ladder is the exact alias **`a = d`**
- because that alias is exact on the already-fixed packet facts, it **does close the local split** without requiring any broader framing
- so there is no remaining packet-local distinction to preserve between `a` and `d` on this lane

## Conclusion

At this exact stop point on the preserved first-L88 packet-local ladder, the Task 195 multiplier seam does not create a new surviving carrier beyond what was already known.

The honest bridge is simply:

- **`a = d`**

And on this packet that is enough to close the split completely:

- `a` was the tightest side of the `color.a` product split
- `a` immediately aliases back to the already-established packet-local scalar `d`
- so the packet-local ladder is rejoined without widening into later export/writeback aliases, blend-consumer framing, or speculative fixes

Best narrow wording after this slice:

> on the exact preserved first-L88 no-outline packet, the Task 195 local multiplier `a` does not remain distinct from the prior packet-local carrier; the exact bridge is `a = d`, which cleanly rejoins and closes the local split on packet-local facts alone.

The next seam, if the plan wants to continue from here, is no longer inside a surviving `a` versus `d` packet-local split. Any further movement would need to decide whether to step outward from the rejoined packet-local `d` ladder into the first downstream non-packet-local consequence, or stop here as the completed local rejoin.
