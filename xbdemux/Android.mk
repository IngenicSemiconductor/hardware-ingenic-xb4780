ifeq (true,false)
LOCAL_PATH:= $(call my-dir)
# XBOMX_FLAGS := -DPREFETCHER_DEPACK_NAL -D_GNU_SOURCE -DIPU_4780BUG_ALIGN=2048
XBOMX_FLAGS := -D_GNU_SOURCE -DIPU_4780BUG_ALIGN=2048
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                     \
        LUMEExtractor.cpp \
	LUMEStream.cpp \
	LUMERecognizer.cpp \
	LUMEPrefetcher.cpp \
	LUMEDefs.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/./include \
	$(LOCAL_PATH)/lume/stream \
	$(LOCAL_PATH)/lume/libmpdemux \
	$(TOP)/hardware/xb4780/xbomx/component/dec/include \
	$(TOP)/hardware/xb4780/xbomx/component/dec/lume/ \
	$(TOP)/frameworks/av/media/libstagefright/include \
	$(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax

LOCAL_CFLAGS := \
        -DUSE_IPU_THROUGH_MODE $(XBOMX_FLAGS)

LOCAL_SHARED_LIBRARIES :=               \
	libstagefright \
	libstagefright_omx \
	libstagefright_foundation \
	libstagefright_hard_vlume \
	libcutils	\
	libutils \
	libbinder	\
	libdl \
	libui \
	libjzipu \
	libdmmu \
        libz

LOCAL_STATIC_LIBRARIES := \
	libstagefright_vlume_codec \
        libstagefright_ffmpdemuxer \
        libstagefright_ffavformat  \
        libstagefright_alumedecoder \
        libstagefright_ffmpcommon \
        libstagefright_ffavutil  \
        libstagefright_ffavcore

LOCAL_MODULE:= libstagefright_xbdemux

include $(BUILD_SHARED_LIBRARY)

################################################################################

include $(call all-makefiles-under,$(LOCAL_PATH))
endif