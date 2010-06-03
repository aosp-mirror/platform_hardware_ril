# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    t.pb.cpp \
    mock-ril.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils libril libstlport

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-cpp-2.3.0-lite

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE -UNDEBUG -DGOOGLE_PROTOBUF_NO_RTTI

LOCAL_C_INCLUDES := \
    external/protobuf/src \
    bionic \
    external/stlport/stlport \
    $(KERNEL_HEADERS)

# build shared library but don't require it be prelinked
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DMOCK_RIL -DRIL_SHLIB
LOCAL_MODULE:= libmock_ril

include $(BUILD_SHARED_LIBRARY)
