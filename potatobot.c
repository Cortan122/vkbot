#include "botlib.h"
#include "LinkedDict.h"

#define DISABLE_EDITS
#define DISABLE_FORMAT
#define USE_PTHREADS
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
#include <sys/time.h>

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

#define ATTACHMENT_LINK(emoji) \
  Buffer$printf(b, "%s https://vk.com/%s%d_%d", emoji, type, \
    E(cJSON_GetObjectItemCaseSensitive(inner, "owner_id"))->valueint, \
    E(cJSON_GetObjectItemCaseSensitive(inner, "id"))->valueint \
  )

#define ATTACHMENT_LINK_fromid(emoji) \
  Buffer$printf(b, "%s https://vk.com/%s%d_%d", emoji, type, \
    E(cJSON_GetObjectItemCaseSensitive(inner, "from_id"))->valueint, \
    E(cJSON_GetObjectItemCaseSensitive(inner, "id"))->valueint \
  )

#define ATTACHMENT_URL(emoji) \
  Buffer$printf(b, "%s %s", emoji, E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(inner, "url")))))

#define ATTACHMENT_URL_linkmp3(emoji) \
  Buffer$printf(b, "%s %s", emoji, E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(inner, "link_mp3")))))

#define ATTACHMENT_MSG(emoji, msg) if(msg){ \
  int prevlen = prefix->len; \
  if(Buffer$endsWith(b, pre))b->len -= prevlen; \
  if(prefix->len)prefix->len--; \
  Buffer$appendString(prefix, emoji); \
  formatAttachments(b, msg, prefix); \
  prefix->len = prevlen; \
  if(prefix->len)prefix->body[prefix->len - 1] = ' '; \
  pre = Buffer$toString(prefix); \
}

#define FROMAT_USER(b, obj) Buffer$printf(&b, "@%s (%s %s)", \
  E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "domain")))), \
  E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "first_name")))), \
  E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "last_name")))) \
)

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
  Buffer b2 = Buffer$new();
  Buffer$printf(&b2, "Memory usage at %s \e[33m%3.1f\e[0m%c / \e[33m%3.1f\e[0m%c ", getTimeString(), d1,c1, d2,c2);

  Buffer b = Buffer$new();
  Buffer$printf(&b, "grep VmRSS /proc/%d/status", getpid()); // todo?
  Buffer$popen(&b2, Buffer$toString(&b));
  Buffer$delete(&b);

  fprintf(stderr, "%s", Buffer$toString(&b2));
  Buffer$delete(&b2);
  fflush(stdout);
  fflush(stderr);
}

typedef struct Potato {
  char* text;
  time_t time;
  int user;
  int chat;
  int edit; // this is a bool for passing to threads
  int id; // also for passing to threads
} Potato;

LinkedDict* userDict = NULL;
LinkedDict* chatDict = NULL;

char* getUserName(int id){
  if(id == 0)return NULL;
  char* res = LinkedDict$get(chatDict, id);
  if(res)return res;

  Buffer b = Buffer$new();
  cJSON* json;
  cJSON* obj;
  if(id < 0){
    Buffer$printf(&b, "%d", -id);
    json = E(apiRequest("groups.getById", "bottoken.txt", "group_id", Buffer$toString(&b), NULL));
    obj = E(cJSON_GetArrayItem(json, 0));
    Buffer$reset(&b);
    Buffer$printf(&b, "https://vk.com/%s/",
      E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(obj, "screen_name"))))
    );
  }else{
    Buffer$printf(&b, "%d", id);
    json = E(apiRequest("users.get", "bottoken.txt", "user_ids", Buffer$toString(&b), "fields", "domain", NULL));
    obj = E(cJSON_GetArrayItem(json, 0));
    Buffer$reset(&b);
    FROMAT_USER(b, obj);
  }

  cJSON_Delete(json);
  res = Buffer$toString(&b);
  LinkedDict$add(chatDict, id, res);
  finally:
  if(res == NULL)Buffer$delete(&b);
  return res;
}

