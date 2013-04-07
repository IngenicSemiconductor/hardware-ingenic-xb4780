/*
 * Camera HAL for Ingenic android 4.2
 *
 * Copyright 2012 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "CameraHalSelector.h"

camera_module_t HAL_MODULE_INFO_SYM = {

common: {
     tag :                 HARDWARE_MODULE_TAG,
     module_api_version :  CAMERA_MODULE_API_VERSION_2_0,
     hal_api_version :     HARDWARE_HAL_API_VERSION,
     id :                  CAMERA_HARDWARE_MODULE_ID,
     name :                "Camera Module",
     author :              "JZ4780",
     methods :             &android::CameraHalSelector::mCameraModuleMethods,
     dso :                 NULL,
     reserved :            {0},
},
     get_number_of_cameras :    android::CameraHalSelector::get_number_of_cameras,
     get_camera_info :          android::CameraHalSelector::get_camera_info
};
