#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "stream/stream.h"
#include "demuxer.h"
#include "parse_es.h"
#include "stheader.h"
#include "ms_hdr.h"
#include "mpeg_hdr.h"

#include <utils/Log.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <poll.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>

#include "demux_isdbt_if.h"
//#define DEF_ISDBT_AACV2		1	/* 1 FOR Brazil AAC V2 */

static unsigned int mtv_pos=0;
static double cur_ts=0;

unsigned char *audio_buf;
unsigned char *video_buf;
unsigned int  audio_len=0,video_len=0;
double ts_a,ts_v;

demuxer_t *dsdemuxer;
static int last_type; 

typedef enum
{
	UNKNOWN		= -1,
	VIDEO_MPEG1 	= 0x10000001,
	VIDEO_MPEG2 	= 0x10000002,
	VIDEO_MPEG4 	= 0x10000004,
	VIDEO_H264 	= 0x10000005,
	VIDEO_AVC	= mmioFOURCC('a', 'v', 'c', '1'),
	VIDEO_VC1	= mmioFOURCC('W', 'V', 'C', '1'),
	AUDIO_MP2   	= 0x50,
	AUDIO_A52   	= 0x2000,
	AUDIO_DTS	= 0x2001,
	AUDIO_LPCM_BE  	= 0x10001,
	AUDIO_AAC	= mmioFOURCC('M', 'P', '4', 'A'),
	SPU_DVD		= 0x3000000,
	SPU_DVB		= 0x3000001,
	PES_PRIVATE1	= 0xBD00000,
	SL_PES_STREAM	= 0xD000000,
	SL_SECTION	= 0xD100000,
	MP4_OD		= 0xD200000,
} es_stream_type_t;

void set_watch(void *addr);

void add_video_packet(double ts,unsigned char *p,int len)
{
     //LOGE("v pts %f len:%d %x\n",ts,len,p);

	 demux_packet_t* dp;
	 dp = new_demux_packet(len);
	 //	 cur_ts=ts;

	 if(dp)
	 {
	      memcpy(dp->buffer, p, len);
	      mtv_pos +=len;
      	      dp->pts = ts;
	      dp->pos = mtv_pos;
	      dp->len = len;
	      dp->flags = 0;
	      ds_add_packet(dsdemuxer->video, dp);
	 }

}



void add_audio_packet(double ts,unsigned char *p,int len)
{
  	//LOGE("a pts %f len:%d %x\n",ts,len,p);

	 demux_packet_t* dp;
	 dp = new_demux_packet(len);
	 cur_ts=ts;
	 //   	LOGE("end a pts %f len:%d %x\n",ts,len,p);
	 if(dp)
	 {
	      memcpy(dp->buffer, p, len);
	      mtv_pos +=len;
	      dp->pts = ts;
	      dp->pos = mtv_pos;
	      dp->len = len;
	      dp->flags = 0;
	      ds_add_packet(dsdemuxer->audio, dp);
	 }
	 //LOGE("AUD pts %f len:%d \n",ts,len);
}

static int demux_isdbt_fill_buffer_data(demuxer_t * demuxer, demux_stream_t *ds);


//***************************************************//
//
// demuxer functions:
//
//***************************************************//


static int GetIntValue(const char* URI, const char* str)
{
    const char* c;
    int val;
    static const char intformatter[] = "%d";
    char formatter[16]="%d";
    c = strstr(URI, str);
    //    LOGE("URI:%s str:%s c:%s \n",URI,str,c);
    if (c == NULL)
        return -1;

    else if (*(c+strlen(str)) != '=')
        return -1;
    else
    {
        //printf("%d\%d", len );
      snprintf(formatter, sizeof(formatter),"%s=%s:", str,intformatter);
      /// LOGE("formatter %s",formatter);
        sscanf(c,formatter, &val);
	//	LOGE("val %d\n",val);
        return val;
    }
}

