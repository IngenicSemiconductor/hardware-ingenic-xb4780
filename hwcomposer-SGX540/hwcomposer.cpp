//#define LOG_NDEBUG 0

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <cutils/properties.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <cutils/memory.h>
#include <utils/StopWatch.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <utils/Timers.h>
#include <hardware_legacy/uevent.h>

#include <system/graphics.h>

#include "gralloc_priv.h"
#include "dmmu.h"
#include "hwcomposer.h"
#include "hal_public.h"

#define X2DNAME "/dev/x2d"
#define FB0DEV  "/dev/graphics/fb0"
#define FB1DEV  "/dev/graphics/fb1"
#define X2D_SCALE_FACTOR 512.0
#define HW_SUPPORT_LAY_NUM 4
#define INVALID_FORMAT 0xa

#define WIDTH(rect) ((rect).right - (rect).left)
#define HEIGHT(rect) ((rect).bottom - (rect).top)
#define DWORD_ALIGN(a) (((a) >> 3) << 3)

//#define X2D_DEBUG
#undef X2D_DEBUG

#ifndef X2D_DEBUG_LOGD
#undef LOGD
#define LOGD //
#endif

#define LOG_TAG "HWC-JZX2D"

//#define SUPPORT_GLOBAL_ALPHA

struct jz_hwc_device {
    hwc_composer_device_1_t base;
    hwc_procs_t *procs;

    pthread_mutex_t lock;

    int x2d_fd;
    int render_layer_num;
    void *maped_addr[4];
    int handle_rect[4][3];
    int video_rect[3][2];

    struct jz_x2d_config x2d_cfg;

    int geometryChanged;
    int hasComposition;
    unsigned int gtlb_base;

    gralloc_module_t const* gralloc_module;
    struct hwc_framebuffer_info_t *fbDev;
    alloc_device_t* grDev;
    pthread_t vsync_thread;
};

typedef struct jz_hwc_device jz_hwc_device_t;

static void dump_layer(hwc_layer_1_t const* l) {
    ALOGE("dump_layer: %p", l);
#ifdef SUPPORT_GLOBAL_ALPHA
    ALOGE("type=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, global_alpha=%02x",
         l->compositionType, l->flags, l->handle, l->transform, l->blending, l->global_alpha);
#else
    ALOGE("type=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x",
         l->compositionType, l->flags, l->handle, l->transform, l->blending);
#endif
    ALOGE("sourceCrop{%d,%d,%d,%d}, displayFrame{%d,%d,%d,%d}",
         l->sourceCrop.left,
         l->sourceCrop.top,
         l->sourceCrop.right,
         l->sourceCrop.bottom,
         l->displayFrame.left,
         l->displayFrame.top,
         l->displayFrame.right,
         l->displayFrame.bottom
         );

    hwc_rect_t const* r;
    size_t rc, numRects;
    numRects = l->visibleRegionScreen.numRects;
    r = l->visibleRegionScreen.rects;
    ALOGE("visibleRegionScreen.numRects=%d", numRects);
    /* dump visibleRegionScreen */
    for (rc=0; rc < numRects; rc++) {
        ALOGE("visibleRegionScreen rect%d (%d, %d, %d, %d)",
             rc, r->left, r->top, r->right, r->bottom);
        r++;
    }
}

static void x2d_dump_cfg(jz_hwc_device_t *hwc_dev)
{
    int i;

    ALOGE("jz_x2d_config watchdog_cnt: %d\n, tlb_base: %08x", hwc_dev->x2d_cfg.watchdog_cnt, hwc_dev->x2d_cfg.tlb_base);
    ALOGE("dst_address: %08x", hwc_dev->x2d_cfg.dst_address);
    ALOGE("dst_alpha_val: %d\n dst_stride:%d\n dst_mask_val:%08x",
         hwc_dev->x2d_cfg.dst_alpha_val, hwc_dev->x2d_cfg.dst_stride, hwc_dev->x2d_cfg.dst_mask_val);
    ALOGE("dst_width: %d\n dst_height:%d\n dst_bcground:%08x",
         hwc_dev->x2d_cfg.dst_width, hwc_dev->x2d_cfg.dst_height, hwc_dev->x2d_cfg.dst_bcground);
    ALOGE("dst_format: %d\n dst_back_en:%08x\n dst_preRGB_en:%08x",
         hwc_dev->x2d_cfg.dst_format, hwc_dev->x2d_cfg.dst_back_en, hwc_dev->x2d_cfg.dst_preRGB_en);
    ALOGE("dst_glb_alpha_en: %d\ndst_mask_en:%08x\n x2d_cfg.layer_num: %d",
         hwc_dev->x2d_cfg.dst_glb_alpha_en, hwc_dev->x2d_cfg.dst_mask_en, hwc_dev->x2d_cfg.layer_num);

    for (i = 0; i < 4; i++) {
        ALOGE("layer[%d]: ======================================\n", i);
        ALOGE("format: %d\n transform: %d\n global_alpha_val: %d\n argb_order: %d",
             hwc_dev->x2d_cfg.lay[i].format, hwc_dev->x2d_cfg.lay[i].transform,
             hwc_dev->x2d_cfg.lay[i].global_alpha_val, hwc_dev->x2d_cfg.lay[i].argb_order);
        ALOGE("osd_mode: %d\n preRGB_en: %d\n glb_alpha_en: %d\n mask_en: %d",
             hwc_dev->x2d_cfg.lay[i].osd_mode, hwc_dev->x2d_cfg.lay[i].preRGB_en,
             hwc_dev->x2d_cfg.lay[i].glb_alpha_en, hwc_dev->x2d_cfg.lay[i].mask_en);
        ALOGE("color_cov_en: %d\n in_width: %d\n in_height: %d\n out_width: %d",
             hwc_dev->x2d_cfg.lay[i].color_cov_en, hwc_dev->x2d_cfg.lay[i].in_width,
             hwc_dev->x2d_cfg.lay[i].in_height, hwc_dev->x2d_cfg.lay[i].out_width);
        ALOGE("out_height: %d\n out_w_offset: %d\n out_h_offset: %d\n v_scale_ratio: %d",
             hwc_dev->x2d_cfg.lay[i].out_height, hwc_dev->x2d_cfg.lay[i].out_w_offset,
             hwc_dev->x2d_cfg.lay[i].out_h_offset, hwc_dev->x2d_cfg.lay[i].v_scale_ratio);
        ALOGE("h_scale_ratio: %d\n yuv address addr: %08x\n u_addr: %08x\n v_addr: %08x",
             hwc_dev->x2d_cfg.lay[i].h_scale_ratio, hwc_dev->x2d_cfg.lay[i].addr,
             hwc_dev->x2d_cfg.lay[i].u_addr, hwc_dev->x2d_cfg.lay[i].v_addr);
        ALOGE("y_stride: %d\n v_stride: %d\n",
             hwc_dev->x2d_cfg.lay[i].y_stride, hwc_dev->x2d_cfg.lay[i].v_stride);
    }
}

