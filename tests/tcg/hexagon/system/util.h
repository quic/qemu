#ifndef UTIL_H
#define UTIL_H 1

typedef struct {
  int is_open;
  int buf_allocated;
  void *buf;
  unsigned mode;
  long int buf_maxsize;
  long int crnt_filesize;
  long int pos;
} FILE_MEM;

FILE_MEM *fmemopen_mem(void *buf, int max, const char *mode);
int fclose_mem(FILE_MEM *fp);
int fgetc_mem(FILE_MEM *fp);
int fread_mem(void *ptr, int max, int nmemb, FILE_MEM *fp);
int fwrite_mem(void *ptr, int max, int nmemb, FILE_MEM *fp);
int frewind_mem(FILE_MEM *fp);
long fsize_mem(FILE_MEM *fp);

#endif
