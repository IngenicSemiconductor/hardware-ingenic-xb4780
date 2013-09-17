#ifndef OMX_Core_Vendor_h
#define OMX_Core_Vendor_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header shall include all required header files to allow the
 *  header to compile without errors.  The includes below are required
 *  for this header file to compile successfully 
 */

#include <OMX_Index.h>
#include <OMX_Core.h>

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* --------------- OMX_INDEXTYPE -------------------------*/
enum {
    OMX_IndexINGParamDynamicResSetting = OMX_IndexVendorStartUnused + 1, /**< reference: OMX_VIDEO_CONFIG_DYNAMIC_RES_SETTING */
    OMX_IndexINGConfigDynamicResSetting, /**< reference: OMX_VIDEO_CONFIG_DYNAMIC_RES_SETTING */
    OMX_IndexINGParamDynamicResMaxSize /**< reference: OMX_VIDEO_PARAM_DYNAMIC_RES_MAXSIZE_TYPE */
};

/* --------------- OMX_EXTRADATATYPE -------------------------*/
enum {
    OMX_ExtraDataINGDynamicRes = OMX_ExtraDataVendorStartUnused + 1 /**< The data payload contains INGDynamicRes */
};


#define MAX_DYNAMIC_RES_CONFIG_PARAMS (10)
/** 
 * This extradata is sent along with each buffer. Whenever a dynamic event
 * needs to be triggered, application toggles the bDynamicEvent. Toggling
 * ensures that event is not lost even if one buffer is lost/discarded 
 */
typedef struct OMX_VIDEO_EXTRADATA_DYNAMIC_RES
{
    OMX_U16 nWidth; //width of image in input buffer
    OMX_U16 nHeight;//height of image in input buffer
    OMX_U16 nPaddedWidth;//ignore-unused
    OMX_U16 nPaddedHeight;//ignore-unused
    OMX_U16 nXOffset;//set to 0
    OMX_U16 nYOffset;//set as 0
    OMX_BOOL bDynamicEvent;//will be toggled by application, this event is copied to output buffer
    OMX_U32 reserved[6];
} OMX_VIDEO_EXTRADATA_DYNAMIC_RES;

/** 
 * Application sends this to indicate the maximum width and height for
 * dynamic resolution change. This information is used by encoder to allocate
 * the maximum buffer size, so that the buffers can be re-used during size
 * change. 
 */
typedef struct OMX_VIDEO_PARAM_DYNAMIC_RES_MAXSIZE_TYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nMaxWidth; /* Max input width the encoder should allocate to handle dynamic resolution changes. */
    OMX_U32 nMaxHeight; /* Max input height the encoder should allocate to handle dynamic resolution changes. */
} OMX_VIDEO_PARAM_DYNAMIC_RES_MAXSIZE_TYPE;

/** 
 * These are enumerations to be used in eConfigType of
 * OMX_VIDEO_CONFIG_DYNAMIC_RES_SETTING. These indices are used to indicate
 * reconfiguration paramteres to encoder during dynamic resolution change. 
 */
typedef enum OMX_VIDEO_DYNAMIC_RES_INDEXTYPE
{
    OMX_IndexDynResFrameRate, /* nParam1=Framerate in Q16.16 format*/
    OMX_IndexDynResnPFrames, /* nParam1=IDR Interval */
    OMX_IndexDynResBitRate, /* nParam1=Bitrate in bits per second */
    OMX_IndexDynResWidthHeight, /* nParam1=Width, nParam2=Height */
    OMX_IndexDynResProfileLevel, /* nParam1=Profile, nParam2=Level */
    /* Extensible as needed */
} OMX_VIDEO_DYNAMIC_RES_INDEXTYPE;

/** 
 * This is sent by application layer before bDynamicEvent is toggled in
 * Extradata of a buffer. Encoder stores this configuration and applies it
 * during reconfiguration upon receiving a toggled bDynamicEvent. Upon applying
 * the settings once, the setting is removed inside encoder.
 * If toggled bDynamicEvent is received but no setting is available, then only
 * width height change is performed. Application must send only one setting
 * prior to a dynamic event. If more than one is received, then old setting is
 * overwritten 
 */
typedef struct OMX_VIDEO_CONFIG_DYNAMIC_RES_SETTING
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_DYNAMIC_RES_INDEXTYPE eConfigType[MAX_DYNAMIC_RES_CONFIG_PARAMS];
    OMX_U16 nNumConfigParams; /* Can configure upto 10 params */
    OMX_U32 nParam1[MAX_DYNAMIC_RES_CONFIG_PARAMS];
    OMX_U32 nParam2[MAX_DYNAMIC_RES_CONFIG_PARAMS];
} OMX_VIDEO_CONFIG_DYNAMIC_RES_SETTING;

#endif
/* File EOF */

