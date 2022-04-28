#pragma once

#include "cJSON.h"
#include "Buffer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define USE_PTHREADS
#define MY_ID 560101729

#define cJSON_GetObjectItem cJSON_GetObjectItemCaseSensitive

typedef void (JSONCallback)(cJSON*);
typedef void (JSONCallbackEx)(cJSON*, void*);

char* getTimeString();
void waitForInternet();
char* find(char* name, char* ext);
char* findToken(char* name);

char* request(char* url);
int printJson(cJSON* json);
cJSON* apiRequest(char* endpoint, char* token, ...); //  __attribute__((sentinel))
cJSON* apiRequest_argv(char* endpoint, char* token, char** argv);
cJSON* postFile(char* url, char* name, char* path);
char* uploadFile(char* path, char* type, char* destination, char* token);
void longpoll(char* token, JSONCallback callback);
void longpollEx(char* token, JSONCallbackEx callback, void* arg);
char* getRandomId();

cJSON* gapiRequest(char* spreadsheetid, char* range, char* token);

typedef struct TelegramAttachment {
  char* type;
  char* path;
} TelegramAttachment;

#define getNumber(json, name) (int64_t)E(cJSON_GetObjectItem(json, name))->valuedouble
cJSON* tgapiRequest(char* endpoint, char* token, TelegramAttachment* attachments, ...);
void tglongpoll(char* token, JSONCallback callback);

extern volatile sig_atomic_t longpollFlag;

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
