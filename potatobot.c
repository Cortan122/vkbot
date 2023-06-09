#include "botlib.h"
#include "LinkedDict.h"
#include "extralib.h"

#define DISABLE_EDITS
// #define DISABLE_FORMAT
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

#define ATTACHMENT_LINK(emoji) \
  Buffer$printf(b, "%s https://vk.com/%s%d_%d", emoji, type, \
    E(cJSON_GetObjectItem(inner, "owner_id"))->valueint, \
    E(cJSON_GetObjectItem(inner, "id"))->valueint \
  )

#define ATTACHMENT_LINK_fromid(emoji) \
  Buffer$printf(b, "%s https://vk.com/%s%d_%d", emoji, type, \
    E(cJSON_GetObjectItem(inner, "from_id"))->valueint, \
    E(cJSON_GetObjectItem(inner, "id"))->valueint \
  )

#define ATTACHMENT_URL(emoji) \
  Buffer$printf(b, "%s %s", emoji, E(cJSON_GetStringValue(E(cJSON_GetObjectItem(inner, "url")))))

#define ATTACHMENT_URL_linkmp3(emoji) \
  Buffer$printf(b, "%s %s", emoji, E(cJSON_GetStringValue(E(cJSON_GetObjectItem(inner, "link_mp3")))))

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
  E(cJSON_GetStringValue(E(cJSON_GetObjectItem(obj, "domain")))), \
  E(cJSON_GetStringValue(E(cJSON_GetObjectItem(obj, "first_name")))), \
  E(cJSON_GetStringValue(E(cJSON_GetObjectItem(obj, "last_name")))) \
)

#define APPEND_TO_POTATO_TEXT(...) ({ \
  Buffer b = Buffer$new(); \
  Buffer$appendString(&b, p->text); \
  Buffer$untrim(&b); \
  Buffer$printf(&b, __VA_ARGS__); \
  free(p->text); \
  p->text = Buffer$toString(&b); \
})

static double humanredableSize(int bytes, char* out){
  static const char sizes[] = " kMGTPE";
  double len = bytes;
  unsigned int order = 0;
  while(len >= 1024 && order < sizeof(sizes) - 1){
    order++;
    len /= 1024;
  }

  *out = sizes[order];
  return len;
}

static void printMallocStats(){
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  struct mallinfo info = mallinfo();
  #pragma GCC diagnostic pop
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

LinkedDict* chatDict = NULL;

static char* getUserName(int id){
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
      E(cJSON_GetStringValue(E(cJSON_GetObjectItem(obj, "screen_name"))))
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

static char* getChatName(int id){
  if(id == 0)return "какаято непонятная беседа";
  else if(id < 2000000000)return getUserName(id);
  char* res = LinkedDict$get(chatDict, id);
  if(res)return res;

  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id-2000000000);
  cJSON* json = E(apiRequest("messages.getChat", "token.txt", "chat_id", Buffer$toString(&b), NULL));
  res = strdup(E(cJSON_GetStringValue(E(cJSON_GetObjectItem(json, "title")))));
  cJSON_Delete(json);

  LinkedDict$add(chatDict, id, res);
  finally:
  Buffer$delete(&b);
  return res;
}

static int formatCopyrightString(Buffer* b, int user, int chat, time_t date){
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

static void formatAttachments(Buffer* b, cJSON* json, Buffer* prefix){
  char* pre = Buffer$toString(prefix);

  char* text = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(json, "text"))));
  if(text[0]){
    Buffer$untrim(b);
    Buffer$appendString(b, pre);
  }
  for(int i = 0; text[i]; i++){
    Buffer$appendChar(b, text[i]);
    if(text[i] == '\n')Buffer$appendString(b, pre);
  }
  if(text[0])Buffer$printf(b, "\n%s\n%s", pre, pre);

  cJSON* reply = cJSON_GetObjectItem(json, "reply_message");
  ATTACHMENT_MSG("↩ ", reply);

  cJSON* fwd = cJSON_GetObjectItem(json, "fwd_messages");
  cJSON* msg;
  cJSON_ArrayForEach(msg, fwd){
    ATTACHMENT_MSG("➕ ", msg);
  }

  cJSON* attachments = cJSON_GetObjectItem(json, "attachments");
  cJSON* attachment;
  cJSON_ArrayForEach(attachment, attachments){
    if(!Buffer$endsWith(b, pre)){
      Buffer$untrim(b);
      Buffer$appendString(b, pre);
    }

    char* type = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(attachment, "type"))));
    cJSON* inner = E(cJSON_GetObjectItem(attachment, type));
    if(strcmp(type, "photo") == 0){
      Buffer$printf(b, "🌅 %s",
        E(getBestPhotoUrl(E(cJSON_GetObjectItem(E(cJSON_GetObjectItem(attachment, type)), "sizes"))))
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
        E(cJSON_GetObjectItem(E(cJSON_GetObjectItem(attachment, type)), "sticker_id"))->valueint
      );
    }else{
      // audio_playlist, call
      Buffer$printf(b, "❓ какойето непонятное вложение типа \"%s\"", type);
      printJson(attachment);
    }
  }

  if(strlen(pre)){
    if(!Buffer$endsWith(b, pre)){
      Buffer$untrim(b);
      Buffer$appendString(b, pre);
    }
    cJSON* peer_id = cJSON_GetObjectItem(json, "peer_id");
    Z(formatCopyrightString(b,
      E(cJSON_GetObjectItem(json, "from_id"))->valueint,
      peer_id ? peer_id->valueint : 0,
      E(cJSON_GetObjectItem(json, "date"))->valueint
    ));
  }else{
    Buffer$untrim(b);
    // Buffer$puts(b);
  }

  finally:;
}

