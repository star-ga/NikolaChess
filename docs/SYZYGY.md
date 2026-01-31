# Syzygy Tablebase Integration

Copyright (c) 2026 STARGA, Inc. All rights reserved.

## Overview

NikolaChess supports Syzygy tablebases for perfect endgame play.

## Supported Configurations

| Pieces | Size | Description |
|--------|------|-------------|
| 3-5 man | ~1 GB | Standard endgames |
| 6 man | ~150 GB | Extended coverage |
| 7 man | ~140 GB | Near-complete |
| 8 man | ~16 TB | Experimental |

## File Formats

- `.rtbw` - WDL (Win/Draw/Loss) tables
- `.rtbz` - DTZ (Distance to Zero) tables

## Probing

```mind
fn probe_wdl(board: &Board) -> Option<WdlResult> {
    if board.piece_count() > TB_MAX_PIECES {
        return None;
    }

    let result = syzygy_probe_wdl(board);
    match result {
        WDL_WIN => Some(WdlResult::Win),
        WDL_DRAW => Some(WdlResult::Draw),
        WDL_LOSS => Some(WdlResult::Loss),
        _ => None,
    }
}
```

## Search Integration

1. **Root probing**: Check tablebase at root for best move
2. **In-search probing**: Cutoff when position is in tablebase
3. **DTZ for 50-move rule**: Ensure moves preserve winning DTZ

## UCI Options

| Option | Default | Description |
|--------|---------|-------------|
| SyzygyPath | | Path to tablebase files |
| SyzygyProbeDepth | 1 | Minimum depth to probe |
| Syzygy50MoveRule | true | Respect 50-move rule |
| SyzygyProbeLimit | 7 | Max pieces to probe |

## Download

Use the syzygy tool:
```bash
mindc run tools/syzygy.mind -- download 6 ./syzygy
```

Or download from:
- https://tablebase.lichess.ovh/
- http://tablebase.sesse.net/

## Memory Usage

Tablebases are memory-mapped:
- Only pages accessed are loaded
- OS handles caching
- Minimal memory footprint

## Performance

| Operation | Time |
|-----------|------|
| WDL probe | <1μs |
| DTZ probe | <10μs |
| Cache miss | ~1ms |
