CXX := g++
CXXFLAGS := -O3 -std=c++23 -ffast-math -flto -march=native -pthread
STRIP := strip

ifeq ($(OS),Windows_NT)
    EXE ?= Solo.exe
    # -link esnasında debug symbolleri otomatik silsin diye -s eklendi
    LINKER := -static -static-libgcc -static-libstdc++ -s
else
    EXE ?= Solo
    LINKER := -lm -s
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
	@echo "Solo binary was successfully created."

clean:
	@rm -f $(EXE) $(EXE).exe
	@rm -f *.o
	@echo "Removed solo binaries."