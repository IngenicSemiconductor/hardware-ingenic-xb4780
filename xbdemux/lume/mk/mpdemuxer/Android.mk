LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
MPTOP :=
AUDIO_CODEC:=no
VIDEO_CODEC:=no
MPLAYER_DEMUXER:=yes
SRCS_COMMON:=
SRCS_COMMON-yes:=

LUME_PATH := hardware/xb4780/xbomx/component/dec/lume/
include $(LUME_PATH)/codec_config.mak
include $(LOCAL_PATH)/demuxer.mk

MLOCAL_SRC_FILES := $(SRCS_COMMON)  
LOCAL_SRC_FILES := $(addprefix $(MPTOP),$(MLOCAL_SRC_FILES)) 
LOCAL_MODULE := libstagefright_ffmpdemuxer

JZC_CFG = $(LUME_PATH)/jzconfig.h
LOCAL_CFLAGS := $(PV_CFLAGS) -DNO_FREEP -DHAVE_AV_CONFIG_H -ffunction-sections  -Wmissing-prototypes -Wundef -Wdisabled-optimization -Wno-pointer-sign -Wdeclaration-after-statement -std=gnu99 -Wall -Wno-switch -Wpointer-arith -Wredundant-decls -O2 -pipe -ffast-math -UNDEBUG -UDEBUG -fno-builtin -DMPLAYER_DEMUXER -imacros $(JZC_CFG)

ifeq ($(BOARD_HAS_ISDBT), true)
	LOCAL_CFLAGS += -DBOARD_HAS_ISDBT
endif

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(LUME_PATH)  \
	$(LUME_PATH)/libavutil \
	$(LUME_PATH)/libavcodec \
	$(LUME_PATH)/libmpcodecs \
	$(LUME_PATH)/../include \
	$(LOCAL_PATH)/stream \
	$(LOCAL_PATH)/../../libmpdemux \
	$(LOCAL_PATH)/../../libavformat \
	$(LOCAL_PATH)/../../../include \
	$(TOP)/hardware/xb4780/xbomx/component/dec/include \
	$(TOP)/frameworks/av/include/media/stagefright/

LOCAL_COPY_HEADERS_TO :=

LOCAL_COPY_HEADERS :=

include $(BUILD_STATIC_LIBRARY)
