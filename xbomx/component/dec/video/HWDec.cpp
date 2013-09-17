/*
 * Copyright (C) 2011 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "HWDec"
#include <utils/Log.h>

#include "HWDec.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/IOMX.h>
#include <dlfcn.h>
#include "lume_dec.h"
#include "PlanarImage.h"

#include <LUMEDefs.h>
#include <HardwareAPI.h>
#include <ui/GraphicBufferMapper.h>
#include "HardwareRenderer_FrameBuffer.h"

#include "Ingenic_OMX_Def.h"

extern "C"{
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
}


using namespace android;

extern "C" {
/*
 *exposed interface for libstagefright_soft_lume.so.
 */
  VideoDecorder* CreateLUMESoftVideoDecoder();
  OMX_ERRORTYPE DecInit(VideoDecorder*videoD);
  OMX_ERRORTYPE DecDeinit(VideoDecorder*videoD);
  OMX_BOOL VideoDecSetConext(VideoDecorder*videoD,sh_video_t *sh);
  OMX_BOOL DecodeVideo(VideoDecorder*videoD,
		       OMX_U8* aOutBuffer, OMX_U32* aOutputLength,
		       OMX_U8** aInputBuf, OMX_U32* aInBufSize,
		       OMX_PARAM_PORTDEFINITIONTYPE* aPortParam,
		       OMX_S32* aFrameCount, OMX_BOOL aMarkerFlag, OMX_BOOL *aResizeFlag);
}

