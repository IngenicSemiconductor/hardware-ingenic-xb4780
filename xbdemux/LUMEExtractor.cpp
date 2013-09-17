/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0

#include "include/LUMEExtractor.h"
#include "include/LUMERecognizer.h"

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <LUMEDefs.h>
#include <utils/String8.h>
#include "ID3.h"
#include "cutils/properties.h"

extern "C"
{
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "codecs.conf.h"
}
//Convertion between StreamID and TrackID
#define STREAMID_TO_TRACKID_VIDEO(x) ((x)+1)
#define STREAMID_TO_TRACKID_AUDIO(x) ((x)+257)
#define STREAMID_TO_TRACKID_TEXT(x) ((x)+513)

#define TRACKID_TO_STREAMID_VIDEO(x) ((x)-1)
#define TRACKID_TO_STREAMID_AUDIO(x) ((x)-257)
#define TRACKID_TO_STREAMID_TEXT(x) ((x)-513)

#define LOG_TAG "LUMEExtractor"
#include <utils/Log.h>

#ifdef DEBUG_LUMEEXTRACTOR_CODE_PATH
#define EL_CODEPATH(x, y...) LOGE(x, ##y);
#else
#define EL_CODEPATH(x, y...)
#endif

#define EL_P1(x,y...) //LOGE(x,##y);
#define EL_P2(x,y...) //LOGE(x,##y);
#define EL(x,y...) //{LOGE("%s %d",__FILE__,__LINE__); LOGE(x,##y);}

#if defined(WORK_AROUND_FOR_CTS_MEDIAPLAYERFLAKY_OPENDEMUX)
static char *cstmediaflakylist[] = {
  "raw/video_480x360_mp4_h264_1350kbps_30fps_aac_stereo_192kbps",
  "raw/video_480x360_mp4_h264_1000kbps_25fps_aac_stereo_128kbps",
  "raw/video_480x360_mp4_h264_1350kbps_30fps_aac_stereo_128kbps",
  "raw/video_480x360_mp4_h264_1350kbps_30fps_aac_stereo_192kbps",
  "raw/video_176x144_3gp_h263_300kbps_25fps_aac_stereo_128kbps"
};
#endif


namespace android {

  /*
    Exception Handle:
    
    Log pri:
    1."Error:" - The failure is not recoverable, stop the execution immidiately by CHECK(false);
    2."Warning:" - The failure is recoverable or effects not much, but need to notify.
    3.xxx - whatever for normal.
    LOG_TAG should always be defined.
  */

class LUMESource : public MediaSource {
public:
    LUMESource(LUMESourceInfo*);

    virtual status_t start(MetaData *params = NULL) = 0;
    virtual status_t stop();
    
    virtual sp<MetaData> getFormat();
    
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options = NULL) = 0;

protected:
    virtual ~LUMESource();
    
    Mutex mLock;
    int32_t mFIFOLock;
    bool mStarted;
    bool mEOF;
    
    demuxer_t* mDemuxer;
    DemuxerLock* mDemuxerLock;
    sp<MetaData> mMeta;
    MediaBufferGroup *mGroup;

    LUMESource(const LUMESource &);
    LUMESource &operator=(const LUMESource &);
};//LUMESource

class LUMEAudioSource : public LUMESource {
public:
    LUMEAudioSource(const sp<LUMEExtractor> &extractor, LUMESourceInfo*);
    virtual status_t start(MetaData *params = NULL);
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
private:
    int  mSeekFlag;
    int64_t mFrameOffsetwithLen[AUDIO_EXTRACTOR_BUFFER_COUNT][MAX_AUDIO_EXTRACTOR_FRAME_OFFSET_WITH_LEN_NUM];
    MediaBuffer* mMediaBuffers[AUDIO_EXTRACTOR_BUFFER_COUNT];
    sp<LUMEExtractor> mExtractor;
};//LUMEAudioSource

class LUMEVideoSource : public LUMESource {
public:
    LUMEVideoSource(LUMESourceInfo*);
    virtual status_t start(MetaData *params = NULL);
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
private:
    int64_t mLastVideoPts;
    MediaBuffer* mMediaBuffers[VIDEO_EXTRACTOR_BUFFER_COUNT];
};//LUMEVideoSource

class LUMESubSource : public LUMESource {
public:
    LUMESubSource(LUMESourceInfo*);
    virtual status_t start(MetaData *params = NULL);
    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);
private:
    MediaBuffer* mMediaBuffers[SUB_EXTRACTOR_BUFFER_COUNT];
};//LUMESubSource

////////////////////////////////////////////////////////////LUMESource definition

LUMESource::LUMESource(LUMESourceInfo* info)
    : mFIFOLock(FIFO_VALUE),
      mStarted(false),
      mEOF(false),
      mDemuxer(info->demuxer),
      mDemuxerLock(info->demuxerLock),
      mMeta(info->meta){

    mMeta->setPointer(kKeyAVSyncLock,mDemuxerLock);
}

LUMESource::~LUMESource() {
    if(mStarted)
        stop();
}

//Release the allocated MediaBuffer
status_t LUMESource::stop() {
    CHECK(mStarted);

    delete mGroup;
    mGroup = NULL;
    mStarted = false;
    
    return OK;
}

sp<MetaData> LUMESource::getFormat() {
    return mMeta;
}

////////////////////////////////LUMEAudioSource definition
LUMEAudioSource::LUMEAudioSource(const sp<LUMEExtractor> &extractor, LUMESourceInfo* srcInfo)
    :LUMESource(srcInfo),
     mExtractor(extractor){
    mSeekFlag = 0;
}

