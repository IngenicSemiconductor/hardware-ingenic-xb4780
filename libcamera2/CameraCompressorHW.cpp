/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2011 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define LOG_TAG "CameraCompressorHW"
//#define LOG_NDEBUG 0

#include "CameraCompressorHW.h"
#include "SkJpegUtility.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <jpeglib.h>
#include <jpegint.h>
#include <jinclude.h>
#include <jzasm.h>
#include <jzm_jpeg_enc.h>
#include <jerror.h>
#include <utils/Log.h>
#include <hardware/camera.h>

    extern int tcsm_fd;
    extern volatile unsigned char *sde_base;
    extern volatile unsigned char *tcsm0_base;
    extern volatile unsigned char *tcsm1_base;
    extern volatile unsigned char *sram_base;
    extern volatile unsigned char *gp0_base;
    extern volatile unsigned char *vpu_base;
    extern volatile unsigned char *cpm_base;
    extern volatile unsigned char *jpgc_base;

    typedef struct {
        struct jpeg_destination_mgr pub;

        char * outdata;
        int  *pSize;
        int nOutOffset;
        JOCTET * buffer;
    } my_destination_mgr;

    typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE 4096

    static void init_destination (j_compress_ptr cinfo) {

        my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

        /* Allocate the output buffer --- it will be released when done with image */
        dest->buffer = (JOCTET *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                        OUTPUT_BUF_SIZE * SIZEOF(JOCTET));

        dest->pub.next_output_byte = dest->buffer;
        dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    }

    static boolean empty_output_buffer2 (j_compress_ptr cinfo) {

        my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

        memcpy(dest->outdata+dest->nOutOffset,dest->buffer,OUTPUT_BUF_SIZE);
        dest->nOutOffset+=OUTPUT_BUF_SIZE;
        *(dest->pSize)=dest->nOutOffset;

        dest->pub.next_output_byte = dest->buffer;
        dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
        return TRUE;
    }

    static void term_destination2 (j_compress_ptr cinfo) {

        my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
        size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

        /* Write any data remaining in the buffer */
        if (datacount > 0) {
            memcpy(dest->outdata+dest->nOutOffset,dest->buffer,datacount);
            dest->nOutOffset+=datacount;
            *(dest->pSize)=dest->nOutOffset;

            dest->pub.next_output_byte = dest->buffer;
            dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
        }
    }

    static void jpeg_stdio_dest2 (j_compress_ptr cinfo, char * outdata, int *pSize) {

        my_dest_ptr dest;

        if (cinfo->dest == NULL) {
            cinfo->dest = (struct jpeg_destination_mgr *)
                (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                            SIZEOF(my_destination_mgr));
        }

        dest = (my_dest_ptr) cinfo->dest;
        dest->pub.init_destination = init_destination;
        dest->pub.empty_output_buffer = empty_output_buffer2;
        dest->pub.term_destination = term_destination2;

        dest->outdata = outdata;
        dest->nOutOffset = 0;
        dest->pSize = pSize;
        *(dest->pSize)= 0;
    }


#ifdef __cplusplus
}
#endif


    extern void VAE_map();
    extern void VAE_unmap();
    extern void Lock_Vpu();
    extern void UnLock_Vpu();

namespace android{

    CameraCompressorHW::CameraCompressorHW()
    {
        memset(&mcparamsHW, 0, sizeof(struct compress_params_hw));
    }

    int CameraCompressorHW::setPrameters(compress_params_hw_ptr hw_cinfo) {

        mcparamsHW.pictureYUV420_y = hw_cinfo->pictureYUV420_y;
        mcparamsHW.pictureYUV420_c = hw_cinfo->pictureYUV420_c;
        mcparamsHW.pictureWidth = hw_cinfo->pictureWidth;
        mcparamsHW.pictureHeight = hw_cinfo->pictureHeight;
        mcparamsHW.pictureQuality = hw_cinfo->pictureQuality;

        mcparamsHW.thumbnailWidth = hw_cinfo->thumbnailWidth;
        mcparamsHW.thumbnailHeight = hw_cinfo->thumbnailHeight;
        mcparamsHW.thumbnailQuality = hw_cinfo->thumbnailQuality;

        mcparamsHW.format = hw_cinfo->format;

        mcparamsHW.jpeg_out = hw_cinfo->jpeg_out;
        mcparamsHW.jpeg_size = hw_cinfo->jpeg_size;

        mcparamsHW.th_jpeg_out = hw_cinfo->th_jpeg_out;
        mcparamsHW.th_jpeg_size = hw_cinfo->th_jpeg_size;

        mcparamsHW.tlb_addr = hw_cinfo->tlb_addr;

        mcparamsHW.requiredMem = hw_cinfo->requiredMem;

        return 0;
    }