char* getChatName(int id){
  if(id == 0)return "какаято непонятная беседа";
  if(id > 2000000000)id -= 2000000000;
  else return getUserName(id);
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

int formatCopyrightString(Buffer* b, int user, int chat, time_t date){
  time_t rawtime = time(NULL);
  struct tm* today = localtime(&rawtime);
  int today_tm_yday = today->tm_yday;
  int today_tm_year = today->tm_year;
  struct tm* posttime = localtime(&date);

  Buffer$appendString(b, "© ");
  Buffer$appendString(b, E(getUserName(user)));
  if(posttime->tm_yday != today_tm_yday || posttime->tm_year != today_tm_year){
    Buffer$printf(b, " %02d.%02d.%04d", posttime->tm_mday, posttime->tm_mon+1, posttime->tm_year+1900);
  }
  Buffer$printf(b, " %d:%02d ", posttime->tm_hour, posttime->tm_min);
  if(chat != user)Buffer$printf(b, "(%s) ", E(getChatName(chat)));

  return 0;
  finally:
  return 1;
}

char* getBestPhotoUrl(cJSON* arr){
  char* rom = "smxyzw";
  char* res = NULL;
  int resIndex = -1;
  cJSON* e;
  cJSON_ArrayForEach(e, arr){
    char type = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(e, "type"))))[0];
    int i = 0;
    while(rom[i] && rom[i] != type)i++;
    if(!rom[i])i = -1;
    if(resIndex < i){
      resIndex = i;
      res = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(e, "url"))));
    }
  }
  return res;
  finally:
  return NULL;
}

void formatAttachments(Buffer* b, cJSON* json, Buffer* prefix){
  char* pre = Buffer$toString(prefix);

  char* text = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(json, "text"))));
  if(text[0]){
    Buffer$untrim(b);
    Buffer$appendString(b, pre);
  }
  for(int i = 0; text[i]; i++){
    Buffer$appendChar(b, text[i]);
    if(text[i] == '\n')Buffer$appendString(b, pre);
  }
  if(text[0])Buffer$printf(b, "\n%s\n%s", pre, pre);

  cJSON* reply = cJSON_GetObjectItemCaseSensitive(json, "reply_message");
  ATTACHMENT_MSG("↩ ", reply);

  cJSON* fwd = cJSON_GetObjectItemCaseSensitive(json, "fwd_messages");
  cJSON* msg;
  cJSON_ArrayForEach(msg, fwd){
    ATTACHMENT_MSG("➕ ", msg);
  }

  cJSON* attachments = cJSON_GetObjectItemCaseSensitive(json, "attachments");
  cJSON* attachment;
  cJSON_ArrayForEach(attachment, attachments){
    if(!Buffer$endsWith(b, pre)){
      Buffer$untrim(b);
      Buffer$appendString(b, pre);
    }

    char* type = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(attachment, "type"))));
    cJSON* inner = E(cJSON_GetObjectItemCaseSensitive(attachment, type));
    if(strcmp(type, "photo") == 0){
      Buffer$printf(b, "🌅 %s",
        E(getBestPhotoUrl(E(cJSON_GetObjectItemCaseSensitive(E(cJSON_GetObjectItemCaseSensitive(attachment, type)), "sizes"))))
      );
    }else if(strcmp(type, "video") == 0){
      ATTACHMENT_LINK("📽");
    }else if(strcmp(type, "doc") == 0){
      ATTACHMENT_LINK("📄"); // or ATTACHMENT_URL("📄");
    }else if(strcmp(type, "graffiti") == 0){
      ATTACHMENT_LINK("🎨"); // or ATTACHMENT_URL("🎨");
    }else if(strcmp(type, "poll") == 0){
      ATTACHMENT_LINK("✅");
    }else if(strcmp(type, "audio") == 0){
      ATTACHMENT_URL("🎵");
    }else if(strcmp(type, "link") == 0){
      ATTACHMENT_URL("🔗");
    }else if(strcmp(type, "wall") == 0){
      ATTACHMENT_LINK_fromid("🧱");
    }else if(strcmp(type, "audio_message") == 0){
      ATTACHMENT_URL_linkmp3("🎤"); // todo: attach
    }else if(strcmp(type, "sticker") == 0){
      Buffer$printf(b, "➕ https://vk.com/sticker/1-%d-512b",
        E(cJSON_GetObjectItemCaseSensitive(E(cJSON_GetObjectItemCaseSensitive(attachment, type)), "sticker_id"))->valueint
      );
    }else{
      // audio_playlist
      Buffer$printf(b, "❓ какойето непонятное вложение типа \"%s\"", type);
      printJson(attachment);
    }
  }

  if(strlen(pre)){
    if(!Buffer$endsWith(b, pre)){
      Buffer$untrim(b);
      Buffer$appendString(b, pre);
    }
    cJSON* peer_id = cJSON_GetObjectItemCaseSensitive(json, "peer_id");
    Z(formatCopyrightString(b,
      E(cJSON_GetObjectItemCaseSensitive(json, "from_id"))->valueint,
      peer_id ? peer_id->valueint : 0,
      E(cJSON_GetObjectItemCaseSensitive(json, "date"))->valueint
    ));
  }else{
    Buffer$untrim(b);
    // Buffer$puts(b);
  }

  finally:;
}

