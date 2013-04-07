
GPU_SGX540_LOCAL_PATH := hardware/ingenic/xb4780/libGPU

BOARD_EGL_CFG := $(GPU_SGX540_LOCAL_PATH)/egl/egl.cfg

PRODUCT_COPY_FILES += \
	$(GPU_SGX540_LOCAL_PATH)/libglslcompiler.so:system/lib/libglslcompiler.so           \
	$(GPU_SGX540_LOCAL_PATH)/libIMGegl.so:system/lib/libIMGegl.so                       \
	$(GPU_SGX540_LOCAL_PATH)/libpvr2d.so:system/lib/libpvr2d.so                         \
	$(GPU_SGX540_LOCAL_PATH)/libpvrANDROID_WSEGL.so:system/lib/libpvrANDROID_WSEGL.so   \
	$(GPU_SGX540_LOCAL_PATH)/libPVRScopeServices.so:system/lib/libPVRScopeServices.so   \
	$(GPU_SGX540_LOCAL_PATH)/libsrv_init.so:system/lib/libsrv_init.so                   \
	$(GPU_SGX540_LOCAL_PATH)/libsrv_um.so:system/lib/libsrv_um.so                       \
	$(GPU_SGX540_LOCAL_PATH)/libusc.so:system/lib/libusc.so                             \
	$(GPU_SGX540_LOCAL_PATH)/hw/gralloc.xxx.so:system/lib/hw/gralloc.xb4780.so                              \
	$(GPU_SGX540_LOCAL_PATH)/egl/libEGL_POWERVR_SGX540_130.so:system/lib/egl/libEGL_POWERVR_SGX540_130.so              \
	$(GPU_SGX540_LOCAL_PATH)/egl/libGLESv1_CM_POWERVR_SGX540_130.so:system/lib/egl/libGLESv1_CM_POWERVR_SGX540_130.so  \
	$(GPU_SGX540_LOCAL_PATH)/egl/libGLESv2_POWERVR_SGX540_130.so:system/lib/egl/libGLESv2_POWERVR_SGX540_130.so        \
	$(GPU_SGX540_LOCAL_PATH)/pvrsrvctl:system/bin/pvrsrvctl

