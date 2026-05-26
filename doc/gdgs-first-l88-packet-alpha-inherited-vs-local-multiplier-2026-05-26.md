# GDGS first-L88 packet-local `color.a`: inherited pre-MSDF alpha versus packet-local multiplier `a` note (2026-05-26)

## Goal

Continue exactly from Task 194 without widening scope.

The narrow question here is:

- after Task 194 reduced the surviving exact `L88` source-alpha carrier to the packet-local/shared-fragment `color.a`,
- is the tighter remaining seam inside that carrier the **inherited pre-MSDF packet alpha** already present on `color.a`,
- or the **packet-local multiplier `a`** applied by the exact no-outline MSDF lane?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact boundary context: the live preserved `Command Graph (L88)` `BLEND_MODE_MIX` merge under `UI_PASS`
- exact outer identity kept only as context: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 194

Task 194 already reduced the surviving exported source-alpha carrier one rung deeper:

- the later export/writeback alias `frag_color.a` was demoted
- the tighter surviving carrier was the packet-local/shared-fragment alpha `color.a`
- the exact live no-outline relation remained:

```glsl
vec4 color = color_interp;
...
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
frag_color = color;
```

So this task only asks how to split the surviving `color.a` carrier itself one rung deeper.

## Exact packet-local composition inside `color.a`

On the exact no-outline MSDF lane in `servers/rendering/renderer_rd/shaders/canvas.glsl`, the surviving `color.a` carrier is formed by multiplying two ingredients that do not have the same scope:

```glsl
vec4 color = color_interp;
...
float a = clamp((d - 0.5) * px_size + 0.5, 0.0, 1.0);
color.a = a * color.a;
```

That means the live packet-local/shared-fragment alpha at export time is composed from:

1. **Inherited pre-MSDF packet alpha**
   - this is the incoming `color.a` already present when the fragment lane starts from `vec4 color = color_interp;`
   - it exists before the MSDF-specific branch-local rewrite
   - it is shared upstream fragment state, not created by the exact no-outline MSDF lane

2. **Packet-local multiplier `a`**
   - this is created only inside the no-outline MSDF lane
   - it is the branch-local scalar applied to the inherited alpha carrier
   - on the exact packet facts already established earlier, `px_size = 1.0`, so the no-outline relation reduces to `a = d`

So the surviving `color.a` carrier is not primitive. It is the product of one inherited upstream carrier and one packet-local branch-owned multiplier.

## Which side is tighter?

### Inherited pre-MSDF packet alpha

This is broader.

Why:

- it already exists before the exact MSDF packet-specific rewrite
- it is not unique to the no-outline MSDF lane
- it only becomes part of the surviving exact seam because the packet-local multiplier later consumes it

So inherited pre-MSDF `color.a` is a real operand, but not the tighter preserved seam.

### Packet-local multiplier `a`

This is tighter.

Why:

- it is produced inside the exact no-outline MSDF packet rather than inherited from shared upstream fragment state
- it is the branch-local ingredient that distinguishes the surviving packet-local rewrite from the preexisting alpha carrier
- the later `color.a = a * color.a` result is broader because it mixes this packet-local multiplier with the inherited upstream alpha lane

On the already-fixed exact packet facts, `a` also immediately collapses to the same value as `d` because `px_size = 1.0`. That does **not** make inherited alpha tighter; it only means the narrower side of this split reconnects to the already-established packet-local `d` ladder rather than creating a new broader carrier.

## Exact classification

For the exact preserved `Command Graph (L88)` crash path:

- the surviving packet-local/shared-fragment alpha carrier `color.a` splits one rung deeper into:
  - **inherited pre-MSDF packet alpha**
  - **packet-local multiplier `a`**
- the tighter preserved seam is **the packet-local multiplier `a`**
- the inherited pre-MSDF packet alpha remains a broader upstream operand already present before this MSDF-specific rewrite
- the full rewritten `color.a` remains broader than `a` because it is the composite product of local multiplier plus inherited carrier

## Conclusion

At the exact preserved `L88` source-alpha admission seam, the surviving packet-local `color.a` carrier splits one rung deeper by exact packet-local facts into inherited upstream alpha versus the local multiplier applied by the no-outline MSDF lane.

The tighter surviving side is:

- **packet-local multiplier:** `a`

while the broader side is:

- **inherited pre-MSDF packet alpha:** the incoming `color.a` loaded from `color_interp`

Best narrow wording after this slice:

> inside the preserved exact `L88` packet-local `color.a` carrier, the tighter surviving seam is the packet-local MSDF multiplier `a`; the inherited pre-MSDF alpha is only the broader upstream operand that `a` rewrites.

On this exact packet, `a` immediately reduces to `d` because `px_size = 1.0`, so any further honest narrowing from here rejoins the already-established packet-local `d` classification ladder rather than reopening later export/writeback or blend-consumer aliases.