static void dump_x2d_config(jz_hwc_device_t *hwc_dev)
{
    int i, ret;
    struct jz_x2d_config cfg;

    ALOGD("----------------get_x2d_config");
    ret = ioctl(hwc_dev->x2d_fd, IOCTL_X2D_GET_SYSINFO, &cfg);
    if (ret < 0) {
        ALOGD("IOCTL_X2D_GET_SYSINFO failed!\n");
    }
    ALOGD("dst  addr: 0x%x   width:  %d  height: %d",
         cfg.dst_address, cfg.dst_width, cfg.dst_height);
    ALOGD("overlay num is: %d",cfg.layer_num);
    for(i=0; i<cfg.layer_num; i++) {
        ALOGD("src layer: %d",i);
        ALOGD("addr: 0x%x   width: %d  height: %d", cfg.lay[i].addr,
             cfg.lay[i].in_width, cfg.lay[i].in_height);
        ALOGD("out width: %d  height: %d",
             cfg.lay[i].out_width, cfg.lay[i].out_height);
        ALOGD("out pos x: %d y: %d", cfg.lay[i].out_w_offset,
             cfg.lay[i].out_h_offset);
    }
}

static void jz_hwc_dump(struct hwc_composer_device_1* dev, char *buff, int buff_len)
{

}

static void jz_hwc_registerProcs(hwc_composer_device_1 *dev,
                                 hwc_procs_t const *procs)
{
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *)dev;

    hwc_dev->procs = (typeof(hwc_dev->procs)) procs;
}

static void deinit_dev_para(jz_hwc_device_t *hwc_dev)
{
    int i;

    hwc_dev->render_layer_num = 0;
    memset((void *)&hwc_dev->x2d_cfg, 0, sizeof(struct jz_x2d_config));
}

static int get_bpp(hwc_layer_1_t *layer, void *handle)
{
    int bpp = 0;
    int format = 0;

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        format = ((struct VideoFrameInfo *)handle)->format;
    } else {
        format = ((IMG_native_handle_t *)handle)->iFormat;
    }
    switch (format) {
    case HAL_PIXEL_FORMAT_RGB_565:
            bpp = 2;
            break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
            bpp = 4;
            break;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            bpp = 16;
            break;
    case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
    case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
            bpp = 16;
            break;
    default:
            ALOGE("get_bpp unknown layer data format 0x%08x", format);
            bpp = 4;
            break;
    }

    return bpp;
}

static int map_lay_addr(struct src_layer *config,
                        IMG_native_handle_t *handle, hwc_layer_1_t *layer)
{
    int bpp = 0;
    int offset = 0;
    int ret = 0;
    struct dmmu_mem_info mem;

    bpp = get_bpp(layer, (void *)handle);
    memset(&mem, 0, sizeof(struct dmmu_mem_info));
    mem.vaddr = (void *)config->addr;
    mem.size = handle->iStride * handle->iHeight * bpp;
    if (mem.vaddr) {
        ret = dmmu_map_user_memory(&mem);
        if (ret < 0) {
            ALOGE("dmmu_map_user_memory in layers failed!~~~~~~~~~>");
            return -1;
        }
    } else {
        ALOGE("mem.vaddr is NULL !!!!!!!");
        return -1;
    }

    if (layer->sourceCrop.left || layer->sourceCrop.top) {
        offset = ((layer->sourceCrop.top)*handle->iStride + (layer->sourceCrop.left)) * bpp;
        config->addr += offset;
        ALOGD("++++++++++++++++config->addr: %08x, get_bpp(): %d",
             config->addr, get_bpp(layer, (void *)handle));
        config->addr = DWORD_ALIGN(config->addr);
        ALOGD("++++++++++++++++config->addr: %08x, get_bpp(): %d",
             config->addr, get_bpp(layer, (void *)handle));
    }

    return 0;
}

static int map_video_layer_addrs(struct VideoFrameInfo *handle)
{
    if (handle->dmmu_maped) {
        ALOGD("************************** handle->dmmu_maped skip map_video_layer_addrs return.");
        return 0;
    }

    int ret = 0;
    int i;

    for (i = 0; i < handle->count; i++) {
        void * vaddr = (void *)handle->addr[i];
        int size = handle->size[i];
        if (vaddr) {
            dmmu_match_user_mem_tlb(vaddr, size);
            ret = dmmu_map_user_mem(vaddr, size);
            if (ret < 0) {
                ALOGE("dmmu_map_user_memory in layers failed!~~~~~~~~~>");
                return -1;
            }
        } else {
            ALOGE("mem.vaddr is NULL !!!!!!!");
            return -1;
        }
    }

    return 0;
}

static int map_dst_addr(jz_hwc_device_t *hwc_dev, int i)
{
    int ret = 0;
    struct dmmu_mem_info mem;

    memset(&mem, 0, sizeof(struct dmmu_mem_info));
    mem.vaddr = (void *)hwc_dev->fbDev->addr[i];
    mem.size = hwc_dev->fbDev->var_info.width * hwc_dev->fbDev->var_info.height * 4;
    ret = dmmu_map_user_memory(&mem);
    if (ret < 0) {
        ALOGE("dmmu_map_user_memory failed!~~~~~~~~~>");
        return -1;
    }

    return 0;
}

