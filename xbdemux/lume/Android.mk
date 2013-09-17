LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#include $(call all-makefiles-under,$(LOCAL_PATH))

LUME_TOP := $(LOCAL_PATH)

PV_CFLAGS := -DUSE_IPU_THROUGH_MODE $(XBOMX_FLAGS)

include $(LUME_TOP)/mk/mpdemuxer/Android.mk
include $(LUME_TOP)/mk/avformat/Android.mk