status_t LUMEAudioSource::start(MetaData *) {
    CHECK(!mStarted);
    
    mGroup = new MediaBufferGroup;
    for(int i = 0; i < AUDIO_EXTRACTOR_BUFFER_COUNT; ++i){
	MediaBuffer* buffer = new MediaBuffer(MAX_AUDIO_EXTRACTOR_BUFFER_RANGE);
	mGroup->add_buffer(buffer);
	//buffer->meta_data()->setPointer(kKeyComposedPacketOffsetwithLen, mFrameOffsetwithLen[i]);//make no sense because buffer->reset eachtime acquired.
	mMediaBuffers[i] = buffer;
    }

    mMeta->setInt32(kKeyExtracterBufCount, AUDIO_EXTRACTOR_BUFFER_COUNT);

    mStarted = true;
  
    return OK;
}
    
status_t LUMEAudioSource::read(MediaBuffer **out, const ReadOptions *options){
    *out = NULL;

    MediaBuffer* buffer = NULL;

#ifndef SINGLE_THREAD_FOR_EXTRACTOR
    mDemuxerLock->lock();
#endif
    int64_t seekTimeUs = 0;
    mSeekFlag = 0;
    ReadOptions::SeekMode mode;    
    if (options != NULL && options->getSeekTo(&seekTimeUs, &mode)) {
	LOGV("Audio seek to time: %lld, if -1 in a video it is OK.", seekTimeUs);
	mSeekFlag = 1;
	if(!mExtractor->mHasVideoSource && seekTimeUs >= 0){
	  demux_seek(mDemuxer,(float)(seekTimeUs/MULTIPLE_S_TO_US), 0.0,1);
	}
	mEOF = false;
    }
    if(mEOF){
	LOGV("Warning:already end of audio track");

#ifndef SINGLE_THREAD_FOR_EXTRACTOR
	mDemuxerLock->unlock();
#endif

	return ERROR_END_OF_STREAM;
    }

#ifdef SINGLE_THREAD_FOR_EXTRACTOR
    
    bool freeBufferFound = false;
    for(int i = 0; i < AUDIO_EXTRACTOR_BUFFER_COUNT; ++i){
	if(mMediaBuffers[i]->refcount() == 0){
	    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
	    freeBufferFound = true;
	    break;
	}
    }

    if(!freeBufferFound){
	return NO_MEMORY;
    }

#else
    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
#endif

    int index;
    for(int i = 0; i < AUDIO_EXTRACTOR_BUFFER_COUNT; ++i){
	if(mMediaBuffers[i] == buffer){
	    index = i;
	    break;
	}		
    }

    void* frameOffsetwithLen = mFrameOffsetwithLen[index];
    buffer->meta_data()->setPointer(kKeyComposedPacketOffsetwithLen, frameOffsetwithLen);


    demux_stream_t* ds = mDemuxer->audio;
    if(NULL == ds){
        LOGE("Error:no demuxer->audio!!");
        CHECK(false);
    }

    sh_audio_t *shAudio = (sh_audio_t *)ds->sh;
    if(NULL == shAudio){
	LOGE("Error:no ds->sh!!");
	CHECK(false);
    }
    unsigned char* packet = NULL;
    double curPts = 0.0, nextPts = 0.0;
    int packetNum = 0;
    int64_t bufferHeadPts = 0;

    /*when two audio track have diffrent samplerate, must free old data*/
    if(mDemuxer->changeAudioFlag==1){
    	ds_free_packs(ds);
    	mDemuxer->changeAudioFlag=0;
    }

    int frameOffsetwithLenItemCount = 0;
    int packetSize = ds_get_packet_pts(ds, &packet, &curPts);//Replace ds_get_packet. pts convinently got from "ds->current->pts" which difers with "ds->pts"; If any pro, recover to "ds->pts".
    EL_P2("read packet:size:%d;pts:%f", packetSize, curPts);

    if(packetSize == -1)
    {
      EL_P1("Warning:end of audio track");
      ALOGE("Warning:end of audio track");
      buffer->release();
      buffer = NULL;
      
#ifndef SINGLE_THREAD_FOR_EXTRACTOR
	    mDemuxerLock->unlock();
#endif
	    return ERROR_END_OF_STREAM;
	}
	
    memcpy((unsigned char*)buffer->data(), packet, packetSize);
#ifdef AUDIODEC_IN_UNIT_OF_1_FRAME	
    int32_t* frameOffset = (int32_t*)((int64_t*)frameOffsetwithLen + frameOffsetwithLenItemCount);
    int32_t* frameLen = (int32_t*)(frameOffset + 1);
    
    *frameOffset = packetSize;
    *frameLen = packetSize;
    
    frameOffsetwithLenItemCount += 1;
#endif
    
    if(shAudio->i_bps)
      bufferHeadPts = (int64_t)((ds->pts + ((float)(ds->pts_bytes-packetSize))/shAudio->i_bps) * MULTIPLE_S_TO_US);
    else
      bufferHeadPts = (int64_t)(ds->pts * MULTIPLE_S_TO_US);
    
#ifndef SINGLE_THREAD_FOR_EXTRACTOR
    mDemuxerLock->unlock();
#endif

    buffer->set_range(0, packetSize);
    buffer->meta_data()->setInt64(kKeyTime, bufferHeadPts);
    LOGV("audio pts = %lld",bufferHeadPts);

#ifdef AUDIODEC_IN_UNIT_OF_1_FRAME
    buffer->meta_data()->setInt32(kKeyAudioSeek, mSeekFlag);
    buffer->meta_data()->setInt32(kKeyComposedPacketOffsetwithLenItemCount, frameOffsetwithLenItemCount);
    buffer->meta_data()->setInt32(kKeyComposedPacketOffsetwithLenItemConsumed, 0);
#endif
    *out = buffer;    

    return OK;
}

