# Copyright 2010 The Android Open Source Project

# not currently building V8 for x86 targets
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    t.pb.cpp \
    mock-ril.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils libril

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-cpp-2.3.0-lite libv8

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE -UNDEBUG -DGOOGLE_PROTOBUF_NO_RTTI

LOCAL_C_INCLUDES := \
    external/protobuf/src \
    external/v8/include \
    bionic \
    $(KERNEL_HEADERS)

# stlport conflicts with the host stl library
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libstlport
LOCAL_C_INCLUDES += external/stlport/stlport
endif

# build shared library but don't require it be prelinked
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DMOCK_RIL -DRIL_SHLIB
LOCAL_MODULE:= libmock_ril

include $(BUILD_SHARED_LIBRARY)

endif