namespace android {


static const CodecProfileLevel kProfileLevels[] = {
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
};

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

HWDec::HWDec(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleHardOMXComponent(name, callbacks, appData, component),
      mWidth(320),
      mHeight(240),
      mPictureSize(mWidth * mHeight * 3 / 2),
      mCropLeft(0),
      mCropTop(0),
      mCropWidth(mWidth),
      mCropHeight(mHeight),
      mEOSStatus(INPUT_DATA_AVAILABLE),
      mOutputPortSettingsChange(NONE),
      mVideoDecoder(NULL),
      mShContext(NULL),
      mVideoFormat(VF_INVAL),
      mOutBuf(NULL),
      mVContextNeedFree(false),
      mDecInited(false),
      mConverterInited(false),
      mConverter(NULL),
      mUseGraphicBuffer(false),
      mPortSettingsChangedWaitingForOutFull(false){
    
    initPorts();
    mOutBuf = (PlanarImage *)malloc(sizeof(PlanarImage));

    mConvertSrcFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile;
    mConvertDstFormat = OMX_COLOR_FormatYUV420Planar;

    memset(&mPortParam, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    memset(&mCropParam, 0, sizeof(CropParams));
    ALOGE("HWDec construct mdstformat:0x%0x", mConvertDstFormat);
}

HWDec::~HWDec() {
    ALOGV("~HWDec ");
    
    List<BufferInfo *> &outQueue = getPortQueue(kOutputPortIndex);
    List<BufferInfo *> &inQueue = getPortQueue(kInputPortIndex);
    CHECK(outQueue.empty());
    CHECK(inQueue.empty());
    
    if(mOutBuf){
	free(mOutBuf);
	mOutBuf = NULL;
    }

    deInitDecoder();
    
    ALOGV("~HWDec out");
}

void HWDec::initPorts() {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = kInputPortIndex;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = kNumInputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = MAX_VIDEO_EXTRACTOR_BUFFER_RANGE;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainVideo;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;
    
    def.format.video.pNativeRender = NULL;
    def.format.video.nFrameWidth = mWidth;
    def.format.video.nFrameHeight = mHeight;
    def.format.video.nStride = def.format.video.nFrameWidth;
    def.format.video.nSliceHeight = def.format.video.nFrameHeight;
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    def.format.video.pNativeWindow = NULL;

    addPort(def);

    def.nPortIndex = kOutputPortIndex;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = kNumOutputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainVideo;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 2;

    def.format.video.cMIMEType = const_cast<char *>(MEDIA_MIMETYPE_VIDEO_RAW);
    def.format.video.pNativeRender = NULL;
    def.format.video.nFrameWidth = mWidth;
    def.format.video.nFrameHeight = mHeight;
    def.format.video.nStride = def.format.video.nFrameWidth;
    def.format.video.nSliceHeight = def.format.video.nFrameHeight;
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;//as default
    def.format.video.pNativeWindow = NULL;

    def.nBufferSize =
        (def.format.video.nFrameWidth * def.format.video.nFrameHeight * 3) / 2;

    addPort(def);

    updateVideoFormat();
}

OMX_ERRORTYPE HWDec::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    ALOGV("internalGetParameter in index = 0x%x",index);
    switch ((int)index) {
        case OMX_IndexParamGetAndroidNativeBuffer:
	{
	    GetAndroidNativeBufferUsageParams *Usageparams = (GetAndroidNativeBufferUsageParams *)params;
	    Usageparams->nUsage |= (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
	    return OMX_ErrorNone;
	}
        case OMX_IndexParamVideoPortFormat:
	{
	    OMX_VIDEO_PARAM_PORTFORMATTYPE *formatParams = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)params;
	    
	    if (formatParams->nPortIndex > kOutputPortIndex) {
		return OMX_ErrorUndefined;
	    }
	    
	    if (formatParams->nIndex != 0) {
		return OMX_ErrorNoMore;
	    }
	    
	    if (formatParams->nPortIndex == kInputPortIndex) {
		OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(formatParams->nPortIndex)->mDef;
		formatParams->eCompressionFormat = def->format.video.eCompressionFormat;//OMX_VIDEO_CodingAVC;//
		
		formatParams->eColorFormat = OMX_COLOR_FormatUnused;
		formatParams->xFramerate = 0;
	    } else {
		CHECK(formatParams->nPortIndex == kOutputPortIndex);
		
		OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(formatParams->nPortIndex)->mDef;
		formatParams->eColorFormat = def->format.video.eColorFormat;//OMX_COLOR_FormatYUV420Planar;		
		formatParams->eCompressionFormat = OMX_VIDEO_CodingUnused;
		formatParams->xFramerate = 0;
	    }
	    return OMX_ErrorNone;
	}
	
        case OMX_IndexParamVideoProfileLevelQuerySupported:
	{
	    OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) params;
	    
	    if (profileLevel->nPortIndex != kInputPortIndex) {
		ALOGE("Invalid port index: %ld", profileLevel->nPortIndex);
		return OMX_ErrorUnsupportedIndex;
	    }
	    
	    size_t index = profileLevel->nProfileIndex;
	    size_t nProfileLevels =
		sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
	    if (index >= nProfileLevels) {
		return OMX_ErrorNoMore;
	    }
	    
	    profileLevel->eProfile = kProfileLevels[index].mProfile;
	    profileLevel->eLevel = kProfileLevels[index].mLevel;
	    return OMX_ErrorNone;
	}
    
        default:
	    return SimpleHardOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE HWDec::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    ALOGV("internalSetParameter in index = %x, %x ",index, OMX_IndexParamStandardComponentRole);
    switch ((int)index) {
        case OMX_IndexParamEnableAndroidBuffers:
	{
	    //OMX.google.android.index.enableAndroidNativeBuffers
	    EnableAndroidNativeBuffersParams *pANBParams = (EnableAndroidNativeBuffersParams *) params;
	    if(pANBParams->nPortIndex == kOutputPortIndex 
	       && pANBParams->enable == OMX_TRUE) {
		OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(pANBParams->nPortIndex)->mDef;
		def->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE) HAL_PIXEL_FORMAT_RGBX_8888;//value with incompatiable HAL only under Android ANB mode.
		mConvertDstFormat = (OMX_COLOR_FORMATTYPE) OMX_COLOR_Format32bitRGBA8888;//OMX_COLOR_Format16bitRGB565;//
		mUseGraphicBuffer = true;
	    }
	    
	    return OMX_ErrorNone;
	}
	
        case OMX_IndexParamSetShContext:
	{
	    if (!mShContext) {
		mShContext = (sh_video_t *)params;//directly valuing is not a good idea for openmax actually.
		mVContextNeedFree = false;
		ALOGE("mShContext=%p mShContext->format=%x", mShContext, mShContext->format);
	    }
	    return OMX_ErrorNone;
	}

        case OMX_IndexParamStandardComponentRole:
	{
	    const OMX_PARAM_COMPONENTROLETYPE *roleParams =
		(const OMX_PARAM_COMPONENTROLETYPE *)params;
	    
	    ALOGV("roleParams->cRole =  %s", (const char *)roleParams->cRole);
	    if (strncmp((const char *)roleParams->cRole, INGENIC_OMX_ROLE_MPEG4, OMX_MAX_STRINGNAME_SIZE - 1) == 0){
		mVideoFormat = VF_MPEG4;
	    }else if (strncmp((const char *)roleParams->cRole, INGENIC_OMX_ROLE_H264, OMX_MAX_STRINGNAME_SIZE - 1) == 0){
		mVideoFormat = VF_H264;
	    }else if (strncmp((const char *)roleParams->cRole, INGENIC_OMX_ROLE_WMV3, OMX_MAX_STRINGNAME_SIZE - 1) == 0){
		mVideoFormat = VF_WMV3;
	    }else if (strncmp((const char *)roleParams->cRole, INGENIC_OMX_ROLE_RV40, OMX_MAX_STRINGNAME_SIZE - 1) == 0){
		mVideoFormat = VF_RV40;
	    }else{//TODO. more format should be add to ingenic extended. not this. also need re-value src/dst format for colorconverter.
		//return OMX_ErrorUndefined;
		mVideoFormat = VF_H264;
	    }

	    updateVideoFormat();
	    
	    return OMX_ErrorNone;
	}

        case OMX_IndexParamVideoPortFormat:
	{
	    OMX_VIDEO_PARAM_PORTFORMATTYPE *formatParams =
		(OMX_VIDEO_PARAM_PORTFORMATTYPE *)params;
	    
	    if (formatParams->nPortIndex > kOutputPortIndex) {
		return OMX_ErrorUndefined;
	    }
	
	    if (formatParams->nIndex != 0) {
		return OMX_ErrorNoMore;
	    }
	
	    if(formatParams->nPortIndex == kOutputPortIndex) {
		//do nothing when android ANB mode, because its colorformat has been settled already. further color setparam wont work anymore.
		if(mUseGraphicBuffer){
		    return OMX_ErrorNone;
		}

		//set out omx colorformat for standard omx component when non android ANB mode.
		OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(formatParams->nPortIndex)->mDef;

		switch((int)formatParams->eColorFormat){
		    case OMX_COLOR_FormatYUV420Tile:
		    case OMX_COLOR_FormatYUV420ArrayPlanar:
		    {
			if(mVideoFormat == VF_MPEG4 || mVideoFormat == VF_H264 || 
			   mVideoFormat == VF_WMV3 || mVideoFormat == VF_RV40){//self adjust.
			    def->format.video.eColorFormat = mConvertDstFormat = (OMX_COLOR_FORMATTYPE) OMX_COLOR_FormatYUV420Tile;
			}else{//TODO: add support for other soft dec formats.
			    def->format.video.eColorFormat = mConvertDstFormat = (OMX_COLOR_FORMATTYPE) OMX_COLOR_FormatYUV420ArrayPlanar;
			}
			break;
		    }
		    case OMX_COLOR_FormatYUV420Planar:
		    case OMX_COLOR_Format16bitRGB565:
		    case OMX_COLOR_Format32bitRGBA8888:
		    {
			def->format.video.eColorFormat = mConvertDstFormat = formatParams->eColorFormat;
			break;
		    }
		    default:
		    {
			//unknown output color format.
			ALOGE("Error: does not support the out format:0x%0x", formatParams->eColorFormat);
			return OMX_ErrorUnsupportedSetting;
		    }
		}
	    }

	    return OMX_ErrorNone;
	}

	/*add by gysun : get w*h from ACodec.cpp*/
        case OMX_IndexParamPortDefinition:
	{
	    /**** decoder should have no need to gain width/height from outside.
	      OMX_PARAM_PORTDEFINITIONTYPE *def = (OMX_PARAM_PORTDEFINITIONTYPE *)params;
	      mWidth = def->format.video.nFrameWidth;
	      mHeight = def->format.video.nFrameHeight;
	      ALOGE("change w*h=(%d*%d)",mWidth,mHeight);
	    */
	}
	
        default:
	    return SimpleHardOMXComponent::internalSetParameter(index, params);
    }
}

OMX_ERRORTYPE HWDec::getConfig(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)params;

