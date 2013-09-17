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

#define LOG_TAG "IPUConverter"
#include <utils/Log.h>

#include <ui/GraphicBufferMapper.h>
#include <gui/Surface.h>

#include "IPUConverter.h"
#include "PlanarImage.h"
#include "jzasm.h"

#define EL(x,y...) //ALOGE(x,##y);


namespace android {

IPUConverter::IPUConverter(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to)
    :mSrcFormat(from),
     mDstFormat(to),
     mIsIPUValid(false),
     mIsIPUInited(false),
     mIPUHandler(NULL),
     mTlbBasePhys(0){
    memset(&mSrcMemInfo, 0, sizeof(dmmu_mem_info));
    memset(&mDstMemInfo, 0, sizeof(dmmu_mem_info));

    int ret;
    ret = ipu_open(&mIPUHandler);
    if (ret < 0) {
	ALOGE("Error: ipu_open() failed!!!");
	return;
    }
    
    ret = dmmu_get_page_table_base_phys(&mTlbBasePhys);
    if (ret < 0) {
	ALOGE("Error: dmmu_get_page_table_base_phys failed!!!");
	return;
    }

    mIsIPUValid = true;

    mHalFormat = HAL_PIXEL_FORMAT_RGB_565;
    mBytesPerDstPixel = 2;
    if (mDstFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitRGBA8888){
	mHalFormat = HAL_PIXEL_FORMAT_RGBX_8888;//HAL_PIXEL_FORMAT_RGBX_8888; as the same format with softrender.
	mBytesPerDstPixel = 4;
    }else if(mDstFormat == OMX_COLOR_Format16bitRGB565){
	mHalFormat = HAL_PIXEL_FORMAT_RGB_565;
	mBytesPerDstPixel = 2;
    }

    ALOGE("Info: IPUConverter constructor. srcformat:%d, dstformat:%d", mSrcFormat, mDstFormat);
}

bool IPUConverter::isValid() const {
    if(mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile &&
       mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar &&
       mSrcFormat != OMX_COLOR_FormatYUV420Planar) {
	ALOGE("Error: unsupported src format for ipu converter!!!");
	return false;
    }
    
    if(mDstFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitRGBA8888 &&
       mDstFormat != OMX_COLOR_Format16bitRGB565){
	ALOGE("Error: unsupported dst format for ipu converter!!!");
	return false;
    }

    if(!mIsIPUValid)
	return false;

    return true;
}
  
IPUConverter::~IPUConverter() {  
    if (mIPUHandler) {
	ipu_close(&mIPUHandler);
	mIPUHandler = NULL;
    }
}
  
void IPUConverter::initIPUSourceBuffer(void* data, size_t stride, size_t width, size_t height){
    if(mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile &&
       mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){//no need for jzbuf, which has already been demmu mapped.
	memset(&mSrcMemInfo, 0, sizeof(dmmu_mem_info));
	mSrcMemInfo.vaddr = data;
	mSrcMemInfo.size = width * height * 3 / 2;//nonjzmedia default as 420P.
	
	int ret = dmmu_map_user_memory(&mSrcMemInfo);
	if (ret < 0) {
	    ALOGE("Error: src dmmu_map_user_memory pimg0->planar[0] failed!\n");
	    memset(&mSrcMemInfo, 0, sizeof(dmmu_mem_info));
	    return;
	}
    }
    
    struct source_data_info *src = &mIPUHandler->src_info;;
    struct ipu_data_buffer *srcBuf = &mIPUHandler->src_info.srcBuf;
    PlanarImage *pimg = (PlanarImage *)data;
    
    if(mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile){
	src->fmt = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
    }else if(mSrcFormat == OMX_COLOR_FormatYUV420Planar || mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){
	src->fmt = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
    }
        
    src->is_virt_buf = 1;
    src->stlb_base = mTlbBasePhys;

    src->width = width;
    src->height = height;

    srcBuf->y_buf_phys = 0; 
    srcBuf->u_buf_phys = 0; 
    srcBuf->v_buf_phys = 0; 
    
    if (mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){
	srcBuf->y_buf_v = reinterpret_cast<void*>(pimg->planar[0]);	    
	srcBuf->u_buf_v = reinterpret_cast<void*>(pimg->planar[1]);
	srcBuf->v_buf_v = reinterpret_cast<void*>(pimg->planar[2]);
	
	srcBuf->y_stride = pimg->stride[0];
	srcBuf->u_stride = pimg->stride[1];
	srcBuf->v_stride = pimg->stride[1];
    }else if(mSrcFormat == OMX_COLOR_FormatYUV420Planar){
	srcBuf->y_buf_v = data;
	srcBuf->u_buf_v = (unsigned char*)data + width * height;
	srcBuf->v_buf_v = (unsigned char*)data + width * height + width * height / 4;
	
	srcBuf->y_stride = width;
	srcBuf->u_stride = width / 2;
	srcBuf->v_stride = width / 2;
    }else if(mSrcFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile){
	srcBuf->y_buf_v = reinterpret_cast<void*>(pimg->planar[0]);	    /* virtual address of y buffer or base address */
	srcBuf->u_buf_v = reinterpret_cast<void*>(pimg->planar[1]);	
	srcBuf->v_buf_v = reinterpret_cast<void*>(pimg->planar[1]);
	
	srcBuf->y_stride = pimg->stride[0];
	srcBuf->u_stride = pimg->stride[1];
	srcBuf->v_stride = pimg->stride[1];
    }
}

void IPUConverter::initIPUDestBuffer(void* data, size_t stride, size_t width, size_t height){    
    //dstStride must be multiple of 32. don't activate it unless GPU native_handle_t in hal_public.h is not working.
    //stride = (width + 31) & 0xFFFFFFE0;

    //demmu map the dst.
    memset(&mDstMemInfo, 0, sizeof(dmmu_mem_info));
    mDstMemInfo.vaddr = data;
    mDstMemInfo.size = stride * height * mBytesPerDstPixel;
    
    int ret = dmmu_map_user_memory(&mDstMemInfo);
    if (ret < 0) {
	ALOGE("Error: !!!!dst dmmu_map_user_memory failed!\n");
	memset(&mDstMemInfo, 0, sizeof(dmmu_mem_info));
	return;
    }
    
    struct dest_data_info *dst = &mIPUHandler->dst_info;
    unsigned int output_mode;
    struct ipu_data_buffer *dstBuf = &dst->dstBuf;
    
    memset(dst, 0, sizeof(dest_data_info));
    memset(dstBuf, 0, sizeof(ipu_data_buffer));
    
    dst->dst_mode = IPU_OUTPUT_TO_FRAMEBUFFER | IPU_OUTPUT_BLOCK_MODE; 
    dst->fmt = mHalFormat;
    
    dst->left = 0;
    dst->top = 0;
    
    dst->width = width;
    dst->height = height;
    dstBuf->y_buf_phys = 0;
    dst->dtlb_base = mTlbBasePhys;
    dst->out_buf_v = data;

    dstBuf->y_stride = stride * mBytesPerDstPixel;
}
    
bool IPUConverter::convert(const void *srcBits, size_t srcStride,
			   size_t srcWidth, size_t srcHeight,
			   size_t srcCropLeft, size_t srcCropTop,
			   size_t srcCropRight, size_t srcCropBottom,
			   void *dstBits, size_t dstStride,
			   size_t dstWidth, size_t dstHeight,
			   size_t dstCropLeft, size_t dstCropTop,
			   size_t dstCropRight, size_t dstCropBottom){

    if(mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile)//when non 420B.
	jz_dcache_wb();
    
    //ipu w/h can not crop, and has limit.
    size_t ipuwidth = srcWidth;
    size_t ipuheight = srcHeight;
    
    initIPUSourceBuffer(const_cast<void*>(srcBits), srcStride, ipuwidth, ipuheight);
    initIPUDestBuffer(dstBits, dstStride, ipuwidth, ipuheight);

    //it means ipu_init may fail sometimes? get continue with it.
    if (!mIsIPUInited) {
	if (ipu_init(mIPUHandler) < 0) {
	    ALOGE("Error: ipu_init() failed mIPUHandler=%p", mIPUHandler);
	    return false;
	} else {
	    mIsIPUInited = true;
	}
    }
    
    ipu_postBuffer(mIPUHandler);
    
    //clean up the mapped dmmu mem. but still it looks ok even without it...
    int ret;	
    if(mDstMemInfo.vaddr != NULL){
	ret = dmmu_unmap_user_memory(&mDstMemInfo);//memset 0 at init is the best to cover all the other data members of mDstMemInfo.
	if (ret < 0) {
	    ALOGE("Error: dst dmmu_unmap_user_memory failed!\n");
	}
    }
    if(mSrcFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile && mSrcMemInfo.vaddr != NULL){
	ret = dmmu_unmap_user_memory(&mSrcMemInfo);
	if (ret < 0) {
	    ALOGE("Error: src dmmu_unmap_user_memory failed!\n");
	}
    }

#if 0
    static int frameCount = 0;
    char filename[20] = {0};
    sprintf(filename, "/data/rgb/rgb%d.raw", ++frameCount);
    FILE* sfd = fopen(filename, "wb");
    if(sfd){
	ALOGE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!fwrite");
	fwrite(dstBits, ipuwidth * ipuheight * 4, 1, sfd);
	fclose(sfd);
	sfd = NULL;
    }else
	ALOGE("fail to open nals.txt:%s", strerror(errno));
#endif

    return true;
}

}  // namespace android
