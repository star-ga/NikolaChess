# NikolaChess API Documentation

Copyright (c) 2026 STARGA, Inc. All rights reserved.

## Overview

NikolaChess provides a comprehensive API system supporting all major chess protocols, databases, and online platforms. The engine can operate in multiple modes: standalone UCI/CECP, online bot (Lichess/Chess.com), and analysis server.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      NikolaChess Engine                         │
├─────────────────────────────────────────────────────────────────┤
│                         Unified API                              │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │ Syzygy  │ │ Opening │ │  Cloud  │ │  Games  │ │ Players │   │
│  │   TB    │ │  Books  │ │  Eval   │ │   DB    │ │   API   │   │
│  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘   │
│       │          │          │          │          │             │
│  ┌────┴──────────┴──────────┴──────────┴──────────┴────┐       │
│  │               Network Layer                          │       │
│  │  ┌──────┐  ┌──────┐  ┌───────────┐                  │       │
│  │  │ HTTP │  │ TCP  │  │ WebSocket │                  │       │
│  │  └──────┘  └──────┘  └───────────┘                  │       │
│  └──────────────────────────────────────────────────────┘       │
├─────────────────────────────────────────────────────────────────┤
│                       Protocols                                  │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐               │
│  │   UCI   │ │  CECP   │ │ Lichess │ │Chess.com│               │
│  │         │ │ xboard  │ │   Bot   │ │   Bot   │               │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘               │
└─────────────────────────────────────────────────────────────────┘
```

## Protocols

### UCI (Universal Chess Interface)

The primary protocol for GUI communication. Full UCI 2.0 support.

**File:** `src/api/uci.mind`

**Standard Commands:**
- `uci` - Initialize UCI mode
- `isready` / `readyok` - Synchronization
- `setoption name <name> value <value>` - Set options
- `ucinewgame` - New game
- `position [startpos|fen <fen>] [moves <move>...]` - Set position
- `go [options]` - Start search
- `stop` - Stop search
- `ponderhit` - Ponder move was played
- `quit` - Exit

**NikolaChess Options:**
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| Hash | spin | 256 | Hash table size (MB) |
| Threads | spin | 1 | Search threads |
| Use NNUE | check | true | Enable NNUE evaluation |
| NNUE Path | string | | Custom NNUE network path |
| Use Cloud Eval | check | false | Enable cloud evaluation |
| LAN Tablebase | string | | LAN tablebase server address |
| SyzygyPath | string | | Path to Syzygy tablebases |
| Ponder | check | false | Enable pondering |
| MultiPV | spin | 1 | Number of principal variations |

**Custom Commands:**
- `d` / `display` - Display current board
- `eval` - Show evaluation details
- `probe` - Probe tablebases/books
- `bench [depth]` - Run benchmark
- `perft <depth>` - Performance test

### CECP/XBoard

Full protocol v2 support for compatibility with XBoard, WinBoard, and Arena.

**File:** `src/api/cecp.mind`

**Key Commands:**
- `xboard` - Enter xboard mode
- `protover N` - Protocol version
- `new` - New game
- `force` - Force mode (accept moves only)
- `go` - Start thinking
- `level MPS BASE INC` - Time control
- `setboard FEN` - Set position
- `analyze` - Enter analysis mode
- `undo` / `remove` - Undo moves
- `post` / `nopost` - Thinking output

**Features Announced:**
- setboard, ping, playother, san=0
- usermove, time, draw, analyze
- memory, smp, egt="syzygy"

### Lichess Bot API

Full Lichess Bot API support for playing online.

**File:** `src/api/lichess.mind`

**Features:**
- OAuth2 authentication
- Event streaming (SSE)
- Challenge accept/decline with criteria
- Game streaming and move making
- Chat handling
- Draw offers, resignation

**Configuration:**
```mind
let settings = LichessSettings {
    min_rating: 1500,
    max_rating: 2800,
    accept_rated: true,
    accept_casual: true,
    accept_bullet: true,
    accept_blitz: true,
    accept_rapid: true,
    variants: vec!["standard"],
    max_concurrent_games: 4,
};
```

### Chess.com Bot API

WebSocket-based API for Chess.com bot accounts.

**File:** `src/api/chesscom.mind`

**Features:**
- Cometd/WebSocket connection
- Challenge handling
- Real-time game state updates
- Public API for player/game data

## Databases

### Syzygy Tablebases

7-man and 8-man support with intelligent fallback.

**File:** `src/api/syzygy.mind`

**Probe Order:**
1. Local (Fathom FFI)
2. LAN Server
3. Lichess API (7-man)
4. ChessDB API (7-man + 8-man)

**LAN Server Protocol:**
```
Client: PROBE <fen>
Server: WDL <wdl> DTZ <dtz> MOVE <uci>
```

**Starting LAN Server:**
```mind
let server = SyzygyServer::new("0.0.0.0", 7777);
server.set_path("/path/to/syzygy");
server.run();
```

### Opening Books

Support for multiple book formats.

**File:** `src/api/opening.mind`

**Supported Formats:**
| Format | Extension | Source |
|--------|-----------|--------|
| Polyglot | .bin | Local |
| ChessBase CTG | .ctg | Local |
| Arena ABK | .abk | Local |
| Lichess Explorer | HTTP | Remote |
| Lichess Masters | HTTP | Remote |
| ChessDB Opening | HTTP | Remote |

**Book Move Selection:**
```mind
let book = PolyglotBook::open("/path/to/book.bin");
let moves = book.probe(&board);

