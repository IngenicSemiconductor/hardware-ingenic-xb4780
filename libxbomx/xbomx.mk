LOCAL_PATH := hardware/ingenic/xb4780/libxbomx

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/libstagefrighthw.so:system/lib/libstagefrighthw.so           			\
	$(LOCAL_PATH)/libstagefright_hard_alume.so:system/lib/libstagefright_hard_alume.so              \
	$(LOCAL_PATH)/libstagefright_hard_vlume.so:system/lib/libstagefright_hard_vlume.so              \
	$(LOCAL_PATH)/libstagefright_hard_x264hwenc.so:system/lib/libstagefright_hard_x264hwenc.so      \
	$(LOCAL_PATH)/libOMX_Core.so:system/lib/libOMX_Core.so                       			\
	$(LOCAL_PATH)/mpeg4_p1.bin:system/etc/mpeg4_p1.bin

