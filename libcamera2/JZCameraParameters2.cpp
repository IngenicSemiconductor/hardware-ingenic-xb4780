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
#define LOG_TAG "JZCameraParameters2"
//#define LOG_NDEBUG 0
#include "JZCameraParameters2.h"

#define ENTRY_CAPACITY 27
#define DATA_CAPACITY 16
#define CONTAINER_CAP 40

namespace android {

    using namespace camera2;

    const char JZCameraParameters2::KEY_LUMA_ADAPTATION[] = "luma-adaptation"; 
    const char JZCameraParameters2::KEY_NIGHTSHOT_MODE[]  = "nightshot-mode";
    const char JZCameraParameters2::KEY_ORIENTATION[]     = "orientation";

    const mode_map_t JZCameraParameters2::wb_map[] = {
        { ANDROID_CONTROL_AWB_AUTO,             WHITE_BALANCE_AUTO },
        { ANDROID_CONTROL_AWB_INCANDESCENT,     WHITE_BALANCE_INCANDESCENT },
        { ANDROID_CONTROL_AWB_FLUORESCENT,      WHITE_BALANCE_FLUORESCENT },
        { ANDROID_CONTROL_AWB_WARM_FLUORESCENT, WHITE_BALANCE_WARM_FLUORESCENT },
        { ANDROID_CONTROL_AWB_DAYLIGHT,         WHITE_BALANCE_DAYLIGHT },
        { ANDROID_CONTROL_AWB_CLOUDY_DAYLIGHT,  WHITE_BALANCE_CLOUDY_DAYLIGHT },
        { ANDROID_CONTROL_AWB_TWILIGHT,         WHITE_BALANCE_TWILIGHT },
        { ANDROID_CONTROL_AWB_SHADE,            WHITE_BALANCE_SHADE }
    };

    const  mode_map_t JZCameraParameters2::effect_map[] = {
        { ANDROID_CONTROL_EFFECT_OFF,        EFFECT_NONE },
        { ANDROID_CONTROL_EFFECT_MONO,       EFFECT_MONO },
        { ANDROID_CONTROL_EFFECT_NEGATIVE,   EFFECT_NEGATIVE },
        { ANDROID_CONTROL_EFFECT_SOLARIZE,   EFFECT_SOLARIZE },
        { ANDROID_CONTROL_EFFECT_SEPIA,      EFFECT_SEPIA },
        { ANDROID_CONTROL_EFFECT_POSTERIZE,  EFFECT_POSTERIZE },
        { ANDROID_CONTROL_EFFECT_WHITEBOARD, EFFECT_WHITEBOARD },
        { ANDROID_CONTROL_EFFECT_BLACKBOARD, EFFECT_BLACKBOARD },
        { ANDROID_CONTROL_EFFECT_AQUA,       EFFECT_AQUA }
        //{ "pastel", EFFECT_PASTEL },
        //{ "mosaic", EFFECT_MOSAIC },
        //{ "resize", EFFECT_RESIZE }
    };

    const mode_map_t JZCameraParameters2::antibanding_map[] = {
        { ANDROID_CONTROL_AE_ANTIBANDING_AUTO, ANTIBANDING_AUTO },
        { ANDROID_CONTROL_AE_ANTIBANDING_50HZ, ANTIBANDING_50HZ },
        { ANDROID_CONTROL_AE_ANTIBANDING_60HZ, ANTIBANDING_60HZ },
        { ANDROID_CONTROL_AE_ANTIBANDING_OFF,  ANTIBANDING_OFF }
    };

    const mode_map_t JZCameraParameters2::scene_map[] = {
        { ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED,     SCENE_MODE_AUTO },
        { ANDROID_CONTROL_SCENE_MODE_ACTION,          SCENE_MODE_ACTION },
        { ANDROID_CONTROL_SCENE_MODE_PORTRAIT,        SCENE_MODE_PORTRAIT },
        { ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,       SCENE_MODE_LANDSCAPE },
        { ANDROID_CONTROL_SCENE_MODE_NIGHT,           SCENE_MODE_NIGHT},
        { ANDROID_CONTROL_SCENE_MODE_NIGHT_PORTRAIT,  SCENE_MODE_NIGHT_PORTRAIT},
        { ANDROID_CONTROL_SCENE_MODE_THEATRE,         SCENE_MODE_THEATRE },
        { ANDROID_CONTROL_SCENE_MODE_BEACH,           SCENE_MODE_BEACH},
        { ANDROID_CONTROL_SCENE_MODE_SNOW,            SCENE_MODE_SNOW },
        { ANDROID_CONTROL_SCENE_MODE_SUNSET,          SCENE_MODE_SUNSET},
        { ANDROID_CONTROL_SCENE_MODE_STEADYPHOTO,     SCENE_MODE_STEADYPHOTO },
        { ANDROID_CONTROL_SCENE_MODE_FIREWORKS,       SCENE_MODE_FIREWORKS},
        { ANDROID_CONTROL_SCENE_MODE_SPORTS,          SCENE_MODE_SPORTS},
        { ANDROID_CONTROL_SCENE_MODE_PARTY,           SCENE_MODE_PARTY},
        { ANDROID_CONTROL_SCENE_MODE_CANDLELIGHT,     SCENE_MODE_CANDLELIGHT},
        { ANDROID_CONTROL_SCENE_MODE_BARCODE,         SCENE_MODE_BARCODE}
    };

    const mode_map_t JZCameraParameters2::flash_map[] = {
        { ANDROID_CONTROL_AE_OFF,                  FLASH_MODE_OFF },
        { ANDROID_CONTROL_AE_ON_AUTO_FLASH,        FLASH_MODE_AUTO },
        { ANDROID_CONTROL_AE_ON,                   FLASH_MODE_ON },
        { ANDROID_CONTROL_AE_ON_AUTO_FLASH_REDEYE, FLASH_MODE_RED_EYE },
        { ANDROID_CONTROL_AE_ON_ALWAYS_FLASH,      FLASH_MODE_ALWAYS },
        { ANDROID_FLASH_TORCH,                     FLASH_MODE_TORCH }
    };

#define ANDROID_CONTROL_AF_INFINITY 6
    const mode_map_t JZCameraParameters2::focus_map[] = {
        { ANDROID_CONTROL_AF_OFF,                FOCUS_MODE_FIXED },
        { ANDROID_CONTROL_AF_AUTO,               FOCUS_MODE_AUTO },
        { ANDROID_CONTROL_AF_MACRO,              FOCUS_MODE_MACRO },
        { ANDROID_CONTROL_AF_CONTINUOUS_VIDEO,   FOCUS_MODE_CONTINUOUS_VIDEO},
        { ANDROID_CONTROL_AF_CONTINUOUS_PICTURE, FOCUS_MODE_CONTINUOUS_PICTURE},
        { ANDROID_CONTROL_AF_INFINITY,           FOCUS_MODE_INFINITY },
        { ANDROID_CONTROL_AF_EDOF,               FOCUS_MODE_EDOF }
    };

