/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2012 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ANDROID_HARDWARE_CAMERAJZPARAMETERS_HARDWARE_H
#define ANDROID_HARDWARE_CAMERAJZPARAMETERS_HARDWARE_H

#include <camera/CameraParameters.h>
#include "CameraDeviceCommon.h"

#define MAX_FACE 3
#define MAX_ZOOM 5
#define MAX_FOCUS 2

namespace android {

    typedef struct Area {
        Rect rect;
        int width;
        int num;
    }Area;

    class ExifElementsTable;

    typedef struct mode_map {
        const char* dsc;
        unsigned short mode;
    }mode_map_t;

    class JZCameraParameters {

    public:
        static const char KEY_LUMA_ADAPTATION[]; 
        static const char KEY_NIGHTSHOT_MODE[];
        static const char KEY_ORIENTATION[];

        static const int num_wb;
        static const mode_map_t wb_map[];
        static const int num_eb;
        static const mode_map_t effect_map[];
        static const int num_ab;
        static const mode_map_t antibanding_map[];
        static const int num_flb;
        static const mode_map_t flash_map[];
        static const int num_sb;
        static const mode_map_t scene_map[];
        static const int num_fb;
        static const mode_map_t focus_map[];
        static const int num_pfb;
        static const mode_map_t pix_format_map[];

    private:
        unsigned short string_to_mode(const char* string, const mode_map_t map_table[], int len) {

            unsigned int i ;
            for (i = 0; i <= (len-1); i++)
                if (strcmp(string, map_table[i].dsc) == 0)
                    return map_table[i].mode;

            ALOGE("%s: %s mode not found", __FUNCTION__, string);
            return -1;
        }

        const char* mode_to_string(unsigned short mode, const mode_map_t map_table[], int len) {

            unsigned int i;
            for (i = 0; i <= (len-1); i++)
                if (mode & map_table[i].mode)
                    return map_table[i].dsc;

            ALOGE("%s: mode %u string not found", __FUNCTION__, mode);
            return NULL;
        }

        char* modes_to_string(unsigned short modes,const mode_map_t map_table[], int len) {

            static char modes_dsc[1024];
            int offset = 0;
            unsigned int i;
              
            memset(modes_dsc, 0, 1024);

            for (i = 0; i <= (len - 1); i++) {
                if (modes & map_table[i].mode) {
                    if (offset == 0)
                        offset += snprintf(modes_dsc, 1024, "%s", map_table[i].dsc);
                    else
                        offset += snprintf(modes_dsc + offset, 1024 - offset, ",%s",map_table[i].dsc);
                }
            }
            modes_dsc[offset+1] = '\0';
            return modes_dsc;
        }

    public:
        JZCameraParameters(CameraDeviceCommon* cdc, int id);
        ~JZCameraParameters();
         
    public:
        int    setParameters(String8 params);
        PixelFormat getFormat(int32_t format);
        void   setPreviewFormat(int* previewformat, const char* valstr);
        bool   is_preview_size_change(void);
        bool   is_video_size_change(void);
        bool   is_picture_size_change(void);
        void   initDefaultParameters(int facing);
        int    getPropertyPictureSize(int* width, int* height);          
        status_t setUpEXIF(ExifElementsTable* exifTable);
        void update_device(CameraDeviceCommon* device) {
            mCameraDevice = device;
        }
        void resetSizeChanged(void) {
            isPreviewSizeChange = false;
            isPictureSizeChange = false;
            isVideoSizeChange = false;
        }

        CameraParameters& getCameraParameters() {
            return mParameters;
        }

    private:
        int  getPropertyValue(const char* property);
        bool if_need_picture_upscale(void);          
        bool isParameterValid(const char *param, const char *supportedParams);
        bool isValidFocusAreas(const char* areas);
        void clearGpsData(void);
    public:
        void getMaxPreviewSize(unsigned int* width, unsigned int* height) {
            if ((maxpreview_width > 0) && (maxpreview_height > 0)) {
                *width = maxpreview_width;
                *height = maxpreview_height;
            } else {
                *width = MIN_WIDTH;
                *height = MIN_HEIGHT;
            }
        }

