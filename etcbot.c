#include "botlib.h"
#include "extralib.h"
#include "etcbot.h"

#include <string.h>
#include <stdbool.h>
#include <pcre.h>

#define PRIVATE_COMMAND(cmd) ({ \
  if(cmd->user != MY_ID){ \
    Buffer b = Buffer$new(); \
    Buffer$printf(&b, "ты что захотел меня сломать¿\nкоманда '%s' если что приватная))\nкак ты вообще о ней знаешь?", cmd->text); \
    respond(cmd, Buffer$toString(&b)); \
    Buffer$delete(&b); \
    return; \
  } \
})

static void respond(ParsedCommand* cmd, char* str){
  if(str == NULL || *str == '\0')str = "кажется получился пустой ответ 😇";
  sendMessage(cmd->token, cmd->str_chat,
    "message", str,
    "reply_to", cmd->replyId ? cmd->str_replyId : ""
  );
  finally:;
}

static int extractAttachments(ParsedCommand* cmd, Buffer* out){
  int numLinks = 0;
  cJSON* attachments = cJSON_GetArrayItem(cmd->event, 7);

  cJSON* att;
  cJSON_ArrayForEach(att, attachments){
    if(!att->string || !att->valuestring)continue;
    if(strncmp(att->string, "attach", 6))continue;
    if(strcmp(att->string + strlen(att->string) - 5, "_type"))continue;

    if(strcmp(att->valuestring, "link") == 0){
      numLinks++;
    }else{
      if(out->len)Buffer$appendChar(out, ',');
      Buffer$appendString(out, att->valuestring);
      if(strcmp(att->valuestring, "poll") == 0){
        Buffer$printf(out, "%s_", cmd->str_user);
      }

      char* key = strndup(att->string, strlen(att->string) - 5);
      char* data = cJSON_GetStringValue(cJSON_GetObjectItem(attachments, key));
      Buffer$appendString(out, data);
      free(key);
    }
  }

  return numLinks;
}

void print_callback(cJSON* json){
  printJson(json);
}

void startbot_command(ParsedCommand* cmd){
  respond(cmd, "Ты что считаешь меня ботом‽‽ 0_0");
}

void pybot_command(ParsedCommand* cmd){
  respond(cmd,
    "Тебе реально досихпор нужен ПитоноБот?\n"
    "\n"
    "Я теперь на втором курсе и у меня больше нет матпраков\n"
    "Может, если ты первокурсник и тебе нужен питонобот, ты можешь написать мне и я наверно смогу закостылить чтото новое\n"
    "PS: для тех, кому нужен старый питонобот, у меня в личке должна работать команда /python2019"
  );
}

static const int chatIdMap[] = { 1, 8, 16, 17, 20, 19, 25, 28, 30, 0 };

void read_command(ParsedCommand* cmd){
  // todo: fail gracefully
  if(cmd->user != MY_ID)return;
  if(cmd->chat < 2000000000)return;

  int botid = 0;
  for(; chatIdMap[botid]; botid++){
    if(chatIdMap[botid] == cmd->chat - 2000000000)break;
  }
  botid++;

  Buffer b = Buffer$new();
  cJSON* r = NULL;
  Buffer$printf(&b, "%d", botid + 2000000000);

  char* msg = "read_command"; // todo?: random
  sendMessage("bottoken.txt", Buffer$toString(&b), "message", msg);

  r = E(apiRequest("messages.getConversationsById", "token.txt", "peer_ids", cmd->str_chat, NULL));
  int id = E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0)), "last_message_id"))->valueint;
  Buffer$reset(&b);
  Buffer$printf(&b, "%d", id);
  cJSON_Delete(r); r = NULL;
  r = E(apiRequest("messages.getById", "token.txt", "message_ids", Buffer$toString(&b), NULL));
  cJSON* lastMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  char* msgtext = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(lastMessage, "text"))));

  if(strcmp(msgtext, msg) == 0){
    cJSON_Delete(E(apiRequest("messages.delete", "token.txt", "message_ids", Buffer$toString(&b), NULL)));
    cJSON_Delete(E(apiRequest("messages.delete", "token.txt", "message_ids", cmd->str_replyId, NULL)));
  }

  finally:;
  cJSON_Delete(r);
  Buffer$delete(&b);
}

