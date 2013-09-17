###### variable declarations #######

# muxers/demuxers
SRCS_COMMON = allformats.c         \
       cutils.c             \
       metadata.c           \
       metadata_compat.c    \
       options.c            \
       os_support.c         \
       sdp.c                \
       seek.c               \
       utils.c              \

HEADERS = avformat.h avio.h rtp.h rtsp.h rtspcodes.h

# muxers/demuxers
SRCS_COMMON-$(CONFIG_A64_MUXER)                 += a64.c
SRCS_COMMON-$(CONFIG_AAC_DEMUXER)               += aacdec.c id3v1.c id3v2.c rawdec.c
SRCS_COMMON-$(CONFIG_AC3_DEMUXER)               += ac3dec.c rawdec.c
SRCS_COMMON-$(CONFIG_AC3_MUXER)                 += rawenc.c
SRCS_COMMON-$(CONFIG_ADTS_MUXER)                += adtsenc.c
SRCS_COMMON-$(CONFIG_AEA_DEMUXER)               += aea.c pcm.c
SRCS_COMMON-$(CONFIG_AIFF_DEMUXER)              += aiffdec.c riff.c pcm.c
SRCS_COMMON-$(CONFIG_AIFF_MUXER)                += aiffenc.c riff.c
SRCS_COMMON-$(CONFIG_AMR_DEMUXER)               += amr.c
SRCS_COMMON-$(CONFIG_AMR_MUXER)                 += amr.c
SRCS_COMMON-$(CONFIG_ANM_DEMUXER)               += anm.c
SRCS_COMMON-$(CONFIG_APC_DEMUXER)               += apc.c
SRCS_COMMON-$(CONFIG_APE_DEMUXER)               += ape.c apetag.c
SRCS_COMMON-$(CONFIG_APPLEHTTP_DEMUXER)         += applehttp.c
SRCS_COMMON-$(CONFIG_ASF_DEMUXER)               += asfdec.c asf.c asfcrypt.c \
                                            riff.c avlanguage.c
