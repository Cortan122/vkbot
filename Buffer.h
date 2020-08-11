#ifndef NO_DEBUG
  #define THROW(str,type,val) ({fprintf( \
      stderr, \
      "\x1b[33m%s\x1b[0m returned \x1b[35m"type"\x1b[0m on line \x1b[32m%d\x1b[0m in \x1b[33m%s\x1b[0m()\n", \
      str, \
      val, \
      __LINE__, \
      __FUNCTION__ \
    ); fflush(stderr); goto finally;})
  #define E(code) (code ?: (THROW(#code,"%s","NULL"),(__typeof__(code))NULL))
  #define Z(code) (({__typeof__(code) t = code; if(t)THROW(#code,"%d",t);}),0)
#else
  #define E(x) x
  #define Z(x) x
#endif

#define MIN(a,b) ({ \
  __typeof__(a) a1 = (a); \
  __typeof__(b) b1 = (b); \
  a1 < b1 ? a1 : b1; \
})
#define MAX(a,b) ({ \
  __typeof__(a) a1 = (a); \
  __typeof__(b) b1 = (b); \
  a1 > b1 ? a1 : b1; \
})

#define _NUM(x) #x
#define NUM(x) _NUM(x)

#define STARTS_WITH(haystack, needle) (strlen(haystack) >= strlen(needle) && strncmp(haystack, needle, strlen(needle)) == 0)

typedef struct Buffer {
  char* body;
  int cap;
  int len;
} Buffer;

Buffer Buffer$new();
void Buffer$delete(Buffer* b);
void Buffer$reset(Buffer* b);
void Buffer$appendChar(Buffer* b, char c);
void Buffer$append(Buffer* b, char* mem, int size);
char* Buffer$appendString(Buffer* b, char* str);
int Buffer$appendFile(Buffer* b, char* path);
char* Buffer$toString(Buffer* b);
void Buffer$trimEnd(Buffer* b);
void Buffer$untrim(Buffer* b);
void Buffer$printf(Buffer* b, char* format, ...);
int Buffer$popen(Buffer* b, char* command);
void Buffer$puts(Buffer* b);
int Buffer$endsWith(Buffer* b, char* needle);