            if (rectParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            rectParams->nLeft = mCropLeft;
            rectParams->nTop = mCropTop;
            rectParams->nWidth = mCropWidth;
            rectParams->nHeight = mCropHeight;

            return OMX_ErrorNone;
        }

        default:
            return OMX_ErrorUnsupportedIndex;
    }
}

void HWDec::onQueueFilled(OMX_U32 portIndex) {
    bool ret;
    ret = initDecoder();
    if(!ret){
	notify(OMX_EventError, OMX_ErrorUndefined, ret, NULL);
	return;
    }

    ret = initConverter();
    if(!ret){
	notify(OMX_EventError, OMX_ErrorUndefined, ret, NULL);
	return;
    }

    if (mOutputPortSettingsChange != NONE) {
	return;
    }
    
    if (mEOSStatus == OUTPUT_FRAMES_FLUSHED) {
	return;
    }
    
    List<BufferInfo *> &inQueue = getPortQueue(kInputPortIndex);
    List<BufferInfo *> &outQueue = getPortQueue(kOutputPortIndex);

    //android OMXCodec requires that all out buffers should have been sent to its owner component before notified with portsettingschanged event.
    if(mPortSettingsChangedWaitingForOutFull){
	if(outQueue.size() < kNumOutputBuffers){
	    ALOGE("Warning: portsettingschanged waiting for outqueue full filled!!");
	    return;
	}else{
	    ALOGE("Warning: portsettingschanged got outqueue full filled!!!!!!");
	    handlePortSettingChangeEvent(&mPortParam);
	    handleCropRectEvent(&mCropParam);
	    mPortSettingsChangedWaitingForOutFull = false;
	    return;
	}
    }

    status_t err = OK;
    bool portSettingsChanged = false;
    while ((mEOSStatus != INPUT_DATA_AVAILABLE || !inQueue.empty())
	   && !outQueue.empty()) {

	if (mEOSStatus == INPUT_EOS_SEEN) {
	    drainAllOutputBuffers();
	    return;
	}

	BufferInfo *inInfo = *inQueue.begin();
	OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;
	inQueue.erase(inQueue.begin());//always consume inqueue buffer.
	
	if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
	    inInfo->mOwnedByUs = false;
	    notifyEmptyBufferDone(inHeader);
	    mEOSStatus = INPUT_EOS_SEEN;
	    continue;
	}

	uint64_t mFinalPts = inHeader->nTimeStamp;
	
	//input for lume decoder.
	OMX_U8 * inBuf = inHeader->pBuffer + inHeader->nOffset;
	OMX_U32 inLength = inHeader->nFilledLen;

	//output for lume decoder
	BufferInfo *outInfo = *outQueue.begin();
	OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;
	OMX_U8 *outBuf = (OMX_U8*)mOutBuf;
	if(mConvertDstFormat == (OMX_COLOR_FORMATTYPE) OMX_COLOR_FormatYUV420Tile || mConvertDstFormat == (OMX_COLOR_FORMATTYPE) OMX_COLOR_FormatYUV420ArrayPlanar){
	    outBuf = outHeader->pBuffer + outHeader->nOffset;
	}
	OMX_U32 outLength = 0;

	//incoming pts and seekflag for lume decoder
	mVideoDecoder->shContext->pts = ((double)inHeader->nTimeStamp)/1000000.0;
	if(inHeader->nFlags & OMX_BUFFERFLAG_SEEKFLAG){
	    mVideoDecoder->shContext->seekFlag = 1;
	}else{
	    mVideoDecoder->shContext->seekFlag = 0;
	}

	//the rest params for lume decoder
	mPortParam.format.video.nFrameWidth = mWidth;
	mPortParam.format.video.nFrameHeight = mHeight;
	OMX_BOOL dropFrame = OMX_FALSE;
	OMX_S32 frameCount = 0;

	//TODO: not valued in decoding yet.
	mCropParam.cropLeftOffset = mCropLeft;
        mCropParam.cropTopOffset = mCropTop;
        mCropParam.cropOutWidth = mCropWidth;
        mCropParam.cropOutHeight = mCropHeight;

	//where decoding happens.
	OMX_BOOL ret = DecodeVideo(mVideoDecoder,
				   (OMX_U8*)outBuf,
				   (OMX_U32*)&outLength,		 
				   (OMX_U8**)(&inBuf),
				   &inLength,
				   &mPortParam,
				   &frameCount,
				   (OMX_BOOL)1,
				   &dropFrame);
	
	status_t err = OK;
	if(ret == OMX_TRUE){
	    //decoded video size changed.
	    if (((int)mPortParam.format.video.nFrameWidth != mWidth)
		|| ((int)mPortParam.format.video.nFrameHeight != mHeight)) {
		ALOGE("Warning: lume video decoded size changed from (%d * %d) to (%d * %d)!!", mWidth, mHeight,
		      (int)mPortParam.format.video.nFrameWidth, (int)mPortParam.format.video.nFrameHeight);

		if(outQueue.size() == kNumOutputBuffers){
		    handlePortSettingChangeEvent(&mPortParam);
		    mPortSettingsChangedWaitingForOutFull = false;
		}else{
		    mPortSettingsChangedWaitingForOutFull = true;
		}
		
		portSettingsChanged = true;		    

		//means to trigger crop event.
		mCropParam.cropLeftOffset = mCropLeft;
		mCropParam.cropTopOffset = mCropTop;
		mCropParam.cropOutWidth = (int)mPortParam.format.video.nFrameWidth;
		mCropParam.cropOutHeight = (int)mPortParam.format.video.nFrameHeight;
	    }
	    
	    if(mCropLeft != mCropParam.cropLeftOffset || mCropTop != mCropParam.cropTopOffset ||
	       mCropWidth != mCropParam.cropOutWidth || mCropHeight != mCropParam.cropOutHeight){
		ALOGE("Warning: lume video decoded crop left * top * width * height changed from (%d * %d * %d * %d) to (%d * %d * %d * %d)!!",
		      mCropLeft, mCropTop, mCropWidth, mCropHeight, 
		      mCropParam.cropLeftOffset, mCropParam.cropTopOffset, mCropParam.cropOutWidth, mCropParam.cropOutHeight);
		
		if(outQueue.size() == kNumOutputBuffers){
		    handleCropRectEvent(&mCropParam);
		}
		
		portSettingsChanged = true;
	    }

	    if(!portSettingsChanged && outLength > 0){
		void* inColorBuf = mOutBuf;
		void* outColorBuf = outHeader->pBuffer;
		
		colorConvert(inColorBuf, outColorBuf);
		
		mFinalPts = ((PlanarImage*)outBuf)->pts;
		outHeader->nTimeStamp = mFinalPts;
		outHeader->nFlags = inHeader->nFlags;
		outHeader->nFilledLen = mPictureSize;
	    }
	    
	    if(outLength == 0){
		err = ERROR_MALFORMED;
	    }
	}else{
	    err = ERROR_MALFORMED;
	}
	
	inInfo->mOwnedByUs = false;
	notifyEmptyBufferDone(inHeader);
	
	if (portSettingsChanged) {
	    portSettingsChanged = false;
	    return;
	}

	//handle decoded fail by only skiping. 
	if(err != OK){
	    ALOGE("Warning: lume video decode failed ,try next mFinalPts = %lld", mFinalPts);
	    //notify(OMX_EventError, OMX_ErrorUndefined, ERROR_MALFORMED, NULL);//ignore the error.
	    return;
	}
    
	outQueue.erase(outQueue.begin());
	outInfo->mOwnedByUs = false;
	notifyFillBufferDone(outHeader);
    }
}