WAVEFORMATEX wf;
int mFreq,mChannel;
char	isAudioTypeAACV2;	//BTL , Differentiate AAC and AACV2
#if 0
static demuxer_t *demux_isdbt_open(demuxer_t * demuxer)
{
	sh_video_t *shv;
	sh_audio_t *sha;
	char* filename;
	int 	ret;

	LOGE("demux_isdbt_open in\n");
	//	char audio_context[20] = {0x2b,0x10,0x88,0x00};
	//	char audio_context[20] = {0x11,0x90,};
	/* Fix me lator BTLSYS */
	char audio_context[20] = {0x13,0x10,0x88,0x00};	//48k
	//char audio_context[20] = {0x16,0x10,0x88,0x00};	//24k
	//char audio_context[20] = {0xeb,0x09,0x88,0x00};	//Audio object type :AAC LC(Low complexity)

	//uint8_t vcodecinfo[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x0d, 0x96, 0x52, 0x02, 0x83, 0xf4, 0x90, 0x08, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80};
	uint8_t vcodecinfo[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x0d, 0x99, 0xa0, 0x68, 0x7e, 0xc0, 0x5a, 0x83, 0x03, 0x03, 0x78, 0x00, 0x00, 0x1f, 0x48, 0x00, 0x07, 0x53, 0x04, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80};
	//	demuxer->video->id =-2;
	//	LOGE("DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
	//	   demuxer->audio->id, demuxer->video->id, demuxer->sub->id);

	mp_msg(MSGT_DEMUX, MSGL_V, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		   demuxer->audio->id, demuxer->video->id, demuxer->sub->id);
	demuxer->type= DEMUXER_TYPE_ISDBT;
	demuxer->file_format= DEMUXER_TYPE_ISDBT;
	//	stream_reset(demuxer->stream);
	filename=demuxer->filename;
	if( strncmp(filename, "mtv:", strlen("mtv:") ) == 0 )
	  LOGE("file name is :%s \n",filename);
	LOGE("demux_isdbt_open in111 %s\n",demuxer->filename);

	mFreq = 0;
	mFreq = GetIntValue( filename, "freq" ); 
	if( mFreq <= 0 )
	  LOGE("error get freq!\n");
	else
	  LOGE("get freq is : %d\n",mFreq );

	mChannel = 0;
	mChannel =GetIntValue(filename, "channel");
	if( mChannel < 0 )
	  LOGE("error get channel num!\n");
	else
	  LOGE("get channel num is : %d\n",mChannel);


	demuxer->seekable = 0;
#if 1
	LOGE("demux_isdbt_open in222\n");
	sha = new_sh_audio_aid(demuxer, 1, 64,NULL);
	LOGE("demux_isdbt_open in333\n");
	if(sha)
	{
	      sha->format = AUDIO_AAC;
	      sha->ds = demuxer->audio;
	      sha->samplerate = 24000;
		  //sha->samplerate = 48000;	
	      sha->samplesize = 2;
	      sha->channels = 2;
#if 1
	      sha->wf = (WAVEFORMATEX *) malloc(sizeof (WAVEFORMATEX) + 4);
	      sha->wf->cbSize = 4;
	      sha->wf->nChannels = 2;
	      memcpy(sha->wf + 1, audio_context, 4);
#else

	      sha->wf = (WAVEFORMATEX *) malloc(sizeof (WAVEFORMATEX) + 2);
	      sha->wf->cbSize = 2;
	      memcpy(sha->wf + 1, audio_context, 2);
#endif

	      demuxer->audio->id = 64;
	      sha->ds = demuxer->audio;
	      demuxer->audio->sh = sha;
	}
#endif
#if 1
	shv = new_sh_video_vid(demuxer, 0, 80);
	if(shv)
	{

	      shv->format = VIDEO_AVC;
		  //shv->format = VIDEO_H264;	
	      shv->ds = demuxer->video;
	      shv->bih = (BITMAPINFOHEADER *) calloc(1, sizeof(BITMAPINFOHEADER) + 25);
	      shv->bih->biSize= sizeof(BITMAPINFOHEADER);

		  // BTL , 110308 --> no meaning
#if 0
		  shv->fps=24; // we probably won't even care about fps
		  shv->frametime = 1.0f/shv->fps;
#endif
	      shv->bih->biWidth = 320;		// no video display
		  //shv->bih->biWidth = 416;	
	      //shv->bih->biHeight = 240;
		  shv->bih->biHeight = 180;
	
	      demuxer->video->id = 80;
	      shv->ds = demuxer->video;
	      demuxer->video->sh = shv;
	}
#endif
	demuxer->movi_start = 0;

	/* 5 th try */
	for (int i=0 ; i < 5 ; i++)
	{
		ret = isdbt_tv_set_channel(mFreq,mChannel);
		LOGE("demux_isdbt_open() ret %d", ret);
		if (ret == 0)
			break;
	}
	if (ret != 0)
		return NULL;
	//isdbt_get_video_data_buffer(demuxer->video);
#if 0 //dmesg
	set_watch((void *)&demuxer->v_streams[47]);
#endif

	//	LOGE("demux_isdbt_open out");
	return demuxer;
}
#endif
static demuxer_t *demux_isdbt_open(demuxer_t * demuxer)
{
	sh_video_t *shv;
	sh_audio_t *sha;
	char* filename;
	int 	ret;
	int 	resolution, samplefreq;

	LOGE("demux_isdbt_open in\n");
	//	char audio_context[20] = {0x2b,0x10,0x88,0x00};
	//	char audio_context[20] = {0x11,0x90,};
	/* Fix me lator BTLSYS */
//#if DEF_ISDBT_AACV2	/* Brazil */
//	LOGE("[ISDBT_DEMUX] OPEN in AACv2 Brazil version\n");
//	char audio_context[4] = {0xeb,0x09,0x88,0x00};	//Audio object type :AAC LC(Low complexity)
//#else
//	LOGE("[ISDBT_DEMUX] OPEN in AAC Japan version\n");
//	char audio_context[4] = {0x13,0x10,0x88,0x00};
//#endif
	char audio_AACV2_context[4] = {0xeb,0x09,0x88,0x00};
	char audio_AAC_context[4] = {0x13,0x10,0x88,0x00};
	//char audio_context[20] = {0x16,0x10,0x88,0x00};
	//char audio_context[20] = {0xeb,0x09,0x88,0x00};	//Audio object type :AAC LC(Low complexity)

	//uint8_t vcodecinfo[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x0d, 0x96, 0x52, 0x02, 0x83, 0xf4, 0x90, 0x08, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80};
	uint8_t vcodecinfo[] = {0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x0d, 0x99, 0xa0, 0x68, 0x7e, 0xc0, 0x5a, 0x83, 0x03, 0x03, 0x78, 0x00, 0x00, 0x1f, 0x48, 0x00, 0x07, 0x53, 0x04, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80};
	//	demuxer->video->id =-2;
	//	LOGE("DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
	//	   demuxer->audio->id, demuxer->video->id, demuxer->sub->id);

	mp_msg(MSGT_DEMUX, MSGL_V, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		   demuxer->audio->id, demuxer->video->id, demuxer->sub->id);
	demuxer->type= DEMUXER_TYPE_ISDBT;
	demuxer->file_format= DEMUXER_TYPE_ISDBT;
	//	stream_reset(demuxer->stream);
	filename=demuxer->filename;
	if( strncmp(filename, "mtv:", strlen("mtv:") ) == 0 )
	  LOGE("file name is :%s \n",filename);
	LOGE("demux_isdbt_open in111 %s\n",demuxer->filename);

	mFreq = 0;
	mFreq = GetIntValue( filename, "freq" ); 
	if( mFreq <= 0 )
	  LOGE("error get freq!\n");
	else
	  LOGE("get freq is : %d\n",mFreq );

	mChannel = 0;
	mChannel =GetIntValue(filename, "channel");
	if( mChannel < 0 )
	  LOGE("error get channel num!\n");
	else
	  LOGE("get channel num is : %d\n",mChannel);


	demuxer->seekable = 0;

	/* 5 th try */
	for (int i=0 ; i < 5 ; i++)
	{
		ret = isdbt_tv_set_channel(mFreq,mChannel);
		LOGE("demux_isdbt_open() ret %d", ret);
		if (ret == 0)
			break;
	}

	if (ret != 0)
		return NULL;

	resolution = samplefreq = 0;
	isdbt_getStreamInfo(&resolution, &samplefreq);

	if (samplefreq < 10)
	{
		isAudioTypeAACV2 = 0;	//AAC
	}
	else  
	{
		isAudioTypeAACV2 = 1;	//AACV2
		samplefreq -= 10;
	}

	if (samplefreq < 0 || resolution > 7)
		samplefreq = 6;

	if (resolution != 0 && resolution != 1)
		resolution = 0;

	if (isAudioTypeAACV2)
		LOGE("isdbt_getStreamInfo() AACV2 resolution %d , samplefreq %d", resolution , samplefreq);
	else
		LOGE("isdbt_getStreamInfo() AAC resolution %d , samplefreq %d", resolution , samplefreq);		

#if 1
	sha = new_sh_audio_aid(demuxer, 1, 64,NULL);

	if(sha)
	{
	      sha->format = AUDIO_AAC;
	      sha->ds = demuxer->audio;

			if (samplefreq == 3) 
				sha->samplerate = 48000;
			else if (samplefreq == 4) 
				sha->samplerate = 44100;
			else if (samplefreq == 5) 
				sha->samplerate = 32000;
			else if (samplefreq == 6) 
				sha->samplerate = 24000;
			else if (samplefreq == 8) 
				sha->samplerate = 16000;
			else
			    sha->samplerate = 24000;

		  LOGE("[Demux Open] Audio SampleFreq %d\n",sha->samplerate);

		  //sha->samplerate = 44100;	//testing
		/** In case Japan ISDBT AAC , Not setting is more stable 
               	 **/
		if (isAudioTypeAACV2)
		{
		      sha->samplesize = 2;
		      sha->channels = 2;
	
		      //sha->i_bps = 96*1000/8;		//120326 Audio bps 96k
		
		  //sha->channels = 0;			//testing 
		      sha->wf = (WAVEFORMATEX *) malloc(sizeof (WAVEFORMATEX) + 4);
		      sha->wf->cbSize = 4;
		      sha->wf->nChannels = 2;
		  //sha->wf->nChannels = 0;		//testing 
	
//		  if (isAudioTypeAACV2)
//		      memcpy(sha->wf + 1, audio_AACV2_context, 4);
//		  else
		      memcpy(sha->wf + 1, audio_AAC_context, 4);			
		}
		
	      	demuxer->audio->id = 64;
	      	sha->ds = demuxer->audio;
	      	demuxer->audio->sh = sha;
	}
#endif
#if 1
	shv = new_sh_video_vid(demuxer, 0, 80);
	if(shv)
	{

	      shv->format = VIDEO_AVC;
		  //shv->format = VIDEO_H264;	
	      shv->ds = demuxer->video;
	      shv->bih = (BITMAPINFOHEADER *) calloc(1, sizeof(BITMAPINFOHEADER) + 25);
	      shv->bih->biSize= sizeof(BITMAPINFOHEADER);

		  // BTL , 110308 --> no meaning
#if 0
		  shv->fps=24; // we probably won't even care about fps
		  shv->frametime = 1.0f/shv->fps;
#endif
	      shv->bih->biWidth = 320;		// no video display
		  //shv->bih->biWidth = 416;	
	      //shv->bih->biHeight = 240;
		  if (resolution == 1)
		  	shv->bih->biHeight = 240;
		  else
			shv->bih->biHeight = 180;

		  LOGE("[Demux Open] Video Resolution (320 x %d)\n",shv->bih->biHeight);
	      demuxer->video->id = 80;
	      shv->ds = demuxer->video;
	      demuxer->video->sh = shv;
	}
#endif
	demuxer->movi_start = 0;


#if 0	//AAC channel config = 0 problem patch testing 
    if(!sha->codecdata_len){
      sha->a_in_buffer_size=6144;
      sha->a_in_buffer = av_mallocz(sha->a_in_buffer_size);
      //sha->a_in_buffer_len = demux_read_data(sha->ds, sha->a_in_buffer, sha->a_in_buffer_size);
	  sha->a_in_buffer_len = isdbt_get_audio_first_frame(sha->a_in_buffer, sha->a_in_buffer_size);
      //demux_seek(demuxer,demuxer->movi_start,0,SEEK_ABSOLUTE);
    }
#endif
	return demuxer;
}

