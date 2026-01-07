.PHONY : build mac windows android linux clean all

build:
	@echo "Please use one of the following targets: mac, windows, android, linux"

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
	$(MAKE) -f Makefile.clean

all:
	@echo "Building for all platforms..."
	$(MAKE) -f Makefile.mac
	$(MAKE) -f Makefile.windows
	$(MAKE) -f Makefile.android
	$(MAKE) -f Makefile.linux

default: mac

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
