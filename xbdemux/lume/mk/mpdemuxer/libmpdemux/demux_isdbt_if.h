#ifndef DEMUX_ISDBT_IF_H
#define DEMUX_ISDBT_IF_H

#include <stdlib.h>

#if 0
extern "C" {
#ifndef STREAM_H
#include "stream/stream.h"
#endif
#ifndef DEMUXER_H
#include "demuxer.h"
#endif
#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#elif defined(USE_LIBAVCODEC)
#include "libavcodec/avcodec.h"
#endif
}
#endif

#define DEF_ISDBT_AACV2		1	/* 1 FOR Brazil AAC V2 , 0 For JAPAN AAC */

// Codec-specific initialization routines:
void isdbt_get_audio_data_buffer(void* pmem);
void isdbt_get_video_data_buffer(void* pmem);

#endif
