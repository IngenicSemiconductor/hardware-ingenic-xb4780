LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
MPTOP := libavformat/
#MPLAYER_DEMUXER:=yes

LUME_PATH := hardware/xb4780/xbomx/component/dec/lume/

AUDIO_CODEC:=no
VIDEO_CODEC:=no
SRCS_COMMON-yes:=
CONFIG_COOK_DECODER:=no
include $(LUME_PATH)/codec_config.mak
include $(LOCAL_PATH)/avformat.mk

MLOCAL_SRC_FILES := $(sort $(SRCS_COMMON))
LOCAL_SRC_FILES := $(addprefix $(MPTOP),$(MLOCAL_SRC_FILES))

LOCAL_MODULE := libstagefright_ffavformat
JZC_CFG := jzconfig.h

LOCAL_CFLAGS := -DNO_FREEP -DHAVE_AV_CONFIG_H -ffunction-sections  -Wmissing-prototypes -Wundef -Wdisabled-optimization -Wno-pointer-sign -Wdeclaration-after-statement -std=gnu99 -Wall -Wno-switch -Wpointer-arith -Wredundant-decls -O2 -pipe -ffast-math -UNDEBUG -UDEBUG -fno-builtin -DMPLAYER_DEMUXER -D__LINUX__ -DCONFIG_PARSER -DCONFIG_PARSER_AVFORMAT -DAUDIO_CODEC -imacros $(JZC_CFG)

LOCAL_MXU_CFLAGS = $(LOCAL_CFLAGS)
LOCAL_MXU_AFLAGS = $(LOCAL_CFLAGS)

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(LUME_PATH) \
	$(LUME_PATH)/libavutil \
	$(LOCAL_PATH)/../../libmpdemux \
	$(LUME_PATH)/libavcodec \
	$(LUME_PATH)/libswscale \
	$(LOCAL_PATH)/../../stream \
	$(LUME_PATH)/libmpcodecs \
	$(LOCAL_PATH)/../../libavformat

LOCAL_COPY_HEADERS_TO :=

LOCAL_COPY_HEADERS :=

include $(BUILD_STATIC_LIBRARY)