//////////////////////////////LUMEVideoSource definition

LUMEVideoSource::LUMEVideoSource(LUMESourceInfo* srcInfo)
  :LUMESource(srcInfo){  
  mLastVideoPts = 0;
}

status_t LUMEVideoSource::start(MetaData *) {
    CHECK(!mStarted);

    mGroup = new MediaBufferGroup;
    for(int i = 0; i < VIDEO_EXTRACTOR_BUFFER_COUNT; ++i){
	MediaBuffer* buffer = new MediaBuffer(MAX_VIDEO_EXTRACTOR_BUFFER_RANGE);
	mGroup->add_buffer(buffer);
	mMediaBuffers[i] = buffer;
    }

    mMeta->setInt32(kKeyExtracterBufCount, VIDEO_EXTRACTOR_BUFFER_COUNT);
  
    mStarted = true;
    return OK;
}

status_t LUMEVideoSource::read(MediaBuffer **out, const ReadOptions *options){
    EL_CODEPATH("LUMEVideoSource read");
    *out = NULL;    

#ifndef SINGLE_THREAD_FOR_EXTRACTOR 
    mDemuxerLock->lock();
#endif
    int64_t seekTimeUs = 0;
    ReadOptions::SeekMode mode;
    int mseeking = 0;
    int isthumb = 0;
    if (options != NULL && options->getSeekTo(&seekTimeUs, &mode)) {
	EL("Video seek to time: %lld", seekTimeUs);
	mseeking = 1;
	if(seekTimeUs >= 0){
	  if((mDemuxer->thumb_nseekable == 1) && (SEEK_WITH_IS_THUMB_MODE == mode))//(options->getIsThumb(&isthumb)==1))
	    demux_seek(mDemuxer,0.0,0.0,1);
	  else{
	    mDemuxer->thumb_eindexfind = 0;
	    if(SEEK_WITH_IS_THUMB_MODE == mode)//options->getIsThumb(&isthumb)==1)
	      mDemuxer->thumb_eindexfind = 1;
	     demux_seek(mDemuxer,(float)(seekTimeUs/MULTIPLE_S_TO_US), 0.0,1);
	  }
	    mEOF = false;
	}
    }

    if(mEOF){
	EL_P1("Warning:already end of video track");
	
#ifndef SINGLE_THREAD_FOR_EXTRACTOR
	mDemuxerLock->unlock();
#endif

	return ERROR_END_OF_STREAM;
    }

    MediaBuffer* buffer = NULL;

#ifdef SINGLE_THREAD_FOR_EXTRACTOR 

    bool freeBufferFound = false;
    for(int i = 0; i < VIDEO_EXTRACTOR_BUFFER_COUNT; ++i){
	if(mMediaBuffers[i]->refcount() == 0){
	    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
	    freeBufferFound = true;
	    break;
	}
    }

    if(!freeBufferFound){
	return NO_MEMORY;
    }

#else
    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
#endif
    demux_stream_t* ds = mDemuxer->video;
    if(NULL == ds){
        LOGE("Error:no demuxer->video!!");
        CHECK(false);
    }

    if(MAX_VIDEO_EXTRACTOR_BUFFER_RANGE < (int)ds->buffer_size){
        LOGE("Error:video packet size out of range!!");
        return ERROR_OUT_OF_RANGE;
    }
        
    sh_video_t *shVideo = (sh_video_t *)ds->sh;
    if(NULL == shVideo){
        LOGE("Error:no demuxer->video->sh!!");
        CHECK(false);
    }
    
    unsigned char* frame = NULL;
    float frameTime = 0.0;
    int   frameSizeInvalid = 0;
drop_frame:
    int frameSize = video_read_frame(shVideo,&frameTime,&frame,0);

    if(frameSize > (int)buffer->size()){//suppose (int)buffer->size() would change into minus number
      goto drop_frame;
    }
    
    if(frameSize == -1)
      {
	if(mLastVideoPts < shVideo->stream_duration - 500000ll){
	  if(frameSizeInvalid ++ < 3 )
	    goto drop_frame;
	}
        EL("warning:end of video track");
	buffer->release();
	buffer = NULL;

	mEOF = true;

#ifndef SINGLE_THREAD_FOR_EXTRACTOR
	mDemuxerLock->unlock();
#endif
        return ERROR_END_OF_STREAM;
    }
    
    int64_t bufferHeadPts = (int64_t)(shVideo->pts * MULTIPLE_S_TO_US);
    if((bufferHeadPts < mLastVideoPts) && !mseeking)
      {
	bufferHeadPts = (mLastVideoPts +(frameTime*MULTIPLE_S_TO_US));
      }
    
    mLastVideoPts = bufferHeadPts;
#ifndef SINGLE_THREAD_FOR_EXTRACTOR
    mDemuxerLock->unlock();
#endif
#ifdef PREFETCHER_DEPACK_NAL
   if(!shVideo->is_rtsp){
    if(shVideo->need_depack_nal){
      unsigned char* dst=(unsigned char*)buffer->data();
      unsigned char *src=frame;
      int i;
      if(shVideo->is_avc){
	int si=0;
	int di=0;
	int length=0;
	while(si<frameSize){//for one frame
	  int curr_nalsize=0;
	  if(src != NULL){
	    for(i = 0; i < shVideo->nal_length_size; i++){
	      curr_nalsize = (curr_nalsize << 8) | src[si++];
	    }
	  }
	  di += shVideo->nal_length_size;
	  length += curr_nalsize+shVideo->nal_length_size;
	  if(length > frameSize || (curr_nalsize > MAX_VIDEO_EXTRACTOR_BUFFER_RANGE) || (curr_nalsize <= 0))
	  {
		  frameSize=0;
		  goto drop_frame;
		  //return ERROR_OUT_OF_RANGE;
	  }
	  // LOGE("si=0x%x,di=0x%x,curr_nalsize=0x%x",si,di,curr_nalsize);
	  while(si+2<length){//for one nal
	    //remove escapes (very rare 1:2^22)
	    if(src[si+2]>3){
	      dst[di++]= src[si++];
	      dst[di++]= src[si++];
	    }else if(src[si]==0 && src[si+1]==0){
	      if(src[si+2]==3){ //escape
		dst[di++]= 0;
		dst[di++]= 0;
		si+=3;
		curr_nalsize--;
		continue;
	      }
	    }
	    dst[di++]= src[si++];
	  }
	  while(si<length)
	    dst[di++]= src[si++];

	  int dst_nalsize_pos=di-curr_nalsize-1;
	  for(i = 0; i < shVideo->nal_length_size; i++){
	    dst[dst_nalsize_pos-i]=curr_nalsize&0xff;
	    curr_nalsize>>=8;
	  }
	}
	frameSize=di;
      }
      else{
	int si=0;
	int di=0;
	int length=0;
	for(; si + 3 < frameSize; si++){
	  // This should always succeed in the first iteration.
	  if(src[si] == 0 && src[si+1] == 0 && src[si+2] == 1)
	    break;
	}
	if(si+3 >= frameSize){
	  frameSize=0;
	  return ERROR_OUT_OF_RANGE;
	}
	si+=3;

	di = shVideo->nal_length_size;//default =4	
	int dst_nal_start_pos=di;
	while(si+2<frameSize){//for one frame
	  //remove escapes (very rare 1:2^22)
	  if(src[si+2]>3){
	    dst[di++]= src[si++];
	    dst[di++]= src[si++];
	  }else if(src[si]==0 && src[si+1]==0){
	    if(src[si+2]==3){ //escape
	      dst[di++]= 0;
	      dst[di++]= 0;
	      si+=3;
	      continue;
	    }else{
	      //EL("si=0x%x,di=0x%x,dst_nal_start_pos=0x%x",si,di,dst_nal_start_pos);
	      int curr_nalsize=di-dst_nal_start_pos;
	      for(i = 0; i < shVideo->nal_length_size; i++){
		dst[dst_nal_start_pos-i-1]=curr_nalsize&0xff;
		curr_nalsize>>=8;
	      }
	      di += shVideo->nal_length_size;
	      dst_nal_start_pos=di;

	      for(; si + 3 < frameSize; si++){
		// This should always succeed in the first iteration.
		if(src[si] == 0 && src[si+1] == 0 && src[si+2] == 1)
		  break;
	      }
	      if(si+3 >= frameSize){
		break;
	      }
	      si+=3;

	      continue;
	    }
	  }
	  dst[di++]= src[si++];
	}
	while(si<frameSize)
	  dst[di++]= src[si++];

	int curr_nalsize=di-dst_nal_start_pos;
	for(i = 0; i < shVideo->nal_length_size; i++){
	  dst[dst_nal_start_pos-i-1]=curr_nalsize&0xff;
	  curr_nalsize>>=8;
	}
	frameSize=di;
      }
     }else{
      memcpy((unsigned char*)buffer->data(), frame, frameSize);//TODO(high pri):The memcpy may could be removed by passing start ptr to mediabuffer.
    }
}
#else
    //should set_range first, because set_range would check buffer size   
    //buffer->set_range(0, frameSize);
    memcpy((unsigned char*)buffer->data(), frame, frameSize);//TODO(high pri):The memcpy may could be removed by passing start ptr to mediabuffer.
#endif
    buffer->set_range(0, frameSize);
    buffer->meta_data()->setInt64(kKeyTime, (int64_t)(bufferHeadPts));
    buffer->meta_data()->setInt64(kKeyStartTime, (int64_t)(mDemuxer->start_time));

    *out = buffer;    
    return OK;

}

