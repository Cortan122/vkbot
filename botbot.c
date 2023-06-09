#include "etcbot.h"

#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>

const Command rom[] = {
  { "token.txt", "/start", startbot_command, NULL, NULL },
  { "bottoken.txt", "/start", startbot_command, NULL, NULL },
  { "token.txt", "/python", pybot_command, NULL, NULL },
  // { "pybottoken.txt", "*/python", pybot_command, NULL, NULL },
  { "token.txt", "/read", read_command, NULL, NULL },
  { "token.txt", "/stats", stats_command, NULL, NULL },
  { "token.txt", "/stat", stats_command, NULL, NULL },
  { "token.txt", "/стат", stats_command, NULL, NULL },
  { "token.txt", "/friend", friend_command, NULL, NULL },
  { "token.txt", "/ded", ded_command, NULL, NULL },
  { "token.txt", "/ded_tg", ded_tg_command, NULL, NULL },
  { "token.txt", "/дед", ded_command, NULL, NULL },
  { "token.txt", "/tgrestart", tgrestart_command, NULL, NULL },
  { "token.txt", "/wifirestart", wifirestart_command, NULL, NULL },
  { "token.txt", "/email", email_command, NULL, NULL },
  { "token.txt", "/banner", banner_command, NULL, NULL },
  { "token.txt", "/at", at_command, NULL, NULL },
  { "token.txt", "/rev", rev_command, NULL, NULL },
  { "token.txt", "/рев", rev_command, NULL, NULL },
  { "token.txt", "/кум", rev_command, NULL, NULL },
  { "token.txt", "\\rev", rev_command, NULL, NULL },
  { "token.txt", "\\рев", rev_command, NULL, NULL },
  { "token.txt", "\\кум", rev_command, NULL, NULL },
  { "token.txt", ".кум", rev_command, NULL, NULL },
  { "token.txt", "", poll_observer, NULL, NULL },
  { "token.txt", "", antilink_observer, NULL, NULL },
  { "token.txt", "", tiktok_observer, tiktok_init, tiktok_deinit },
  { "token.txt", NULL, potato_callback, potato_init, potato_deinit }, // potatobot должен быть в конце тк он шакалит json (как?)
  { NULL }
};

char* toString(int v){
  Buffer b = Buffer$new();
  Buffer$printf(&b, "%d", v);
  return Buffer$toString(&b);
}

bool startsWithCommand(char* text, char* cmd){
  int textlen = strlen(text);
  int cmdlen = strlen(cmd);
  return textlen >= cmdlen && memcmp(text, cmd, cmdlen) == 0 && text[cmdlen] <= ' ';
}

void ParsedCommand$delete(ParsedCommand* cmd){
  if(!cmd)return;
  free(cmd->argv[0]);
  free(cmd->argv);
  free(cmd->str_chat);
  free(cmd->str_user);
  free(cmd->str_replyId);
  free(cmd);
}

ParsedCommand* ParsedCommand$new(cJSON* json){
  ParsedCommand* res = NULL;
  char* textDup = NULL;

  int type = E(cJSON_GetArrayItem(json, 0))->valueint;
  if(type != 5 && type != 4)return NULL;
  int chatid = E(cJSON_GetArrayItem(json, 3))->valueint;
  char* text = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(json, 5))));
  int flags = E(cJSON_GetArrayItem(json, 2))->valueint;

  res = calloc(sizeof(ParsedCommand), 1);
  res->event = json;
  res->user = res->chat = chatid;
  res->text = text;
  if(chatid > 2000000000){
    res->replyId = E(cJSON_GetArrayItem(json, 1))->valueint;
    res->user = atoi(E(cJSON_GetStringValue(E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 6)), "from")))));
  }else if(flags&2){
    res->user = MY_ID;
  }

  res->str_chat = toString(res->chat);
  res->str_user = toString(res->user);
  res->str_replyId = toString(res->replyId);

  // parse argv
  // todo: do something about <br> tags and double spaces
  // todo: do something about ", \\ and \" (or is this too hard)
  res->argc = 1;
  for(int i = 0; text[i]; i++){
    if(text[i] <= ' ')res->argc++;
  }
  res->argv = malloc(sizeof(char*) * (res->argc+1));
  int argCount = 0;
  res->argv[argCount++] = textDup = strdup(text);
  for(int i = 0; text[i]; i++){
    if(text[i] <= ' '){
      res->argv[argCount++] = textDup + i + 1;
      textDup[i] = '\0';
    }
  }
  res->argv[res->argc] = NULL; // argv is null-terminated

  return res;
  finally:;
  ParsedCommand$delete(res);
  return NULL;
}

