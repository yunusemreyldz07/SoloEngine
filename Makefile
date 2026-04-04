CXX := g++
CXXFLAGS := -O3 -std=c++23 -ffast-math -flto -march=native -pthread
STRIP := strip

ifeq ($(OS),Windows_NT)
    EXE := Solo.exe
    LINKER := -static -static-libgcc -static-libstdc++
else
    EXE := Solo
    LINKER := -lm
endif


SOURCES := main.cpp \
           uci.cpp \
           board.cpp \
           movegen.cpp \
           search.cpp \
           evaluation.cpp \
           bitboard.cpp \
           history.cpp \
           nnue.cpp \
           datagen.cpp

build: $(EXE)

$(EXE): $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(EXE) $(LINKER)
	@if command -v $(STRIP) >/dev/null 2>&1; then \
		$(STRIP) $(EXE); \
	fi
	@echo "Solo binary was successfully created."

clean:
	@rm -f $(EXE)
	@rm -f *.o
	@echo "Removed solo binaries."