SRCS_COMMON-$(CONFIG_ASF_MUXER)                 += asfenc.c asf.c riff.c
SRCS_COMMON-$(CONFIG_ASS_DEMUXER)               += assdec.c
SRCS_COMMON-$(CONFIG_ASS_MUXER)                 += assenc.c
SRCS_COMMON-$(CONFIG_AU_DEMUXER)                += au.c pcm.c
SRCS_COMMON-$(CONFIG_AU_MUXER)                  += au.c
SRCS_COMMON-$(CONFIG_AVI_DEMUXER)               += avidec.c riff.c avi.c
SRCS_COMMON-$(CONFIG_AVI_MUXER)                 += avienc.c riff.c avi.c
SRCS_COMMON-$(CONFIG_AVISYNTH)                  += avisynth.c
SRCS_COMMON-$(CONFIG_AVM2_MUXER)                += swfenc.c
SRCS_COMMON-$(CONFIG_AVS_DEMUXER)               += avs.c vocdec.c voc.c
SRCS_COMMON-$(CONFIG_BETHSOFTVID_DEMUXER)       += bethsoftvid.c
SRCS_COMMON-$(CONFIG_BFI_DEMUXER)               += bfi.c
SRCS_COMMON-$(CONFIG_BINK_DEMUXER)              += bink.c
SRCS_COMMON-$(CONFIG_C93_DEMUXER)               += c93.c vocdec.c voc.c
SRCS_COMMON-$(CONFIG_CAF_DEMUXER)               += cafdec.c caf.c mov.c riff.c isom.c
SRCS_COMMON-$(CONFIG_CAVSVIDEO_DEMUXER)         += cavsvideodec.c rawdec.c
SRCS_COMMON-$(CONFIG_CDG_DEMUXER)               += cdg.c
SRCS_COMMON-$(CONFIG_CRC_MUXER)                 += crcenc.c
SRCS_COMMON-$(CONFIG_DAUD_DEMUXER)              += daud.c
SRCS_COMMON-$(CONFIG_DAUD_MUXER)                += daud.c
SRCS_COMMON-$(CONFIG_DIRAC_DEMUXER)             += diracdec.c rawdec.c
SRCS_COMMON-$(CONFIG_DIRAC_MUXER)               += rawenc.c
SRCS_COMMON-$(CONFIG_DNXHD_DEMUXER)             += dnxhddec.c rawdec.c
SRCS_COMMON-$(CONFIG_DNXHD_MUXER)               += rawenc.c
SRCS_COMMON-$(CONFIG_DSICIN_DEMUXER)            += dsicin.c
SRCS_COMMON-$(CONFIG_DTS_DEMUXER)               += dtsdec.c rawdec.c
SRCS_COMMON-$(CONFIG_DTS_MUXER)                 += rawenc.c
SRCS_COMMON-$(CONFIG_DV_DEMUXER)                += dv.c
SRCS_COMMON-$(CONFIG_DV_MUXER)                  += dvenc.c
SRCS_COMMON-$(CONFIG_DXA_DEMUXER)               += dxa.c riff.c
SRCS_COMMON-$(CONFIG_EA_CDATA_DEMUXER)          += eacdata.c
SRCS_COMMON-$(CONFIG_EA_DEMUXER)                += electronicarts.c
SRCS_COMMON-$(CONFIG_EAC3_DEMUXER)              += ac3dec.c rawdec.c
SRCS_COMMON-$(CONFIG_EAC3_MUXER)                += rawenc.c
SRCS_COMMON-$(CONFIG_FFM_DEMUXER)               += ffmdec.c
SRCS_COMMON-$(CONFIG_FFM_MUXER)                 += ffmenc.c
SRCS_COMMON-$(CONFIG_FILMSTRIP_DEMUXER)         += filmstripdec.c
SRCS_COMMON-$(CONFIG_FILMSTRIP_MUXER)           += filmstripenc.c
SRCS_COMMON-$(CONFIG_FLAC_DEMUXER)              += flacdec.c rawdec.c id3v1.c \
                                            id3v2.c oggparsevorbis.c \
                                            vorbiscomment.c
SRCS_COMMON-$(CONFIG_FLAC_MUXER)                += flacenc.c flacenc_header.c \
                                            vorbiscomment.c
SRCS_COMMON-$(CONFIG_FLIC_DEMUXER)              += flic.c
SRCS_COMMON-$(CONFIG_FLV_DEMUXER)               += flvdec.c
SRCS_COMMON-$(CONFIG_FLV_MUXER)                 += flvenc.c avc.c
SRCS_COMMON-$(CONFIG_FOURXM_DEMUXER)            += 4xm.c
SRCS_COMMON-$(CONFIG_FRAMECRC_MUXER)            += framecrcenc.c
SRCS_COMMON-$(CONFIG_FRAMEMD5_MUXER)            += md5enc.c
SRCS_COMMON-$(CONFIG_GIF_MUXER)                 += gif.c
SRCS_COMMON-$(CONFIG_GSM_DEMUXER)               += rawdec.c
SRCS_COMMON-$(CONFIG_GXF_DEMUXER)               += gxf.c
SRCS_COMMON-$(CONFIG_GXF_MUXER)                 += gxfenc.c audiointerleave.c
SRCS_COMMON-$(CONFIG_H261_DEMUXER)              += h261dec.c rawdec.c
SRCS_COMMON-$(CONFIG_H261_MUXER)                += rawenc.c
SRCS_COMMON-$(CONFIG_H263_DEMUXER)              += h263dec.c rawdec.c
SRCS_COMMON-$(CONFIG_H263_MUXER)                += rawenc.c
SRCS_COMMON-$(CONFIG_H264_DEMUXER)              += h264dec.c rawdec.c
SRCS_COMMON-$(CONFIG_H264_MUXER)                += rawenc.c
SRCS_COMMON-$(CONFIG_IDCIN_DEMUXER)             += idcin.c
SRCS_COMMON-$(CONFIG_IFF_DEMUXER)               += iff.c
SRCS_COMMON-$(CONFIG_IMAGE2_DEMUXER)            += img2.c
SRCS_COMMON-$(CONFIG_IMAGE2_MUXER)              += img2.c
SRCS_COMMON-$(CONFIG_IMAGE2PIPE_DEMUXER)        += img2.c
SRCS_COMMON-$(CONFIG_IMAGE2PIPE_MUXER)          += img2.c
SRCS_COMMON-$(CONFIG_INGENIENT_DEMUXER)         += ingenientdec.c rawdec.c
SRCS_COMMON-$(CONFIG_IPMOVIE_DEMUXER)           += ipmovie.c
SRCS_COMMON-$(CONFIG_ISS_DEMUXER)               += iss.c
SRCS_COMMON-$(CONFIG_IV8_DEMUXER)               += iv8.c
SRCS_COMMON-$(CONFIG_IVF_DEMUXER)               += ivfdec.c riff.c
SRCS_COMMON-$(CONFIG_LMLM4_DEMUXER)             += lmlm4.c
SRCS_COMMON-$(CONFIG_M4V_DEMUXER)               += m4vdec.c rawdec.c
SRCS_COMMON-$(CONFIG_M4V_MUXER)                 += rawenc.c
SRCS_COMMON-$(CONFIG_MATROSKA_DEMUXER)          += matroskadec.c matroska.c \
                                            riff.c isom.c rmdec.c rm.c
