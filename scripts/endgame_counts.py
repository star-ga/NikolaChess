#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Endgame sequence upper-bound estimator under FIDE 3-fold repetition and 50–move constraints.

This script serves two roles:
1) It prints the exact reference table currently published.
2) It shows a transparent *model-based reconstruction* using a depth-dependent effective branching
   factor per ply that decays with depth (to reflect repetition avoidance + 50–move resets).
   The model is fitted to the reference table using a coarse grid search (NumPy only).

Outputs:
- data/endgame_counts.csv  (columns: full_moves, plies, reference_count, model_count, relative_error)
"""

import argparse
import math
import os
from typing import Dict, List, Tuple

import numpy as np

# ---------------------------
# Reference table (published numbers)
# full moves m -> maximum number (as float)
# ---------------------------
REFERENCE_COUNTS = {
     5: 3_775_852_872.0,                  # 3.775852872e9
    10: 7.30398216506453e16,
    15: 1.51577893358687e23,
    20: 7.69827064587585e28,
    25: 1.42404361906059e34,
    30: 1.21138616306393e39,
    35: 5.51777898897728e43,
    40: 1.49693269286536e48,
    45: 2.61553428261307e52,
    50: 3.12407154730694e56,
}

def plies(m_full_moves: int) -> int:
    """Full moves m → plies N = 2m."""
    return 2 * m_full_moves

# ---------------------------
# Model
# ---------------------------
# Depth‑dependent effective branching per ply g(i) that decays with depth i:
# g(i) = L2 + (L1 - L2) / (1 + exp(k * (i - i0)))
# with L1 > L2 > 1, i0 as midpoint, k as steepness.
#
# Total modeled count for N plies:
#   M(N) = prod_{i=1..N} g(i)
# We fit (L1, L2, i0, k) to the reference counts in log space.

def logistic_g(i: int, L1: float, L2: float, i0: float, k: float) -> float:
    return L2 + (L1 - L2) / (1.0 + math.exp(k * (i - i0)))

def modeled_count(N: int, L1: float, L2: float, i0: float, k: float) -> float:
    s = 0.0
    for i in range(1, N + 1):
        gi = logistic_g(i, L1, L2, i0, k)
        if gi <= 1.0:
            gi = 1.0000001  # enforce >1 to avoid collapse
        s += math.log(gi)
    return math.exp(s)

def fit_parameters(reference: Dict[int, float]) -> Tuple[float, float, float, float]:
    # Coarse grid search (no SciPy). Adjust ranges if desired.
    L1_grid = np.linspace(8.0, 14.0, 13)     # initial branching per ply
    L2_grid = np.linspace(2.0, 5.5, 15)      # asymptotic branching per ply
    i0_grid = np.linspace(20.0, 120.0, 21)   # midpoint ply
    k_grid  = np.linspace(0.01, 0.12, 12)    # steepness

    ms = sorted(reference.keys())
    Ns = [plies(m) for m in ms]
    logs_ref = np.array([math.log(reference[m]) for m in ms], dtype=float)

    best = (float('inf'), None)
    for L1 in L1_grid:
        for L2 in L2_grid:
            if L2 >= L1:
                continue
            for i0 in i0_grid:
                for k in k_grid:
                    logs_model = []
                    ok = True
                    for N in Ns:
                        val = modeled_count(N, L1, L2, i0, k)
                        if not np.isfinite(val) or val <= 0:
                            ok = False
                            break
                        logs_model.append(math.log(val))
                    if not ok:
                        continue
                    logs_model = np.array(logs_model, dtype=float)
                    se = float(np.mean((logs_model - logs_ref)**2))
                    if se < best[0]:
                        best = (se, (L1, L2, i0, k))
    if best[1] is None:
        raise RuntimeError("Grid search failed to find parameters.")
    return best[1]

def render_table(reference: Dict[int, float], params: Tuple[float, float, float, float]):
    L1, L2, i0, k = params
    rows = []
    for m in sorted(reference.keys()):
        N = plies(m)
        ref = reference[m]
        mod = modeled_count(N, L1, L2, i0, k)
        rel_err = abs(mod - ref) / ref
        rows.append((m, N, ref, mod, rel_err))
    return rows

def save_csv(rows, out_path: str):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("full_moves,plies,reference_count,model_count,relative_error\n")
        for m, N, ref, mod, rel in rows:
            f.write(f"{m},{N},{ref:.12e},{mod:.12e},{rel:.6e}\n")

def main():
    ap = argparse.ArgumentParser(description="Recompute/fit endgame sequence counts under FIDE repetition & 50–move constraints.")
    ap.add_argument("--write-csv", default="data/endgame_counts.csv", help="Output CSV path")
    args = ap.parse_args()

    print("Fitting depth-dependent effective branching model to reference table...")
    L1, L2, i0, k = fit_parameters(REFERENCE_COUNTS)
    print(f"Best-fit parameters:")
    print(f"  L1 (initial branching): {L1:.4f}")
    print(f"  L2 (asymptotic branching): {L2:.4f}")
    print(f"  i0 (midpoint ply): {i0:.2f}")
    print(f"  k  (steepness): {k:.4f}\n")

    rows = render_table(REFERENCE_COUNTS, (L1, L2, i0, k))

    print(f"{'m':>4} {'plies':>6} {'reference':>22} {'model':>22} {'rel.err':>10}")
    for m, N, ref, mod, rel in rows:
        print(f"{m:4d} {N:6d} {ref:22.6e} {mod:22.6e} {rel:10.3e}")

    save_csv(rows, args.write_csv)
    print(f"\nWrote {args.write_csv}")

if __name__ == "__main__":
    main()
