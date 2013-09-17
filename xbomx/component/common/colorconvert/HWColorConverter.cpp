/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "HWColorConverter"
#include <utils/Log.h>

#include "HWColorConverter.h"
#include "PlanarImage.h"

#include <ui/GraphicBufferMapper.h>
#include <gui/Surface.h>

#include "hal_public.h"

namespace android {

HWColorConverter::HWColorConverter(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to, const Config& config)
    : mSrcFormat(from),
      mDstFormat(to),
      mClip(NULL),
      mIPUConverter(NULL){

    if ((mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile || 
	 mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar ||
	 mSrcFormat == OMX_COLOR_FormatYUV420Planar) &&
	(mDstFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitRGBA8888 ||
	 mDstFormat == OMX_COLOR_Format16bitRGB565)){
	mIPUConverter = new IPUConverter(from, to);
    }

    mConfig.useDstGraphicBuffer = config.useDstGraphicBuffer;
}

HWColorConverter::~HWColorConverter() {
    if(mClip != NULL){
	delete[] mClip;
	mClip = NULL;
    }

    if(mIPUConverter != NULL){
	delete mIPUConverter;
	mIPUConverter = NULL;
    }
}

bool HWColorConverter::isValid() const {
    if (mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile && mSrcFormat != OMX_COLOR_FormatYUV420Planar){
	return false;
    }
    
    if (mDstFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitRGBA8888 && mDstFormat != OMX_COLOR_FormatYUV420Planar &&
	mDstFormat != OMX_COLOR_Format16bitRGB565) {
        return false;
    }
    
    return true;
}

HWColorConverter::BitmapParams::BitmapParams(
	void *bits, size_t stride,
        size_t width, size_t height,
        size_t cropLeft, size_t cropTop,
        size_t cropRight, size_t cropBottom)
    : mBits(bits),
      mStride(stride),
      mWidth(width),
      mHeight(height),
      mCropLeft(cropLeft),
      mCropTop(cropTop),
      mCropRight(cropRight),
      mCropBottom(cropBottom) {
}

size_t HWColorConverter::BitmapParams::cropWidth() const {
    return mCropRight - mCropLeft + 1;
}

size_t HWColorConverter::BitmapParams::cropHeight() const {
    return mCropBottom - mCropTop + 1;
}

bool HWColorConverter::convert(
        const void *srcBits, size_t srcStride,
        size_t srcWidth, size_t srcHeight,
        size_t srcCropLeft, size_t srcCropTop,
        size_t srcCropRight, size_t srcCropBottom,
        void *dstBits, size_t dstStride,
        size_t dstWidth, size_t dstHeight,
        size_t dstCropLeft, size_t dstCropTop,
        size_t dstCropRight, size_t dstCropBottom) {
    
    void* finalDst = dstBits;

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    buffer_handle_t bufferHandle = (buffer_handle_t)dstBits;
    if(mConfig.useDstGraphicBuffer){
	Rect bounds(dstWidth, dstHeight);
	if(0 != mapper.lock(bufferHandle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &finalDst)){
	    ALOGE("Error: fail to mapper lock buffer for HWConverter!!!");
	    return false;
	}
	const IMG_native_handle_t* gpuBufHandle = (IMG_native_handle_t*)bufferHandle;
	dstStride = gpuBufHandle->iStride;
    }

    BitmapParams dst(
	    finalDst, dstStride,
            dstWidth, dstHeight,
            dstCropLeft, dstCropTop, dstCropRight, dstCropBottom);

    BitmapParams src(
	    const_cast<void *>(srcBits), srcStride, //stride not working for now.
            srcWidth, srcHeight,
            srcCropLeft, srcCropTop, srcCropRight, srcCropBottom);

    bool res = false;
    if(mDstFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitRGBA8888 ||
       mDstFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format16bitRGB565){
	if(mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile ||
	   mSrcFormat == OMX_COLOR_FormatYUV420Planar ||
	   mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){
	    
	    if(mIPUConverter != NULL){
		res = mIPUConverter->convert(srcBits, srcStride,
					     srcWidth, srcHeight,
					     srcCropLeft, srcCropTop,
					     srcCropRight, srcCropBottom,
					     finalDst, dstStride,
					     dstWidth, dstHeight,
					     dstCropLeft, dstCropTop,
					     dstCropRight, dstCropBottom);
	    }
	}
    } else if(mDstFormat == OMX_COLOR_FormatYUV420Planar){
	if(mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile){
	    res = convertFromYUV420TileTo420Planar(src, dst);
	}else if(mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){
	    res = convertFromYUV420ArrayTo420Planar(src, dst);
	}
    }
    
    if(mConfig.useDstGraphicBuffer){
	mapper.unlock(bufferHandle);
    }
    
    return res;
}

bool HWColorConverter::convertFromYUV420TileTo420Planar(const BitmapParams &src, const BitmapParams &dst){
    int8_t *dst_ptr = (int8_t*)dst.mBits + dst.mCropTop * dst.mWidth + dst.mCropLeft;
    int8_t *dst_start = dst_ptr;
    
    PlanarImage* p = (PlanarImage*)src.mBits;    
    int8_t *src_y = (int8_t *)p->planar[0];
    int8_t *src_u = (int8_t *)p->planar[1];
    int8_t *src_v = src_u + 8;
    
    int8_t *dst_y = dst_ptr;
    int8_t *dst_u = dst_y + dst.mWidth * dst.mHeight;
    int8_t *dst_v = dst_y + dst.mWidth * dst.mHeight + dst.mWidth * dst.mHeight / 4;
    
    int iLoop = 0, jLoop = 0, kLoop = 0;
    int dxy;
    int stride_y = p->stride[0];
    int stride_c = p->stride[1];

    //ALOGE("HWColorConverter::convertFromYUV420TileTo420Planar in with stride wh:%d * %d. crop:%d * %d, y:%d, c:%d, dst_ptr:%p", dst.mWidth, dst.mHeight, src.cropWidth(), src.cropHeight(), stride_y, stride_c, dst_ptr);

    size_t dst_y_index = 0, dst_uv_index = 0;
    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
	    if (iLoop>0 && iLoop%8==0){
		jLoop++;
		iLoop = 0;
		dxy = iLoop*2 + jLoop * 256;
		iLoop++;
	    }else{
		dxy = iLoop*2 + jLoop * 256;
		iLoop++;
	    }

            signed y1 = (signed)src_y[dxy];
            signed y2 = (signed)src_y[dxy + 1];

            signed u = (signed)src_u[dxy / 2];
            signed v = (signed)src_v[dxy / 2];

	    dst_y[dst_y_index++] = y1;
	    dst_y[dst_y_index++] = y2;
	    
	    if(y%2 == 0){
		dst_u[dst_uv_index] = u;
		dst_v[dst_uv_index] = v;
		++dst_uv_index;
	    }
        }

