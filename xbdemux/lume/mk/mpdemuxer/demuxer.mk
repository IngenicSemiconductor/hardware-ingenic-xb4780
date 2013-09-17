.SUFFIXES:
include $(LUME_PATH)/codec_config.mak
###### variable declarations #######
SRCS_AUDIO_INPUT-$(ALSA1X)           += stream/ai_alsa1x.c
SRCS_AUDIO_INPUT-$(ALSA9)            += stream/ai_alsa.c
SRCS_AUDIO_INPUT-$(OSS)              += stream/ai_oss.c
SRCS_COMMON-$(AUDIO_INPUT)           += $(SRCS_AUDIO_INPUT-yes)
SRCS_COMMON-$(BITMAP_FONT)           += libvo/font_load.c
SRCS_COMMON-$(CDDA)                  += stream/stream_cdda.c \
                                        stream/cdinfo.c
SRCS_COMMON-$(CDDB)                  += stream/stream_cddb.c
SRCS_COMMON-$(DVBIN)                 += stream/dvb_tune.c \
                                        stream/stream_dvb.c
SRCS_COMMON-$(DVDNAV)                += stream/stream_dvdnav.c

SRCS_COMMON-$(DVDREAD)               += stream/stream_dvd.c \
                                        stream/stream_dvd_common.c

SRCS_COMMON-$(FAAD_INTERNAL)         += libfaad2/bits.c \
                                        libfaad2/cfft.c \
                                        libfaad2/common.c \
                                        libfaad2/decoder.c \
                                        libfaad2/drc.c \
                                        libfaad2/drm_dec.c \
                                        libfaad2/error.c \
                                        libfaad2/filtbank.c \
                                        libfaad2/hcr.c \
                                        libfaad2/huffman.c \
                                        libfaad2/ic_predict.c \
                                        libfaad2/is.c \
                                        libfaad2/lt_predict.c \
                                        libfaad2/mdct.c \
                                        libfaad2/mp4.c \
                                        libfaad2/ms.c \
                                        libfaad2/output.c \
                                        libfaad2/pns.c \
                                        libfaad2/ps_dec.c \
                                        libfaad2/ps_syntax.c  \
                                        libfaad2/pulse.c \
                                        libfaad2/rvlc.c \
                                        libfaad2/sbr_dct.c \
                                        libfaad2/sbr_dec.c \
                                        libfaad2/sbr_e_nf.c \
                                        libfaad2/sbr_fbt.c \
                                        libfaad2/sbr_hfadj.c \
                                        libfaad2/sbr_hfgen.c \
                                        libfaad2/sbr_huff.c \
                                        libfaad2/sbr_qmf.c \
                                        libfaad2/sbr_syntax.c \
                                        libfaad2/sbr_tf_grid.c \
                                        libfaad2/specrec.c \
                                        libfaad2/ssr.c \
                                        libfaad2/ssr_fb.c \
                                        libfaad2/ssr_ipqf.c \
                                        libfaad2/syntax.c \
                                        libfaad2/tns.c \

SRCS_COMMON-$(FTP)                   += stream/stream_ftp.c
SRCS_COMMON-$(GIF)                   += libmpdemux/demux_gif.c

SRCS_COMMON-$(LADSPA)                += libaf/af_ladspa.c \
                                        liba52/resample.c \
                                        liba52/bit_allocate.c \
                                        liba52/bitstream.c \
                                        liba52/downmix.c \
                                        liba52/imdct.c \
                                        liba52/parse.c \

SRCS_COMMON-$(LIBA52_INTERNAL)       += $(SRCS_LIBA52_INTERNAL)


SRCS_COMMON-$(LIBAVFORMAT)           += libmpdemux/demux_lavf.c 

#					libmpdemux/opt.c
#                                        stream/stream_ffmpeg.c \

SRCS_COMMON-$(LIBDV)                 += libmpdemux/demux_rawdv.c

SRCS_LIBMPEG2-$(ARCH_ALPHA)          += libmpeg2/idct_alpha.c \
                                        libmpeg2/motion_comp_alpha.c
SRCS_LIBMPEG2-$(ARCH_ARM)            += libmpeg2/motion_comp_arm.c \
                                        libmpeg2/motion_comp_arm_s.S
SRCS_LIBMPEG2-$(HAVE_ALTIVEC)        += libmpeg2/idct_altivec.c \
                                        libmpeg2/motion_comp_altivec.c
SRCS_LIBMPEG2-$(HAVE_MMX)            += libmpeg2/idct_mmx.c \
                                        libmpeg2/motion_comp_mmx.c
SRCS_LIBMPEG2-$(HAVE_VIS)            += libmpeg2/motion_comp_vis.c


SRCS_COMMON-$(LIBNEMESI)             += libmpdemux/demux_nemesi.c \
                                        stream/stream_nemesi.c
SRCS_COMMON-$(LIBNUT)                += libmpdemux/demux_nut.c

SRCS_COMMON-$(LIBSMBCLIENT)          += stream/stream_smb.c

SRCS_COMMON-$(LIVE555)               += libmpdemux/demux_rtp.cpp \
                                        libmpdemux/demux_rtp_codec.cpp \
                                        stream/stream_live555.c
SRCS_COMMON-$(MNG)                   += libmpdemux/demux_mng.c


SRCS_COMMON-$(MUSEPACK)              += libmpdemux/demux_mpc.c
SRCS_COMMON-$(NATIVE_RTSP)           += stream/stream_rtsp.c \
                                        stream/freesdp/common.c \
                                        stream/freesdp/errorlist.c \
                                        stream/freesdp/parser.c \
                                        stream/librtsp/rtsp.c \
                                        stream/librtsp/rtsp_rtp.c \
                                        stream/librtsp/rtsp_session.c \