    int CameraCompressorHW::hw_compress_to_jpeg() {

        struct camera_buffer tmp_pjpeg_bsa;
        struct camera_buffer tmp_reginfo;

        unsigned int time1, time2;

        unsigned char* p_jpeg_bsa = NULL;
        unsigned int* hw_reginfo = NULL;

        memset(&tmp_pjpeg_bsa, 0, sizeof(struct camera_buffer));
        memset(&tmp_reginfo, 0, sizeof(struct camera_buffer));

        int w = mcparamsHW.pictureWidth;
        int h = mcparamsHW.pictureHeight;

        int y_size = w * h;
        int th_w = mcparamsHW.thumbnailWidth;
        int th_h = mcparamsHW.thumbnailHeight;

        int p_q = mcparamsHW.pictureQuality;
        int th_q = mcparamsHW.thumbnailQuality;

        int* th_jpeg_size = mcparamsHW.th_jpeg_size;
        int* jpeg_size = mcparamsHW.jpeg_size;

        unsigned char* th_jpeg_out = mcparamsHW.th_jpeg_out;
        unsigned char* jpeg_out = mcparamsHW.jpeg_out;

        unsigned int tlb_addr = mcparamsHW.tlb_addr;

        ALOGV("jpeg_out address = %p",jpeg_out);
        ALOGV("jpeg_size address = %p",jpeg_size);
        ALOGV("tlb_address = %08x",tlb_addr);

        int size_py = w * h +255;
        int size_pc = w * h / 2 + 255;
        int size_jpeg = w * h * 3 + 255;
        int size_reginfo = 10000;

        if((w%16 != 0) || (h%16 != 0)) {
            ALOGE("%s: pictureWidth = %d, pictureHeight = %d",__FUNCTION__, w, h);
            return 1;
        }

        camera_mem_alloc(&tmp_pjpeg_bsa, size_jpeg, 1);
        camera_mem_alloc(&tmp_reginfo, size_reginfo, 1);

        if(tmp_pjpeg_bsa.common==NULL || tmp_reginfo.common==NULL) {

            ALOGE("malloc error");
            return 2;
        }

        p_jpeg_bsa = (unsigned char *)(((int)(tmp_pjpeg_bsa.common->data) + 255) & ~0xFF);
        hw_reginfo = (unsigned int *)(((int)(tmp_reginfo.common->data) + 255) & ~0xFF);


        ALOGV("hw_reginfo_address = %p",hw_reginfo);

        Lock_Vpu();
        put_image_jpeg(mcparamsHW.pictureYUV420_y, mcparamsHW.pictureYUV420_c, p_jpeg_bsa, hw_reginfo, w, h, p_q, jpeg_out, jpeg_size, tlb_addr);
        UnLock_Vpu();

        camera_mem_free(&tmp_pjpeg_bsa);
        camera_mem_free(&tmp_reginfo);

        return 0;
    }

    void CameraCompressorHW::rgb565_to_jpeg(unsigned char* dest_img, int *dest_size, unsigned char* rgb,
                                         int width, int height,int quality) {
        struct jpeg_compress_struct cinfo;
        skjpeg_error_mgr        sk_err;
        SkDynamicMemoryWStream stream;
        skjpeg_destination_mgr  sk_wstream(&stream);

        ALOGV("%s: begin to start compress",__FUNCTION__);

        cinfo.err = jpeg_std_error(&sk_err);
        sk_err.error_exit = skjpeg_error_exit;
        if (setjmp(sk_err.fJmpBuf)) {
            ALOGE("%s: compress error",__FUNCTION__);
            return;
        }

        jpeg_create_compress(&cinfo);
        cinfo.dest = &sk_wstream;
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);

        jpeg_set_quality(&cinfo,quality,TRUE);
        jpeg_start_compress(&cinfo,TRUE);