static void unmap_dst_addr(jz_hwc_device_t *hwc_dev)
{
    int ret;
    struct dmmu_mem_info mem;

    memset(&mem, 0, sizeof(struct dmmu_mem_info));
    mem.vaddr = (void *)hwc_dev->fbDev->addr[hwc_dev->fbDev->buf_index];
    mem.size = hwc_dev->fbDev->var_info.width * hwc_dev->fbDev->var_info.height * 4;
    ret = dmmu_unmap_user_memory(&mem);
    if (ret < 0) {
        ALOGE("%s %s %d: dmmu_unmap_user_memory failed", __FILE__, __FUNCTION__, __LINE__);
    }
}

static void unmap_layers_addr(jz_hwc_device_t *hwc_dev, hwc_display_contents_1_t *list)
{
    int ret = 0;
    int i, j;

    for (i = 0; i < 4; i++) {
        struct dmmu_mem_info mem;

        memset(&mem, 0, sizeof(struct dmmu_mem_info));
        mem.vaddr = hwc_dev->maped_addr[i];
        ALOGD("w: %d, h: %d, bpp: %d~~~~~~~>", hwc_dev->handle_rect[i][0],
             hwc_dev->handle_rect[i][1], hwc_dev->handle_rect[i][2]);
        //        mem.size = hwc_dev->x2d_cfg.lay[i].in_width * hwc_dev->x2d_cfg.lay[i].in_height * 4;
        mem.size = hwc_dev->handle_rect[i][0] * hwc_dev->handle_rect[i][1] * hwc_dev->handle_rect[i][2];
        if (mem.vaddr && mem.size) {
            ret = dmmu_unmap_user_memory(&mem);
            if (ret < 0) {
                ALOGE("%s %s %d: dmmu_unmap_user_memory failed", __FILE__, __FUNCTION__, __LINE__);
                return;
            }
        }
    }

    for (i = 0; i < 3; i++) {
        struct dmmu_mem_info mem;

        memset(&mem, 0, sizeof(struct dmmu_mem_info));
        mem.vaddr = (void *)hwc_dev->video_rect[i][0];
        mem.size = hwc_dev->video_rect[i][1];
        if (mem.vaddr && mem.size) {
            ret = dmmu_unmap_user_memory(&mem);
            if (ret < 0) {
                ALOGE("%s %s %d: dmmu_unmap_user_memory failed", __FILE__, __FUNCTION__, __LINE__);
                //return;
            }
        } else {
            break;
        }
    }

    for (i = 0; i < 4; i++) {
        hwc_dev->maped_addr[i] = NULL;
        for (j = 0; j < 3; j++) {
            hwc_dev->handle_rect[i][j] = 0;
            hwc_dev->handle_rect[i][j] = 0;
            hwc_dev->handle_rect[i][j] = 0;
        }
    }

    for (i = 0; i < 3; i++)
        for (j = 0; j < 2; j++) {
            hwc_dev->video_rect[i][j] = 0;
            hwc_dev->video_rect[i][j] = 0;
        }
}

static int is_null_addr(hwc_layer_1_t *layer)
{
    void *addr0 = NULL;
    void *addr1 = NULL;

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        addr0 = ((struct VideoFrameInfo *)layer->handle)->addr[0];
        addr1 = ((struct VideoFrameInfo *)layer->handle)->addr[1];
        addr0 = (void *)(((unsigned int)addr0 >> 12) << 12);
        addr1 = (void *)(((unsigned int)addr1 >> 12) << 12);
        if (!addr0 || !addr1)
            return 1;
    }
    return 0;
}

static int is_supported_format(hwc_layer_1_t *layer)
{
    int format;

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        format = ((struct VideoFrameInfo *)layer->handle)->format;
    } else {
        format = ((IMG_native_handle_t *)layer->handle)->iFormat;
    }

    if (format == INVALID_FORMAT)
        return 0;

    switch (format) {
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
    case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
            return 1;
    default:
            return 0;
    }
}

static int is_support_this_layer(hwc_layer_1_t *layer)
{
    int width, height;
    void *handle = NULL;

    //dump_layer(layer);

    handle = (void *)(layer->handle);

    if (handle) {
        if (is_null_addr(layer)) {
            ALOGE("null addr !!!");
            return 0;
        }

        if (!is_supported_format(layer)) {
            ALOGD("format not support!!!");
            return 0;
        }
    } else {
        /*
        if (!strcmp(layer->TypeID, "LayerScreenshot"))
            return 0;
        */
    }

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_DFB) {
        ALOGE("video layer ipu direct mode!");
        return 0;
    }

    return 1;
}

static void setup_config_addr(jz_hwc_device_t *hwc_dev,
                              struct src_layer *config, hwc_layer_1_t *layer, int j)
{
    int ret = 0;

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        struct VideoFrameInfo *handle = (struct VideoFrameInfo *)(layer->handle);

