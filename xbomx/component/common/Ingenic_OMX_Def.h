/*
 *
 * Copyright 2012 Ingenic Co. LTD
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

/*
 * @file    Inenic_OMX_Def.h
 * @brief   Inenic_OMX specific define
 * @author  Jim Qian (jqian@ingenic.com)
 * @version    1.0.0
 * @history
 *   2013.08.29 : Create
 */

#ifndef INGENIC_OMX_DEF
#define INGENIC_OMX_DEF

#include "OMX_Types.h"
#include "OMX_IVCommon.h"


#define VERSIONMAJOR_NUMBER                1
#define VERSIONMINOR_NUMBER                0
#define REVISION_NUMBER                    0
#define STEP_NUMBER                        0

#define INGENIC_OMX_ROLE_H264              "video_decoder.avc"
#define INGENIC_OMX_ROLE_MPEG4             "video_decoder.mpeg4"
#define INGENIC_OMX_ROLE_WMV3              "video_decoder.wmv3"
#define INGENIC_OMX_ROLE_RV40              "video_decoder.rv40"

typedef enum _INGENIC_OMX_INDEXTYPE
{
    /* for Android Native Window */
    //"OMX.google.android.index.enableAndroidNativeBuffers"
    OMX_IndexParamEnableAndroidBuffers      = 0x7F000011,
    //"OMX.google.android.index.getAndroidNativeBufferUsage"
    OMX_IndexParamGetAndroidNativeBuffer    = 0x7F000012,
    //"OMX.google.android.index.useAndroidNativeBuffer"
    OMX_IndexParamUseAndroidNativeBuffer    = 0x7F000013,

    //"OMX.lume.android.index.setShContext"
    OMX_IndexParamSetShContext              = 0x7F000014,

} INGENIC_OMX_INDEXTYPE;

typedef enum _INGENIC_OMX_COLOR_FORMATTYPE
{
    OMX_COLOR_FormatYUV420Tile              = 0x7F000011,
    OMX_COLOR_Format32bitRGBA8888           = 0x7F000012,
    OMX_COLOR_FormatYUV420ArrayPlanar         = 0x7F000013,

} INGENIC_OMX_COLOR_FORMATTYPE;

typedef struct _INGENIC_OMX_YUV_FORMAT
{
    OMX_U32 nPhyPlanar[4];
    OMX_U32 nPlanar[4];
    OMX_U32 nStride[4];
    OMX_U32 nIsValid;
    OMX_S64 nPts;
    OMX_S32 nIsDechw;
    OMX_PTR pMemHeapBase[4];
    OMX_U32 nMemHeapBaseOffset[4];

} INGENIC_OMX_YUV_FORMAT;

#endif
