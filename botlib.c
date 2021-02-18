#include "botlib.h"
#include "LinkedDict.h"

#define USE_RATE_LIMIT
#define UNSAFE_ENDPOINT "messages.getById"
// #define LOG_REQUEST_TIMES

#include <stdlib.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stdbool.h>

#ifdef LOG_REQUEST_TIMES
  #include <sys/time.h>
#endif

#define CHECK_FILE(ext) ({ \
  Buffer$printf(&b, "%s/%s%s", exe, name, ext); \
  if(access(Buffer$toString(&b), R_OK) != -1)return Buffer$toString(&b); \
  Buffer$reset(&b); \
})

#define CHECK_DIR(ext) ({ \
  int len = strlen(exe); \
  while(len > 1){ \
    while(len && exe[len-1] != '/')exe[--len] = '\0'; \
    while(len && exe[len-1] == '/')exe[--len] = '\0'; \
    CHECK_FILE(""); \
    if(ext)CHECK_FILE(ext); \
  } \
})

static LinkedDict* tokenDict = NULL;

static int hashString(char* str){
  int res = 5381;
  for(int c; c = *str++;)res = (res << 5) + res + c;
  return res;
}

char* find(char* name, char* ext){
  if(ext && ext[0] == '\0')ext = NULL;

  char exe[PATH_MAX];
  memset(exe, 0, PATH_MAX); // make valgrind happy
  Buffer b = Buffer$new();

  E(readlink("/proc/self/exe", exe, sizeof(exe)-1));
  CHECK_DIR(ext);
  E(realpath(".", exe));
  CHECK_DIR(ext);

  finally:
  Buffer$delete(&b);
  fprintf(stderr, "can't find \x1b[32m'%s'\x1b[0m\n", name);
  fflush(stderr);
  return NULL;
}

char* findToken(char* name){
  if(tokenDict == NULL)tokenDict = LinkedDict$new(0, NULL);
  int hash = hashString(name);
  char* res = LinkedDict$get(tokenDict, hash);
  if(res)return res;

  char* path = E(find(name, ".txt"));

  Buffer resbuf = Buffer$new();
  Z(Buffer$appendFile(&resbuf, path));
  free(path);
  Buffer$trimEnd(&resbuf);
  LinkedDict$add(tokenDict, hash, Buffer$toString(&resbuf));
  return Buffer$toString(&resbuf);

  finally:
  return NULL;
}

static int curlWriteHook(void* ptr, int size, int nmemb, Buffer *b){
  Buffer$append(b, ptr, size*nmemb);
  return size*nmemb;
}

static char* guessAppropriateFilename(char* data){
  // todo: look at mimetype? use file(1)¿¿ libmagic
  // todo?: get real mimetype from curl
  // source: https://en.wikipedia.org/wiki/List_of_file_signatures
  if(STARTS_WITH(data, "\x89PNG\x0D\x0A\x1A\x0A"))return "image.png";
  if(STARTS_WITH(data, "\xFF\xD8\xFF"))return "photo.jpg";
  if(STARTS_WITH(data, "GIF8"))return "animation.gif";
  if(STARTS_WITH(data, "%PDF-"))return "document.pdf";
  if(STARTS_WITH(data, "PK\x03\x04"))return "archive.zip.txt";
  return "unknown file.txt";
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
  if(!system("ping -c 1 1.1.1.1 >/dev/null"))return;
  fprintf(stderr, "Waiting for \e[41minternet\e[0m at %s\n", getTimeString());
  fflush(stderr);
  while(system("ping -c 1 1.1.1.1 >/dev/null"));
  fprintf(stderr, "Done waiting for \e[41minternet\e[0m at %s\n", getTimeString());
  fflush(stderr);
}

static Buffer _request_impl(char* url, int post, ...){
  #ifdef LOG_REQUEST_TIMES
    struct timeval start;
    gettimeofday(&start, NULL);
  #endif

  CURL* curl = NULL;
  curl_mime* form = NULL;
  curl = E(curl_easy_init());

  Buffer b = Buffer$new();
  if(post == 1){
    // may (will) segfault if url is pointing to rom
    char* urlargs = url;
    while(*urlargs && *urlargs != '?')urlargs++;
    if(*urlargs == '?'){
      *urlargs = '\0';
      urlargs++;
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, urlargs);
    }
  }else if(post == 2){
    va_list ap;
    va_start(ap, post);
    char* name = E(va_arg(ap, char*));
    char* path = E(va_arg(ap, char*));
    va_end(ap);

    form = E(curl_mime_init(curl));
    curl_mimepart* field = E(curl_mime_addpart(form));
    Z(curl_mime_name(field, name));

    if(access(path, R_OK)){
      if(!STARTS_WITH(path, "https://") && !STARTS_WITH(path, "http://")){
        perror(path);
        goto finally;
      }
      Buffer urlres = _request_impl(path, 0);
      Z(urlres.len < 0);
      Z(curl_mime_data(field, urlres.body, urlres.len));
      Z(curl_mime_filename(field, guessAppropriateFilename(urlres.body)));
    }else{
      Z(curl_mime_filedata(field, path));
    }
    Z(curl_easy_setopt(curl, CURLOPT_MIMEPOST, form));
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
  curl_mime_free(form);

  #ifdef LOG_REQUEST_TIMES
    struct timeval stop;
    gettimeofday(&stop, NULL);
    printf("request: %s \e[34m%6.3f\e[0ms\n",
      url,
      ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000.0
    );
  #endif

  Buffer$toString(&b);
  return b;
  finally:
  curl_easy_cleanup(curl);
  curl_mime_free(form);
  Buffer$delete(&b);
  b.len = -1;
  return b;
}