	if (kLoop > 0 && kLoop % 15 == 0){
	    src_y += stride_y + 16 - 256;
	    src_u += stride_c + 16 - 128;
	    src_v = src_u + 8;
	    iLoop = 0;jLoop = 0;kLoop = 0;
	}else if (kLoop & 1){
	    src_y += 16;
	    src_u += 16;
	    src_v = src_u + 8;
	    iLoop = 0;jLoop = 0;kLoop++;
	}else{
	    src_y += 16;
	    iLoop = 0;jLoop = 0;kLoop++;
	}
    }
    
#if 0
    static int frameCount = 0;
    char filename[20] = {0};
    sprintf(filename, "/data/rgb/yuv%d.raw", ++frameCount);
    FILE* sfd = fopen(filename, "wb");
    if(sfd){
	LOGE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!fwrite dst_start:%p, wh:w * h:%d*%d", dst_start, dst.mWidth, dst.mHeight);
	fwrite(dst_start, dst.mWidth * dst.mHeight * 3 / 2, 1, sfd);
	fclose(sfd);
	sfd = NULL;
    }else
	LOGE("fail to open nals.txt:%s", strerror(errno));
#endif

    return true;
}

bool HWColorConverter::convertFromYUV420ArrayTo420Planar(const BitmapParams &src, const BitmapParams &dst){
    uint8_t *dst_ptr = (uint8_t*)dst.mBits + dst.mCropTop * dst.mWidth + dst.mCropLeft;
    uint8_t *dst_start = dst_ptr;
    
    PlanarImage* p = (PlanarImage*)src.mBits;    
    uint8_t *src_y = (uint8_t *)p->planar[0];
    uint8_t *src_u = (uint8_t *)p->planar[1];
    uint8_t *src_v = (uint8_t *)p->planar[2];
    
    uint8_t *dst_y = dst_ptr;
    uint8_t *dst_u = dst_y + dst.mWidth * dst.mHeight;
    uint8_t *dst_v = dst_y + dst.mWidth * dst.mHeight + dst.mWidth * dst.mHeight / 4;

    memcpy(dst_y, src_y, src.mWidth * src.mHeight);
    memcpy(dst_u, src_u, src.mWidth * src.mHeight / 4);
    memcpy(dst_v, src_v, src.mWidth * src.mHeight / 4);
    
    return true;
}

