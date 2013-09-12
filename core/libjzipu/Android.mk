
##ifeq ($(BUILD_WITH_LIB_JZ_IPU), true)
ifeq (true, true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
android_jz_ipu.c \
android_jz_ipu_table.c \
android_jz_ipu_table_const.c


## include jz_ipu.h
LOCAL_C_INCLUDES := \
	kernel/drivers/video/jz4780-ipu

LOCAL_SHARED_LIBRARIES:= \
	liblog \
	libcutils

LOCAL_MODULE := libjzipu
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
endif				#BUILD_WITH_LIB_JZ_IPU
