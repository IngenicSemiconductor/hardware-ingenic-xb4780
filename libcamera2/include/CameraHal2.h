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

#ifndef __CAMERA_HW_HAL2_H_
#define __CAMERA_HW_HAL2_H_

#include "CameraHalCommon.h"
#include "CameraColorConvert.h"
#include "CameraFaceDetect.h"

//#define USE_X2D
#define X2D_NAME "/dev/x2d"
#define X2D_SCALE_FACTOR 512.0

#define NUM_MAX_STREAM_THREAD  (5)
#define NUM_MAX_CAMERA_BUFFERS (16)
#define NUM_MAX_SUBSTREAM      (4)

#define SIGNAL_MAIN_REQ_Q_NOT_EMPTY             (SIGNAL_THREAD_COMMON_LAST<<1)
#define SIGNAL_MAIN_STREAM_OUTPUT_DONE          (SIGNAL_THREAD_COMMON_LAST<<3)
#define SIGNAL_SENSOR_START_REQ_PROCESSING      (SIGNAL_THREAD_COMMON_LAST<<4)
#define SIGNAL_THREAD_RELEASE                   (SIGNAL_THREAD_COMMON_LAST<<8)
#define SIGNAL_STREAM_REPROCESSING_START        (SIGNAL_THREAD_COMMON_LAST<<14)
#define SIGNAL_STREAM_DATA_COMING               (SIGNAL_THREAD_COMMON_LAST<<15)

#define STREAM_ID_PREVIEW           (0)
#define STREAM_MASK_PREVIEW         (1<<STREAM_ID_PREVIEW)
#define STREAM_ID_RECORD            (1)
#define STREAM_MASK_RECORD          (1<<STREAM_ID_RECORD)
#define STREAM_ID_PRVCB             (2)
#define STREAM_MASK_PRVCB           (1<<STREAM_ID_PRVCB)
#define STREAM_ID_JPEG              (4)
#define STREAM_MASK_JPEG            (1<<STREAM_ID_JPEG)
#define STREAM_ID_ZSL               (5)
#define STREAM_MASK_ZSL             (1<<STREAM_ID_ZSL)

#define STREAM_ID_JPEG_REPROCESS    (8)
#define STREAM_ID_LAST              STREAM_ID_JPEG_REPROCESS

#define SUBSTREAM_TYPE_NONE         (0)
#define SUBSTREAM_TYPE_JPEG         (1)
#define SUBSTREAM_TYPE_RECORD       (2)
#define SUBSTREAM_TYPE_PRVCB        (3)

#define SIG_WAITING_TICK            (5000)

namespace android{

typedef struct stream_parameters {
            uint32_t                width;
            uint32_t                height;
            int                     format;
            const   camera2_stream_ops_t*   streamOps;
            uint32_t                usage;
            int                     numHwBuffers;
            int                     numSvcBuffers;
            int                     numOwnSvcBuffers;
            int                     planes;
            int                     metaPlanes;
            int                     numSvcBufsInHal;
            buffer_handle_t         svcBufHandle[NUM_MAX_CAMERA_BUFFERS];
            int                     bufIndex;
            int                     minUndequedBuffer;
            bool                    needsIonMap;
} stream_parameters_t;

typedef struct substream_entry {
    int                     priority;
    int                     streamId;
} substream_entry_t;

    class CameraHal2 : public CameraHalCommon {

    public:
          
        CameraHal2(int id, CameraDeviceCommon* device);
        ~CameraHal2();

    private:

        mutable Mutex mLock;
        CameraDeviceCommon* mDevice;
        JZCameraParameters2* mJzParameters2;
        int mcamera_id;
        bool mirror;
        bool mModuleOpened;
        StreamType mStreamType;
        camera2_device_t* mCameraModuleDev;

        const camera2_request_queue_src_ops_t * mRequest_src_ops;
        const camera2_frame_queue_dst_ops_t * mFrame_dst_ops;

