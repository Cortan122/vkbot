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
      char* plot = E(find("pybot/corona.plot", ""));
      Z(setenv("plot", plot, 1));
      Z(system("\"$plot\""));
      free(plot);

      free(attachment); // todo
      attachment = E(uploadFile("corona.png", "photo", "2000000001", "bottoken.txt"));
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

bool isLastMessageBotted(char* chat, char* token){
  cJSON* r = E(apiRequest("messages.getConversationsById", "token.txt", "peer_ids", "2000000001", NULL));
  int id = E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(E(cJSON_GetObjectItem(r, "items")), 0)), "last_message_id"))->valueint;
  cJSON_Delete(r);
  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", id);

  r = E(apiRequest("messages.getById", "token.txt", "message_ids", Buffer$toString(&b), NULL));
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

int main(){
  if(!isLastMessageBotted("2000000001", "token.txt")){
    char* res = E(getMessage());
    printf("%s", res);
    sendMessage("bottoken.txt", "2000000001", "message", res, "attachment", attachment ?: "");
    free(res);

    // char* path = E(find("send", ".exe"));
    // Z(setenv("path", path, 1)); // path != PATH?
    // FILE* sender = popen("\"$path\" -sn -t 2000000001", "w");
    // fwrite(res, 1, strlen(res), sender);
    // pclose(sender);
    // free(path);
    // free(res);
  }else{
    printf("the last message is also botted at %s\n", getTimeString());
  }

  free(attachment);

  return 0;
  finally:
  return 1;
}