////////////////////////////////LUMESubSource definition
LUMESubSource::LUMESubSource(LUMESourceInfo* srcInfo)
  :LUMESource(srcInfo){
}

status_t LUMESubSource::start(MetaData *) {
    CHECK(!mStarted);

    mGroup = new MediaBufferGroup;
    for(int i = 0; i < SUB_EXTRACTOR_BUFFER_COUNT; ++i){
	MediaBuffer* buffer = new MediaBuffer(MAX_SUB_EXTRACTOR_BUFFER_RANGE);
	mGroup->add_buffer(buffer);
	mMediaBuffers[i] = buffer;
    }

    mStarted = true;
  
    return OK;
}


status_t LUMESubSource::read(MediaBuffer **out, const ReadOptions *options){
    *out = NULL;
    MediaBuffer* buffer = NULL;

#ifdef SINGLE_THREAD_FOR_EXTRACTOR 
    bool freeBufferFound = false;
    for(int i = 0; i < SUB_EXTRACTOR_BUFFER_COUNT; ++i){
	if(mMediaBuffers[i]->refcount() == 0){
	    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
	    freeBufferFound = true;
	    break;
	}
    }

    if(!freeBufferFound){
	return NO_MEMORY;
    }
#else
    CHECK_EQ(mGroup->acquire_buffer(&buffer), OK);
#endif

    demux_stream_t* ds = mDemuxer->sub;
    if(NULL == ds){
        LOGE("Error:no demuxer->sub!!");
        CHECK(false);
    }

    sh_sub_t *shSub = (sh_sub_t *)ds->sh;

    if((NULL == shSub) && (!mDemuxer->ists)){
	LOGE("Error:no ds->sh!!");
	CHECK(false);
    }

    char type = shSub ? shSub->type : 'v';

    unsigned char* packet = NULL;
    int packetSize = 0;
    double spts = 0.0, endpts=0.0;

    packetSize = ds_get_packet_sub(ds,&packet,&spts,&endpts); 

    if(packetSize == -1) {
        buffer->release();
	buffer = NULL;
	mEOF = true;
	return ERROR_END_OF_STREAM;
    }else if((packetSize == -2) || (type == 'v')){
        unsigned char space[2] = "";
	memcpy((unsigned char*)buffer->data(), space, 1);
	buffer->set_range(0, 300);    
	*out = buffer;
	return OK;
    }else{
      unsigned char* p = packet;
      int frommsec, fromsec, tomsec, tosec;
    
      if (type == 'a') {
	int i;
	int skip_commas = 8;
	if (packetSize > 10 && memcmp(packet, "Dialogue: ", 10) == 0)
	  skip_commas = 9;
	for (i=0; i < skip_commas && *p != '\0'; p++)
	  if (*p == ','){
	    i++;	
	  }

	packet = p;
      
	packetSize -= p - packet;	    
	p = packet;
      }
    
      memcpy((unsigned char*)buffer->data(), packet, packetSize);
      buffer->set_range(0, packetSize);    

      buffer->meta_data()->setInt64(kKeySpts, (int64_t)(spts*1000.0));
      buffer->meta_data()->setInt64(kKeyEpts, (int64_t)(endpts*1000.0));
  
      *out = buffer;
      return OK;
    }
}
///////////////////////////////////////////////////////LUMEExtractor Definition
//Generate stream and find demuxer corresponding to the DataSource.
Mutex LUMEExtractor::mGlobalDemuxerLock;
LUMEExtractor::LUMEExtractor(const sp<DataSource> &source)
    : mSource(source),
      mFileFormat(-1),
      mStream(NULL),
      mDemuxer(NULL),
      mTrackList(NULL),
      mTrackCount(0),
      mTrackCounted(false),
      mTrackListInited(false),
      mHasVideoSource(false),
      mHasVideo(false),
      mHasAudio(false),
      mStarted(false),
      mHasSub(false){
  Mutex::Autolock autoLock(mGlobalDemuxerLock);
    EL_CODEPATH("LUMEExtractor constructor");
#if defined(WORK_AROUND_FOR_CTS_MEDIAPLAYERFLAKY_OPENDEMUX)
    char *url = (char *)source->getUri().string();
    int ctsmediaflakyflag = 0;
    if(url != NULL){
      int i;
      for (i = 0; i < 5; i++){
	if (strstr(url, cstmediaflakylist[i])){
	  ctsmediaflakyflag = 1;
	  break;
	}
      }
    }
#endif
    //mSource->seek(0,SEEK_SET);
    mPrefetcher = new LUMEPrefetcher;
    mLUMEStream = new LUMEStream(mSource.get());

    mStream = open_stream_opencore(NULL,NULL,&mFileFormat,(int)(mLUMEStream.get()));//TODO:(low pri),we are reusing opencore's demuxer things, need name modification??
    if(NULL == mStream){
        LOGE("Error:stream control failed");
        //CHECK(false);//The CHECK(false) will stop the execution. behavior like oscl_leave.
	return;
    }
    mStream->Context = (void *)this;
 
#if defined(WORK_AROUND_FOR_CTS_MEDIAPLAYERFLAKY_OPENDEMUX)
    if (ctsmediaflakyflag)
      mDemuxer = demux_open(mStream,mFileFormat,-1,-1,-2,DEMUXER_CTS_MEDIA_FLAKY);
    else
#endif
      mDemuxer = demux_open(mStream,mFileFormat,-1,-1,-2,NULL);
  
    if(NULL == mDemuxer){
      LOGE("Error:demux open failed tid = %d",gettid());
        //CHECK(false);
      return;
    }
    double slen=demuxer_get_time_length(mDemuxer);
    if(slen>0.0&&slen<5.0)
      demux_seek(mDemuxer,0.0,0.0,SEEK_ABSOLUTE);

    if(mDemuxer && mDemuxer->video && mDemuxer->video->sh){
      ((sh_video_t*)(mDemuxer->video->sh))->videopicture = NULL;    
      video_read_properties((sh_video_t*)mDemuxer->video->sh);
    }

    mStarted = true;  

    EL_P1("demuxer type after demux_open:%d",mDemuxer->type);
}


