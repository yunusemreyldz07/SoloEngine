.PHONY: build default windows linux mac android all clean mac-clean linux-clean windows-clean android-clean objs distclean list debug debug-windows debug-linux debug-mac debug-android windows-all linux-all

EXE := Solo

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

default: build

BASE_CXXFLAGS := -O3 -std=c++23 -ffast-math -pthread
ARCH_GENERIC := 
ARCH_POPCNT := -mpopcnt
ARCH_BMI2 := -mpopcnt -mbmi2
ARCH_AVX2 := -mpopcnt -mbmi2 -mavx2

EXEEXT ?=
OUT ?= $(EXE)$(EXEEXT)

$(OUT): $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(OUT) $(LINKER)
	@if command -v $(STRIP) >/dev/null 2>&1; then \
		$(STRIP) $(OUT); \
	else \
		echo "Strip tool '$(STRIP)' not found; skipping."; \
	fi

asan:
	$(CXX) $(SOURCES) -std=c++23 -g -O1 -fsanitize=address -fno-omit-frame-pointer -pthread $(ASAN_ARCH) -o $(EXE)_asan$(EXEEXT) $(LINKER)
	@echo "ASAN build created as $(EXE)_asan$(EXEEXT) (symbols preserved, no strip)"

debug: asan

WIN_CXX := g++
WIN_LINKER := -static -static-libgcc -static-libstdc++
WIN_STRIP := strip

windows: windows-avx2 

windows-all: windows-generic windows-popcnt windows-bmi2 windows-avx2

windows-generic:
	$(MAKE) build-windows EXEEXT=-windows-generic.exe CXX="$(WIN_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_GENERIC)" LINKER="$(WIN_LINKER)" STRIP="$(WIN_STRIP)"

windows-popcnt:
	$(MAKE) build-windows EXEEXT=-windows-popcnt.exe CXX="$(WIN_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_POPCNT)" LINKER="$(WIN_LINKER)" STRIP="$(WIN_STRIP)"

windows-bmi2:
	$(MAKE) build-windows EXEEXT=-windows-bmi2.exe CXX="$(WIN_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_BMI2)" LINKER="$(WIN_LINKER)" STRIP="$(WIN_STRIP)"

windows-avx2:
	$(MAKE) build-windows EXEEXT=.exe CXX="$(WIN_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_AVX2)" LINKER="$(WIN_LINKER)" STRIP="$(WIN_STRIP)"

build-windows: $(OUT)

debug-windows: EXEEXT := .exe
debug-windows: CXX := $(WIN_CXX)
debug-windows: ASAN_ARCH :=
debug-windows: LINKER :=
debug-windows:
	@asan_lib_a=`$(CXX) -print-file-name=libasan.a`; \
	asan_lib_dll=`$(CXX) -print-file-name=libasan.dll.a`; \
	if [ "$$asan_lib_a" = "libasan.a" ] && [ "$$asan_lib_dll" = "libasan.dll.a" ]; then \
		echo "ASAN runtime not found; building non-ASAN debug binary instead."; \
		$(CXX) $(SOURCES) -std=c++23 -g -O1 -fno-omit-frame-pointer -pthread -o $(EXE)_asan$(EXEEXT); \
	else \
		$(CXX) $(SOURCES) -std=c++23 -g -O1 -fsanitize=address -fno-omit-frame-pointer -pthread -o $(EXE)_asan$(EXEEXT); \
	fi
	@echo "ASAN build created as $(EXE)_asan$(EXEEXT) (symbols preserved, no strip)"

LINUX_CXX := g++
LINUX_LINKER := -lm
LINUX_STRIP := strip

linux: linux-avx2 

linux-all: linux-generic linux-popcnt linux-bmi2 linux-avx2

linux-generic:
	$(MAKE) build-linux EXEEXT=-linux-generic CXX="$(LINUX_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_GENERIC)" LINKER="$(LINUX_LINKER)" STRIP="$(LINUX_STRIP)"

