#
# Audio Policy
#

LOCAL_PATH := $(call my-dir)

ifeq ($(AUDIO_POLICY_FOR_MID), true)

include $(CLEAR_VARS)

LOCAL_MODULE := audio_policy.xb4780
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SRC_FILES := audio_policy.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif #AUDIO_POLICY_USE_LEGACY