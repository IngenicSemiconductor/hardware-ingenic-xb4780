/*
 * DEMUXER v2.5
 *
 * This file is part of MPlayer.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "m_config.h"
#include "mplayer.h"

#include "libvo/fastmemcpy.h"
#include "avformat.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "mf.h"
#include "demux_audio.h"

#include "libaf/af_format.h"
#include "libmpcodecs/dec_teletext.h"
#include "libmpcodecs/vd_ffmpeg.h"

#ifdef CONFIG_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
#endif
#define MAX_AUDIO_EXTRACTOR_BUFFER_RANGE 1048576
#ifdef CONFIG_LIBAVCODEC
#include "libavcodec/avcodec.h"
#if MP_INPUT_BUFFER_PADDING_SIZE < FF_INPUT_BUFFER_PADDING_SIZE
#error MP_INPUT_BUFFER_PADDING_SIZE is too small!
#endif
#endif
#include <utils/Log.h>
#define EL(x,y...) //{LOGE("%s %d",__FILE__,__LINE__); LOGE(x,##y); }
// This is quite experimental, in particular it will mess up the pts values
// in the queue - on the other hand it might fix some issues like generating
// broken files with mencoder and stream copy.
// Better leave it disabled for now, if we find no use for it this code should
// just be removed again.
#define PARSE_ON_ADD 1

static void clear_parser(sh_common_t *sh);
void resync_video_stream(sh_video_t *sh_video);
void resync_audio_stream(sh_audio_t *sh_audio);

// Demuxer list
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
//extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_avi;
extern const demuxer_desc_t demuxer_desc_y4m;
extern const demuxer_desc_t demuxer_desc_asf;
extern const demuxer_desc_t demuxer_desc_real;
extern const demuxer_desc_t demuxer_desc_smjpeg;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_realaudio;
extern const demuxer_desc_t demuxer_desc_vqf;
extern const demuxer_desc_t demuxer_desc_mov;
extern const demuxer_desc_t demuxer_desc_vivo;
extern const demuxer_desc_t demuxer_desc_fli;
extern const demuxer_desc_t demuxer_desc_film;
extern const demuxer_desc_t demuxer_desc_roq;
extern const demuxer_desc_t demuxer_desc_gif;
extern const demuxer_desc_t demuxer_desc_ogg;
extern const demuxer_desc_t demuxer_desc_avs;
extern const demuxer_desc_t demuxer_desc_pva;
extern const demuxer_desc_t demuxer_desc_nsv;
extern const demuxer_desc_t demuxer_desc_mpeg_ts;
extern const demuxer_desc_t demuxer_desc_lmlm4;
extern const demuxer_desc_t demuxer_desc_mpeg_ps;
extern const demuxer_desc_t demuxer_desc_mpeg_pes;
extern const demuxer_desc_t demuxer_desc_mpeg_es;
extern const demuxer_desc_t demuxer_desc_mpeg_gxf;
extern const demuxer_desc_t demuxer_desc_mpeg4_es;
extern const demuxer_desc_t demuxer_desc_h264_es;
extern const demuxer_desc_t demuxer_desc_rawdv;
extern const demuxer_desc_t demuxer_desc_mpc;
extern const demuxer_desc_t demuxer_desc_audio;
extern const demuxer_desc_t demuxer_desc_xmms;
extern const demuxer_desc_t demuxer_desc_pmp;
//extern const demuxer_desc_t demuxer_desc_mpeg_ty;
extern const demuxer_desc_t demuxer_desc_rtp;
extern const demuxer_desc_t demuxer_desc_rtp_nemesi;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_lavf_preferred;
extern const demuxer_desc_t demuxer_desc_aac;
extern const demuxer_desc_t demuxer_desc_nut;
extern const demuxer_desc_t demuxer_desc_mng;
#ifdef BOARD_HAS_ISDBT
extern const demuxer_desc_t demuxer_desc_isdbt;
#endif
/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#ifdef CONFIG_TV
    &demuxer_desc_tv,
#endif
    &demuxer_desc_mf,
#ifdef CONFIG_LIBAVFORMAT
    &demuxer_desc_lavf_preferred,
#endif
    &demuxer_desc_avi,
    &demuxer_desc_y4m,
    //&demuxer_desc_asf,
    &demuxer_desc_nsv,
    &demuxer_desc_real,
    &demuxer_desc_smjpeg,
    &demuxer_desc_matroska,
    &demuxer_desc_realaudio,
    &demuxer_desc_vqf,
    &demuxer_desc_mov,
    &demuxer_desc_vivo,
    &demuxer_desc_fli,
    &demuxer_desc_film,
    &demuxer_desc_roq,
#ifdef CONFIG_GIF
    &demuxer_desc_gif,
#endif
#ifdef CONFIG_OGGVORBIS
    &demuxer_desc_ogg,
#endif
#ifdef CONFIG_WIN32DLL
    &demuxer_desc_avs,
#endif
    &demuxer_desc_pva,
    &demuxer_desc_mpeg_ts,
    &demuxer_desc_lmlm4,
    &demuxer_desc_mpeg_gxf,
    &demuxer_desc_mpeg4_es,
    &demuxer_desc_h264_es,
    &demuxer_desc_audio,
//    &demuxer_desc_mpeg_ty,
#ifdef CONFIG_LIVE555
    &demuxer_desc_rtp,
#endif
#ifdef CONFIG_LIBNEMESI
    &demuxer_desc_rtp_nemesi,
#endif
    &demuxer_desc_mpeg_ps,
#ifdef CONFIG_LIBAVFORMAT
    &demuxer_desc_lavf,
#endif
    &demuxer_desc_mpeg_pes,
    &demuxer_desc_mpeg_es,
#ifdef CONFIG_MUSEPACK
    &demuxer_desc_mpc,
#endif
#ifdef CONFIG_LIBDV095
    &demuxer_desc_rawdv,
#endif
    &demuxer_desc_aac,
#ifdef CONFIG_LIBNUT
    &demuxer_desc_nut,
#endif
#ifdef CONFIG_XMMS
    &demuxer_desc_xmms,
#endif
#ifdef CONFIG_MNG
    &demuxer_desc_mng,
#endif
    &demuxer_desc_pmp,
    /* Please do not add any new demuxers here. If you want to implement a new
     * demuxer, add it to libavformat, except for wrappers around external
     * libraries and demuxers requiring binary support. */
    NULL
};
/* add for video : when file is over ,then free last packets that currnet_v  to current*/
void ds_free_last_packs(demux_stream_t *ds){
  demux_packet_t *dp=ds->current_v;
    while(dp){
	demux_packet_t *dn=dp->next;
	free_demux_packet(dp);
	dp=dn;
    } 
    //LOGE("ds_free_last_packs end");
}

void free_demuxer_stream(demux_stream_t *ds)
{
    ds_free_packs(ds);
    /* */
    if(ds == ds->demuxer->video)
	ds_free_last_packs(ds);

    free(ds);
}

demux_stream_t *new_demuxer_stream(struct demuxer *demuxer, int id)
{
    demux_stream_t *ds = malloc(sizeof(demux_stream_t));
    *ds = (demux_stream_t){
        .id = id,
        .demuxer = demuxer,
        .asf_seq = -1,
	
 	.first = NULL,
	.last = NULL,
	.current = NULL,
	.current_v = NULL,
	.need_free = 0,
	.seek_flag = 0,	
    };
    return ds;
}


/**
 * Get demuxer description structure for a given demuxer type
 *
 * @param file_format    type of the demuxer
 * @return               structure for the demuxer, NULL if not found
 */
static const demuxer_desc_t *get_demuxer_desc_from_type(int file_format)
{
    int i;

    for (i = 0; demuxer_list[i]; i++)
        if (file_format == demuxer_list[i]->type)
            return demuxer_list[i];

    return NULL;
}


demuxer_t *new_demuxer(stream_t *stream, int type, int a_id, int v_id,
                       int s_id, char *filename)
{
    demuxer_t *d = malloc(sizeof(demuxer_t));
    memset(d, 0, sizeof(demuxer_t));
    d->stream = stream;
    d->stream_pts = MP_NOPTS_VALUE;
    d->reference_clock = MP_NOPTS_VALUE;
    d->movi_start = stream->start_pos;
    d->movi_end = stream->end_pos;
    d->seekable = 1;
    d->synced = 0;
    d->filepos = -1;
    d->audio = new_demuxer_stream(d, a_id);
    d->video = new_demuxer_stream(d, v_id);
    d->sub = new_demuxer_stream(d, s_id);
    d->type = type;
    d->a_changed_id = -2;
    d->s_changed_id = 0;
    d->ists = 0;
    d->tssub_cnt = 0;
    if (type)
        if (!(d->desc = get_demuxer_desc_from_type(type)))
            mp_msg(MSGT_DEMUXER, MSGL_ERR,
                   "BUG! Invalid demuxer type in new_demuxer(), "
                   "big troubles ahead.");
    if (filename) // Filename hack for avs_check_file
        d->filename = strdup(filename);
    stream->eof = 0;
    stream_seek(stream, stream->start_pos);
    return d;
}