        void getMaxCaptureSize(unsigned int* width, unsigned int* height) {
            if ((maxcapture_width > 0) && (maxcapture_height > 0)) {
                *width = maxcapture_width;
                *height = maxcapture_height;
            } else {
                *width = MIN_WIDTH;
                *height = MIN_HEIGHT;
            }
        }


    private:
        CameraParameters mParameters;
        CameraDeviceCommon *mCameraDevice;

        mutable Mutex mMutex;
 
        int mCameraId;
        Area mFocusArea[MAX_FOCUS];

        unsigned int maxpreview_width;
        unsigned int maxpreview_height;
        unsigned int maxcapture_width;
        unsigned int maxcapture_height;
    
        bool isPreviewSizeChange;
        bool isPictureSizeChange;
        bool isVideoSizeChange;
    
    };

    //------------- exif class declear ------------

    extern "C" {
#include <jhead.h>
    }

#define MAX_EXIF_TAGS_SUPPORTED 28

    // these have to match strings defined in external/jhead/exif.c
    static const char TAG_MODEL[] = "Model";
    static const char TAG_MAKE[] = "Make";
    static const char TAG_SOFTWARE[] = "Software";
    static const char TAG_FOCALLENGTH[] = "FocalLength";
    static const char TAG_DATETIME[] = "DateTime";
    static const char TAG_IMAGE_WIDTH[] = "ImageWidth";
    static const char TAG_IMAGE_LENGTH[] = "ImageLength";
    static const char TAG_GPS_LAT[] = "GPSLatitude";
    static const char TAG_GPS_LAT_REF[] = "GPSLatitudeRef";
    static const char TAG_GPS_LONG[] = "GPSLongitude";
    static const char TAG_GPS_LONG_REF[] = "GPSLongitudeRef";
    static const char TAG_GPS_ALT[] = "GPSAltitude";
    static const char TAG_GPS_ALT_REF[] = "GPSAltitudeRef";
    static const char TAG_GPS_PROCESSING_METHOD[] = "GPSProcessingMethod";
    static const char TAG_GPS_TIMESTAMP[] = "GPSTimeStamp";
    static const char TAG_GPS_DATESTAMP[] = "GPSDateStamp";
    static const char TAG_ORIENTATION[] = "Orientation";
    static const char TAG_FLASH[] = "Flash";
    static const char TAG_WHITEBALANCE[] = "WhiteBalance";
    static const char TAG_LIGHT_SOURCE[] = "LightSource";
    static const char TAG_METERING_MODE[] = "MeteringMode";
    static const char TAG_EXPOSURE_PROGRAM[] = "ExposureProgram";
    static const char TAG_COLOR_SPACE[] = "ColorSpace";
    static const char TAG_CPRS_BITS_PER_PIXEL[] = "CompressedBitsPerPixel";
    static const char TAG_SENSING_METHOD[] = "SensingMethod";
    static const char TAG_CUSTOM_RENDERED[] = "CustomRendered";

    class ExifElementsTable {
          
    private:
        ExifElement_t table[MAX_EXIF_TAGS_SUPPORTED];
        unsigned int gps_tag_count;
        unsigned int exif_tag_count;
        unsigned int position;
        bool jpeg_opened;
        bool has_datatime_tag;
    public:
        ExifElementsTable():
            gps_tag_count(0),exif_tag_count(0),position(0),
            jpeg_opened(false), has_datatime_tag(false){}
        ~ExifElementsTable();
          
        status_t insertElement(const char* tag, const char* value);
        void insertExifToJpeg(unsigned char* jpeg, size_t jpeg_size);
        status_t insertExifThumbnailImage(const char*, int);
        void saveJpeg(unsigned char* picture, size_t jpeg_size);
        static const char* degreesToExifOrientation(unsigned int);
        static void stringToRational(const char*, unsigned int *, unsigned int *);
        static bool isAsciiTag(const char* tag);   

    };
};

#endif
