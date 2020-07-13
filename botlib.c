#include "botlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <stdarg.h>

#include <time.h>

Buffer Buffer$new(){
  Buffer r;
  r.cap = 16;
  r.len = 0;
  r.body = malloc(r.cap);
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
    b->cap = MAX(b->cap*2, b->len+1);
    b->body = realloc(b->body, b->cap);
    return 1;
  }
  return 0;
}

void Buffer$appendChar(Buffer* b, char c){
  b->body[b->len++] = c;
  Buffer$_realloc(b);
}

void Buffer$append(Buffer* b, char* mem, int size){
  int oldlen = b->len;
  b->len += size;
  Buffer$_realloc(b);
  memcpy(b->body+oldlen, mem, size);
}

char* Buffer$appendString(Buffer* b, char* str){
  Buffer$append(b, str, strlen(str));
  return str;
}

int Buffer$appendFile(Buffer* b, char* path){
  int retval = 1;
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
  perror("");
  return 1;
}

char* Buffer$toString(Buffer* b){
  b->body[b->len] = '\0'; // this does not change the buffer
  return b->body;
}

void Buffer$trimEnd(Buffer* b){
  while(b->len > 0 && b->body[b->len - 1] <= ' ')b->len--;
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

void Buffer$puts(Buffer* b){
  puts(Buffer$toString(b));
}

static int curlWriteHook(void* ptr, int size, int nmemb, Buffer *b){
  Buffer$append(b, ptr, size*nmemb);
  return size*nmemb;
}

char* request(char* url){
  CURL* curl = E(curl_easy_init());

  Buffer b = Buffer$new();
  Z(curl_easy_setopt(curl, CURLOPT_URL, url));
  Z(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteHook));
  Z(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b));
  Z(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 100L));
  Z(curl_easy_perform(curl));

  curl_easy_cleanup(curl);
  return Buffer$toString(&b);
  finally:
  curl_easy_cleanup(curl);
  Buffer$delete(&b);
  return NULL;
}

int printJson(cJSON* json){
  char* str = E(cJSON_Print(json));
  int parity = 1;
  for(int i = 0; str[i]; i++){
    if(str[i] == '\t'){
      putchar(' ');
      if(str[i-1] != ':')putchar(' ');
    }else if(str[i] == '"' && str[i-1]!='\\'){
      if(parity){
        if(str[i-1] == '\t' && str[i-2] != ':')printf("\e[36m\"");
        else printf("\e[32m\"");
      }
      else printf("\"\e[0m");
      parity ^= 1;
    }else putchar(str[i]);
  }
  puts("\e[0m");
  free(str);
  return 0;

  finally:
  return 1;
}

cJSON* apiRequest(char* endpoint, char* token, ...){
  Buffer b = Buffer$new();
  Buffer$appendString(&b, "https://api.vk.com/method/");
  Buffer$appendString(&b, endpoint);
  Buffer$appendString(&b, "?v=5.120&access_token=");
  Z(Buffer$appendFile(&b, token)); // todo: cache tokens
  Buffer$trimEnd(&b);

  va_list ap;
  va_start(ap, token);int i = 0;
  while(1){
    char* v1 = va_arg(ap, char*);
    if(v1 == NULL)break; // yay у нас кончились все аргументы
    char* v2 = E(va_arg(ap, char*));
    Buffer$appendChar(&b, '&');
    Buffer$appendString(&b, v1);
    Buffer$appendChar(&b, '=');
    curl_free(Buffer$appendString(&b, E(curl_easy_escape(NULL, v2, 0))));
  }
  va_end(ap);

  char* res = E(request(Buffer$toString(&b)));
  Buffer$delete(&b);
  cJSON* json = E(cJSON_Parse(res));
  free(res);

  cJSON* err = cJSON_GetObjectItemCaseSensitive(json, "error");
  if(err){
    Z(printJson(err));
    cJSON_Delete(json);
    return NULL;
  }else{
    cJSON* response = E(cJSON_DetachItemFromObjectCaseSensitive(json, "response"));
    // response = E(cJSON_Duplicate(response, 1));
    cJSON_Delete(json);
    return response;
  }

  finally:
  return NULL;
}

static Buffer* getLongpollLink(char* token, Buffer* b){
  cJSON* res = E(apiRequest(
    "messages.getLongPollServer", token,
    "lp_version", "3",
    NULL
  ));
  // Z(printJson(res));
  char* server = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "server"))));
  char* key = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "key"))));
  int ts = E(cJSON_GetObjectItemCaseSensitive(res, "ts"))->valueint;

  Buffer$reset(b);
  Buffer$printf(b, "https://%s?act=a_check&key=%s&wait=90&mode=2&version=3&ts=%d", server, key, ts);
  // Buffer$puts(b);
  cJSON_Delete(res);

  return b; // this is marginally more useful than just returning a bool
  finally:
  return NULL;
}

volatile sig_atomic_t longpollFlag;

void longpoll(char* token, JSONCallback callback){
  Buffer b = Buffer$new();
  E(getLongpollLink(token, &b));
  int ts = -1; // todo?: read file?

  longpollFlag = 1;
  while(longpollFlag){
    if(ts != -1){
      while(b.len > 0 && b.body[b.len - 1] != '=')b.len--;
      Buffer$printf(&b, "%d", ts);
    }
    char* res = E(request(Buffer$toString(&b)));
    cJSON* json = E(cJSON_Parse(res));
    free(res);
    // Z(printJson(json));

    cJSON* tsPtr = cJSON_GetObjectItemCaseSensitive(json, "ts");
    if(tsPtr)ts = tsPtr->valueint;

    cJSON* failedPtr = cJSON_GetObjectItemCaseSensitive(json, "failed");
    if(failedPtr){
      switch(failedPtr->valueint){
        default:
        printf("тут у нас соовсем какоето извращение с ошибками longpoll-а");
        Z(printJson(json));
        return;
        case 3:
        ts = -1;
        case 2:
        E(getLongpollLink(token, &b));
        case 1:;
      }
      continue;
    }

    for(cJSON* item = E(cJSON_GetObjectItemCaseSensitive(json, "updates"))->child; item; item = item->next)callback(item);

    cJSON_Delete(json);
  }

  finally:
  Buffer$delete(&b);
}

char* getRandomId(){
  static char result[15];
  static int counter = 0;
  snprintf(result, sizeof(result), "%d%d", time(NULL), counter++);
  return result;
}