void stats_command(ParsedCommand* cmd){
  cJSON* r = NULL;
  Buffer b = Buffer$new();
  Buffer lastidb = Buffer$new();

  r = E(apiRequest("messages.getHistory", cmd->token, "count", "0", "peer_id", cmd->str_chat, NULL));
  int count = E(cJSON_GetObjectItem(r, "count"))->valueint;

  char* query = NULL;
  int searchcount = 0;
  if(cmd->argc > 1){
    query = cmd->text + (cmd->argv[1] - cmd->argv[0]);
    cJSON_Delete(r); r = NULL;
    r = E(apiRequest("messages.search", cmd->token, "count", "0", "peer_id", cmd->str_chat, "q", query, NULL));
    searchcount = E(cJSON_GetObjectItem(r, "count"))->valueint;
  }

  cJSON_Delete(r); r = NULL;
  r = E(apiRequest("messages.getConversationsById", cmd->token, "peer_ids", cmd->str_chat, NULL));
  cJSON* chat = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  int lastid = E(cJSON_GetObjectItem(chat, "last_message_id"))->valueint;
  Buffer$printf(&lastidb, "%d", lastid);
  int members = 0;
  if(cmd->chat > 2000000000){
    members = E(cJSON_GetObjectItem(E(cJSON_GetObjectItem(chat, "chat_settings")), "members_count"))->valueint;
  }

  cJSON_Delete(r); r = NULL;
  r = E(apiRequest("messages.getById", cmd->token, "message_ids", Buffer$toString(&lastidb), NULL));
  cJSON* lastMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  int totalcount = E(cJSON_GetObjectItem(lastMessage, "conversation_message_id"))->valueint;

  Buffer$printf(&b, "Всего сообщений: %'d\n", totalcount);
  Buffer$printf(&b, "Неудалённых сообщений: %'d\n", count);
  if(query)Buffer$printf(&b, "Сообщений '%s': %'d\n", query, searchcount);
  if(members)Buffer$printf(&b, "Всего людей: %'d\n", members);

  respond(cmd, Buffer$toString(&b));

  finally:;
  Buffer$delete(&b);
  Buffer$delete(&lastidb);
  cJSON_Delete(r);
}

void friend_command(ParsedCommand* cmd){
  cJSON_Delete(apiRequest("friends.add", cmd->token, "user_id", cmd->str_user, NULL));
}

void ded_command(ParsedCommand* cmd){
  if(cmd->user != 77662058 && cmd->user != 234288523)PRIVATE_COMMAND(cmd);
  system("./deadlinebot &"); // todo
}

void tgrestart_command(ParsedCommand* cmd){
  PRIVATE_COMMAND(cmd);
  system("killall telegram-cli; rm ~/.telegram-cli/state && ~/bots/c/cronbot"); // todo
}

void wifirestart_command(ParsedCommand* cmd){
  PRIVATE_COMMAND(cmd);
  system("sudo ip link set wlan0 down && sudo ip link set wlan0 up");
}

void email_command(ParsedCommand* cmd){
  PRIVATE_COMMAND(cmd);
  Z(setenv("chat", cmd->str_chat, 1));
  Z(setenv("replyId", cmd->str_replyId, 1));
  system("../twixtractor/rssupdate.sh $chat $replyId &"); // todo
  finally:;
}

void banner_command(ParsedCommand* cmd){
  cJSON* r = NULL;
  Buffer lastidb = Buffer$new();
  bool isBroken = true;

  Buffer$printf(&lastidb, "%d", E(cJSON_GetArrayItem(cmd->event, 1))->valueint);
  r = E(apiRequest("messages.getById", cmd->token, "message_ids", Buffer$toString(&lastidb), NULL));
  cJSON* theMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  cJSON* theAttachment = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(theMessage, "attachments")), 0));
  cJSON* theSizes = E(cJSON_GetObjectItem(E(cJSON_GetObjectItem(theAttachment, "photo")), "sizes"));

  Z(setenv("url", getBestPhotoUrl(theSizes), 1));
  Z(system("tmux kill-session -t banner; tmux new -d -s banner sudo -u pi DISPLAY=:0 feh -FYZ \"$url\""));

  isBroken = false;
  finally:
  Buffer$delete(&lastidb);
  cJSON_Delete(r);
  if(isBroken)respond(cmd, "чтото пошло нетак 😇");
}

void poll_observer(ParsedCommand* cmd){
  Buffer attach = Buffer$new();

  if(E(cJSON_GetArrayItem(cmd->event, 0))->valueint != 4)return;
  if(cmd->chat < 2000000000)return;
  if(cmd->chat == 2000000025)return;
  if(cmd->user < 0)return;

  cJSON* attachments = E(cJSON_GetArrayItem(cmd->event, 7));
  if(!cJSON_GetObjectItem(attachments, "attach1_type"))return;
  if(strcmp(E(cJSON_GetStringValue(cJSON_GetObjectItem(attachments, "attach1_type"))), "poll"))return;

  if(strstr(cmd->text, "PRIVATE"))return;

  char* pollid = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(attachments, "attach1"))));
  Buffer$printf(&attach, "poll%s_%s", cmd->str_user, pollid);
  sendMessage("bottoken.txt", "2000000007",
    "attachment", Buffer$toString(&attach),
    "message", cmd->chat == 2000000002 ? "ВАЖНО" : ""
  );

  finally:
  Buffer$delete(&attach);
}