bool HWDec::handleCropRectEvent(const CropParams *crop) {
    mCropLeft = crop->cropLeftOffset;
    mCropTop = crop->cropTopOffset;
    mCropWidth = crop->cropOutWidth;
    mCropHeight = crop->cropOutHeight;
    
    notify(OMX_EventPortSettingsChanged, 1,
	   OMX_IndexConfigCommonOutputCrop, NULL);//to set nativewindow rect.
    
    return true;
}

bool HWDec::handlePortSettingChangeEvent(const OMX_PARAM_PORTDEFINITIONTYPE *param){
    deInitConverter();
    
    mWidth = (int)param->format.video.nFrameWidth;
    mHeight = (int)param->format.video.nFrameHeight;
    
    updatePictureSize();
    
    updatePortDefinitions();
    notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
    mOutputPortSettingsChange = AWAITING_DISABLED;

    return true;
}

bool HWDec::drainAllOutputBuffers() {
    ALOGV("drainAllOutputBuffers in");
    List<BufferInfo *> &outQueue = getPortQueue(kOutputPortIndex);

    for(int i = 0;i< (outQueue.size()-1);i++){
      BufferInfo *outInfo = *outQueue.begin();
      outQueue.erase(outQueue.begin());
    }
    /*last*/
    BufferInfo *outInfo = *outQueue.begin();
    outQueue.erase(outQueue.begin());
    OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;

    outHeader->nTimeStamp = 0;
    outHeader->nFilledLen = 0;
    outHeader->nFlags = OMX_BUFFERFLAG_EOS;
    mEOSStatus = OUTPUT_FRAMES_FLUSHED;

    outInfo->mOwnedByUs = false;
    notifyFillBufferDone(outHeader);

    return true;
}

