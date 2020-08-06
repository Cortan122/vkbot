#include "cJSON.h"
#include "Buffer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define USE_PTHREADS
#define MY_ID 560101729

typedef void (JSONCallback)(cJSON*);

char* getTimeString();
void waitForInternet();

// char* request(char* url, int post);
int printJson(cJSON* json);
cJSON* apiRequest(char* endpoint, char* token, ...); //  __attribute__((sentinel))
cJSON* postFile(char* url, char* name, char* path);
void longpoll(char* token, JSONCallback callback);
char* getRandomId();

volatile extern sig_atomic_t longpollFlag;

#define sendMessage(token, peer_id, ...) ({ \
  char* t = getRandomId(); \
  cJSON_Delete(E(apiRequest("messages.send", token, "peer_id", peer_id, "random_id", t, __VA_ARGS__, NULL))); \
  free(t); \
})

#ifdef USE_PTHREADS
  #include <pthread.h>
  #define START_THREAD(func, arg) ({ \
    pthread_t threadId; \
    Z(pthread_create(&threadId, NULL, func, arg)); \
    Z(pthread_detach(threadId)); \
  })
#else
  #define START_THREAD(func, arg) func(arg)
#endif
