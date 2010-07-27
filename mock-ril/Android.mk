# Copyright 2010 The Android Open Source Project
#
# not currently building V8 for x86 targets
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH:= $(call my-dir)

# Mock-ril only buid for debug variants
ifneq ($(filter userdebug eng tests, $(TARGET_BUILD_VARIANT)),)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ctrl_server.cpp \
    experiments.cpp \
    js_support.cpp \
    msgheader.pb.cpp \
    mock_ril.cpp \
    node_buffer.cpp \
    node_util.cpp \
    protobuf_v8.cpp \
    responses.cpp \
    requests.cpp \
    ril.pb.cpp \
    util.cpp \
    worker.cpp \
    worker_v8.cpp


LOCAL_SHARED_LIBRARIES := \
    libz libcutils libutils libril

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-cpp-2.3.0-full libv8

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
# __BSD_VISIBLE for htolexx macros.
LOCAL_STRIP_MODULE := true
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DMOCK_RIL -DRIL_SHLIB -D__BSD_VISIBLE
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libmock_ril

include $(BUILD_SHARED_LIBRARY)

endif

# Java librilproto
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := librilproto-java

LOCAL_STATIC_JAVA_LIBRARIES := libprotobuf-java-2.3.0-micro

LOCAL_SRC_FILES := $(call all-java-files-under, com)

include $(BUILD_STATIC_JAVA_LIBRARY)
# =======================================================

endif
