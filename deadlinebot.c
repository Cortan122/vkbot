#define _XOPEN_SOURCE
#define _DEFAULT_SOURCE
#include "botlib.h"
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>

#define SHLEX "2000000001"

char* attachment = NULL;

int proccesLine(cJSON* line, Buffer* b){
  int prevlen = b->len;

  if(cJSON_GetArraySize(line) == 0){
    Buffer$appendChar(b, '\n');
    return 0;
  }

  char* str = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(line, 0))));
  if(str[0] == '#'){
    if(STARTS_WITH(str, "#coronaplot")){
      char* plot = E(find("twixtractor/corona.plot", "")); // todo: fix hardcoded path
      Z(setenv("plot", plot, 1));
      Z(system("\"$plot\""));
      free(plot);

      free(attachment); // todo: append to attachment, in case we have multiple attachments
      attachment = E(uploadFile("corona.png", "photo", SHLEX, "bottoken.txt"));
      Z(remove("corona.png"));
    }else if(STARTS_WITH(str, "#corona")){
      char* res = request("https://corona-stats.online/russia?format=json");
      cJSON* json = E(cJSON_Parse(res));
      free(res);
      cJSON* data = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(json, "data")), 0));
      Buffer$printf(b, "Корона: новых %d, всего %d\n",
        E(cJSON_GetObjectItem(data, "todayCases"))->valueint,
        E(cJSON_GetObjectItem(data, "active"))->valueint
      );
      cJSON_Delete(json);
    }
    return 0;
  }

  if(cJSON_GetArraySize(line) == 1){
    Buffer$printf(b, "%s\n", str);
    return 0;
  }

  Z(cJSON_GetArraySize(line) != 2);

  char* timestr = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(line, 1))));
  int charCount = 0;
  for(int i = 0; timestr[i] && charCount == 0; i++)if(timestr[i] > ' ')charCount++;
  if(charCount == 0){
    Buffer$printf(b, "%s\n", str);
    return 0;
  }

  bool reverse = false;
  if(timestr[0] == '#'){
    timestr++;
    reverse = true;
  }
  struct tm tm;
  memset(&tm, 0, sizeof(struct tm));
  E(strptime(timestr, "%F %R GMT%z", &tm));
  double days = (mktime(&tm) - time(NULL))/60./60./24.;
  if(reverse)days = -days;
  if(days < 0)return 0;

  int whole = floor(days);
  int part = floor((days-whole)*24);

  for(int i = 0; str[i]; i++){
    if(str[i] == '{' && str[i+1] && str[i+2] == '}'){
      if(str[i+1] == 'M'){
        Buffer$printf(b, "%d", (int)ceil(days));
      }else if((str[i+1] == 'n' || str[i+1] == 'N') && whole == 0){
        Buffer$printf(b, "%dч", part);
      }else if(str[i+1] == 'N'){
        Buffer$printf(b, "%dд", whole);
      }else if(str[i+1] == 'n'){
        Buffer$printf(b, "%dд %dч", whole, part);
      }else continue;
      i += 2;
    }else Buffer$appendChar(b, str[i]);
  }

  Buffer$appendChar(b, '\n');
  return 0;
  finally:
  b->len = prevlen;
  return 1;
}

char* getMessage(){
  Buffer b = Buffer$new();
  cJSON* table = NULL;
  table = E(gapiRequest("1HTom_rLaBVQFYkWGJWEM8v1HQSZj5pHkrUDbpYVtH1g", "Лист1!A2:B", NULL));
  printJson(table);

  cJSON* line;
  cJSON_ArrayForEach(line, table)proccesLine(line, &b);

  cJSON_Delete(table);
  return Buffer$toString(&b);
  finally:
  cJSON_Delete(table);
  Buffer$delete(&b);
  return NULL;
}

cJSON* traverseJsonPath(cJSON* json, ...){
  va_list ap;
  va_start(ap, json);
  char* name;
  while(1){
    name = va_arg(ap, char*);
    if(name == NULL)break;
    json = E(cJSON_GetObjectItem(json, name));
  }
  va_end(ap);

  return json;
  finally:
  fprintf(stderr, "%s not found\n", name);
  fflush(stderr);
  return NULL;
}

bool isLastMessageBotted(char* chat, char* token){
  cJSON* r = E(apiRequest("messages.getConversationsById", token, "peer_ids", chat, NULL));
  int id = E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0)), "last_message_id"))->valueint;
  cJSON_Delete(r);
  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id);

  r = E(apiRequest("messages.getById", token, "message_ids", Buffer$toString(&b), NULL));
  Buffer$delete(&b);
  cJSON* lastMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  int fromid = E(cJSON_GetObjectItem(lastMessage, "from_id"))->valueint;
  char* action = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(lastMessage, "action"), "type"));

  bool res = fromid < 0 || (action && strcmp(action, "chat_pin_message") == 0);
  cJSON_Delete(r);
  return res;
  finally:
  return true;
}

int pinLastMessage(char* chat){
  cJSON* r = E(apiRequest("messages.getConversationsById", "token.txt", "peer_ids", chat, NULL));
  cJSON* item = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  int id = E(cJSON_GetObjectItem(item, "last_message_id"))->valueint;
  int fromid = E(traverseJsonPath(item, "chat_settings", "pinned_message", "from_id", NULL))->valueint;
  cJSON_Delete(r);

  if(fromid >= 0)return 0;

  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id);
  r = E(apiRequest("messages.getById", "token.txt", "message_ids", Buffer$toString(&b), NULL));
  cJSON* lastMessage = E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0));
  fromid = E(cJSON_GetObjectItem(lastMessage, "from_id"))->valueint;

  if(fromid < 0){
    cJSON_Delete(E(apiRequest("messages.pin", "token.txt", "peer_id", chat, "message_id", Buffer$toString(&b), NULL)));
  }else{
    printf("can't pin at %s\n", getTimeString());
    fflush(stdout);
  }
  Buffer$delete(&b);

  return 0;
  finally:
  return 1;
}

int main(int argc, char** argv){
  if(!isLastMessageBotted(SHLEX, "token.txt")){
    char* res = E(getMessage());
    printf("%s", res);
    if(argc == 2 && strcmp(argv[1], "--preview") == 0)return 0;

    sendMessage("bottoken.txt", SHLEX, "message", res, "attachment", attachment ?: "");
    sendMessage("bottoken.txt", "190499058", "message", res);
    free(res);

    Z(pinLastMessage(SHLEX));
  }else{
    printf("the last message is also botted at %s\n", getTimeString());
  }

  free(attachment);

  return 0;
  finally:
  return 1;
}
