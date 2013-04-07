#
# legacy Audio HW
#

LOCAL_PATH := $(call my-dir)

ifeq ($(AUDIO_HW_USE_LEGACY), true)

include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.xb4780
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SRC_FILES := \
	audio_hw.cpp \
	AudioHardware.cpp

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libutils \
	libmedia \

LOCAL_STATIC_LIBRARIES := \
	libmedia_helper \
	libaudiohw_legacy

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif #AUDIO_HW_USE_LEGACY