LUMEExtractor::~LUMEExtractor() {
    EL_P1("~LUMEExtractor");
    if(!mStarted)
	return;

    if(NULL != mTrackList){
        delete[] mTrackList;
        mTrackList = NULL;
    }
    
    int hasvideostream=0;
    if(mDemuxer && mDemuxer->video && mDemuxer->video->sh){
      hasvideostream=1;
    }

    if(mDemuxer && mDemuxer->video && mDemuxer->video->sh)
    {
        sh_video_t *shVideo = (sh_video_t *)mDemuxer->video->sh;
        if(shVideo->videobuffer)
            free(shVideo->videobuffer);
        shVideo->videobuffer = NULL;
        if(shVideo->videopicture)
            free(shVideo->videopicture);
        shVideo->videopicture = NULL;
    }
    
    if(NULL != mDemuxer){
        free_demuxer(mDemuxer);
        mDemuxer = NULL;
    }
    
    if(NULL != mStream){
        free_stream(mStream);
        mStream = NULL;
    }
    
    mStarted = false;

    if (mPrefetcher != NULL) {
        CHECK_EQ(mPrefetcher->getStrongCount(), 1);
    }
    mPrefetcher.clear();
}

//Track(stream) count. 
size_t LUMEExtractor::countTracks() {
    if(!mStarted)
	return 0;

    if(!mTrackCounted){  
        uint32_t index = 0;
	
        // video tracks
        for (int i = 0; i < MAX_V_STREAMS; ++i){
            if (NULL != mDemuxer->v_streams[i]){
                ++index;
                mHasVideo = true;
            }
        }

        // audio tracks
        for (int i = 0; i < MAX_A_STREAMS; ++i){
          if (NULL != mDemuxer->a_streams[i]){
              ++index;
              mHasAudio = true;
          }
        }

        // sub tracks
        for (int i = 0; i < MAX_S_STREAMS; ++i){
          if (NULL != mDemuxer->s_streams[i]){
              ++index;
              mHasSub = true;
          }
        } 
	
        
        mTrackCount = index;
        //mTrackCounted = true;
    }

    return mTrackCount;
}

