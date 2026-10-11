#!/usr/bin/make -f
# Kinetik KMSP-6 — build with DPF (https://github.com/DISTRHO/DPF)
NAME = KinetikKMSP6
FILES_DSP = MonoPlugin.cpp
FILES_UI  = MonoUI.cpp
DPF_PATH ?= ../dpf
DPF_TARGET_DIR = ../bin
DPF_BUILD_DIR = ../build
include $(DPF_PATH)/Makefile.plugins.mk
BUILD_CXX_FLAGS += -std=gnu++17
TARGETS = vst3 clap
ifeq ($(MACOS),true)
TARGETS += au
endif
ifeq ($(LINUX),true)
TARGETS += jack
endif
all: $(TARGETS)
