#include "botlib.h"

// #define USE_PTHREADS
// #define USE_PYBOT

#ifdef USE_PYBOT
  #define BOT_TOK "pybottoken.txt"
  #define BOT_DEST "2000000001"
#else
  #define BOT_TOK "bottoken.txt"
  #define BOT_DEST "2000000002"
#endif

#include <malloc.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

char* getTimeString(){
  time_t rawtime = time(NULL);
  struct tm* timeinfo = localtime(&rawtime);
  static char result[30];
  strftime(result, sizeof(result), "\e[36m%d.%m.%Y %T\e[0m", timeinfo);
  return result;
}

double humanredableSize(int bytes, char* out){
  static const char sizes[] = " kMGTPE";
  double len = bytes;
  int order = 0;
  while(len >= 1024 && order < sizeof(sizes) - 1){
    order++;
    len /= 1024;
  }

  *out = sizes[order];
  return len;
}

void printMallocStats(){
  struct mallinfo info = mallinfo();
  char c1, c2;
  double d1 = humanredableSize(info.uordblks, &c1);
  double d2 = humanredableSize(info.fordblks + info.uordblks, &c2);
  fprintf(stderr, "Memory usage at %s \e[33m%3.1f\e[0m%c / \e[33m%3.1f\e[0m%c\n", getTimeString(), d1,c1, d2,c2);
  // todo: /proc/self/statm
}

typedef struct LinkedDict {
  struct LinkedDict* next;
  struct LinkedDict* last; // this is correct only when this is the first node
  int key;
  char* val;
} LinkedDict;

LinkedDict* LinkedDict$new(int key, char* val){
  LinkedDict* res = malloc(sizeof(LinkedDict));
  res->next = NULL;
  res->last = res;
  res->key = key;
  res->val = val;
  return res;
}

void LinkedDict$delete(LinkedDict* d){
  LinkedDict* next;
  while(d != NULL){
    next = d->next;
    free(d->val);
    free(d);
    d = next;
  }
}

void LinkedDict$add(LinkedDict* d, int key, char* val){
  if(val == NULL)return;
  d->last = d->last->next = LinkedDict$new(key, val);
}

char* LinkedDict$get(LinkedDict* d, int key){
  while(d->next){
    if(d->key == key && d->val != NULL)return d->val;
    d = d->next;
  }
  return NULL;
}

typedef struct Potato {
  char* text;
  time_t time;
  int user;
  int chat;
  int edit; // this is a bool for passing to threads
  cJSON* json; // also for passing to threads
} Potato;

Potato* Potato$new(cJSON* json){
  Potato* p = malloc(sizeof(Potato));

  cJSON* str = E(cJSON_GetArrayItem(json, 5));
  Z(!cJSON_IsString(str));
  p->text = E(str->valuestring);
  str->valuestring = NULL;

  p->json = NULL;
  p->time = E(cJSON_GetArrayItem(json, 4))->valueint;

  int t = E(cJSON_GetArrayItem(json, 3))->valueint;
  if(t > 2000000000){
    p->user = atoi(E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(E(cJSON_GetArrayItem(json, 6)), "from")))));
    p->chat = t;
  }else{
    p->user = t;
    p->chat = 0;
  }

  cJSON* attachments = E(cJSON_GetArrayItem(json, 7));
  Z(!cJSON_IsObject(attachments));
  if(cJSON_GetArraySize(attachments)){
    char* attach1_type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(attachments, "attach1_type"));
    int isSticker = attach1_type && strcmp(attach1_type, "sticker") == 0;
    if(isSticker){
      Buffer b = Buffer$new();
      Buffer$appendString(&b, p->text);
      Buffer$untrim(&b);
      Buffer$printf(
        &b, "\n➕ https://vk.com/sticker/1-%s-512b\n",
        E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(attachments, "attach1"))))
      );
      free(p->text);
      p->text = Buffer$toString(&b);
    }
    if(!isSticker || cJSON_GetObjectItemCaseSensitive(attachments, "reply")){
      // p->json = E(cJSON_DetachItemViaPointer(json, attachments));
      // START_THREAD(, p);
    }
  }

  return p;
  finally:
  free(p);
  return NULL;
}

void Potato$delete(Potato* p){
  if(p == NULL)return;
  free(p->text);
  free(p);
}

typedef struct Bag {
  Potato** body; // the bag contains a body))
  int cap;
  int len;
  int firstIndex;
} Bag;

void Bag$_init(Bag* b){
  memset(b->body + b->len, 0, (b->cap - b->len)*sizeof(Potato*));
}

int Bag$_realloc(Bag* b, int oldlen){
  int newlen = b->len;
  if(b->len >= b->cap){
    b->cap = MAX(b->cap*2, b->len+1);
    b->body = realloc(b->body, b->cap * sizeof(Potato*));
    b->len = oldlen;
    Bag$_init(b);
    b->len = newlen;
    return 1;
  }
  return 0;
}

Bag* Bag$new(){
  Bag* b = malloc(sizeof(Bag));
  b->cap = 16;
  b->len = 0;
  b->body = malloc(b->cap * sizeof(Potato*));
  Bag$_init(b);
  b->firstIndex = -1;
  return b;
}

void Bag$delete(Bag* b){
  if(b == NULL)return;
  for(int i = 0; i < b->len; i++)Potato$delete(b->body[i]);
  free(b->body);
  free(b);
}