const char *sh_sub_type2str(int type)
{
    switch (type) {
    case 't': return "text";
    case 'm': return "movtext";
    case 'a': return "ass";
    case 'v': return "vobsub";
    case 'x': return "xsub";
    case 'b': return "dvb";
    case 'd': return "dvb-teletext";
    case 'p': return "hdmv pgs";
    }
    return "unknown";
}


int ass_convert_timestamp(unsigned char *str, int *sec, int *msec) {
  int hh, mm, ss, ms = 0;
  if (sscanf(str, "%d:%d:%d.%d", &hh, &mm, &ss, &ms) < 3) {
    hh = 0;
    if (sscanf(str, "%d:%d.%d", &mm, &ss, &ms) < 2) {
      mm = 0;
      if (sscanf(str, "%d.%d", &ss, &ms) < 1) {
	ss = 0;
	ms = 0;
      }
    }
  }
  if (sec)
    *sec = hh * 3600 + mm * 60 + ss;
  if (msec)
    *msec = ms;
  return 1;
}

sh_sub_t *new_sh_sub_sid(demuxer_t *demuxer, int id, int sid, const char *lang)
{
    dvdsub_id = 0;
    if (id > MAX_S_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested sub stream id overflow (%d > %d)\n", id,
               MAX_S_STREAMS);
        return NULL;
    }
    if (demuxer->s_streams[id])
        mp_msg(MSGT_DEMUXER, MSGL_WARN, "Sub stream %i redefined\n", id);
    else {
        sh_sub_t *sh = calloc(1, sizeof(sh_sub_t));
        demuxer->s_streams[id] = sh;
        sh->sid = sid;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SUBTITLE_ID=%d\n", sid);
        if (lang && lang[0] && strcmp(lang, "und")) {
            sh->lang = strdup(lang);
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", sid, lang);
        }
    }

    if (sid == dvdsub_id) {
        demuxer->sub->id = id;
        demuxer->sub->sh = demuxer->s_streams[id];
    }
    return demuxer->s_streams[id];
}

static void free_sh_sub(sh_sub_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_sub at %p\n", sh);
    free(sh->extradata);
#ifdef CONFIG_ASS
    if (sh->ass_track)
        ass_free_track(sh->ass_track);
#endif
    free(sh->lang);
#ifdef CONFIG_LIBAVCODEC
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
}

sh_audio_t *new_sh_audio_aid(demuxer_t *demuxer, int id, int aid, const char *lang)
{
    if (id > MAX_A_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested audio stream id overflow (%d > %d)\n", id,
               MAX_A_STREAMS);
        return NULL;
    }
    if (demuxer->a_streams[id])
        mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_AudioStreamRedefined, id);
    else {
        sh_audio_t *sh = calloc(1, sizeof(sh_audio_t));
        mp_msg(MSGT_DEMUXER, MSGL_V, MSGTR_FoundAudioStream, id);
        demuxer->a_streams[id] = sh;
        sh->aid = aid;
        sh->ds = demuxer->audio;
        // set some defaults
        sh->samplesize = 2;
        sh->sample_format = AF_FORMAT_S16_NE;
        sh->audio_out_minsize = 8192;   /* default size, maybe not enough for Win32/ACM */
	sh->need_parsing=0;
        sh->pts = MP_NOPTS_VALUE;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_ID=%d\n", aid);
        if (lang && lang[0] && strcmp(lang, "und")) {
            sh->lang = strdup(lang);
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_LANG=%s\n", aid, lang);
        }
    }
    return demuxer->a_streams[id];
}

void free_sh_audio(demuxer_t *demuxer, int id)
{
    sh_audio_t *sh = demuxer->a_streams[id];
    demuxer->a_streams[id] = NULL;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_audio at %p\n", sh);
    free(sh->wf);
    free(sh->codecdata);
    free(sh->lang);
#ifdef CONFIG_LIBAVCODEC
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
}

sh_video_t *new_sh_video_vid(demuxer_t *demuxer, int id, int vid)
{
    if (id > MAX_V_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested video stream id overflow (%d > %d)\n", id,
               MAX_V_STREAMS);
        return NULL;
    }
    if (demuxer->v_streams[id])
        mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_VideoStreamRedefined, id);
    else {
        sh_video_t *sh = calloc(1, sizeof(sh_video_t));
        mp_msg(MSGT_DEMUXER, MSGL_V, MSGTR_FoundVideoStream, id);
        demuxer->v_streams[id] = sh;
        sh->vid = vid;
        sh->ds = demuxer->video;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ID=%d\n", vid);
    }
    return demuxer->v_streams[id];
}

void free_sh_video(sh_video_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_video at %p\n", sh);
    free(sh->bih);
#ifdef CONFIG_LIBAVCODEC
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
}

void free_demuxer(demuxer_t *demuxer)
{
    int i;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing %s demuxer at %p\n",
           demuxer->desc->shortdesc, demuxer);
    EL("< %s > demuxer->desc->close = 0x%08x, name = %s, type = %d",
	 __FUNCTION__, demuxer->desc->close, demuxer->desc->name, demuxer->desc->type);
    if (demuxer->desc->close)
        demuxer->desc->close(demuxer);
    // Very ugly hack to make it behave like old implementation
    if (demuxer->desc->type == DEMUXER_TYPE_DEMUXERS)
        goto skip_streamfree;
    // free streams:
    EL("< %s > free streams", __FUNCTION__);
    for (i = 0; i < MAX_A_STREAMS; i++)
        if (demuxer->a_streams[i])
            free_sh_audio(demuxer, i);
    for (i = 0; i < MAX_V_STREAMS; i++)
        if (demuxer->v_streams[i])
            free_sh_video(demuxer->v_streams[i]);
    for (i = 0; i < MAX_S_STREAMS; i++)
        if (demuxer->s_streams[i])
            free_sh_sub(demuxer->s_streams[i]);
    // free demuxers:
    EL("< %s > free demuxers", __FUNCTION__);
    free_demuxer_stream(demuxer->audio);
    free_demuxer_stream(demuxer->video);
    free_demuxer_stream(demuxer->sub);
    free(demuxer->audio_info);
    if (demuxer->sub_info) free(demuxer->sub_info);
 skip_streamfree:
    EL("< %s > skip_streamfree", __FUNCTION__);
    if (demuxer->info) {
        for (i = 0; demuxer->info[i] != NULL; i++)
            free(demuxer->info[i]);
        free(demuxer->info);
    }
    free(demuxer->filename);
    if (demuxer->chapters) {
        for (i = 0; i < demuxer->num_chapters; i++)
            free(demuxer->chapters[i].name);
        free(demuxer->chapters);
    }
    if (demuxer->attachments) {
        for (i = 0; i < demuxer->num_attachments; i++) {
            free(demuxer->attachments[i].name);
            free(demuxer->attachments[i].type);
            free(demuxer->attachments[i].data);
        }
        free(demuxer->attachments);
    }
#if 0
    if (demuxer->teletext)
        teletext_control(demuxer->teletext, TV_VBI_CONTROL_STOP, NULL);
#endif
    free(demuxer);
}


static void ds_add_packet_internal(demux_stream_t *ds, demux_packet_t *dp)
{
    // append packet to DS stream:
    ++ds->packs;
    ds->bytes += dp->len;

    /* for video current means last : do a new link with current_v and current */
    if(ds==ds->demuxer->video){
	/*first packet in stream*/
	if(ds->current_v == NULL){
	    ds->current_v = ds->current = dp;
	}else{
	    ds->current->next = dp;
	    ds->current = dp;
	}
    }

    if (ds->last) {
        // next packet in stream
        ds->last->next = dp;
        ds->last = dp;
    } else {
        ds->first = ds->last = dp;
    }

    ds->start = dp->pts;
    ds->end = dp->endpts;

    mp_dbg(MSGT_DEMUXER, MSGL_DBG2,
           "DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
           (ds == ds->demuxer->audio) ? "d_audio" : "d_video", dp->len,
           dp->pts, (unsigned int) dp->pos, ds->demuxer->audio->packs,
           ds->demuxer->video->packs);
}

