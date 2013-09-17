
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern "C"
{
#include "mp_msg.h"
#include "stream.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
}

    
#include "LUMEStream.h"
//#include "DataSource.h"
#include "LUMEDefs.h"

#include "utils/Log.h"

static struct stream_priv_s {
    char* filename;
    char *filename2;
} stream_priv_dflts = {
    NULL, NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static m_option_t stream_opts_fields[] = {
    {"string", ST_OFF(filename), CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"filename", ST_OFF(filename2), CONF_TYPE_STRING, 0, 0 ,0, NULL},
    { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static struct m_struct_st stream_opts = {
    "file",
    sizeof(struct stream_priv_s),
    &stream_priv_dflts,
    stream_opts_fields
};

#include "utils/Log.h"

using namespace android;


/* return 1 means success ,0 means failed */

//////////////////CachingDataSource fncs
static int lumestream_fill_buffer(stream_t *s, char* buffer, int max_len){

    LUMEStream* ipFilePtr = (LUMEStream*)(s->fd);
    int total = 0, result = 0;
    int size = max_len;

    while(total < max_len){
	result = ipFilePtr->read(buffer, 1, size);
	
	if(result <= 0)
	    break;

	total += result;
	size -= result;
    }

    return total;
}

static int lumestream_seek(stream_t *s,loff_t newpos) {

    LUMEStream* ipFilePtr = (LUMEStream*)(s->fd);
    
    if(ipFilePtr->seek(newpos, SEEK_SET) == -1){
	LOGE("Error: ipFilePtr->lseek newpos failed:%lld", newpos);
	//s->pos = newpos;
	s->eof=1;
	return 0;
    }
    
    s->pos = newpos;
    
    return 1;
}

static int lumestream_control(stream_t *s, int cmd, void *arg) {
    //TODO
    return STREAM_UNSUPPORTED;
}

//////////////////

static int open_f(stream_t *stream,int mode, void* opts, int* file_format) {

  LUMEStream* ipFilePtr = (LUMEStream*)(stream->fd);

  stream->end_pos = ipFilePtr->getSize();
  if(stream->end_pos <= 0){
      LOGE("Error: the got lumestream Src size is <= 0!!");
  }
  
  //LOGE("LUMEStream Length(stream->end_pos):%lld", stream->end_pos);

  stream->fill_buffer = lumestream_fill_buffer;
  stream->seek = lumestream_seek;
  stream->control = lumestream_control;
      
  m_struct_free(&stream_opts,opts);

  return STREAM_OK;
}


stream_info_t stream_info_opencore = {
  "Opencore",
  "opencore",
  "Albeu",
  "based on the code from ??? (probably Arpi)",
  open_f,
  { "file", "", NULL },
  &stream_opts,
  1 // Urls are an option string  
};
