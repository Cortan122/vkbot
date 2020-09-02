#include "botlib.h"

#include <string.h>

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
