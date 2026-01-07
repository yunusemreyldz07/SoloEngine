EXE = SoloEngine

<<<<<<< HEAD
SOURCES = main.cpp board.cpp search.cpp evaluation.cpp
=======
ifeq ($(OS),Windows_NT)
    DETECTED_OS := windows
else

    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        DETECTED_OS := linux
    endif
    ifeq ($(UNAME_S),Darwin)
        DETECTED_OS := mac
    endif
endif

ifndef DETECTED_OS
    DETECTED_OS := unknown_os
endif

build:
	@echo "Detected OS: $(DETECTED_OS)"
ifeq ($(DETECTED_OS),unknown_os)
	@echo "Error: Could not detect OS. Please use: make windows, make linux, or make mac"
	@exit 1
else
	$(MAKE) $(DETECTED_OS)
endif
>>>>>>> 4eac210 (Makefile update)

CXX = g++

CXXFLAGS = -Ofast -march=native -flto -DNDEBUG -funroll-loops -fno-stack-protector -fomit-frame-pointer -std=c++17 -s

all:
<<<<<<< HEAD
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(EXE)
clean:
	rm -f $(EXE)
=======
	@echo "Building for all platforms..."
	$(MAKE) -f Makefile.mac
	$(MAKE) -f Makefile.windows
	$(MAKE) -f Makefile.android
	$(MAKE) -f Makefile.linux

# Debug/ASAN build targets
debug-mac:
	@echo "Building debug/ASAN for macOS..."
	$(MAKE) -f Makefile.mac asan

debug-linux:
	@echo "Building debug/ASAN for Linux..."
	$(MAKE) -f Makefile.linux asan

debug-windows:
	@echo "Building debug/ASAN for Windows..."
	$(MAKE) -f Makefile.windows asan

debug-android:
	@echo "Building debug/ASAN for Android..."
	$(MAKE) -f Makefile.android asan

unknown_os:
	@:
>>>>>>> 4eac210 (Makefile update)