void* formatAttachments_thread(void* voidptr){
  Potato* p = voidptr;
  cJSON* response = NULL;
  Buffer b = Buffer$new();

  struct timeval start;
  gettimeofday(&start, NULL);

  Buffer$printf(&b, "%d", p->id);
  response = E(apiRequest(
    "messages.getById", "token.txt",
    "message_ids", Buffer$toString(&b),
    "fields", "domain",
    "preview_length", "0",
    "extended", "1",
    NULL
  ));
  Buffer$reset(&b);

  cJSON* profiles = cJSON_GetObjectItemCaseSensitive(response, "profiles");
  cJSON* profile;
  cJSON_ArrayForEach(profile, profiles){
    int id = E(cJSON_GetObjectItemCaseSensitive(profile, "id"))->valueint;
    if(LinkedDict$get(chatDict, id))continue;
    Buffer userBuff = Buffer$new();
    FROMAT_USER(userBuff, profile);
    LinkedDict$add(chatDict, id, Buffer$toString(&userBuff));
  }

  Buffer prefixBuffer = Buffer$new();
  formatAttachments(&b, E(cJSON_GetArrayItem(E(cJSON_GetObjectItemCaseSensitive(response, "items")), 0)), &prefixBuffer);
  Buffer$delete(&prefixBuffer);

  struct timeval stop;
  gettimeofday(&stop, NULL);
  printf("Formatting a long message \e[35mid%d\e[0m from \e[32m%9d\e[0m took \e[34m%6.3f\e[0ms for \e[33m%3d\e[0m bytes at %s\n",
    p->id,
    p->user,
    ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000.0,
    b.len,
    getTimeString()
  );
  fflush(stdout);

  free(p->text);
  p->text = Buffer$toString(&b);
  cJSON_Delete(response);
  return NULL;

  finally:
  Buffer$delete(&b);
  cJSON_Delete(response);
  return NULL;
}

Potato* Potato$new(cJSON* json){
  Potato* p = malloc(sizeof(Potato));

  cJSON* str = E(cJSON_GetArrayItem(json, 5));
  Z(!cJSON_IsString(str));
  p->text = E(str->valuestring);
  str->valuestring = NULL;

  p->id = E(cJSON_GetArrayItem(json, 1))->valueint;
  p->time = E(cJSON_GetArrayItem(json, 4))->valueint;

  int t = E(cJSON_GetArrayItem(json, 3))->valueint;
  if(t > 2000000000){
    p->user = atoi(E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(E(cJSON_GetArrayItem(json, 6)), "from")))));
    p->chat = t;
  }else{
    int flags = E(cJSON_GetArrayItem(json, 2))->valueint;
    p->user = (flags&2) ? MY_ID : t;
    p->chat = t;
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
    #ifndef DISABLE_FORMAT
      if(!isSticker || cJSON_GetObjectItemCaseSensitive(attachments, "reply")){
        START_THREAD(formatAttachments_thread, p);
      }
    #endif
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

void* sendPotato_thread(void* voidptr){
  Potato* p = voidptr;

  if(p == NULL || p == NULL+1){
    if(p)return NULL; // todo?
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
  Z(formatCopyrightString(&b, p->user, p->chat, p->time));
  if(p->edit)Buffer$appendString(&b, "[edit]");

  sendMessage(BOT_TOK, BOT_DEST, "disable_mentions", "1", "message", Buffer$toString(&b));

  finally:
  Buffer$delete(&b);
  Potato$delete(p);
  return NULL;
}

void sendPotato(Potato* p, int edit){
  if(p){
    if(p->user == MY_ID && p->chat != 2000000008)return;
    p->edit = edit;
    printf("A message \e[35mid%d\e[0m from \e[32m%d\e[0m just got deleted at %s\n", p->id, p->user, getTimeString());
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
    Potato* old = Bag$get(potatoBag, id);
    #ifndef DISABLE_EDITS
      if(old == NULL || strcmp(E(cJSON_GetStringValue(E(cJSON_GetArrayItem(json, 5)))), old->text) != 0){
        sendPotato(old, 1);
        old = E(Potato$new(json));
      }
    #endif
    Bag$add(potatoBag, id, old);
  }

  finally:
  return;
}

void potato_init(){
  potatoBag = Bag$new();
  userDict = LinkedDict$new(0, NULL);
  chatDict = LinkedDict$new(0, NULL);

  printf("Starting \x1b[93mpotatobot\x1b[0m at %s\n", getTimeString());
  fflush(stdout);
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
