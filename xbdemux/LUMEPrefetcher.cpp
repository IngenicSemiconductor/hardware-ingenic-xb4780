/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "LUMEPrefetcher"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "include/LUMEPrefetcher.h"

#include <media/stagefright/MediaBuffer.h>
#include <MediaDebug.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaDefs.h>
#include <utils/List.h>

#include <LUMEDefs.h>

#define EL_P1(x,y...) //LOGE(x,##y);
#define EL_P2(x,y...) //LOGE(x,##y);

namespace android {

struct LUMEPrefetchedSource : public MediaSource {
    LUMEPrefetchedSource(
            size_t index,
            const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual sp<MetaData> getFormat();

protected:
    virtual ~LUMEPrefetchedSource();

private:
    friend struct LUMEPrefetcher;

    Mutex mLock;
    Condition mCondition;

    sp<MediaSource> mSource;
    size_t mIndex;
    bool mStarted;
    bool mReachedEOS;
    status_t mFinalStatus;
    int64_t mSeekTimeUs;
    bool mSeeking;

    bool mLUMEPrefetcherStopped;
    bool mCurrentlyLUMEPrefetching;

    List<MediaBuffer *> mCachedBuffers;

    bool mReadyForCache;
    int32_t mHighWaterBufCount;
    int32_t mLowWaterBufCount; 

    void clearCache_l();
    void cacheMore();
    void onLUMEPrefetcherStopped();
    bool needCacheMore();

    LUMEPrefetchedSource(const LUMEPrefetchedSource &);
    LUMEPrefetchedSource &operator=(const LUMEPrefetchedSource &);
};

LUMEPrefetcher::LUMEPrefetcher()
    : mDone(false),
      mThreadExited(false) {
    startThread();
}

LUMEPrefetcher::~LUMEPrefetcher() {
    stopThread();
}

sp<MediaSource> LUMEPrefetcher::addSource(const sp<MediaSource> &source) {
    Mutex::Autolock autoLock(mLock);

    sp<LUMEPrefetchedSource> psource =
        new LUMEPrefetchedSource(mSources.size(), source);

    mSources.add(psource);

    return psource;
}

void LUMEPrefetcher::clearSource(){
    Mutex::Autolock autoLock(mLock);

    int i;
    for (i = 0; i < mSources.size(); i++){
      sp<LUMEPrefetchedSource> psource = mSources[i].promote();
      sp<MetaData> meta = psource->getFormat();
      const char *mime;
      CHECK(meta->findCString(kKeyMIMEType, &mime));

      if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_NB)){
	psource->clearCache_l();
      }
    }
}

void LUMEPrefetcher::startThread() {
    mThreadExited = false;
    mDone = false;

    int res = androidCreateThreadEtc(
            ThreadWrapper, this, "LUMEPrefetcher",
            ANDROID_PRIORITY_DEFAULT, 0, &mThread);

    CHECK_EQ(res, 1);
}

void LUMEPrefetcher::stopThread() {
    Mutex::Autolock autoLock(mLock);

    while (!mThreadExited) {
        mDone = true;
        mCondition.signal();
        mCondition.wait(mLock);
    }
}

// static
int LUMEPrefetcher::ThreadWrapper(void *me) {
    static_cast<LUMEPrefetcher *>(me)->threadFunc();

    return 0;
}

void LUMEPrefetcher::threadFunc() {
    bool fillingCache = false;

    for (;;) {
        int64_t minCacheDurationUs = -1;

        {
            Mutex::Autolock autoLock(mLock);
            if (mDone) {
                break;
            }

            mCondition.waitRelative(mLock, fillingCache ? 1ll : 100000000ll);//TODO: 400ms <= videosrc.mLowWaterBufCount * 1000ms/30(max_fps). The wait time also get limited to audio, which we don't know too much about it yet. need adjust later of the audio/video buffer count and the wait time.

            fillingCache = false;
            for (size_t i = 0; i < mSources.size(); ++i) {
                sp<LUMEPrefetchedSource> source = mSources[i].promote();

                if (source == NULL) {
                    continue;
                }
		
		if(source->needCacheMore()){
		  fillingCache = true;
		  source->cacheMore();
		}
             }
       }
    }

    Mutex::Autolock autoLock(mLock);
    for (size_t i = 0; i < mSources.size(); ++i) {
        sp<LUMEPrefetchedSource> source = mSources[i].promote();

        if (source == NULL) {
            continue;
        }

        source->onLUMEPrefetcherStopped();
    }

    mThreadExited = true;
    mCondition.signal();
}

////////////////////////////////////////////////////////////////////////////////