void HWDec::onPortFlushCompleted(OMX_U32 portIndex) {
    if (portIndex == kInputPortIndex) {
        mEOSStatus = INPUT_DATA_AVAILABLE;
	mVideoDecoder->shContext->seekFlag = 1;
    }
}

void HWDec::onPortEnableCompleted(OMX_U32 portIndex, bool enabled) {
    switch (mOutputPortSettingsChange) {
        case NONE:
            break;

        case AWAITING_DISABLED:
        {
            CHECK(!enabled);
            mOutputPortSettingsChange = AWAITING_ENABLED;
            break;
        }

        default:
        {
            CHECK_EQ((int)mOutputPortSettingsChange, (int)AWAITING_ENABLED);
            CHECK(enabled);
            mOutputPortSettingsChange = NONE;
            break;
        }
    }
}

void HWDec::updatePortDefinitions() {
    ALOGV("updatePortDefinitions in");
    OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(0)->mDef;
    def->format.video.nFrameWidth = mWidth;
    def->format.video.nFrameHeight = mHeight;
    def->format.video.nStride = def->format.video.nFrameWidth;
    def->format.video.nSliceHeight = def->format.video.nFrameHeight;

    def = &editPortInfo(1)->mDef;
    def->format.video.nFrameWidth = mWidth;
    def->format.video.nFrameHeight = mHeight;
    def->format.video.nStride = def->format.video.nFrameWidth;
    def->format.video.nSliceHeight = def->format.video.nFrameHeight;

    switch((int)mConvertDstFormat){
        case OMX_COLOR_Format32bitRGBA8888:
	{
	    def->nBufferSize = (def->format.video.nFrameWidth * def->format.video.nFrameHeight) * 4;
	    break;
	}
        case OMX_COLOR_Format16bitRGB565:
	{
	    def->nBufferSize = (def->format.video.nFrameWidth * def->format.video.nFrameHeight) * 2;
	    break;
	}
        case OMX_COLOR_FormatYUV420Tile:
        case OMX_COLOR_FormatYUV420ArrayPlanar:
	{
	    def->nBufferSize = sizeof(PlanarImage);
	    break;
	}
        case OMX_COLOR_FormatYUV420Planar:
	{
	    def->nBufferSize = (def->format.video.nFrameWidth * def->format.video.nFrameHeight * 3) / 2;
	    break;
	}
        default:
	{
	    def->nBufferSize = (def->format.video.nFrameWidth * def->format.video.nFrameHeight * 3) / 2;
	    break;
	}	
    }
}

