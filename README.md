# Endgame Sequence Bounds (FIDE 3-fold + 50-move)

This repository provides a **transparent, reproducible** way to regenerate the table of
maximum legal endgame sequences within `m` full moves **subject to FIDE constraints**:
- **Threefold repetition** forbidden (no third occurrence of the same position with the same side to move and the same rights).
- **50–move rule** in effect (no pawn move and no capture in the last 50 moves by each side ⇒ draw claim).

We include:
- A **reference table** (the published numbers),
- A simple, **depth‑dependent effective branching** model fitted to those points,
- A script to print the table and write a CSV for downstream analysis,
- A short derivation note describing the model and assumptions,
- CI that re-runs the script and tests on every push.

## Quick Start

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python scripts/endgame_counts.py
# -> prints the table and writes data/endgame_counts.csv
```

See `docs/derivation.md` for the derivation and model details.

## Reproducibility

```bash
python scripts/endgame_counts.py
```

This prints the reference table alongside a model reconstruction and writes
`data/endgame_counts.csv` with columns:
`full_moves, plies, reference_count, model_count, relative_error`.

## CI

A minimal GitHub Actions workflow (`.github/workflows/ci.yml`) installs dependencies, runs tests,
executes the script, and uploads the CSV artifact produced by the run.