// Weighted random selection
let selected = select_book_move(&moves, BookSelectionMode::WeightedRandom);
```

### Cloud Evaluation

Unified interface for cloud analysis.

**File:** `src/api/cloud.mind`

**Sources:**
- Lichess Cloud Eval (Stockfish analysis)
- ChessDB Eval (deep analysis)
- Custom servers

**API Response:**
```mind
struct CloudEval {
    score_cp: i32,
    score_mate: Option<i32>,
    depth: i32,
    pv: Vec<Move>,
    source: str,
    cached: bool,
}
```

### Game Database

PGN parsing, storage, and position search.

**File:** `src/api/games.mind`

**Features:**
- PGN parsing (single and multi-game files)
- Position indexing
- Opening statistics
- Game search (player, result, ECO, date, ELO)
- Remote database queries (Lichess, Chess.com)

**Query Example:**
```mind
let db = GameDatabase::load("games.pgn");

let query = GameQuery::new()
    .player("Carlsen")
    .eco("B90")
    .min_elo(2700)
    .limit(100);

let games = db.search(&query);
```

### Player API

Player information and statistics.

**File:** `src/api/players.mind`

**Sources:**
- Lichess API
- Chess.com API
- FIDE (limited)

**Features:**
- Profile lookup
- Rating history
- Titled player lists
- Leaderboards

## Network Modules

### HTTP Client

Connection-pooled HTTP/HTTPS client.

**File:** `src/api/http.mind`

**Usage:**
```mind
let pool = HttpClientPool::new(4);

let response = pool.get("https://lichess.org/api/user/DrNykterstein")
    .header("Accept", "application/json")
    .timeout(10000)
    .send()?;

let json = response.json()?;
```

### TCP Client/Server

For LAN communication and custom protocols.

**File:** `src/api/tcp.mind`

**Features:**
- Connection pooling
- Service discovery (multicast)
- LAN tablebase protocol
- LAN analysis protocol

### WebSocket

RFC 6455 WebSocket implementation.

**File:** `src/api/websocket.mind`

**Features:**
- Secure (wss://) and plain (ws://)
- Automatic ping/pong
- Connection pooling
- Reconnecting WebSocket wrapper

## Unified API

Single entry point for all data sources.

**File:** `src/api/unified.mind`

**Configuration:**
```mind
let config = APIConfig {
    // Syzygy
    syzygy_enabled: true,
    syzygy_path: "/path/to/syzygy",
    syzygy_lan_server: Some("192.168.1.100:7777"),

    // Opening books
    opening_book_enabled: true,
    opening_book_path: "/path/to/book.bin",
    lichess_explorer_enabled: true,

    // Cloud evaluation
    lichess_cloud_enabled: true,
    chessdb_eval_enabled: true,

    // Online play
    lichess_token: Some("lip_..."),

    // Performance
    cache_size: 1_000_000,
    remote_timeout_ms: 5000,
};

let api = ChessAPI::new(config);
```

**Probing:**
```mind
// Intelligent probe with fallback
let result = api.probe(&board);

match result {
    ProbeResult::Tablebase(tb) => {
        println!("TB: WDL={} DTZ={}", tb.wdl, tb.dtz);
    }
    ProbeResult::Book(moves) => {
        println!("Book: {} moves", moves.len());
    }
    ProbeResult::CloudEval(eval) => {
        println!("Cloud: {} @ depth {}", eval.score_cp, eval.depth);
    }
    ProbeResult::None => {
        println!("No data available");
    }
}
```

## Usage Examples

### UCI Engine Mode

```bash
./nikolachess
uci
setoption name Hash value 1024
setoption name Threads value 8
setoption name SyzygyPath value /syzygy
ucinewgame
position startpos moves e2e4 e7e5
go depth 20
```

### Lichess Bot

```mind
let config = APIConfig::default();
config.lichess_token = Some("lip_xxxxxxxxxxxxx");

let mut bot = LichessBot::new("NikolaChessBot", config);
bot.settings.min_rating = 1800;
bot.settings.accept_bullet = false;

run_lichess_bot(&mut bot);
```

### Analysis Server

```mind
// Start LAN analysis server
let mut server = TcpServer::new("0.0.0.0", 9999);

server.start(|client, request| {
    if request.starts_with("ANALYZE") {
        let fen = request[8..].trim();
        let result = analyze_position(fen, 20);
        return Some(format_analysis_result(&result));
    }
    None
});
```

## Performance Considerations

1. **Caching:** All remote sources are cached (configurable size)
2. **Connection Pooling:** HTTP and TCP connections are pooled
3. **Async Probing:** Non-blocking probes for parallel search
4. **Fallback Order:** Optimized for latency (local → LAN → remote)
5. **Batch Probing:** Multiple positions can be probed in one call

## Security Notes

- API tokens stored in memory only
- TLS for all remote connections
- No credential logging
- Rate limiting compliance
- Sandboxed network access

## Version History

- **1.0** - Initial release with UCI, CECP, Lichess
- **1.1** - Added Chess.com, 8-man Syzygy
- **1.2** - Added game database, player API
