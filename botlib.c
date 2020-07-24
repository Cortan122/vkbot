#include "botlib.h"

// #define LOG_REQUEST_TIMES

#include <stdlib.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef LOG_REQUEST_TIMES
  #include <sys/time.h>
#endif

#include <unistd.h>

static int curlWriteHook(void* ptr, int size, int nmemb, Buffer *b){
  Buffer$append(b, ptr, size*nmemb);
  return size*nmemb;
}

char* getTimeString(){
  time_t rawtime = time(NULL);
  struct tm* timeinfo = localtime(&rawtime);
  static char result[30];
  strftime(result, sizeof(result), "\e[36m%d.%m.%Y %T\e[0m", timeinfo);
  return result;
}

void waitForInternet(){
  // todo? (i give up)
  fprintf(stderr, "Waiting for \e[41minternet\e[0m at %s\n", getTimeString());
  fflush(stderr);
  while(system("ping -c 1 1.1.1.1 >/dev/null"));
  fprintf(stderr, "Done waiting for \e[41minternet\e[0m at %s\n", getTimeString());
  fflush(stderr);
}

char* request(char* url, int post){
  #ifdef LOG_REQUEST_TIMES
    struct timeval start;
    gettimeofday(&start, NULL);
  #endif

  CURL* curl = E(curl_easy_init());

  Buffer b = Buffer$new();
  if(post){
    // may (will) segfault if url is pointing to rom
    char* urlargs = url;
    while(*urlargs && *urlargs != '?')urlargs++;
    if(*urlargs == '?'){
      *urlargs = '\0';
      urlargs++;
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, urlargs);
    }
  }
  Z(curl_easy_setopt(curl, CURLOPT_URL, url));
  Z(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteHook));
  Z(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b));
  Z(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 100L));
  CURLcode retcode = curl_easy_perform(curl);
  while(retcode == CURLE_OPERATION_TIMEDOUT || retcode == CURLE_COULDNT_CONNECT || retcode == CURLE_COULDNT_RESOLVE_HOST){
    fprintf(stderr, "curl request \x1b[35m%s\x1b[0m: buffer has \e[33m%d\e[0m bytes\n", curl_easy_strerror(retcode), b.len);
    fflush(stderr);
    Buffer$reset(&b);
    waitForInternet();
    retcode = curl_easy_perform(curl);
  }
  if(retcode)THROW("curl_easy_perform(curl)", "%s", curl_easy_strerror(retcode));

  curl_easy_cleanup(curl);

  #ifdef LOG_REQUEST_TIMES
    struct timeval stop;
    gettimeofday(&stop, NULL);
    printf("request: %s \e[34m%6.3f\e[0ms\n",
      url,
      ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000.0
    );
  #endif

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
  fflush(stdout);
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

  char* res = E(request(Buffer$toString(&b), 1));
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
    char* res = E(request(Buffer$toString(&b), 0));
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
  static char result[20];
  static uint8_t counter = 0;
  snprintf(result, sizeof(result), "%d%03d%d", time(NULL), counter++, getpid());
  return result;
}
