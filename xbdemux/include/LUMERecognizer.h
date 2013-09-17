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

#ifndef LUME_RECOGNIZER_H_

#define LUME_RECOGNIZER_H_

#include <media/stagefright/DataSource.h>
#include <media/stagefright/Utils.h>
#include <LUMEStream.h>

namespace android {

class LUMERecognizer{
public:
    static bool is3GPFILE(LUMEStream* source);
    static bool isAMRFILE(LUMEStream* source);
    static bool isSWFFILE(LUMEStream* source);
    static bool isIMYFILE(LUMEStream* source);
    static bool isOGGFILE(LUMEStream* source);
    static bool isSystemRingToneFILE(LUMEStream* source);

    static bool recognize(const sp<DataSource> &source);

private:
    static int64_t SrcCurOffset;

};

}
#endif
