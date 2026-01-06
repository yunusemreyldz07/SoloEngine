EXE = SoloEngine

SOURCES = main.cpp board.cpp search.cpp evaluation.cpp

CXX = g++

CXXFLAGS = -Ofast -march=native -flto -DNDEBUG -funroll-loops -fno-stack-protector -fomit-frame-pointer -std=c++17 -s

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(EXE)
clean:
	rm -f $(EXE)
