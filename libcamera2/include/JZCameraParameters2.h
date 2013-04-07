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

#ifndef ANDROID_HARDWARE_CAMERAJZPARAMETERS2_HARDWARE_H
#define ANDROID_HARDWARE_CAMERAJZPARAMETERS2_HARDWARE_H

#include <camera2/CameraMetadata.h>
#include "CameraDeviceCommon.h"

#define SENSOR_BACK_WIDTH 1600
#define SENSOR_BACK_HEIGHT 1200

#define SENSOR__FRONT_WIDTH 640
#define SENSOR_FRONT_HEIGHT 480

namespace android {

    class ExifElementsTable;

    typedef struct mode_map {
        const int      tag;
        unsigned short mode;
    }mode_map_t;

    enum StreamType {
        NONE,
        PREVIEW,
        RECORD
    };
 
    class JZCameraParameters2 {

    private:

        static const char KEY_LUMA_ADAPTATION[]; 
        static const char KEY_NIGHTSHOT_MODE[];
        static const char KEY_ORIENTATION[];

        static const mode_map_t wb_map[];
        static const mode_map_t effect_map[];
        static const mode_map_t antibanding_map[];
        static const mode_map_t flash_map[];
        static const mode_map_t scene_map[];
        static const mode_map_t focus_map[];
        static const mode_map_t pix_format_map[];

        unsigned short tag_to_mode (const int tag, const mode_map_t map_table[], int len);
        int mode_to_tag (unsigned short mode, const mode_map_t map_table[], int len);
        uint8_t* getSupportTags (unsigned short modes, const mode_map_t map_table[], 
                                 int len, int* datalen, int last_tag = -1);

        void setDataFormat(int srcFormat, int* destFormat);
        void setCommonMode(void);
        void setCameraParameters(void);
        bool if_need_picture_upscale(void);
        int getPropertyValue(const char* property);

    public:

        JZCameraParameters2(CameraDeviceCommon* cdc, int id);

        virtual ~JZCameraParameters2();

        const camera_metadata_t *get_camera_metadata(void);

        camera_metadata_t * getPreviewRequest(void);

        camera_metadata_t * getRecordRequest(void);

        const char* getVendorSectionName(uint32_t tag);

        const char* getVendorTagName(uint32_t tag);

        int getVendorTagType(uint32_t tag);

        void initDefaultParameters(int facing);

        int setParameters2(camera_metadata_t* params);

        status_t setUpEXIF(ExifElementsTable* exifTable);

        int getPropertyPictureSize(int* width, int* height);

        void dump(int fd, StreamType type, String8& result);

        bool isSupportedResolution(int width, int height);

        bool isSupportedJpegResolution(int width, int height);
   
        camera_metadata_ro_entry_t staticInfo(uint32_t tag,
            size_t minCount=0, size_t maxCount=0) const;

        camera_metadata_entry find(uint32_t tag);
 
        camera_metadata_ro_entry find(uint32_t tag) const;

        bool isAvaliableSensorSize(int width, int height) {
            return (width==mCurrentSensorWidth) && (height==mCurrentSensorHeight);
        }

        bool isAvaliablePreviewFormat(int format) {
            return format == mPreviewFormat;
        }

       status_t constructDefaultRequest(int request_template,
          camera_metadata_t **request, bool sizeRequest);

    private:
        mutable Mutex mLock;
        CameraDeviceCommon* mDevice;
        int mCameraId;
        int mFacing;
        int mPreviewWidth;
        int mPreviewHeight;
        int mCaptureWidth;
        int mCaptureHeight;
        int mPreviewFormat;
        int mCaptureFormat;
        int mPreviewFps;
        int mwbMode;
        int meffectMode;
        int mantibandingMode;
        int msceneMode;
        int mflashMode;
        int mfocusMode;
        bool mrecordingHint;
        bool mvideoStabilization;
        bool menableFaceDetect;
        int mCurrentSensorWidth;
        int mCurrentSensorHeight;

        camera_metadata_t * mPreviewMetaDataBuffer;
        camera_metadata_t * mRecordMetaDataBuffer;

    };

    //------------------------- exif ----------------

#define MAX_EXIF_TAGS_SUPPORTED 28

    extern "C" {
#include <jhead.h>
    }

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
