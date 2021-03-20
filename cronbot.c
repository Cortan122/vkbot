#include "botlib.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

char* startScriptPath = NULL;
int restart = 0;

int startTmuxJob(char* path, char* name, int killall){
  Z(setenv("name", name, 1));
  Z(setenv("path", path, 1));
  if(!startScriptPath)startScriptPath = E(find("start.sh", NULL));
  Z(setenv("start", startScriptPath, 1));

  int has = !system("tmux has -t \"$name\"");

  if(!has && killall != 2){
    Z(system("tmux new -d -s \"$name\" \"$start\" \"$path\""));
    printf("started session %s\n", name);
    fflush(stdout);
  }else if(killall == 1){
    Z(system("tmux kill-session -t \"$name\""));
    Z(system("tmux new -d -s \"$name\" \"$start\" \"$path\""));
    printf("restarted session %s\n", name);
    fflush(stdout);
  }else if(has && killall == 2){
    Z(system("tmux kill-session -t \"$name\""));
    printf("stopped session %s\n", name);
    fflush(stdout);
  }else if(killall == 3){
    // killall == 3 is unused for now
    system(
      "[ \"$(stat -c '%Y' \"$path\")\" -gt "
      " \"$(tmux ls -F '#{session_name}:#{session_created}' | grep -F \"$name\" | awk -F: '{ print $2 }')\" ] && "
      "tmux kill-session -t \"$name\" && "
      "tmux new -d -s \"$name\" \"$start\" \"$path\" && "
      "echo started session \"$name\""
    );
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

char* getBasename(char* name){
  name += lastIndexOf(name, '/')+1;
  int t = lastIndexOf(name, '.');
  if(t != -1)name[t] = '\0';
  return name;
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
    Z(startTmuxJob(path, getBasename(name), restart));
  }else if(strcmp(cron, "stop") == 0){
    Z(startTmuxJob(path, getBasename(name), 2));
  }else if(startsWith(cron, "start ")){
    Z(startTmuxJob(path, cron+strlen("start "), restart));
  }else if(startsWith(cron, "stop ")){
    Z(startTmuxJob(path, cron+strlen("stop "), 2));
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
  cJSON* table = E(gapiRequest("1Slk3qh-E9w8LRa6QRAsoVhwFSkRFKPqHi-e2FCMPW_w", "crontab!A2:B", NULL));

  cJSON* line;
  cJSON_ArrayForEach(line, table)proccesLine(line, &b);
  cJSON_Delete(table);
  free(startScriptPath);

  return Buffer$toString(&b);
  finally:
  Buffer$delete(&b);
  return NULL;
}

bool isValidName(char* name){
  if(name[0] < 'a' || name[0] > 'z')return false;

  for(int i = 0; name[i]; i++){
    if( (name[0] < 'a' || name[0] > 'z') && (name[0] < 'A' || name[0] > 'Z') )return false;
  }

  return true;
}

char* getNewVartab(){
  Buffer b = Buffer$new();
  cJSON* table = NULL;
  table = E(gapiRequest("1Slk3qh-E9w8LRa6QRAsoVhwFSkRFKPqHi-e2FCMPW_w", "vars!A:B", NULL));

  cJSON* line;
  cJSON_ArrayForEach(line, table){
    if(cJSON_GetArraySize(line) < 2)continue;

    char* name = E(cJSON_GetStringValue(cJSON_GetArrayItem(line, 0)));
    char* value = E(cJSON_GetStringValue(cJSON_GetArrayItem(line, 1)));

    if(!isValidName(name)){
      fprintf(stderr, "var name '%s' contains invalid char\n", name);
      fflush(stderr);
      continue;
    }

    Buffer$printf(&b, "%s='", name);
    for(int i = 0; value[i]; i++){
      if(value[i] == '\''){
        Buffer$appendString(&b, "'\\''");
      }else{
        Buffer$appendChar(&b, value[i]);
      }
    }
    Buffer$appendString(&b, "'\n");
  }
  cJSON_Delete(table);

  return Buffer$toString(&b);
  finally:
  cJSON_Delete(table);
  Buffer$delete(&b);
  return NULL;
}

void updateAnimeUrl(){
  Buffer b = Buffer$new();
  char* vartab = NULL;
  char* path = NULL;

  vartab = E(getNewVartab());

  path = find("twixtractor/vartab.sh", NULL); // todo: fix hardcoded path
  if(path){
    Buffer$appendFile(&b, path);
  }else{
    char* folder = E(find("twixtractor", NULL));
    Buffer$printf(&b, "%s/vartab.sh", folder);
    path = strdup(Buffer$toString(&b));
    free(folder);
  }

  if(strcmp(Buffer$toString(&b), vartab)){
    printf("Replacing anime vartab at %s\n", getTimeString());
    fflush(stdout);
    FILE* fp = E(fopen(path, "w"));
    fputs(vartab, fp);
    Z(fclose(fp));
  }

  finally:;
  Buffer$delete(&b);
  free(vartab);
  free(path);
}

int main(int argc, char** argv){
  srand(time(NULL));

  if(argc == 2 && strcmp(argv[1], "--restart") == 0)restart = 1;

  updateAnimeUrl();
  if(argc == 2 && strcmp(argv[1], "--anime") == 0)return 0;

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
