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

#ifndef IPU_CONVERTER_H_
#define IPU_CONVERTER_H_

#include <utils/RefBase.h>
#include <sys/ioctl.h>

#include <android_jz_ipu.h>
#include "dmmu.h"

#include <OMX_Video.h>
#include "Ingenic_OMX_Def.h"

/* rotation degree */
#define ClockWiseRot_0      1
#define ClockWiseRot_90     0
#define ClockWiseRot_180    3
#define ClockWiseRot_270    2

namespace android {

class IPUConverter {
public:
    IPUConverter(OMX_COLOR_FORMATTYPE from, OMX_COLOR_FORMATTYPE to);
    virtual ~IPUConverter();

    bool convert(const void *srcBits, size_t srcStride,
		 size_t srcWidth, size_t srcHeight,
		 size_t srcCropLeft, size_t srcCropTop,
		 size_t srcCropRight, size_t srcCropBottom,
		 void *dstBits, size_t dstStride,
		 size_t dstWidth, size_t dstHeight,
		 size_t dstCropLeft, size_t dstCropTop,
		 size_t dstCropRight, size_t dstCropBottom);
private:
    OMX_COLOR_FORMATTYPE mSrcFormat, mDstFormat;
    bool mIsIPUValid;
    bool mIsIPUInited;
        
    struct ipu_image_info* mIPUHandler;
    
    dmmu_mem_info mSrcMemInfo;
    dmmu_mem_info mDstMemInfo;
    unsigned int mTlbBasePhys;

    int mHalFormat;
    int mBytesPerDstPixel;
    
    bool isValid() const;
    void initIPUDestBuffer(void* data, size_t stride, size_t width, size_t height);
    void initIPUSourceBuffer(void* data, size_t stride, size_t width, size_t height);
    
    IPUConverter(const IPUConverter &);
    IPUConverter &operator=(const IPUConverter &);
};

}  // namespace android

#endif  // HARDWARE_RENDERER_H_