#ifdef CONFIG_LIBAVCODEC
static void allocate_parser(AVCodecContext **avctx, AVCodecParserContext **parser, unsigned format)
{
    enum CodecID codec_id = CODEC_ID_NONE;

    avcodec_init();
    avcodec_register_all();

//    init_avcodec();

    switch (format) {
    case 0x2000:
    case 0x332D6361:
    case 0x332D4341:
    case MKTAG('d', 'n', 'e', 't'):
    case MKTAG('s', 'a', 'c', '3'):
        codec_id = CODEC_ID_AC3;
        break;
    case MKTAG('E', 'A', 'C', '3'):
        codec_id = CODEC_ID_EAC3;
        break;
    case 0x2001:
    case 0x86:
        codec_id = CODEC_ID_DTS;
        break;
    case MKTAG('f', 'L', 'a', 'C'):
        codec_id = CODEC_ID_FLAC;
        break;
    case MKTAG('M', 'L', 'P', ' '):
        codec_id = CODEC_ID_MLP;
        break;
    case 0x55:
    case 0x5500736d:
    case MKTAG('.', 'm', 'p', '3'):
    case MKTAG('M', 'P', 'E', ' '):
    case MKTAG('L', 'A', 'M', 'E'):
        codec_id = CODEC_ID_MP3;
        break;
    case 0x50:
    case MKTAG('.', 'm', 'p', '2'):
    case MKTAG('.', 'm', 'p', '1'):
        codec_id = CODEC_ID_MP2;
        break;
    case MKTAG('T', 'R', 'H', 'D'):
        codec_id = CODEC_ID_TRUEHD;
        break;
    }
    if (codec_id != CODEC_ID_NONE) {
        *avctx = avcodec_alloc_context();
        if (!*avctx)
            return;
        *parser = av_parser_init(codec_id);
        if (!*parser)
            av_freep(avctx);
    }
}

static void get_parser(sh_common_t *sh, AVCodecContext **avctx, AVCodecParserContext **parser)
{
    *avctx  = NULL;
    *parser = NULL;

    if (!sh || !sh->needs_parsing){
        return;
    }
    *avctx  = sh->avctx;
    *parser = sh->parser;
    if (*parser)
        return;
    allocate_parser(avctx, parser, sh->format);
    sh->avctx  = *avctx;
    sh->parser = *parser;
    if (!*parser)
      {
	  //LOGE("allocate_parser is invalid\n");
      }
}

int ds_parse(demux_stream_t *ds, uint8_t **buffer, int *len, double pts, loff_t pos)
{

    AVCodecContext *avctx;
    AVCodecParserContext *parser;
    get_parser(ds, &avctx, &parser);
    if (!parser)
        return *len;
    return av_parser_parse2(parser, avctx, buffer, len, *buffer, *len, pts, pts, pos);
}

static void clear_parser(sh_common_t *sh)
{
    av_parser_close(sh->parser);
    sh->parser = NULL;
    av_freep(&sh->avctx);
}

void ds_clear_parser(demux_stream_t *ds)
{
    if (!ds->sh)
        return;
    clear_parser(ds->sh);
}
#endif

void ds_add_packet(demux_stream_t *ds, demux_packet_t *dp)
{
#if PARSE_ON_ADD && defined(CONFIG_LIBAVCODEC)
    int len = dp->len;
    int pos = 0;
    int parsed_flag = 0;
    int dp_flag = 0;
    
    while (len > 0) {
        uint8_t *parsed_start = dp->buffer + pos;
        int parsed_len = len;
        int consumed = ds_parse(ds->sh, &parsed_start, &parsed_len, dp->pts, dp->pos);
        pos += consumed;
        len -= consumed;
        if (parsed_start == dp->buffer && parsed_len == dp->len) {
	    dp_flag = 1;
            ds_add_packet_internal(ds, dp);
        } else if (parsed_len) {
	    parsed_flag = 1;
            demux_packet_t *dp2 = new_demux_packet(parsed_len);
            dp2->pos = dp->pos;
            dp2->pts = dp->pts; // should be parser->pts but that works badly
            memcpy(dp2->buffer, parsed_start, parsed_len);
            ds_add_packet_internal(ds, dp2);
        }
    }
    if(parsed_flag == 1 && dp_flag == 0)
      free_demux_packet(dp);	  
#else
    ds_add_packet_internal(ds, dp);
#endif
}

void ds_read_packet(demux_stream_t *ds, stream_t *stream, int len,
                    double pts, loff_t pos, int flags)
{
    demux_packet_t *dp = new_demux_packet(len);
    len = stream_read(stream, dp->buffer, len);
    resize_demux_packet(dp, len);
    dp->pts = pts;
    dp->pos = pos;
    dp->flags = flags;
    // append packet to DS stream:
    ds_add_packet(ds, dp);

#if PARSE_ON_ADD && defined(CONFIG_LIBAVCODEC)
    if (!dp->len) {
      if (dp->buffer) free(dp->buffer);
      free(dp); 
    }
#endif
}

// return value:
//     0 = EOF or no stream found or invalid type
//     1 = successfully read a packet

int demux_fill_buffer(demuxer_t *demux, demux_stream_t *ds)
{
    // Note: parameter 'ds' can be NULL!
    return demux->desc->fill_buffer(demux, ds);
}

// return value:
//     0 = EOF
//     1 = successful
#define MAX_ACUMULATED_PACKETS 64
int ds_fill_buffer(demux_stream_t *ds)
{
    demuxer_t *demux = ds->demuxer;
#if 1
    if(ds==ds->demuxer->video){
      //LOGE("need_free = %d packs = %d ---",ds->need_free,ds->packs);
      if((ds->current_v)&&(ds->need_free > 1)){
	      demux_packet_t *p= ds->current_v;
	      ds->current_v = ds->current_v->next;
	      free_demux_packet(p);
	      ds->need_free -= 1;
	  }
      }
#else
    if(ds==ds->demuxer->video){
      if(ds->current_v)
	free_demux_packet(ds->current_v);
      ds->current_v = NULL;
    }
#endif      
      else{
	if (ds->current)
	  free_demux_packet(ds->current);
	ds->current = NULL;
      }
    if (mp_msg_test(MSGT_DEMUXER, MSGL_DBG3)) {
        if (ds == demux->audio)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(d_audio) called\n");
        else if (ds == demux->video)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(d_video) called\n");
        else if (ds == demux->sub)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3, "ds_fill_buffer(d_sub) called\n");
        else
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(unknown 0x%X) called\n", (unsigned int) ds);
    }
    while (1) {
        if (ds->packs) {
            demux_packet_t *p = ds->first;
	    if((ds == ds->demuxer->audio) && (p->len >= MAX_AUDIO_EXTRACTOR_BUFFER_RANGE)){ 
	      p->len = 0; 
	    } 
#if 0
            if (demux->reference_clock != MP_NOPTS_VALUE) {
                if (   p->pts != MP_NOPTS_VALUE
                    && p->pts >  demux->reference_clock
                    && ds->packs < MAX_ACUMULATED_PACKETS) {
                    if (demux_fill_buffer(demux, ds))
                        continue;
                }
            }
#endif

            // copy useful data:
            ds->buffer = p->buffer;
            ds->buffer_pos = 0;
            ds->buffer_size = p->len;
            ds->pos = p->pos;
            ds->dpos += p->len; // !!!
            ++ds->pack_no;
            if (p->pts != MP_NOPTS_VALUE) {
                ds->pts = p->pts;
                ds->pts_bytes = 0;
            }
            ds->pts_bytes += p->len;    // !!!
            if (p->stream_pts != MP_NOPTS_VALUE)
                demux->stream_pts = p->stream_pts;
            ds->flags = p->flags;
            // unlink packet:
            ds->bytes -= p->len;

	    if(ds == ds->demuxer->audio)
	      ds->current = p;
#if 0	    
	    if(ds == ds->demuxer->video)
	      ds->current_v = p;
#endif     
	    ds->first = p->next;
            if (!ds->first)
                ds->last = NULL;
            --ds->packs;
            return 1;
        }
        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyAudioInBuffer,
                   demux->audio->packs, demux->audio->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            break;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyVideoInBuffer,
                   demux->video->packs, demux->video->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            break;
        }
#ifdef BOARD_HAS_ISDBT
	int ret = demux_fill_buffer(demux, ds);
	if(ret == 2){
	  /*for isdbt , 2 means no data geted but not timeout*/
	  return ret;
	}else if (ret == 0){
#else
        if (!demux_fill_buffer(demux, ds)) {
#endif

#if PARSE_ON_ADD && defined(CONFIG_LIBAVCODEC)
            uint8_t *parsed_start = NULL;
            int parsed_len = 0;
            ds_parse(ds->sh, &parsed_start, &parsed_len, MP_NOPTS_VALUE, 0);
            if (parsed_len) {
                demux_packet_t *dp2 = new_demux_packet(parsed_len);
                dp2->pts = MP_NOPTS_VALUE;
                memcpy(dp2->buffer, parsed_start, parsed_len);
                ds_add_packet_internal(ds, dp2);
                continue;
            }
#endif
            mp_dbg(MSGT_DEMUXER, MSGL_DBG2,
                   "ds_fill_buffer()->demux_fill_buffer() failed\n");
            break; // EOF
        }
    }
    ds->buffer_pos = ds->buffer_size = 0;
    ds->buffer = NULL;
    mp_msg(MSGT_DEMUXER, MSGL_V,
           "ds_fill_buffer: EOF reached (stream: %s)  \n",
           ds == demux->audio ? "audio" : "video");
    ds->eof = 1;
    return 0;
}