    const mode_map_t JZCameraParameters2::pix_format_map[] = {
        { HAL_PIXEL_FORMAT_YCbCr_422_I,  PIXEL_FORMAT_YUV422I},
        { HAL_PIXEL_FORMAT_YCrCb_420_SP, PIXEL_FORMAT_YUV420SP},
        { HAL_PIXEL_FORMAT_YCbCr_422_SP, PIXEL_FORMAT_YUV422SP},
        { HAL_PIXEL_FORMAT_YV12,  PIXEL_FORMAT_YUV420P},
        { HAL_PIXEL_FORMAT_JZ_YUV_420_B, PIXEL_FORMAT_JZ_YUV420T},
        { HAL_PIXEL_FORMAT_JZ_YUV_420_P, PIXEL_FORMAT_JZ_YUV420P}
    };

    unsigned short JZCameraParameters2::tag_to_mode (const int tag, const mode_map_t map_table[], int len) {
        unsigned short mode = map_table[0].mode;

        for (int i = 0; i < len; ++i) {
            if (tag == map_table[i].tag) {
                mode = map_table[i].mode;
            }
        }
        return mode;
    }
   
    int JZCameraParameters2::mode_to_tag (unsigned short mode, const mode_map_t map_table[], int len) {
        int tag = map_table[0].tag;

        for (int i = 0; i < len; ++i) {
            if (mode == map_table[i].mode) {
                tag = map_table[i].tag;
            }
        }
        return tag;
    }

    uint8_t* JZCameraParameters2::getSupportTags (unsigned short modes, const mode_map_t map_table[], 
                                                  int len, int* datalen, const int last_tag) {
        static uint8_t data[18];
        int index = 0;

        memset(data, 16, 0);
        for (int i = 0; i < len; ++i) {
            if (modes & map_table[i].mode) {
                data[index++] = map_table[i].tag;
            }
        }
        if (last_tag != -1) {
            data[index++] = last_tag;
        }
        *datalen = index;
        return data;
    }


    JZCameraParameters2::JZCameraParameters2(CameraDeviceCommon* cdc, int id)
        :mDevice(cdc),
         mCameraId(id),
         mFacing(CAMERA_FACING_BACK),
         mPreviewWidth(640),
         mPreviewHeight(480),
         mCaptureWidth(1600),
         mCaptureHeight(1200),
         mPreviewFormat(PIXEL_FORMAT_JZ_YUV420P),
         mCaptureFormat(PIXEL_FORMAT_YUV422I),
         mPreviewFps(30),
         mwbMode(WHITE_BALANCE_AUTO),
         meffectMode(EFFECT_NONE),
         mantibandingMode(ANTIBANDING_AUTO),
         msceneMode(SCENE_MODE_AUTO),
         mflashMode(FLASH_MODE_OFF),
         mfocusMode(FOCUS_MODE_AUTO),
         mrecordingHint(false),
         mvideoStabilization(false),
         menableFaceDetect(false),
         mCurrentSensorWidth(SENSOR_BACK_WIDTH),
         mCurrentSensorHeight(SENSOR_BACK_HEIGHT),
         mPreviewMetaDataBuffer(NULL),
         mRecordMetaDataBuffer(NULL) {

    }

    JZCameraParameters2::~JZCameraParameters2() {

        if (mPreviewMetaDataBuffer) {
            free_camera_metadata(mPreviewMetaDataBuffer);
            mPreviewMetaDataBuffer = NULL;
        }

        if (mRecordMetaDataBuffer) {
            free_camera_metadata(mRecordMetaDataBuffer);
            mRecordMetaDataBuffer = NULL;
        }

    }

    const camera_metadata_t* JZCameraParameters2::get_camera_metadata(void) {
        ALOGV("%s: Id: %d",__FUNCTION__, mCameraId);
        return mPreviewMetaDataBuffer;
    }

    camera_metadata_t * JZCameraParameters2::getPreviewRequest(void) {
        return mPreviewMetaDataBuffer;
    }

    camera_metadata_t * JZCameraParameters2::getRecordRequest(void) {
        return mRecordMetaDataBuffer;
    }

    //--------------------------------- vendor section -------------------

    /** Custom tag definitions */

    // vendor camera metadata sections
    enum {
        VENDOR_SECTION_FORMAT = VENDOR_SECTION,
        VENDOR_SECTION_JPEG,
        END_VENDOR_SECTIONS,

        VENDOR_FORMAT_START = VENDOR_SECTION_FORMAT << 16,
        VENDOR_JPEG_START = VENDOR_SECTION_JPEG << 16

    };

    // vendor camera metadata tags
    enum {
        //yuv420 tile
        VENDOR_FORMAT_YUV420TILE = VENDOR_FORMAT_START,
        VENDOR_FORMAT_YUV420P,
        VENDOR_FORMAT_END,

        //vendor camer jpeg tags
        VENDOR_JPEG_75 = VENDOR_JPEG_START,
        VENDOR_JPEG_80,
        VENDOR_JPEG_85,
        VENDOR_JPEG_90,
        VENDOR_JPEG_95,
        VENDOR_JPEG_END

    };

    unsigned int vendor_camera_metadata_section_bounds[END_VENDOR_SECTIONS - VENDOR_SECTION][2] = {
        { VENDOR_FORMAT_START, VENDOR_FORMAT_END },
        { VENDOR_JPEG_START, VENDOR_JPEG_END }
    };

    const char *vendor_camera_metadata_section_names[END_VENDOR_SECTIONS - VENDOR_SECTION] = {
        "cn.ingenic.cim.format",
        "cn.ingenic.cim.jpeg"
    };

    typedef struct vendor_camera_tag_info {
        const char *tag_name;
        uint8_t     tag_type;
    } vendor_camera_tag_info_t;

    vendor_camera_tag_info_t vendor_camera_format[VENDOR_FORMAT_END - VENDOR_FORMAT_START] = {
        { "jzyuv420tile", TYPE_INT32 },
        { "jzyuv420p", TYPE_INT32 }
    };

