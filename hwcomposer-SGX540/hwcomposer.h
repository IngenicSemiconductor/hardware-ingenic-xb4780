#ifndef __JZ_HWCOMPOSER_H__
#define __JZ_HWCOMPOSER_H__

__BEGIN_DECLS

#define MAX_NUM_FRAMEBUFFERS            3
#define X2D_IOCTL_MAGIC 'X'

#define JZFB_SET_VSYNCINT		_IOW('F', 0x210, int)

#define IOCTL_X2D_SET_CONFIG			_IOW(X2D_IOCTL_MAGIC, 0x01, struct jz_x2d_config)
#define IOCTL_X2D_START_COMPOSE		    _IO(X2D_IOCTL_MAGIC, 0x02)
#define IOCTL_X2D_GET_SYSINFO		    _IOR(X2D_IOCTL_MAGIC, 0x03, struct jz_x2d_config)
#define IOCTL_X2D_STOP					_IO(X2D_IOCTL_MAGIC, 0x04)

#define X2D_INFORMAT_NOTSUPPORT		-1
#define X2D_RGBORDER_RGB 		 0
#define X2D_RGBORDER_BGR		 1
#define X2D_RGBORDER_GRB		 2
#define X2D_RGBORDER_BRG		 3
#define X2D_RGBORDER_RBG		 4
#define X2D_RGBORDER_GBR		 5

#define X2D_ALPHA_POSLOW		 1
#define X2D_ALPHA_POSHIGH		 0

#define X2D_H_MIRROR			 4
#define X2D_V_MIRROR			 8
#define X2D_ROTATE_0			 0
#define	X2D_ROTATE_90 			 1
#define X2D_ROTATE_180			 2
#define X2D_ROTATE_270			 3

#define X2D_INFORMAT_ARGB888		0
#define X2D_INFORMAT_RGB555		1
#define X2D_INFORMAT_RGB565		2
#define X2D_INFORMAT_YUV420SP		3
#define X2D_INFORMAT_TILE420		4
#define X2D_INFORMAT_NV12		5
#define X2D_INFORMAT_NV21		6

#define X2D_OUTFORMAT_ARGB888		0
#define X2D_OUTFORMAT_XRGB888		1
#define X2D_OUTFORMAT_RGB565		2
#define X2D_OUTFORMAT_RGB555		3

#define X2D_OSD_MOD_CLEAR		3
#define X2D_OSD_MOD_SOURCE		1
#define X2D_OSD_MOD_DST			2
#define X2D_OSD_MOD_SRC_OVER		0
#define X2D_OSD_MOD_DST_OVER		4
#define X2D_OSD_MOD_SRC_IN		5
#define X2D_OSD_MOD_DST_IN		6
#define X2D_OSD_MOD_SRC_OUT		7
#define X2D_OSD_MOD_DST_OUT		8
#define X2D_OSD_MOD_SRC_ATOP		9
#define X2D_OSD_MOD_DST_ATOP		0xa
#define X2D_OSD_MOD_XOR			0xb

struct VideoFrameInfo {
    int32_t index;
    int32_t count;
    int32_t format;
    int32_t width;						/* in pixel */
    int32_t height;						/* in pixel */
    int32_t offset[4];					/* in byte */
    int32_t stride[4];					/* in byte */
    void * addr[4];
    int32_t size[4];					/* in byte */
    int32_t dmmu_maped;                                 /* mapVideoBuffer in SurfaceFlinger */

	int32_t out_width;
	int32_t out_height;
	int32_t out_w_offset;
	int32_t out_h_offset;
//#ifdef __cplusplus
//    sp<IMemory> mVideoMem[4];
//    sp<IMemoryHeap> mVideoMemHeap[4];
//#endif
};

struct hwc_framebuffer_info_t {
    int        fd;
    int        size;
    //    sec_rect   rect_info;
    uint32_t   addr[MAX_NUM_FRAMEBUFFERS];
    int        buf_index;
    int        power_state;
    int        blending;
    int        layer_index;
    uint32_t   layer_prev_buf;
    int        set_win_flag;
    int        status;
    int        vsync;

    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
    struct fb_var_screeninfo lcd_info;
};

struct src_layer {
    int format;
    int transform;		// such as rotate or mirror
    int global_alpha_val;
    int argb_order;
    int osd_mode;
    int preRGB_en;
    int glb_alpha_en;
    int mask_en;
    int color_cov_en;
    //input  output size
    int in_width;		//LAY_SGS
    int in_height;
	int in_w_offset;
	int in_h_offset;
    int out_width;		//LAY_OGS
    int out_height;
    int out_w_offset;		//LAY_OOSF
    int out_h_offset;

    int v_scale_ratio;
    int h_scale_ratio;
    int msk_val;

    //yuv address
    int addr;
    int u_addr;
    int v_addr;
    int y_stride;
    int v_stride;
};

struct jz_x2d_config{
    //global
    int watchdog_cnt;
    unsigned int tlb_base;

    //dst
    int dst_address;
    int dst_alpha_val;
    int dst_stride;
    int dst_mask_val;
    int dst_width;
    int dst_height;
    int dst_bcground;
    int dst_format;
    int dst_back_en;
    int dst_preRGB_en;
    int dst_glb_alpha_en;
    int dst_mask_en;
    //int dst_backpic_alpha_en;

    //src layers
    int layer_num;
    struct src_layer lay[4];
};


enum {
    HWC_IPU_LAYER = 0x00000020,
    HWC_INGENIC_VIDEO_LAYER_X2D = 0x00000100,
    HWC_INGENIC_VIDEO_LAYER_DFB = 0x00000200,
};

__END_DECLS

#endif
