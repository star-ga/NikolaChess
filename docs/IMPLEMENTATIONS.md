# NikolaChess - Implementation Index

## Summary

All advanced search and evaluation improvements for NikolaChess.

| Feature | File | Status |
|---------|------|--------|
| History-modulated LMR | `src/search/search_improvements.mind` | ✅ Implemented |
| ProbCut Pruning | `src/search/search_improvements.mind` | ✅ Implemented |
| Killer Moves | `src/search/search_improvements.mind` | ✅ Implemented |
| Countermove Heuristic | `src/search/search_improvements.mind` | ✅ Implemented |
| Fortress Detection | `src/eval/eval_improvements.mind` | ✅ Implemented |
| Tapered Phase Eval | `src/eval/eval_improvements.mind` | ✅ Implemented |
| Dynamic Contempt | `src/eval/eval_improvements.mind` | ✅ Implemented |
| GPU-Batched NNUE | `src/gpu/batched_nnue.mind` | ✅ Implemented |
| GPU MCTS | `src/search/mcts.mind` | ✅ Implemented |
| Hybrid Search (SPTT) | `src/search/hybrid.mind` | ✅ Implemented |
| Benchmark Framework | `src/bench/framework.mind` | ✅ Implemented |
| Benchmark Runner | `src/bench/runner.mind` | ✅ Implemented |

---

## Implementation Details

### 1. Search Improvements (`src/search/search_improvements.mind`)

**History-Modulated LMR**
- Modulates Late Move Reductions by historical move success
- `history_bonus = history[side][from][to] / 16384.0`
- Reduces reduction for historically good moves

**ProbCut Pruning**
- Uses shallow search to verify likely cutoffs
- Margin: ~200 centipawns (2 pawns)
- Minimum depth: 5 ply

**Move Ordering**
- TT move: 10,000,000
- Winning captures (SEE ≥ 0): 1,500,000 + MVV-LVA
- Promotions: 900,000 + piece value
- Killer moves: 800,000 / 700,000
- Countermoves: 600,000
- History heuristic: ±16,384

---

### 2. Evaluation Improvements (`src/eval/eval_improvements.mind`)

**Fortress Detection**
- Detects KBN vs K, blocked pawn chains
- Prevents overvaluation of material advantage in drawn positions

**Tapered Phase Evaluation**
```
phase = total(piece_material) / max_material * 256
eval = (midgame * phase + endgame * (256 - phase)) / 256
```

**Dynamic Contempt**
- Adjusts contempt based on:
  - Opponent strength (ELO-based)
  - Position imbalance
  - Game phase

---

### 3. GPU-Batched NNUE (`src/gpu/batched_nnue.mind`)

**Architecture**
- HalfKA: 45056 → 1024 → 8 → 32 → 1
- Batches evaluations across Lazy SMP threads
- Single GPU kernel for 64-4096 positions

**Key Components**
- `BatchedEvaluator`: Queues positions, processes in batches
- `Accumulator`: Incremental HalfKA feature updates
- GPU annotations: `on(gpu0)`, `on(gpu0..gpu7)`

---

### 4. GPU MCTS (`src/search/mcts.mind`)

**PUCT Formula**
```
UCB = Q(s,a) + c_puct * P(s,a) * sqrt(N(s)) / (1 + N(s,a))
```

**Components**
- Policy+Value network (ResNet backbone)
- Batched leaf evaluation on GPU
- Virtual loss for parallel search
- Dirichlet noise for exploration

---

### 5. Hybrid Search (`src/search/hybrid.mind`)

**SPTT - Superparallel Tree Traversal**
- Position analyzer determines algorithm choice
- MCTS for high-branching, complex positions
- Alpha-beta for tactical, endgame positions
- Optional verification of MCTS results with shallow alpha-beta

**Decision Criteria**
- Branching factor > 35 → MCTS
- Complexity > 0.7 → MCTS
- Endgame → Alpha-beta
- Tactical position → Alpha-beta

---

### 6. Benchmark Framework (`src/bench/`)

**SPRT Testing**
- H0: No improvement (Elo0 = 0)
- H1: Improvement exists (Elo1 = 5 or 20)
- α = β = 0.05 (5% error rates)

**Test Suite**
1. History-LMR vs baseline
2. ProbCut vs baseline
3. GPU batch (64) vs CPU
4. MCTS vs alpha-beta
5. Multi-GPU scaling (8 vs 1)
6. All improvements combined

**30 Benchmark Positions**
- Openings (4 positions)
- Middlegame tactical (8 positions)
- Complex middlegame (4 positions)
- Tactical puzzles (3 positions)
- Basic endgames (5 positions)
- Rook endgames (3 positions)
- Pawn endgames (2 positions)
- Complex endgames (3 positions)

---

## Expected Total Gains

| Component | Elo Gain | Cumulative |
|-----------|----------|------------|
| History-LMR | +15-30 | +15-30 |
| ProbCut | +10-20 | +25-50 |
| Eval improvements | +20-40 | +45-90 |
| GPU-Batched NNUE | +50-100 | +95-190 |
| MCTS/Hybrid | +150-300 | +245-490 |

**Conservative estimate: +250 Elo**
**Optimistic estimate: +500 Elo**

With multi-GPU scaling (8+ GPUs): **+300-600 Elo**

---

## File Structure

```
src/
├── search/
│   ├── search_improvements.mind  # History-LMR, ProbCut, move ordering
│   ├── mcts.mind                 # GPU MCTS implementation
│   └── hybrid.mind               # Hybrid search (SPTT)
├── eval/
│   └── eval_improvements.mind    # Fortress, tapered, contempt
├── gpu/
│   └── batched_nnue.mind         # GPU-batched NNUE evaluation
├── bench/
│   ├── framework.mind            # SPRT, A/B testing framework
│   └── runner.mind               # Benchmark test runner
└── api/
    └── uci_protocol.mind         # UCI protocol implementation
```

---

## Next Steps

1. **Run benchmarks** to validate each improvement
2. **Train policy+value network** for MCTS
3. **Optimize GPU kernels** for specific hardware
4. **Multi-GPU testing** on cluster
5. **Tournament testing** against top engines

---

*Copyright (c) 2026 STARGA, Inc. All rights reserved.*
