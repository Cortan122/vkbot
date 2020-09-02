#include "botlib.h"

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

void readCommand(ParsedCommand* cmd){
  if(cmd->user != MY_ID)return;
  // todo
}