SRCS_COMMON-$(CONFIG_MATROSKA_MUXER)            += matroskaenc.c matroska.c \
                                            riff.c isom.c avc.c \
                                            flacenc_header.c
SRCS_COMMON-$(CONFIG_MD5_MUXER)                 += md5enc.c
SRCS_COMMON-$(CONFIG_MJPEG_DEMUXER)             += rawdec.c
SRCS_COMMON-$(CONFIG_MJPEG_MUXER)               += rawenc.c
SRCS_COMMON-$(CONFIG_MLP_DEMUXER)               += rawdec.c
SRCS_COMMON-$(CONFIG_MLP_MUXER)                 += rawenc.c
SRCS_COMMON-$(CONFIG_MM_DEMUXER)                += mm.c
SRCS_COMMON-$(CONFIG_MMF_DEMUXER)               += mmf.c pcm.c
SRCS_COMMON-$(CONFIG_MMF_MUXER)                 += mmf.c riff.c
SRCS_COMMON-$(CONFIG_MOV_DEMUXER)               += mov.c riff.c isom.c
SRCS_COMMON-$(CONFIG_MOV_MUXER)                 += movenc.c riff.c isom.c avc.c movenchint.c
SRCS_COMMON-$(CONFIG_MP2_MUXER)                 += mp3.c id3v1.c id3v2.c
SRCS_COMMON-$(CONFIG_MP3_DEMUXER)               += mp3.c id3v1.c id3v2.c
SRCS_COMMON-$(CONFIG_MP3_MUXER)                 += mp3.c id3v1.c id3v2.c
SRCS_COMMON-$(CONFIG_MPC_DEMUXER)               += mpc.c id3v1.c id3v2.c apetag.c
SRCS_COMMON-$(CONFIG_MPC8_DEMUXER)              += mpc8.c
SRCS_COMMON-$(CONFIG_MPEG1SYSTEM_MUXER)         += mpegenc.c
SRCS_COMMON-$(CONFIG_MPEG1VCD_MUXER)            += mpegenc.c
SRCS_COMMON-$(CONFIG_MPEG2DVD_MUXER)            += mpegenc.c
SRCS_COMMON-$(CONFIG_MPEG2VOB_MUXER)            += mpegenc.c
SRCS_COMMON-$(CONFIG_MPEG2SVCD_MUXER)           += mpegenc.c
SRCS_COMMON-$(CONFIG_MPEG1VIDEO_MUXER)          += rawenc.c
SRCS_COMMON-$(CONFIG_MPEG2VIDEO_MUXER)          += rawenc.c
SRCS_COMMON-$(CONFIG_MPEGPS_DEMUXER)            += mpeg.c
SRCS_COMMON-$(CONFIG_MPEGTS_DEMUXER)            += mpegts.c
SRCS_COMMON-$(CONFIG_MPEGTS_MUXER)              += mpegtsenc.c adtsenc.c
SRCS_COMMON-$(CONFIG_MPEGVIDEO_DEMUXER)         += mpegvideodec.c rawdec.c
SRCS_COMMON-$(CONFIG_MPJPEG_MUXER)              += mpjpeg.c
SRCS_COMMON-$(CONFIG_MSNWC_TCP_DEMUXER)         += msnwc_tcp.c
SRCS_COMMON-$(CONFIG_MTV_DEMUXER)               += mtv.c
SRCS_COMMON-$(CONFIG_MVI_DEMUXER)               += mvi.c
SRCS_COMMON-$(CONFIG_MXF_DEMUXER)               += mxfdec.c mxf.c
SRCS_COMMON-$(CONFIG_MXF_MUXER)                 += mxfenc.c mxf.c audiointerleave.c
SRCS_COMMON-$(CONFIG_NC_DEMUXER)                += ncdec.c
SRCS_COMMON-$(CONFIG_NSV_DEMUXER)               += nsvdec.c
SRCS_COMMON-$(CONFIG_NULL_MUXER)                += nullenc.c
SRCS_COMMON-$(CONFIG_NUT_DEMUXER)               += nutdec.c nut.c riff.c
SRCS_COMMON-$(CONFIG_NUT_MUXER)                 += nutenc.c nut.c riff.c
SRCS_COMMON-$(CONFIG_NUV_DEMUXER)               += nuv.c riff.c
SRCS_COMMON-$(CONFIG_OGG_DEMUXER)               += oggdec.c         \
                                            oggparsedirac.c  \
                                            oggparseflac.c   \
                                            oggparseogm.c    \
                                            oggparseskeleton.c \
                                            oggparsespeex.c  \
                                            oggparsetheora.c \
                                            oggparsevorbis.c \
                                            riff.c \
                                            vorbiscomment.c
