LOCAL_PATH := $(call my-dir)

CORE_DIR := $(LOCAL_PATH)/../..

include $(CORE_DIR)/libretro-build/Makefile.common

COREFLAGS := -DHAVE_XLOCALE -DHAVE_STRTOF_L -DDISABLE_THREADING -DMINIMAL_CORE=2 $(RETRODEFS) $(INCLUDES)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_CXX)
LOCAL_CPPFLAGS  := -O3 $(COREFLAGS)
LOCAL_CFLAGS    := -O3 $(COREFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(CORE_DIR)/link.T
LOCAL_ARM_MODE  := arm
include $(BUILD_SHARED_LIBRARY)