static void* formatAttachments_thread(void* voidptr){
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

  cJSON* profiles = cJSON_GetObjectItem(response, "profiles");
  cJSON* profile;
  cJSON_ArrayForEach(profile, profiles){
    int id = E(cJSON_GetObjectItem(profile, "id"))->valueint;
    if(LinkedDict$get(chatDict, id))continue;
    Buffer userBuff = Buffer$new();
    FROMAT_USER(userBuff, profile);
    LinkedDict$add(chatDict, id, Buffer$toString(&userBuff));
  }

  cJSON* groups = cJSON_GetObjectItem(response, "groups");
  cJSON* group;
  cJSON_ArrayForEach(group, groups){
    int id = -E(cJSON_GetObjectItem(group, "id"))->valueint;
    if(LinkedDict$get(chatDict, id))continue;
    Buffer userBuff = Buffer$new();
    Buffer$printf(&userBuff, "https://vk.com/%s/",
      E(cJSON_GetStringValue(E(cJSON_GetObjectItem(group, "screen_name"))))
    );
    LinkedDict$add(chatDict, id, Buffer$toString(&userBuff));
  }

  Buffer prefixBuffer = Buffer$new();
  formatAttachments(&b, E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(response, "items")), 0)), &prefixBuffer);
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

static Potato* Potato$new(cJSON* json){
  Potato* p = malloc(sizeof(Potato));

  cJSON* str = E(cJSON_GetArrayItem(json, 5));
  Z(!cJSON_IsString(str));
  p->text = E(str->valuestring);
  str->valuestring = NULL;

  p->id = E(cJSON_GetArrayItem(json, 1))->valueint;
  p->time = E(cJSON_GetArrayItem(json, 4))->valueint;

  int t = E(cJSON_GetArrayItem(json, 3))->valueint;
  if(t > 2000000000){
    p->user = atoi(E(cJSON_GetStringValue(E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 6)), "from")))));
    p->chat = t;
  }else{
    int flags = E(cJSON_GetArrayItem(json, 2))->valueint;
    p->user = (flags&2) ? MY_ID : t;
    p->chat = t;
  }

  cJSON* attachments = E(cJSON_GetArrayItem(json, 7));
  Z(!cJSON_IsObject(attachments));
  if(cJSON_GetArraySize(attachments)){
    char* attach1_type = cJSON_GetStringValue(cJSON_GetObjectItem(attachments, "attach1_type"));
    int isSticker = attach1_type && strcmp(attach1_type, "sticker") == 0;
    if(isSticker){
      APPEND_TO_POTATO_TEXT(
        "\n➕ https://vk.com/sticker/1-%s-512b\n",
        E(cJSON_GetStringValue(E(cJSON_GetObjectItem(attachments, "attach1"))))
      );
    }
    if(!isSticker || cJSON_GetObjectItem(attachments, "reply")){
      #ifndef DISABLE_FORMAT
        START_THREAD(formatAttachments_thread, p);
      #else
        char* t = E(cJSON_Print(attachments));
        APPEND_TO_POTATO_TEXT("\n🤐 %s\n", t);
        free(t);
      #endif
    }
  }

  return p;
  finally:
  free(p);
  return NULL;
}