        struct TagOps : public vendor_tag_query_ops {
            CameraHal2 *parent;
        };
        TagOps mVendorTagOps;

        camera2_notify_callback mCamera2_notify_callback;
        void* mNotifyUserPtr;

        Mutex mInputMutex; // Protects mActive, mRequestCount
        Condition mInputSignal;
        bool mActive; // Whether we're waiting for input requests or actively
                      // working on them
        size_t mRequestCount;

        camera_metadata_t *mRequest;

    public:
          
        void update_device(CameraDeviceCommon* device);

        int module_open(const hw_module_t* module, const char* id, hw_device_t** device);

        bool getModuleInit() {
            return mModuleOpened;
        }

        int get_number_cameras(void);

        int get_cameras_info(int camera_id, struct camera_info* info);    

    private:

        void initialize(void);     

        int device_Close(void);

        int set_Request_queue_src_ops(const camera2_request_queue_src_ops_t *request_src_ops);

        int notify_Request_queue_not_empty(void);

        int set_Frame_queue_dst_ops(const camera2_frame_queue_dst_ops_t *frame_dst_ops);

        int get_In_progress_count(void);

        int flush_Captures_in_progress(void);

        int construct_Default_request(int request_template,
                                      camera_metadata_t **request);

        /**********************************************************************
         * Stream management
         */

        int allocate_Stream(
                            // inputs
                            uint32_t width,
                            uint32_t height,
                            int      format,
                            const camera2_stream_ops_t *stream_ops,
                            // outputs
                            uint32_t *stream_id,
                            uint32_t *format_actual, // IGNORED, will be removed
                            uint32_t *usage,
                            uint32_t *max_buffers);

        int register_Stream_buffers(
                                    uint32_t stream_id,
                                    int num_buffers,
                                    buffer_handle_t *buffers);

        int release_Stream(
                           uint32_t stream_id);

        int allocate_Reprocess_stream(
                                      uint32_t width,
                                      uint32_t height,
                                      uint32_t format,
                                      const camera2_stream_in_ops_t *reprocess_stream_ops,
                                      // outputs
                                      uint32_t *stream_id,
                                      uint32_t *consumer_usage,
                                      uint32_t *max_buffers);

        int allocate_Reprocess_stream_from_stream(
                                                  uint32_t output_stream_id,
                                                  const camera2_stream_in_ops_t *reprocess_stream_ops,
                                                  // outputs
                                                  uint32_t *stream_id);

        int release_Reprocess_stream(
                                     uint32_t stream_id);

        /**********************************************************************
         * Miscellaneous methods
         */

        int trigger_Action(uint32_t trigger_id,
                           int32_t ext1,
                           int32_t ext2);

        int set_Notify_callback(camera2_notify_callback notify_cb,
                                void *user);

        int get_Metadata_vendor_tag_ops(vendor_tag_query_ops_t **ops);

        int Dump(int fd);

        const char *get_Camera_vendor_section_name(uint32_t tag);

        const char * get_Camera_vendor_tag_name(uint32_t tag);

        int get_Camera_vendor_tag_type(uint32_t tag);

    private:
        class MainThread : public SignalDrivenThread {
            CameraHal2* mhal2;
        public:
            MainThread(CameraHal2* hw)
                :mhal2(hw) { }
            ~MainThread();
            void threadFuntionInternal() {
                mhal2->mainThreadFunc(this);
            }
            void release(void);
            bool m_releasing;
        };

        friend class MainThread;

        class StreamThread : public SignalDrivenThread {
            CameraHal2* mhal2;
        public:
            StreamThread(CameraHal2* hw, uint8_t new_index)
                :SignalDrivenThread(), mhal2(hw), m_index(new_index) {  }

            ~StreamThread();

            void threadFuntionInternal() {
                mhal2->streamThreadFunc(this);
            }

            void setParameters(void* new_parameters);
            status_t addtachSubStream(int stream_id, int priority);
            status_t detachSubStream(int stream_id);
            void release(void);
            int findBufferIndex(void* buffAddr);
            int findBufferIndex(buffer_handle_t* bufHandle);