SRCS_COMMON-$(CONFIG_OGG_MUXER)                 += oggenc.c \
                                            vorbiscomment.c
SRCS_COMMON-$(CONFIG_OMA_DEMUXER)               += oma.c pcm.c id3v2.c id3v1.c
SRCS_COMMON-$(CONFIG_PCM_ALAW_DEMUXER)          += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_ALAW_MUXER)            += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_F32BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_F32BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_F32LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_F32LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_F64BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_F64BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_F64LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_F64LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_MULAW_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_MULAW_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S16BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S16BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S16LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S16LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S24BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S24BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S24LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S24LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S32BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S32BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S32LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S32LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_S8_DEMUXER)            += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_S8_MUXER)              += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U16BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U16BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U16LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U16LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U24BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U24BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U24LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U24LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U32BE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U32BE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U32LE_DEMUXER)         += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U32LE_MUXER)           += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PCM_U8_DEMUXER)            += pcmdec.c pcm.c rawdec.c
SRCS_COMMON-$(CONFIG_PCM_U8_MUXER)              += pcmenc.c rawenc.c
SRCS_COMMON-$(CONFIG_PVA_DEMUXER)               += pva.c
SRCS_COMMON-$(CONFIG_QCP_DEMUXER)               += qcp.c
SRCS_COMMON-$(CONFIG_R3D_DEMUXER)               += r3d.c
SRCS_COMMON-$(CONFIG_RAWVIDEO_DEMUXER)          += rawvideodec.c rawdec.c
SRCS_COMMON-$(CONFIG_RAWVIDEO_MUXER)            += rawenc.c
SRCS_COMMON-$(CONFIG_RL2_DEMUXER)               += rl2.c
SRCS_COMMON-$(CONFIG_RM_DEMUXER)                += rmdec.c rm.c
SRCS_COMMON-$(CONFIG_RM_MUXER)                  += rmenc.c rm.c
SRCS_COMMON-$(CONFIG_ROQ_DEMUXER)               += idroqdec.c
SRCS_COMMON-$(CONFIG_ROQ_MUXER)                 += idroqenc.c rawenc.c
SRCS_COMMON-$(CONFIG_RSO_DEMUXER)               += rsodec.c rso.c pcm.c
SRCS_COMMON-$(CONFIG_RSO_MUXER)                 += rsoenc.c rso.c
SRCS_COMMON-$(CONFIG_RPL_DEMUXER)               += rpl.c
SRCS_COMMON-$(CONFIG_RTP_MUXER)                 += rtp.c         \
                                            rtpenc_aac.c     \
                                            rtpenc_amr.c     \
                                            rtpenc_h263.c    \
                                            rtpenc_mpv.c     \
                                            rtpenc.c      \
                                            rtpenc_h264.c \
                                            rtpenc_vp8.c  \
                                            rtpenc_xiph.c \
                                            avc.c