SRCS_COMMON-$(NETWORK)               += stream/stream_netstream.c \
                                        stream/asf_mmst_streaming.c \
                                        stream/asf_streaming.c \
                                        stream/cookies.c \
                                        stream/http.c \
                                        stream/network.c \
                                        stream/pnm.c \
                                        stream/rtp.c \
                                        stream/udp.c \
                                        stream/tcp.c \
                                        stream/stream_rtp.c \
                                        stream/stream_udp.c \
                                        stream/librtsp/rtsp.c \
                                        stream/realrtsp/asmrp.c \
                                        stream/realrtsp/real.c \
                                        stream/realrtsp/rmff.c \
                                        stream/realrtsp/sdpplin.c \
                                        stream/realrtsp/xbuffer.c \


SRCS_COMMON-$(PVR)                   += stream/stream_pvr.c
SRCS_COMMON-$(RADIO)                 += stream/stream_radio.c
SRCS_COMMON-$(RADIO_CAPTURE)         += stream/audio_in.c
SRCS_COMMON-$(STREAM_CACHE)          += stream/cache2.c

SRCS_COMMON-$(TREMOR_INTERNAL)       += tremor/bitwise.c \
                                        tremor/block.c \
                                        tremor/codebook.c \
                                        tremor/floor0.c \
                                        tremor/floor1.c \
                                        tremor/framing.c \
                                        tremor/info.c \
                                        tremor/mapping0.c \
                                        tremor/mdct.c \
                                        tremor/registry.c \
                                        tremor/res012.c \
                                        tremor/sharedbook.c \
                                        tremor/synthesis.c \
                                        tremor/window.c \

SRCS_COMMON-$(TV)                    += stream/stream_tv.c stream/tv.c \
                                        stream/frequencies.c stream/tvi_dummy.c
SRCS_COMMON-$(TV_BSDBT848)           += stream/tvi_bsdbt848.c
SRCS_COMMON-$(TV_DSHOW)              += stream/tvi_dshow.c 

SRCS_COMMON-$(TV_V4L1)               += stream/tvi_v4l.c  stream/audio_in.c
SRCS_COMMON-$(TV_V4L2)               += stream/tvi_v4l2.c stream/audio_in.c
SRCS_COMMON-$(UNRAR_EXEC)            += unrar_exec.c
SRCS_COMMON-$(VCD)                   += stream/stream_vcd.c
SRCS_COMMON-$(VORBIS)                += libmpdemux/demux_ogg.c
SRCS_COMMON-$(VSTREAM)               += stream/stream_vstream.c
SRCS_QTX_EMULATION                   += loader/wrapper.S
SRCS_COMMON-$(QTX_EMULATION)         += $(SRCS_QTX_EMULATION)
SRCS_WIN32_EMULATION                 += loader/elfdll.c \
                                        loader/ext.c \
                                        loader/ldt_keeper.c \
                                        loader/module.c \
                                        loader/pe_image.c \
                                        loader/pe_resource.c \
                                        loader/registry.c \
                                        loader/resource.c \
                                        loader/win32.c \

SRCS_COMMON-$(WIN32_EMULATION)       += $(SRCS_WIN32_EMULATION)


SRCS_COMMON-$(XMMS_PLUGINS)          += libmpdemux/demux_xmms.c

SRCS_COMMON = libmpdemux/demuxer.c \
              libmpdemux/demux_aac.c \
	      libmpdemux/demux_ts.c\
              libmpdemux/demux_asf.c \
              libmpdemux/demux_audio.c \
              libmpdemux/demux_avi.c \
              libmpdemux/demux_demuxers.c \
              libmpdemux/demux_film.c \
              libmpdemux/demux_fli.c \
              libmpdemux/demux_lmlm4.c \
              libmpdemux/demux_mf.c \
              libmpdemux/demux_mkv.c \
              libmpdemux/demux_mov.c \
              libmpdemux/demux_mpg.c \
              libmpdemux/demux_nsv.c \
              libmpdemux/demux_pva.c \
              libmpdemux/demux_rawaudio.c \
              libmpdemux/demux_rawvideo.c \
              libmpdemux/demux_realaud.c \
              libmpdemux/demux_real.c \
              libmpdemux/demux_roq.c \
              libmpdemux/demux_smjpeg.c \
              libmpdemux/demux_viv.c \
              libmpdemux/demux_vqf.c \
              libmpdemux/demux_y4m.c \
			  libmpdemux/demux_pmp.c \
              libmpdemux/ebml.c \
              libmpdemux/extension.c \
              libmpdemux/mf.c \
              libmpdemux/mp3_hdr.c \
              libmpdemux/mpeg_hdr.c \
              libmpdemux/mpeg_packetizer.c \
              libmpdemux/parse_es.c \
              libmpdemux/parse_mp4.c \
              libmpdemux/video.c \
              libmpdemux/yuv4mpeg.c \
              libmpdemux/yuv4mpeg_ratio.c \
              libmpdemux/asfheader.c \
              libmpdemux/aviheader.c \
              libmpdemux/aviprint.c \
			  libmpdemux/aac_hdr.c \
              libmpdemux/mp_taglists.c \
              stream/open.c \
              stream/stream.c \
             m_struct.c \
              stream/stream_stagefright.cpp \
             m_option.c \
             sub_cc.c \
              $(SRCS_COMMON-yes)

ifeq ($(BOARD_HAS_ISDBT),true)
SRCS_COMMON += \
	      libmpdemux/demux_isdbt.c \
	      libmpdemux/demux_isdbt_if.cpp 
endif
