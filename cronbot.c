#include "botlib.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

char* startScriptPath = NULL;

int startTmuxJob(char* path, char* name){
  Z(setenv("name", name, 1));
  if(system("tmux has -t \"$name\"")){
    Z(setenv("path", path, 1));
    if(!startScriptPath)startScriptPath = E(find("start.sh", NULL));
    Z(setenv("start", startScriptPath, 1));
    Z(system("tmux new -d -s \"$name\" \"$start\" \"$path\""));
  }

  return 0;
  finally:
  return 1;
}

int lastIndexOf(char* str, char c){
  int ret = -1;
  for(int i = 0; str[i]; i++){
    if(str[i] == c)ret = i;
  }
  return ret;
}

int proccesLine(cJSON* line, Buffer* b){
  int prevlen = b->len;
  char* path = NULL;

  if(cJSON_GetArraySize(line) == 0)return 0;
  char* name = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(line, 0))));
  if(strlen(name) == 0)return 0;
  if(name[0] == '/' && name[1] == '/')return 0;
  Z(cJSON_GetArraySize(line) - 2);

  char* cron = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(line, 1))));
  if(strcmp(cron, "never") == 0)return 0;

  path = E(find(name, ".exe"));
  if(strcmp(cron, "start") == 0){
    name += lastIndexOf(name, '/')+1;
    int t = lastIndexOf(name, '.');
    if(t != -1)name[t] = '\0';
    Z(startTmuxJob(path, name));
  }else if(STARTS_WITH(cron, "start ")){
    Z(startTmuxJob(path, cron+strlen("start ")));
  }else{
    // trim:
    for(; *cron <= ' '; cron++);
    int len = strlen(cron);
    for(; len > 0 && cron[len] <= ' '; len--)cron[len] = '\0';

    // verify:
    int numSpaces = 0;
    for(int i = 0; cron[i]; i++){
      if(cron[i] == ' '){
        if(cron[i-1] == ' ')continue;
        numSpaces++;
        Buffer$appendChar(b, cron[i]);
        if(numSpaces < 5)continue;
      }else if(
        'A' <= cron[i] && cron[i] <= 'Z' ||
        'a' <= cron[i] && cron[i] <= 'z' ||
        '0' <= cron[i] && cron[i] <= '9' ||
        cron[i] == ',' || cron[i] == '-' ||
        cron[i] == '/' || cron[i] == '*' ||
        cron[i] == '?' || cron[i] == '#'
      ){
        Buffer$appendChar(b, cron[i]);
        continue;
      }else if(cron[i] == '@'){
        Buffer$printf(b, "%d", rand()%(numSpaces?24:60));
        continue;
      }

      fprintf(stderr, "cron expression '%s' contains invalid char '%c'\n", cron, cron[i]);
      fflush(stderr);
      goto finally;
    }

    Buffer$printf(b, " \"%s\" >>\"%s.log\" 2>&1\n", path, path);
  }

  free(path);
  return 0;
  finally:
  free(path);
  b->len = prevlen;
  return 1;
}

char* getNewCrontab(){
  Buffer b = Buffer$new();
  cJSON* table = E(gapiRequest("1HTom_rLaBVQFYkWGJWEM8v1HQSZj5pHkrUDbpYVtH1g", "crontab!A2:B", NULL));

  cJSON* line;
  cJSON_ArrayForEach(line, table)proccesLine(line, &b);
  cJSON_Delete(table);
  free(startScriptPath);

  return Buffer$toString(&b);
  finally:
  Buffer$delete(&b);
  return NULL;
}

int main(){
  srand(time(NULL));

  Buffer b = Buffer$new();
  Buffer$popen(&b, "crontab -l"); // explicitly no Z()
  Buffer$toString(&b);
  int i = 0;
  if(b.body[0] == '#'){
    for(; i < b.len; i++){
      if(b.body[i] == '\n' && b.body[i+1] != '#'){
        i++;
        break;
      }
    }
  }

  char* old = Buffer$toString(&b)+i;
  char* new = E(getNewCrontab());
  if(strcmp(old, new) != 0){
    printf("Replacing crontab at %s\n", getTimeString());
    // printf("'%s' -> '%s'\n", old, new);
    fflush(stdout);

    FILE* proc = E(popen("crontab -", "w"));
    b.len = i;
    fprintf(proc, "%s%s", Buffer$toString(&b), new);
    Z(pclose(proc));
  }

  free(new);
  Buffer$delete(&b);

  return 0;
  finally:
  return 1;
}
