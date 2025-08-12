# Derivation Notes: Endgame Sequence Upper Bounds

We estimate the maximum number of legal endgame sequences within `m` full moves **subject to FIDE constraints**:
- **Threefold repetition**: a position (same side to move, same castling/en-passant rights) may not occur a third time.
- **50–move rule**: if no pawn move and no capture occurs in the last 50 moves by each side, either side may claim a draw.

This is not the naive branching like ~35^plies. Repetition-avoidance and the 50–move window reduce the **effective branching** as depth grows.

## Model

Let `g(i)` be the **effective branching per ply** at depth `i` once we enforce:
1. Avoiding a third occurrence of any previous position,
2. Resetting the 50–move counter via a capture or pawn move sufficiently often.

We use a smooth logistic decay:
```
g(i) = L2 + (L1 – L2) / (1 + exp(k * (i – i0)))
```
- `L1`: initial per‑ply branching (shallow depth),
- `L2`: asymptotic per‑ply branching (deep depth, still > 1),
- `i0`: midpoint ply,
- `k`: steepness of the decay.

Total sequences for `N = 2m` plies:
```
M(N) = Π_{i=1..N} g(i)
```
This captures that early plies have many safe options, but deeper plies are increasingly constrained by repetition and required 50–move resets.

## Fitting to the Table

We fit `(L1, L2, i0, k)` to the **published reference counts** (see `scripts/endgame_counts.py`) by minimizing squared error in **log space**. The script emits a CSV:
`data/endgame_counts.csv` with
`full_moves, plies, reference_count, model_count, relative_error`.

This model is intentionally simple and transparent. If we later implement an exact enumerator
(e.g., legal move generation + state hashing including castling/en‑passant + 50–move counter + repetition map), we can swap it in but keep the same CSV interface for downstream figures.