bool HWDec::initDecoder() {
    if(mDecInited)
	return true;

    mVideoDecoder = CreateLUMESoftVideoDecoder();
    CHECK(mVideoDecoder);

    if(DecInit(mVideoDecoder) != OMX_ErrorNone){
	ALOGE("Error: failed to DecInit!!!!!");
	return false;
    }

    if (!mShContext) {
	mShContext = new sh_video_t;
	memset(mShContext,0x0,sizeof(sh_video_t));
	
	mShContext->bih = new BITMAPINFOHEADER;//no extradata.
	memset(mShContext->bih,0,sizeof(BITMAPINFOHEADER) + 0);
    
	mShContext->bih->biSize = sizeof(BITMAPINFOHEADER)  + 0;
        
	if (mVideoFormat == VF_MPEG4){
	    mShContext->bih->biCompression = mmioFOURCC('F', 'M', 'P', '4');
	    mShContext->format = mmioFOURCC('F', 'M', 'P', '4');
	}else if (mVideoFormat == VF_H264){
	    mShContext->bih->biCompression = mmioFOURCC('A','V','C','1');
	    mShContext->format = mmioFOURCC('A', 'V', 'C', '1');
	}else if (mVideoFormat == VF_WMV3){
	    mShContext->bih->biCompression = mmioFOURCC('V','C','-','1');
	    mShContext->format = mmioFOURCC('V','C','-','1');
	}else if (mVideoFormat == VF_RV40){
	    mShContext->bih->biCompression = mmioFOURCC('R','V','4','0');
	    mShContext->format = mmioFOURCC('R','V','4','0');
	}
	
	mShContext->is_rtsp = 1; //no extradata

	mShContext->disp_w = mShContext->bih->biWidth = mWidth;
	mShContext->disp_h = mShContext->bih->biHeight = mHeight;
    
	mShContext->bih->biBitCount = 16; //YUV
	mShContext->bih->biSizeImage = mWidth * mHeight * mShContext->bih->biBitCount/8;
	mShContext->bih->biCompression = mShContext->format;
	
	mVContextNeedFree = true;
    }
    
    if(VideoDecSetConext(mVideoDecoder, mShContext)==OMX_FALSE){
	ALOGE("Error: VideoDecSetConext failed!!!");
	return false;
    }

    mDecInited = true;
    
    return true;
}