        if (handle->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            config->addr = (int)handle->addr[0];
            config->u_addr = (int)handle->addr[1];
            config->v_addr = (int)handle->addr[1];
        } else {
            config->addr = (int)handle->addr[0];
            config->u_addr = (int)handle->addr[1];
            config->v_addr = (int)handle->addr[2];
        }
        ALOGD("config->addr: %08x u_addr: %08x v_addr: %08x================>",
             config->addr, config->u_addr, config->v_addr);
        map_video_layer_addrs(handle);
    } else {
        IMG_native_handle_t *handle = (IMG_native_handle_t *)(layer->handle);

        ret = hwc_dev->gralloc_module->lock(hwc_dev->gralloc_module, (buffer_handle_t)handle,
                                            GRALLOC_USAGE_SW_READ_OFTEN,
                                            0, 0, handle->iWidth, handle->iHeight,
                                            (void **)&config->addr);
        if (ret) {
            ALOGE("%s %s %d: Get source vaddr error", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        hwc_dev->maped_addr[j] = (void *)config->addr;

        ret = hwc_dev->gralloc_module->unlock(hwc_dev->gralloc_module, (buffer_handle_t)handle);
        if (ret) {
            ALOGE("%s %s %d: Get source vaddr error", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        ret = map_lay_addr(config, handle, layer);
        if (ret < 0) {
            ALOGE("map_lay_addr failed!");
            return;
        }
    }
}

static void setup_config_alpha(struct src_layer *config, hwc_layer_1_t *layer)
{
    switch (layer->blending) {
    case HWC_BLENDING_NONE:
#ifdef SUPPORT_GLOBAL_ALPHA
            config->glb_alpha_en = 1;
            config->global_alpha_val = layer->global_alpha;
#endif
            break;
    case HWC_BLENDING_COVERAGE:
    case HWC_BLENDING_PREMULT:
            break;
    }
}

static void setup_config_transform(struct src_layer *config, hwc_layer_1_t *layer)
{
    switch (layer->transform) {
    case HWC_TRANSFORM_FLIP_H:
            ALOGD("HWC_TRANSFORM_FLIP_H~~~~");
            config->transform = X2D_H_MIRROR;
            break;
    case HWC_TRANSFORM_FLIP_V:
            ALOGD("HWC_TRANSFORM_FLIP_V~~~~");
            config->transform = X2D_V_MIRROR;
            break;
    case HWC_TRANSFORM_ROT_90:
            ALOGD("HWC_TRANSFORM_ROT_90~~~~");
            config->transform = X2D_ROTATE_90;
            break;
    case HWC_TRANSFORM_ROT_180:
            ALOGD("HWC_TRANSFORM_ROT_180~~~~");
            config->transform = X2D_ROTATE_180;
            break;
    case HWC_TRANSFORM_ROT_270:
            ALOGD("HWC_TRANSFORM_ROT_270~~~~");
            config->transform = X2D_ROTATE_270;
            break;
    default:
            ALOGD("HWC_TRANSFORM_ROT_0~~~~");
            config->transform = X2D_ROTATE_0;
            break;
    }
}

static void setup_config_format(struct src_layer *config, hwc_layer_1_t *layer)
{
    int format;

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        format = ((struct VideoFrameInfo *)layer->handle)->format;
    } else {
        format = ((IMG_native_handle_t *)layer->handle)->iFormat;
    }

    switch (format) {
    case HAL_PIXEL_FORMAT_RGB_565:
            ALOGD("1------------HAL_PIXEL_FORMAT_RGB_565");
            config->argb_order = X2D_RGBORDER_RGB;
            config->format = X2D_INFORMAT_RGB565;
#ifdef SUPPORT_GLOBAL_ALPHA
            config->glb_alpha_en = 1;
            config->global_alpha_val = layer->global_alpha;
#endif
            break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
            ALOGD("2------------HAL_PIXEL_FORMAT_RGBA_8888:");
            config->argb_order = X2D_RGBORDER_BGR;
            config->format = X2D_INFORMAT_ARGB888;
            break;
    case HAL_PIXEL_FORMAT_RGBX_8888:
            ALOGD("3------------HAL_PIXEL_FORMAT_RGBX_8888:");
            config->argb_order = X2D_RGBORDER_BGR;
            config->format = X2D_INFORMAT_ARGB888;
#ifdef SUPPORT_GLOBAL_ALPHA
            config->glb_alpha_en = 1;
            config->global_alpha_val = layer->global_alpha;
#endif
            break;
    case HAL_PIXEL_FORMAT_BGRX_8888:
            config->argb_order = X2D_RGBORDER_RGB;
            config->format = X2D_INFORMAT_ARGB888;
#ifdef SUPPORT_GLOBAL_ALPHA
            config->glb_alpha_en = 1;
            config->global_alpha_val = layer->global_alpha;
#endif
            break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
            ALOGD("4------------HAL_PIXEL_FORMAT_BGRA_8888:");
            config->argb_order = X2D_RGBORDER_RGB;
            config->format = X2D_INFORMAT_ARGB888;
            break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            config->format = X2D_INFORMAT_NV12;
            break;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
            config->format = X2D_INFORMAT_YUV420SP;
            break;
    case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
            config->format = X2D_INFORMAT_TILE420;
            break;
    default:
            ALOGE("%s %s %d: setup_config_format unknown layer data format 0x%08x",
                 __FILE__, __FUNCTION__, __LINE__, format);
            config->format = X2D_INFORMAT_NOTSUPPORT;
            break;
    }
}

static void setup_config_size(jz_hwc_device_t *hwc_dev, struct src_layer *config, hwc_layer_1_t *layer)
{
    int i, bpp;
    float v_scale, h_scale;
    float tmp_v_scale = 0, tmp_h_scale = 0;

    config->in_width =  WIDTH(layer->sourceCrop);
    config->in_height = HEIGHT(layer->sourceCrop);
    config->out_width = WIDTH(layer->displayFrame);
    config->out_height = HEIGHT(layer->displayFrame);
    config->out_w_offset = layer->displayFrame.left;
    config->out_h_offset = layer->displayFrame.top;

    /* ALOGE("///////////////////////////"); */
    /* dump_layer(layer); */
    /* ALOGE("hwc_dev->fbDev->buf_index = %d", hwc_dev->fbDev->buf_index); */
    /* ALOGE("\\\\\\\\\\\\\\\\\\\\\\\\\\\\"); */

    ALOGD("+++++++++++in_width: %d, in_height: %d, out_width: %d, out_height: %d",
         config->in_width, config->in_height, config->out_width, config->out_height);
    ALOGD("+++++++++++out_w_offset: %d, out_h_offset: %d", config->out_w_offset, config->out_h_offset);

    if ((config->out_w_offset < 0) || (config->out_w_offset > (int)hwc_dev->fbDev->var_info.width)) {
        ALOGW("w_offset: %d exceed the boundary!!!!!!!!!", config->out_w_offset);
        if (layer->flags != HWC_INGENIC_VIDEO_LAYER_X2D) {
            tmp_h_scale = (float)config->in_width / (float)config->out_width;
        }

        if (config->out_w_offset < 0) {
            config->out_w_offset = 0;
        }
        if (config->out_w_offset > (int)hwc_dev->fbDev->var_info.width) {
            config->out_w_offset = hwc_dev->fbDev->var_info.width;
        }
    }
    if ((config->out_h_offset < 0) || (config->out_h_offset > (int)hwc_dev->fbDev->var_info.height)) {
        ALOGW("h_offset: %d exceed the boundary!!!!!!!!!", config->out_h_offset);
        if (layer->flags != HWC_INGENIC_VIDEO_LAYER_X2D) {
            tmp_v_scale = (float)config->in_height / (float)config->out_height;
        }

        if (config->out_h_offset < 0) {
            config->out_h_offset = 0;
        }
        if (config->out_h_offset > (int)hwc_dev->fbDev->var_info.height) {
            config->out_h_offset = (int)hwc_dev->fbDev->var_info.height;
        }
    }
    if ((config->out_width + config->out_w_offset) > (int)hwc_dev->fbDev->var_info.width) {
        config->out_width = hwc_dev->fbDev->var_info.width - config->out_w_offset;
    }
    if ((config->out_height + config->out_h_offset) > (int)hwc_dev->fbDev->var_info.height) {
        config->out_height = hwc_dev->fbDev->var_info.height - config->out_h_offset;
    }

    if ((int)tmp_h_scale) {
        //ALOGE("tmp_h_scale: %f, tmp_v_scale: %f", tmp_h_scale, tmp_v_scale);
        int in_h_offset = config->in_width - config->out_width*(int)tmp_h_scale;
        IMG_native_handle_t *handle = (IMG_native_handle_t *)(layer->handle);
        bpp = get_bpp(layer, (void *)handle);

        //ALOGE("in_h_offset: %d", in_h_offset);
        if (in_h_offset < 0) {
            in_h_offset = -in_h_offset;
        }
        config->in_width = config->out_width * (int)tmp_h_scale;
        config->addr += in_h_offset * bpp;
        config->addr = DWORD_ALIGN(config->addr);
    }
    if ((int)tmp_v_scale) {
        //ALOGE("tmp_h_scale: %f, tmp_v_scale: %f", tmp_h_scale, tmp_v_scale);
        int in_v_offset = config->in_height - config->out_height*(int)tmp_v_scale;
        IMG_native_handle_t *handle = (IMG_native_handle_t *)(layer->handle);
        bpp = get_bpp(layer, (void *)handle);

        if (in_v_offset < 0) {
            in_v_offset = -in_v_offset;
        }
        config->in_height = config->out_height * (int)tmp_v_scale;
        config->addr += in_v_offset * handle->iStride * bpp;
        config->addr = DWORD_ALIGN(config->addr);
    }

    ALOGD("-----------in_width: %d, in_height: %d, out_width: %d, out_height: %d",
         config->in_width, config->in_height, config->out_width, config->out_height);

    if (config->out_width && config->out_height) {
        switch (config->transform) {
            case X2D_H_MIRROR:
            case X2D_V_MIRROR:
            case X2D_ROTATE_0:
            case X2D_ROTATE_180:
                h_scale = (float)config->in_width / (float)config->out_width;
                v_scale = (float)config->in_height / (float)config->out_height;
                config->h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR) - 1; /* fix upscale */
                config->v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR) - 1; /* fix upscale */
                break;
            case X2D_ROTATE_90:
            case X2D_ROTATE_270:
                h_scale = (float)config->in_width / (float)config->out_height;
                v_scale = (float)config->in_height / (float)config->out_width;
                config->h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR) - 1; /* fix upscale */
                config->v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR) - 1; /* fix upscale */
                break;
            default:
                ALOGE("%s %s %d:undefined rotation degree!!!!", __FILE__, __FUNCTION__, __LINE__);
        }
    } else {
        ALOGE("layer: out_width: %d or out_height: %d is 0!", config->out_width, config->out_height);
    }

    if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
        struct VideoFrameInfo * handle = (struct VideoFrameInfo *)(layer->handle);
        ALOGD("handle->width: %d, handle->height: %d", handle->stride[0], handle->stride[1]);

        config->y_stride = handle->stride[0];
        config->v_stride = handle->stride[1];

        if (handle->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            config->y_stride = handle->stride[0]>>4;
            config->v_stride = handle->stride[1]>>4;
        }
        /* YUV_420tile, width and height 16 aligned. */
        config->in_width =  (config->in_width+0xF) & ~(0xF);
        config->in_height =  (config->in_height+0xF) & ~(0xF);
    } else {
        /* ALOGE("bpp: %d iStride: %d iWidth: %d+++++++++++++++++++++", bpp, handle->iStride, handle->iWidth); */
        IMG_native_handle_t *handle = (IMG_native_handle_t *)(layer->handle);
        bpp = get_bpp(layer, (void *)handle);
        config->y_stride = handle->iStride * bpp;
    }
}