int demux_read_data(demux_stream_t *ds, unsigned char *mem, int len)
{
    int x;
    int bytes = 0;
    while (len > 0) {
        x = ds->buffer_size - ds->buffer_pos;
        if (x == 0) {
            if (!ds_fill_buffer(ds))
                return bytes;
        } else {
            if (x > len)
                x = len;
            if (mem)
                fast_memcpy(mem + bytes, &ds->buffer[ds->buffer_pos], x);
            bytes += x;
            len -= x;
            ds->buffer_pos += x;
        }
    }
    return bytes;
}

/**
 * \brief read data until the given 3-byte pattern is encountered, up to maxlen
 * \param mem memory to read data into, may be NULL to discard data
 * \param maxlen maximum number of bytes to read
 * \param read number of bytes actually read
 * \param pattern pattern to search for (lowest 8 bits are ignored)
 * \return whether pattern was found
 */
int demux_pattern_3(demux_stream_t *ds, unsigned char *mem, int maxlen,
                    int *read, uint32_t pattern)
{
    register uint32_t head = 0xffffff00;
    register uint32_t pat = pattern & 0xffffff00;
    int total_len = 0;
    do {
        register unsigned char *ds_buf = &ds->buffer[ds->buffer_size];
        int len = ds->buffer_size - ds->buffer_pos;
        register long pos = -len;
        if (unlikely(pos >= 0)) { // buffer is empty
            ds_fill_buffer(ds);
            continue;
        }
        do {
            head |= ds_buf[pos];
            head <<= 8;
        } while (++pos && head != pat);
        len += pos;
        if (total_len + len > maxlen)
            len = maxlen - total_len;
        len = demux_read_data(ds, mem ? &mem[total_len] : NULL, len);
        total_len += len;
    } while ((head != pat || total_len < 3) && total_len < maxlen && !ds->eof);
    if (read)
        *read = total_len;
    return total_len >= 3 && head == pat;
}

void ds_free_packs(demux_stream_t *ds)
{
    demux_packet_t *dp = ds->first;

  if(ds==ds->demuxer->video){
      demux_packet_t *dp_v = ds->current_v;
      if(dp_v && ds->first){

	  while(1){
	      /*in ds : just have one packet*/
	      if(dp_v == ds->first){
		  ds->current_v = NULL;
		  break;
	      }
	      if(dp_v->next == ds->first){
		  dp_v->next = NULL;
		  break;
	      }
	      dp_v = dp_v->next;
	  }
	  ds->current = dp_v;
      }
  }
    while (dp) {
        demux_packet_t *dn = dp->next;
        free_demux_packet(dp);
        dp = dn;
    }
    if (ds->asf_packet) {
        // free unfinished .asf fragments:
        free(ds->asf_packet->buffer);
        free(ds->asf_packet);
        ds->asf_packet = NULL;
    }
    ds->first = ds->last = NULL;
    ds->packs = 0; // !!!!!
    ds->bytes = 0;

    if ((ds->current) && (ds == ds->demuxer->audio)){
        free_demux_packet(ds->current);
	ds->current = NULL;
    }
    ds->buffer = NULL;
    ds->buffer_pos = ds->buffer_size;
    ds->pts = 0;
    ds->pts_bytes = 0;
}

int ds_get_packet(demux_stream_t *ds, unsigned char **start)
{
    int len;
    if (ds->buffer_pos >= ds->buffer_size) {
#ifdef BOARD_HAS_ISDBT
      int ret = ds_fill_buffer(ds);
      /**/
      if(ret == 2){
	  /*for isdbt, 2 means no data geted but no timeout*/
	  return -2;
      }else if(ret == 0){	  
#else      
        if (!ds_fill_buffer(ds)) {
#endif 

            // EOF
            *start = NULL;
            return -1;
        }
    }
    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
    return len;
}

int ds_get_packet_pts(demux_stream_t *ds, unsigned char **start, double *pts)
{
    int len;
    int format = 0;
    *pts = MP_NOPTS_VALUE;

    len = ds_get_packet(ds, start);

    if (len < 0)
        return len;
    // Return pts unless this read starts from the middle of a packet
    if (len == ds->buffer_pos)
        *pts = ds->current->pts;

    if ((sh_audio_t*)(ds->sh) && (ds==ds->demuxer->audio)) format = ((sh_audio_t*)(ds->sh))->format;
    
    if (len && (format == 0x2000) && (ds==ds->demuxer->audio)){
      uint8_t *st = *start;
      if ((st[0] != 0xb) || (st[1] != 0x77)){
	LOGE("AC3 sync_word error");
	return 0;
      }
    }
    return len;
}

/**
 * Get a subtitle packet. In particular avoid reading the stream.
 * \param pts input: maximum pts value of subtitle packet. NOPTS or NULL for any.
 *            output: start/referece pts of subtitle
 *            May be NULL.
 * \param endpts output: pts for end of display time. May be NULL.
 * \return -1 if no packet is available
 */
#if 0
int ds_get_packet_sub(demux_stream_t *ds, unsigned char **start,
                      double *pts, double *endpts)
{

    int len;
    *start = NULL;

    if (pts)
        *pts    = MP_NOPTS_VALUE;
    if (endpts){
        *endpts = MP_NOPTS_VALUE;
    }

    if (ds->buffer_pos >= ds->buffer_size) {
        if (!ds->packs)
            return -1;  // no sub
        if (!ds_fill_buffer(ds))
            return -1;  // EOF
    }
    // only start of buffer has valid pts
    if (ds->buffer_pos == 0) {
      if (endpts){
	*endpts = ds->current->endpts;
      }
      if (pts) {
	*pts    = ds->current->pts;
	// check if we are too early
	if (*pts != MP_NOPTS_VALUE && ds->current->pts != MP_NOPTS_VALUE &&
	    ds->current->pts > *pts)
	  return -1;
      }
    }

    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
    return len;
}
#else
int ds_get_packet_sub(demux_stream_t *ds, unsigned char **start,
                      double *pts, double *endpts)
{
    int len;
    *start = NULL;

    if (ds->buffer_pos >= ds->buffer_size) {
        if (!ds->packs)
            return -2;  // no sub
        if (!ds_fill_buffer(ds))
            return -1;  // EOF
    }

    *pts = ds->start;
    *endpts = ds->end;

    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
 
    //LOGE("len = %d, start= %f,end = %f",len, ds->start,ds->end);
    return len;
}
#endif

int ds_get_next_flags(demux_stream_t *ds){
  demuxer_t* demux = ds->demuxer;
  
  while(!ds->first) {
    if(demux->audio->packs>=MAX_PACKS || demux->audio->bytes>=MAX_PACK_BYTES){
      return -1;
    }
    if(demux->video->packs>=MAX_PACKS || demux->video->bytes>=MAX_PACK_BYTES){
      return -1;
    }
    if(!demux_fill_buffer(demux,ds))
    {
        return -1;
    }
  }
  return ds->first->flags;
}

double ds_get_next_pts(demux_stream_t *ds)
{
    demuxer_t *demux = ds->demuxer;
    // if we have not read from the "current" packet, consider it
    // as the next, otherwise we never get the pts for the first packet.
    while (!ds->first && (!ds->current || ds->buffer_pos)) {
        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyAudioInBuffer,
                   demux->audio->packs, demux->audio->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            return MP_NOPTS_VALUE;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyVideoInBuffer,
                   demux->video->packs, demux->video->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            return MP_NOPTS_VALUE;
        }
        if (!demux_fill_buffer(demux, ds))
            return MP_NOPTS_VALUE;
    }
    // take pts from "current" if we never read from it.
    if (ds->current && !ds->buffer_pos)
        return ds->current->pts;
    return ds->first->pts;
}

// ====================================================================

void demuxer_help(void)
{
    int i;

    mp_msg(MSGT_DEMUXER, MSGL_INFO, "Available demuxers:\n");
    mp_msg(MSGT_DEMUXER, MSGL_INFO, " demuxer:  type  info:  (comment)\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DEMUXERS\n");
    for (i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type > DEMUXER_TYPE_MAX)   // Don't display special demuxers
            continue;
        if (demuxer_list[i]->comment && strlen(demuxer_list[i]->comment))
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %2d   %s (%s)\n",
                   demuxer_list[i]->name, demuxer_list[i]->type,
                   demuxer_list[i]->info, demuxer_list[i]->comment);
        else
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %2d   %s\n",
                   demuxer_list[i]->name, demuxer_list[i]->type,
                   demuxer_list[i]->info);
    }
}


/**
 * Get demuxer type for a given demuxer name
 *
 * @param demuxer_name    string with demuxer name of demuxer number
 * @param force           will be set if demuxer should be forced.
 *                        May be NULL.
 * @return                DEMUXER_TYPE_xxx, -1 if error or not found
 */
