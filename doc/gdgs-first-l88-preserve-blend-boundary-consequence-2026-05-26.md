# GDGS first-L88 preserve-content blend-boundary consequence note (2026-05-26)

## Goal

Continue one rung outward from Task 187 without widening into a fix.

The narrow question here is:

- at the exact `Command Graph (L88)` preserve-content blend boundary,
- what is the **tightest downstream structural consequence** of the already-reduced packet-local carrier `d`,
- and is that consequence best located at:
  - source-alpha color weighting,
  - alpha writeback,
  - or the preserve/load dependency itself?

This note stays documentation-only and source-backed.

## Scope lock

This slice stays on the same already-approved preserved crash path:

- exact packet: the first clipped preserve-rect `Command Graph (L88)` no-outline MSDF packet
- exact preserved outer identity: `submit_serial=9`, `fence_wait_error submit_serial=9`, `Tonemap (L87)`, `Command Graph (L88)`, later `BLIT_PASS`, exit `-6`
- exact widened boundary inherited from Task 187: the live `L88` preserve-content canvas mix-blend merge against already-loaded root contents

It does **not** reopen specialization-cache theory, request-hash theory, CPU-side vertex-format-cache theory, or widen into a speculative fix.

## Starting point from Task 187

Task 187 already established the first honest outward carry:

```text
d
-> a (= d on this exact packet)
-> color.a = d * inherited_color.a
-> frag_color.a
-> preserve-content L88 blend boundary
```

It also established that the first structural difference outside the packet appears only when the exported packet result meets already-present root contents under the live `UI_PASS` preserve/load contract.

So Task 188 does **not** reopen whether the boundary exists. It asks what the narrowest structural consequence is **inside that exact merge**.

## Source-backed live merge contract

The current source-backed `L88` canvas mix recipe in `servers/rendering/renderer_rd/storage_rd/material_storage.cpp` is:

```cpp
case BLEND_MODE_MIX: {
	attachment.enable_blend = true;
	attachment.alpha_blend_op = RD::BLEND_OP_ADD;
	attachment.color_blend_op = RD::BLEND_OP_ADD;
	attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
	attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
	attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
} break;
```

And the durable `REF-07` evidence already keeps the exact lane pinned as preserve-content UI with blend enabled:

- `owner_label="Command Graph (L88) (Draw)"`
- `breadcrumb=UI_PASS`
- `attachment_load_ops=[0:LOAD]`
- `first_blend_mode="mix"`
- `first_destination_color_blend_mode="mix"`
- `blend_enabled_attachment_mask="0x1"`

So the exact attachment-level merge for the first packet is:

```text
color_out = src.rgb * src.a + dst.rgb * (1 - src.a)
alpha_out = src.a * 1 + dst.a * (1 - src.a)
```

For this packet, the Task 187 / Task 186 chain means the live branch-owned carry entering that merge is the exported source alpha descended from `d`.

## Candidate 1: preserve/load dependency

The preserve/load dependency is real, but it is **not** the tightest downstream structural consequence.

Why it remains broader:

- `attachment_load_ops=[0:LOAD]` is what keeps a destination participant available at all
- without preserved destination contents, there would be no `dst` term for the source packet to mix against
- so preserve/load is the **attachment-level prerequisite/context** for this exact merge

But it is not the narrowest carrier of the already-reduced packet-local seam, because preserve/load does not describe *how* the exported source alpha descended from `d` actually enters the merge math. It only says the merge has a live destination side to depend on.

So preserve/load stays as the exact external dependency boundary inherited from Task 187, but it is too coarse to be the tightest consequence *within* that boundary.

## Candidate 2: alpha writeback

Alpha writeback is also real, but it is still not the tightest consequence.

Why it is narrower than preserve/load yet still not the best location:

- `src_alpha_blend_factor = ONE`
- `dst_alpha_blend_factor = ONE_MINUS_SRC_ALPHA`
- therefore the packet's exported source alpha does contribute to the stored post-merge alpha channel

So yes, the branch-owned carry descended from `d` does survive into `alpha_out`.

But alpha writeback is still one step too downstream / generic for the tightest classification here:

- it describes the resulting attachment alpha after the merge is already being performed
- it does not isolate the first structural role of the source alpha at the merge boundary
- it is also partly a later stored consequence of the same source-alpha participation that is already active in the blend equations themselves

So alpha writeback remains a real downstream effect, but not the tightest seam.

## Candidate 3: source-alpha color weighting

This is the tightest downstream structural consequence at the exact merge.

Why:

1. Task 187 already reduced the packet-owned surviving carrier to exported source alpha descended from `d`.
2. At the exact `BLEND_MODE_MIX` boundary, that alpha first becomes an active coefficient in the merge equations.
3. The color-side contract uses the source alpha twice at once:
   - `src.rgb * src.a`
   - `dst.rgb * (1 - src.a)`
4. That means the same exported alpha descended from `d` simultaneously:
   - weights how much new packet color enters, and
   - weights how much preserved destination/root color remains

This is tighter than alpha writeback because it captures the **first active merge role** of the packet's carried alpha at the structural boundary itself, rather than a later stored channel result.

This is tighter than preserve/load dependency because it explains the exact role played by the packet-owned carrier once the destination side is present.

So the best narrow classification is:

- preserve/load dependency = exact boundary context / prerequisite
- alpha writeback = real downstream consequence, but broader/later than necessary
- **source-alpha color weighting = tightest downstream structural consequence at the exact attachment-level merge**

## Why this is the best stop point for Task 188

This classification keeps the widened seam honest without speculating past the evidence.

It does **not** claim:

- that color weighting is proven to be the root cause
- that alpha writeback is irrelevant
- that preserve/load can be ignored
- that the right fix is to disable preserve, alter mix mode, or rewrite alpha

It only says that if the investigation continues from Task 187's exact blend boundary, the narrowest next seam should be phrased around the **source-alpha-driven color merge over preserved destination contents**, not around alpha writeback alone and not around the broader preserve/load prerequisite alone.

## Conclusion

At the exact `Command Graph (L88)` preserve-content blend boundary, the tightest downstream structural consequence of the first packet's already-reduced carrier `d` is **source-alpha color weighting**.

More precisely:

- the preserve/load dependency is the exact external prerequisite that keeps destination contents alive
- alpha writeback is a real post-merge consequence
- but the narrowest structural seam is the moment the exported source alpha descended from `d` becomes the coefficient that mixes packet color with preserved destination/root color under `BLEND_MODE_MIX`

So the next honest downstream wording should be:

> the live seam is the first-L88 preserve-content `BLEND_MODE_MIX` source-alpha color merge over loaded root contents, with alpha writeback and preserve/load retained as broader consequences/context rather than the tightest carrier.
