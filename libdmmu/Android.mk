
##ifeq ($(BUILD_WITH_LIB_JZ_IPU), true)
ifeq (true, true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	dmmu.c \


LOCAL_C_INCLUDES := \
	kernel/drivers/video

LOCAL_SHARED_LIBRARIES:= \
	liblog \
	libcutils

LOCAL_MODULE := libdmmu
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

endif				#BUILD_WITH_LIB_JZ_IPU
