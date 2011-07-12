# Copyright 2010 The Android Open Source Project
#
# not currently building V8 for x86 targets

LOCAL_PATH:= $(call my-dir)

# Directories of source files
src_cpp := src/cpp
src_java := src/java
src_py := src/py
src_js := src/js
src_proto := src/proto

ifeq ($(TARGET_ARCH),arm)
# Mock-ril only buid for debug variants
ifneq ($(filter userdebug eng tests, $(TARGET_BUILD_VARIANT)),)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    $(src_cpp)/ctrl_server.cpp \
    $(src_cpp)/experiments.cpp \
    $(src_cpp)/js_support.cpp \
    $(src_cpp)/mock_ril.cpp \
    $(src_cpp)/node_buffer.cpp \
    $(src_cpp)/node_util.cpp \
    $(src_cpp)/protobuf_v8.cpp \
    $(src_cpp)/responses.cpp \
    $(src_cpp)/requests.cpp \
    $(src_cpp)/util.cpp \
    $(src_cpp)/worker.cpp \
    $(src_cpp)/worker_v8.cpp \
    $(call all-proto-files-under, $(src_proto))

LOCAL_SHARED_LIBRARIES := \
    libz libcutils libutils libril

LOCAL_STATIC_LIBRARIES := \
    libv8

LOCAL_CFLAGS := -D_GNU_SOURCE -UNDEBUG -DRIL_SHLIB

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/$(src_cpp) \
    external/v8/include \
    bionic \
    $(KERNEL_HEADERS)

LOCAL_SHARED_LIBRARIES += libstlport
LOCAL_C_INCLUDES += external/stlport/stlport

# __BSD_VISIBLE for htolexx macros.
LOCAL_STRIP_MODULE := true

LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DMOCK_RIL -D__BSD_VISIBLE
LOCAL_PROTOC_OPTIMIZE_TYPE := full
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libmock_ril

include $(BUILD_SHARED_LIBRARY)

endif
endif

# Java librilproto
# =======================================================
include $(CLEAR_VARS)

LOCAL_MODULE := librilproto-java

LOCAL_PROTOC_OPTIMIZE_TYPE := micro

LOCAL_SRC_FILES := $(call all-java-files-under, $(src_java)) \
	$(call all-proto-files-under, $(src_proto))

include $(BUILD_STATIC_JAVA_LIBRARY)
# =======================================================

src_cpp :=
src_java :=
src_py :=
src_js :=
src_proto :=
