LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        HardOMXComponent.cpp \
	SimpleHardOMXComponent.cpp \
	colorconvert/IPUConverter.cpp \
	colorconvert/HWColorConverter.cpp

LOCAL_C_INCLUDES += \
	$(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
	$(TOP)/hardware/ingenic/xb4780/libdmmu \
	$(TOP)/hardware/ingenic/xb4780/libjzipu \
	$(TOP)/hardware/ingenic/xb4780/hwcomposer-SGX540 \
        $(TOP)/hardware/ingenic/xb4780/xbomx/core

LOCAL_SHARED_LIBRARIES :=               \
        libbinder                       \
        libmedia                        \
        libutils                        \
        libui                           \
        libcutils                       \
        libstagefright_foundation       \
        libdl				\
	libjzipu 			\
	libdmmu

LOCAL_MODULE:= libOMX_Basecomponent

include $(BUILD_STATIC_LIBRARY)
