# Solo - UCI Chess Engine
**Version 2.0.1**

A bitboard-based aggressive chess engine with advanced search techniques and evaluation.

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
  - Singular Extensions (SE)
  - Multicut
  - History Pruning

- **Move Ordering**:
  - TT move first
  - Good captures (SEE ≥ threshold) with MVV-LVA scoring
  - Killer moves
  - History + continuation history for quiet moves (Bad quiets are penalized)
  - Bad captures ordered last

- **Evaluation**:
  - 256HL NNUE net trained with 500M self-play generated data. Fine-tuned with 2M filtered aggressive positions. Estimated EAS score is 200k.

## Building

The Makefile automatically detects your operating system and compiles with the highest optimizations available for your processor architecture.
```bash
# Simply run make
make

# Clean build artifacts
make clean
```

**Output**: Executable will be created as `Solo.exe` (Windows) or `Solo` (Linux/Mac)

### Manual Compilation

If you don't have Make:

# Windows (MinGW/MSYS2)
```g++ -O3 -flto -march=native -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp nnue.cpp datagen.cpp -o Solo.exe -static -static-libgcc -static-libstdc++```

# Linux
```g++ -O3 -flto -march=native -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp nnue.cpp datagen.cpp -o Solo -lm```

# macOS (Apple Silicon)
```clang++ -O3 -flto -march=native -std=c++23 -ffast-math -pthread main.cpp board.cpp movegen.cpp search.cpp evaluation.cpp bitboard.cpp history.cpp nnue.cpp datagen.cpp -o Solo -lm```

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

- **CCRL ELO**: [~2900+](https://computerchess.org.uk/ccrl/4040/cgi/engine_details.cgi?print=Details&each_game=0&eng=Solo%201.6.0%2064-bit#Solo_1_6_0_64-bit)
- **Lichess ELO**: [~2500](https://lichess.org/@/SoloBot)

## Roadmap

- [ ] Multi-threading support (Lazy SMP)
- [ ] Continuation history (2 & 4 ply)

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
├── nnue.cpp/h          # NNUE evaluation
├── datagen.cpp/h       # Data generation for training
├── main.cpp            # Entry point
└── Makefile            # Build system
```

## Credits

- **Author**: xsolod3v
- **Inspired by**: Patricia engine by Adam Kulju, Potential Engine by ProgramciDusunur
- **Evaluation Base**: For HCE: PeSTO piece-square tables by Ronald Friederich (further tuned) For NNUE: 500M self-play generated data.
- **Resources**: Chess Programming Wiki, Potential source code, Ethereal source code, Patricia wiki

## License

**MIT License** - Do whatever you want with it!

See [LICENSE](LICENSE) for full details.

---

**Want to contribute?** Feel free to open issues or pull requests!
