# NikolaChess Roadmap

**Current Stable**: v3.21.0
**Next Major**: v4.0.0 (Codename: NIKOLA 2.0)
**Repository**: github.com/star-ga/nikolachess
**Core Technology**: 100% MindLang (mindlang.dev)
**Target Release**: Q4 2026

---

## Executive Summary

NikolaChess v4.0 represents a generational leap from the current NNUE-based architecture to a **hybrid neural-symbolic engine** combining:

- **Transformer-based evaluation** replacing NNUE for superior pattern recognition
- **GPU-batched Monte Carlo Tree Search** (MCTS) with AlphaZero-style learning
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
v4.0 (2026)     Transformer + MCTS hybrid, 4000+ Elo target
```

**Backward Compatibility**: v4.0 will maintain UCI protocol compatibility. Existing GUIs, tournament software, and analysis tools will work without modification.

---

## Phase 0: v3.21.x Maintenance (Now – Q1 2026)

Continue supporting the current stable branch while v4.0 development proceeds.

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

## Phase 1: Core Infrastructure (Q1 2026)

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

Ensure consistent tensor operations across all targets:

```
┌─────────────────────────────────────────────────────────┐
│                    MindLang Tensor API                  │
├─────────────┬─────────────┬─────────────┬───────────────┤
│    CUDA     │    ROCm     │    Metal    │    WebGPU     │
│  (NVIDIA)   │    (AMD)    │   (Apple)   │   (Browser)   │
├─────────────┴─────────────┴─────────────┴───────────────┤
│                      CPU Fallback                       │
│              (AVX-512 / NEON / WASM SIMD)               │
└─────────────────────────────────────────────────────────┘
```

**Benchmarks Required**:
- Matrix multiply throughput (GFLOPS)
- Batch inference latency (positions/sec)
- Memory bandwidth utilization

### 1.3 Search Architecture Preparation

Introduce modular search interface supporting both paradigms:

```mind
trait SearchStrategy {
    fn search(pos: Position, limits: SearchLimits) -> SearchResult;
    fn stop();
    fn ponder_hit();
}

// v3.x compatibility
impl SearchStrategy for AlphaBetaSearch { ... }

// v4.0 new
impl SearchStrategy for MCTSSearch { ... }

// Hybrid mode
impl SearchStrategy for HybridSearch { ... }
```

**Milestone**: Both search strategies playable via UCI by end of Q1.

---

## Phase 2: Transformer Evaluator (Q2 2026)

**Objective**: Replace NNUE with a transformer-based policy+value network.

### 2.1 Architecture Specification

**Input Encoding** (119 planes, 8x8 each):
```
Planes 0-11:    Own pieces (P, N, B, R, Q, K × 2 colors)
Planes 12-23:   Enemy pieces
Planes 24-31:   Attack maps (per piece type)
Planes 32-35:   Castling rights (4 binary)
Planes 36:      En passant square
Planes 37-44:   Repetition history (last 4 positions × 2)
Planes 45-118:  Move history encoding (optional)
```

**Network Architecture**:
```
Input: 119 × 8 × 8
    │
    ▼
┌─────────────────────────────┐
│  Patch Embedding (8×8→64)   │
│  + Positional Encoding      │
└─────────────────────────────┘
    │
    ▼
┌─────────────────────────────┐
│  Transformer Blocks ×12     │
│  - Hidden dim: 512          │
│  - Attention heads: 8       │
│  - FFN expansion: 4x        │
│  - Pre-norm, GELU           │
└─────────────────────────────┘
    │
    ├──────────────────┐
    ▼                  ▼
┌──────────┐    ┌──────────┐
│  Policy  │    │  Value   │
│  Head    │    │  Head    │
│  1858    │    │  Scalar  │
│  logits  │    │  [-1,1]  │
└──────────┘    └──────────┘
```

**Parameter Count**: ~25M (small), ~100M (large), ~400M (research)

### 2.2 Policy Head Design

Map transformer output to legal moves:

```
Policy output: 1858 logits
├── Queen-like moves: 56 directions × 64 squares = 3584 (masked to ~1792)
├── Knight moves: 8 directions × 64 squares = 512 (masked to ~336)
├── Underpromotions: 3 types × 8 files × 2 colors = 48
└── Total unique moves per position: ~35 average, 218 max
```

Legal move masking applied before softmax.

### 2.3 Value Head Design

```
Transformer output [CLS token]
    │
    ▼
Linear(512 → 256) + GELU
    │
    ▼
Linear(256 → 1) + Tanh
    │
    ▼