int get_demuxer_type_from_name(char *demuxer_name, int *force)
{
    int i;
    long type_int;
    char *endptr;

    if (!demuxer_name || !demuxer_name[0])
        return DEMUXER_TYPE_UNKNOWN;
    if (force)
        *force = demuxer_name[0] == '+';
    if (demuxer_name[0] == '+')
        demuxer_name = &demuxer_name[1];
    for (i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type > DEMUXER_TYPE_MAX)   // Can't select special demuxers from commandline
            continue;
        if (strcmp(demuxer_name, demuxer_list[i]->name) == 0)
            return demuxer_list[i]->type;
    }

    // No match found, try to parse name as an integer (demuxer number)
    type_int = strtol(demuxer_name, &endptr, 0);
    if (*endptr)  // Conversion failed
        return -1;
    if ((type_int > 0) && (type_int <= DEMUXER_TYPE_MAX))
        return (int) type_int;

    return -1;
}

int extension_parsing = 1; // 0=off 1=mixed (used only for unstable formats)

int correct_pts = 0;
//int user_correct_pts = -1;
int user_correct_pts = 0;//by gjiwang

/*
  NOTE : Several demuxers may be opened at the same time so
  demuxers should NEVER rely on an external var to enable them
  self. If a demuxer can't do any auto-detection it should only use
  file_format. The user can explicitly set file_format with the -demuxer
  option so there is really no need for another extra var.
  For convenience an option can be added to set file_format directly
  to the right type (ex: rawaudio,rawvideo).
  Also the stream can override the file_format so a demuxer which rely
  on a special stream type can set file_format at the stream level
  (ex: tv,mf).
*/

static demuxer_t *demux_open_stream(stream_t *stream, int file_format,
                                    int force, int audio_id, int video_id,
                                    int dvdsub_id, char *filename)
{
    demuxer_t *demuxer = NULL;

    sh_video_t *sh_video = NULL;

    const demuxer_desc_t *demuxer_desc;
    int fformat = 0;
    int i;

    // If somebody requested a demuxer check it
    if (file_format) {
        if ((demuxer_desc = get_demuxer_desc_from_type(file_format))) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);
            if (demuxer_desc->check_file)
                fformat = demuxer_desc->check_file(demuxer);
            if (force || !demuxer_desc->check_file)
                fformat = demuxer_desc->type;
            if (fformat != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    // Move messages to demuxer detection code?
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
                    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
                        demuxer = demux2;
                        goto dmx_open;
                    }
                } else {
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    return demux_open_stream(stream, fformat, force, audio_id,
                                             video_id, dvdsub_id, filename);
                }
            }
            // Check failed for forced demuxer, quit
            free_demuxer(demuxer);
            return NULL;
        }
    }
    // Test demuxers with safe file checks
    for (i = 0; (demuxer_desc = demuxer_list[i]); i++) {
        if (demuxer_desc->safe_check) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);

	    fformat = demuxer_desc->check_file(demuxer);	   
            //if ((fformat = demuxer_desc->check_file(demuxer)) != 0) {
	    EL("fformat = %d, demuxer_desc->type = %d, demuxer_desc->name = %s",fformat,demuxer_desc->type, demuxer_desc->name);
	    if ((fformat) != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
#if defined(WORK_AROUND_FOR_CTS_MEDIAPLAYERFLAKY_OPENDEMUX)
		    if (filename != NULL && strstr(filename, DEMUXER_CTS_MEDIA_FLAKY)){
		    }else{
		      if (!demuxer->desc->open
			  || (demux2 = demuxer->desc->open(demuxer))) {
			demuxer = demux2;
			EL("");
			goto dmx_open;
		      }
		    }
#else
		    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
		      demuxer = demux2;
		      EL("");
		      goto dmx_open;
                    }
#endif
                } else {
                    if (fformat == DEMUXER_TYPE_PLAYLIST)
                        return demuxer; // handled in mplayer.c
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    demuxer = demux_open_stream(stream, fformat, force,
                                                audio_id, video_id,
                                                dvdsub_id, filename);
                    if (demuxer)
                        return demuxer; // done!
                    file_format = DEMUXER_TYPE_UNKNOWN;
                }
            }
            free_demuxer(demuxer);
            demuxer = NULL;
        }
    }

    // If no forced demuxer perform file extension based detection
    // Ok. We're over the stable detectable fileformats, the next ones are
    // a bit fuzzy. So by default (extension_parsing==1) try extension-based
    // detection first:
    if (file_format == DEMUXER_TYPE_UNKNOWN && filename
        && extension_parsing == 1) {
        file_format = demuxer_type_by_filename(filename);
        if (file_format != DEMUXER_TYPE_UNKNOWN) {
            // we like recursion :)
            demuxer = demux_open_stream(stream, file_format, force, audio_id,
                                        video_id, dvdsub_id, filename);
            if (demuxer)
                return demuxer; // done!
            file_format = DEMUXER_TYPE_UNKNOWN; // continue fuzzy guessing...
            mp_msg(MSGT_DEMUXER, MSGL_V,
                   "demuxer: continue fuzzy content-based format guessing...\n");
        }
    }

    // Try detection for all other demuxers
    for (i = 0; (demuxer_desc = demuxer_list[i]); i++) {
        if (!demuxer_desc->safe_check && demuxer_desc->check_file) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);
            if ((fformat = demuxer_desc->check_file(demuxer)) != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
                    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
                        demuxer = demux2;
                        goto dmx_open;
                    }
                } else {
                    if (fformat == DEMUXER_TYPE_PLAYLIST)
                        return demuxer; // handled in mplayer.c
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    demuxer = demux_open_stream(stream, fformat, force,
                                                audio_id, video_id,
                                                dvdsub_id, filename);
                    if (demuxer)
                        return demuxer; // done!
                    file_format = DEMUXER_TYPE_UNKNOWN;
                }
            }
            free_demuxer(demuxer);
            demuxer = NULL;
        }
    }

    return NULL;
    //====== File format recognized, set up these for compatibility: =========
 dmx_open:

    demuxer->file_format = file_format;
   
    if ((sh_video = demuxer->video->sh) && sh_video->bih) {
        int biComp = le2me_32(sh_video->bih->biCompression);
        mp_msg(MSGT_DEMUX, MSGL_INFO,
               "VIDEO:  [%.4s]  %dx%d  %dbpp  %5.3f fps  %5.1f kbps (%4.1f kbyte/s)\n",
               (char *) &biComp, sh_video->bih->biWidth,
               sh_video->bih->biHeight, sh_video->bih->biBitCount,
               sh_video->fps, sh_video->i_bps * 0.008f,
               sh_video->i_bps / 1024.0f);
    }

#ifdef CONFIG_ASS
    if (ass_enabled && ass_library) {
        for (i = 0; i < MAX_S_STREAMS; ++i) {
            sh_sub_t *sh = demuxer->s_streams[i];
            if (sh && sh->type == 'a') {
                sh->ass_track = ass_new_track(ass_library);
                if (sh->ass_track && sh->extradata)
                    ass_process_codec_private(sh->ass_track, sh->extradata,
                                              sh->extradata_len);
            } else if (sh && sh->type != 'v')
                sh->ass_track = ass_default_track(ass_library);
        }
    }
#endif
    return demuxer;
}

char *audio_stream = NULL;
char *sub_stream = NULL;
int audio_stream_cache = 0;

char *demuxer_name = NULL;       // parameter from -demuxer
char *audio_demuxer_name = NULL; // parameter from -audio-demuxer
char *sub_demuxer_name = NULL;   // parameter from -sub-demuxer

extern float stream_cache_min_percent;
extern float stream_cache_seek_min_percent;

typedef struct{
    int32_t geodata_start_tag;
    int32_t lang_code;
  //unsigned char latitude[8];
  //unsigned char longitudex[9 + 1];//for '\0'
    unsigned char latitude_longitudex[8 + 9 + 1];//+1 for '\0'
    //int8_t char;//'/', 0x2F  
}GeoDataBox;

#define printf LOGE

#ifndef memstr
static char * memstr(char *haystack, char *needle, int size)
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
    if(pch){
        pch = memstr(pch + 4, "\xA9xyz", udta_len - (pch + 4 - udta_buf));
	
	if(pch){
	    GeoDataBox geoDataBox;
	    memcpy(&geoDataBox, pch, sizeof(GeoDataBox));
	    geoDataBox.latitude_longitudex[8+ 9] = '\0';

	    metadata.key = "location";
	    metadata.value = &geoDataBox.latitude_longitudex;
	    printf("demux_info_add: k->key = %s, t->value = %s, demuxer = 0x%x\n", metadata.key, metadata.value, demuxer);
	    demux_info_add(demuxer, metadata.key, metadata.value);
	    fould_geodata = 1;
	}else{
	    printf("Geodata xyz NOT fould\n");
	}

    }else{
        //printf("udta box not fould\n");
    }


    stream_seek(demuxer->stream, old_filepos);

    return fould_geodata;
}