int processCommand(cJSON* json, const Command* rom){
  ParsedCommand* res = NULL;
  char* textDup = NULL;
  int retval = -1; // -1 = error, 0 = no match, 1 = yes match

  int type = E(cJSON_GetArrayItem(json, 0))->valueint;
  if(type != 5 && type != 4)return 0;
  int chatid = E(cJSON_GetArrayItem(json, 3))->valueint;
  if(chatid < 0)return 0; // тут мы уходим чтобы боты другдруга не перебивали

  char* text = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(json, 5))));
  if(!startsWithCommand(text, rom->cmd))return 0;
  res = E(ParsedCommand$new(json));
  res->token = rom->token;

  struct timeval start;
  gettimeofday(&start, NULL);
  CommandCallback* cb = rom->callback;
  cb(res);
  struct timeval stop;
  gettimeofday(&stop, NULL);

  // print all the args
  printf("Running command \e[36m%s\e[0m as \e[33m%s\e[0m with argv = [", rom->cmd, rom->token);
  for(int i = 0; i < res->argc; i++){
    printf("\e[32m\"%s\"\e[0m%s", res->argv[i], (i == res->argc-1)?"":", ");
  }
  printf("] took \e[34m%6.3f\e[0ms\n", ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000000.0);
  fflush(stdout);

  retval = 1;
  finally:;
  ParsedCommand$delete(res);
  free(textDup);
  return retval;
}

void callback(cJSON* json, void* arg){
  char* token = arg;
  bool commandFlag = true;
  for(int i = 0; rom[i].token; i++){
    if(strcmp(rom[i].token, token))continue;
    if(rom[i].cmd == NULL){
      JSONCallback* cb = rom[i].callback;
      cb(json);
      continue;
    }
    if(strlen(rom[i].cmd) == 0){
      ParsedCommand* res = ParsedCommand$new(json);
      if(res){
        res->token = rom->token;
        CommandCallback* cb = rom[i].callback;
        cb(res);
        ParsedCommand$delete(res);
        continue;
      }
    }
    if(commandFlag && processCommand(json, rom+i) == 1){
      // we can only trigger one command at a time (or can we¿?)
      // тут нельзя break тк у нас есть ещё callback-и
      commandFlag = false;
    }
  }
}

void* startLongpoll_thread(void* arg){
  char* token = arg;
  while(1){
    time_t startTime = time(NULL);
    longpollEx(token, callback, token);
    time_t delta = time(NULL) - startTime;
    if(time(NULL) - startTime < 3600){
      THROW("longpollEx", "after %ld seconds", delta);
    }
  }
  finally:
  exit(1);
  return NULL;
}

int main(){
  mallopt(M_CHECK_ACTION, 0b001);
  setlocale(LC_NUMERIC, "en_US.UTF-8");

  int tokcap = 5;
  int toklen = 0;
  char** tokens = malloc(sizeof(char*) * tokcap);
  for(int i = 0; rom[i].token; i++){
    if(rom[i].init)rom[i].init();

    for(int j = 0; j < toklen; j++){
      if(strcmp(tokens[j], rom[i].token) == 0)goto continue2;
    }

    tokens[toklen++] = rom[i].token;
    if(toklen == tokcap){
      tokcap *= 2;
      tokens = realloc(tokens, sizeof(char*) * tokcap);
    }

    continue2:;
  }

  #ifdef USE_PTHREADS
    pthread_t* threads = malloc(sizeof(pthread_t) * toklen);
    for(int j = 0; j < toklen; j++){
      Z(pthread_create(threads+j, NULL, startLongpoll_thread, tokens[j]));
    }
  #else
    for(int j = 0; j < toklen; j++){
      if(fork())continue;
      startLongpoll_thread(tokens[j]);
      exit(0);
    }
  #endif

  #ifndef USE_PTHREADS
    while(wait(NULL) > 0);
  #else
    finally:
    for(int j = 0; j < toklen; j++){
      pthread_join(threads[j], NULL);
    }
    free(threads);
  #endif
  for(int i = 0; rom[i].token; i++){
    if(rom[i].deinit)rom[i].deinit();
  }
  free(tokens);
  return 0;
}