Value ∈ [-1, 1]
```

Interpretation: -1 = loss, 0 = draw, +1 = win (from side-to-move perspective).

### 2.4 NNUE Fallback Mode

Retain NNUE for:
- Low-latency scenarios (bullet chess, mobile)
- CPU-only deployments
- A/B testing during transition

```mind
config EvalMode {
    Transformer,      // Default v4.0
    NNUE,            // v3.x compatible
    Hybrid(ratio),   // Blend both (research)
}
```

**Milestone**: Transformer eval reaching 3500+ Elo in isolation by end of Q2.

---

## Phase 3: MCTS Implementation (Q2–Q3 2026)

**Objective**: High-throughput GPU-batched Monte Carlo Tree Search.

### 3.1 Core MCTS Algorithm

```
function MCTS(root_position, iterations):
    tree = Tree(root_position)

    for i in 1..iterations:
        # Selection: traverse tree using PUCT
        leaf = select(tree.root)

        # Expansion: add node if not terminal
        if not leaf.is_terminal():
            leaf.expand()

        # Evaluation: neural network inference
        policy, value = network.evaluate(leaf.position)
        leaf.set_priors(policy)

        # Backpropagation: update ancestors
        backpropagate(leaf, value)

    return best_move(tree.root)
```

### 3.2 PUCT Selection Formula

```
UCB(s, a) = Q(s, a) + c_puct × P(s, a) × (√N(s)) / (1 + N(s, a))

Where:
  Q(s, a)   = Mean value of action a from state s
  P(s, a)   = Prior probability from policy network
  N(s)      = Visit count of state s
  N(s, a)   = Visit count of action a from state s
  c_puct    = Exploration constant (typically 1.5–2.5)
```

**Enhancements**:
- First Play Urgency (FPU): Initialize unvisited Q values below parent
- Virtual loss: Prevent thread collision during parallel selection
- Progressive widening: Limit branching factor early in search

### 3.3 GPU Batching Strategy

```
┌─────────────────────────────────────────────────────────┐
│                   MCTS Controller                       │
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ Worker 1 │  │ Worker 2 │  │ Worker N │  ...×1024     │
│  │  Tree 1  │  │  Tree 2  │  │  Tree N  │               │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘               │
│       │             │             │                     │
│       └─────────────┼─────────────┘                     │
│                     ▼                                   │
│            ┌─────────────────┐                          │
│            │  Batch Collector │                         │
│            │  (512–4096 pos) │                          │
│            └────────┬────────┘                          │
│                     ▼                                   │
│            ┌─────────────────┐                          │
│            │   GPU Inference  │  ← Single forward pass  │
│            │   (Transformer)  │                         │
│            └────────┬────────┘                          │
│                     ▼                                   │
│            ┌─────────────────┐                          │
│            │ Result Scatter   │                         │
│            └─────────────────┘                          │
└─────────────────────────────────────────────────────────┘
```

**Target Throughput**:
| Hardware | Batch Size | Positions/sec |
|----------|------------|---------------|
| RTX 4090 | 2048 | 800K–1.2M |
| A100 | 4096 | 2M–3M |
| M3 Max | 1024 | 300K–500K |
| CPU (AVX-512) | 64 | 10K–20K |

### 3.4 Tree Memory Management

```mind
struct MCTSNode {
    position_hash: u128,
    parent: Option<NodeId>,
    children: SmallVec<[Edge; 32]>,
    visit_count: AtomicU32,
    value_sum: AtomicF32,
    policy_prior: f16,
    flags: NodeFlags,
}

struct Edge {
    move: Move,
    child: Option<NodeId>,
}
```

**Memory Budget**: 8GB allows ~50M nodes (sufficient for tournament play).

**Milestone**: 1M+ positions/sec on RTX 4090 by end of Q3.

---

## Phase 4: Self-Play Training (Q3 2026)

**Objective**: Close the AlphaZero loop with MindLang-accelerated training.

### 4.1 Training Data Generation

```
Self-Play Pipeline:

