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

#ifndef LUME_EXTRACTOR_H_

#define LUME_EXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>
#include <utils/List.h>
#include <utils/threads.h>
#include <LUMEStream.h>
#include "LUMEPrefetcher.h"

#ifdef __cplusplus
extern "C"
{
	struct demuxer;
	typedef struct demuxer demuxer_t;	
	struct stream;
	typedef struct stream stream_t;	
}
#endif

namespace android {

struct DemuxerLock{//externable for subtitle? don't know.(
    Mutex avLock;
    int avStreamCount;

    DemuxerLock(){avStreamCount = 0;}
    ~DemuxerLock(){}

    void lock(){if(avStreamCount > 1)avLock.lock();}
    void unlock(){if(avStreamCount > 1)avLock.unlock();}
};
    
struct LUMESourceInfo {
    demuxer_t* demuxer;
    DemuxerLock* demuxerLock;
    sp<MetaData> meta;
};//LUMESourceInfo



struct AMessage;
class DataSource;
class SampleTable;
class String8;

class LUMEExtractor : public MediaExtractor {
	
public:
    // Extractor assumes ownership of "source".
    LUMEExtractor(const sp<DataSource> &source);
    
    virtual size_t countTracks();
    virtual sp<MediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);
    
    virtual sp<MetaData> getMetaData();

    virtual uint32_t flags() const;
    virtual void changeAudioTrack(int changedId);
    virtual void changeSubTrack(int changedId);
    virtual int getCurrentTrack();
    virtual int getVideoTrackCount();
    virtual int getTrackCount();
    virtual int getCharStreamCount();
    virtual int getCurrentSubTrack();
    virtual int isTsStream();
    virtual status_t getTrackInfo(char *info);
    virtual status_t getCharStreamInfo(char* info);
    virtual int getAudioTrackIndex(int id);
    virtual int getSubTrackIndex(int id);
    virtual int getFirstSubTrackIndex();
    virtual int getFirstSubTrackId();
    virtual status_t getTrackList_l();

    DemuxerLock* getDemuxerLock(){
        return &mDemuxerLock;
    }
    
    enum MediaStreamType {  
      audioType,
      videoType,
      subType,//TODO:subtitle none included??
    };
    
    typedef struct{
        uint32_t trackID;
        uint32_t streamID;
        MediaStreamType streamType;
        uint32_t audioID;//Got from sh_audio_t.aid
        uint32_t videoID;//Get from sh_video_t.vid
	    uint32_t subID;//Get from sh_sub_t.sid
        sp<MetaData> metaData;
    }TrackItem;

    bool mHasVideoSource;
protected:
    virtual ~LUMEExtractor();

private:
    sp<DataSource> mSource;
    sp<MetaData> mMeta;
	

    int mFileFormat;
    int mSubCounts;
    stream_t* mStream;
    
    static Mutex mGlobalDemuxerLock;
    DemuxerLock mDemuxerLock;
    demuxer_t* mDemuxer;

    TrackItem* mTrackList;
    uint32_t mTrackCount;
    bool mTrackCounted;
    bool mTrackListInited;
    
    LUMESourceInfo srcInfo;

    bool mHasVideo;
    bool mHasAudio;
    bool mHasSub;
    bool mStarted;

    sp<LUMEStream> mLUMEStream;
    sp<LUMEPrefetcher> mPrefetcher;

    status_t getTrackList();

    uint8_t* getAudioTrackDecoderSpecificInfo(int streamID);//passing the sh_audio_t(not the original one in demuxer) ptr to audio decoder.
    uint8_t* getVideoTrackDecoderSpecificInfo(int streamID);
    void getFileParaseExtraInfoContent(int streamID, MediaStreamType type, uint8_t* data);
    int32_t getFileParaseExtraInfoSize(int id);

    LUMEExtractor(const LUMEExtractor &);
    LUMEExtractor &operator=(const LUMEExtractor &);
};


bool SniffLUME(const sp<DataSource> &source, String8 *mimeType, float *confidence, sp<AMessage> *);

}  // namespace android

#endif  // LUME_EXTRACTOR_H_
