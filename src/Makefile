#!/usr/bin/make -f
# Kinetik KMSP-6 — build with DPF (https://github.com/DISTRHO/DPF)
NAME = KinetikKMSP6
FILES_DSP = MonoPlugin.cpp third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c
FILES_UI  = MonoUI.cpp Updater.cpp Updater.cpp
DPF_PATH ?= ../dpf
DPF_TARGET_DIR = ../bin
DPF_BUILD_DIR = ../build
include $(DPF_PATH)/Makefile.plugins.mk
BUILD_CXX_FLAGS += -std=gnu++17 -DKN_VERSION='"$(shell cat ../VERSION)"'
# plug-in version (hosts, bundle Info.plist, installers) = the VERSION file
KN_VPARTS := $(subst ., ,$(shell cat ../VERSION))
BUILD_CXX_FLAGS += -DKN_VER_MAJOR=$(or $(word 1,$(KN_VPARTS)),0) -DKN_VER_MINOR=$(or $(word 2,$(KN_VPARTS)),0) -DKN_VER_MICRO=$(or $(word 3,$(KN_VPARTS)),0) -DKN_VERSION='"$(shell cat ../VERSION)"'
ifeq ($(WINDOWS),true)
LINK_FLAGS += -lshell32
endif
TARGETS = vst3 clap
ifeq ($(MACOS),true)
TARGETS += au
endif
ifeq ($(LINUX),true)
TARGETS += jack
endif
all: $(TARGETS)
