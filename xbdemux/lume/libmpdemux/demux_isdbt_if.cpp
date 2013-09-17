extern "C" {

#include "demux_rtp.h"
#include "stheader.h"

}

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

#include "demux_isdbt_if.h"
#include "isdbt.h"

#define MAXAVREADTRY	100	//40	

extern "C" void add_audio_packet(double ts,unsigned char *p,int len);
extern "C" void add_video_packet(double ts,unsigned char *p,int len);
extern "C" char	isAudioTypeAACV2;

namespace android {

Isdbt isdbt;
static void isdbt_get_audio_data_buffer1(void* pmem)
{
//    Isdbt isdbt;
    isdbt.getAudioBuffer();
    return;
}

static void isdbt_get_video_data_buffer1(void* pmem)
{
//    Isdbt isdbt;
    isdbt.getVideoBuffer();
    return;
}

static int isdbt_set_channel(int freq, int channel)
{
//	Isdbt isdbt;
	return isdbt.setIsdbtChannel(freq,channel);
}

static int isdbt_clear_channel(int freq, int channel)
{
//	Isdbt isdbt;
	return isdbt.clearIsdbtChannel(freq,channel);
}


  static  unsigned long long start_audio_timestamp;
  static  unsigned long long start_video_timestamp;
  static int start_audio = 1;
  static int start_video = 1;
  static  unsigned int	audio_no_data_cnt;
  static  unsigned int	video_no_data_cnt;	
/* This variables are defined in IsdbtService.cpp also */
#define MAXVIDEOFRAMELEN	40960	//10240	//4096
#define MAXAUDIOFRAMELEN	20000	//5000	//2000
typedef struct 
{
	unsigned long long  timestamp;
	unsigned int  framelen;
	unsigned char frame[MAXAUDIOFRAMELEN];
} MTV_AUDIO_FRAME;

typedef struct 
{
	unsigned long long  timestamp;
	unsigned int  framelen;
	unsigned char frame[MAXVIDEOFRAMELEN];
} MTV_VIDEO_FRAME;

static double lastvideots , basevideots;
static double lastaudiots , baseaudiots;
static char	isDevOpen = 0;

static int isdbt_get_audio_pmem(void* pmem , char audioType)
{

    Parcel reply;
    double ts;	

    while(1)
    {

    	int ret = isdbt.getAudioPmem(&reply);

    	
    	sp<IBinder> heap = reply.readStrongBinder();
    	int  offset = reply.readInt32();
    	int  size = reply.readInt32();
    	
	ret=reply.readInt32(); 

    	if(ret < 0)
    	{
	  	audio_no_data_cnt++;
      		LOGE("opencore fail to get audio!\n");
		if (isDevOpen == 0)
		{
			LOGE("Channel already closed!\n");
			return 0;
		}
#if 0
	  	if (audioType)
	  	{
		  	if (audio_no_data_cnt > (MAXAVREADTRY/4))
		  	{
    	  			LOGE("AUDIO close.... sleep!!!!!!!\n");
				audio_no_data_cnt = 0;
				return 0;
		  	}	
	  		usleep(50000);	//need more Audio AACV2 Frame buffering 
      			continue;	//1;
	  	}
	  	else
	  	{
#endif
		  	if (audio_no_data_cnt > MAXAVREADTRY)
		  	{
    	  			LOGE("AUDIO close.... sleep!!!!!!!\n");
				audio_no_data_cnt = 0;
				return 0;
		  	}	
			usleep(10000);
      			continue;	//return 2;	//1;
//	  	}
    	}
    	//LOGE("====Get Audio offset=%d,size=%d\n",offset,size);

    	sp<IMemoryHeap> mHeap = interface_cast<IMemoryHeap> (heap);
    	MTV_AUDIO_FRAME * pBuf=static_cast<MTV_AUDIO_FRAME*> (mHeap->base());

	audio_no_data_cnt = 0;

	//LOGE("Audio TS(%d)=%llu , start_audio_ts(%llu)\n",start_audio, pBuf->timestamp , start_audio_timestamp);

	if(start_audio && pBuf->timestamp < start_audio_timestamp)
    	{
		return 1;		
    	}
	//else if (pBuf->timestamp >= start_audio_timestamp)
	else if (pBuf->timestamp > start_audio_timestamp)	//111028
	{
		if(start_audio)
	  		start_audio = 0; 

		//if (audioType)
			ts = (double)(pBuf->timestamp-start_audio_timestamp)/90000.0 + baseaudiots;	
		//else
		//	ts = (double)(pBuf->timestamp-start_audio_timestamp)/96000.0 + baseaudiots;	
			
		lastaudiots = ts;
	}
	else
	{	
		start_audio_timestamp = pBuf->timestamp;
		baseaudiots = lastaudiots;

		ts =  lastaudiots + 0.042;	//PTS difference 3840/90000 ;	
		lastaudiots = ts;
		LOGE("!!!RESET Audio TS=%f \n", baseaudiots);
	}
	
	add_audio_packet(ts,pBuf->frame,pBuf->framelen);

    	//pBuf++;
    	return 1;
    }	
}


#if 1
static int isdbt_get_video_pmem(void* pmem)
{
    Parcel reply;
    double ts;

    while(1)
    {

    	int ret = isdbt.getVideoPmem(&reply);

    
    	sp<IBinder> heap = reply.readStrongBinder();
    	int  offset = reply.readInt32();
    	int  size = reply.readInt32();
    
	ret=reply.readInt32(); 

    	if(ret < 0)
    	{
	  	video_no_data_cnt++;	
      		LOGE("opencore fail to get video!\n");

		if (isDevOpen == 0)
		{
			LOGE("Channel already closed!\n");
			return 0;
		}

	  	if (video_no_data_cnt > MAXAVREADTRY)
	  	{
      			LOGE("VIDEO close.... sleep!!!!!!!\n");
			video_no_data_cnt = 0;
			return 0;	
	  	}		
	  	usleep(10000);
      		continue;
    	}

    	sp<IMemoryHeap> mHeap = interface_cast<IMemoryHeap> (heap);
    	MTV_VIDEO_FRAME * pBuf=static_cast<MTV_VIDEO_FRAME*> (mHeap->base());

//	LOGE("Video TS=%llu \n",pBuf->timestamp);

      	if(start_video)
      	{
		start_video_timestamp = pBuf->timestamp;
		start_audio_timestamp = pBuf->timestamp;
		start_video = 0;
		basevideots = 0;
		lastvideots = 0;
		baseaudiots = 0;
		lastaudiots = 0;
		ts=0;
      	}
      	else
      	{
		if (pBuf->timestamp > start_video_timestamp)
		{
			ts = (double)(pBuf->timestamp-start_video_timestamp)/90000.0 + basevideots;	//BTL
			//ts = (double)(pBuf->timestamp-start_video_timestamp)/100000.0 + basevideots;	//BTL
			lastvideots = ts;
		}
		else
		{	
			start_video_timestamp = pBuf->timestamp;
			basevideots = lastvideots;
			if (isAudioTypeAACV2) 	//for Brazil, 30 frames/1sec
				ts = lastvideots + 0.033;	//PTS difference 0.04;
			else 			//for Japan, 15 frames/1sec
				ts = lastvideots + 0.066;	//PTS difference 0.04;

			lastvideots = ts;
			LOGE("!!!RESET Video TS=%f \n", basevideots);
		}
      	}

	video_no_data_cnt = 0;

      	add_video_packet(ts,pBuf->frame,pBuf->framelen);
      	//pBuf++;

    	return 1;
    }
}
#else

static int isdbt_get_video_pmem(void* pmem)
{

//    Isdbt isdbt;
    Parcel reply;
	unsigned char naltype;

start:
//    LOGE("start get video!\n");
    int ret = isdbt.getVideoPmem(&reply);

    double ts;
    sp<IBinder> heap = reply.readStrongBinder();
    int  offset = reply.readInt32();
    int  size = reply.readInt32();
    int count=reply.readInt32(); 
//    unsigned int  timestamp = reply.readInt32();
    ret=reply.readInt32(); 


    if(ret < 0)
    {
	  video_no_data_cnt++;	
      LOGE("opencore fail to get video!\n");

	  if (video_no_data_cnt > 200)
	  {
      	LOGE("VIDEO close.... sleep!!!!!!!\n");
		video_no_data_cnt = 0;
		return 0;
	  }		
	  usleep(100000);
      return 1;
    }

	naltype = pBuf[3] & 0x1F;
	if(start_video)
    {
		if(naltype != 0x7 || pBuf[4] != 0x42)
		{
	  		goto start;
		}
    }

    if(naltype == 0x7 && pBuf[4] == 0x42)
    {
		if(!start_video)
		{
	  		add_video_packet(lastvideots,videoframe,videoframelen); 
	  		memset(videoframe,0,MAXVIDEOFRAMELEN);
		}

		if(start_video)
		{
	  		vp = videoframe;
		  	//retSize = cmmb_add_header(pBuf,retSize,tmpvideoframe);
			retSize = pBuf->framelen;
	  		memcpy(vp, tmpvideoframe, retSize);
	  vp+=retSize;
	  videoframelen =retSize;
	  //	LOGE("first ret Size is :%d\n",retSize);

	  retSize = hVideo->read(ppp, timeStamp);
	  pBuf=(unsigned char *)ppp;
	  retSize = cmmb_add_header(pBuf,retSize,tmpvideoframe);
	  memcpy(vp, tmpvideoframe, retSize);
	  vp+=retSize;
	  videoframelen += retSize;
	  //	LOGE("second ret Size is :%d\n",retSize);

	  retSize = hVideo->read(ppp, timeStamp);
	  pBuf=(unsigned char *)ppp;
	  retSize = cmmb_add_header(pBuf,retSize,tmpvideoframe);
	  memcpy(vp, tmpvideoframe, retSize);
	  videoframelen += retSize;
	  //	LOGE("third ret Size is :%d\n",retSize);
	}
	else
	{
	  retSize = hVideo->read(ppp, timeStamp);
	  vp = videoframe;
	  retSize = hVideo->read(ppp, timeStamp);
	  pBuf=(unsigned char *)ppp;
	  retSize = cmmb_add_header(pBuf,retSize,tmpvideoframe);
	  memcpy(vp, tmpvideoframe, retSize);
	  videoframelen = retSize;
	}
	}
	else
	{

	}
//    LOGE("====jim gao offset=%d,size=%d,count=%d==  timestamp=%d \n",offset,size,count,  timestamp);
	//LOGE("====jim gao count=%d==  ret=%d\n",count, ret);

    sp<IMemoryHeap> mHeap = interface_cast<IMemoryHeap> (heap);
    MTV_VIDEO_FRAME * pBuf=static_cast<MTV_VIDEO_FRAME*> (mHeap->base());

    for(int i=0;i<count;i++)
    {
      if(start_video)
      {
		start_video_timestamp=pBuf->timestamp;
		start_video = 0;
		basevideots = 0;
		lastvideots = 0;
		ts=0;
      }
      else
      {
		if (pBuf->timestamp > start_video_timestamp)
		{
			ts = (double)(pBuf->timestamp-start_video_timestamp)/91000.0 + basevideots;	//BTL , best
			//ts = (double)(pBuf->timestamp-start_video_timestamp)/90000.0 + basevideots;	//BTL
			//ts = (double)(pBuf->timestamp-start_video_timestamp)/100000.0 + basevideots;	//BTL
			lastvideots = ts;
		}
		else
		{	
			start_video_timestamp = pBuf->timestamp;
			basevideots = lastvideots;
			ts = lastvideots + 0.04;
			lastvideots = ts;
			LOGE("!!!BASE Video=%f \n", basevideots);
		}
      }

      add_video_packet(ts,pBuf->frame,pBuf->framelen);
      pBuf++;
    }

    return 1;
}
#endif


static int isdbt_StreamInfo(int *resolution , int *samplefreq)
{
    Parcel reply;
	unsigned char naltype;

    int ret = isdbt.getStreamInfo(&reply);

    sp<IBinder> heap = reply.readStrongBinder();

   	*resolution = reply.readInt32();
    *samplefreq = reply.readInt32();

    LOGE("ISDBT get stream info(%d, %d)!\n" , *resolution , *samplefreq);

	return 0;
}

static int isdbt_get_aud_first_frame(char *frame, int size)
{

    Parcel reply;

    int ret = isdbt.getAudioPmem(&reply);

    sp<IBinder> heap = reply.readStrongBinder();
    int  offset = reply.readInt32();
    int  cnt = reply.readInt32();

    ret=reply.readInt32(); 

    if(ret < 0)
    {
    	for(int i=0;i<5;i++)
    	{
    		ret = isdbt.getAudioPmem(&reply);

    		sp<IBinder> heap = reply.readStrongBinder();
    		offset = reply.readInt32();
    		cnt = reply.readInt32();

    		ret=reply.readInt32(); 

			if (ret < 0)
				usleep(10000);
			else
				break;
    	}
	}
	
	if (ret < 0 )
	{
		LOGE("ISDBT Audio First frame error!!!\n");
		return 0;
	}
    sp<IMemoryHeap> mHeap = interface_cast<IMemoryHeap> (heap);
    MTV_AUDIO_FRAME * pBuf=static_cast<MTV_AUDIO_FRAME*> (mHeap->base());

	//memcpy(vp, tmpvideoframe, retSize);
	if (pBuf->framelen > 6144)
	{
		LOGE("ISDBT Audio First frame length over\n", pBuf->framelen);
		return 0;
	}

	memcpy(frame , pBuf->frame, pBuf->framelen);

	return pBuf->framelen;

}


#ifdef __cplusplus
extern "C"{
#endif

  void isdbt_get_audio_data_buffer(void* pmem)
  {
    isdbt_get_audio_data_buffer1(pmem);
    return;
  }

  void isdbt_get_video_data_buffer(void* pmem)
  {
    isdbt_get_video_data_buffer1(pmem);
    return;
  }

  int isdbt_tv_set_channel(int freq, int channel)
  {
  	start_audio_timestamp = start_video_timestamp = 0;
  	start_audio = start_video = 1;

	video_no_data_cnt = 0;
	audio_no_data_cnt = 0;

	isDevOpen = 1;
    	return isdbt_set_channel(freq,channel);
  }

  int isdbt_tv_clear_channel(int freq, int channel)
  {
	isDevOpen = 0;
    	return isdbt_clear_channel(freq,channel);
  }

#if 0
  void isdbt_get_audio_pmemory(void* pmem)
  {
    isdbt_get_audio_pmem(pmem);
    return;
  }

  void isdbt_get_video_pmemory(void* pmem)
  {
    isdbt_get_video_pmem(pmem);
    return;
  }
#endif
  int isdbt_get_audio_pmemory(void* pmem , char AudioType)
  {
    return isdbt_get_audio_pmem(pmem , AudioType);
  }

  int isdbt_get_video_pmemory(void* pmem)
  {
    return isdbt_get_video_pmem(pmem);
  }

  int isdbt_getStreamInfo(int* resolution , int* samplefreq)
  {
	return isdbt_StreamInfo(resolution , samplefreq);
  }

  int isdbt_get_audio_first_frame(char *frame, int size)
  {
	return isdbt_get_aud_first_frame(frame, size);
  }	

#ifdef __cplusplus
}
#endif

}  
