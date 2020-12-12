#include "botlib.h"

#include <string.h>
#include <stdbool.h>

#define PRIVATE_COMMAND(cmd) ({ \
  if(cmd->user != MY_ID && cmd->user != 156402199 && cmd->user != 77662058){ \
    Buffer b = Buffer$new(); \
    Buffer$printf(&b, "ты что захотел меня сломать¿\nкоманда '%s' если что приватная))\nкак ты вообще о ней знаешь?", cmd->text); \
    respond(cmd, Buffer$toString(&b)); \
    Buffer$delete(&b); \
    return; \
  } \
})

static void respond(ParsedCommand* cmd, char* str){
  sendMessage(cmd->token, cmd->str_chat,
    "message", str,
    "reply_to", cmd->replyId ? cmd->str_replyId : ""
  );
  finally:;
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

static const int chatIdMap[] = { 1, 8, 16, 17, 20, 19, 0 };

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
  PRIVATE_COMMAND(cmd);
  system("./deadlinebot &"); // todo
}

char* getBestPhotoUrl(cJSON* arr);
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

void poll_callback(cJSON* json){
  Buffer attach = Buffer$new();

  if(E(cJSON_GetArrayItem(json, 0))->valueint != 4)return;
  if(E(cJSON_GetArrayItem(json, 3))->valueint < 2000000000)return;
  char* userid = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 6)), "from"))));
  if(atoi(userid) < 0)return;

  cJSON* attachments = E(cJSON_GetArrayItem(json, 7));
  if(!cJSON_GetObjectItem(attachments, "attach1_type"))return;
  if(strcmp(E(cJSON_GetStringValue(cJSON_GetObjectItem(attachments, "attach1_type"))), "poll"))return;

  char* pollid = E(cJSON_GetStringValue(E(cJSON_GetObjectItem(attachments, "attach1"))));
  Buffer$printf(&attach, "poll%s_%s", userid, pollid);
  sendMessage("bottoken.txt", "2000000007", "attachment", Buffer$toString(&attach));

  finally:
  Buffer$delete(&attach);
}

static char* parseDestination_number(char* src, char* res){
  bool isChat = src[0]=='c' || src[0]=='C';
  src += isChat;

  if(STARTS_WITH(src, "с") || STARTS_WITH(src, "С")){
    isChat = true;
    src += strlen("с");
  }

  int val;
  if(sscanf(src, "%d", &val) != 1)return NULL;

  snprintf(res, 20, "%d", val);
  if(strcmp(res, src))return NULL;
  if(!isChat)return src;

  snprintf(res, 20, "%d", val+2000000000);
  return res;
}

static char* parseDestination(char* src, ParsedCommand* cmd, char* buf){
  char* bufornull = NULL;
  char* tmp = parseDestination_number(src, buf);
  if(tmp)return tmp;

  if(strcmp(src, "here") == 0)return cmd->str_chat;

  cJSON* res = NULL;
  res = E(apiRequest("users.get", cmd->token, "user_ids", src, NULL));
  int id = E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(res, 0)), "id"))->valueint;
  snprintf(buf, 20, "%d", id);
  bufornull = buf;

  finally:
  cJSON_Delete(res);
  return bufornull;
}

void at_command(ParsedCommand* cmd){
  PRIVATE_COMMAND(cmd);
  if(cmd->argc < 4){
    respond(cmd, "нам надо хотябы 4 аргумента");
    return;
  }
  bool isBroken = true;

  char buf[20];
  char* dest = parseDestination(cmd->argv[2], cmd, buf);
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

void censorbot_init_command(ParsedCommand* cmd){
  // todo: fail gracefully
  if(cmd->user != MY_ID)return;
  if(cmd->chat != 2000000026)return;

  char* script = NULL;
  script = E(find("shady/draw_vk_msg", ".sh"));

  Z(setenv("script", script, 1));
  Z(setenv("id", cmd->argv[1], 1));
  Z(system("\"$script\" \"$id\" --init"));

  finally:;
  free(script);
}

void censorbot_command(ParsedCommand* cmd){
  // todo: fail gracefully
  if(cmd->chat != 2000000026)return;

  char* script = NULL;
  script = E(find("shady/draw_vk_msg", ".sh"));

  Z(setenv("script", script, 1));
  Z(setenv("word", cmd->argv[1], 1));
  Z(system("\"$script\" auto \"$word\" &"));

  finally:;
  free(script);
}