bool HWDec::deInitDecoder(){
    if(mShContext && mVContextNeedFree){
	if(mShContext->bih){
	    delete mShContext->bih;
	    mShContext->bih = NULL;
	}
	
	delete mShContext;
	mShContext = NULL;
    }

    if(mVideoDecoder){
	delete mVideoDecoder;
	mVideoDecoder = NULL;
    }
    
    mDecInited = false;

    return true;
}

bool HWDec::initConverter(){
    if(mConverterInited)
	return true;

    if(mConvertDstFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Tile &&
       mConvertDstFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420ArrayPlanar){

	HWColorConverter::Config config;
	config.useDstGraphicBuffer = mUseGraphicBuffer;
	
	mConverter = new HWColorConverter(mConvertSrcFormat, mConvertDstFormat, config);
	if(!mConverter->isValid()){
	    ALOGE("Error: fail to init colorconverter!!!");
	    mConverter.clear();
	    mConverter = NULL;
	    return false;
	}
    }//it is ok to output even without colorconverter.
    
    mConverterInited = true;

    updatePictureSize();
    
    return true;
}

bool HWDec::deInitConverter(){
    if(mConverter != NULL){
	mConverter.clear();
	mConverter = NULL;
    }
    
    mConverterInited = false;

    return true;
}