static const char *GetMIMETypeForHandler(uint32_t fourcc, bool audio) {
  const codecs_t *codecs = audio ? builtin_audio_codecs : builtin_video_codecs;
  bool found = false;
  for (/* NOTHING */; codecs->name; codecs++) {
    for (int j = 0; j < CODECS_MAX_FOURCC; j++) {
      if(codecs->fourcc[j] == -1) break;
      if (codecs->fourcc[j] == fourcc) {
	found = true;
	break;
      }
    }
    if(found) break;
  }

  if(found){
    char *tmp = codecs->dll ? codecs->dll : codecs->name;
    ALOGV("GetMIMETypeForHandler tmp=%s name:%s type:%x",tmp, codecs->name, fourcc);
    if(strstr(tmp, "h264"))
      return MEDIA_MIMETYPE_VIDEO_AVC;
    else if(strstr(tmp, "mpeg4"))
      return MEDIA_MIMETYPE_VIDEO_MPEG4;
    else if (strstr(tmp, "wmv3"))
      return MEDIA_MIMETYPE_VIDEO_WMV3;
    else if (strstr(tmp, "drvc") && fourcc == 0x30345652){
      return MEDIA_MIMETYPE_VIDEO_RV40;
    }else if (strstr(tmp, "vc1"))
      return MEDIA_MIMETYPE_VIDEO_WMV3;
    else if(strstr(tmp, "aac"))
      return MEDIA_MIMETYPE_AUDIO_AAC;
    else if (strstr(tmp, "vorbis"))
      return MEDIA_MIMETYPE_AUDIO_VORBIS;
    else if (strstr(tmp, "ac3") || strstr(tmp, "dca")) 
      return MEDIA_MIMETYPE_AUDIO_MP4A;
    else if(audio/*strstr(tmp, "aac")*/)
      return /*MEDIA_MIMETYPE_AUDIO_AAC*/"aaa";

  }
  return NULL;
}

//Get the TrackList with metaData, streamID/Type stuff.
status_t LUMEExtractor::getTrackList()
{
    if(mTrackListInited){
        delete[] mTrackList;
        mTrackList = NULL;
        //return OK;
    }
  
    if(0 == countTracks()){
        LOGE("Error:No track found");
        CHECK(false);
    }
    
    mTrackList = new TrackItem[mTrackCount];
    
    int index = 0;
  
    // video tracks
    for (int i = 0; i < MAX_V_STREAMS; ++i){
        if (NULL != mDemuxer->v_streams[i]){
            EL_P1("v_stream found");
            sh_video_t* shVideo = (sh_video_t*)(mDemuxer->v_streams[i]);
            
            mTrackList[index].streamID = i;
            mTrackList[index].videoID = shVideo->vid;
            mTrackList[index].trackID = STREAMID_TO_TRACKID_VIDEO(shVideo->vid);
            mTrackList[index].streamType = LUMEExtractor::videoType;
            
            sp<MetaData> meta = new MetaData;
	    const char *mime = GetMIMETypeForHandler(shVideo->format, false);
	    meta->setCString(kKeyMIMEType, mime);
            if(shVideo->bih){
                /////TODO:(high pri)Need another "config_parser"(in opencore) to fill them.
                if(0 == shVideo->disp_w)
                    shVideo->disp_w = shVideo->bih->biWidth;
                if(0 == shVideo->disp_h)
                    shVideo->disp_h = shVideo->bih->biHeight;
            }

            meta->setInt32(kKeyWidth, shVideo->disp_w);
            meta->setInt32(kKeyHeight, shVideo->disp_h);

            meta->setInt32(kKeyDisplayWidth, shVideo->disp_w);
            meta->setInt32(kKeyDisplayHeight, shVideo->disp_h);
	    meta->setInt32(kKeyRotation,shVideo->rotation_degrees);

            meta->setInt32(kKeySampleRate, shVideo->fps);
            meta->setInt32(kKeyBitRate, shVideo->i_bps);
	    int64_t duration = demuxer_get_time_length(mDemuxer) * MULTIPLE_S_TO_US;
	    shVideo->stream_duration = duration;
            meta->setInt64(kKeyDuration, duration);
	    meta->setInt64(kKeyThumbnailTime, duration * THUMBNAIL_TIME_PERCENTAGE);
            meta->setPointer(kKeyVideoSH, (void *)shVideo);
            
            mTrackList[index].metaData = meta;
            
            ++index;
	    ++(mDemuxerLock.avStreamCount);
        }
    }
    
    // audio tracks
    for (int i = 0; i < MAX_A_STREAMS; ++i){
        if (NULL != mDemuxer->a_streams[i]){
            EL_P1("a_stream found");
            sh_audio_t* shAudio = (sh_audio_t*)(mDemuxer->a_streams[i]);
            
            mTrackList[index].streamID = i;
            mTrackList[index].audioID = shAudio->aid;
            mTrackList[index].trackID = STREAMID_TO_TRACKID_AUDIO(shAudio->aid);
            mTrackList[index].streamType = LUMEExtractor::audioType;
            
            sp<MetaData> meta = new MetaData;
	    const char *mime = GetMIMETypeForHandler(shAudio->format, true);
	    if (shAudio->format == 0x726d6173)//amr_nb
	      meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_AMR_NB);
	    else
	      meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_GENERAL);
	    meta->setCString(kKeyMIMEType, mime);
            meta->setInt32(kKeySampleRate, shAudio->samplerate);
            meta->setInt32(kKeyBitsPerSample, shAudio->samplesize * 8);
            meta->setInt32(kKeyBitRate, shAudio->i_bps * 8);
            meta->setInt64(kKeyDuration, demuxer_get_time_length(mDemuxer) * MULTIPLE_S_TO_US);
            
            if(shAudio->wf) {
                /////TODO:(high pri)Need another "config_parser"(in opencore) to fill them.
                shAudio->channels = shAudio->wf->nChannels;
                shAudio->samplerate = shAudio->wf->nSamplesPerSec;
                /////
                meta->setInt32(kKeyBitsPerSample,shAudio->wf->wBitsPerSample);
                meta->setInt32(kKeyBitRate, shAudio->wf->nAvgBytesPerSec * 8);
                meta->setInt32(kKeySampleRate, shAudio->wf->nSamplesPerSec);	     
            }
	    meta->setInt32(kKeyChannelCount, shAudio->channels >= 2 ? 2 : shAudio->channels);
            
            meta->setPointer(kKeyAudioSH, shAudio);
            
            mTrackList[index].metaData = meta;
            
            ++index;
	    ++(mDemuxerLock.avStreamCount);
        }
    }
    
    // sub tracks
    for (int i = 0; i < MAX_S_STREAMS; ++i){
        if (NULL != mDemuxer->s_streams[i]){
            EL("s_stream found");
            sh_sub_t* shSub = (sh_sub_t*)(mDemuxer->s_streams[i]);
            
            mTrackList[index].streamID = i;
            mTrackList[index].subID = shSub->sid;
            mTrackList[index].trackID = STREAMID_TO_TRACKID_TEXT(shSub->sid);
            mTrackList[index].streamType = LUMEExtractor::subType;

            sp<MetaData> meta = new MetaData;
	    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_SUBTL_LUME);

	    //meta->setPointer(kKeySubSH, shSub);
            mTrackList[index].metaData = meta;
            ++index;
        }
    }


    mTrackListInited = true;
  
    return OK;
}

