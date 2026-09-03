SHELL := /bin/bash

CMAKE ?= cmake
BUILD_DIR ?= build
TOOLCHAIN_FILE ?= cmake/aarch64-none-elf.toolchain.cmake
JOBS ?= 16

# CMake accepts a toolchain file only when a build directory is first
# configured. Avoid the confusing "variable not used" warning on later makes.
CMAKE_TOOLCHAIN_ARG :=
ifeq ($(wildcard $(BUILD_DIR)/CMakeCache.txt),)
CMAKE_TOOLCHAIN_ARG := -DCMAKE_TOOLCHAIN_FILE="$(TOOLCHAIN_FILE)"
endif

.PHONY: all configure build clean help

all: build

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_TOOLCHAIN_ARG)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" -j"$(JOBS)"
	$(CMAKE) -E make_directory application
	$(CMAKE) -E copy_if_different "$(BUILD_DIR)/kernel_2712.img" application/kernel_2712.img
	@printf '\nImage file located at application/kernel_2712.img\n\n'

clean:
	@if [[ -f "$(BUILD_DIR)/CMakeCache.txt" ]]; then \
		$(CMAKE) --build "$(BUILD_DIR)" --target clean; \
	fi
	$(CMAKE) -E rm -f application/kernel_2712.img

help:
	@printf '%s\n' \
		'make clean' \
		'make'
