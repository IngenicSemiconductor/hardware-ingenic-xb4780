#define LOG_TAG "LUMEStream"
#include <LUMEStream.h>
#include <MediaDebug.h>
#include <utils/Log.h>

namespace android{

LUMEStream::LUMEStream(DataSource* src)
    :mReadOffset(0),
     mLength(0){
    mSrc = src;
    //CHECK(src->getSize(&mLength) == OK);
    if (src->getSize(&mLength) != OK)
      ALOGE("Error: LUMEStream getSize");
}

int LUMEStream::read(void *buffer, int size, int count){
    int bytesRead = mSrc->readAt(mReadOffset, buffer, size * count);
    if(bytesRead > 0)
	mReadOffset += bytesRead;

    return bytesRead;
}

int64_t LUMEStream::seek(int64_t offset, int fromwhere){
    switch(fromwhere){
    case SEEK_SET:
	if(offset >= mLength){
	    LOGE("Error: lseek SEEK_SET to offset:%lld overrage the stream len:%lld", offset, mLength);
	    return -1;
	}
	mReadOffset = offset;
	break;
    case SEEK_CUR:
	if((offset + mReadOffset) >= mLength){
	    LOGE("Error: lseek SEEK_CUR to offset:%lld plus mreadoffset:%lld overrage the stream len:%lld", offset, mReadOffset, mLength);
	    return -1;
	}
	mReadOffset += offset;
	break;
    case SEEK_END:
	mReadOffset = mLength + offset;//FIXME: mLength + offset - 1 ??
	break;
    default:
	LOGE("Error: unrecognized seek type:%d", fromwhere);
	return -1;
    }

    return mReadOffset;
}

int64_t LUMEStream::tell(){
    return mReadOffset;
}

int64_t LUMEStream::getSize(){
    return mLength;
}

}