void HWDec::updatePictureSize(){
    switch((int)mConvertDstFormat){
        case OMX_COLOR_Format32bitRGBA8888:
	{
	    mPictureSize = mWidth * mHeight * 4;
	    break;
	}
        case OMX_COLOR_Format16bitRGB565:
	{
	    mPictureSize = mWidth * mHeight * 2;
	    break;
	}
        case OMX_COLOR_FormatYUV420Tile:
        case OMX_COLOR_FormatYUV420ArrayPlanar:
	{
	    mPictureSize = sizeof(PlanarImage);
	    break;
	}
        case OMX_COLOR_FormatYUV420Planar:
	{
	    mPictureSize = (mWidth * mHeight * 3) / 2;
	    break;
	}
        default:
	{
	    mPictureSize = (mWidth * mHeight * 3) / 2;
	    break;
	}	
    }
}

void HWDec::colorConvert(void* inBuf, void* outBuf){
    if(mConverter != NULL){
	size_t srcStride, dstStride;
	size_t srcWidth, srcHeight;
	size_t dstWidth, dstHeight;
	size_t srcCropLeft, srcCropTop, srcCropRight, srcCropBottom;
	size_t dstCropLeft, dstCropTop, dstCropRight, dstCropBottom;
	srcWidth = dstWidth = srcStride = dstStride = mWidth;
	srcHeight = dstHeight = mHeight;
	srcCropLeft = dstCropLeft = srcCropTop = dstCropTop = 0;
	srcCropRight = dstCropRight = srcWidth - 1;
	srcCropBottom = dstCropBottom = srcHeight -1;
	
	mConverter->convert(inBuf, srcStride,
			    srcWidth, srcHeight,
			    srcCropLeft, srcCropTop,
			    srcCropRight, srcCropBottom,
			    outBuf, dstStride,
			    dstWidth, dstHeight,
			    dstCropLeft, dstCropTop,
			    dstCropRight, dstCropBottom);    
    }
}

void HWDec::updateVideoFormat(){
    PortInfo* info = editPortInfo(0);
    if(info){
	OMX_PARAM_PORTDEFINITIONTYPE *def = &info->mDef;
	
	switch(mVideoFormat){
	    case VF_MPEG4:
	    {
		def->format.video.cMIMEType = const_cast<char *>(MEDIA_MIMETYPE_VIDEO_MPEG4);
		def->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
		break;
	    }
	    case VF_H264:
	    {
		def->format.video.cMIMEType = const_cast<char *>(MEDIA_MIMETYPE_VIDEO_AVC);
		def->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
		break;
	    }
	    case VF_WMV3:
	    {
		def->format.video.cMIMEType = const_cast<char *>(MEDIA_MIMETYPE_VIDEO_WMV3);
		def->format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
		break;
	    }
	    case VF_RV40:
	    {
		def->format.video.cMIMEType = const_cast<char *>(MEDIA_MIMETYPE_VIDEO_RV40);
		def->format.video.eCompressionFormat = OMX_VIDEO_CodingRV;      
		break;
	    }
	    default:
	    {
		break;
	    }
	}
    }
}

}  // namespace android

OMX_ERRORTYPE createHardOMXComponent(
        const char *name, const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData, OMX_COMPONENTTYPE **component) {
    ALOGV("video createHardOMXComponent in");
    sp<HardOMXComponent> codec = new android::HWDec(name, callbacks, appData, component);
    if (codec == NULL) {
	return OMX_ErrorInsufficientResources;
    }
    
    OMX_ERRORTYPE err = codec->initCheck();
    if (err != OMX_ErrorNone)
	return err;
    
    codec->incStrong(NULL);

    return OMX_ErrorNone;
}