static void demux_isdbt_close(demuxer_t * demuxer)
{
	uint16_t i;
	
	LOGE("111111111111111111111demux_isdbt_close in !!!!!!!!!!!!!!!!!!!!\n");	
	isdbt_tv_clear_channel(mFreq,mChannel);
	if(demuxer->priv)
	{
	  free(demuxer->priv);
	}
	demuxer->priv=NULL;
	LOGE("111111111111111111111demux_isdbt_close out!!!!!!!!!!!!!!!!!!!!\n");


}
 
static void demux_isdbt_seek(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
        demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	demux_stream_t *d_sub=demuxer->sub;
	sh_audio_t *sh_audio=d_audio->sh;
	sh_video_t *sh_video=d_video->sh;
	//================= seek in isdbt ==========================
//	ts_dump_streams(demuxer->priv);
//	reset_fifos(priv, sh_audio != NULL, sh_video != NULL, demuxer->sub->id > 0);
	demuxer->stream_pts=cur_ts;
       	LOGE("seek from demux!--------------------\n"); 
  	if(sh_audio != NULL)
		ds_free_packs(d_audio);
	if(sh_video != NULL)
		ds_free_packs(d_video);
	if(demuxer->sub->id > 0)
		ds_free_packs(d_sub);
}

////////////////////////////////////
static int demux_isdbt_fill_buffer_data(demuxer_t * demuxer, demux_stream_t *ds)
{
        demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	demux_stream_t *d_sub=demuxer->sub;
//	sh_audio_t *sh_audio=d_audio->sh;
//	sh_video_t *sh_video=d_video->sh;

	 dsdemuxer =  demuxer;

  if(ds==demuxer->audio)
  {
    //    LOGD("need audio!\n");
		if (isAudioTypeAACV2)	
	       return isdbt_get_audio_pmemory(d_audio , 1);
		else
	       return isdbt_get_audio_pmemory(d_audio , 0);
    //	  isdbt_get_audio_data_buffer(d_audio);
  }

  if(ds==demuxer->video)
  {
    //    LOGD("need Video!\n");
       return isdbt_get_video_pmemory(d_audio);
    //	  isdbt_get_video_data_buffer(d_video);
  }
	return 1;  

}