            uint8_t m_index;
            bool m_activated;
            bool m_isBufferInit;
            bool m_releasing;
            int streamType;
            int m_numRegisteredStream;
            stream_parameters_t m_parameters;
            substream_entry_t attachedSubStreams[NUM_MAX_SUBSTREAM];
        };
        friend class StreamThread;

        sp<MainThread> m_mainThread;
        sp<StreamThread> m_streamThreads[NUM_MAX_STREAM_THREAD];

        void mainThreadFunc (SignalDrivenThread* self);
        void streamThreadFunc (SignalDrivenThread* self);
        void subStreamFunc(StreamThread* self, void* srcImageBuf,
             int stream_id, nsecs_t frameTimeStamp);
        void jpegCreator(StreamThread* selfThread, void* srcImageBuf,
                         nsecs_t frameTimeStamp);
        void recordCreator(StreamThread* selfThread, void* srcImageBuf,
                           nsecs_t frameTimeStamp);
        void previewCreator(StreamThread* selfThread, void* srcImageBuf,
                            nsecs_t frameTimeStamp);

    public:

        static camera2_device_ops_t mCamera2Ops;

        static int device_close(hw_device_t* device);

        /**********************************************************************
         * Request and frame queue setup and management methods
         */

        /**
         * Pass in input request queue interface methods.
         */
        static int set_request_queue_src_ops(const struct camera2_device *,
                                             const camera2_request_queue_src_ops_t *request_src_ops);

        /**
         * Notify device that the request queue is no longer empty. Must only be
         * called when the first buffer is added a new queue, or after the source
         * has returned NULL in response to a dequeue call.
         */
        static int notify_request_queue_not_empty(const struct camera2_device *);

        /**
         * Pass in output frame queue interface methods
         */
        static int set_frame_queue_dst_ops(const struct camera2_device *,
                                           const camera2_frame_queue_dst_ops_t *frame_dst_ops);

        /**
         * Number of camera requests being processed by the device at the moment
         * (captures/reprocesses that have had their request dequeued, but have not
         * yet been enqueued onto output pipeline(s ). No streams may be released
         * by the framework until the in-progress count is 0.
         */
        static int get_in_progress_count(const struct camera2_device *);

        /**
         * Flush all in-progress captures. This includes all dequeued requests
         * (regular or reprocessing) that have not yet placed any outputs into a
         * stream or the frame queue. Partially completed captures must be completed
         * normally. No new requests may be dequeued from the request queue until
         * the flush completes.
         */
        static int flush_captures_in_progress(const struct camera2_device *);

        /**
         * Create a filled-in default request for standard camera use cases.
         *
         * The device must return a complete request that is configured to meet the
         * requested use case, which must be one of the CAMERA2_TEMPLATE_*
         * enums. All request control fields must be included, except for
         * android.request.outputStreams.
         *
         * The metadata buffer returned must be allocated with
         * allocate_camera_metadata. The framework takes ownership of the buffer.
         */
        static int construct_default_request(const struct camera2_device *,
                                             int request_template,
                                             camera_metadata_t **request);

        /**********************************************************************
         * Stream management
         */