demuxer_t *demux_open(stream_t *vs, int file_format, int audio_id,
                      int video_id, int dvdsub_id, char *filename)
{
    stream_t *as = NULL, *ss = NULL;
    demuxer_t *vd, *ad = NULL, *sd = NULL;
    demuxer_t *res;
    int afmt = DEMUXER_TYPE_UNKNOWN, sfmt = DEMUXER_TYPE_UNKNOWN;
    int demuxer_type;
    int audio_demuxer_type = 0, sub_demuxer_type = 0;
    int demuxer_force = 0, audio_demuxer_force = 0, sub_demuxer_force = 0;

#ifdef BOARD_HAS_ISDBT
    if(filename)
    {
	/* first select isdbt demuxer */
	if( strncmp(filename, "mtv:", strlen("mtv:") ) == 0 )
	{
	    demuxer_t *demuxer=NULL;    
	    LOGE("%s %d \n ISDBT demuxer open %s %d %d",
		 __FILE__,__LINE__,filename,audio_id,video_id);
	    /* DEMUXER_TYPE_ISDBT 46  
	     * vs no used in ISDBT
	     */
	    
	    if(demuxer = malloc(sizeof(demuxer_t))){
	        memset(demuxer, 0, sizeof(demuxer_t));
	      
		demuxer->audio= new_demuxer_stream(demuxer,audio_id);
		demuxer->video= new_demuxer_stream(demuxer,video_id);
		demuxer->sub= new_demuxer_stream(demuxer,dvdsub_id);
		
		for(int i; i < MAX_A_STREAMS;i++){
		    demuxer->a_streams[i] = NULL;
		    demuxer->v_streams[i] = NULL;
		}
		
		for(int i; i < MAX_S_STREAMS;i++){
		    demuxer->s_streams[i] = NULL;
		}
		demuxer->info=NULL;
		demuxer->chapters=NULL;
		demuxer->attachments=NULL;
#if 0 //dmesg
		set_watch((void *)&demuxer->v_streams[47]);
#endif
	       
		demuxer->desc = &demuxer_desc_isdbt; 
		demuxer->filename = strdup(filename); 
		demuxer = demuxer->desc->open(demuxer);
		return demuxer;
	    }else{
		LOGE("%s %d \n cmmd demuxer open error",__FILE__,__LINE__);
		return NULL;
	    }
	}
    }
#endif

    if ((demuxer_type =
         get_demuxer_type_from_name(demuxer_name, &demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-demuxer %s does not exist.\n",
               demuxer_name);
    }
    if ((audio_demuxer_type =
         get_demuxer_type_from_name(audio_demuxer_name,
                                    &audio_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-audio-demuxer %s does not exist.\n",
               audio_demuxer_name);
    }
    if ((sub_demuxer_type =
         get_demuxer_type_from_name(sub_demuxer_name,
                                    &sub_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-sub-demuxer %s does not exist.\n",
               sub_demuxer_name);
    }

    if (audio_stream) {
        as = open_stream(audio_stream, 0, &afmt);
        if (!as) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_CannotOpenAudioStream,
                   audio_stream);
            return NULL;
        }
        if (audio_stream_cache) {
            if (!stream_enable_cache
                (as, audio_stream_cache * 1024,
                 audio_stream_cache * 1024 * (stream_cache_min_percent /
                                              100.0),
                 audio_stream_cache * 1024 * (stream_cache_seek_min_percent /
                                              100.0))) {
                free_stream(as);
                mp_msg(MSGT_DEMUXER, MSGL_ERR,
                       "Can't enable audio stream cache\n");
                return NULL;
            }
        }
    }
    if (sub_stream) {
        ss = open_stream(sub_stream, 0, &sfmt);
        if (!ss) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_CannotOpenSubtitlesStream,
                   sub_stream);
            return NULL;
        }
    }

    vd = demux_open_stream(vs, demuxer_type ? demuxer_type : file_format,
                           demuxer_force, audio_stream ? -2 : audio_id,
                           video_id, sub_stream ? -2 : dvdsub_id, filename);

    if (!vd) {
        if (as)
            free_stream(as);
        if (ss)
            free_stream(ss);
        return NULL;
    }
    if (as) {
        ad = demux_open_stream(as,
                               audio_demuxer_type ? audio_demuxer_type : afmt,
                               audio_demuxer_force, audio_id, -2, -2,
                               audio_stream);
        if (!ad) {
            mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_OpeningAudioDemuxerFailed,
                   audio_stream);
            free_stream(as);
        } else if (ad->audio->sh
                   && ((sh_audio_t *) ad->audio->sh)->format == 0x55) // MP3
            hr_mp3_seek = 1;    // Enable high res seeking
    }
    if (ss) {
        sd = demux_open_stream(ss, sub_demuxer_type ? sub_demuxer_type : sfmt,
                               sub_demuxer_force, -2, -2, dvdsub_id,
                               sub_stream);
        if (!sd) {
            mp_msg(MSGT_DEMUXER, MSGL_WARN,
                   MSGTR_OpeningSubtitlesDemuxerFailed, sub_stream);
            free_stream(ss);
        }
    }

    if (ad && sd)
        res = new_demuxers_demuxer(vd, ad, sd);
    else if (ad)
        res = new_demuxers_demuxer(vd, ad, vd);
    else if (sd)
        res = new_demuxers_demuxer(vd, vd, sd);
    else
        res = vd;
    correct_pts = user_correct_pts;
    if (correct_pts < 0)
        correct_pts = !force_fps && demux_control(res, DEMUXER_CTRL_CORRECT_PTS, NULL)
                      == DEMUXER_CTRL_OK;

    demux_lavf_find_geodata(res);
    return res;
}

/**
 * Do necessary reinitialization after e.g. a seek.
 * Do _not_ call ds_fill_buffer between the seek and this, it breaks at least
 * seeking with ASF demuxer.
 */
static void demux_resync(demuxer_t *demuxer)
{
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;
    demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
    if (sh_video) {
        //resync_video_stream(sh_video);
    }
    if (sh_audio) {
        //resync_audio_stream(sh_audio);
    }
}

void demux_flush(demuxer_t *demuxer)
{
#if PARSE_ON_ADD
    ds_clear_parser(demuxer->video);
    ds_clear_parser(demuxer->audio);
    ds_clear_parser(demuxer->sub);
#endif
    ds_free_packs(demuxer->video);
    ds_free_packs(demuxer->audio);
    ds_free_packs(demuxer->sub);
}

int demux_seek(demuxer_t *demuxer, float rel_seek_secs, float audio_delay,
               int flags)
{
    double tmp = 0;
    double pts;
    int itime = 0;
    int KeyFrame = 0;
    if (!demuxer->seekable) {
        LOGE("demuxer %s is not seekable",demuxer->desc->name);
        
        if (demuxer->file_format == DEMUXER_TYPE_AVI)
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_CantSeekRawAVI);
#ifdef CONFIG_TV
        else if (demuxer->file_format == DEMUXER_TYPE_TV)
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_TVInputNotSeekable);
#endif
        else
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_CantSeekFile);
        return 0;
    }

    demux_flush(demuxer);

    demuxer->stream->eof = 0;
    demuxer->video->eof = 0;
    demuxer->audio->eof = 0;
    demuxer->sub->eof = 0;

    //#define SEEK_ABSOLUTE (1 << 0)
    //#define SEEK_FACTOR   (1 << 1)
    if (flags & SEEK_ABSOLUTE)
        pts = 0.0f;
    else {
        if (demuxer->stream_pts == MP_NOPTS_VALUE)
            goto dmx_seek;
        pts = demuxer->stream_pts;
    }

    if (flags & SEEK_FACTOR) {
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &tmp)
            == STREAM_UNSUPPORTED)
            goto dmx_seek;
        pts += tmp * rel_seek_secs;
    } else
        pts += rel_seek_secs;

    if (stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &pts) !=
        STREAM_UNSUPPORTED) {
        demux_resync(demuxer);
        return 1;
    }

  dmx_seek:
    if (demuxer->desc->seek){
      demuxer->desc->seek(demuxer, rel_seek_secs, audio_delay, flags);
    }
    //double tmp1_audio_pts = ds_get_next_pts(demuxer->audio);
    demuxer->video->seek_flag = 1;
    demuxer->audio->seek_flag = 1;
    demux_resync(demuxer);