static void set_dst_config(jz_hwc_device_t *hwc_dev)
{
    hwc_dev->x2d_cfg.dst_address = hwc_dev->fbDev->addr[hwc_dev->fbDev->buf_index];
    hwc_dev->x2d_cfg.dst_width = hwc_dev->fbDev->var_info.width;
    hwc_dev->x2d_cfg.dst_height = hwc_dev->fbDev->var_info.height;
    hwc_dev->x2d_cfg.dst_format = X2D_OUTFORMAT_ARGB888;
    hwc_dev->x2d_cfg.dst_stride = hwc_dev->x2d_cfg.dst_width * 4;

    hwc_dev->x2d_cfg.dst_back_en = 0;
    hwc_dev->x2d_cfg.dst_glb_alpha_en = 0;
    hwc_dev->x2d_cfg.dst_preRGB_en = 0;
    hwc_dev->x2d_cfg.dst_mask_en = 0;
    hwc_dev->x2d_cfg.dst_alpha_val = 0xff;
}

static void set_dst_config2(jz_hwc_device_t *hwc_dev)
{
    hwc_dev->x2d_cfg.dst_address = hwc_dev->fbDev->addr[hwc_dev->fbDev->buf_index];
    hwc_dev->x2d_cfg.dst_width = hwc_dev->fbDev->var_info.width;
    hwc_dev->x2d_cfg.dst_height = hwc_dev->fbDev->var_info.height;
    hwc_dev->x2d_cfg.dst_format = X2D_OUTFORMAT_ARGB888;
    hwc_dev->x2d_cfg.dst_stride = hwc_dev->x2d_cfg.dst_width * 4;

    hwc_dev->x2d_cfg.dst_back_en = 1;
    hwc_dev->x2d_cfg.dst_glb_alpha_en = 0;
    hwc_dev->x2d_cfg.dst_preRGB_en = 1;
    hwc_dev->x2d_cfg.dst_mask_en = 0;
    hwc_dev->x2d_cfg.dst_alpha_val = 0xff;
}