char* request(char* url){
  return _request_impl(url, 0).body;
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

static time_t lastApiRequestTime = 0;

static cJSON* _apiRequest_impl(Buffer* b, char* endpoint){
  // todo: fixme
  #ifdef USE_RATE_LIMIT
    #ifdef LOG_REQUEST_TIMES
      int j = 0;
    #endif
    #ifdef UNSAFE_ENDPOINT
      if(strcmp(UNSAFE_ENDPOINT, endpoint) == 0)
    #endif
    while(lastApiRequestTime == time(NULL)){
      #ifdef LOG_REQUEST_TIMES
        printf("apiRequest: %s sleeping[%d]\n", endpoint, j++);
      #endif
      #ifdef USE_PTHREADS
        usleep(rand() % 10000);
      #endif
      sleep(1);
    }
    lastApiRequestTime = time(NULL);
  #endif

  char* res = E(_request_impl(Buffer$toString(b), 1).body);
  Buffer$delete(b);
  cJSON* json = E(cJSON_Parse(res));
  free(res);

  cJSON* err = cJSON_GetObjectItem(json, "error");
  if(err){
    Z(printJson(err));
    cJSON_Delete(json);
    return NULL;
  }else{
    cJSON* response = E(cJSON_DetachItemFromObjectCaseSensitive(json, "response"));
    cJSON_Delete(json);
    return response;
  }

  finally:
  return NULL;
}

#define APIREQUEST_INIT(b) Buffer$printf(&b, "https://api.vk.com/method/%s?v=5.120&access_token=%s", endpoint, E(findToken(token)));

#define APIREQUEST_ADD(b, key, val) ({ \
  Buffer$appendChar(&b, '&'); \
  Buffer$appendString(&b, key); \
  Buffer$appendChar(&b, '='); \
  curl_free(Buffer$appendString(&b, E(curl_easy_escape(NULL, val, 0)))); \
})

cJSON* apiRequest_argv(char* endpoint, char* token, char** argv){
  Buffer b = Buffer$new();
  APIREQUEST_INIT(b);

  while(1){
    char* v1 = *(argv++);
    if(v1 == NULL)break; // yay у нас кончились все аргументы
    char* v2 = *(argv++);
    APIREQUEST_ADD(b, v1, v2);
  }

  return E(_apiRequest_impl(&b, endpoint));
  finally:
  Buffer$delete(&b);
  return NULL;
}

cJSON* apiRequest(char* endpoint, char* token, ...){
  Buffer b = Buffer$new();
  APIREQUEST_INIT(b);

  va_list ap;
  va_start(ap, token);
  while(1){
    char* v1 = va_arg(ap, char*);
    if(v1 == NULL)break; // yay у нас кончились все аргументы
    char* v2 = E(va_arg(ap, char*));
    APIREQUEST_ADD(b, v1, v2);
  }
  va_end(ap);

  return E(_apiRequest_impl(&b, endpoint));
  finally:
  Buffer$delete(&b);
  return NULL;
}

cJSON* postFile(char* url, char* name, char* path){
  cJSON* json = NULL;
  char* res = E(_request_impl(url, 2, name, path).body);
  json = E(cJSON_Parse(res));
  free(res);

  if(cJSON_GetObjectItem(json, "error")){
    Z(printJson(json));
    goto finally;
  }

  return json;
  finally:
  cJSON_Delete(json);
  return NULL;
}