#if 1
    if(demuxer->video && demuxer->video->sh && demuxer->audio && demuxer->audio->sh && !(demuxer->seekable&2))
    {
        double tmp_video_pts = ds_get_next_pts(demuxer->video);
	double tmp_audio_pts = ds_get_next_pts(demuxer->audio);
	if(tmp_video_pts == MP_NOPTS_VALUE || tmp_audio_pts == MP_NOPTS_VALUE){
	  tmp_video_pts = demuxer->video->pts;
	  tmp_audio_pts = demuxer->audio->pts;
	}
	//LOGE("tmp_video_pts = %f, tmp_audio_pts = %f next_video_pts = %f next_audio_pts = %f", tmp_video_pts, tmp_audio_pts,  (double)ds_get_next_pts(demuxer->video),(double)ds_get_next_pts(demuxer->audio));
	//FIXME: video_pts is relative time, audio_pts is absolute time!!!
#if 1
	while((tmp_audio_pts - tmp_video_pts)>0.4)
	  {
	    //LOGE("b tmp_audio_pts =%f tmp_video_pts = %f",tmp_audio_pts,tmp_video_pts);
	    /* dp->flags no set keyframe flags*/
	    if((demuxer->type == 7)  || /*demux_mov*/
	       (demuxer->type == 35) || /*demux_lavf FIXME*/
	       (demuxer->type == 1)  || /*demux_mpg*/
	       (demuxer->type == 2)  ||
	       (demuxer->type == 27) ||
	       (demuxer->type == 41) ||
	       (demuxer->type == 42) ||
	       (demuxer->type == 44) ||
	       (demuxer->type == 30)) break;

	    if(demuxer->type == 6)
	      KeyFrame = 0x01;
	    else
	      KeyFrame = 0x10;

	    while(1){
	      unsigned char *start;
	      ds_get_packet(demuxer->video, &start);
	      demuxer->video->need_free ++;
	      /*keyframe*/
	      if(ds_get_next_flags(demuxer->video) == KeyFrame){
		tmp_video_pts = demuxer->video->pts;
		break;
	      }
	      else
		if(demuxer->video->eof)
		  {
		    return 0;
		  }
	    }
	  }
#endif

#if 1
	while(tmp_audio_pts < tmp_video_pts - 0.1)
	{
	    //LOGE("audio_pts=%f, video_pts=%f",tmp_audio_pts,tmp_video_pts);
	    unsigned char *start;
	    ds_get_packet(demuxer->audio, &start);
	    if(demuxer->audio->eof)
	      {
		break;
	      }
	    if(((sh_audio_t*)(demuxer->audio->sh))->i_bps)
	      tmp_audio_pts=demuxer->audio->pts+(double)demuxer->audio->pts_bytes/((sh_audio_t*)(demuxer->audio->sh))->i_bps;
	    else
	      tmp_audio_pts=demuxer->audio->pts;
	    //EL("audio_pts=%f, video_pts=%f",tmp_audio_pts,tmp_video_pts);
	}
#endif
	//if(tmp_video_pts == MP_NOPTS_VALUE)
	//  tmp_audio_pts = tmp_video_pts = demuxer_get_time_length(demuxer);
	//LOGE("a tmp_audio_pts =%f tmp_video_pts = %f",tmp_audio_pts,tmp_video_pts);
	//video_pts=(double)(tmp_video_pts * 1000000.0);
	//audio_pts=(double)(tmp_audio_pts * 1000000.0);
	//return video_pts / 1000;
    }
#endif
    return 1;
}

int demux_info_add(demuxer_t *demuxer, const char *opt, const char *param)
{
  char **info = demuxer->info;
  int n = 0;


  for (n = 0; info && info[2 * n] != NULL; n++) {
    if (!strcasecmp(opt, info[2 * n])) {
      if (!strcmp(param, info[2 * n + 1])) {
	mp_msg(MSGT_DEMUX, MSGL_V, "Demuxer info %s set to unchanged value %s\n", opt, param);
	return 0;
      }
      mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_DemuxerInfoChanged, opt,
	     param);
      free(info[2 * n + 1]);
      info[2 * n + 1] = strdup(param);
      return 0;
    }
  }

  info = demuxer->info = realloc(info, (2 * (n + 2)) * sizeof(char *));
  info[2 * n] = strdup(opt);
  info[2 * n + 1] = strdup(param);
  memset(&info[2 * (n + 1)], 0, 2 * sizeof(char *));

  return 1;
}

int demux_info_print(demuxer_t *demuxer)
{
  char **info = demuxer->info;
  int n;

  if (!info)
    return 0;

  mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_ClipInfo);
  for (n = 0; info[2 * n] != NULL; n++) {
    mp_msg(MSGT_DEMUX, MSGL_INFO, " %s: %s\n", info[2 * n],
	   info[2 * n + 1]);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_NAME%d=%s\n", n,
	   info[2 * n]);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_VALUE%d=%s\n", n,
	   info[2 * n + 1]);
  }
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_N=%d\n", n);

  return 0;
}

char *demux_info_get(demuxer_t *demuxer, const char *opt)
{
  int i;
  char **info = demuxer->info;

  for (i = 0; info && info[2 * i] != NULL; i++) {
    if (!strcasecmp(opt, info[2 * i]))
      return info[2 * i + 1];
  }

  return NULL;
}

int demux_control(demuxer_t *demuxer, int cmd, void *arg)
{

  if (demuxer->desc->control)
    return demuxer->desc->control(demuxer, cmd, arg);

  return DEMUXER_CTRL_NOTIMPL;
}



double demuxer_get_time_length(demuxer_t *demuxer)
{
  double get_time_ans;
  sh_video_t *sh_video = demuxer->video->sh;
  sh_audio_t *sh_audio = demuxer->audio->sh;
  // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
  if (demux_control
      (demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, (void *) &get_time_ans) <= 0) {
    if (sh_video && sh_video->i_bps && sh_audio && sh_audio->i_bps)
      get_time_ans = (double) (demuxer->movi_end -
			       demuxer->movi_start) / (sh_video->i_bps +
						       sh_audio->i_bps);
    else if (sh_video && sh_video->i_bps)
      get_time_ans = (double) (demuxer->movi_end -
			       demuxer->movi_start) / sh_video->i_bps;
    else if (sh_audio && sh_audio->i_bps)
      get_time_ans = (double) (demuxer->movi_end -
			       demuxer->movi_start) / sh_audio->i_bps;
    else
      get_time_ans = 0;
  }
  return get_time_ans;
}

/**
 * \brief demuxer_get_current_time() returns the time of the current play in three possible ways:
 *        either when the stream reader satisfies STREAM_CTRL_GET_CURRENT_TIME (e.g. dvd)
 *        or using sh_video->pts when the former method fails
 *        0 otherwise
 * \return the current play time
 */
int demuxer_get_current_time(demuxer_t *demuxer)
{
  double get_time_ans = 0;
  sh_video_t *sh_video = demuxer->video->sh;
  if (demuxer->stream_pts != MP_NOPTS_VALUE)
    get_time_ans = demuxer->stream_pts;
  else if (sh_video)
    get_time_ans = sh_video->pts;
  return (int) get_time_ans;
}

int demuxer_get_percent_pos(demuxer_t *demuxer)
{
  int ans = 0;
  int res = demux_control(demuxer, DEMUXER_CTRL_GET_PERCENT_POS, &ans);
  int len = (demuxer->movi_end - demuxer->movi_start) / 100;
  if (res <= 0) {
    loff_t pos = demuxer->filepos > 0 ? demuxer->filepos : stream_tell(demuxer->stream);
    if (len > 0)
      ans = (pos - demuxer->movi_start) / len;
    else
      ans = 0;
  }
  if (ans < 0)
    ans = 0;
  if (ans > 100)
    ans = 100;
  return ans;
}

int demuxer_switch_audio(demuxer_t *demuxer, int index)
{
  int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_AUDIO, &index);
  if (res == DEMUXER_CTRL_NOTIMPL)
    index = demuxer->audio->id;
  if (demuxer->audio->id >= 0)
    demuxer->audio->sh = demuxer->a_streams[demuxer->audio->id];
  else
    demuxer->audio->sh = NULL;
  return index;
}

int demuxer_switch_video(demuxer_t *demuxer, int index)
{
  int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_VIDEO, &index);
  if (res == DEMUXER_CTRL_NOTIMPL)
    index = demuxer->video->id;
  if (demuxer->video->id >= 0)
    demuxer->video->sh = demuxer->v_streams[demuxer->video->id];
  else
    demuxer->video->sh = NULL;
  return index;
}

int demuxer_add_attachment(demuxer_t *demuxer, const char *name,
                           const char *type, const void *data, size_t size)
{
  if (!(demuxer->num_attachments & 31))
    demuxer->attachments = realloc(demuxer->attachments,
				   (demuxer->num_attachments + 32) * sizeof(demux_attachment_t));

  demuxer->attachments[demuxer->num_attachments].name = strdup(name);
  demuxer->attachments[demuxer->num_attachments].type = strdup(type);
  demuxer->attachments[demuxer->num_attachments].data = malloc(size);
  memcpy(demuxer->attachments[demuxer->num_attachments].data, data, size);
  demuxer->attachments[demuxer->num_attachments].data_size = size;

  return demuxer->num_attachments++;
}