    vendor_camera_tag_info_t vendor_camera_jpeg[VENDOR_JPEG_END - VENDOR_JPEG_START] = {
        { "jzjpeg75", TYPE_INT32 },
        { "jzjpeg80", TYPE_INT32 },
        { "jzjpeg85", TYPE_INT32 },
        { "jzjpeg90", TYPE_INT32 },
        { "jzjpeg95", TYPE_INT32 }
    };

    vendor_camera_tag_info_t *vendor_tag_info[END_VENDOR_SECTIONS - VENDOR_SECTION] = {
        vendor_camera_format,
        vendor_camera_jpeg
    };

    //vendor section ops
    const char* JZCameraParameters2::getVendorSectionName(uint32_t tag) {
        ALOGV("%s", __FUNCTION__);
        uint32_t section = tag >> 16;
        if (section < VENDOR_SECTION || section > END_VENDOR_SECTIONS) return NULL;
        return vendor_camera_metadata_section_names[section - VENDOR_SECTION];
    }

    const char* JZCameraParameters2::getVendorTagName(uint32_t tag) {
        ALOGV("%s", __FUNCTION__);
        uint32_t section = tag >> 16;
        if (section < VENDOR_SECTION || section > END_VENDOR_SECTIONS) return NULL;
        uint32_t section_index = section - VENDOR_SECTION;
        if (tag >= vendor_camera_metadata_section_bounds[section_index][1]) {
            return NULL;
        }
        uint32_t tag_index = tag & 0xFFFF;
        return vendor_tag_info[section_index][tag_index].tag_name;
    }

    int JZCameraParameters2::getVendorTagType(uint32_t tag) {
        ALOGV("%s", __FUNCTION__);
        uint32_t section = tag >> 16;
        if (section < VENDOR_SECTION || section > END_VENDOR_SECTIONS) return -1;
        uint32_t section_index = section - VENDOR_SECTION;
        if (tag >= vendor_camera_metadata_section_bounds[section_index][1]) {
            return -1;
        }
        uint32_t tag_index = tag & 0xFFFF;
        return vendor_tag_info[section_index][tag_index].tag_type;
    }

    //private method
    void JZCameraParameters2::setCommonMode(void) {

        ALOGV("%s: camera %d",__FUNCTION__, mCameraId);

        mDevice->setCommonMode(WHITE_BALANCE, mwbMode);
        mDevice->setCommonMode(EFFECT_MODE, meffectMode);
        mDevice->setCommonMode(ANTIBAND_MODE, mantibandingMode);
        mDevice->setCommonMode(SCENE_MODE, msceneMode);
        mDevice->setCommonMode(FLASH_MODE, mflashMode);
        mDevice->setCommonMode(FLASH_MODE, mfocusMode);

    }

    void JZCameraParameters2::setCameraParameters(void) {
        struct camera_param p;

        memset(&p, 0, sizeof (struct camera_param));

        ALOGV("%s: camera %d",__FUNCTION__, mCameraId);

        p.cmd = CPCMD_SET_RESOLUTION;
        p.param.ptable[0].w = mPreviewWidth;
        p.param.ptable[0].h = mPreviewHeight;
        mDevice->setCameraParam(p, mPreviewFps);
    }

    int JZCameraParameters2::getPropertyValue(const char* property) {
        char prop[16];

        memset(prop, 0, 16);
        if (property_get(property, prop, NULL) > 0) {
            char *prop_end = prop;
            int val = strtol(prop, &prop_end, 10);
            if (*prop_end == '\0') {
                return val;	
            }
        }
        return BAD_VALUE;
    }

    int JZCameraParameters2::getPropertyPictureSize(int* width, int* height) {

        int tmpWidth = 0, tmpHeight = 0;

        if (if_need_picture_upscale()) {

            tmpWidth = getPropertyValue("ro.board.camera.sensor_w");
            tmpHeight = getPropertyValue("ro.board.camera.sensor_h");
            if ((tmpWidth > 0) && (tmpHeight > 0)) {
                *width = tmpWidth;
                *height = tmpHeight;
                return NO_ERROR;
            }
        }

        return BAD_VALUE;
    }

    bool JZCameraParameters2::if_need_picture_upscale(void) {

        const char property1[] = "ro.board.camera.picture_upscale";
        const char property2[] = "ro.board.camera.upscale_id";
        bool ret = false;
        char prop[16];

        memset(prop, 0, 16);

        if (property_get(property1, prop, NULL) > 0) {
            if (strcmp(prop,"true") == 0)
                ret = true;
        }

        int id = getPropertyValue(property2);
        if (id == mCameraId)
            ret = true;

        return ret;
    }

  status_t addOrSize(camera_metadata_t *request,
                     bool sizeRequest,
                     size_t *entryCount,
                     size_t *dataCount,
                     uint32_t tag,
                     const void *entryData,
                     size_t entryDataCount) {
    status_t res;
    if (!sizeRequest) {
      return add_camera_metadata_entry(request, tag, entryData,
                                       entryDataCount);
    } else {
      int type = get_camera_metadata_tag_type(tag);
      if (type < 0 ) return BAD_VALUE;
      (*entryCount)++;
      (*dataCount) += calculate_camera_metadata_entry_data_size(type,
                                                                entryDataCount);
      return OK;
    }
  }

