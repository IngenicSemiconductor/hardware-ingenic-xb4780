/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2011 Ingenic Semiconductor LTD.
 *
 * author:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_TAG "CameraCompressor"
//#define LOG_NDEBUG 0
#define DEBUG_COMPRESSOR 0

#include "CameraCompressor.h"

namespace android {

    CameraCompressor::CameraCompressor(compress_params_t* yuvImage,bool mirror,int rot) {

        if (mirror) {
            if (rot == 90 || rot == 270) {
                yuyv_upturn(yuvImage->src,yuvImage->pictureWidth,yuvImage->pictureHeight);
            } else if (rot == 0 || rot == 180) {
                yuyv_mirror(yuvImage->src,yuvImage->pictureWidth,yuvImage->pictureHeight);
            }
        }

        mSrc = yuvImage->src;
        mPictureWidth = yuvImage->pictureWidth;
        mPictureHeight = yuvImage->pictureHeight;

        if (yuvImage->pictureQuality <= 0 || yuvImage->pictureQuality > 100)
            mPictureQuality = 90;
        else
            mPictureQuality = yuvImage->pictureQuality;

        mThumbnailWidth = yuvImage->thumbnailWidth;
        mThumbnailHeight = yuvImage->thumbnailHeight;

        if (yuvImage->thumbnailQuality <= 0 || yuvImage->thumbnailQuality > 100)
            mThumbnailQuality = 90;
        else
            mThumbnailQuality = yuvImage->thumbnailQuality;

        mFormat = yuvImage->format;
        mRequiredMem = yuvImage->requiredMem;
        mStrides[0] = 0;
        mStrides[1] = 0;

        if (mPictureWidth >0 && mPictureHeight >0)
            mEncoder = YuvToJpegEncoder::create(mFormat, mStrides);
        else
            mEncoder = NULL;
    }


    status_t CameraCompressor::compressRawImage(int width, int height, int quality) {

        ALOGV("%s: %p[%dx%d]", __FUNCTION__, mSrc, width, height);

        if (mSrc == NULL) {
            ALOGE("%s: stream cannot be null", __FUNCTION__);
            return BAD_VALUE;
        }

        uint8_t* pY = mSrc;
        int offsets[2];
        int tmpWidth = width & (~1);
        int tmpHeight = height & (~1);

        if (mFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
            if (tmpHeight % 8 != 0) {
                tmpHeight -= (tmpHeight % 8);
            }
            offsets[0] = 0;
            offsets[1] = tmpWidth * tmpHeight;
            mStrides[0] = tmpWidth;
            mStrides[1] = tmpWidth;
        } else if (mFormat == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            if (tmpHeight % 16 != 0) {
                tmpHeight -= (tmpHeight % 16);
            }
            mStrides[0] = 2 * tmpWidth;
            mStrides[1] = 0;
            offsets[0] = 0;
            offsets[1] = 0;
        } else {
            ALOGE("%s: don't support this format : %d",
                  __FUNCTION__, mFormat);
            return BAD_VALUE;
        }

        if ((NULL != mEncoder) && (mEncoder->encode(&mStream, (void*)pY, 
                                                    tmpWidth, tmpHeight, offsets, quality))) {
            ALOGV("%s: Compressed JPEG: %d[%dx%d] -> %d bytes",
                  __FUNCTION__, (tmpWidth * tmpHeight * 12)/8, tmpWidth, tmpHeight, mStream.getOffset());
            return NO_ERROR;
        } else {
            ALOGE("%s: JPEG compression failed, mEncoder = %s", __FUNCTION__, mEncoder?"not null":"NULL");
            return errno ? errno : EINVAL;
        }
    }

    status_t CameraCompressor::compress_to_jpeg(ExifElementsTable* exif,camera_memory_t** jpegMem) {
        status_t ret = NO_ERROR;
        camera_memory_t* picJpegMem = NULL;
        camera_memory_t* thumbJpegMem = NULL;
        size_t thumb_size = 0;
        size_t jpeg_size = 0;

        /* First, yuv imge -> thumbnail picuture */
        if (mThumbnailWidth*mThumbnailHeight > 0) {
            ret = compressRawImage(mThumbnailWidth, mThumbnailHeight, mThumbnailQuality);
            if (ret != 0) {
                ALOGE("%s: create thumbnail jpeg fail, errno: %d -> %s",
                      __FUNCTION__, errno, strerror(errno));
                return ret;
            }

            thumb_size = getCompressedSize();
            thumbJpegMem = mRequiredMem(-1, thumb_size, 1, NULL);
            if (NULL !=  thumbJpegMem && thumbJpegMem->data !=NULL) {
                getCompressedImage(thumbJpegMem->data);
                resetSkstream();
            } else {
                ALOGE("%s: creat pic jpeg mem fail, errno: %d -> %s",
                      __FUNCTION__, errno, strerror(errno));
                ret = NO_MEMORY;
                goto fail;
            }
        }

        /* second, yuv imge ->  jpeg picture */
        ret = compressRawImage(mPictureWidth, mPictureHeight, mPictureQuality);
        if (ret != 0) {
            ALOGE("%s: create picture jpeg fail, errno: %d -> %s",
                  __FUNCTION__, errno, strerror(errno));
            goto fail;
        }

        jpeg_size = getCompressedSize();
        picJpegMem = mRequiredMem(-1, (jpeg_size + thumb_size*2), 1, NULL);
        if (NULL !=  picJpegMem && picJpegMem->data != NULL) {
            getCompressedImage(picJpegMem->data);
            resetSkstream();
        } else {
            ALOGE("%s: creat pic jpeg mem fail, errno: %d -> %s",
                  __FUNCTION__, errno, strerror(errno));
            ret = NO_MEMORY;
            goto fail;
        }
        
        /* third, insert exif -> jpeg picture */
        if (NULL != exif) {
            Section_t* exif_section = NULL;
            if (NULL != picJpegMem && picJpegMem->data != NULL && jpeg_size > 0) {
                /* fourth, insert exif thumnail image */
                exif->insertExifToJpeg((unsigned char*)(picJpegMem->data),jpeg_size);
                if (NULL != thumbJpegMem && thumbJpegMem->data != NULL && thumb_size > 0) {
                    exif->insertExifThumbnailImage((const char*)(thumbJpegMem->data), (int)thumb_size);
                }

                exif_section = FindSection(M_EXIF);
                if (NULL != exif_section) {
                    *jpegMem = mRequiredMem(-1, (jpeg_size + exif_section->Size), 1, NULL);
                    if (NULL != (*jpegMem) && (*jpegMem)->data) {
                        exif->saveJpeg((unsigned char*)((*jpegMem)->data),(jpeg_size + exif_section->Size));
                    }
                } 
            }
            delete exif;
            exif = NULL;
        } else {
            *jpegMem = mRequiredMem(-1, jpeg_size,1,NULL);
            if ((*jpegMem) && (*jpegMem)->data)
                {
                    memcpy((*jpegMem)->data, picJpegMem->data, jpeg_size);
                }
        }

    fail:
        if (NULL != picJpegMem && picJpegMem->data != NULL)
            {
                picJpegMem->release(picJpegMem);
            }

        if (thumbJpegMem != NULL && thumbJpegMem->data != NULL)
            {
                thumbJpegMem->release(thumbJpegMem);
            }
        return ret;
    }
};