┌─────────────────────────────────────────────────────────┐
│                    Game Generator                       │
│                                                         │
│  For each game:                                         │
│    1. Initialize from random opening (8-ply book)       │
│    2. Play with MCTS (800 simulations/move)             │
│    3. Temperature schedule:                             │
│       - Moves 1-30: τ=1.0 (exploration)                 │
│       - Moves 31+: τ=0.1 (exploitation)                 │
│    4. Add Dirichlet noise at root: Dir(0.3)             │
│    5. Record (position, policy, outcome) tuples         │
│    6. Terminate on checkmate, stalemate, or 512 moves   │
│                                                         │
│  Output: ~500 positions per game                        │
└─────────────────────────────────────────────────────────┘
```

**Parallelism**: 1024 concurrent games on 8×A100 cluster.

### 4.2 Replay Buffer

```
┌─────────────────────────────────────────────────────────┐
│                    Replay Buffer                        │
│                                                         │
│  Capacity: 2M positions (FIFO eviction)                 │
│                                                         │
│  Each sample:                                           │
│    - Position tensor: 119 × 8 × 8 (f16)                 │
│    - Policy target: 1858 (f16, visit proportions)       │
│    - Value target: scalar (f32, game outcome)           │
│    - Metadata: game_id, move_number, timestamp          │
│                                                         │
│  Augmentation:                                          │
│    - 8× symmetry (rotations + reflections)              │
│    - Color flip (swap perspectives)                     │
│                                                         │
│  Storage: ~150GB uncompressed, ~40GB with fp16          │
└─────────────────────────────────────────────────────────┘
```

### 4.3 Training Loop

```mind
fn train_iteration(network: &mut Network, buffer: &ReplayBuffer) {
    let batch = buffer.sample(4096);

    // Forward pass
    let (policy_logits, value) = network.forward(batch.positions);

    // Compute losses
    let policy_loss = cross_entropy(policy_logits, batch.policy_targets);
    let value_loss = mse(value, batch.value_targets);
    let l2_loss = network.l2_regularization(1e-4);

    let total_loss = policy_loss + value_loss + l2_loss;

    // Backward pass (MindLang compile-time autodiff)
    let gradients = autodiff(total_loss, network.parameters());

    // Optimizer step
    adam.step(network, gradients, lr=0.001);
}
```

**Training Schedule**:
| Phase | Games | Positions | GPU Hours |
|-------|-------|-----------|-----------|
| Bootstrap | 100K | 50M | 100 |
| Main training | 1M | 500M | 1000 |
| Fine-tuning | 100K | 50M | 100 |

### 4.4 Evaluation Arena

Automated tournament system for checkpoint evaluation:

```
Arena Schedule (every 10K training steps):
  1. Current checkpoint vs. previous best
  2. 400 games, 1 min + 1 sec increment
  3. Win rate > 55% → promote to best
  4. Log Elo progression, loss curves, game samples
```

**Milestone**: Net showing +200 Elo vs. v3.21.0 baseline by end of Q3.

---

## Phase 5: Symbolic Integration (Q4 2026)

**Objective**: Enhance neural evaluation with compile-time symbolic knowledge.

### 5.1 Tablebase Integration

```
┌─────────────────────────────────────────────────────────┐
│              Syzygy Tablebase Probing                   │
│                                                         │
│  Trigger: ≤7 pieces on board                            │
│                                                         │
│  Probe returns:                                         │
│    - WDL: Win/Draw/Loss with optimal play               │
│    - DTZ: Distance to zeroing (capture/pawn move)       │
│                                                         │
│  Integration modes:                                     │
│    1. Hard cutoff: Replace search at probe depth        │
│    2. Soft bias: Adjust value head output               │
│    3. Training signal: Include TB positions in buffer   │
│                                                         │
│  Coverage: 6-man (1.5GB), 7-man (140GB)                 │
└─────────────────────────────────────────────────────────┘
```

### 5.2 Fortress Detection

Neural-symbolic hybrid for drawn positions:

```mind
fn detect_fortress(pos: Position, value: f32) -> f32 {
    // Compile-time pattern matching
    if pos.is_kbk_wrong_color_bishop() {
        return 0.0;  // Forced draw
    }

    if pos.is_rook_vs_bishop_fortress() && value.abs() < 0.3 {
        return value * 0.5;  // Dampen winning chances
    }

    // Neural fortress detector (small CNN)
    let fortress_prob = fortress_net.evaluate(pos);
    if fortress_prob > 0.8 {
        return value * (1.0 - fortress_prob);
    }

    value
}
```

### 5.3 Opening Book

```
Opening Book Modes:
  1. Off: Pure neural play from move 1
  2. Narrow: Top 3 book moves for first 8 ply
  3. Wide: Human grandmaster repertoire for variety
  4. Anti-computer: Lines known to trouble neural nets
