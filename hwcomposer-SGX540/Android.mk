LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	hwcomposer.cpp

LOCAL_SHARED_LIBRARIES:= \
	libutils \
	liblog \
	libbinder \
	libcutils \
	libhardware \
	libEGL \
	libhardware_legacy \
	libz	\
	libdmmu

LOCAL_CFLAGS += -DCAMERA_INFO_MODULE=\"$(PRODUCT_MODEL)\"
LOCAL_CFLAGS += -DCAMERA_INFO_MANUFACTURER=\"$(PRODUCT_MANUFACTURER)\"

LOCAL_C_INCLUDES += \
	external/jpeg \
	external/jhead \
	hardware/libhardware/include \
	hardware/libhardware/modules/gralloc \
	kernel/driver/misc/jz_x2d \
	$(LOCAL_PATH)/../libdmmu  \
	external/neven/FaceRecEm/common/src/b_FDSDK


#LOCAL_MODULE_PATH := out/target/product/warrior/root
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := hwcomposer.xb4780

include $(BUILD_SHARED_LIBRARY)
