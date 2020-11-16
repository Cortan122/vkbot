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
  if(cmd->user != MY_ID){
    respond(cmd, "ты что захотел меня сломать¿");
    return;
  }
  system("./deadlinebot"); // todo
}