linux-popcnt:
	$(MAKE) build-linux EXEEXT=-linux-popcnt CXX="$(LINUX_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_POPCNT)" LINKER="$(LINUX_LINKER)" STRIP="$(LINUX_STRIP)"

linux-bmi2:
	$(MAKE) build-linux EXEEXT=-linux-bmi2 CXX="$(LINUX_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_BMI2)" LINKER="$(LINUX_LINKER)" STRIP="$(LINUX_STRIP)"

linux-avx2:
	$(MAKE) build-linux EXEEXT= CXX="$(LINUX_CXX)" CXXFLAGS="$(BASE_CXXFLAGS) $(ARCH_AVX2)" LINKER="$(LINUX_LINKER)" STRIP="$(LINUX_STRIP)"

build-linux: $(OUT)

debug-linux: CXX := $(LINUX_CXX)
debug-linux: ASAN_ARCH :=
debug-linux: LINKER := $(LINUX_LINKER)
debug-linux: asan

mac: CXX := clang++
mac: CXXFLAGS := $(BASE_CXXFLAGS) -march=armv8-a
mac: LINKER := -lm
mac: STRIP := llvm-strip
mac: build-mac

build-mac: $(OUT)

debug-mac: CXX := clang++
debug-mac: ASAN_ARCH := -march=armv8-a
debug-mac: LINKER := -lm
debug-mac: asan

ANDROID_API ?= 35
ANDROID_HOST_TAG ?=
ANDROID_NDK_HOME ?=

ifeq ($(strip $(ANDROID_HOST_TAG)),)
ANDROID_HOST_TAG := $(shell uname -s | tr '[:upper:]' '[:lower:]')-x86_64
endif

android: EXEEXT :=
android: CXX ?= $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/$(ANDROID_HOST_TAG)/bin/aarch64-linux-android$(ANDROID_API)-clang++
android: CXXFLAGS := $(BASE_CXXFLAGS) -march=armv8-a
android: LINKER := -lm -static-libstdc++
android: STRIP := llvm-strip
android: build-android

build-android: check-android-ndk $(OUT)

check-android-ndk:
ifeq ($(strip $(ANDROID_NDK_HOME)),)
	$(error ANDROID_NDK_HOME is not set (e.g. export ANDROID_NDK_HOME=/path/to/ndk))
endif

debug-android: CXX ?= $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/$(ANDROID_HOST_TAG)/bin/aarch64-linux-android$(ANDROID_API)-clang++
debug-android: ASAN_ARCH := -march=armv8-a
debug-android: LINKER := -lm -static-libstdc++
debug-android: check-android-ndk asan

all:
	@echo "Building for all platforms..."
	$(MAKE) mac
	$(MAKE) windows-all
	$(MAKE) android
	$(MAKE) linux-all

clean: mac-clean linux-clean windows-clean android-clean objs
	@echo "Clean complete."

mac-clean:
	@rm -f $(EXE) $(EXE)_asan
	@echo "Removed Mac binaries"

linux-clean:
	@rm -f $(EXE)-linux-* $(EXE)_asan
	@echo "Removed Linux binaries"

windows-clean:
	@rm -f $(EXE)-windows-*.exe $(EXE)_asan.exe
	@echo "Removed Windows binaries"

android-clean:
	@rm -f $(EXE) $(EXE)_asan
	@echo "Removed Android binaries"

objs:
	@EXTRA_OBJS=`ls *.o 2>/dev/null`; \
	if [ -n "$$EXTRA_OBJS" ]; then rm -f $$EXTRA_OBJS; echo "Removed object files"; else echo "No extra object files found"; fi

distclean: clean
	@rm -f core core.* *.core
	@echo "Distclean complete (core dumps removed)."

list:
	@echo "Commands available:"
	@echo "  make windows"
	@echo "  make windows-all"
	@echo "  make linux"
	@echo "  make linux-all"
	@echo "  make mac"
	@echo "  make clean"