        /**
         * allocate_stream:
         *
         * Allocate a new output stream for use, defined by the output buffer width,
         * height, target, and possibly the pixel format.  Returns the new stream's
         * ID, gralloc usage flags, minimum queue buffer count, and possibly the
         * pixel format, on success. Error conditions:
         *
         *  - Requesting a width/height/format combination not listed as
         *    supported by the sensor's static characteristics
         *
         *  - Asking for too many streams of a given format type (2 bayer raw
         *    streams, for example).
         *
         * Input parameters:
         *
         * - width, height, format: Specification for the buffers to be sent through
         *   this stream. Format is a value from the HAL_PIXEL_FORMAT_* list. If
         *   HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED is used, then the platform
         *   gralloc module will select a format based on the usage flags provided
         *   by the camera HAL and the consumer of the stream. The camera HAL should
         *   inspect the buffers handed to it in the register_stream_buffers call to
         *   obtain the implementation-specific format if necessary.
         *
         * - stream_ops: A structure of function pointers for obtaining and queuing
         *   up buffers for this stream. The underlying stream will be configured
         *   based on the usage and max_buffers outputs. The methods in this
         *   structure may not be called until after allocate_stream returns.
         *
         * Output parameters:
         *
         * - stream_id: An unsigned integer identifying this stream. This value is
         *   used in incoming requests to identify the stream, and in releasing the
         *   stream.
         *
         * - usage: The gralloc usage mask needed by the HAL device for producing
         *   the requested type of data. This is used in allocating new gralloc
         *   buffers for the stream buffer queue.
         *
         * - max_buffers: The maximum number of buffers the HAL device may need to
         *   have dequeued at the same time. The device may not dequeue more buffers
         *   than this value at the same time.
         *
         */
        static int allocate_stream(
                                   const struct camera2_device *,
                                   // inputs
                                   uint32_t width,
                                   uint32_t height,
                                   int      format,
                                   const camera2_stream_ops_t *stream_ops,
                                   // outputs
                                   uint32_t *stream_id,
                                   uint32_t *format_actual, // IGNORED, will be removed
                                   uint32_t *usage,
                                   uint32_t *max_buffers);

        /**
         * Register buffers for a given stream. This is called after a successful
         * allocate_stream call, and before the first request referencing the stream
         * is enqueued. This method is intended to allow the HAL device to map or
         * otherwise prepare the buffers for later use. num_buffers is guaranteed to
         * be at least max_buffers (from allocate_stream), but may be larger. The
         * buffers will already be locked for use. At the end of the call, all the
         * buffers must be ready to be returned to the queue. If the stream format
         * was set to HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, the camera HAL should
         * inspect the passed-in buffers here to determine any platform-private
         * pixel format information.
         */
        static int register_stream_buffers(
                                           const struct camera2_device *,
                                           uint32_t stream_id,
                                           int num_buffers,
                                           buffer_handle_t *buffers);

        /**
         * Release a stream. Returns an error if called when get_in_progress_count
         * is non-zero, or if the stream id is invalid.
         */
        static int release_stream(
                                  const struct camera2_device *,
                                  uint32_t stream_id);

        /**
         * allocate_reprocess_stream:
         *
         * Allocate a new input stream for use, defined by the output buffer width,
         * height, and the pixel format.  Returns the new stream's ID, gralloc usage
         * flags, and required simultaneously acquirable buffer count, on
         * success. Error conditions:
         *
         *  - Requesting a width/height/format combination not listed as
         *    supported by the sensor's static characteristics
         *
         *  - Asking for too many reprocessing streams to be configured at once.
         *
         * Input parameters:
         *
         * - width, height, format: Specification for the buffers to be sent through
         *   this stream. Format must be a value from the HAL_PIXEL_FORMAT_* list.
         *
         * - reprocess_stream_ops: A structure of function pointers for acquiring
         *   and releasing buffers for this stream. The underlying stream will be
         *   configured based on the usage and max_buffers outputs.
         *
         * Output parameters:
         *
         * - stream_id: An unsigned integer identifying this stream. This value is
         *   used in incoming requests to identify the stream, and in releasing the
         *   stream. These ids are numbered separately from the input stream ids.
         *
         * - consumer_usage: The gralloc usage mask needed by the HAL device for
         *   consuming the requested type of data. This is used in allocating new
         *   gralloc buffers for the stream buffer queue.
         *
         * - max_buffers: The maximum number of buffers the HAL device may need to
         *   have acquired at the same time. The device may not have more buffers
         *   acquired at the same time than this value.
         *
         */
        static int allocate_reprocess_stream(const struct camera2_device *,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t format,
                                             const camera2_stream_in_ops_t *reprocess_stream_ops,
                                             // outputs
                                             uint32_t *stream_id,
                                             uint32_t *consumer_usage,
                                             uint32_t *max_buffers);

