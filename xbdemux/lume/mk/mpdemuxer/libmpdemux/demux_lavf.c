/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of Mplayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <unistd.h>
#include <limits.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "av_opts.h"

#include "stream/stream.h"
#include "aviprint.h"
#include "demuxer.h"
#include "mp3_hdr.h"
#include "stheader.h"
#include "m_option.h"
#include "libvo/sub.h"

#include "avformat.h"
#include "avio.h"
#include "libavutil/avutil.h"
#include "libavutil/avstring.h"
#include "libavcodec/opt.h"

#include "mp_taglists.h"
#include <utils/Log.h>

//#define mp_msg(w,x,y,z...) LOGE(y,##z)

#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define SMALL_MAX_PROBE_SIZE (32 * 1024)
#define PROBE_BUF_SIZE (2*1024*1024)

static unsigned int opt_probesize = 0;
static unsigned int opt_analyzeduration = 0;
static char *opt_format;
static char *opt_cryptokey;
static char *opt_avopt = NULL;
//static double start_time = 0.0;
const m_option_t lavfdopts_conf[] = {
	{"probesize", &(opt_probesize), CONF_TYPE_INT, CONF_RANGE, 32, INT_MAX, NULL},
	{"format",    &(opt_format),    CONF_TYPE_STRING,       0,  0,       0, NULL},
	{"analyzeduration",    &(opt_analyzeduration),    CONF_TYPE_INT,       CONF_RANGE,  0,       INT_MAX, NULL},
	{"cryptokey", &(opt_cryptokey), CONF_TYPE_STRING,       0,  0,       0, NULL},
        {"o",                  &opt_avopt,                CONF_TYPE_STRING,    0,           0,             0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#define BIO_BUFFER_SIZE 32768

typedef struct lavf_priv {
    AVInputFormat *avif;
    AVFormatContext *avfc;
    ByteIOContext *pb;
    uint8_t buffer[BIO_BUFFER_SIZE];
    int audio_streams;
    int video_streams;
    int sub_streams;
    int64_t last_pts;
    int astreams[MAX_A_STREAMS];
    int vstreams[MAX_V_STREAMS];
    int sstreams[MAX_S_STREAMS];
    int cur_program;
}lavf_priv_t;

static int mp_read(void *opaque, uint8_t *buf, int size) {
    demuxer_t *demuxer = opaque;
    stream_t *stream = demuxer->stream;
    int ret;

    ret=stream_read(stream, buf, size);

    mp_msg(MSGT_HEADER,MSGL_DBG2,"%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
           ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence) {
    demuxer_t *demuxer = opaque;
    stream_t *stream = demuxer->stream;
    int64_t current_pos;
    mp_msg(MSGT_HEADER,MSGL_DBG2,"mp_seek(%p, %"PRId64", %d)\n", stream, pos, whence);
    if(whence == SEEK_CUR)
        pos +=stream_tell(stream);
    else if(whence == SEEK_END && stream->end_pos > 0)
        pos += stream->end_pos;
    else if(whence == SEEK_SET)
        pos += stream->start_pos;
    else if(whence == AVSEEK_SIZE && stream->end_pos > 0)
        return stream->end_pos - stream->start_pos;
    else
        return -1;

    if(pos<0)
        return -1;
    current_pos = stream_tell(stream);
    if(stream_seek(stream, pos)==0) {
        stream_reset(stream);
        stream_seek(stream, current_pos);
        return -1;
    }

    return pos - stream->start_pos;
}

static int64_t mp_read_seek(void *opaque, int stream_idx, int64_t ts, int flags) {
    demuxer_t *demuxer = opaque;
    stream_t *stream = demuxer->stream;
    lavf_priv_t *priv = demuxer->priv;
    AVStream *st = priv->avfc->streams[stream_idx];
    int ret;
    double pts;

    pts = (double)ts * st->time_base.num / st->time_base.den;
    ret = stream_control(stream, STREAM_CTRL_SEEK_TO_TIME, &pts);
    if (ret < 0){
        ret = AVERROR(ENOSYS);
    }
    return ret;
}

static void list_formats(void) {
    AVInputFormat *fmt;
    mp_msg(MSGT_DEMUX, MSGL_INFO, "Available lavf input formats:\n");
    for (fmt = first_iformat; fmt; fmt = fmt->next)
        mp_msg(MSGT_DEMUX, MSGL_INFO, "%15s : %s\n", fmt->name, fmt->long_name);
}

static int lavf_check_file(demuxer_t *demuxer){
    AVProbeData avpd;
    lavf_priv_t *priv;
    int probe_data_size = 0;
    int read_size = INITIAL_PROBE_SIZE;
    int score;
    if(!demuxer->priv)
        demuxer->priv=calloc(sizeof(lavf_priv_t),1);
    priv= demuxer->priv;

    av_register_all();

    if (opt_format) {
        if (strcmp(opt_format, "help") == 0) {
           list_formats();
           return 0;
        }
        priv->avif= av_find_input_format(opt_format);
        if (!priv->avif) {
            mp_msg(MSGT_DEMUX,MSGL_FATAL,"Unknown lavf format %s\n", opt_format);
            return 0;
        }
        mp_msg(MSGT_DEMUX,MSGL_INFO,"Forced lavf %s demuxer\n", priv->avif->long_name);
        return DEMUXER_TYPE_LAVF;
    }

    avpd.buf = av_mallocz(FFMAX(BIO_BUFFER_SIZE, PROBE_BUF_SIZE) +
                          FF_INPUT_BUFFER_PADDING_SIZE);
    do {
        read_size = stream_read(demuxer->stream, avpd.buf + probe_data_size, read_size);
        if(read_size < 0) {
            av_free(avpd.buf);
            return 0;
        }
        probe_data_size += read_size;
        avpd.filename= demuxer->stream->url;
        if (!avpd.filename) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "Stream url is not set!\n");
            avpd.filename = "";
        }
        if (!strncmp(avpd.filename, "ffmpeg://", 9))
            avpd.filename += 9;
        avpd.buf_size= probe_data_size;

        score = 0;
        priv->avif= av_probe_input_format2(&avpd, probe_data_size > 0, &score);
	if(score == AVPROBE_SCORE_MAX/4-1){
	  av_free(avpd.buf);
	  return 0;
	}
        read_size = FFMIN(2*read_size, PROBE_BUF_SIZE - probe_data_size);
    } while ((demuxer->desc->type != DEMUXER_TYPE_LAVF_PREFERRED ||
              probe_data_size < SMALL_MAX_PROBE_SIZE) &&
             score <= AVPROBE_SCORE_MAX / 4 &&
             read_size > 0 && probe_data_size < PROBE_BUF_SIZE);
    av_free(avpd.buf);

    if(!priv->avif){
        mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: no clue about this gibberish!\n");
        return 0;
    }else
      mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: %s\n", priv->avif->long_name);
#if 0
    if (strcmp("mpeg", priv->avif->name) == 0 && strcmp("MPEG-PS format", priv->avif->long_name) == 0){
      return DEMUXER_TYPE_MPEG_PS;
    }  
    LOGE("LAVF_check: %s,  %s", priv->avif->name, priv->avif->long_name);
#endif

    return DEMUXER_TYPE_LAVF;
}

static const char * const preferred_list[] = {
    "dxa",
    "flv",
    "gxf",
    "nut",
    "nuv",
    "matroska,webm",
    "mov,mp4,m4a,3gp,3g2,mj2",
    "mpc",
    "mpc8",
    "mxf",
    "ogg",
    "swf",
    "vqf",
    "w64",
    "wv",
    "ape",
    NULL
};

static int lavf_check_preferred_file(demuxer_t *demuxer){
    if (lavf_check_file(demuxer)) {
        const char * const *p = preferred_list;
        lavf_priv_t *priv = demuxer->priv;
	demuxer->matroflag = 0;
        while (*p) {
	  if (strcmp(*p, priv->avif->name) == 0){
	    if (strcmp("matroska,webm", priv->avif->name) == 0)
	      demuxer->matroflag = 1;
	    return DEMUXER_TYPE_LAVF_PREFERRED;
	  }
	  p++;
        }
    }
    return 0;
}

static uint8_t char2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void parse_cryptokey(AVFormatContext *avfc, const char *str) {
    int len = strlen(str) / 2;
    uint8_t *key = av_mallocz(len);
    int i;
    avfc->keylen = len;
    avfc->key = key;
    for (i = 0; i < len; i++, str += 2)
        *key++ = (char2int(str[0]) << 4) | char2int(str[1]);
}

static void handle_stream(demuxer_t *demuxer, AVFormatContext *avfc, int i) {
    lavf_priv_t *priv= demuxer->priv;
    AVStream *st= avfc->streams[i];
    AVCodecContext *codec= st->codec;
    char *stream_type = NULL;
    int stream_id;
    AVMetadataTag *lang = av_metadata_get(st->metadata, "language", NULL, 0);
    AVMetadataTag *title= av_metadata_get(st->metadata, "title",    NULL, 0);
    int g, override_tag = av_codec_get_tag(mp_codecid_override_taglists,
                                           codec->codec_id);
    // For some formats (like PCM) always trust CODEC_ID_* more than codec_tag
    if (override_tag)
        codec->codec_tag = override_tag;

    switch(codec->codec_type){
        case CODEC_TYPE_AUDIO:{
            WAVEFORMATEX *wf;
            sh_audio_t* sh_audio;
            sh_audio = new_sh_audio_aid(demuxer, i, priv->audio_streams, lang ? lang->value : NULL);
            if(!sh_audio)
                break;
            stream_type = "audio";
            priv->astreams[priv->audio_streams] = i;
            wf= calloc(sizeof(WAVEFORMATEX) + codec->extradata_size, 1);
            // mp4a tag is used for all mp4 files no matter what they actually contain
            if(codec->codec_tag == MKTAG('m', 'p', '4', 'a'))
                codec->codec_tag= 0;
            if(!codec->codec_tag)
                codec->codec_tag= av_codec_get_tag(mp_wav_taglists, codec->codec_id);
            wf->wFormatTag= codec->codec_tag;
            wf->nChannels= codec->channels;
            wf->nSamplesPerSec= codec->sample_rate;
            wf->nAvgBytesPerSec= codec->bit_rate/8;
            wf->nBlockAlign= codec->block_align ? codec->block_align : 1;
            wf->wBitsPerSample= codec->bits_per_coded_sample;
            wf->cbSize= codec->extradata_size;
            if(codec->extradata_size)
                memcpy(wf + 1, codec->extradata, codec->extradata_size);
            sh_audio->wf= wf;
            sh_audio->audio.dwSampleSize= codec->block_align;
            if(codec->frame_size && codec->sample_rate){
                sh_audio->audio.dwScale=codec->frame_size;
                sh_audio->audio.dwRate= codec->sample_rate;
            }else{
                sh_audio->audio.dwScale= codec->block_align ? codec->block_align*8 : 8;
                sh_audio->audio.dwRate = codec->bit_rate;
            }
            g= av_gcd(sh_audio->audio.dwScale, sh_audio->audio.dwRate);

            sh_audio->audio.dwScale /= g;
            sh_audio->audio.dwRate  /= g;
//          printf("sca:%d rat:%d fs:%d sr:%d ba:%d\n", sh_audio->audio.dwScale, sh_audio->audio.dwRate, codec->frame_size, codec->sample_rate, codec->block_align);
            sh_audio->ds= demuxer->audio;
            sh_audio->format= codec->codec_tag;
            sh_audio->channels= codec->channels;
            sh_audio->samplerate= codec->sample_rate;
            /*
	      in opencore : 
              try_decode_frame() delete in libavformat/utils.c ,so some parame maybe not be get 
	      so if samplerate no get in header, then give a fix value 44100
	    */
	    if(!sh_audio->channels)
		sh_audio->channels = 2;
	    if(!sh_audio->samplerate)
		sh_audio->samplerate = 44100;

            sh_audio->i_bps= codec->bit_rate/8;
            switch (codec->codec_id) {
                case CODEC_ID_PCM_S8:
                case CODEC_ID_PCM_U8:
                    sh_audio->samplesize = 1;
                    break;
                case CODEC_ID_PCM_S16LE:
                case CODEC_ID_PCM_S16BE:
                case CODEC_ID_PCM_U16LE:
                case CODEC_ID_PCM_U16BE:
                    sh_audio->samplesize = 2;
                    break;
                case CODEC_ID_PCM_ALAW:
                    sh_audio->format = 0x6;
                    break;
                case CODEC_ID_PCM_MULAW:
                    sh_audio->format = 0x7;
                    break;
            }
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_NAME=%s\n", priv->audio_streams, title->value);
            if (st->disposition & AV_DISPOSITION_DEFAULT)
              sh_audio->default_track = 1;
            if(mp_msg_test(MSGT_HEADER,MSGL_V) ) print_wave_header(sh_audio->wf, MSGL_V);
            // select the first audio stream
            if (!demuxer->audio->sh) {
                demuxer->audio->id = i;
                demuxer->audio->sh= demuxer->a_streams[i];
            } else
	        st->discard= AVDISCARD_ALL;
            stream_id = priv->audio_streams++;
            break;
        }
        case CODEC_TYPE_VIDEO:{
            sh_video_t* sh_video;
            BITMAPINFOHEADER *bih;
            sh_video=new_sh_video_vid(demuxer, i, priv->video_streams);
            if(!sh_video) break;
            stream_type = "video";
            priv->vstreams[priv->video_streams] = i;
            bih=calloc(sizeof(BITMAPINFOHEADER) + codec->extradata_size,1);

            if(codec->codec_id == CODEC_ID_RAWVIDEO) {
                switch (codec->pix_fmt) {
                    case PIX_FMT_RGB24:
                        codec->codec_tag= MKTAG(24, 'B', 'G', 'R');
                }
            }
            if(!codec->codec_tag)
                codec->codec_tag= av_codec_get_tag(mp_bmp_taglists, codec->codec_id);
            bih->biSize= sizeof(BITMAPINFOHEADER) + codec->extradata_size;
	    if(!codec->width || !codec->height){
	      codec->width = 16*4;	    
	      codec->height = 9*4;
	    }
            bih->biWidth= codec->width;
            bih->biHeight= codec->height;
            bih->biBitCount= codec->bits_per_coded_sample;
            bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount/8;
            bih->biCompression= codec->codec_tag;
            sh_video->bih= bih;
            sh_video->disp_w= codec->width;
            sh_video->disp_h= codec->height;
            if (st->time_base.den) { /* if container has time_base, use that */
                sh_video->video.dwRate= st->time_base.den;
                sh_video->video.dwScale= st->time_base.num;
            } else {
                sh_video->video.dwRate= codec->time_base.den;
                sh_video->video.dwScale= codec->time_base.num;
            }
            sh_video->fps=av_q2d(st->r_frame_rate);
            sh_video->frametime=1/av_q2d(st->r_frame_rate);
            sh_video->format=bih->biCompression;
            if(st->sample_aspect_ratio.num)
                sh_video->aspect = codec->width  * st->sample_aspect_ratio.num
                         / (float)(codec->height * st->sample_aspect_ratio.den);
            else
                sh_video->aspect=codec->width  * codec->sample_aspect_ratio.num
                       / (float)(codec->height * codec->sample_aspect_ratio.den);
	    sh_video->rotation_degrees = st->rotation_degrees;
            sh_video->i_bps=codec->bit_rate/8;
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VID_%d_NAME=%s\n", priv->video_streams, title->value);
            mp_msg(MSGT_DEMUX,MSGL_DBG2,"aspect= %d*%d/(%d*%d)\n",
                codec->width, codec->sample_aspect_ratio.num,
                codec->height, codec->sample_aspect_ratio.den);

            sh_video->ds= demuxer->video;
            if(codec->extradata_size)
                memcpy(sh_video->bih + 1, codec->extradata, codec->extradata_size);
            if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_video_header(sh_video->bih, MSGL_V);
            /*
                short biPlanes;
                int  biXPelsPerMeter;
                int  biYPelsPerMeter;
                int biClrUsed;
                int biClrImportant;
            */
            if(demuxer->video->id != i && demuxer->video->id != -1)
                st->discard= AVDISCARD_ALL;
            else{
                demuxer->video->id = i;
                demuxer->video->sh= demuxer->v_streams[i];
            }
            stream_id = priv->video_streams++;
            break;
        }
        case CODEC_TYPE_SUBTITLE:{
            sh_sub_t* sh_sub;
            char type;
            /* only support text subtitles for now */
            if(codec->codec_id == CODEC_ID_TEXT)
                type = 't';
            else if(codec->codec_id == CODEC_ID_MOV_TEXT)
                type = 'm';
            else if(codec->codec_id == CODEC_ID_SSA)
                type = 'a';
            else if(codec->codec_id == CODEC_ID_DVD_SUBTITLE)
                type = 'v';
            else if(codec->codec_id == CODEC_ID_XSUB)
                type = 'x';
            else if(codec->codec_id == CODEC_ID_DVB_SUBTITLE)
                type = 'b';
            else if(codec->codec_id == CODEC_ID_DVB_TELETEXT)
                type = 'd';
            else if(codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE)
                type = 'p';
            else
                break;
            sh_sub = new_sh_sub_sid(demuxer, i, priv->sub_streams, lang ? lang->value : NULL);
            if(!sh_sub) break;
            stream_type = "subtitle";
            priv->sstreams[priv->sub_streams] = i;
            sh_sub->type = type;
            if (codec->extradata_size) {
                sh_sub->extradata = malloc(codec->extradata_size);
                memcpy(sh_sub->extradata, codec->extradata, codec->extradata_size);
                sh_sub->extradata_len = codec->extradata_size;
            }
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_NAME=%s\n", priv->sub_streams, title->value);
            if (st->disposition & AV_DISPOSITION_DEFAULT)
              sh_sub->default_track = 1;
            stream_id = priv->sub_streams++;
            break;
        }
        case CODEC_TYPE_ATTACHMENT:{
            if (st->codec->codec_id == CODEC_ID_TTF)
                demuxer_add_attachment(demuxer, st->filename,
                                       "application/x-truetype-font",
                                       codec->extradata, codec->extradata_size);
            break;
        }
        default:
            st->discard= AVDISCARD_ALL;
    }
    if (stream_type) {
        AVCodec *avc = avcodec_find_decoder(codec->codec_id);
        const char *codec_name = avc ? avc->name : "unknown";
	if (strcmp("tta", codec_name) == 0)
	  demuxer->ttaflag = 1;
        if (!avc && *stream_type == 's' && demuxer->s_streams[stream_id])
            codec_name = sh_sub_type2str(((sh_sub_t *)demuxer->s_streams[stream_id])->type);
        mp_msg(MSGT_DEMUX, MSGL_INFO, "[lavf] stream %d: %s (%s), -%cid %d", i, stream_type, codec_name, *stream_type, stream_id);
        if (lang && lang->value && *stream_type != 'v')
            mp_msg(MSGT_DEMUX, MSGL_INFO, ", -%clang %s", *stream_type, lang->value);
        if (title && title->value)
            mp_msg(MSGT_DEMUX, MSGL_INFO, ", %s", title->value);
        mp_msg(MSGT_DEMUX, MSGL_INFO, "\n");
    }
}
void get_aac_duration(demuxer_t *demuxer, AVFormatContext *avfc){
    uint8_t hdr[8];
    int len,nf=0,srate,num,c,framesize,i;
    loff_t init = stream_tell(demuxer->stream);
    lavf_priv_t *priv= demuxer->priv;
    AVCodecContext *codec;

    stream_seek(demuxer->stream, demuxer->movi_start);
    while(1){
      c = 0;
      while(c != 0xFF){
	c = stream_read_char(demuxer->stream);
	if(c < 0)
	  break;
      }
      hdr[0] = 0xFF;
      if(stream_read(demuxer->stream, &(hdr[1]), 7) != 7)
	break;

      len = aac_parse_frame(hdr, &srate, &num);
      if(len > 0){
	nf++;
	stream_skip(demuxer->stream, len - 8);
      }else{
	stream_skip(demuxer->stream, -6);
      }
    }
    stream_seek(demuxer->stream, init);
    for(i=0; i<avfc->nb_streams; i++){
      codec= ((AVStream *)avfc->streams[i])->codec;
      if((framesize = codec->frame_size)>0)
	break;
    }
    //framesize = framesize>0?framesize:1024;
    framesize = 1024;
    if(nf && srate){
      priv->avfc->duration = (double)((nf*framesize/srate)*AV_TIME_BASE);
    }
}

#if 0
/*
 * Geodata is stored according to ISO-6709 standard.
 */
void MPEG4Writer::writeGeoDataBox() {
    beginBox("\xA9xyz");
    /*
     * For historical reasons, any user data start
     * with "\0xA9", must be followed by its assoicated
     * language code.
     * 0x0012: text string length
     * 0x15c7: lang (locale) code: en
     */
    writeInt32(0x001215c7);
    writeLatitude(mLatitudex10000);
    writeLongitude(mLongitudex10000);
    writeInt8(0x2F);
    endBox();
}
#endif

#if 0
typedef struct{
    int32_t geodata_start_tag;
    int32_t lang_code;
  //unsigned char latitude[8];
  //unsigned char longitudex[9 + 1];//for '\0'
    unsigned char latitude_longitudex[8 + 9 + 1];//for '\0'
    //int8_t char;//'/', 0x2F  
}GeoDataBox;

#define printf LOGE

#ifndef memstr
char *memstr(char *haystack, char *needle, int size)
{
	char *p;
	char needlesize = strlen(needle);

	for (p = haystack; p <= (haystack-needlesize+size); p++)
	{
		if (memcmp(p, needle, needlesize) == 0)
			return p; /* found */
	}
	return NULL;
}
#endif

static int demux_lavf_find_geodata(demuxer_t *demuxer){
    int fould_geodata = 0;
    AVMetadataTag metadata;

    int64_t old_filepos= stream_tell(demuxer->stream);

    stream_seek(demuxer->stream, 0);

    int udta_len = 320;
    unsigned char udta_buf[udta_len];

    int ret = stream_read(demuxer->stream, udta_buf, udta_len);

#if 0    
    int j = udta_len;
    while(j){
	if(j%16 == 0){
	  printf("\n");
	}

        printf("%02x, ", udta_buf[udta_len - j]);
	--j;
    }
    printf("\n\n");
#endif

    unsigned char *pch;
    pch = memstr(udta_buf, "udta", udta_len);
    //printf("%x\n", 'udta');
    if(pch){
        //printf("fould udta box, %02x, %02x, %02x, %02x\n", pch[0], pch[1], pch[2], pch[3]);
	//printf("%x\n", 'zyx\xA9');

        pch = memstr(pch + 4, "\xA9xyz", udta_len - (pch + 4 - udta_buf));
	
	if(pch){
	  //printf("xyz fould %02x, %02x, %02x, %02x\n", pch[0], pch[1], pch[2], pch[3]);
	    GeoDataBox geoDataBox;
	    memcpy(&geoDataBox, pch, sizeof(GeoDataBox));
	    geoDataBox.latitude_longitudex[8+ 9] = '\0';

	    //printf("geodata_start_tag = %x\n", geoDataBox.geodata_start_tag);
	    //printf("lang_code = %x\n", geoDataBox.lang_code);
	    printf("lat_long = %s\n", geoDataBox.latitude_longitudex);

	    metadata.key = "location";
	    metadata.value = &geoDataBox.latitude_longitudex;
	    printf("demux_info_add: k->key = %s, t->value = %s, demuxer = 0x%x\n", metadata.key, metadata.value, demuxer);
	    demux_info_add(demuxer, metadata.key, metadata.value);
	    fould_geodata = 1;
	}else{
	    printf("Geodata xyz NOT fould\n");
	}

    }else{
        printf("udta box not fould\n");
    }


    stream_seek(demuxer->stream, old_filepos);

    return fould_geodata;
}
#endif

static demuxer_t* demux_open_lavf(demuxer_t *demuxer){
    AVFormatContext *avfc;
    AVFormatParameters ap;
    const AVOption *opt;
    AVMetadataTag *t = NULL;
    lavf_priv_t *priv= demuxer->priv;
    int i;
    //start_time = 0.0;
    char mp_filename[256]="mp:";
    memset(&ap, 0, sizeof(AVFormatParameters));

    //demux_lavf_find_geodata(demuxer);

    stream_seek(demuxer->stream, 0);

    int filepos=stream_tell(demuxer->stream);
    
    avfc = avformat_alloc_context();

    if (opt_cryptokey)
        parse_cryptokey(avfc, opt_cryptokey);
    if (user_correct_pts != 0)
        avfc->flags |= AVFMT_FLAG_GENPTS;
    /* if (index_mode == 0) */
    /*     avfc->flags |= AVFMT_FLAG_IGNIDX; */

    ap.prealloced_context = 1;
#if 0
    if(opt_probesize) {
        opt = av_set_int(avfc, "probesize", opt_probesize);
        if(!opt) mp_msg(MSGT_HEADER,MSGL_ERR, "demux_lavf, couldn't set option probesize to %u\n", opt_probesize);
    }
    if(opt_analyzeduration) {
        opt = av_set_int(avfc, "analyzeduration", opt_analyzeduration * AV_TIME_BASE);
        if(!opt) mp_msg(MSGT_HEADER,MSGL_ERR, "demux_lavf, couldn't set option analyzeduration to %u\n", opt_analyzeduration);
    }

    if(opt_avopt){
        if(parse_avopts(avfc, opt_avopt) < 0){
            mp_msg(MSGT_HEADER,MSGL_ERR, "Your options /%s/ look like gibberish to me pal\n", opt_avopt);
            return NULL;
        }
    }
#endif
    if(demuxer->stream->url) {
        if (!strncmp(demuxer->stream->url, "ffmpeg://rtsp:", 14))
            av_strlcpy(mp_filename, demuxer->stream->url + 9, sizeof(mp_filename));
        else
            av_strlcat(mp_filename, demuxer->stream->url, sizeof(mp_filename));
    } else
        av_strlcat(mp_filename, "foobar.dummy", sizeof(mp_filename));

    priv->pb = av_alloc_put_byte(priv->buffer, BIO_BUFFER_SIZE, 0,
                                 demuxer, mp_read, NULL, mp_seek);
    priv->pb->read_seek = mp_read_seek;
    priv->pb->is_streamed = !demuxer->stream->end_pos || (demuxer->stream->flags & MP_STREAM_SEEK) != MP_STREAM_SEEK;
    filepos=stream_tell(demuxer->stream);
    if(av_open_input_stream(&avfc, priv->pb, mp_filename, priv->avif, &ap)<0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_open_input_stream() failed\n");
        return NULL;
    }
    filepos=stream_tell(demuxer->stream);
    priv->avfc= avfc;
    if(av_find_stream_info(avfc) < 0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_find_stream_info() failed\n");
        return NULL;
    }
    filepos=stream_tell(demuxer->stream);
    if(!strncmp(avfc->iformat->name,"aac",4))
      get_aac_duration(demuxer,avfc);

    if(!strncmp(avfc->iformat->name,"mp3",4))
       priv->avfc->duration = get_mp3_duration(demuxer) * AV_TIME_BASE;

    /* Add metadata. */
    av_metadata_conv(avfc, NULL, avfc->iformat->metadata_conv);
    while((t = av_metadata_get(avfc->metadata, "", t, AV_METADATA_IGNORE_SUFFIX))){
        demux_info_add(demuxer, t->key, t->value);
    }

    for(i=0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        uint64_t start = av_rescale_q(c->start, c->time_base, (AVRational){1,1000});
        uint64_t end   = av_rescale_q(c->end, c->time_base, (AVRational){1,1000});
        t = av_metadata_get(c->metadata, "title", NULL, 0);
        demuxer_add_chapter(demuxer, t ? t->value : NULL, start, end);
    }
    demuxer->ttaflag = 0;
    for(i=0; i<avfc->nb_streams; i++)
        handle_stream(demuxer, avfc, i);
    if(demuxer->matroflag && !demuxer->ttaflag)
      return NULL;
    if(avfc->nb_programs) {
        int p;
        for (p = 0; p < avfc->nb_programs; p++) {
            AVProgram *program = avfc->programs[p];
            t = av_metadata_get(program->metadata, "title", NULL, 0);
            mp_msg(MSGT_HEADER,MSGL_INFO,"LAVF: Program %d %s\n", program->id, t ? t->value : "");
        }
    }

    mp_msg(MSGT_HEADER,MSGL_V,"LAVF: %d audio and %d video streams found\n",priv->audio_streams,priv->video_streams);
    mp_msg(MSGT_HEADER,MSGL_V,"LAVF: build %d\n", LIBAVFORMAT_BUILD);
    if(!priv->audio_streams) demuxer->audio->id=-2;  // nosound
//    else if(best_audio > 0 && demuxer->audio->id == -1) demuxer->audio->id=best_audio;
    if(!priv->video_streams){
        if(!priv->audio_streams){
	    mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF: no audio or video headers found - broken file?\n");
            return NULL;
        }
        demuxer->video->id=-2; // audio-only
    } //else if (best_video > 0 && demuxer->video->id == -1) demuxer->video->id = best_video;

    filepos=stream_tell(demuxer->stream);

    int j=0;
    AVCodecContext *codec;
    AVCodec *avc;
    demuxer->audio_info = malloc(100*avfc->nb_streams);
    for(i=0; i<avfc->nb_streams; i++){
      if(demuxer->a_streams[i]){
	char *info = malloc(100);
	codec= ((AVStream *)avfc->streams[i])->codec;
	avc = avcodec_find_decoder(codec->codec_id);
	char *cn = avc ? avc->name : "unknown";
	char *codec_name = malloc(100);
	strncpy(codec_name,cn,100);
	
	if((codec_name[0]=='d')&&(codec_name[1]=='c')&&(codec_name[2]=='a'))
	  strncpy(codec_name,"dts",100);
	int a;
	for(a=0;codec_name[a];a++) 
	  if(codec_name[a]>='a'&&codec_name[a]<='z')
	    codec_name[a]-=32; 
	sprintf(info, "%s(%dHz %dCh)--%d", codec_name,codec->sample_rate, codec->channels, i);
	free(codec_name);
	memcpy(demuxer->audio_info+j*100,info, 100);
	j++;
	free(info);
	info = NULL;
      }
    }

    j = 0;
    demuxer->sub_info = malloc(100*avfc->nb_streams);
    for(i=0; i<avfc->nb_streams; i++){
      if(demuxer->s_streams[i]){
	char *info = malloc(100);
	AVStream *st= avfc->streams[i];
	AVMetadataTag *lang = av_metadata_get(st->metadata, "language", NULL, 0);
	codec= st->codec;
        if (lang && lang->value)
	  strcpy(info, lang->value);
	else
	  strcpy(info, "unknown");
	sprintf(info, "%s--%d", info,i);
	memcpy(demuxer->sub_info+j*100,info, 100);
	j++;
	free(info);
	info = NULL;
      }
    }

    if(AV_NOPTS_VALUE == priv->avfc->start_time){
        LOGW("priv->avfc->start_time = AV_NOPTS_VALUE");
        demuxer->start_time = 0;
    }else{
        demuxer->start_time = priv->avfc->start_time;
    }

    return demuxer;
}
static int demux_lavf_fill_buffer(demuxer_t *demux, demux_stream_t *dsds){
    lavf_priv_t *priv= demux->priv;
    AVPacket pkt;
    demux_packet_t *dp;
    demux_stream_t *ds;
    int id;

    mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_lavf_fill_buffer()\n");
    demux->filepos=stream_tell(demux->stream);
    
    if(demux->a_changed_id != -2){
      if(demux->a_changed_id!=demux->audio->id && demux->a_streams[demux->a_changed_id]){
	priv->avfc->streams[demux->audio->id]->discard = AVDISCARD_ALL;
	demux->audio->id = demux->a_changed_id;
	demux->audio->sh = demux->a_streams[demux->audio->id];
	priv->avfc->streams[demux->audio->id]->discard = AVDISCARD_NONE;
      }
      demux->a_changed_id = -2;
    }

    if(demux->s_changed_id){
      if(demux->s_changed_id!=demux->sub->id){
	priv->avfc->streams[demux->sub->id]->discard = AVDISCARD_ALL;
	demux->sub->id = demux->s_changed_id;
	demux->sub->sh = demux->s_streams[demux->sub->id];
	priv->avfc->streams[demux->sub->id]->discard = AVDISCARD_NONE;
      }
      demux->s_changed_id = 0;
    }

    if(av_read_frame(priv->avfc, &pkt) < 0)
        return 0;

    id= pkt.stream_index;
    
    if(id==demux->audio->id){
        // audio
        ds=demux->audio;
        if(!ds->sh){
            ds->sh=demux->a_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF audio ID = %d\n",ds->id);
        }
    } else if(id==demux->video->id){
        // video
        ds=demux->video;
        if(!ds->sh){
            ds->sh=demux->v_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF video ID = %d\n",ds->id);
        }
    } else if(id==demux->sub->id){
        // subtitle
        ds=demux->sub;
        sub_utf8=1;
    } else {
        av_free_packet(&pkt);
        return 1;
    }
    if(pkt.destruct == av_destruct_packet && !CONFIG_MEMALIGN_HACK){
        dp=new_demux_packet(0);
        dp->len=pkt.size;
        dp->buffer=pkt.data;
        pkt.destruct= NULL;
    }else{
        dp=new_demux_packet(pkt.size);
        memcpy(dp->buffer, pkt.data, pkt.size);
        av_free_packet(&pkt);
    }
    if(pkt.pts != AV_NOPTS_VALUE){
      dp->pts=pkt.pts * av_q2d(priv->avfc->streams[id]->time_base);

#if 0
      if(demux->filepos <= demux->movi_start + 1 && dp->pts > 10.0){
	start_time = dp->pts;
      }
      dp->pts -= start_time;
#endif
      priv->last_pts= dp->pts * AV_TIME_BASE;
      // always set endpts for subtitles, even if PKT_FLAG_KEY is not set,
      // otherwise they will stay on screen to long if e.g. ASS is demuxed from mkv
        if((ds == demux->sub || (pkt.flags & PKT_FLAG_KEY)) &&
           pkt.convergence_duration > 0)
            dp->endpts = dp->pts + pkt.convergence_duration * av_q2d(priv->avfc->streams[id]->time_base);
    }
    dp->pos=demux->filepos;
    dp->flags= !!(pkt.flags&PKT_FLAG_KEY);
    // append packet to DS stream:
    ds_add_packet(ds,dp);
    return 1;
}

//char accurate_seek_try;
static void demux_seek_lavf(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags){
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;
    priv->avfc->thumb_index_unused = 0;
    if(demuxer->thumb_eindexfind==1){
      priv->avfc->thumb_index_unused = 1;
    }
    /*FIXME:add for opencore*/
    //priv->avfc->start_time = 0;
    if (flags & SEEK_ABSOLUTE) {
      if(AV_NOPTS_VALUE == priv->avfc->start_time){
	priv->last_pts = 0;
      }else{
	priv->last_pts = priv->avfc->start_time;
      }
    } else {
      if (rel_seek_secs < 0) avsflags = AVSEEK_FLAG_BACKWARD;
    }
    if (flags & SEEK_FACTOR) {
      if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
        return;
      priv->last_pts += rel_seek_secs * priv->avfc->duration;
    } else {
      priv->last_pts += rel_seek_secs * AV_TIME_BASE;
    }

    //accurate_seek_try = 1;
    //avsflags ^= AVSEEK_FLAG_BACKWARD;// The default seek forward hpwang 2011-12-03
    //libavformate/utils.c::av_seek_frame
    if (av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags) < 0) {
        avsflags ^= AVSEEK_FLAG_BACKWARD;
        av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
    }

    if(!strncmp(priv->avfc->iformat->name,"asf",4)){
      demux_stream_t *d_video=demuxer->video;
      if (d_video->id >= 0){
	while(1){
	  if(d_video->flags&1) break; // found a keyframe!
	  if(!ds_fill_buffer(d_video)) break; // skip frame.  EOF?
	}
      }
    }
}