        int line_legth = width*2;
        unsigned char* line = rgb;
        for (int m=0; m < height; ++m, line += line_legth) {
            jpeg_write_scanlines(&cinfo, &line, 1);
        }
        jpeg_finish_compress(&cinfo);
        *dest_size = stream.getOffset();
        stream.copyTo((void*)dest_img);
        stream.reset();
        jpeg_destroy_compress(&cinfo);
        ALOGV("%s: compress %dx%d => %d bypes",__FUNCTION__, width, height,
              *dest_size);
    }

    int CameraCompressorHW::getpictureYuv420Data( unsigned char* yuv_y, unsigned char* yuv_c) {

        return 0;
    }

    int CameraCompressorHW::getthumbnailYuv420Data( unsigned char* yuv_y, unsigned char* yuv_c) {
        return 0;
    }

    int CameraCompressorHW::put_image_jpeg(unsigned char* yuv_y, unsigned char* yuv_c, 
                                           unsigned char* bsa, unsigned int* reginfo, int w, int h,
                                           int quality, unsigned char* jpeg_out, int *size, 
                                           unsigned int tlb_addr) {

        struct jpeg_compress_struct cjpeg;
        struct jpeg_error_mgr jerr;
        int Q_tbl = 0;

        memset(&cjpeg, 0, sizeof(struct jpeg_compress_struct));
        memset(&jerr, 0, sizeof(struct jpeg_error_mgr));

        if(quality < 85) {
            Q_tbl = 0; 
        }
        else if(85 <= quality && quality < 95) {
            Q_tbl = 1; 
        }
        else {
            Q_tbl = 2; 
        }

        cjpeg.err = jpeg_std_error(&jerr);
        jpeg_create_compress (&cjpeg);

        cjpeg.image_width = w;
        cjpeg.image_height= h;

        cjpeg.input_components = 3;
        cjpeg.num_components = 1;

        cjpeg.in_color_space =JCS_YCbCr;
        jpeg_set_defaults (&cjpeg);

        jz_jpeg_set_quality(&cjpeg, quality);

        cjpeg.dct_method = JDCT_FLOAT;
        jpeg_stdio_dest2 (&cjpeg,(char *)jpeg_out,size);

        jpeg_start_compress (&cjpeg, TRUE);


        /* output frame/scan headers*/
        if (cjpeg.master->call_pass_startup)
            (*cjpeg.master->pass_startup) (&cjpeg);

        term_destination2 (&cjpeg);

        /*init reg input data */
        _JPEGE_SliceInfo* s = NULL;
        s = (_JPEGE_SliceInfo *)malloc(sizeof(_JPEGE_SliceInfo));

        s->des_va = reginfo;
        s->des_pa = reginfo;

        ALOGV("s->des_va = %08x, s->des_pa = %08x",(int)s->des_va, (int)s->des_pa);

        s->ncol = 0x2;                   /* number of color/components of a MCU minus one */
        s->bsa = bsa;                   /* bitstream buffer address  */
        s->p0a = yuv_y;
        s->p1a = yuv_c;                 /* componet 0-3 plane buffer address */
        s->nrsm = 0x0;                   /* Re-Sync-Marker gap number */
        s->nmcu = w * h / 256 -1;                  /* number of MCU minus one */
        s->ql_sel = Q_tbl;
        s->huffenc_sel = 0;           /* Huffman ENC Table select */


        ALOGV("s->bsa = %08x, s->p0a = %08x, s->p1a = %08x",
                 (int)s->bsa, (int)s->p0a, (int)s->p1a);
        ALOGV("s->ncol = %d, s->nrsm = %d, s->nmcu = %d, s->ql_sel = %d", 
                 (int)s->ncol, (int)s->nrsm, (int)s->nmcu, (int)s->ql_sel);
        ALOGV("tlb_addr = %08x",tlb_addr);

        VAE_map();  //vae_map
        usleep(100);

        RST_VPU();   //vpu reset

        /* using tlb */
        // write_reg(vpu_base,SCH_GLBC_HIAXI | (0x1<<30) |  (0x1<<29));

        *((volatile unsigned int *)(vpu_base + REG_SCH_GLBC)) = (SCH_GLBC_HIAXI
                                                                 | SCH_GLBC_TLBE | SCH_GLBC_TLBINV
                                                                 | SCH_INTE_ACFGERR
                                                                 | SCH_INTE_TLBERR
                                                                 | SCH_INTE_ENDF
                                                                 );


        *((volatile unsigned int *)(vpu_base + REG_SCH_TLBA)) = tlb_addr;

        ALOGV("vpu_base + REG_SCH_TLBA = %08x\n",
                 (unsigned int)read_reg(vpu_base, 0x30));

        JPEGE_SliceInit(s);   //write reg

        jz_dcache_wb();

        write_reg( gp0_base + 0x8 , VDMA_ACFG_DHA(s->des_pa) | VDMA_ACFG_RUN);  //vpu working

        int a = 0;
        ioctl(tcsm_fd, 0, &a);
        if ((a & 0x11) == 0x11){
        }else{
            ALOGE("REG_JPGC_GLBI = %08x\n",
                  (unsigned int)read_reg(jpgc_base, 0x04));
            ALOGE("VDMA task trigger = %08x\n",
                  (unsigned int)read_reg(gp0_base, 0x8));

        }

        int bslen = read_reg(jpgc_base, 0x8) & 0xFFFFFF;

        VAE_unmap();

        /*push data*/
        my_dest_ptr dest_tmp = (my_dest_ptr)cjpeg.dest;
        memcpy(dest_tmp->outdata+dest_tmp->nOutOffset, bsa, bslen);
        dest_tmp->nOutOffset+=bslen;
        *(dest_tmp->pSize)=dest_tmp->nOutOffset;

        /*set cjpeg state*/
        cjpeg.global_state == CSTATE_RAW_OK;
        cjpeg.next_scanline = cjpeg.image_height;
        cjpeg.master->is_last_pass = 0x1;

        ALOGV("dest_tmp->outdata = %08x, *(dest_tmp->pSize) = %d ",
                 (unsigned int ) dest_tmp->outdata, *(dest_tmp->pSize));

        jpeg_finish_compress (&cjpeg);

        jpeg_destroy_compress (&cjpeg);

        ALOGV("end work.......................................");
        return 0;
    }

    /* This fuction used for hardware encoder*/
    void CameraCompressorHW::yuv422i_to_yuv420_block(unsigned char *dsty,unsigned char *dstc,
                                                     char *inyuv,int w,int h) {

        ALOGV("%s: start doing yuv422i_to_yuv420_block",__FUNCTION__);

        int i = 0, j = 0;
        int x = 0, y = 0;

        unsigned char* outy = NULL; 
        unsigned char* outcb = NULL;
        unsigned char* outcr = NULL;
        unsigned int*  inyuv_4 = NULL; 
        unsigned int* yuv_tmp = NULL;
        unsigned int temp = 0;
        int outYsize = 0, w_offset = 0, h_offset = 0;

        int width = ((w + 15) >> 4) << 4;
        int height = ((h + 15) >> 4) << 4;

        outYsize = (width * height);

        inyuv_4 = (uint *)inyuv;

        outy = dsty;
        outcb = dstc + 8;
        outcr = dstc;

        w_offset = width - w;
        h_offset = height -h;

        ALOGV("  show me w_offset=%d,  h_offset=%d",w_offset,h_offset);
        ALOGV("width = %08x,height= %08x,w= %08x,h= %08x",width,height,w,h);

        for (y = 0; y < height>>4 ; y++) {
            ALOGV("show me y = %d",y);

            for (x = 0; x < width>>4 ; x++) {

                for (i=0; i<16 ; i++) {
                    yuv_tmp = inyuv_4 + (y*16+i)*width/2  + x*8;
                    for (j=0; j<8; j++) {
                        if (i%2) {
                            if(((y*16+i) >= h) || ((x*16+j*2) >= w)) {
                                ALOGV("out of range");
                                *outy++ = 0;
                                *outy++ = 0;
                            }
                            else {
                                temp = *yuv_tmp++;
                                *outy++ = (unsigned char)((temp >> 0) & 0xFF);
                                *outy++ = (unsigned char)((temp >> 16) & 0xFF);
                            }
                        }
                        else {
                            if (((y*16+i) >= h) || ((x*16+j*2) >= w)) {
                                ALOGV("out of range");
                                *outcb++ = 0;
                                *outy++ = 0;
                                *outcr++ = 0;
                                *outy++ = 0;
                            }
                            else {
                                temp = *yuv_tmp++;
                                *outcr++ = (unsigned char)((temp >> 8) & 0xFF);
                                *outy++ = (unsigned char)((temp >> 0) & 0xFF);
                                *outcb++ = (unsigned char)((temp >> 24) & 0xFF);
                                *outy++ = (unsigned char)((temp >> 16) & 0xFF);
                            }
                        }
                    }

                    if (i%2) {
                        outcr += 8;
                        outcb += 8;
                    }
                }
            }
        }
    }

    void CameraCompressorHW::camera_mem_free(struct camera_buffer* buf) {

        if(buf->common != NULL) {
            ALOGV("now doing camera_mem_free");
            dmmu_unmap_user_memory(&buf->dmmu_info);
            buf->common->release(buf->common);
            buf->common = NULL;
            memset(buf,0,sizeof(struct camera_buffer));
        }
    }

    /*when first using struct camera_buffer* buf , setting buf->common = NULL*/
    int CameraCompressorHW::camera_mem_alloc(struct camera_buffer* buf,int size,int nr) {

        ALOGV("now doing camera_mem_alloc");

        if(buf->common) {
            camera_mem_free(buf);
        }

        buf->common = mcparamsHW.requiredMem(-1,size,nr,NULL);

        ALOGV(" get_buffer hou %p",buf->common->data);

        if(!buf->common) {
            ALOGE("%s failed.",__func__);
            return -1;
        }

        buf->dmmu_info.vaddr = buf->common->data;
        buf->dmmu_info.size = size*nr;

        memset(buf->common->data,0,size*nr);

        dmmu_map_user_memory(&buf->dmmu_info);
        return 0;
    }
};