LUMEPrefetchedSource::LUMEPrefetchedSource(size_t index, const sp<MediaSource> &source)
    : mSource(source),
      mIndex(index),
      mStarted(false),
      mReachedEOS(false),
      mSeekTimeUs(-1),
      mSeeking(false),
      mLUMEPrefetcherStopped(false),
      mCurrentlyLUMEPrefetching(false),
      mReadyForCache(true),
      mHighWaterBufCount(0),
      mLowWaterBufCount(0){
}

LUMEPrefetchedSource::~LUMEPrefetchedSource() {
    if (mStarted) {
        stop();
    }
}

status_t LUMEPrefetchedSource::start(MetaData *params) {
    CHECK(!mStarted);
    
    Mutex::Autolock autoLock(mLock);

    status_t err = mSource->start(params);

    if (err != OK) {
        return err;
    }

    sp<MetaData> meta = mSource->getFormat();
    int32_t srcBufCount = 0;
    meta->findInt32(kKeyExtracterBufCount, &srcBufCount);

    mHighWaterBufCount = srcBufCount;
    mLowWaterBufCount = srcBufCount*3/4;//TODO:Adjust.

    mStarted = true;

    return OK;
}

status_t LUMEPrefetchedSource::stop() {
    CHECK(mStarted);

    Mutex::Autolock autoLock(mLock);

    while (mCurrentlyLUMEPrefetching) {
        mCondition.wait(mLock);
    }

    clearCache_l();

    status_t err = mSource->stop();

    mStarted = false;

    return err;
}

status_t LUMEPrefetchedSource::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    Mutex::Autolock autoLock(mLock);

    CHECK(mStarted);

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        clearCache_l();

        mReachedEOS = false;
        mSeekTimeUs = seekTimeUs;
	mSeeking = true;
    }

    while (!mLUMEPrefetcherStopped && !mReachedEOS && mCachedBuffers.empty()) {
	EL_P1("Warning:cached buf empty and wait to fill!");
        mCondition.wait(mLock);
    }

    if (mCachedBuffers.empty()) {
        return mReachedEOS ? mFinalStatus : ERROR_END_OF_STREAM;
    }

    *out = *mCachedBuffers.begin();
    mCachedBuffers.erase(mCachedBuffers.begin());

    return OK;
}

sp<MetaData> LUMEPrefetchedSource::getFormat() {
    return mSource->getFormat();
}

void LUMEPrefetchedSource::cacheMore() {
    MediaSource::ReadOptions options;
  
    Mutex::Autolock autoLock(mLock);

    if (!mStarted) {
        return;
    }

    mCurrentlyLUMEPrefetching = true;

    if (mSeeking) {
        options.setSeekTo(mSeekTimeUs);
        mSeeking = false;
    }

    // Ensure our object does not go away while we're not holding
    // the lock.
    sp<LUMEPrefetchedSource> me = this;

    mLock.unlock();
    MediaBuffer *buffer;
    status_t err = mSource->read(&buffer, &options);
    mLock.lock();

    if (err == NO_MEMORY) {
	mCurrentlyLUMEPrefetching = false;
	mCondition.signal();
	return;
    }

    if (err != OK) {
        mCurrentlyLUMEPrefetching = false;
        mReachedEOS = true;
        mFinalStatus = err;
        mCondition.signal();
        return;
    }

    CHECK(buffer != NULL);
    
    mCachedBuffers.push_back(buffer);
    
    mCurrentlyLUMEPrefetching = false;
    mCondition.signal();
}

void LUMEPrefetchedSource::clearCache_l() {
    List<MediaBuffer *>::iterator it = mCachedBuffers.begin();
    while (it != mCachedBuffers.end()) {
        (*it)->release();

        it = mCachedBuffers.erase(it);
    }

    mReadyForCache = true;
}

void LUMEPrefetchedSource::onLUMEPrefetcherStopped() {
    Mutex::Autolock autoLock(mLock);
    mLUMEPrefetcherStopped = true;
    mCondition.signal();
}

bool LUMEPrefetchedSource::needCacheMore() {
    Mutex::Autolock autoLock(mLock);

    if(mReachedEOS)
       return false;

    int cachedSize = mCachedBuffers.size();
    //Caching will be stopped once reach the high water.
    if(mReadyForCache){
      if(cachedSize >= mHighWaterBufCount)
	mReadyForCache = false;
    } else {//Caching will be re-started once reach the low water.
      if(cachedSize <= mLowWaterBufCount)
	mReadyForCache = true;
    }

    return mReadyForCache;
}

}  // namespace android
