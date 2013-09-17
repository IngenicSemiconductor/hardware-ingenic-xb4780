#ifndef LUME_STREAM_H
#define LUME_STREAM_H

#include <media/stagefright/DataSource.h>
#include <utils/RefBase.h>

namespace android{

//not supposed to be thread safe.
class LUMEStream : public RefBase{
 public:    
    LUMEStream(DataSource* src);

    int read(void *buffer, int size, int count);
    int64_t seek(int64_t offset, int fromwhere);//lseek
    int64_t tell();

    int64_t getSize();

 private:
    DataSource* mSrc;
    int64_t mReadOffset;
    int64_t mLength;
};


}

#endif