int demuxer_add_chapter(demuxer_t *demuxer, const char *name, uint64_t start,
                        uint64_t end)
{
  if (demuxer->chapters == NULL)
    demuxer->chapters = malloc(32 * sizeof(*demuxer->chapters));
  else if (!(demuxer->num_chapters % 32))
    demuxer->chapters = realloc(demuxer->chapters,
				(demuxer->num_chapters + 32) *
				sizeof(*demuxer->chapters));

  demuxer->chapters[demuxer->num_chapters].start = start;
  demuxer->chapters[demuxer->num_chapters].end = end;
  demuxer->chapters[demuxer->num_chapters].name = strdup(name ? name : MSGTR_Unknown);

  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_ID=%d\n", demuxer->num_chapters);
  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_START=%"PRIu64"\n", demuxer->num_chapters, start);
  if (end)
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_END=%"PRIu64"\n", demuxer->num_chapters, end);
  if (name)
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_NAME=%s\n", demuxer->num_chapters, name);

  return demuxer->num_chapters++;
}

/**
 * \brief demuxer_seek_chapter() seeks to a chapter in two possible ways:
 *        either using the demuxer->chapters structure set by the demuxer
 *        or asking help to the stream layer (e.g. dvd)
 * \param chapter - chapter number wished - 0-based
 * \param mode 0: relative to current main pts, 1: absolute
 * \param seek_pts set by the function to the pts to seek to (if demuxer->chapters is set)
 * \param num_chapters number of chapters present (set by this function is param is not null)
 * \param chapter_name name of chapter found (set by this function is param is not null)
 * \return -1 on error, current chapter if successful
 */

int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, int mode,
                         float *seek_pts, int *num_chapters,
                         char **chapter_name)
{
  int ris;
  int current, total;

  if (!demuxer->num_chapters || !demuxer->chapters) {
    if (!mode) {
      ris = stream_control(demuxer->stream,
			   STREAM_CTRL_GET_CURRENT_CHAPTER, &current);
      if (ris == STREAM_UNSUPPORTED)
	return -1;
      chapter += current;
    }

    demux_flush(demuxer);

    ris = stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_CHAPTER,
			 &chapter);

    demux_resync(demuxer);

    // exit status may be ok, but main() doesn't have to seek itself
    // (because e.g. dvds depend on sectors, not on pts)
    *seek_pts = -1.0;

    if (num_chapters) {
      if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
			 num_chapters) == STREAM_UNSUPPORTED)
	*num_chapters = 0;
    }

    if (chapter_name) {
      *chapter_name = NULL;
      if (num_chapters && *num_chapters) {
	char *tmp = malloc(16);
	if (tmp) {
	  snprintf(tmp, 16, " of %3d", *num_chapters);
	  *chapter_name = tmp;
	}
      }
    }

    return ris != STREAM_UNSUPPORTED ? chapter : -1;
  } else {  // chapters structure is set in the demuxer
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;

    total = demuxer->num_chapters;

    if (mode == 1)  //absolute seeking
      current = chapter;
    else {          //relative seeking
      uint64_t now;
      now = (sh_video ? sh_video->pts : (sh_audio ? sh_audio->pts : 0.))
	* 1000 + .5;

      for (current = total - 1; current >= 0; --current) {
	demux_chapter_t *chapter = demuxer->chapters + current;
	if (chapter->start <= now)
	  break;
      }
      current += chapter;
    }

    if (current >= total)
      return -1;
    if (current < 0)
      current = 0;

    *seek_pts = demuxer->chapters[current].start / 1000.0;

    if (num_chapters)
      *num_chapters = demuxer->num_chapters;

    if (chapter_name) {
      if (demuxer->chapters[current].name)
	*chapter_name = strdup(demuxer->chapters[current].name);
      else
	*chapter_name = NULL;
    }

    return current;
  }
}

int demuxer_get_current_chapter(demuxer_t *demuxer)
{
  int chapter = -1;
  if (!demuxer->num_chapters || !demuxer->chapters) {
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_CHAPTER,
		       &chapter) == STREAM_UNSUPPORTED)
      chapter = -1;
  } else {
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;
    uint64_t now;
    now = (sh_video ? sh_video->pts : (sh_audio ? sh_audio->pts : 0))
      * 1000 + 0.5;
    for (chapter = demuxer->num_chapters - 1; chapter >= 0; --chapter) {
      if (demuxer->chapters[chapter].start <= now)
	break;
    }
  }
  return chapter;
}

char *demuxer_chapter_name(demuxer_t *demuxer, int chapter)
{
  if (demuxer->num_chapters && demuxer->chapters) {
    if (chapter >= 0 && chapter < demuxer->num_chapters
	&& demuxer->chapters[chapter].name)
      return strdup(demuxer->chapters[chapter].name);
  }
  return NULL;
}

char *demuxer_chapter_display_name(demuxer_t *demuxer, int chapter)
{
  char *chapter_name = demuxer_chapter_name(demuxer, chapter);
  if (chapter_name) {
    char *tmp = malloc(strlen(chapter_name) + 14);
    snprintf(tmp, 63, "(%d) %s", chapter + 1, chapter_name);
    free(chapter_name);
    return tmp;
  } else {
    int chapter_num = demuxer_chapter_count(demuxer);
    char tmp[30];
    if (chapter_num <= 0)
      snprintf(tmp, 30, "(%d)", chapter + 1);
    else
      snprintf(tmp, 30, "(%d) of %d", chapter + 1, chapter_num);
    return strdup(tmp);
  }
}

float demuxer_chapter_time(demuxer_t *demuxer, int chapter, float *end)
{
  if (demuxer->num_chapters && demuxer->chapters && chapter >= 0
      && chapter < demuxer->num_chapters) {
    if (end)
      *end = demuxer->chapters[chapter].end / 1000.0;
    return demuxer->chapters[chapter].start / 1000.0;
  }
  return -1.0;
}

int demuxer_chapter_count(demuxer_t *demuxer)
{
  if (!demuxer->num_chapters || !demuxer->chapters) {
    int num_chapters = 0;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
		       &num_chapters) == STREAM_UNSUPPORTED)
      num_chapters = 0;
    return num_chapters;
  } else
    return demuxer->num_chapters;
}

int demuxer_angles_count(demuxer_t *demuxer)
{
  int ris, angles = -1;

  ris = stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_ANGLES, &angles);
  if (ris == STREAM_UNSUPPORTED)
    return -1;
  return angles;
}

int demuxer_get_current_angle(demuxer_t *demuxer)
{
  int ris, curr_angle = -1;
  ris = stream_control(demuxer->stream, STREAM_CTRL_GET_ANGLE, &curr_angle);
  if (ris == STREAM_UNSUPPORTED)
    return -1;
  return curr_angle;
}


int demuxer_set_angle(demuxer_t *demuxer, int angle)
{
  int ris, angles = -1;

  angles = demuxer_angles_count(demuxer);
  if ((angles < 1) || (angle > angles))
    return -1;

  demux_flush(demuxer);

  ris = stream_control(demuxer->stream, STREAM_CTRL_SET_ANGLE, &angle);
  if (ris == STREAM_UNSUPPORTED)
    return -1;

  demux_resync(demuxer);

  return angle;
}

int demuxer_audio_track_by_lang(demuxer_t *d, char *lang)
{
  int i, len;
  lang += strspn(lang, ",");
  while ((len = strcspn(lang, ",")) > 0) {
    for (i = 0; i < MAX_A_STREAMS; ++i) {
      sh_audio_t *sh = d->a_streams[i];
      if (sh && sh->lang && strncmp(sh->lang, lang, len) == 0)
	return sh->aid;
    }
    lang += len;
    lang += strspn(lang, ",");
  }
  return -1;
}

int demuxer_sub_track_by_lang(demuxer_t *d, char *lang)
{
  int i, len;
  lang += strspn(lang, ",");
  while ((len = strcspn(lang, ",")) > 0) {
    for (i = 0; i < MAX_S_STREAMS; ++i) {
      sh_sub_t *sh = d->s_streams[i];
      if (sh && sh->lang && strncmp(sh->lang, lang, len) == 0)
	return sh->sid;
    }
    lang += len;
    lang += strspn(lang, ",");
  }
  return -1;
}

int demuxer_default_audio_track(demuxer_t *d)
{
  int i;
  for (i = 0; i < MAX_A_STREAMS; ++i) {
    sh_audio_t *sh = d->a_streams[i];
    if (sh && sh->default_track)
      return sh->aid;
  }
  for (i = 0; i < MAX_A_STREAMS; ++i) {
    sh_audio_t *sh = d->a_streams[i];
    if (sh)
      return sh->aid;
  }
  return -1;
}

int demuxer_default_sub_track(demuxer_t *d)
{
  int i;
  for (i = 0; i < MAX_S_STREAMS; ++i) {
    sh_sub_t *sh = d->s_streams[i];
    if (sh && sh->default_track)
      return sh->sid;
  }
  return -1;
}

int ds_get_packet_len(demux_stream_t *ds)
{
  if(ds->buffer_pos>=ds->buffer_size){
    if(!ds_fill_buffer(ds)){
      return -1;
    }
  }
  return (ds->buffer_size-ds->buffer_pos);      
}