void at_command(ParsedCommand* cmd){
  PRIVATE_COMMAND(cmd);
  if(cmd->argc < 4){
    respond(cmd, "нам надо хотябы 4 аргумента");
    return;
  }
  bool isBroken = true;

  char buf[20];
  char* dest = parseDestination(cmd->argv[2], cmd->str_chat, cmd->token, buf);
  if(!dest){
    Buffer b = Buffer$new();
    Buffer$printf(&b, "кто такой вообще этот твой '%s'? я его незнаю", cmd->argv[2]);
    respond(cmd, Buffer$toString(&b));
    Buffer$delete(&b);
    return;
  }

  Z(setenv("time", cmd->argv[1], 1));
  Z(setenv("dest", dest, 1));
  Z(setenv("msg", cmd->text + (cmd->argv[3] - cmd->argv[0]), 1));
  Z(system("echo \"./send -t $dest \\\"\\$msg\\\"\" | at \"$time\""));
  // THIS IS (was) A ACTIUAL SECURITY FLAW ($msg cannot contain ", \ or $) (fixed?)

  isBroken = false;
  finally:
  if(isBroken)respond(cmd, "чтото пошло нетак 😇");
}

void rev_command(ParsedCommand* cmd){
  cJSON* r = NULL;
  char* text = NULL;
  Buffer message_id = Buffer$new();
  Buffer attachment = Buffer$new();
  bool isBroken = true;
  Buffer$printf(&message_id, "%d", E(cJSON_GetArrayItem(cmd->event, 1))->valueint);

  if(cmd->argc > 1){
    char* text = invertKeyboardLayout(cmd->text + (cmd->argv[1] - cmd->argv[0]));
    if(cmd->user == MY_ID){
      extractAttachments(cmd, &attachment);
      cJSON_Delete(E(apiRequest("messages.edit", "token.txt",
        "peer_id", cmd->str_chat,
        "message_id", Buffer$toString(&message_id),
        "attachment", Buffer$toString(&attachment),
        "message", text,
        "keep_forward_messages", "1",
      NULL)));
    }else{
      respond(cmd, text);
    }
    free(text);
    goto end;
  }

  r = E(apiRequest("messages.getById", cmd->token, "message_ids", Buffer$toString(&message_id), NULL));
  cJSON* theMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  cJSON* reply = cJSON_GetObjectItem(theMessage, "reply_message");
  cJSON* forward = cJSON_GetObjectItem(theMessage, "fwd_messages");
  if(reply){
    text = E(invertKeyboardLayout(E(cJSON_GetStringValue(E(cJSON_GetObjectItem(reply, "text"))))));
  }else if(forward && cJSON_GetArraySize(forward)){
    Buffer$reset(&message_id);
    cJSON* e;
    cJSON_ArrayForEach(e, forward){
      Buffer$appendString(&message_id, E(cJSON_GetStringValue(E(cJSON_GetObjectItem(e, "text")))));
      Buffer$appendString(&message_id, "\n\n");
    }
    text = E(invertKeyboardLayout(Buffer$toString(&message_id)));
  }else{
    cJSON_Delete(r); r = NULL;
    r = E(apiRequest("messages.getHistory", cmd->token, "peer_id", cmd->str_chat, NULL));
    cJSON* arr = E(cJSON_GetObjectItem(r, "items"));
    cJSON* e;
    bool isReady = false;
    cJSON_ArrayForEach(e, arr){
      char* text2 = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(e, "text"))));
      if(strcmp(text2, cmd->text) == 0){
        isReady = true;
      }else if(isReady){
        text = E(invertKeyboardLayout(text2));
        break;
      }
    }
    if(!isReady)goto finally;
  }
  if(startsWith(text, "/rev") || startsWith(text, ".кум"))text[0] = '_';
  respond(cmd, text);

  end:;
  isBroken = false;
  finally:
  free(text);
  Buffer$delete(&message_id);
  Buffer$delete(&attachment);
  cJSON_Delete(r);
  if(isBroken)respond(cmd, "чтото пошло нетак 😇");
}

typedef struct TiktokDTO {
  char* url;
  int msgid;
  int chatId;
} TiktokDTO;

static char* tiktok_regex_src = "https://vm\\.tiktok\\.com/[0-9a-zA-Z\\-_]{6,}/?|https://www\\.tiktok\\.com/@[a-zA-Z0-9_.]+/video/[0-9]{15,}/?";
static char* tiktok_tempdir = NULL;
static pcre* tiktok_regex = NULL;
static pcre_extra* tiktok_extra = NULL;

