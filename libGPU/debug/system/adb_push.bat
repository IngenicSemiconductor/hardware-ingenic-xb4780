adb remount
adb push lib /system/lib/
adb push bin /system/bin/
adb shell chmod 755 system/bin/pvrsrvctl
adb shell chmod 755 system/bin/hal_client_test
adb shell chmod 755 system/bin/hal_server_test
adb shell chmod 755 system/bin/framebuffer_test
adb shell chmod 755 system/bin/services_test
adb shell chmod 755 system/bin/sgx_flip_test
adb shell chmod 755 system/bin/sgx_init_test
adb shell chmod 755 system/bin/sgx_render_flip_test
adb shell chmod 755 system/bin/testwrap
adb shell chmod 755 system/bin/texture_benchmark

adb shell sync
PAUSE