```

**Milestone**: v4.0.0 stable release achieving 4000+ Elo on TCEC hardware.

---

## Phase 6: Deployment & Ecosystem (Q4 2026+)

### 6.1 Platform Targets

| Platform | Binary Size | Notes |
|----------|-------------|-------|
| Linux x64 | ~15MB | Primary development target |
| Windows x64 | ~15MB | Tournament standard |
| macOS ARM | ~12MB | Apple Silicon optimized |
| Linux ARM | ~10MB | Raspberry Pi, embedded |
| WebAssembly | ~8MB | Browser-based analysis |
| Android | ~12MB | Mobile app integration |
| iOS | ~12MB | App Store distribution |

### 6.2 Deployment Modes

```
┌─────────────────────────────────────────────────────────┐
│                    Deployment Modes                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  [Standalone]                                           │
│    Single binary, UCI protocol                          │
│    Works with: Arena, SCID, Lichess, Chess.com          │
│                                                         │
│  [Server]                                               │
│    REST API + WebSocket for real-time analysis          │
│    Docker image, Kubernetes ready                       │
│                                                         │
│  [Cluster]                                              │
│    Distributed hash table across nodes                  │
│    MPI or gRPC for inter-node communication             │
│    Target: TCEC Superfinal hardware                     │
│                                                         │
│  [Embedded]                                             │
│    Reduced network (10M params)                         │
│    CPU-only, deterministic mode                         │
│    Target: DGT boards, dedicated chess computers        │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 6.3 Commercial Extensions

| Feature | Open Source | Commercial |
|---------|-------------|------------|
| UCI engine | Yes | Yes |
| MCTS search | Yes | Yes |
| Training code | Yes | Yes |
| Pre-trained weights | Yes (small) | Yes (all sizes) |
| Deterministic mode | No | Yes |
| Cluster orchestration | No | Yes |
| Priority support | No | Yes |
| Custom training | No | Yes |

### 6.4 Community Tools

- **nikola-gui**: Cross-platform analysis GUI (Tauri + React)
- **nikola-trainer**: Self-play training toolkit
- **nikola-book**: Opening book generator from PGN
- **nikola-bench**: Standardized benchmark suite

---

## Performance Targets

### Elo Progression

```
         Elo
        4200 ┤                                    ●────── v4.1 (Cluster)
             │                              ●──────────── v4.0 (Release)
        4000 ┤                        ●──────────────────── Target
             │                  ●────────────────────────── v4.0-rc
        3800 ┤            ●──────────────────────────────── v3.21.0 (Current)
             │      ●────────────────────────────────────── v4.0-beta
        3600 ┤ ●─────────────────────────────────────────── v4.0-alpha
             │
        3400 ┼─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────
                 Q1    Q2    Q3    Q4   Q1    Q2    Q3    Q4
                      2026                    2027
```

### Hardware Benchmarks

| Metric | v3.21.0 | v4.0 Target |
|--------|---------|-------------|
| Nodes/sec (RTX 4090) | 150M | 1M MCTS |
| Nodes/sec (CPU) | 30M | 15K MCTS |
| Memory usage | 512MB | 4GB (default) |
| Startup time | <100ms | <500ms |
| Position eval latency | 1µs | 100µs (batched) |

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Transformer scaling issues | Medium | High | Start small, validate at each step |
| MCTS batching complexity | Medium | Medium | Reference LCZero implementation |
| Training compute costs | High | Medium | Partner with cloud providers |
| Elo plateau before 4000 | Medium | High | Hybrid symbolic rules as differentiator |
| MindLang compiler bugs | Low | High | Comprehensive test suite, fallback to v3.x |
| Community adoption lag | Medium | Low | Frequent public releases, transparent benchmarks |

---

## Contributing

We welcome contributions across all phases:

1. **Core Development**: MindLang proficiency required
2. **Training Infrastructure**: Distributed systems experience
3. **Testing & Benchmarks**: Chess knowledge helpful
4. **Documentation**: Technical writing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Priority Areas
- GPU backend optimizations (Phase 1-2)
- MCTS implementation review (Phase 3)
- Training infrastructure (Phase 4)
- Cross-platform testing (Phase 6)

---

## Timeline Summary

| Phase | Dates | Key Deliverable |
|-------|-------|-----------------|
| 0 | Now – Q1 2026 | v3.21.x maintenance |
| 1 | Q1 2026 | Infrastructure refactor |
| 2 | Q2 2026 | Transformer evaluator |
| 3 | Q2–Q3 2026 | GPU-batched MCTS |
| 4 | Q3 2026 | Self-play training |
| 5 | Q4 2026 | Symbolic integration |
| 6 | Q4 2026+ | v4.0.0 release, ecosystem |

---

## References

- [AlphaZero Paper](https://arxiv.org/abs/1712.01815) - DeepMind, 2017
- [Leela Chess Zero](https://lczero.org/) - Open source AlphaZero implementation
- [MindLang Documentation](https://mindlang.dev/docs) - Language reference
- [TCEC](https://tcec-chess.com/) - Top Chess Engine Championship

---

*© 2026 STARGA Inc. All rights reserved.*