SRCS_COMMON-$(CONFIG_RTSP_DEMUXER)              += rtsp.c httpauth.c
SRCS_COMMON-$(CONFIG_RTSP_MUXER)                += rtsp.c rtspenc.c httpauth.c
SRCS_COMMON-$(CONFIG_SDP_DEMUXER)               += rtsp.c        \
                                            rdt.c         \
                                            rtp.c         \
                                            rtpdec.c      \
                                            rtpdec_amr.c  \
                                            rtpdec_asf.c  \
                                            rtpdec_h263.c \
                                            rtpdec_h264.c \
                                            rtpdec_latm.c \
                                            rtpdec_mpeg4.c \
                                            rtpdec_qdm2.c \
                                            rtpdec_svq3.c \
                                            rtpdec_vp8.c  \
                                            rtpdec_xiph.c
SRCS_COMMON-$(CONFIG_SEGAFILM_DEMUXER)          += segafilm.c
SRCS_COMMON-$(CONFIG_SHORTEN_DEMUXER)           += rawdec.c
SRCS_COMMON-$(CONFIG_SIFF_DEMUXER)              += siff.c
SRCS_COMMON-$(CONFIG_SMACKER_DEMUXER)           += smacker.c
SRCS_COMMON-$(CONFIG_SOL_DEMUXER)               += sol.c pcm.c
SRCS_COMMON-$(CONFIG_SOX_DEMUXER)               += soxdec.c pcm.c
SRCS_COMMON-$(CONFIG_SOX_MUXER)                 += soxenc.c
SRCS_COMMON-$(CONFIG_SPDIF_MUXER)               += spdif.c
SRCS_COMMON-$(CONFIG_SRT_DEMUXER)               += srtdec.c
SRCS_COMMON-$(CONFIG_SRT_MUXER)                 += rawenc.c
SRCS_COMMON-$(CONFIG_STR_DEMUXER)               += psxstr.c
SRCS_COMMON-$(CONFIG_SWF_DEMUXER)               += swfdec.c
SRCS_COMMON-$(CONFIG_SWF_MUXER)                 += swfenc.c
SRCS_COMMON-$(CONFIG_THP_DEMUXER)               += thp.c
SRCS_COMMON-$(CONFIG_TIERTEXSEQ_DEMUXER)        += tiertexseq.c
SRCS_COMMON-$(CONFIG_TMV_DEMUXER)               += tmv.c
SRCS_COMMON-$(CONFIG_TRUEHD_DEMUXER)            += rawdec.c
SRCS_COMMON-$(CONFIG_TRUEHD_MUXER)              += rawenc.c
SRCS_COMMON-$(CONFIG_TTA_DEMUXER)               += tta.c id3v1.c id3v2.c
SRCS_COMMON-$(CONFIG_TTY_DEMUXER)               += tty.c sauce.c
SRCS_COMMON-$(CONFIG_TXD_DEMUXER)               += txd.c
SRCS_COMMON-$(CONFIG_VC1_DEMUXER)               += rawdec.c
SRCS_COMMON-$(CONFIG_VC1T_DEMUXER)              += vc1test.c
SRCS_COMMON-$(CONFIG_VC1T_MUXER)                += vc1testenc.c
SRCS_COMMON-$(CONFIG_VMD_DEMUXER)               += sierravmd.c
SRCS_COMMON-$(CONFIG_VOC_DEMUXER)               += vocdec.c voc.c
SRCS_COMMON-$(CONFIG_VOC_MUXER)                 += vocenc.c voc.c
SRCS_COMMON-$(CONFIG_VQF_DEMUXER)               += vqf.c
SRCS_COMMON-$(CONFIG_W64_DEMUXER)               += wav.c riff.c pcm.c
SRCS_COMMON-$(CONFIG_WAV_DEMUXER)               += wav.c riff.c pcm.c
SRCS_COMMON-$(CONFIG_WAV_MUXER)                 += wav.c riff.c
SRCS_COMMON-$(CONFIG_WC3_DEMUXER)               += wc3movie.c
SRCS_COMMON-$(CONFIG_WEBM_MUXER)                += matroskaenc.c matroska.c \
                                            riff.c isom.c avc.c \
                                            flacenc_header.c
