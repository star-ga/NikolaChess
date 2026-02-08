# NikolaChess Roadmap (Updated February 2026)

**Current Stable**: v3.21.0 (Feb 2026 – full MIND transition, GPU NNUE/SPTT hybrid)
**Next Major**: v4.0.0 (Codename: NIKOLA 2.0 – Hybrid Neural-Symbolic + Remizov enhancements)
**Repository**: github.com/star-ga/nikolachess
**Core Technology**: 100% MindLang (mindlang.dev) – GPU-native, autodiff, deterministic
**Target Release**: Q4 2026
**Elo Goal**: 4000+ on consumer GPU, 4200+ on clusters

---

## Why This Roadmap?

v4.0 shifts from pure NNUE/alpha-beta → **hybrid symbolic-neural engine**:
- Transformer policy/value net (better pattern understanding)
- GPU-batched MCTS with **Remizov-Monte-Carlo rollouts** for long-horizon stability
- Symbolic tools (Remizov ODE solvers) for fortress/draw detection & eval smoothing
- Leverages MindLang examples: `remizov_feynman.mind`, `remizov_gpu.mind`, `remizov_inverse.mind`

---

## Executive Summary

NikolaChess v4.0 represents a generational leap from the current NNUE-based architecture to a **hybrid neural-symbolic engine** combining:

- **Transformer-based evaluation** replacing NNUE for superior pattern recognition
- **GPU-batched Monte Carlo Tree Search** (MCTS) with AlphaZero-style learning
- **Remizov-Monte-Carlo integration** for stability in long-horizon calculations
- **Compile-time symbolic reasoning** for endgames, fortresses, and theoretical positions
- **Self-play training pipeline** leveraging MindLang's microsecond compile times

**Target Performance**: 4000+ Elo on standard hardware, 4200+ on compute clusters.

---

## Why MindLang?

MindLang provides unique advantages that make this architecture possible:

| Capability | Benefit for Chess Engines |
|------------|---------------------------|
| Microsecond compile times | Iterate neural architectures 100x faster than C++/Rust |
| Compile-time autodiff | Training loops with zero runtime gradient overhead |
| Bit-level determinism | Reproducible games across all hardware platforms |
| Native GPU codegen | CUDA, ROCm, Metal, WebGPU from single source |
| Unified scalar/tensor | Bitboard search + neural eval in one language |

---

## Version History & Migration Path

```
v1.x (2024)     Classical alpha-beta, handcrafted eval
     │
v2.x (2025)     NNUE integration, basic GPU acceleration
     │
v3.x (Current)  Full NNUE, multi-backend GPU, 3850+ Elo
     │
v4.0 (2026)     Transformer + MCTS + Remizov hybrid, 4000+ Elo target
```

**Backward Compatibility**: v4.0 will maintain UCI protocol compatibility. Existing GUIs, tournament software, and analysis tools will work without modification.

---

## Phase 0: v3.21.x Maintenance (Now – Q1 2026)

Continue supporting the current stable branch while v4.0 development proceeds.

- Bug fixes from tournaments
- NNUE auto-detection & weight updates
- Windows installer & WebGPU stabilization
- SPTT hybrid search docs & benchmarks

**Deliverable**: Stable v3.21.1–v3.22.0 releases

### v3.21.1 (February 2026)
- [ ] Bug fixes from tournament feedback
- [ ] Minor NNUE weight updates
- [ ] Build system improvements

### v3.22.0 (March 2026)
- [ ] Final NNUE architecture optimizations
- [ ] Improved time management heuristics
- [ ] WebGPU backend stabilization

**Note**: v3.x receives security and critical fixes through Q2 2027.

---

## Phase 1: Remizov-Monte-Carlo Integration (Q1–Q2 2026) – Quick Win

**Goal**: Integrate Remizov-Monte-Carlo into MCTS for lower variance in long-horizon evaluations and stronger fortress/draw handling.

### Key Additions
1. **Remizov-MC Rollouts** (high priority)
   - Use `remizov_feynman.mind` / `remizov_gpu.mind` for short trajectories (n=30–100)
   - Model eval features (material delta, king safety, center control) as 2nd-order ODE
   - Replace 20–50% random rollouts → better endgame/draw stability

2. **Remizov Smoother in Evaluation**
   - Hybrid: `score = nn_output + α * remizov_smooth(features, t=5–10 pseudo-moves)`
   - GPU-batched via `remizov_gpu.mind`

3. **Fortress/Draw Detector**
   - Remizov limit → stability score (low variance across trajectories → fortress flag)
   - Dynamic contempt & draw adjudication

**Milestones**:
- Feature flag `--remizov-rollout` in MCTS

---

## Phase 1.5: Core Infrastructure (Q1 2026)

**Objective**: Refactor the engine core to support hybrid evaluation and MCTS.

### 1.1 Board Representation Overhaul