    //Override parent method
    void JZCameraParameters2::initDefaultParameters(int facing) {
        status_t res;

        ALOGV("%s: Id: %d, facing: %d",__FUNCTION__, mCameraId, facing);

        CameraMetadata* tmpInfo = new CameraMetadata(ENTRY_CAPACITY, DATA_CAPACITY);

        //--------------- start construct --------

        struct sensor_info sinfo;
        struct resolution_info rinfo;
        int32_t containers[CONTAINER_CAP];
        float floatcontainers[CONTAINER_CAP];
        uint8_t int8containers[CONTAINER_CAP];
        double doublecontainers[CONTAINER_CAP];
        int datalen = 0;
        int last_tag = 0;
        const uint8_t* int8Data = NULL;
        String8 stringData;

        memset(containers, 0, CONTAINER_CAP*4);
        memset(floatcontainers, 0.0f, CONTAINER_CAP*4);
        memset(int8containers, 0, CONTAINER_CAP);
        memset(doublecontainers, 0, CONTAINER_CAP*4);
        memset(&sinfo, 0, sizeof(struct sensor_info));

        containers[0] = mCameraId ?
           ANDROID_LENS_FACING_FRONT : ANDROID_LENS_FACING_BACK;
        res = tmpInfo->update(ANDROID_LENS_FACING, (const int32_t*) containers, 1);
        if (res != OK) {
          ALOGE("%s: camera %d init camera id error",__FUNCTION__, mCameraId);
          goto update_error;
        }

        //android flash
        uint8_t flashAvailable;
        if (mCameraId == 0) {
          flashAvailable = 1;
        } else {
          flashAvailable = 0;
        }
        res = tmpInfo->update(ANDROID_FLASH_AVAILABLE, (const uint8_t*) &flashAvailable, 1);

        int8containers[0] = ANDROID_STATS_FACE_DETECTION_OFF;
        // int8containers[2] = ANDROID_STATS_FACE_DETECTION_SIMPLE;
        int8containers[1] = ANDROID_STATS_FACE_DETECTION_FULL;
        res = tmpInfo->update(ANDROID_STATS_AVAILABLE_FACE_DETECT_MODES,
                              (const uint8_t *)int8containers, 2);
        if (res != OK) {
            ALOGE("%s: camera %d init face detect mode error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        ALOGV("%s: camera %d init face detect mode %d",__FUNCTION__,
                 mCameraId, int8containers[0]);

        containers[0] = atoi(CAMERA_FACEDETECT);
        res = tmpInfo->update(ANDROID_STATS_MAX_FACE_COUNT, (const int32_t*)containers,
                              1);
        if (res != OK) {
            ALOGE("%s: camera %d init max face conunt error",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        static const int32_t histogramSize = 64;
        res = tmpInfo->update(ANDROID_STATS_HISTOGRAM_BUCKET_COUNT, &histogramSize, 1);
        if (res != OK) {
          ALOGE("%s: camera %d init histogram bucket count error",__FUNCTION__, mCameraId);
          goto update_error;
        }

        static const int32_t maxHistogramCount = 1000;
        res = tmpInfo->update(ANDROID_STATS_MAX_HISTOGRAM_COUNT, &maxHistogramCount, 1);
        if (res != OK) {
          ALOGE("%s: camera %d init max histogram count error",__FUNCTION__, mCameraId);
          goto update_error;
        }
        

        ALOGV("%s: camera %d init max face %d",__FUNCTION__,
                 mCameraId, containers[0]);

        mDevice->getSensorInfo(&sinfo,&rinfo);
        for (size_t i = 0, j = 0; i < CONTAINER_CAP; i+=2) {
            if (j == sinfo.prev_resolution_nr)
                break;
            containers[i] = rinfo.ptable[j].w;
            containers[i+1] = rinfo.ptable[j].h;
            ALOGV("%s: (%d): support preview size: %dx%d",__FUNCTION__, j, rinfo.ptable[i].w, rinfo.ptable[i].h);
            j++;
        }

        if (sinfo.prev_resolution_nr >= 1) {
            res = tmpInfo->update(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES,
                                  (const int32_t *)containers, sinfo.prev_resolution_nr);
            if (res != OK) {
                ALOGE("%s: init camera %d supported preview sizes error.",__FUNCTION__, mCameraId);
                goto update_error;
            }
        }

        mPreviewWidth = containers[0];
        mPreviewHeight = containers[1];
        mPreviewFormat = PIXEL_FORMAT_YUV422I;

        ALOGV("%s: camera %d preview size %dx%d, format %d",__FUNCTION__,
                 mCameraId, mPreviewWidth, mPreviewHeight, mPreviewFormat);

        //build fast info
        //containers[0] = (mCameraId==0) ? SENSOR_BACK_WIDTH : SENSOR__FRONT_WIDTH;
        //containers[1] = (mCameraId==0) ? SENSOR_BACK_HEIGHT : SENSOR_FRONT_HEIGHT;
        res = tmpInfo->update(ANDROID_SENSOR_ACTIVE_ARRAY_SIZE, (const int32_t*)containers, 2);
        if (res != OK) {
            ALOGE("%s: camera %d init array size error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        mCurrentSensorWidth = containers[0];
        mCurrentSensorHeight = containers[1];
        res = tmpInfo->update(ANDROID_SCALER_AVAILABLE_RAW_SIZES, (const int32_t*)containers, 2);
        if (res != OK) {
          ALOGE("%s: camera %d init scaler available raw sizes error",__FUNCTION__, mCameraId);
          goto update_error;
        }

        ALOGV( "%s: camera %d array size %dx%d",__FUNCTION__,
                 mCameraId, containers[0], containers[1]);

        containers[0] = 10;
        containers[1] = 15;
        containers[2] = 15;
        containers[3] = 20;
        containers[4] = 25;
        containers[5] = 30;
        containers[6] = 30;
        containers[7] = 30;
        res = tmpInfo->update(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                              (const int32_t*)containers, 8);
        if (res != OK) {
            ALOGE("%s: init camera %d fps ranges error.",__FUNCTION__, mCameraId);
            goto update_error;
        }

        mPreviewFps = containers[7];

        ALOGV("%s: camera %d fps ranges: %d, %d",__FUNCTION__, mCameraId,
                 containers[0]*1000, containers[1]*1000);

        containers[0] = HAL_PIXEL_FORMAT_YCbCr_422_I;
        containers[1] = HAL_PIXEL_FORMAT_YV12;
        containers[2] = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
        containers[3] = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
        containers[4] = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        containers[5] = HAL_PIXEL_FORMAT_YCbCr_422_SP;
        containers[6] = HAL_PIXEL_FORMAT_RAW_SENSOR;
        res = tmpInfo->update(ANDROID_SCALER_AVAILABLE_FORMATS,
                              (const int32_t*)containers, 7);
        if (res != OK) {
            ALOGE("%s: init camera %d preview format error.",__FUNCTION__, mCameraId);
            goto update_error;
        }

        ALOGV("%s, camera %d preview format: yuv422i, yuv422sp, yuv420p, yuv420B, yuv420sp, jzyuv420p",
                 __FUNCTION__, mCameraId);

        for (size_t i = 0, j = 0; i < CONTAINER_CAP; i += 2) {
            if (j == sinfo.prev_resolution_nr)
                break;
            containers[i] = rinfo.ptable[j].w;
            containers[i+1] = rinfo.ptable[j].h;
            ALOGV("%s: (%d): supported capture size: %dx%d",
                     __FUNCTION__, j, containers[i], containers[i+1]);
            j++;
        }

        if (sinfo.prev_resolution_nr >= 1) {
            res = tmpInfo->update(ANDROID_SCALER_AVAILABLE_JPEG_SIZES,
                                  (const int32_t*)containers, sinfo.prev_resolution_nr);
            if (res != OK) {
                ALOGE("%s: camera %d init capture size error.",__FUNCTION__, mCameraId);
                goto update_error;
            }
        }

        mCaptureWidth = containers[0];
        mCaptureHeight = containers[1];

        ALOGV("%s: camera %d capture size %dx%d",__FUNCTION__, mCameraId,
                 mCaptureWidth, mCaptureHeight);

        containers[0] = 0;
        containers[1] = 0;
        containers[2] = 176;
        containers[3] = 144;
        containers[4] = 320;
        containers[5] = 240;
        res = tmpInfo->update(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
                              (const int32_t*)containers, 6);
        if (res != OK) {
            ALOGE("%s: camera %d init jpeg thumbnailSizes error",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        static const int32_t jpegMaxSize = 10 * 1024 * 1024;
        res = tmpInfo->update(ANDROID_JPEG_MAX_SIZE, &jpegMaxSize, 1);
        if (res != OK) {
          ALOGE("%s: camera %d init max jpeg size error", __FUNCTION__, mCameraId);
          goto update_error;
        }

        mwbMode = WHITE_BALANCE_AUTO;
        int8Data = getSupportTags(sinfo.modes.balance, JZCameraParameters2::wb_map,
                                  WHITE_BALANCEVALUES_NUM, &datalen, ANDROID_CONTROL_AWB_OFF);
        res = tmpInfo->update(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init white balance error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        meffectMode = EFFECT_NONE;
        int8Data = getSupportTags(sinfo.modes.effect, JZCameraParameters2::effect_map,
                                  EFFECTVALUES_NUM, &datalen);
        res = tmpInfo->update(ANDROID_CONTROL_AVAILABLE_EFFECTS,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init effect error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        mantibandingMode = ANTIBANDING_AUTO;
        int8Data = getSupportTags(sinfo.modes.antibanding, JZCameraParameters2::antibanding_map,
                                  ANTIBANVALUES_NUM, &datalen);
        res = tmpInfo->update(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init antibanding mode error", __FUNCTION__, mCameraId);
            goto update_error;
        }
   
        msceneMode = SCENE_MODE_AUTO;
        int8Data = getSupportTags(sinfo.modes.scene_mode, JZCameraParameters2::scene_map,
                                  SCENEVALUES_NUM, &datalen, ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY);
        res = tmpInfo->update(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init scene mode error", __FUNCTION__, mCameraId);
            goto update_error;
        }
   
        containers[0] = 1;
        res = tmpInfo->update(ANDROID_FLASH_AVAILABLE, (const int32_t*)containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init flash available error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        mflashMode = FLASH_MODE_OFF;
        int8Data = getSupportTags(sinfo.modes.flash_mode, JZCameraParameters2::flash_map,
                                  FLASHMODE_NUM, &datalen);
        res = tmpInfo->update(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init flash mode error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        // zhu yi if set 0, focus mode: FOCUS_MODE_FIXED
        containers[0] = 100;
        res = tmpInfo->update(ANDROID_LENS_MINIMUM_FOCUS_DISTANCE, (const int32_t*)containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init focus distance error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        mfocusMode = FOCUS_MODE_AUTO; //FOCUS_MODE_FIXED
        int8Data = getSupportTags(sinfo.modes.focus_mode, JZCameraParameters2::focus_map,
                                  FOCUSMODE_NUM, &datalen, ANDROID_CONTROL_AF_OFF);
        res = tmpInfo->update(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                              (const uint8_t*)int8Data, datalen);
        if (res != OK) {
            ALOGE("%s: camera %d init flash mode error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        containers[0] = 4;
        res = tmpInfo->update(ANDROID_CONTROL_MAX_REGIONS, (const int32_t*)containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init max focus area error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        floatcontainers[0] = 0.55;
        res = tmpInfo->update(ANDROID_LENS_AVAILABLE_FOCAL_LENGTHS, (const float *)floatcontainers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init focal lengths error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        floatcontainers[0] = 1.87;
        floatcontainers[1] = 0.55;
        res = tmpInfo->update(ANDROID_SENSOR_PHYSICAL_SIZE, (const float *)floatcontainers, 2);
        if (res != OK) {
            ALOGE("%s: camera %d init sensor physical size error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        containers[0] = 0;
        containers[1] = 0;
        res = tmpInfo->update(ANDROID_CONTROL_AE_EXP_COMPENSATION_RANGE, (const int32_t *)containers, 2);
        if (res != OK) {
            ALOGE("%s: camera %d init sensor physical size error", __FUNCTION__, mCameraId);
            goto update_error;
        }

        camera_metadata_rational_t tmpRational;
        tmpRational.numerator = 0;
        tmpRational.denominator = 1;
        res = tmpInfo->update(ANDROID_CONTROL_AE_EXP_COMPENSATION_STEP,
                              (const camera_metadata_rational_t *)&tmpRational, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init compensation step error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        floatcontainers[0] = 10;
        res = tmpInfo->update(ANDROID_SCALER_AVAILABLE_MAX_ZOOM, (const float*)floatcontainers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init max digital zoom error",__FUNCTION__, mCameraId);
            goto update_error;
        }

        ALOGV("%s: camera %d scaler aailable max zoom %f",__FUNCTION__,
                 mCameraId, floatcontainers[0]);

        containers[0] = 1;
        res = tmpInfo->update(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                              (const int32_t*)containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d int video stabilization error",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        int8containers[0] = 1;
        res = tmpInfo->update(ANDROID_QUIRKS_TRIGGER_AF_WITH_AUTO,
                              (const uint8_t*)int8containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init trigger af with auto error",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        ALOGV("%s: camera %d init trigger af with auto",__FUNCTION__,
                 mCameraId);

        int8containers[0] = 1;
        res = tmpInfo->update(ANDROID_QUIRKS_USE_ZSL_FORMAT,
                              (const uint8_t*)int8containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init use zsl formaterror",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        ALOGV("%s: camera %d init use zsl format",__FUNCTION__,
                 mCameraId);

        int8containers[0] = 1;
        res = tmpInfo->update(ANDROID_QUIRKS_METERING_CROP_REGION,
                              (const uint8_t*)int8containers, 1);
        if (res != OK) {
            ALOGE("%s: camera %d init metering crop region error",__FUNCTION__,
                  mCameraId);
            goto update_error;
        }

        ALOGV("%s: camera %d init metering crop region",__FUNCTION__,
                 mCameraId);

        ALOGV("%s camera %d init complete, entryCount: %d",__FUNCTION__,
                 mCameraId, tmpInfo->entryCount());

        //-------------- end construct -----------

        setCommonMode();
        setCameraParameters();

        mFacing = facing;
        mrecordingHint = false;
        mvideoStabilization = false;
        menableFaceDetect = false;

        if (mPreviewMetaDataBuffer) {
            free_camera_metadata(mPreviewMetaDataBuffer);
            mPreviewMetaDataBuffer = NULL;
        }

        if (mRecordMetaDataBuffer) {
            free_camera_metadata(mRecordMetaDataBuffer);
            mRecordMetaDataBuffer = NULL;
        }

        tmpInfo->sort();
        mPreviewMetaDataBuffer = tmpInfo->release();
        mRecordMetaDataBuffer = clone_camera_metadata(mPreviewMetaDataBuffer);

    update_error:
        if (tmpInfo) {
            delete tmpInfo;
            tmpInfo = NULL;
        }

        return;
    }

    void JZCameraParameters2::setDataFormat(int srcFormat, int* destFormat) {

        if (mrecordingHint) {
            *destFormat = PIXEL_FORMAT_JZ_YUV420T;
            return;
        }

        switch (srcFormat) {
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            *destFormat = PIXEL_FORMAT_YUV422I;
            break;
        case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
            *destFormat = PIXEL_FORMAT_JZ_YUV420T;
            break;
        default:
            *destFormat = PIXEL_FORMAT_JZ_YUV420P;
            break;
        }

    }

    //Override parent method
    int JZCameraParameters2::setParameters2(camera_metadata_t* params) {

        ALOGV("%s: Id: %d, facing: %d",__FUNCTION__, mCameraId, mFacing);

        status_t ret = NO_ERROR;
        String8 tmpParameters;

        return ret;
    }

    //Override parent method
    status_t JZCameraParameters2::setUpEXIF(ExifElementsTable* exifTable) {

        ALOGV("%s: Id: %d, facing: %d",__FUNCTION__, mCameraId, mFacing);

        status_t ret = NO_ERROR;

        return ret;
    }

    void JZCameraParameters2::dump(int fd, StreamType type, String8& result) {
        result.appendFormat("        Stream %d: preview size: %dx%d, preview format: 0x%x, capture size: %dx%d, capture format 0x%x",
                            mCameraId, mPreviewWidth, mPreviewHeight, mPreviewFormat,
                            mCaptureWidth, mCaptureHeight, mCaptureFormat);
        if (type == RECORD)
            dump_indented_camera_metadata(mRecordMetaDataBuffer, fd, 1, 0);

        if (type == PREVIEW)
            dump_indented_camera_metadata(mPreviewMetaDataBuffer, fd, 1, 0);
    }

  bool JZCameraParameters2::isSupportedResolution(int width, int height) {
    unsigned int i = 0;
    camera_metadata_ro_entry_t availablePreviewSizes =
      staticInfo(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES);
    for (i = 0; i < availablePreviewSizes.count; i += 2) {
      if ((availablePreviewSizes.data.i32[i] == width) &&
          (availablePreviewSizes.data.i32[i+1] == height))
        return true;
    }
    ALOGE("%s: Requested preview size %dx%d is not supported",
          __FUNCTION__, width, height);
    return false;
  }

  bool JZCameraParameters2::isSupportedJpegResolution(int width, int height) {
    unsigned int i = 0;
    camera_metadata_ro_entry_t availablePictureSizes =
      staticInfo(ANDROID_SCALER_AVAILABLE_JPEG_SIZES);
    for (i = 0; i < availablePictureSizes.count; i+=2) {
      if ((availablePictureSizes.data.i32[i] == width) &&
          (availablePictureSizes.data.i32[i+1] == height))
        return true;
    }
    ALOGE("%s: Requested picture size %dx%d is not supported",
          __FUNCTION__, width, height);
    return false;
  }

  camera_metadata_ro_entry_t JZCameraParameters2::staticInfo (uint32_t tag,
           size_t minCount, size_t maxCount) const {

    status_t res;
    camera_metadata_ro_entry_t entry = find(tag);

    if (CC_UNLIKELY(entry.count == 0)) {
      const char* tagSection = get_camera_metadata_section_name (tag);
      if (tagSection == NULL) tagSection = "<unknown>";
      const char* tagName = get_camera_metadata_tag_name(tag);
      if (tagName == NULL) tagName = "<unknown>";
      ALOGE("Error finding static metadata entry '%s.%s' (%x)",
            tagSection, tagName, tag);
    } else if (CC_UNLIKELY (
              (minCount != 0 && entry.count < minCount) ||
              (maxCount != 0 && entry.count > maxCount))) {
      const char* tagSection = get_camera_metadata_section_name(tag);
      if (tagSection == NULL) tagSection = "<unknown>";
      const char* tagName = get_camera_metadata_tag_name(tag);
      if (tagName == NULL) tagName = "<unknown>";
      ALOGE("Malformed static metadata entry '%s.%s' (%x):"
            "Expected between %d and %d values, but got %d values",
            tagSection, tagName, tag, minCount, maxCount, entry.count);
    }
    return entry;
  }

  camera_metadata_entry_t JZCameraParameters2::find(uint32_t tag) {
    status_t res;
    camera_metadata_entry entry;
    res = find_camera_metadata_entry(mPreviewMetaDataBuffer, tag, &entry);
    if (CC_UNLIKELY( res != OK )) {
      entry.count = 0;
      entry.data.u8 = NULL;
    }
    return entry;
  }

  camera_metadata_ro_entry_t JZCameraParameters2::find(uint32_t tag) const {
    status_t res;
    camera_metadata_ro_entry entry;
    res = find_camera_metadata_ro_entry(mPreviewMetaDataBuffer, tag, &entry);
    if (CC_UNLIKELY( res != OK )) {
      entry.count = 0;
      entry.data.u8 = NULL;
    }
    return entry;
  }

    status_t JZCameraParameters2::constructDefaultRequest(int request_template,
             camera_metadata_t **request, bool sizeRequest) {

    size_t entryCount = 0;
    size_t dataCount = 0;
    status_t ret;

#define ADD_OR_SIZE( tag, data, count ) \
    if ( ( ret = addOrSize(*request, sizeRequest, &entryCount, &dataCount, \
            tag, data, count) ) != OK ) return ret

    static const int64_t USEC = 1000LL;
    static const int64_t MSEC = USEC * 1000LL;
    static const int64_t SEC = MSEC * 1000LL;

    /** android.request */

    static const uint8_t requestType = ANDROID_REQUEST_TYPE_CAPTURE;
    ADD_OR_SIZE(ANDROID_REQUEST_TYPE, &requestType, 1);

    static const uint8_t metadataMode = ANDROID_REQUEST_METADATA_FULL;
    ADD_OR_SIZE(ANDROID_REQUEST_METADATA_MODE, &metadataMode, 1);

    static const int32_t id = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_ID, &id, 1);

    static const int32_t frameCount = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_FRAME_COUNT, &frameCount, 1);

    // OUTPUT_STREAMS set by user
    entryCount += 1;
    dataCount += 5; // TODO: Should be maximum stream number

   /** android.lens */

    static const float focusDistance = 0;
    ADD_OR_SIZE(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);

    static const float aperture = 2.8f;
    ADD_OR_SIZE(ANDROID_LENS_APERTURE, &aperture, 1);

    static const float focalLength = 5.0f;
    ADD_OR_SIZE(ANDROID_LENS_FOCAL_LENGTH, &focalLength, 1);

    static const float filterDensity = 0;
    ADD_OR_SIZE(ANDROID_LENS_FILTER_DENSITY, &filterDensity, 1);

    static const uint8_t opticalStabilizationMode =
            ANDROID_LENS_OPTICAL_STABILIZATION_OFF;
    ADD_OR_SIZE(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
            &opticalStabilizationMode, 1);

    /** android.sensor */

    static const int64_t exposureTime = 10 * MSEC;
    ADD_OR_SIZE(ANDROID_SENSOR_EXPOSURE_TIME, &exposureTime, 1);

    static const int64_t frameDuration = 33333333L; // 1/30 s
    ADD_OR_SIZE(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);

    static const int32_t sensitivity = 100;
    ADD_OR_SIZE(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);

    /** android.flash */

    static const uint8_t flashMode = ANDROID_FLASH_OFF;
    ADD_OR_SIZE(ANDROID_FLASH_MODE, &flashMode, 1);

    static const uint8_t flashPower = 10;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_POWER, &flashPower, 1);

    static const int64_t firingTime = 0;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_TIME, &firingTime, 1);

/** Processing block modes */
    uint8_t hotPixelMode = 0;
    uint8_t demosaicMode = 0;
    uint8_t noiseMode = 0;
    uint8_t shadingMode = 0;
    uint8_t geometricMode = 0;
    uint8_t colorMode = 0;
    uint8_t tonemapMode = 0;
    uint8_t edgeMode = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      default:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
    }
    ADD_OR_SIZE(ANDROID_HOT_PIXEL_MODE, &hotPixelMode, 1);
    ADD_OR_SIZE(ANDROID_DEMOSAIC_MODE, &demosaicMode, 1);
    ADD_OR_SIZE(ANDROID_NOISE_MODE, &noiseMode, 1);
    ADD_OR_SIZE(ANDROID_SHADING_MODE, &shadingMode, 1);
    ADD_OR_SIZE(ANDROID_GEOMETRIC_MODE, &geometricMode, 1);
    ADD_OR_SIZE(ANDROID_COLOR_MODE, &colorMode, 1);
    ADD_OR_SIZE(ANDROID_TONEMAP_MODE, &tonemapMode, 1);
    ADD_OR_SIZE(ANDROID_EDGE_MODE, &edgeMode, 1);

/** android.noise */
    static const uint8_t noiseStrength = 5;
    ADD_OR_SIZE(ANDROID_NOISE_STRENGTH, &noiseStrength, 1);

    /** android.color */
    static const float colorTransform[9] = {
        1.0f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_COLOR_TRANSFORM, colorTransform, 9);

    /** android.tonemap */
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_RED, tonemapCurve, 4);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_GREEN, tonemapCurve, 4);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_BLUE, tonemapCurve, 4);

  /** android.edge */
    static const uint8_t edgeStrength = 5;
    ADD_OR_SIZE(ANDROID_EDGE_STRENGTH, &edgeStrength, 1);

  /** android.scaler */
    int32_t cropRegion[3] = {
        0, 0, mPreviewWidth
    };
    ADD_OR_SIZE(ANDROID_SCALER_CROP_REGION, cropRegion, 3);

    /** android.jpeg */
    static const int32_t jpegQuality = 80;
    ADD_OR_SIZE(ANDROID_JPEG_QUALITY, &jpegQuality, 1);

    static const int32_t thumbnailSize[2] = {
      176, 144
    };
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);

   static const int32_t thumbnailQuality = 80;
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnailQuality, 1);

  static const double gpsCoordinates[3] = {
        0, 0, 0
    };
    ADD_OR_SIZE(ANDROID_JPEG_GPS_COORDINATES, gpsCoordinates, 3);

    static const uint8_t gpsProcessingMethod[32] = "None";
    ADD_OR_SIZE(ANDROID_JPEG_GPS_PROCESSING_METHOD, gpsProcessingMethod, 32);

    static const int64_t gpsTimestamp = 0;
    ADD_OR_SIZE(ANDROID_JPEG_GPS_TIMESTAMP, &gpsTimestamp, 1);

    static const int32_t jpegOrientation = 0;
    ADD_OR_SIZE(ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    /** android.stats */

    static const uint8_t faceDetectMode = ANDROID_STATS_FACE_DETECTION_OFF;
    ADD_OR_SIZE(ANDROID_STATS_FACE_DETECT_MODE, &faceDetectMode, 1);

    static const uint8_t histogramMode = ANDROID_STATS_OFF;
    ADD_OR_SIZE(ANDROID_STATS_HISTOGRAM_MODE, &histogramMode, 1);

    static const uint8_t sharpnessMapMode = ANDROID_STATS_OFF;
    ADD_OR_SIZE(ANDROID_STATS_SHARPNESS_MAP_MODE, &sharpnessMapMode, 1);

    // faceRectangles, faceScores, faceLandmarks, faceIds, histogram,
    // sharpnessMap only in frames

/** android.control */

    uint8_t controlIntent = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        controlIntent = ANDROID_CONTROL_INTENT_PREVIEW;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        controlIntent = ANDROID_CONTROL_INTENT_STILL_CAPTURE;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        controlIntent = ANDROID_CONTROL_INTENT_VIDEO_RECORD;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        controlIntent = ANDROID_CONTROL_INTENT_VIDEO_SNAPSHOT;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        controlIntent = ANDROID_CONTROL_INTENT_ZERO_SHUTTER_LAG;
        break;
      default:
        controlIntent = ANDROID_CONTROL_INTENT_CUSTOM;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_CAPTURE_INTENT, &controlIntent, 1);

    static const uint8_t controlMode = ANDROID_CONTROL_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_MODE, &controlMode, 1);

    static const uint8_t effectMode = ANDROID_CONTROL_EFFECT_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    //ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY;
    static const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED;
    ADD_OR_SIZE(ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    static const uint8_t aeMode = ANDROID_CONTROL_AE_ON_AUTO_FLASH;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_MODE, &aeMode, 1);

    static const uint8_t aeLock = ANDROID_CONTROL_AE_LOCK_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_LOCK, &aeLock, 1);

    static const int32_t controlRegions[5] = {
        0, 0, mPreviewWidth, mPreviewHeight, 1000
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_REGIONS, controlRegions, 5);

   static const int32_t aeExpCompensation = 0;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_EXP_COMPENSATION, &aeExpCompensation, 1);

    static const int32_t aeTargetFpsRange[2] = {
        10, 30
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, aeTargetFpsRange, 2);

    static const uint8_t aeAntibandingMode =
            ANDROID_CONTROL_AE_ANTIBANDING_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &aeAntibandingMode, 1);

    static const uint8_t awbMode =
            ANDROID_CONTROL_AWB_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    static const uint8_t awbLock = ANDROID_CONTROL_AWB_LOCK_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_AWB_LOCK, &awbLock, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AWB_REGIONS, controlRegions, 5);

   uint8_t afMode = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_PICTURE;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_PICTURE;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_VIDEO;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_VIDEO;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_PICTURE;
        break;
      default:
        afMode = ANDROID_CONTROL_AF_AUTO;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_AF_MODE, &afMode, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AF_REGIONS, controlRegions, 5);

    static const uint8_t vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstabMode, 1);

    // aeState, awbState, afState only in frame

    /** Allocate metadata if sizing */
    if (sizeRequest) {
        ALOGV("Allocating %d entries, %d extra bytes for "
                "request template type %d",
                entryCount, dataCount, request_template);
        *request = allocate_camera_metadata(entryCount, dataCount);
        if (*request == NULL) {
            ALOGE("Unable to allocate new request template type %d "
                    "(%d entries, %d bytes extra data)", request_template,
                    entryCount, dataCount);
            return NO_MEMORY;
        }
    }
    return OK;
#undef ADD_OR_SIZE
    }

    //---------------- exif class implement------------------------

    struct integer_string_pair {
        unsigned int integer;
        const char* string;
    };

    static integer_string_pair degrees_to_exif_lut[] = {
        {0,   "1"},
        {90,  "6"},
        {180, "3"},
        {270, "8"},
    };

    const char* ExifElementsTable::degreesToExifOrientation(unsigned int degrees) {

        for (int i=0; i < 4; ++i) {

            if (degrees == degrees_to_exif_lut[i].integer)
                return degrees_to_exif_lut[i].string;
        }
        return NULL;
    }

    void ExifElementsTable::stringToRational(const char* str, unsigned int* num, unsigned int* den) {

        int len;
        char* tmpVal = NULL;

        if (str != NULL) {

            len = strlen(str);
            tmpVal = (char*) malloc(sizeof(char) * (len + 1));
        }
        
        if (tmpVal != NULL) {

            size_t den_len;                
            char *ctx;
            unsigned int numerator = 0;
            unsigned int denominator = 0;
            char* temp = NULL;
                
            memset(tmpVal, '\0', len + 1);
            strncpy (tmpVal, str, len);
            temp = strtok_r(tmpVal, ".", &ctx);
                
            if (temp != NULL)
                numerator = atoi(temp);
            if (!numerator)
                numerator = 1;
                
            temp = strtok_r(NULL, ".", &ctx);
            if (temp != NULL) {

                den_len = strlen(temp);
                if (den_len == 255)
                    den_len = 0;
                denominator = static_cast<unsigned int>(pow((double)10, (int)den_len));
                numerator = numerator * denominator + atoi(temp);
            } else {
                denominator = 1;
            }
            free(tmpVal);
            *num = numerator;
            *den = denominator;
        }
    }

    bool ExifElementsTable::isAsciiTag(const char* tag) {

        return (strcmp(tag, TAG_GPS_PROCESSING_METHOD) == 0);
    }

    void ExifElementsTable::insertExifToJpeg(unsigned char* jpeg, size_t jpeg_size) {

        ReadMode_t read_mode = (ReadMode_t)(READ_METADATA | READ_IMAGE);

        ResetJpgfile();
        if (ReadJpegSectionsFromBuffer(jpeg, jpeg_size, read_mode)) {

            jpeg_opened = true;
            create_EXIF(table, exif_tag_count, gps_tag_count, has_datatime_tag);
        }
    }

    status_t ExifElementsTable::insertExifThumbnailImage(const char* thumb, int len) {

        status_t ret = NO_ERROR;

        if ((len > 0) && jpeg_opened) {

            ret = ReplaceThumbnailFromBuffer(thumb, len);
        }
        return ret;
    }

    void ExifElementsTable::saveJpeg(unsigned char* jpeg, size_t jpeg_size) {

        if (jpeg_opened) {

            WriteJpegToBuffer(jpeg, jpeg_size);
            DiscardData();
            jpeg_opened = false;
        }
    }

    ExifElementsTable::~ExifElementsTable() {

        int num_elements = gps_tag_count + exif_tag_count;
        
        for (int i = 0; i < num_elements; i++) {

            if (table[i].Value) {

                free(table[i].Value);
                table[i].Value = NULL;
            }
        }

        if (jpeg_opened) {

            DiscardData();
        }
    }

    status_t ExifElementsTable::insertElement(const char* tag, const char* value) {

        unsigned int value_length = 0;
        status_t ret = NO_ERROR;

        if (!value || !tag) {
            return BAD_VALUE;
        }

        if (position >= MAX_EXIF_TAGS_SUPPORTED) {

            return NO_MEMORY;
        }

        if (isAsciiTag(tag)) {
            value_length = sizeof (ExifAsciiPrefix) + strlen(value + sizeof(ExifAsciiPrefix));
        } else {
            value_length = strlen(value);
        }

        if (IsGpsTag(tag)) {

            table[position].GpsTag = TRUE;
            table[position].Tag = GpsTagNameToValue(tag);
            gps_tag_count++;
        } else {
            table[position].GpsTag = FALSE;
            table[position].Tag = TagNameToValue(tag);
            exif_tag_count++;

            if (strcmp(tag, TAG_DATETIME) == 0)
                has_datatime_tag = true;
        }

        table[position].DataLength = 0;
        table[position].Value = (char*) malloc(sizeof(char) * (value_length + 1));
        if (table[position].Value) {
            memcpy(table[position].Value, value, value_length+1);
            table[position].DataLength = value_length + 1;
        }
        position++;
        return ret;
    }

}//end namespace
