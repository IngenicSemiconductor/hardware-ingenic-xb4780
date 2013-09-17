/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, hardware
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HARD_AVC_H_

#define HARD_AVC_H_

#include "SimpleHardOMXComponent.h"
#include <utils/KeyedVector.h>

#include "basetype.h"
#include "lume_dec.h"
#include "PlanarImage.h"

#include "HWColorConverter.h"


namespace android {

typedef struct{
    u32 cropLeftOffset;
    u32 cropOutWidth;
    u32 cropTopOffset;
    u32 cropOutHeight;
} CropParams;

enum VideoFormat {
    VF_INVAL,
    VF_MPEG1,
    VF_H261,
    VF_H263,
    VF_MPEG4,
    VF_MJPEG,
    VF_H264,
    VF_WMV3,
    VF_RV40,
};
  
struct HWDec : public SimpleHardOMXComponent {
    HWDec(const char *name,
	  const OMX_CALLBACKTYPE *callbacks,
	  OMX_PTR appData,
	  OMX_COMPONENTTYPE **component);

protected:
    virtual ~HWDec();

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onPortEnableCompleted(OMX_U32 portIndex, bool enabled);

private:    
    enum {
        kInputPortIndex   = 0,
        kOutputPortIndex  = 1,
        kNumInputBuffers  = 8,
	kNumOutputBuffers = 8,
    };

    enum EOSStatus {
        INPUT_DATA_AVAILABLE,
        INPUT_EOS_SEEN,
        OUTPUT_FRAMES_FLUSHED,
    };

    uint32_t mWidth, mHeight, mPictureSize;
    uint32_t mCropLeft, mCropTop;
    uint32_t mCropWidth, mCropHeight;
    
    EOSStatus mEOSStatus;

    enum OutputPortSettingChange {
        NONE,
        AWAITING_DISABLED,
        AWAITING_ENABLED
    };
    
    OutputPortSettingChange mOutputPortSettingsChange;

    void initPorts();
    void updatePortDefinitions();
    bool drainAllOutputBuffers();

    bool handleCropRectEvent(const CropParams* crop);
    bool handlePortSettingChangeEvent(const OMX_PARAM_PORTDEFINITIONTYPE *param);

    VideoDecorder* mVideoDecoder;
    sh_video_t * mShContext;

    VideoFormat mVideoFormat;
    PlanarImage *mOutBuf;
    bool mVContextNeedFree;
    
    bool mDecInited;
    bool initDecoder();
    bool deInitDecoder();
    
    OMX_COLOR_FORMATTYPE mConvertSrcFormat;
    OMX_COLOR_FORMATTYPE mConvertDstFormat;
    bool mConverterInited;
    sp<HWColorConverter> mConverter;
    bool mUseGraphicBuffer;
    bool initConverter();
    bool deInitConverter();
    void updatePictureSize();
    void colorConvert(void* inBuf, void* outBuf);

    void updateVideoFormat();

    OMX_PARAM_PORTDEFINITIONTYPE mPortParam;
    CropParams mCropParam;
    bool mPortSettingsChangedWaitingForOutFull;

    DISALLOW_EVIL_CONSTRUCTORS(HWDec);
};

}  // namespace android

#endif  // HARD_AVC_H_

