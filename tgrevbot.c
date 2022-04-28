#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>

#include "botlib.h"
#include "extralib.h"

char* rev_commands[] = {"/rev@rev122_bot", "/rev", "/рев", "/кум", "\\rev", "\\рев", "\\кум", ".кум", NULL};

cJSON* lastmsg_db = NULL;

void callback(cJSON* json){
  Buffer b = Buffer$new();
  char* response = NULL;
  char* reply_to_message_id = NULL;
  char* text = cJSON_GetStringValue(cJSON_GetObjectItem(json, "text")); // can be NULL
  int64_t fromid = getNumber(E(cJSON_GetObjectItem(json, "from")), "id");
  int64_t chatid = getNumber(E(cJSON_GetObjectItem(json, "chat")), "id");
  Buffer$printf(&b, "%ld", chatid);

  if(text == NULL){
    // ничего недалаем
  }else if(fromid == chatid){
    // нам пишут в личку
    response = E(invertKeyboardLayout(text));
  }else if(chatid < 0){
    // это беседа
    Buffer b1 = Buffer$new();
    for(int i = 0; rev_commands[i]; i++){
      Buffer$reset(&b1);
      Buffer$appendString(&b1, rev_commands[i]);
      Buffer$appendChar(&b1, ' ');

      if(startsWith(text, Buffer$toString(&b1))){
        response = E(invertKeyboardLayout(text + strlen(rev_commands[i])+1));
        break;
      }else if(strcmp(rev_commands[i], text) == 0){
        // это голая команда
        char* replytext = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "reply_to_message"), "text"));
        if(replytext){
          response = E(invertKeyboardLayout(replytext));
          break;
        }

        char* oldtext = cJSON_GetStringValue(cJSON_GetObjectItem(lastmsg_db, Buffer$toString(&b)));
        if(oldtext){
          response = E(invertKeyboardLayout(oldtext));
          break;
        }

        printJson(json);
        fflush(stdout);
      }
    }

    Buffer$reset(&b1);
    Buffer$printf(&b1, "%ld", getNumber(json, "message_id"));
    reply_to_message_id = Buffer$toString(&b1);

    cJSON_DeleteItemFromObjectCaseSensitive(lastmsg_db, Buffer$toString(&b));
    cJSON_AddItemToObject(lastmsg_db, Buffer$toString(&b), cJSON_DetachItemFromObjectCaseSensitive(json, "text"));
  }else{
    printJson(json);
    fflush(stdout);
  }

  if(response && reply_to_message_id){
    cJSON_Delete(E(tgapiRequest(
      "sendMessage", "tgtoken.txt", NULL,
      "chat_id", Buffer$toString(&b),
      "text", response,
      "reply_to_message_id", reply_to_message_id,
      "allow_sending_without_reply", "true",
      NULL
    )));
  }else if(response){
    cJSON_Delete(E(tgapiRequest(
      "sendMessage", "tgtoken.txt", NULL,
      "chat_id", Buffer$toString(&b),
      "text", response,
      NULL
    )));
  }

  finally:
  free(response);
  free(reply_to_message_id);
  Buffer$delete(&b);
  return;
}

int main(){
  printf("Starting \x1b[93mtgrevbot\x1b[0m at %s\n", getTimeString());
  fflush(stdout);

  lastmsg_db = cJSON_CreateObject();

  tglongpoll("tgtoken.txt", callback);

  return 0;
}
