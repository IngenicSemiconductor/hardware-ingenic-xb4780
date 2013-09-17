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

#ifndef LUME_PREFETCHER_H_

#define LUME_PREFETCHER_H_

#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/threads.h>

namespace android {

struct MediaSource;
struct LUMEPrefetchedSource;

struct LUMEPrefetcher : public RefBase{
    LUMEPrefetcher();
    virtual ~LUMEPrefetcher();

    // Given an existing MediaSource returns a new MediaSource
    // that will benefit from prefetching/caching the original one.
    sp<MediaSource> addSource(const sp<MediaSource> &source);
    
    /*TODO: The prepare prefill things may could previent the blocked unstopable status at the beginning of cachemore.
    // If provided (non-NULL), "continueFunc" will be called repeatedly
    // while preparing and preparation will finish early if it returns
    // false. In this case "-EINTR" is returned as a result.
    status_t prepare(
            bool (*continueFunc)(void *cookie) = NULL,
            void *cookie = NULL);
    */
    void startThread();  //xqliang
    void clearSource();//hpwang
private:
    Mutex mLock;
    Condition mCondition;

    Vector<wp<LUMEPrefetchedSource> > mSources;
    android_thread_id_t mThread;
    bool mDone;
    bool mThreadExited;

    //void startThread();
    void stopThread();

    static int ThreadWrapper(void *me);
    void threadFunc();

    LUMEPrefetcher(const LUMEPrefetcher &);
    LUMEPrefetcher &operator=(const LUMEPrefetcher &);
};

}  // namespace android

#endif  // PREFETCHER_H_
