.PHONY : build mac windows android linux clean all

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

mac:
	@echo "Building for macOS..."
	$(MAKE) -f Makefile.mac

windows:
	@echo "Building for Windows..."
	$(MAKE) -f Makefile.windows

android:
	@echo "Building for Android..."
	$(MAKE) -f Makefile.android

linux:
	@echo "Building for Linux..."
	$(MAKE) -f Makefile.linux

clean:
	@echo "Cleaning build artifacts..."
	$(MAKE) -f Makefile.clean all

all:
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