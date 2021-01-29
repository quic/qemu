#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"

#define MAX_DESC 4

#define READ_OK   0x1
#define WRITE_OK  0x2
#define DO_READ   1
#define DO_WRITE  2

FILE_MEM desc[MAX_DESC];

#define VALID_FP(fp) ((fp) >= &desc[0] && (fp) < &desc[MAX_DESC])
#define FILE_OPEN(fP) ((fp)->is_open)

static int find_free()

{
  for (int i = 0 ; i < MAX_DESC; ++i) {
    if (!desc[i].is_open)
      return i;
  }

  return -1;
}

static void parse_mode(FILE_MEM *fp, int max, const char *mode)

{
  fp->mode = 0x0;

  for (int i = 0 ; i < strlen(mode) ; ++i) {
    if (mode[i] == '+') {
      fp->mode = READ_OK | WRITE_OK;
    } else if (tolower(mode[i]) == 'r') {
      fp->mode |= READ_OK;
      fp->crnt_filesize = max;
    } else if (tolower(mode[i]) == 'w') {
      fp->mode |= WRITE_OK;
      fp->crnt_filesize = 0;
    }
  }
}

FILE_MEM *fmemopen_mem(void *buf, int max, const char *mode)

{
  int entry = find_free();

  if (entry < 0)
      return (FILE_MEM *)NULL;

  desc[entry].is_open = 1;
  if (buf) {
    desc[entry].buf_allocated = 0;
    desc[entry].buf = buf;
  } else  {
    desc[entry].buf_allocated = 1;
    desc[entry].buf = malloc(max);
  }
  desc[entry].buf_maxsize = max;
  parse_mode(&desc[entry], max, mode);
  desc[entry].pos = 0;

  return &desc[entry];
}

int fclose_mem(FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return -1;

  if (fp->buf_allocated && fp->buf)
    free(fp->buf);

  memset(fp, 0, sizeof(FILE_MEM));
  return 0;
}

static int do_xfer(void *dst, void *src, int xferbytes, FILE_MEM *fp, int type)

{
  int checkmax = (type == DO_READ) ? fp->crnt_filesize : fp->buf_maxsize;
  if (fp->pos + xferbytes > checkmax)
    xferbytes = checkmax - fp->pos;
  if (xferbytes > 0) {
    memcpy(dst, src, xferbytes);
    fp->pos += xferbytes;
  } else
    xferbytes = 0;

  return xferbytes;
}

int fgetc_mem(FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return 0;

  if (!(fp->mode & READ_OK))
    return 0;

  if (fp->pos < fp->crnt_filesize)
    return ((unsigned char *)(fp->buf))[(fp->pos)++];
  return -1;
}

int fread_mem(void *ptr, int max, int nmemb, FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return 0;

  if (!(fp->mode & READ_OK))
    return 0;

  return do_xfer(ptr, fp->buf+fp->pos, max * nmemb, fp, DO_READ);
}

int fwrite_mem(void *ptr, int max, int nmemb, FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return 0;

  if (!(fp->mode & WRITE_OK))
    return 0;

  int rval = do_xfer(fp->buf+fp->pos, ptr, max * nmemb, fp, DO_WRITE);
  if (fp->pos > fp->crnt_filesize)
    fp->crnt_filesize = fp->pos;

  return rval;
}

int frewind_mem(FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return -1;

  fp->pos = 0;
  return 0;
}

long fsize_mem(FILE_MEM *fp)

{
  if (!VALID_FP(fp) || !FILE_OPEN(fp))
    return -1;

  return fp->crnt_filesize;
}