static int demux_lavf_control(demuxer_t *demuxer, int cmd, void *arg)
{
    lavf_priv_t *priv = demuxer->priv;

    switch (cmd) {
        case DEMUXER_CTRL_CORRECT_PTS:
	    return DEMUXER_CTRL_OK;
        case DEMUXER_CTRL_GET_TIME_LENGTH:
	    if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
	        return DEMUXER_CTRL_DONTKNOW;

	    *((double *)arg) = (double)priv->avfc->duration / AV_TIME_BASE;
	    return DEMUXER_CTRL_OK;

	case DEMUXER_CTRL_GET_PERCENT_POS:
	    if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
	        return DEMUXER_CTRL_DONTKNOW;
	    *((int *)arg) = (int)((priv->last_pts - priv->avfc->start_time)*100 / priv->avfc->duration);    
	    return DEMUXER_CTRL_OK;
	case DEMUXER_CTRL_SWITCH_AUDIO:
	case DEMUXER_CTRL_SWITCH_VIDEO:
	{
	    int id = *((int*)arg);
	    int newid = -2;
	    int i, curridx = -1;
	    int nstreams, *pstreams;
	    demux_stream_t *ds;
	    
	    if(cmd == DEMUXER_CTRL_SWITCH_VIDEO)
	    {
	        ds = demuxer->video;
	        nstreams = priv->video_streams;
	        pstreams = priv->vstreams;
	    }
	    else
	    {
	        ds = demuxer->audio;
	        nstreams = priv->audio_streams;
	        pstreams = priv->astreams;
	    }
	    for(i = 0; i < nstreams; i++)
	    {
	        if(pstreams[i] == ds->id) //current stream id
		{
	            curridx = i;
	            break;
	        }
	    }

            if(id == -2) { // no sound
                i = -1;
            } else if(id == -1) { // next track
                i = (curridx + 2) % (nstreams + 1) - 1;
                if (i >= 0)
                    newid = pstreams[i];
	    }
	    else // select track by id
	    {
	        if (id >= 0 && id < nstreams) {
	            i = id;
	            newid = pstreams[i];
	        }
	    }
	    if(i == curridx)
	        return DEMUXER_CTRL_NOTIMPL;
	    else
	    {
	        ds_free_packs(ds);
	        if(ds->id >= 0)
	            priv->avfc->streams[ds->id]->discard = AVDISCARD_ALL;
	        *((int*)arg) = ds->id = newid;
	        if(newid >= 0)
	            priv->avfc->streams[newid]->discard = AVDISCARD_NONE;
	        return DEMUXER_CTRL_OK;
	    }
        }
        case DEMUXER_CTRL_IDENTIFY_PROGRAM:
        {
            demux_program_t *prog = arg;
            AVProgram *program;
            int p, i;
            int start;

            prog->vid = prog->aid = prog->sid = -2;	//no audio and no video by default
            if(priv->avfc->nb_programs < 1)
                return DEMUXER_CTRL_DONTKNOW;

            if(prog->progid == -1)
            {
                p = 0;
                while(p<priv->avfc->nb_programs && priv->avfc->programs[p]->id != priv->cur_program)
                    p++;
                p = (p + 1) % priv->avfc->nb_programs;
            }
            else
            {
                for(i=0; i<priv->avfc->nb_programs; i++)
                    if(priv->avfc->programs[i]->id == prog->progid)
                        break;
                if(i==priv->avfc->nb_programs)
                    return DEMUXER_CTRL_DONTKNOW;
                p = i;
            }
            start = p;
redo:
            program = priv->avfc->programs[p];
            for(i=0; i<program->nb_stream_indexes; i++)
            {
                switch(priv->avfc->streams[program->stream_index[i]]->codec->codec_type)
                {
                    case CODEC_TYPE_VIDEO:
                        if(prog->vid == -2)
                            prog->vid = program->stream_index[i];
                        break;
                    case CODEC_TYPE_AUDIO:
                        if(prog->aid == -2)
                            prog->aid = program->stream_index[i];
                        break;
                    case CODEC_TYPE_SUBTITLE:
                        if(prog->sid == -2 && priv->avfc->streams[program->stream_index[i]]->codec->codec_id == CODEC_ID_TEXT)
                            prog->sid = program->stream_index[i];
                        break;
                }
            }
            if(prog->progid == -1 && prog->vid == -2 && prog->aid == -2)
            {
                p = (p + 1) % priv->avfc->nb_programs;
                if (p == start)
                    return DEMUXER_CTRL_DONTKNOW;
                goto redo;
            }
            priv->cur_program = prog->progid = program->id;
            return DEMUXER_CTRL_OK;
        }
	default:
	    return DEMUXER_CTRL_NOTIMPL;
    }
}

