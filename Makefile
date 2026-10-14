#!/usr/bin/make -f
# Scope Synth. Windows VST3 + CLAP (for FL Studio), built from Linux:
#   make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
# macOS AU + VST3 + CLAP: just `make` on a Mac (.github/workflows/macos.yml builds a universal one)
# DPF goes inside this folder (see README); ../dpf and ../crt-scope-vst/dpf also work
DPF_PATH ?= $(firstword $(wildcard dpf ../dpf ../crt-scope-vst/dpf))
DPF_TARGET_DIR = bin
DPF_BUILD_DIR = build
NAME = Scope-Synth
FILES_DSP = ScopeSynthPlugin.cpp
FILES_UI  = ScopeSynthUI.cpp

include $(DPF_PATH)/Makefile.plugins.mk

BUILD_CXX_FLAGS += -std=gnu++17
ifeq ($(WINDOWS),true)
LINK_FLAGS += -lshell32   # presets: OPEN FOLDER
endif

TARGETS += vst3 clap
ifeq ($(MACOS),true)
TARGETS += au   # GarageBand / Logic
endif
all: $(TARGETS)