void Bag$add(Bag* b, int id, Potato* val){
  if(val->user < 0){
    // это сообщение от сообщества, а им нечего скрывать
    Potato$delete(val);
    return;
  }
  if(b->firstIndex == -1)b->firstIndex = id;
  id -= b->firstIndex;

  int oldlen = b->len;
  if(id >= b->len)b->len = id+1; // this is true if this is not an edit
  Bag$_realloc(b, oldlen);
  Potato$delete(b->body[id]);
  b->body[id] = val;
}

Potato* Bag$get(Bag* b, int id){
  id -= b->firstIndex;
  if(id < 0 || id >= b->len)return NULL;
  Potato* r = b->body[id];
  b->body[id] = NULL;
  return r;
}

Bag* potatoBag = NULL;
LinkedDict* userDict = NULL;
LinkedDict* chatDict = NULL;

char* getUserName(int id){
  if(id <= 0)return NULL;
  char* res = LinkedDict$get(chatDict, id);
  if(res)return res;

  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id);
  cJSON* json = E(apiRequest("users.get", "bottoken.txt", "user_ids", Buffer$toString(&b), "fields", "domain", NULL));
  cJSON* obj = E(cJSON_GetArrayItem(json, 0));
  Buffer$reset(&b);
  Buffer$printf(&b, "@%s (%s %s) ",
    E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "domain")))),
    E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "first_name")))),
    E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "last_name"))))
  );
  cJSON_Delete(json);

  res = Buffer$toString(&b);
  LinkedDict$add(chatDict, id, res);
  finally:
  if(res == NULL)Buffer$delete(&b);
  return res;
}

char* getChatName(int id){
  if(id > 2000000000)id -= 2000000000;
  char* res = LinkedDict$get(chatDict, id);
  if(res)return res;

  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id);
  cJSON* json = E(apiRequest("messages.getChat", "token.txt", "chat_id", Buffer$toString(&b), NULL));
  res = strdup(E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(json, "title")))));
  cJSON_Delete(json);

  LinkedDict$add(chatDict, id, res);
  finally:
  Buffer$delete(&b);
  return res;
}

void* sendPotato_thread(void* voidptr){
  Potato* p = voidptr;

  if(p == NULL || p == NULL+1){
    sendMessage(BOT_TOK, BOT_DEST, "message", p ?
      "ПолинаБот: *непомнит какое сообщение было отредактировано*" :
      "ПолинаБот: *непомнит какое сообщение было удалено*"
    );
    return NULL;
  }

  time_t rawtime = time(NULL);
  struct tm* today = localtime(&rawtime);
  struct tm* posttime = localtime(&p->time);

  Buffer b = Buffer$new();
  Buffer$appendString(&b, p->text);
  if(!(b.len && b.body[b.len-1] == '\n'))Buffer$appendString(&b, "\n\n");
  Buffer$appendString(&b, "© ");
  Buffer$appendString(&b, E(getUserName(p->user)));
  if(posttime->tm_yday != today->tm_yday || posttime->tm_year != today->tm_year){
    Buffer$printf(&b, "%02d.%02d.%04d ", posttime->tm_mday, posttime->tm_mon+1, posttime->tm_year+1900);
  }
  Buffer$printf(&b, "%d:%02d ", posttime->tm_hour, posttime->tm_min);
  if(p->chat)Buffer$printf(&b, "(%s) ", E(getChatName(p->chat)));
  if(p->edit)Buffer$appendString(&b, "[edit]");

  sendMessage(BOT_TOK, BOT_DEST, "disable_mentions", "1", "message", Buffer$toString(&b));

  finally:
  Buffer$delete(&b);
  Potato$delete(p);
  return NULL;
}

void sendPotato(Potato* p, int edit){
  if(p){
    if(p->user == 560101729 && p->chat != 2000000008)return;
    p->edit = edit;
    printf("A message from \e[32m%d\e[0m just got deleted at %s\n", p->user, getTimeString());
    printMallocStats();
  }else if(edit)p = NULL+1;

  START_THREAD(sendPotato_thread, p);
  return;
  finally:
  perror("");
}

void potato_callback(cJSON* json){
  int type = E(cJSON_GetArrayItem(json, 0))->valueint;
  if(type > 5)return;

  int id = E(cJSON_GetArrayItem(json, 1))->valueint;
  int flags = E(cJSON_GetArrayItem(json, 2))->valueint;
  if(type == 2 && (flags&131072)){
    sendPotato(Bag$get(potatoBag, id), 0);
  }else if(type == 4){
    Bag$add(potatoBag, id, E(Potato$new(json)));
  }else if(type == 5){
    sendPotato(Bag$get(potatoBag, id), 1);
    Bag$add(potatoBag, id, E(Potato$new(json)));
  }

  finally:
  return;
}

void potato_init(){
  potatoBag = Bag$new();
  userDict = LinkedDict$new(0, NULL);
  chatDict = LinkedDict$new(0, NULL);

  printf("Starting \x1b[93mpotatobot\x1b[0m at %s\n", getTimeString());
}

void potato_deinit(){
  Bag$delete(potatoBag);
  LinkedDict$delete(userDict);
  LinkedDict$delete(chatDict);
}

int main(){
  mallopt(M_CHECK_ACTION, 0b001);

  potato_init();
  longpoll("token.txt", potato_callback);
  potato_deinit();

  finally:
  return 0;
}