char* uploadFile(char* path, char* type, char* destination, char* token){
  Buffer result = Buffer$new();
  cJSON* res = NULL;
  bool isPhoto = strcmp(type, "photo") == 0;
  if(isPhoto){
    res = E(apiRequest("photos.getMessagesUploadServer", token, "peer_id", destination, NULL));
  }else{
    if(strcmp(path + strlen(path) - 4, ".mp3") == 0)type = "audio_message";
    res = E(apiRequest("docs.getMessagesUploadServer", token, "type", type, "peer_id", destination, NULL));
  }

  // todo: cache upload servers
  char* upload_url = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "upload_url"))));
  cJSON* res2 = E(postFile(upload_url, isPhoto?"photo":"file", path));
  cJSON_Delete(res);
  res = res2;

  if(isPhoto){
    Buffer$printf(&result, "%d", E(cJSON_GetObjectItem(res, "server"))->valueint);
    res2 = E(apiRequest("photos.saveMessagesPhoto", token,
      "photo", E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "photo")))),
      "hash", E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "hash")))),
      "server", Buffer$toString(&result),
      NULL
    ));
    Buffer$reset(&result);
    cJSON_Delete(res);
    res = res2;
    cJSON* arr = E(cJSON_GetArrayItem(res, 0));
    Buffer$printf(&result, "photo%d_%d_%s",
      E(cJSON_GetObjectItem(arr, "owner_id"))->valueint,
      E(cJSON_GetObjectItem(arr, "id"))->valueint,
      E(cJSON_GetStringValue(E(cJSON_GetObjectItem(arr, "access_key"))))
    );
  }else{
    res2 = E(apiRequest("docs.save", token, "file", E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "file")))), NULL));
    cJSON_Delete(res);
    res = res2;
    cJSON* arr = E(cJSON_GetObjectItem(res, type));
    Buffer$printf(&result, "doc%d_%d",
      E(cJSON_GetObjectItem(arr, "owner_id"))->valueint,
      E(cJSON_GetObjectItem(arr, "id"))->valueint
    );
  }
  cJSON_Delete(res);
  return Buffer$toString(&result);

  finally:
  Buffer$delete(&result);
  cJSON_Delete(res);
  return NULL;
}

static Buffer* getLongpollLink(char* token, Buffer* b){
  cJSON* res = E(apiRequest(
    "messages.getLongPollServer", token,
    "lp_version", "3",
    NULL
  ));
  // Z(printJson(res));
  char* server = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "server"))));
  char* key = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(res, "key"))));
  int ts = E(cJSON_GetObjectItem(res, "ts"))->valueint;

  Buffer$reset(b);
  Buffer$printf(b, "https://%s?act=a_check&key=%s&wait=90&mode=2&version=3&ts=%d", server, key, ts);
  // Buffer$puts(b);
  cJSON_Delete(res);

  return b; // this is marginally more useful than just returning a bool
  finally:
  return NULL;
}

volatile sig_atomic_t longpollFlag;

void longpollEx(char* token, JSONCallbackEx callback, void* arg){
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

    cJSON* tsPtr = cJSON_GetObjectItem(json, "ts");
    if(tsPtr)ts = tsPtr->valueint;

    cJSON* failedPtr = cJSON_GetObjectItem(json, "failed");
    if(failedPtr){
      switch(failedPtr->valueint){
        default:
        printf("тут у нас соовсем какоето извращение с ошибками longpoll-а");
        Z(printJson(json));
        return;
        case 3:
        ts = -1;
        // fallthrough
        case 2:
        E(getLongpollLink(token, &b));
        case 1:;
      }
      continue;
    }

    for(cJSON* item = E(cJSON_GetObjectItem(json, "updates"))->child; item; item = item->next)callback(item, arg);

    cJSON_Delete(json);
  }

  finally:
  Buffer$delete(&b);
}

static void longpoll_internalCallback(cJSON* json, void* arg){
  JSONCallback* callback = arg;
  callback(json);
}

void longpoll(char* token, JSONCallback callback){
  longpollEx(token, longpoll_internalCallback, callback);
}

char* getRandomId(){
  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d%03d%d", time(NULL), rand()%1000, getpid());
  return Buffer$toString(&b);
}

cJSON* gapiRequest(char* spreadsheetid, char* range, char* token){
  if(token == NULL)token = "apikey.txt";

  Buffer b = Buffer$new();
  char* range2 = E(curl_easy_escape(NULL, range, 0));
  Buffer$printf(&b, "https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s?key=%s", spreadsheetid, range2, E(findToken(token)));
  curl_free(range2);

  char* res = E(request(Buffer$toString(&b)));
  Buffer$delete(&b);
  cJSON* json = E(cJSON_Parse(res));
  free(res);

  cJSON* err = cJSON_GetObjectItem(json, "error");
  if(err){
    Z(printJson(err));
    cJSON_Delete(json);
    return NULL;
  }else{
    cJSON* response = E(cJSON_DetachItemFromObjectCaseSensitive(json, "values"));
    cJSON_Delete(json);
    return response;
  }

  finally:
  return NULL;
}