| Component | Current (v3.x) | Target (v4.0) |
|-----------|----------------|---------------|
| Bitboards | Partial MindLang | 100% pure MindLang |
| Move generation | Mixed C/Mind | Fully vectorized Mind |
| Position hashing | Zobrist 64-bit | Zobrist 128-bit + neural hash |
| State encoding | NNUE input | 119-plane tensor |

**Deliverable**: `board.mind` rewrite with 2x move generation throughput.

### 1.2 Multi-Backend Tensor Runtime

Ensure consistent tensor operations across all targets (CUDA, ROCm, Metal, WebGPU).

**Benchmarks Required**:
- Matrix multiply throughput (GFLOPS)
- Batch inference latency (positions/sec)
- Memory bandwidth utilization

### 1.3 Search Architecture Preparation

Introduce modular search interface supporting both paradigms (AlphaBeta, MCTS, Hybrid).

**Milestone**: Both search strategies playable via UCI by end of Q1.

---

## Phase 2: Transformer Evaluator (Q2 2026)

**Goal**: Introduce transformer-based evaluator for better positional/pattern recognition.

### 2.1 Architecture Specification

**Input Encoding** (119 planes, 8x8 each) & **Network Architecture** (12–16 transformer blocks).

- Input: 119 planes (pieces, attacks, history, repetition)
- 12–16 transformer blocks (dim 512–1024, 8–16 heads)
- Policy head: 1858 logits (masked legal moves)
- Value head: scalar [-1, +1]

**Fallback**: NNUE/hybrid mode for CPU/low-latency play.

**Milestones**:
- Standalone transformer eval ≥3500 Elo
- Hybrid NNUE+Transformer testing
- Complete: end Q3

---

## Phase 3: MCTS Implementation (Q2–Q3 2026)

**Goal**: High-throughput GPU MCTS (1M+ nps on consumer GPUs).

### 3.1 Core MCTS Algorithm & GPU Batching
- PUCT + Dirichlet noise + virtual loss
- Batched inference (512–4096 positions)
- Tree memory: 50M+ nodes (8GB budget)

### 3.2 Remizov Synergy
- Use Remizov-guided priors in selection (stability score boosts drawish/exploratory lines)

**Milestone**: Tournament-ready MCTS by end of Q3.

---

## Phase 4: Self-Play & Training Loop (Q3 2026)

**Goal**: Full self-play training pipeline closing the AlphaZero-style loop.

### 4.1 Training Pipeline
- Generate 500+ positions/game (temperature schedule, Dirichlet)
- Replay buffer: 2M+ augmented samples
- **Remizov Regularizer**: Train transformer with Remizov regularizer (via `remizov_inverse.mind` for interpretable coefficients)

### 4.2 Training Loop & Evaluation
- MindLang compile-time autodiff
- Automated tournament system for checkpoint evaluation

**Milestone**: v4.0 release candidate – 4000+ Elo self-play.

---

## Phase 5: Symbolic Integration (Q4 2026)

**Objective**: Enhance neural evaluation with compile-time symbolic knowledge (Tablebases, Fortress Detection).

---

## Stretch Goals (Q4 2026+)
- Inverse Remizov for eval learning (recover ODE coeffs from games)
- Remizov in opening prep (continuous-time policy smoothing)
- 8-man tablebase hints via symbolic Remizov verification

---

## Phase 6: Deployment & Ecosystem (Q4 2026+)

### 6.1 Platform Targets
Linux, Windows, macOS, WebAssembly, Android, iOS.

### 6.2 Deployment Modes
Standalone, Server, Cluster, Embedded.

---

## Performance Targets & Risk Assessment

(Maintained from previous roadmap)

---

## Contributing

We welcome contributions across all phases, especially in:
1. **Core Development**: MindLang proficiency required
2. **Remizov Integration**: Symbolic math & physics-inspired ML
3. **Training Infrastructure**: Distributed systems experience

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## Timeline Summary

| Phase | Dates | Key Deliverable |
|-------|-------|-----------------|
| 0 | Now – Q1 2026 | v3.21.x maintenance |
| 1 | Q1–Q2 2026 | Remizov-Monte-Carlo Integration |
| 1.5 | Q1 2026 | Infrastructure refactor |
| 2 | Q2 2026 | Transformer evaluator |
| 3 | Q2–Q3 2026 | GPU-batched MCTS |
| 4 | Q3 2026 | Self-play training |
| 5 | Q4 2026 | Symbolic integration |
| 6 | Q4 2026+ | v4.0.0 release, ecosystem |

---

## References

- [AlphaZero Paper](https://arxiv.org/abs/1712.01815)
- [Leela Chess Zero](https://lczero.org/)
- [MindLang Documentation](https://mindlang.dev/docs)
- [Remizov ODE Examples](https://mindlang.dev/examples/remizov)

---

*© 2026 STARGA Inc. All rights reserved.*