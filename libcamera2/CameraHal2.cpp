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

#define LOG_TAG "CameraHal2"
//#define LOG_NDEBUG 0
#include "CameraHalSelector.h"

namespace android{

    CameraHal2::CameraHal2(int id, CameraDeviceCommon* device)
        :mcamera_id(id),
         mirror(false),
         mStreamType(PREVIEW),
         mRequest_src_ops(NULL),
         mFrame_dst_ops(NULL),
         mCamera2_notify_callback(NULL),
         mNotifyUserPtr(NULL),
         mActive(false), 
         mRequestCount(0),
         mRequest(NULL) {

        mDevice = device;
        if (mDevice != NULL) {
            mModuleOpened = false;
            mCameraModuleDev = new camera2_device_t();
            if (mCameraModuleDev != NULL) {
                mCameraModuleDev->common.tag = HARDWARE_DEVICE_TAG;
                mCameraModuleDev->common.version = HARDWARE_DEVICE_API_VERSION(2,0);
                mCameraModuleDev->common.close = CameraHal2::device_close;
                memset(mCameraModuleDev->common.reserved, 0, 37 -2);

                mJzParameters2 = new JZCameraParameters2(mDevice,mcamera_id);
                if (mJzParameters2 == NULL) {
                    ALOGE("%s: create parameters 2 object fail",__FUNCTION__);
                }

                mVendorTagOps.parent = this;
                mVendorTagOps.get_camera_vendor_section_name = CameraHal2::get_camera_vendor_section_name;
                mVendorTagOps.get_camera_vendor_tag_name = CameraHal2::get_camera_vendor_tag_name;
                mVendorTagOps.get_camera_vendor_tag_type = CameraHal2::get_camera_vendor_tag_type;
            }
        }
    }
     
    CameraHal2::~CameraHal2() {

        if (NULL != mJzParameters2) {
            delete mJzParameters2;
            mJzParameters2 = NULL;
        }

        if (NULL != mCameraModuleDev) {
            delete mCameraModuleDev;
            mCameraModuleDev = NULL;
        }
    }

    void CameraHal2::update_device(CameraDeviceCommon* device) {
        mDevice = device;
    }

    int CameraHal2::module_open(const hw_module_t* module, const char* id,hw_device_t** device) {

        status_t ret = NO_MEMORY;

        if ((NULL == mDevice) || (atoi(id) != mcamera_id)) {
            ALOGE("%s: create camera device fail", __FUNCTION__);
            return ret;
        }
          
        if (NULL != mCameraModuleDev) {
            mCameraModuleDev->common.module = const_cast<hw_module_t*>(module);
            mCameraModuleDev->ops = &(CameraHal2::mCamera2Ops);
            mCameraModuleDev->priv = this;
            *device = &(mCameraModuleDev->common);
            initialize();
            mModuleOpened = true;
        }
        return ret;
    }

    int CameraHal2::device_Close(void) {

        if (mModuleOpened) {
            mCameraModuleDev->ops = NULL;
            mCameraModuleDev->priv = NULL;
            mModuleOpened = false;
        }

        return NO_ERROR;
    }

    int CameraHal2::get_number_cameras(void) {

        ALOGV("Enter %s : line=%d",__FUNCTION__,__LINE__);

        if (NULL != mDevice) {
            return mDevice->getCameraNum();
        }
        return -1;
    }

    int CameraHal2::get_cameras_info(int camera_id, struct camera_info* info) {

        status_t ret = BAD_VALUE;

        if (camera_id != mcamera_id) return ret;

        info->device_version = CAMERA_MODULE_API_VERSION_2_0;
        info->static_camera_characteristics = mJzParameters2->get_camera_metadata();

        if (NULL != mDevice)
            ret = mDevice->getCameraModuleInfo(mcamera_id, info);

        if (ret == NO_ERROR && info->facing == CAMERA_FACING_FRONT) {
            mirror = true;
        }

        return ret;
    }

    void CameraHal2::initialize(void) {
        ALOGV("Enter %s : line=%d",__FUNCTION__,__LINE__);
        mDevice->connectDevice(mcamera_id);
        mJzParameters2->initDefaultParameters(mirror?CAMERA_FACING_FRONT:CAMERA_FACING_BACK);
    }

    //-------------android 4.2 camera 2.0 api ----------

    int CameraHal2::set_Request_queue_src_ops(const camera2_request_queue_src_ops_t *request_src_ops) {
        ALOGV("%s: ",__FUNCTION__);
        if ((NULL != request_src_ops) && (NULL != request_src_ops->dequeue_request)
            && (NULL != request_src_ops->free_request) && (NULL != request_src_ops->request_count)) {
            mRequest_src_ops = request_src_ops;
            return OK;
        }
        return BAD_VALUE;
    }