status_t LUMEExtractor::getTrackList_l(){
  getTrackList();
  return OK;
}

int LUMEExtractor::getAudioTrackIndex(int id){
    int index;
    
    for (index = 0; index < mTrackCount; ++index){
      if(mDemuxer->a_streams[mTrackList[index].streamID]){
	if(mDemuxer->ists){
	  if(mTrackList[index].audioID == id){
	    return index;
	  }
	}else if(mTrackList[index].streamID == id){
	  return index;
	}
      }
    } 

    return -1;
}

int LUMEExtractor::getSubTrackIndex(int id){
    int index;
    for (index = 0; index < countTracks(); ++index){
      if(mTrackList[index].streamType == LUMEExtractor::subType){
	if(mDemuxer->ists){
	  if(mTrackList[index].subID == id){
	    return index;
	  }
	}else if(mTrackList[index].streamID == id){
	  return index;
	}
      }
    }
    return -1;
}


int LUMEExtractor::getFirstSubTrackIndex(){
    int index;
    for (index = 0; index < countTracks(); ++index){
      if(mTrackList[index].streamType == LUMEExtractor::subType){
	return index;
      }
    }
    return -1;
}

int LUMEExtractor::getFirstSubTrackId(){
    int index;
    for (index = 0; index < countTracks(); ++index){
      if(mTrackList[index].streamType == LUMEExtractor::subType){
	if(mDemuxer->ists){
	  return mTrackList[index].subID; 
        }else{
	  return mTrackList[index].streamID;
        }
      }
    }
    return -1;
}


status_t LUMEExtractor::getTrackInfo(char *info){
  int atrackcount = getTrackCount();
  int i;
  strcpy(info,"");
  for(i = 0;i < atrackcount;i++){
    strcat(info, "Audio");
    if(mDemuxer->audio_info){
      strcat(info, "/");
      strcat(info, mDemuxer->audio_info+i*100);
    }else{
      sprintf(info, "%s/Stream 0%d--%d", info, i+1, i+1);
    }
    if(i!=atrackcount-1)
      strcat(info, "#");
  }
  return OK;
}

status_t LUMEExtractor::getCharStreamInfo(char *info){
  int atrackcount = (mDemuxer->ists) ? mDemuxer->tssub_cnt : getCharStreamCount();
  int i;
  strcpy(info,"");
  for(i = 0;i < atrackcount;i++){
    strcat(info, "Sub");
    if(mDemuxer->sub_info){
      strcat(info, "/");
      strcat(info, mDemuxer->sub_info+i*100);
    }else{
      sprintf(info, "%s/Stream 0%d--%d", info, i+1, i+1);
    }

    if(i!=atrackcount-1)
      strcat(info, "#");
  }
  return OK;
}


void LUMEExtractor::changeAudioTrack(int changedId){
  mDemuxer->a_changed_id = changedId;
  mDemuxer->changeAudioFlag = 1;
}


void LUMEExtractor::changeSubTrack(int changedId){
  mDemuxer->s_changed_id = changedId;
}

int LUMEExtractor::getCurrentTrack(){
  return mDemuxer->audio->id;
}


int LUMEExtractor::getCurrentSubTrack(){
  return mDemuxer->sub->id;
}