void tiktok_init(){
  const char* err = NULL;
  int erroffset;
  tiktok_tempdir = E(find("twixtractor/tmp", NULL));
  tiktok_regex = E(pcre_compile(tiktok_regex_src, PCRE_UTF8, &err, &erroffset, NULL));
  tiktok_extra = pcre_study(tiktok_regex, 0, &err);
  return;
  finally:
  if(err){
    fprintf(stderr, "Failed to compile regex \x1b[31m/%s/\x1b[0m at offset \x1b[33m%d\x1b[0m: %s\n", tiktok_regex_src, erroffset, err);
    fflush(stderr);
  }
  tiktok_regex = NULL;
}

void tiktok_deinit(){
  pcre_free(tiktok_regex);
  pcre_free_study(tiktok_extra);
  free(tiktok_tempdir);
}

void* tiktok_thread(void* ctx){
  TiktokDTO* dto = ctx;
  cJSON* r = NULL;
  char* attachment = NULL;
  Buffer b = Buffer$new();
  Buffer b2 = Buffer$new();
  Buffer$printf(&b, "%s/tiktok_%ld_%d.mp4", tiktok_tempdir, time(NULL), dto->msgid);

  Z(setenv("url", dto->url, 1));
  Z(setenv("outfile", Buffer$toString(&b), 1));
  Z(system("youtube-dl -o \"$outfile\" \"$url\""));

  Buffer$printf(&b2, "%d", dto->chatId);
  attachment = E(uploadFile(Buffer$toString(&b), "video", "189911804", "token.txt"));

  Buffer$reset(&b);
  Buffer$printf(&b, "%d", dto->msgid);
  r = E(apiRequest("messages.getById", "token.txt", "message_ids", Buffer$toString(&b), NULL));
  cJSON* msg = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  int cmsgid = E(cJSON_GetObjectItem(msg, "conversation_message_id"))->valueint;
  Buffer$reset(&b);
  Buffer$printf(&b, "{\"peer_id\": %d, \"conversation_message_ids\": [%d], \"is_reply\": true}", dto->chatId, cmsgid);

  sendMessage("bottoken.txt", Buffer$toString(&b2),
    "attachment", attachment,
    "forward", Buffer$toString(&b)
  );

  finally:
  Buffer$delete(&b);
  Buffer$delete(&b2);
  free(dto->url);
  free(dto);
  free(attachment);
  cJSON_Delete(r);
  return NULL;
}

void tiktok_observer(ParsedCommand* cmd){
  if(tiktok_regex == NULL)return;

  if(E(cJSON_GetArrayItem(cmd->event, 0))->valueint != 4)return;
  if(cmd->chat < 2000000000)return;
  if(cmd->user < 0)return;

  int out[3];
  int retval = pcre_exec(tiktok_regex, tiktok_extra, cmd->text, strlen(cmd->text), 0, 0, out, 3);
  if(retval == PCRE_ERROR_NOMATCH)return;
  if(retval != 1){
    THROW("pcre_exec(tiktok_regex)", "%d", retval);
  }

  int botid = 0;
  for(; chatIdMap[botid]; botid++){
    if(chatIdMap[botid] == cmd->chat - 2000000000)break;
  }
  botid++;

  char* url = strndup(cmd->text + out[0], out[1]-out[0]);
  printf("found match \x1b[92m'%s'\x1b[0m in \x1b[32m\"%s\"\x1b[0m\n", url, cmd->text);
  fflush(stdout);
  TiktokDTO* dto = calloc(1, sizeof(TiktokDTO));
  dto->url = url;
  dto->msgid = E(cJSON_GetArrayItem(cmd->event, 1))->valueint;
  dto->chatId = botid + 2000000000;
  START_THREAD(tiktok_thread, dto);

  finally:;
}

void antilink_observer(ParsedCommand* cmd){
  if(cmd->user != MY_ID)return;
  if(strlen(cmd->text) == 0)return;
  Buffer message_id = Buffer$new();
  Buffer attachment = Buffer$new();

  int numLinks = extractAttachments(cmd, &attachment);
  if(!numLinks)goto finally;

  int msgid = E(cJSON_GetArrayItem(cmd->event, 1))->valueint;
  printf("found \x1b[33m%d\x1b[0m link attechments in message \e[35mid%d\e[0m\n", numLinks, msgid);
  Buffer$printf(&message_id, "%d", msgid);

  cJSON_Delete(E(apiRequest("messages.edit", "token.txt",
    "peer_id", cmd->str_chat,
    "message_id", Buffer$toString(&message_id),
    "attachment", Buffer$toString(&attachment),
    "message", cmd->text,
    "keep_forward_messages", "1",
  NULL)));

  finally:;
  Buffer$delete(&message_id);
  Buffer$delete(&attachment);
}