    int CameraHal2::set_Frame_queue_dst_ops(const camera2_frame_queue_dst_ops_t *frame_dst_ops) {
        ALOGV("%s: ",__FUNCTION__);
        if ((NULL != frame_dst_ops) && (NULL != frame_dst_ops->dequeue_frame)
            &&(NULL != frame_dst_ops->cancel_frame) && (NULL != frame_dst_ops->enqueue_frame)) {
            mFrame_dst_ops = frame_dst_ops;
            return OK;
        }
        return BAD_VALUE;
    }

    int CameraHal2::notify_Request_queue_not_empty(void) {
      ALOGV("%s: ",__FUNCTION__);
      if ((NULL == mFrame_dst_ops) || (NULL == mRequest_src_ops)) {
        ALOGE("%s: queue ops NULL, ignoring request",__FUNCTION__);
        return BAD_VALUE;
      }
      return NO_ERROR;
    }

    int CameraHal2::get_In_progress_count(void) {
        AutoMutex lock(mLock);
        ALOGV("%s: ",__FUNCTION__); 
        return mRequestCount;
    }

    int CameraHal2::flush_Captures_in_progress(void) {
        return NO_ERROR;
    }

    int CameraHal2::construct_Default_request(int request_template,
                                              camera_metadata_t **request) {
        status_t res;

        if (request == NULL) return BAD_VALUE;
        if ((request_template < 0) || request_template >= CAMERA2_TEMPLATE_COUNT) {
          return BAD_VALUE;
        }

        res = mJzParameters2->constructDefaultRequest(request_template,
              request, true);
        if (res != OK) {
          return res;
        }

        res = mJzParameters2->constructDefaultRequest(request_template,
              request, false);
        if (res != OK) {
          ALOGE("%s: Unable to po;ulate new request for tmeplate %d",
                __FUNCTION__, request_template);
        }
        return res;
    }

    /**********************************************************************
     * Stream management
     */

    int CameraHal2::allocate_Stream(// inputs
                                    uint32_t width,
                                    uint32_t height,
                                    int      format,
                                    const camera2_stream_ops_t *stream_ops,
                                    // outputs
                                    uint32_t *stream_id,
                                    uint32_t *format_actual, // IGNORED, will be removed
                                    uint32_t *usage,
                                    uint32_t *max_buffers) {
        ALOGV("%s: stream %dx%d format: 0x%x", __FUNCTION__, width, height, format);

        if (((format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) || (format == CAMERA2_HAL_PIXEL_FORMAT_OPAQUE)) &&
            mJzParameters2->isSupportedResolution(width, height)) {
          *format_actual = HAL_PIXEL_FORMAT_RGB_565;
          *usage = GRALLOC_USAGE_SW_WRITE_OFTEN;
          *max_buffers = 5;

        } else if ((format == CAMERA2_HAL_PIXEL_FORMAT_ZSL) &&
            mJzParameters2->isAvaliableSensorSize(width, height)) {

        } else if (mJzParameters2->isAvaliablePreviewFormat(format)) {

        } else if ((format == HAL_PIXEL_FORMAT_BLOB) &&
            mJzParameters2->isSupportedJpegResolution(width, height)) {
        }
        
        return NO_ERROR;
    }

    int CameraHal2::register_Stream_buffers(uint32_t stream_id,
                                            int num_buffers,
                                            buffer_handle_t *buffers) {
        return NO_ERROR;
    }

    int CameraHal2::release_Stream(uint32_t stream_id) {
        return NO_ERROR;
    }

    int CameraHal2::allocate_Reprocess_stream(uint32_t width,
                                              uint32_t height,
                                              uint32_t format,
                                              const camera2_stream_in_ops_t *reprocess_stream_ops,
                                              // outputs
                                              uint32_t *stream_id,
                                              uint32_t *consumer_usage,
                                              uint32_t *max_buffers) {
        return NO_ERROR;
    }

    int CameraHal2::allocate_Reprocess_stream_from_stream(uint32_t output_stream_id,
                                                          const camera2_stream_in_ops_t *reprocess_stream_ops,
                                                          // outputs
                                                          uint32_t *stream_id) {
        return NO_ERROR;
    }

    int CameraHal2::release_Reprocess_stream(
                                             uint32_t stream_id) {
        return NO_ERROR;
    }

    /**********************************************************************
     * Miscellaneous methods
     */

    int CameraHal2::trigger_Action(uint32_t trigger_id,
                                   int32_t ext1,
                                   int32_t ext2) {
        return NO_ERROR;
    }

