# SoloEngine - UCI Chess Engine
**Version 1.1.0**

A bitboard-based chess engine with advanced search techniques and evaluation.

## Features

- **Bitboard Representation**: 64-bit bitboards for efficient move generation
- **Magic Bitboards**: Fast sliding piece attack generation using magic numbers
- **Search Algorithm**: 
  - Negamax with alpha-beta pruning
  - Iterative deepening
  - Aspiration windows
  - Null move pruning (NMP)
  - Late move reductions (LMR)
  - Late move pruning (LMP)
  - Quiescence search with delta pruning
  - Principal Variation Search (PVS) with null-window re-search
  - Futility Pruning (FP)
  - Reverse Futility Pruning (RFP)
  - Static Exchange Evaluation (SEE) with pruning
  - Transposition Table with lockless thread-safe design

- **Move Ordering**:
  - Transposition Table (TT) move first
  - MVV-LVA with SEE pruning for bad captures
  - History heuristic with depth-squared bonus
  - PV move from previous iteration prioritized

- **Evaluation**:
  - PeSTO's Piece-Square Tables
  - Tapered evaluation (midgame/endgame interpolation)
  - Incremental Zobrist hashing
  - Threefold repetition detection
  - Mobility Bonus
  - 50-moves & insufficient material draw detection

## Building

The Makefile automatically detects your operating system.
```bash
# Automatic detection (recommended)
make

# Or specify your OS explicitly
make windows
make linux
make mac

# Clean build artifacts
make clean
```

**Output**: Executable will be created as `SoloEngine.exe` (Windows) or `SoloEngine` (Linux/Mac)

### Manual Compilation

If you don't have Make:

# Windows (MinGW/MSYS2)
```g++ -O3 -mavx2 -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o SoloEngine.exe -static -static-libgcc -static-libstdc++```

# Linux
```g++ -O3 -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o SoloEngine -lm```

# macOS (Apple Silicon)
```clang++ -O3 -std=c++23 -ffast-math -march=armv8-a -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o SoloEngine -lm```

## Usage

### UCI Mode
```bash
./SoloEngine

# Example UCI commands:
uci
setoption name Hash value 64
position startpos moves e2e4 e7e5
go depth 10
```

### Benchmark
```bash
./SoloEngine bench
```

Runs a built-in benchmark on 12 positions at depth 8.

## UCI Options

| Option | Type | Default | Range | Description |
|--------|------|---------|-------|-------------|
| `Hash` | spin | 128 | 1-2048 | Transposition table size in MB |
| `Threads` | spin | 1 | 1-8 | Number of search threads *(not implemented yet)* |
| `UseTT` | check | true | - | Enable/disable transposition table |

## Strength

- **Estimated ELO**: ~2136 (based on benchmark tests)
- **Lichess ELO**: [~2100](https://lichess.org/@/SoloBot)
- **Perft Verified**: Passes standard perft test suites

## Roadmap

- [ ] Multi-threading support (lazy SMP)
- [ ] Syzygy endgame tablebase support
- [ ] Improved time management (soft/hard bounds)
- [ ] NNUE evaluation (future consideration)

## Project Structure
```
├── bitboard.cpp/h      # Magic bitboards & attack generation
├── board.cpp/h         # Board representation & move make/unmake
├── evaluation.cpp/h    # PeSTO evaluation
├── movegen.cpp         # Legal move generation
├── search.cpp/h        # Negamax search with pruning
├── history.cpp/h       # History heuristic
├── types.h             # Basic types (Bitboard, etc.)
├── main.cpp            # UCI protocol handler
└── Makefile            # Build system
```

## Credits

- **Author**: xsolod3v
- **Inspired by**: Potential Engine by ProgramciDusunur
- **Evaluation**: PeSTO piece-square tables by Ronald Friederich
- **Resources**: Chess Programming Wiki, Potential source code, Ethereal source code

## License

**MIT License** - Do whatever you want with it!

See [LICENSE](LICENSE) for full details.

---

**Want to contribute?** Feel free to open issues or pull requests!