static void demux_close_lavf(demuxer_t *demuxer)
{
    lavf_priv_t* priv = demuxer->priv;
    if (priv){
        if(priv->avfc)
       {
         av_freep(&priv->avfc->key);
         av_close_input_stream(priv->avfc);
        }
        av_freep(&priv->pb);
        free(priv); demuxer->priv= NULL;
    }
}


const demuxer_desc_t demuxer_desc_lavf = {
  "libavformat demuxer",
  "lavf",
  "libavformat",
  "Michael Niedermayer",
  "supports many formats, requires libavformat",
  DEMUXER_TYPE_LAVF,
  0, // Check after other demuxer
  lavf_check_file,
  demux_lavf_fill_buffer,
  demux_open_lavf,
  demux_close_lavf,
  demux_seek_lavf,
  demux_lavf_control
};

const demuxer_desc_t demuxer_desc_lavf_preferred = {
  "libavformat preferred demuxer",
  "lavfpref",
  "libavformat",
  "Michael Niedermayer",
  "supports many formats, requires libavformat",
  DEMUXER_TYPE_LAVF_PREFERRED,
  1,
  lavf_check_preferred_file,
  demux_lavf_fill_buffer,
  demux_open_lavf,
  demux_close_lavf,
  demux_seek_lavf,
  demux_lavf_control
};