    int CameraHal2::set_Notify_callback(camera2_notify_callback notify_cb,
                                        void *user) {
        AutoMutex lock(mLock);

        if (notify_cb && !mCamera2_notify_callback)
            mCamera2_notify_callback = notify_cb;

        if (user && !mNotifyUserPtr)
            mNotifyUserPtr = user;

        return NO_ERROR;
    }

    int CameraHal2::get_Metadata_vendor_tag_ops(vendor_tag_query_ops_t **ops) {
        AutoMutex lock(mLock);
        *ops = &mVendorTagOps;
        return OK;
    }

    int CameraHal2::Dump(int fd) {
        String8 result;

        result.appendFormat ("CameraHal2 device: \n");
        result.appendFormat ("Stream: \n");

        mJzParameters2->dump(fd, mStreamType, result);
        write(fd, result.string(), result.size());

        return NO_ERROR;
    }


    const char *CameraHal2::get_Camera_vendor_section_name(uint32_t tag) {
        ALOGV("%s",__FUNCTION__);
        return mJzParameters2->getVendorSectionName(tag);
    }

    const char * CameraHal2::get_Camera_vendor_tag_name(uint32_t tag) {
        ALOGV("%s",__FUNCTION__);
        return mJzParameters2->getVendorTagName(tag);
    }

    int CameraHal2::get_Camera_vendor_tag_type(uint32_t tag) {
        ALOGV("%s",__FUNCTION__);
        return mJzParameters2->getVendorTagType(tag);
    }          

    camera2_device_ops_t CameraHal2::mCamera2Ops = {
        CameraHal2::set_request_queue_src_ops,
        CameraHal2::notify_request_queue_not_empty,
        CameraHal2::set_frame_queue_dst_ops,
        CameraHal2::get_in_progress_count,
        CameraHal2::flush_captures_in_progress,
        CameraHal2::construct_default_request,
        CameraHal2::allocate_stream,
        CameraHal2::register_stream_buffers,
        CameraHal2::release_stream,
        CameraHal2::allocate_reprocess_stream,
        CameraHal2::allocate_reprocess_stream_from_stream,
        CameraHal2::release_reprocess_stream,
        CameraHal2::trigger_action,
        CameraHal2::set_notify_callback,
        CameraHal2::get_metadata_vendor_tag_ops,
        CameraHal2::dump,
    };

    //--------------------------------- static ----------------------------


    int CameraHal2::device_close(hw_device_t* device) {

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>((reinterpret_cast<struct camera2_device*>(device))->priv);
        if (ch2 == NULL) {
            ALOGE("%s: camera hal2 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch2->device_Close();
    }

    int CameraHal2::set_request_queue_src_ops(const struct camera2_device * device,
                                              const camera2_request_queue_src_ops_t *request_src_ops) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);

        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->set_Request_queue_src_ops(request_src_ops);
        return ret;
    }

