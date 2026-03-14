# Solo - UCI Chess Engine
**Version 1.6.0**

A bitboard-based chess engine with advanced search techniques and evaluation.

## Features

- **Bitboard Representation**: 64-bit bitboards for efficient move generation
- **Magic Bitboards**: Fast sliding piece attack generation using magic numbers
- **Search Algorithm**: 
  - Negamax with alpha-beta pruning
  - Iterative deepening
  - Aspiration windows
  - Principal Variation Search (PVS)
  - Null Move Pruning (NMP) with adaptive reduction
  - Late Move Reductions (LMR)
  - Late Move Pruning (LMP)
  - Reverse Futility Pruning (RFP)
  - Futility Pruning (FP)
  - Static Exchange Evaluation (SEE) pruning
  - Internal Iterative Reductions (IIR)
  - Check extensions
  - Quiescence search with SEE filtering
  - Repetition / draw detection in search
  - Soft/Hard time management
  - Transposition Table

- **Move Ordering**:
  - TT move first
  - Good captures (SEE ≥ threshold) with MVV-LVA scoring
  - Killer moves
  - History + continuation history for quiet moves (Bad quiets are penalized)
  - Bad captures ordered last

- **Evaluation**:
  - Tuned MG/EG piece values (not default PeSTO values)
  - Tuned PSTs (PeSTO-initialized, then engine-tuned)
  - Tapered evaluation (midgame/endgame interpolation)
  - Piece mobility (Knight, Bishop, Rook, Queen) with MG/EG terms
  - King-zone attack pressure from mobility
  - Double pawn penalty
  - Isolated pawn penalty
  - Passed pawn bonuses (MG/EG tables)
  - King shield bonus
  - Precomputed passed-pawn masks for faster eval hot path

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

**Output**: Executable will be created as `Solo.exe` (Windows) or `Solo` (Linux/Mac)

### Manual Compilation

If you don't have Make:

# Windows (MinGW/MSYS2)
```g++ -O3 -mavx2 -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o Solo.exe -static -static-libgcc -static-libstdc++```

# Linux
```g++ -O3 -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o Solo -lm```

# macOS (Apple Silicon)
```clang++ -O3 -std=c++23 -ffast-math -march=armv8-a -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp -o Solo -lm```

## Usage

### UCI Mode
```bash
./Solo

# Example UCI commands:
uci
setoption name Hash value 64
position startpos moves e2e4 e7e5
go depth 10
```

### Benchmark
```bash
./Solo bench
```

Runs a built-in benchmark on 12 positions at depth 8.

## UCI Options

| Option | Type | Default | Range | Description |
|--------|------|---------|-------|-------------|
| `Hash` | spin | 128 | 1-2048 | Transposition table size in MB |
| `Threads` | spin | 1 | 1-8 | Number of search threads *(not implemented yet)* |

## Strength

- **Estimated ELO**: ~2300+ (based on SPRT testing)
- **Lichess ELO**: [~2200](https://lichess.org/@/SoloBot)

## Roadmap

- [ ] Multi-threading support (Lazy SMP)
- [ ] NNUE evaluation
- [ ] Continuation history
- [ ] Singular extensions

## Project Structure
```
├── bitboard.cpp/h      # Magic bitboards & attack generation
├── board.cpp/h         # Board representation & move make/unmake
├── evaluation.cpp/h    # Tuned tapered evaluation & mobility
├── history.cpp/h       # History heuristic for move ordering
├── movegen.cpp         # Legal move generation
├── search.cpp/h        # Negamax search, pruning & reductions
├── uci.cpp/h           # UCI protocol handler
├── types.h             # Basic types & constants
├── main.cpp            # Entry point
└── Makefile            # Build system
```

## Credits

- **Author**: xsolod3v
- **Inspired by**: Patricia engine by Adam Kulju, Potential Engine by ProgramciDusunur
- **Evaluation Base**: PeSTO piece-square tables by Ronald Friederich (further tuned)
- **Resources**: Chess Programming Wiki, Potential source code, Ethereal source code

## License

**MIT License** - Do whatever you want with it!

See [LICENSE](LICENSE) for full details.

---

**Want to contribute?** Feel free to open issues or pull requests!
