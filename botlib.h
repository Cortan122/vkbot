#include "cJSON.h"
#include <signal.h>

#define THROW(str,type,val) ({fprintf( \
    stderr, \
    "\x1b[33m%s\x1b[0m returned \x1b[35m"type"\x1b[0m on line \x1b[32m%d\x1b[0m in \x1b[33m%s\x1b[0m()\n", \
    str, \
    val, \
    __LINE__, \
    __FUNCTION__ \
  ); goto finally;})
#define E(code) (code ?: (THROW(#code,"%s","NULL"),(__typeof__(code))NULL))
#define Z(code)  (({__typeof__(code) t = code; if(t)THROW(#code,"%d",t);}),0)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef void (JSONCallback)(cJSON*);

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
void Buffer$puts(Buffer* b);
int Buffer$endsWith(Buffer* b, char* needle);

char* getTimeString();
void waitForInternet();

char* request(char* url, int post);
int printJson(cJSON* json);
cJSON* apiRequest(char* endpoint, char* token, ...);
void longpoll(char* token, JSONCallback callback);
char* getRandomId();

volatile extern sig_atomic_t longpollFlag;

#define sendMessage(token, peer_id, ...) cJSON_Delete(E(apiRequest("messages.send", token, "peer_id", peer_id, "random_id", getRandomId(), __VA_ARGS__, NULL)))