    int CameraHal2::notify_request_queue_not_empty(const struct camera2_device * device) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->notify_Request_queue_not_empty();
        return ret;
    }

    int CameraHal2::set_frame_queue_dst_ops(const struct camera2_device * device,
                                            const camera2_frame_queue_dst_ops_t *frame_dst_ops) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
        ret = ch2->set_Frame_queue_dst_ops(frame_dst_ops);
        return ret;
    }

    int CameraHal2::get_in_progress_count(const struct camera2_device * device) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->get_In_progress_count();
        return ret;
    }

    int CameraHal2::flush_captures_in_progress(const struct camera2_device * device) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->flush_Captures_in_progress();
        return ret;
    }

    int CameraHal2::construct_default_request(const struct camera2_device * device,
                                              int request_template,
                                              camera_metadata_t **request) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->construct_Default_request(request_template, request);
        return ret;
    }

    /**********************************************************************
     * Stream management
     */
    int CameraHal2::allocate_stream(
                                    const struct camera2_device * device,
                                    // inputs
                                    uint32_t width,
                                    uint32_t height,
                                    int      format,
                                    const camera2_stream_ops_t *stream_ops,
                                    // outputs
                                    uint32_t *stream_id,
                                    uint32_t *format_actual, // IGNORED, will be removed
                                    uint32_t *usage,
                                    uint32_t *max_buffers) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->allocate_Stream(width, height, format, stream_ops,
                                   stream_id, format_actual, usage, max_buffers);
        return ret;
    }

    int CameraHal2::register_stream_buffers(const struct camera2_device * device,
                                            uint32_t stream_id,
                                            int num_buffers,
                                            buffer_handle_t *buffers) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->register_Stream_buffers(stream_id, num_buffers, buffers);
        return ret;
    }

    int CameraHal2::release_stream(const struct camera2_device * device,
                                   uint32_t stream_id) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->release_Stream(stream_id);
        return ret;
    }

    int CameraHal2::allocate_reprocess_stream(const struct camera2_device * device,
                                              uint32_t width,
                                              uint32_t height,
                                              uint32_t format,
                                              const camera2_stream_in_ops_t *reprocess_stream_ops,
                                              // outputs
                                              uint32_t *stream_id,
                                              uint32_t *consumer_usage,
                                              uint32_t *max_buffers) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
               
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->allocate_Reprocess_stream(width, height, format, reprocess_stream_ops,
                                             stream_id, consumer_usage, max_buffers);
        return ret;
    }

    int CameraHal2::allocate_reprocess_stream_from_stream(const struct camera2_device * device,
                                                          uint32_t output_stream_id,
                                                          const camera2_stream_in_ops_t *reprocess_stream_ops,
                                                          // outputs
                                                          uint32_t *stream_id) {

        int ret = NO_ERROR;
          
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->allocate_Reprocess_stream_from_stream(output_stream_id, reprocess_stream_ops, stream_id);
        return ret;
    }

    int CameraHal2::release_reprocess_stream(const struct camera2_device * device,
                                             uint32_t stream_id) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
           
        ret = ch2->release_Reprocess_stream(stream_id);
        return ret;
    }

    /**********************************************************************
     * Miscellaneous methods
     */

    int CameraHal2::trigger_action(const struct camera2_device * device,
                                   uint32_t trigger_id,
                                   int32_t ext1,
                                   int32_t ext2) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->trigger_Action(trigger_id, ext1, ext2);
        return ret;
    }

    int CameraHal2::set_notify_callback(const struct camera2_device * device,
                                        camera2_notify_callback notify_cb,
                                        void *user) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);

        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
         
        ret = ch2->set_Notify_callback(notify_cb, user);
        return ret;
    }

    /**
     * Release the camera hardware.  Requests that are in flight will be
     * canceled. No further buffers will be pushed into any allocated pipelines
     * once this call returns.
     */
    int CameraHal2::get_metadata_vendor_tag_ops(const struct camera2_device* device,
                                                vendor_tag_query_ops_t **ops) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL) 
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
        ret = ch2->get_Metadata_vendor_tag_ops(ops);
        return ret;
    }

    /**
     * Dump state of the camera hardware
     */
    int CameraHal2::dump(const struct camera2_device * device, int fd) {

        int ret = NO_ERROR;

        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(device->priv);
          
        if (ch2 == NULL)
            {
                ALOGE("%s: camera hal2 is null",__FUNCTION__);
                return NO_MEMORY;
            }
        ret = ch2->Dump(fd);
        return ret;
    }

    /**
     * Get vendor section name for a vendor-specified entry tag. Only called for
     * tags >= 0x80000000. The section name must start with the name of the
     * vendor in the Java package style. For example, CameraZoom inc must prefix
     * their sections with "com.camerazoom." Must return NULL if the tag is
     * outside the bounds of vendor-defined sections.
     */
    const char *CameraHal2::get_camera_vendor_section_name(const vendor_tag_query_ops_t *v,
                                                           uint32_t tag) {
        const TagOps* tagOps = reinterpret_cast<const TagOps*>(v);
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(tagOps->parent);
        if (ch2 == NULL) {
            ALOGE("%s: camera ha2 is null",__FUNCTION__);
            return NULL;
        }

        return ch2->get_Camera_vendor_section_name(tag);
    }

    /**
     * Get tag name for a vendor-specified entry tag. Only called for tags >=
     * 0x80000000. Must return NULL if the tag is outside the bounds of
     * vendor-defined sections.
     */
    const char * CameraHal2::get_camera_vendor_tag_name(const vendor_tag_query_ops_t *v,
                                                        uint32_t tag) {
        const TagOps* tagOps = reinterpret_cast<const TagOps*>(v);
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(tagOps->parent);
        if (ch2 == NULL) {
            ALOGE("%s: camera ha2 is null",__FUNCTION__);
            return NULL;
        }

        return ch2->get_Camera_vendor_tag_name(tag);
    }
    /**
     * Get tag type for a vendor-specified entry tag. Only called for tags >=
     * 0x80000000. Must return -1 if the tag is outside the bounds of
     * vendor-defined sections.
     */
    int CameraHal2::get_camera_vendor_tag_type(const vendor_tag_query_ops_t *v,
                                               uint32_t tag) {
        const TagOps* tagOps = reinterpret_cast<const TagOps*>(v);
        CameraHal2* ch2 = reinterpret_cast<CameraHal2*>(tagOps->parent);
        if (ch2 == NULL) {
            ALOGE("%s: camera ha2 is null",__FUNCTION__);
            return -1;
        }

        return ch2->get_Camera_vendor_tag_type(tag);
    }

};
