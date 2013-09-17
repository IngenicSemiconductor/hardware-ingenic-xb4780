#include "pvfile.h"


#ifdef __cplusplus
extern "C"
{
  #include "stream.h"
}
#endif


static int fill_buffer(stream_t *s, char* buffer, int max_len)
{
  //  printf("fill_buffer: in\n");
    PVFile* pvf = (PVFile*)s->fd;

    int size = pvf->Read(buffer, sizeof(char), max_len);
    
    //    printf("fill_buffer: out\n");

    return (size <= 0) ? -1 : (size * (sizeof(char)));
}

/*
static int write_buffer(stream_t *s, char* buffer, int len) {
  int r = write(s->fd,buffer,len);
  return (r <= 0) ? -1 : r;
}
*/

static int seek(stream_t *s,loff_t newpos) 
{
    //   printf("pvfile seek: in. newpos = %d\n", newpos); 
  PVFile* pvf = (PVFile*)s->fd;

  s->pos = newpos;

  if (pvf->Seek(s->pos, Oscl_File::SEEKSET) != 0)
  {
    s->eof = 1;
    return 0;
  }
  
//  printf("pvfile seek: out\n");
  
  return 1;
}

/*
static int seek_forward(stream_t *s,loff_t newpos) {
  if(newpos < s->pos){
    return 0;
  }

  while(s->pos < newpos){
    int len=s->fill_buffer(s,s->buffer,STREAM_BUFFER_SIZE);
    if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; break; } // EOF
    s->buf_pos=0;
    s->buf_len=len;
    s->pos+=len;
  }
  return 1;
}
*/


static int control(stream_t *s, int cmd, void *arg) 
{
  switch(cmd) 
  {
    case STREAM_CTRL_GET_SIZE: 
    {
      loff_t size;

      PVFile* pvf = (PVFile*)s->fd;

      size = pvf->Seek(0, Oscl_File::SEEKEND);
      pvf->Seek(s->pos, Oscl_File::SEEKSET);
      if (size != (loff_t)-1)
      {
	*((loff_t*)arg) = size;
        return 1;
      }
    }
  }

  return STREAM_UNSUPPORTED;
}

static int open_pvf(stream_t *stream, int mode, void* opts, int* file_format)
{
  stream->type = STREAMTYPE_PVFILE;
  
  stream->seek = seek; 
  stream->control = control;
  stream->fill_buffer = fill_buffer;
  stream->eof = 0;
  stream->close = NULL;

  return STREAM_OK;
}

//extern "C" 
stream_info_t stream_info_pvfile = {
  "pvFile",
  "pvfile",
  "lanchb",
  "for opencore",
  open_pvf,
  {"", NULL},
  NULL,
  1 // Urls are an option string
};