static void set_layers_config(jz_hwc_device_t *hwc_dev,
                              hwc_display_contents_1_t *list, unsigned int *layer_cnt)
{
    unsigned int i;
    int ret = 0, j;
    int width, height;
    unsigned int tmp_cnt = *layer_cnt;

    if (tmp_cnt > list->numHwLayers) {
        ALOGE("%s %s %d: layer_cnt: %d, list->numHwLayers: %d, layers over",
             __FILE__, __FUNCTION__, __LINE__, *layer_cnt, list->numHwLayers);
        return;
    }

    for (i = 0, j = 0; i < list->numHwLayers; i++) {
        int bpp = 0;
        hwc_layer_1_t *layer = NULL;

        if (list->numHwLayers <= tmp_cnt+i) {
            ALOGD("layers run out: %d", tmp_cnt+i);
            break;
        }
        layer = &list->hwLayers[tmp_cnt+i];
        if (layer == NULL) {
            ALOGE("%s %s %d: layer is NULL", __FILE__, __FUNCTION__, __LINE__);
            break;
        }

        *layer_cnt += 1;

        void *tmp_handle = (void *)(layer->handle);
        if (!tmp_handle) {
            usleep(10*1000);
            continue;
        }

        width =  WIDTH(layer->sourceCrop);
        height = HEIGHT(layer->sourceCrop);
        if ((width == 1) && (height == 1)) {
            continue;
        }

        if (layer->compositionType == HWC_FRAMEBUFFER) {
            continue;
        }

        //dump_layer(layer);
        if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
            int k;
            struct VideoFrameInfo * handle = (struct VideoFrameInfo *)(layer->handle);
            if (handle->dmmu_maped) {
                ALOGD("set_layers_config HWC_INGENIC_VIDEO_LAYER_X2D handle->dmmu_maped=%#x", handle->dmmu_maped);
            } else {
                ALOGE("HWC_INGENIC_VIDEO_LAYER_X2D~~~~~~~~~~~~~~>");
                ALOGE("bpp: %d, handle->width: %d, height: %d", bpp, handle->width, handle->height);
                for (k = 0; k < handle->count; k++) {
                    hwc_dev->video_rect[k][0] = (int)handle->addr[k];
                    hwc_dev->video_rect[k][1] = handle->size[k];
                    ALOGE("addr: %08x, size: %d", (unsigned int)handle->addr[k], handle->size[k]);
                }
            }
        } else {
            IMG_native_handle_t *handle = (IMG_native_handle_t *)(layer->handle);
            bpp = get_bpp(layer, (void *)handle);
            hwc_dev->handle_rect[j][0] = handle->iStride;
            hwc_dev->handle_rect[j][1] = handle->iHeight;
            hwc_dev->handle_rect[j][2] = bpp;
        }

        setup_config_addr(hwc_dev, &(hwc_dev->x2d_cfg.lay[j]), layer, j);
        setup_config_alpha(&hwc_dev->x2d_cfg.lay[j], layer);
        setup_config_transform(&hwc_dev->x2d_cfg.lay[j], layer);
        setup_config_format(&hwc_dev->x2d_cfg.lay[j], layer);
        setup_config_size(hwc_dev, &hwc_dev->x2d_cfg.lay[j], layer);

        j++;
        if (j == HW_SUPPORT_LAY_NUM) {
            break;
        }
    }
    ALOGD("hwc_dev->x2d_cfg.layer_num j: %d--------------->", j);
    hwc_dev->x2d_cfg.layer_num = j;
}

int jz_hwc_set(struct hwc_composer_device_1 *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || displays == NULL) {
        ALOGD("set: empty display list");
        return 0;
    }

    int ret = 0;
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *)dev;
    hwc_display_contents_1_t* list = displays[0];  // ignore displays beyond the first
    hwc_display_t dpy = NULL;
    hwc_surface_t sur = NULL;

#ifdef X2D_DEBUG
    android::StopWatch w("+++++++++++++++hwc_set layers");