int LUMEExtractor::getVideoTrackCount(){
  int atrackCount = 0;
  for (int i = 0; i < MAX_V_STREAMS; ++i)
  {
    if (NULL != mDemuxer->v_streams[i])
    {
      ++atrackCount;
    }
  }
  return atrackCount;
}

int LUMEExtractor::getTrackCount(){
  int atrackCount = 0;
  for (int i = 0; i < MAX_A_STREAMS; ++i)
  {
    if (NULL != mDemuxer->a_streams[i])
    {
      ++atrackCount;
    }
  }
  return atrackCount;
}

int LUMEExtractor::getCharStreamCount(){
  int atrackCount = 0;
  for (int i = 0; i < MAX_S_STREAMS; ++i)
  {
    if (NULL != mDemuxer->s_streams[i])
    {
      ++atrackCount;
    }
  }
  return atrackCount;
}

int LUMEExtractor::isTsStream(){
  return mDemuxer->ists;
}

sp<MediaSource> LUMEExtractor::getTrack(size_t index) {
    if(!mStarted)
	return NULL;

    if(getTrackList() != OK){
        LOGE("Error:Failed to getTrackList");
        CHECK(false);
    }
    
    srcInfo.demuxer = mDemuxer;
    srcInfo.demuxerLock = &mDemuxerLock;
    srcInfo.meta = mTrackList[index].metaData;
    
    sp<MediaSource> source = NULL;
    switch(mTrackList[index].streamType)
    {
    case LUMEExtractor::audioType:
        source = new LUMEAudioSource(this, &srcInfo);
	break;
    case LUMEExtractor::videoType:
        mHasVideoSource = true;
        source = new LUMEVideoSource(&srcInfo);
	break;
    case LUMEExtractor::subType:
        source = new LUMESubSource(&srcInfo);
	break;
    default:
        LOGE("Error: unrecognized stream type found!!");
        CHECK(false);
    }
    if (mPrefetcher != NULL) {
      source = mPrefetcher->addSource(source);
    }
    return source;
}
sp<MetaData> LUMEExtractor::getTrackMetaData(size_t index, uint32_t flags) {
    if(!mStarted)
	return NULL;

    if(OK != getTrackList()) {
        LOGE("Error:getTrackList failed");
        CHECK(false);
    }
    
    if(index >= mTrackCount){
        LOGE("Error:Track index out of range");
        CHECK(false);
    }
    
    return mTrackList[index].metaData;
}

//TODO:The differences between trackMetaData and metaData? And make getMetaData the same with getTrackList?
sp<MetaData> LUMEExtractor::getMetaData() {
    if(!mStarted)
	return NULL;

    if(0 == countTracks()){
        LOGE("Error:No track found");
        CHECK(false);
    }
    
    sp<MetaData> meta = new MetaData;
  
    if(mHasVideo)
        meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_LUME);
    else if(mHasAudio)
        meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_LUME);
    else if(mHasSub) 
        meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_SUBTL_LUME);
    //TODO:should the stream type other than a/v be handled? seems subtitle are not supported in stagefright.
  
    
      char* info = NULL;
      info = demux_info_get(mDemuxer, "Title");
      uint8_t *info1 = (uint8_t*)info;
      if(info != NULL)meta->setCString(kKeyTitle, info);
      info = demux_info_get(mDemuxer, "Author");
      if(info == NULL)
	info = demux_info_get(mDemuxer, "Artist");
      if(info != NULL)meta->setCString(kKeyArtist, info);
      info = demux_info_get(mDemuxer, "Album");
      if(info != NULL)meta->setCString(kKeyAlbum, info);
      info = demux_info_get(mDemuxer, "Year");
      if(info != NULL)meta->setCString(kKeyYear, info);
      info = demux_info_get(mDemuxer, "Genre");
      if(info != NULL)meta->setCString(kKeyGenre, info);
      info = demux_info_get(mDemuxer, "Track");
      if(info != NULL)meta->setCString(kKeyCDTrackNumber, info);
      
      //info = demux_info_get(mDemuxer, "udta");
      info = demux_info_get(mDemuxer, "location");
      //LOGE("udta info = %s", info);
      if(info != NULL)meta->setCString(kKeyLocation, info);

      //TODO:The info get from demux could be in form of ISO 8859-1/UTF-8/UTF-16 BE/UCS-2...which is unknown with provided by demux. however, the info got is exactly the correct bytes content.

      ID3 id3(mSource);
      if (!id3.isValid()) {
        return meta;
      }
      size_t dataSize;
      String8 mime;
      const void *data = id3.getAlbumArt(&dataSize, &mime);

      if (data) {
        meta->setData(kKeyAlbumArt, MetaData::TYPE_NONE, data, dataSize);
        meta->setCString(kKeyAlbumArtMIME, mime.string());
      }

    return meta;
}

uint32_t LUMEExtractor::flags() const {
    uint32_t flags = CAN_SEEK | CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_PAUSE;
    
    return flags;
}

bool SniffLUME(const sp<DataSource> &source, String8 *mimeType, float *confidence, sp<AMessage> *) {
    if(LUMERecognizer::recognize(source)){
	*mimeType = MEDIA_MIMETYPE_CONTAINER_LUME;
	*confidence = 0.9f;
	return true;
    }
    
    return false;
}
  
}  // namespace android
using namespace android;
extern "C" {
MediaExtractor *createLUMEExtractor(const sp<DataSource> &source, String8 *mimeType, float *confidence) {
  if (SniffLUME(source, mimeType, confidence, NULL)) {
    ALOGE("createLUMEExtractor mimeType=%s .....",mimeType->string());
    return new LUMEExtractor(source);
  }
  return NULL;
}
}