        /**
         * allocate_reprocess_stream_from_stream:
         *
         * Allocate a new input stream for use, which will use the buffers allocated
         * for an existing output stream. That is, after the HAL enqueues a buffer
         * onto the output stream, it may see that same buffer handed to it from
         * this input reprocessing stream. After the HAL releases the buffer back to
         * the reprocessing stream, it will be returned to the output queue for
         * reuse.
         *
         * Error conditions:
         *
         * - Using an output stream of unsuitable size/format for the basis of the
         *   reprocessing stream.
         *
         * - Attempting to allocatee too many reprocessing streams at once.
         *
         * Input parameters:
         *
         * - output_stream_id: The ID of an existing output stream which has
         *   a size and format suitable for reprocessing.
         *
         * - reprocess_stream_ops: A structure of function pointers for acquiring
         *   and releasing buffers for this stream. The underlying stream will use
         *   the same graphics buffer handles as the output stream uses.
         *
         * Output parameters:
         *
         * - stream_id: An unsigned integer identifying this stream. This value is
         *   used in incoming requests to identify the stream, and in releasing the
         *   stream. These ids are numbered separately from the input stream ids.
         *
         * The HAL client must always release the reprocessing stream before it
         * releases the output stream it is based on.
         *
         */
        static int allocate_reprocess_stream_from_stream(const struct camera2_device *,
                                                         uint32_t output_stream_id,
                                                         const camera2_stream_in_ops_t *reprocess_stream_ops,
                                                         // outputs
                                                         uint32_t *stream_id);

        /**
         * Release a reprocessing stream. Returns an error if called when
         * get_in_progress_count is non-zero, or if the stream id is not
         * valid.
         */
        static int release_reprocess_stream(
                                            const struct camera2_device *,
                                            uint32_t stream_id);

        /**********************************************************************
         * Miscellaneous methods
         */

        /**
         * Trigger asynchronous activity. This is used for triggering special
         * behaviors of the camera 3A routines when they are in use. See the
         * documentation for CAMERA2_TRIGGER_* above for details of the trigger ids
         * and their arguments.
         */
        static int trigger_action(const struct camera2_device *,
                                  uint32_t trigger_id,
                                  int32_t ext1,
                                  int32_t ext2);

        /**
         * Notification callback setup
         */
        static int set_notify_callback(const struct camera2_device *,
                                       camera2_notify_callback notify_cb,
                                       void *user);

        /**
         * Get methods to query for vendor extension metadata tag infomation. May
         * set ops to NULL if no vendor extension tags are defined.
         */
        static int get_metadata_vendor_tag_ops(const struct camera2_device*,
                                               vendor_tag_query_ops_t **ops);

        /**
         * Dump state of the camera hardware
         */
        static int dump(const struct camera2_device *, int fd);

        /**
         * Get vendor section name for a vendor-specified entry tag. Only called for
         * tags >= 0x80000000. The section name must start with the name of the
         * vendor in the Java package style. For example, CameraZoom inc must prefix
         * their sections with "com.camerazoom." Must return NULL if the tag is
         * outside the bounds of vendor-defined sections.
         */
        static const char *get_camera_vendor_section_name(const vendor_tag_query_ops_t *v,
                                                          uint32_t tag);
        /**
         * Get tag name for a vendor-specified entry tag. Only called for tags >=
         * 0x80000000. Must return NULL if the tag is outside the bounds of
         * vendor-defined sections.
         */
        static const char * get_camera_vendor_tag_name(const vendor_tag_query_ops_t *v,
                                                       uint32_t tag);
        /**
         * Get tag type for a vendor-specified entry tag. Only called for tags >=
         * 0x80000000. Must return -1 if the tag is outside the bounds of
         * vendor-defined sections.
         */
        static int get_camera_vendor_tag_type(const vendor_tag_query_ops_t *v,
                                              uint32_t tag);

    };
};

#endif