bool HWColorConverter::convertFromYUV420TileToRGB565(const BitmapParams &src, const BitmapParams &dst){
    uint8_t *kAdjustedClip = initClip();
    
    uint16_t *dst_ptr = (uint16_t *)dst.mBits + dst.mCropTop * dst.mWidth + dst.mCropLeft;
    uint16_t *dst_start = dst_ptr;
    
    PlanarImage* p = (PlanarImage*)src.mBits;    
    uint8_t *src_y = (uint8_t *)p->planar[0];
    uint8_t *src_u = (uint8_t *)p->planar[1];
    uint8_t *src_v = src_u + 8;
    
    int iLoop = 0, jLoop = 0, kLoop = 0;
    int dxy;
    int stride_y = p->stride[0];
    int stride_c = p->stride[1];

    //ALOGE("HWColorConverter::convertFromYUV420TileTo420Planar in with stride wh:%d * %d. crop:%d * %d, y:%d, c:%d, dst_ptr:%p", dst.mWidth, dst.mHeight, src.cropWidth(), src.cropHeight(), stride_y, stride_c, dst_ptr);

    for (size_t y = 0; y < src.cropHeight(); ++y) {
        for (size_t x = 0; x < src.cropWidth(); x += 2) {
	    if (iLoop>0 && iLoop%8==0){
		jLoop++;
		iLoop = 0;
		dxy = iLoop*2 + jLoop * 256;
		iLoop++;
	    }else{
		dxy = iLoop*2 + jLoop * 256;
		iLoop++;
	    }

	    ALOGE("dxy:%d, x:%d, y:%d", dxy, x, y);

            signed y1 = (signed)src_y[dxy] - 16;
            signed y2 = (signed)src_y[dxy + 1] - 16;

            signed u = (signed)src_u[dxy / 2] - 128;
            signed v = (signed)src_v[dxy / 2] - 128;

            signed u_b = u * 517;
            signed u_g = -u * 100;
            signed v_g = -v * 208;
            signed v_r = v * 409;

            signed tmp1 = y1 * 298;
            signed b1 = (tmp1 + u_b) / 256;
            signed g1 = (tmp1 + v_g + u_g) / 256;
            signed r1 = (tmp1 + v_r) / 256;

            signed tmp2 = y2 * 298;
            signed b2 = (tmp2 + u_b) / 256;
            signed g2 = (tmp2 + v_g + u_g) / 256;
            signed r2 = (tmp2 + v_r) / 256;

            uint32_t rgb1 =
                ((kAdjustedClip[r1] >> 3) << 11)
                | ((kAdjustedClip[g1] >> 2) << 5)
                | (kAdjustedClip[b1] >> 3);

            uint32_t rgb2 =
                ((kAdjustedClip[r2] >> 3) << 11)
                | ((kAdjustedClip[g2] >> 2) << 5)
                | (kAdjustedClip[b2] >> 3);
#if 0
	    if (y > 10 && y < 20){
	      LOGE("%d %d :: rgb %x %x", x, y, rgb1, rgb2);
	      if (x == 100 || x == 400)
		usleep(100*1000);
	    }
#endif
            if (x + 1 < src.cropWidth()) {
	      //if (y < 4)
	      //LOGE("1 :: dst %d = %d", x, ((rgb2 << 16) | rgb1));
                *(uint32_t *)(&dst_ptr[x]) = (rgb2 << 16) | rgb1;
            } else {
	      //if (y < 4)
	      //LOGE("2 :: dst %d = %d", x, rgb1);
                dst_ptr[x] = rgb1;
            }
        }

	if (kLoop > 0 && kLoop % 15 == 0){
	    src_y += stride_y + 16 - 256;
	    src_u += stride_c + 16 - 128;
	    src_v = src_u + 8;
	    iLoop = 0;jLoop = 0;kLoop = 0;
	}else if (kLoop & 1){
	    src_y += 16;
	    src_u += 16;
	    src_v = src_u + 8;
	    iLoop = 0;jLoop = 0;kLoop++;
	}else{
	    src_y += 16;
	    iLoop = 0;jLoop = 0;kLoop++;
	}

        dst_ptr += dst.mWidth;
    }

#if 0
    static int frameCount = 0;
    char filename[20] = {0};
    sprintf(filename, "/data/rgb/rgb%d.raw", ++frameCount);
    FILE* sfd = fopen(filename, "wb");
    if(sfd){
	LOGE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!fwrite dst_start:%p, wh:w * h", dst_start, dst.mWidth, dst.mHeight);
	fwrite(dst_start, dst.mWidth * dst.mHeight * 2, 1, sfd);
	fclose(sfd);
	sfd = NULL;
    }else
	LOGE("fail to open nals.txt:%s", strerror(errno));
#endif

    return true;
}

uint8_t *HWColorConverter::initClip() {
    static const signed kClipMin = -278;
    static const signed kClipMax = 535;

    if (mClip == NULL) {
        mClip = new uint8_t[kClipMax - kClipMin + 1];

        for (signed i = kClipMin; i <= kClipMax; ++i) {
            mClip[i - kClipMin] = (i < 0) ? 0 : (i > 255) ? 255 : (uint8_t)i;
        }
    }

    return &mClip[-kClipMin];
}

}  // namespace android