static int check_isdbt_file(demuxer_t *demuxer)
{
	stream_t *s = demuxer->stream;
	demuxer->priv = NULL;
	unsigned int * id = s->priv;
	//	LOGE("check_isdbt_file!\n");
	return DEMUXER_TYPE_ISDBT;
}

static int demux_isdbt_control(demuxer_t *demuxer, int cmd, void *arg)
{
  //        LOGE("demux_isdbt_control get length in!\n");
	switch(cmd)
	{
	case DEMUXER_CTRL_GET_TIME_LENGTH:
	  *((double *)arg)=(double)10000000;
	  return DEMUXER_CTRL_OK;
	
	case DEMUXER_CTRL_SWITCH_AUDIO:
	case DEMUXER_CTRL_SWITCH_VIDEO:
	case DEMUXER_CTRL_IDENTIFY_PROGRAM:	
	  return DEMUXER_CTRL_OK;
	default:
	  return DEMUXER_CTRL_NOTIMPL;
	}
}

demuxer_desc_t demuxer_desc_isdbt = {
          "ISDBT  demuxer",
	  "ISDBT",
	  "ISDBT",
	  "Chang",
	  "",
	  DEMUXER_TYPE_ISDBT,
	  0, // unsafe autodetect
	  check_isdbt_file,
	  demux_isdbt_fill_buffer_data,
	  demux_isdbt_open,
	  demux_isdbt_close,
	  demux_isdbt_seek,
	  demux_isdbt_control
};