static void Potato$delete(Potato* p){
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

static void Bag$_init(Bag* b){
  memset(b->body + b->len, 0, (b->cap - b->len)*sizeof(Potato*));
}

static int Bag$_realloc(Bag* b, int oldlen){
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

static Bag* Bag$new(){
  Bag* b = malloc(sizeof(Bag));
  b->cap = 16;
  b->len = 0;
  b->body = malloc(b->cap * sizeof(Potato*));
  Bag$_init(b);
  b->firstIndex = -1;
  return b;
}

static void Bag$delete(Bag* b){
  if(b == NULL)return;
  for(int i = 0; i < b->len; i++)Potato$delete(b->body[i]);
  free(b->body);
  free(b);
}

static void Bag$add(Bag* b, int id, Potato* val){
  if(val->user < 0){
    // это сообщение от сообщества, а им нечего скрывать
    // но если это крутое сообщение со всякими вложениями и у нас включены потоки
    // когда мы его удалим тут, у нас всё сломается (тк второй поток будет пытатся красиво оформить текст)
    #if !defined(USE_PTHREADS) || defined(DISABLE_FORMAT)
      Potato$delete(val);
      return;
    #endif
  }
  if(b->firstIndex == -1)b->firstIndex = id;
  id -= b->firstIndex;
  if(id < 0)return; // editted unknown message -- ignored (todo: fix leak)

  int oldlen = b->len;
  if(id >= b->len)b->len = id+1; // this is true if this is not an edit
  Bag$_realloc(b, oldlen);
  Potato$delete(b->body[id]);
  b->body[id] = val;
}

static Potato* Bag$get(Bag* b, int id){
  id -= b->firstIndex;
  if(id < 0 || id >= b->len)return NULL;
  Potato* r = b->body[id];
  b->body[id] = NULL;
  return r;
}

Bag* potatoBag = NULL;

static void* sendPotato_thread(void* voidptr){
  Potato* p = voidptr;

  if(p == NULL || p == NULL+1){
    if(p)return NULL; // todo?
    sendMessage(BOT_TOK, BOT_DEST, "message", p ?
      "ПолинаБот: *непомнит какое сообщение было отредактировано*" :
      "ПолинаБот: *непомнит какое сообщение было удалено*"
    );
    return NULL;
  }

  Buffer b = Buffer$new();
  Buffer$appendString(&b, p->text);
  if(!(b.len && b.body[b.len-1] == '\n'))Buffer$appendString(&b, "\n\n");
  Z(formatCopyrightString(&b, p->user, p->chat, p->time));
  if(p->edit == 1){
    Buffer$appendString(&b, "[edit]");
  }else if(p->edit == 2){
    Buffer$appendString(&b, "[bomb]");
  }

  sendMessage(BOT_TOK, BOT_DEST, "disable_mentions", "1", "message", Buffer$toString(&b));

  finally:
  Buffer$delete(&b);
  Potato$delete(p);
  return NULL;
}

static void sendPotato(Potato* p, int edit){
  if(p){
    if(p->user == MY_ID && p->chat != 2000000008)return;
    p->edit = edit;
    printf("A message \e[35mid%d\e[0m from \e[32m%d\e[0m just got deleted at %s\n", p->id, p->user, getTimeString());
    printMallocStats();
  }else if(edit)p = NULL+1;

  START_THREAD(sendPotato_thread, p);
  return;
  #ifdef USE_PTHREADS
    finally:
    perror("");
  #endif
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
    if(old == NULL || strcmp(E(cJSON_GetStringValue(E(cJSON_GetArrayItem(json, 5)))), old->text) != 0){
      #ifndef DISABLE_EDITS
        sendPotato(old, 1);
      #else
        if(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 6)), "is_expired")){
          sendPotato(old, 2);
        }
      #endif
      old = E(Potato$new(json));
    }
    Bag$add(potatoBag, id, old);
  }

  finally:
  return;
}

void potato_init(){
  potatoBag = Bag$new();
  chatDict = LinkedDict$new(0, NULL);

  printf("Starting \x1b[93mpotatobot\x1b[0m at %s\n", getTimeString());
  fflush(stdout);
}

void potato_deinit(){
  Bag$delete(potatoBag);
  LinkedDict$delete(chatDict);
}

#ifdef __GNUC__
__attribute__((weak)) int main(){
  mallopt(M_CHECK_ACTION, 0b001);

  potato_init();
  longpoll("token.txt", potato_callback);
  potato_deinit();

  // finally:
  fflush(stdout);
  fflush(stderr);
  return 0;
}
#endif
