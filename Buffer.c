#include "Buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

Buffer Buffer$new(){
  Buffer r;
  r.cap = 0;
  r.len = 0;
  r.body = NULL;
  return r;
}

void Buffer$delete(Buffer* b){
  if(b == NULL)return;
  free(b->body);
  b->body = NULL;
}

void Buffer$reset(Buffer* b){
  b->len = 0;
}

static int Buffer$_realloc(Buffer* b){
  if(b->len >= b->cap){
    b->cap = MAX(16, MAX(b->cap*2, b->len+1));
    b->body = realloc(b->body, b->cap);
    return 1;
  }
  return 0;
}

void Buffer$appendChar(Buffer* b, char c){
  b->len++;
  Buffer$_realloc(b);
  b->body[b->len-1] = c;
}

void Buffer$append(Buffer* b, char* mem, int size){
  int oldlen = b->len;
  b->len += size;
  Buffer$_realloc(b);
  memcpy(b->body+oldlen, mem, size);
}

char* Buffer$appendString(Buffer* b, char* str){
  if(str == NULL)return NULL;
  Buffer$append(b, str, strlen(str));
  return str;
}

int Buffer$appendFile(Buffer* b, char* path){
  FILE* f = E(fopen(path, "rb"));

  if(fseek(f, 0, SEEK_END)){
    // we cant seek in this file (it is probably stdin)
    fseek(f, 0, SEEK_SET); // just in case
    int t;
    while((t = getc(f)) >= 0){
      Buffer$appendChar(b, t);
    }
    fclose(f);
    return 0;
  }
  int length = ftell(f);
  if(fseek(f, 0, SEEK_SET)){
    fclose(f);
    return 1;
  }

  int oldlen = b->len;
  b->len += length;
  Buffer$_realloc(b);
  fread(b->body+oldlen, 1, length, f);

  fclose(f);
  return 0;

  finally:
  perror(path);
  return 1;
}

char* Buffer$toString(Buffer* b){
  if(b->body == NULL)Buffer$_realloc(b);
  b->body[b->len] = '\0'; // this does not change the buffer
  return b->body;
}

void Buffer$trimEnd(Buffer* b){
  while(b->len > 0 && b->body[b->len - 1] <= ' ')b->len--;
}

void Buffer$untrim(Buffer* b){
  if(b->len && b->body[b->len-1] != '\n')Buffer$appendChar(b, '\n');
}

void Buffer$printf(Buffer* b, char* format, ...){
  int oldlen = b->len;

  va_list argptr;
  va_start(argptr, format);
  int res = vsnprintf(b->body + oldlen, b->cap - oldlen, format, argptr);
  va_end(argptr);
  b->len += res;
  if(Buffer$_realloc(b)){
    va_start(argptr, format);
    Z(res != vsnprintf(b->body + oldlen, b->cap - oldlen, format, argptr));
    va_end(argptr);
  }

  return;
  finally:
  exit(1);
}

int Buffer$popen(Buffer* b, char* command){
  char buf[100];

  FILE* proc = E(popen(command, "r"));
  while(!feof(proc)){
    if(fgets(buf, sizeof(buf), proc))Buffer$appendString(b, buf);
  }
  Z(pclose(proc));
  return 0;
  finally:
  perror("");
  return 1;
}

void Buffer$puts(Buffer* b){
  puts(Buffer$toString(b));
  fflush(stdout);
}

int Buffer$endsWith(Buffer* b, char* needle){
  int len = strlen(needle);
  if(b->len < len)return 0;
  return strcmp(needle, b->body + b->len-len) == 0;
}