#endif

    if (hwc_dev == NULL) {
        ALOGE("%s %s %d: invalid device", __FILE__, __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if (list == NULL) {
        // release our resources, the screen is turning off
        // in our case, there is nothing to do.
        return 0;
    }

    dpy = list->dpy;
    sur = list->sur;

    if (hwc_dev->render_layer_num > 0) {
        int i, cycle_count = 0;
        unsigned int layer_cnt = 0;
        cycle_count = hwc_dev->render_layer_num / 4 + 1;
        if (cycle_count && !(hwc_dev->render_layer_num % 4)) {
            cycle_count -= 1;
        }

        for (i = 0; i < cycle_count; i++) {
            /* set tlb_base */
            hwc_dev->x2d_cfg.tlb_base = hwc_dev->gtlb_base;

            /* set src layers && dst x2d_cfg */
            if (!i) {
                set_dst_config(hwc_dev);
            } else {
                set_dst_config2(hwc_dev);
            }

            set_layers_config(hwc_dev, list, &layer_cnt);
#ifdef X2D_DEBUG
            x2d_dump_cfg(hwc_dev);
#endif
            ret = ioctl(hwc_dev->x2d_fd, IOCTL_X2D_SET_CONFIG, &hwc_dev->x2d_cfg);
            if (ret < 0) {
                ALOGE("%s %s %d: IOCTL_X2D_SET_CONFIG failed", __FILE__, __FUNCTION__, __LINE__);
                goto err_set;
            }

            ret = ioctl(hwc_dev->x2d_fd, IOCTL_X2D_START_COMPOSE);
            if (ret < 0) {
                ALOGE("%s %s %d: IOCTL_X2D_START_COMPOSE failed", __FILE__, __FUNCTION__, __LINE__);
                dump_x2d_config(hwc_dev);
                goto err_set;
            }

            unmap_layers_addr(hwc_dev, list);
            memset((void *)&hwc_dev->x2d_cfg, 0, sizeof(struct jz_x2d_config));
        }
    }

    eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
    hwc_dev->fbDev->buf_index = (hwc_dev->fbDev->buf_index + 1) %
            (hwc_dev->fbDev->var_info.yres_virtual / hwc_dev->fbDev->var_info.yres);
    hwc_dev->render_layer_num = 0;

    return 0;

err_set:
    deinit_dev_para(hwc_dev);

    return ret;
}

int jz_hwc_prepare(struct hwc_composer_device_1 *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || displays == NULL) {
        return 0;
    }

    int video_layer_flag = 0;
    unsigned int i;
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *)dev;
    hwc_display_contents_1_t* list = displays[0];  // ignore displays beyond the first

    hwc_dev->render_layer_num = 0;

    if(!list || (!(list->flags & HWC_GEOMETRY_CHANGED))) {
        if(!list) {
            ALOGE("%s %s %d: !list", __FILE__, __FUNCTION__, __LINE__);
            return 0;
        }
    }

    if (hwc_dev == NULL) {
        ALOGE("%s %s %d: invalid device", __FILE__, __FUNCTION__, __LINE__);
        return -EINVAL;
    }


    for (i = 0; i < list->numHwLayers; i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        if (!is_support_this_layer(layer)) {
            //            ALOGE("Roll Back all the layers");
            return 0;
        }
        if (layer->flags == HWC_INGENIC_VIDEO_LAYER_X2D) {
            video_layer_flag = 1;
        }
    }

    if (!video_layer_flag) {
        return 0;
    }

    for (i = 0; i < list->numHwLayers; i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        //dump_layer(layer);
        layer->compositionType = HWC_OVERLAY;
        layer->hints = HWC_HINT_CLEAR_FB;

        hwc_dev->render_layer_num++;
    }
    
    ALOGD("hwc_dev->render_layer_num: %d", hwc_dev->render_layer_num);
    return 0;
}

int jz_hwc_device_close(hw_device_t *dev)
{
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *) dev;

    unmap_dst_addr(hwc_dev);

    if (dmmu_deinit() < 0) {
        ALOGE("%s %s %d: dmmu_deinit failed", __FILE__, __FUNCTION__, __LINE__);
    }

    if (hwc_dev) {
        if (hwc_dev->x2d_fd > 0) {
            close(hwc_dev->x2d_fd);
            /* pthread will be killed when parent process exits */
            pthread_mutex_destroy(&hwc_dev->lock);
            free(hwc_dev);
        }
    }

    ALOGD("Jz hwcomposer device closed!");
    return 0;
}

void handle_vsync_uevent(jz_hwc_device_t *hwc_dev, const char *buff, int len)
{
    uint64_t timestamp = 0;
    const char *s = buff;

    if(!hwc_dev->procs || !hwc_dev->procs->vsync)
        return;

    s += strlen(s) + 1;

    while(*s) {
        if (!strncmp(s, "VSYNC=", strlen("VSYNC=")))
            timestamp = strtoull(s + strlen("VSYNC="), NULL, 0);

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    hwc_dev->procs->vsync(hwc_dev->procs, 0, timestamp);
}

static void *jz_hwc_vsync_thread(void *data)
{
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *)data;
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();
    while(1) {
        int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);

        int vsync = !strcmp(uevent_desc, "change@/devices/platform/jz-fb.1");
    if(!vsync)
            vsync = !strcmp(uevent_desc, "change@/devices/platform/jz-fb.0");
        if(vsync)
            handle_vsync_uevent(hwc_dev, uevent_desc, len);
    }

    return NULL;
}

static int jz_hwc_eventControl(struct hwc_composer_device_1* dev,
                               int disp, int event, int enabled)
{
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *)dev;
    int val, err;

    switch (event) {
        case HWC_EVENT_VSYNC:
            val = enabled;
            err = ioctl(hwc_dev->fbDev->fd, JZFB_SET_VSYNCINT, &val);
            if (err < 0)
                return -errno;

            return 0;
    }

    return -EINVAL;
}

static int jz_hwc_query(struct hwc_composer_device_1* dev,
                        int what, int* value)
{
    jz_hwc_device_t *hwc_dev = (jz_hwc_device_t *) dev;

    switch (what) {
        default:
            // unsupported query
            return -EINVAL;
    }
    return 0;
}

static int jz_hwc_blank(struct hwc_composer_device_1 *dev,
                        int dpy, int blank)
{
    // We're using an older method of screen blanking based on
    // early_suspend in the kernel.  No need to do anything here.
    return 0;
}

