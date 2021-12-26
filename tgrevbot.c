#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>

#include "botlib.h"
#include "extralib.h"

#define getNumber(json, name) (int64_t)E(cJSON_GetObjectItem(json, name))->valuedouble

cJSON* tgapiRequest(char* endpoint, char* token, ...){
  Buffer b = Buffer$new();
  Buffer$printf(&b, "https://api.telegram.org/bot%s/%s", E(findToken(token)), endpoint);

  bool isFirst = true;
  va_list ap;
  va_start(ap, token);
  while(1){
    char* v1 = va_arg(ap, char*);
    if(v1 == NULL)break; // yay у нас кончились все аргументы
    char* v2 = E(va_arg(ap, char*));
    Buffer$appendChar(&b, isFirst?'?':'&');
    Buffer$appendString(&b, v1);
    Buffer$appendChar(&b, '=');
    curl_free(Buffer$appendString(&b, E(curl_easy_escape(NULL, v2, 0))));
    isFirst = false;
  }
  va_end(ap);

  char* res = E(request(Buffer$toString(&b)));
  Buffer$delete(&b);
  cJSON* json = E(cJSON_Parse(res));
  free(res);

  bool err = E(cJSON_GetObjectItem(json, "ok"))->type == cJSON_False;
  if(err){
    Z(printJson(json));
    fflush(stdout);
    cJSON_Delete(json);
    return NULL;
  }else{
    cJSON* response = E(cJSON_DetachItemFromObjectCaseSensitive(json, "result"));
    cJSON_Delete(json);
    return response;
  }

  finally:
  return NULL;
}

void tglongpoll(char* token, JSONCallback callback){
  cJSON* res = NULL;
  int64_t offset = 0;
  Buffer b = Buffer$new();

  while(true){
    Buffer$reset(&b);
    Buffer$printf(&b, "%ld", offset);
    res = E(tgapiRequest("getUpdates", token,
      "timeout", "1000",
      "offset", Buffer$toString(&b),
      "allowed_updates", "[\"message\", \"edited_message\"]",
      NULL
    ));

    if(cJSON_GetArraySize(res))offset = -1;

    cJSON* update;
    cJSON_ArrayForEach(update, res){
      int64_t id = getNumber(update, "update_id");
      if(offset < id)offset = id+1;

      if(cJSON_GetObjectItem(update, "message")){
        callback(cJSON_GetObjectItem(update, "message"));
      }else if(cJSON_GetObjectItem(update, "edited_message")){
        callback(cJSON_GetObjectItem(update, "edited_message"));
      }
    }
  }

  finally:
  Buffer$delete(&b);
  cJSON_Delete(res);
  return;
}

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
    cJSON_free(E(tgapiRequest(
      "sendMessage", "tgtoken.txt",
      "chat_id", Buffer$toString(&b),
      "text", response,
      "reply_to_message_id", reply_to_message_id,
      "allow_sending_without_reply", "true",
      NULL
    )));
  }else if(response){
    cJSON_free(E(tgapiRequest(
      "sendMessage", "tgtoken.txt",
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