SRCS_COMMON-$(CONFIG_WSAUD_DEMUXER)             += westwood.c
SRCS_COMMON-$(CONFIG_WSVQA_DEMUXER)             += westwood.c
SRCS_COMMON-$(CONFIG_WV_DEMUXER)                += wv.c apetag.c id3v1.c
SRCS_COMMON-$(CONFIG_XA_DEMUXER)                += xa.c
SRCS_COMMON-$(CONFIG_YOP_DEMUXER)               += yop.c
SRCS_COMMON-$(CONFIG_YUV4MPEGPIPE_MUXER)        += yuv4mpeg.c
SRCS_COMMON-$(CONFIG_YUV4MPEGPIPE_DEMUXER)      += yuv4mpeg.c

# external libraries
SRCS_COMMON-$(CONFIG_LIBNUT_DEMUXER)            += libnut.c riff.c
SRCS_COMMON-$(CONFIG_LIBNUT_MUXER)              += libnut.c riff.c

# protocols I/O
SRCS_COMMON+= avio.c aviobuf.c

SRCS_COMMON-$(CONFIG_FILE_PROTOCOL)             += file.c
SRCS_COMMON-$(CONFIG_GOPHER_PROTOCOL)           += gopher.c
SRCS_COMMON-$(CONFIG_HTTP_PROTOCOL)             += http.c httpauth.c
SRCS_COMMON-$(CONFIG_MMSH_PROTOCOL)             += mmsh.c mms.c asf.c
SRCS_COMMON-$(CONFIG_MMST_PROTOCOL)             += mmst.c mms.c asf.c
SRCS_COMMON-$(CONFIG_MD5_PROTOCOL)              += md5proto.c
SRCS_COMMON-$(CONFIG_PIPE_PROTOCOL)             += file.c

# external or internal rtmp
RTMP-SRCS_COMMON-$(CONFIG_LIBRTMP)               = librtmp.c
RTMP-SRCS_COMMON-$(!CONFIG_LIBRTMP)              = rtmpproto.c rtmppkt.c
SRCS_COMMON-$(CONFIG_RTMP_PROTOCOL)             += $(RTMP-SRCS_COMMON-yes)

SRCS_COMMON-$(CONFIG_RTP_PROTOCOL)              += rtpproto.c
SRCS_COMMON-$(CONFIG_TCP_PROTOCOL)              += tcp.c
SRCS_COMMON-$(CONFIG_UDP_PROTOCOL)              += udp.c
SRCS_COMMON-$(CONFIG_CONCAT_PROTOCOL)           += concat.c

# libavdevice dependencies
SRCS_COMMON-$(CONFIG_JACK_INDEV)                += timefilter.c

SRCS_COMMON+= $(SRCS_COMMON-yes)