int jz_hwc_device_open(const hw_module_t *module,
                       const char *name, hw_device_t **dev)
{
    int err = 0;
    jz_hwc_device_t *hwc_dev;
    IMG_native_handle_t *dst_handle;
    int num_of_fb;
    void *fb_base = NULL;
    unsigned int t_base = 0;

    ALOGD("jz_hwc_device_open*************");

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        ALOGD("%s(%d): invalid device name!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    /* alloc memory for hwc device */
    hwc_dev = (jz_hwc_device_t *)malloc(sizeof(struct jz_hwc_device));
    if (hwc_dev == NULL) {
        ALOGD("alloc mem for hwc dev failed!");
        return -ENOMEM;
    }
    memset(hwc_dev, 0, sizeof(struct jz_hwc_device));
    hwc_dev->fbDev = (struct hwc_framebuffer_info_t *)malloc(sizeof(struct hwc_framebuffer_info_t));
    if (hwc_dev->fbDev == NULL) {
        ALOGD("alloc mem for hwc dev failed!");
        return -ENOMEM;
    }
    memset(hwc_dev->fbDev, 0, sizeof(struct hwc_framebuffer_info_t));

    /* initialize variable */
    hwc_dev->base.common.tag      = HARDWARE_MODULE_TAG;
    hwc_dev->base.common.version  = HWC_DEVICE_API_VERSION_1_0;
    hwc_dev->base.common.module   = (hw_module_t *)module;

    /* initialize the procs */
    hwc_dev->base.common.close    = jz_hwc_device_close;
    hwc_dev->base.prepare         = jz_hwc_prepare;
    hwc_dev->base.set             = jz_hwc_set;
    hwc_dev->base.dump            = jz_hwc_dump;
    hwc_dev->base.registerProcs   = jz_hwc_registerProcs;
    hwc_dev->base.eventControl    = jz_hwc_eventControl;
    hwc_dev->base.query           = jz_hwc_query;
    hwc_dev->base.blank           = jz_hwc_blank;
    //    hwc_dev->fb_dev               = hwc_mod->fb_dev;

    /* return device handle */
    *dev = &hwc_dev->base.common;

    /* init pthread mutex */
    if (pthread_mutex_init(&hwc_dev->lock, NULL)) {
        ALOGE("%s %s %d: init pthread_mutex failed", __FILE__, __FUNCTION__, __LINE__);
        goto err_mutex;
    }

    if (dmmu_init() < 0) {
        ALOGE("%s %s %d: dmmu_init failed", __FILE__, __FUNCTION__, __LINE__);
        goto err_init;
    }

    err = dmmu_get_page_table_base_phys(&t_base);
    if (err < 0 || !t_base) {
        ALOGE("%s %s %d: dmmu_get_page_table_base_phys failed", __FILE__, __FUNCTION__, __LINE__);
        goto err_getphys;
    }
    hwc_dev->gtlb_base = t_base;

    // struct hw_module_t const* gl_module = (struct hw_module_t const*)hwc_dev->gralloc_module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (struct hw_module_t const**)&hwc_dev->gralloc_module) == 0) {
        /* open framebuffer */
        hwc_dev->fbDev->fd = open(FB0DEV, O_RDWR);
        if (hwc_dev->fbDev->fd < 0) {
            ALOGE("%s %s %d: fb0 open error", __FILE__, __FUNCTION__, __LINE__);
            goto err_init;
        }

        /* get framebuffer's var_info */
        if (ioctl(hwc_dev->fbDev->fd, FBIOGET_VSCREENINFO, &hwc_dev->fbDev->var_info) < 0) {
            ALOGE("%s %s %d: FBIOGET_VSCREENINFO failed", __FILE__, __FUNCTION__, __LINE__);
            goto err_getinfo;
        }

        /* get framebuffer's fix_info */
        if (ioctl(hwc_dev->fbDev->fd, FBIOGET_FSCREENINFO, &hwc_dev->fbDev->fix_info) < 0) {
            ALOGE("%s %s %d: FBIOGET_FSCREENINFO failed", __FILE__, __FUNCTION__, __LINE__);
            goto err_getinfo;
        }

        hwc_dev->fbDev->var_info.width = hwc_dev->fbDev->var_info.xres;
        hwc_dev->fbDev->var_info.height = hwc_dev->fbDev->var_info.yres;
        hwc_dev->fbDev->buf_index = 0; //this value is 2 in android 4.1 ; but use 0 in android 4.2

        hwc_dev->fbDev->size = hwc_dev->fbDev->fix_info.line_length * hwc_dev->fbDev->var_info.yres;
        num_of_fb = hwc_dev->fbDev->var_info.yres_virtual / hwc_dev->fbDev->var_info.yres;
        /* mmap framebuffer device */
        fb_base = mmap(0, hwc_dev->fbDev->size * num_of_fb, PROT_READ|PROT_WRITE,
                       MAP_SHARED, hwc_dev->fbDev->fd, 0);
        if (fb_base == MAP_FAILED) {
            ALOGE("%s %s %d: mmap failed", __FILE__, __FUNCTION__, __LINE__);
            goto err_getinfo;
        }

        /* get && map dst fb addr */
        int i;
        for (i = 0; i < num_of_fb; i++) {
            hwc_dev->fbDev->addr[i] = (unsigned int)fb_base + (hwc_dev->fbDev->size * i);
            memset((void *)hwc_dev->fbDev->addr[i], 0x0, hwc_dev->fbDev->size);

            err = map_dst_addr(hwc_dev, i);
            if (err < 0) {
                ALOGE("%s %s %d: map_dst_addr failed!", __FILE__, __FUNCTION__, __LINE__);
                goto err_map;
            }
        }
    } else {
        ALOGE("%s %s %d: hw_get_module failed", __FILE__, __FUNCTION__, __LINE__);
        goto err_init;
    }

    /* open x2d device */
    hwc_dev->x2d_fd = open(X2DNAME, O_RDWR);
    if (hwc_dev->x2d_fd < 0) {
        ALOGE("%s %s %d: open x2d device failed", __FILE__, __FUNCTION__, __LINE__);
        goto err_getinfo;
    }

    err = pthread_create(&hwc_dev->vsync_thread, NULL, jz_hwc_vsync_thread, hwc_dev);
    if (err) {
        ALOGE("%s::pthread_create() failed : %s", __func__, strerror(err));
        goto err_pthread_create;
    }

    ALOGE("open jz hwcomposer device success!\n");
    return 0;

err_pthread_create:
    close(hwc_dev->x2d_fd);
err_map:
    munmap(fb_base, hwc_dev->fbDev->size*num_of_fb);
err_getinfo:
    close(hwc_dev->fbDev->fd);
err_getphys:
    dmmu_deinit();
err_init:
    pthread_mutex_destroy(&hwc_dev->lock);
err_mutex:
    free(hwc_dev->fbDev);
    free(hwc_dev);
    *dev = NULL;

    return -EINVAL;
}

static struct hw_module_methods_t jz_hwc_module_methods = {
    open: jz_hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag:                  HARDWARE_MODULE_TAG,
        version_major:        1,
        version_minor:        0,
        id:                   HWC_HARDWARE_MODULE_ID,
        name:                 "Jz4780 Hardware Composer HAL",
        author:               "sw1-ingenic",
        methods:              &jz_hwc_module_methods,
    }
};
