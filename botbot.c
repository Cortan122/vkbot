#include "botlib.h"

#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

typedef void (Action)();

typedef struct Command {
  char* token;
  char* cmd;
  void* callback;
  Action* init;
  Action* deinit;
} Command;

typedef struct ParsedCommand {
  char* token;
  char* text;
  int chat;
  int user;
  int argc;
  char** argv;
  // todo?: char** rest;
  // const Command* rom;
} ParsedCommand;

typedef void (CommandCallback)(ParsedCommand*);

void potato_callback(cJSON* json);
void potato_init();
void potato_deinit();

const Command rom[] = {
  { "token.txt", NULL, potato_callback, potato_init, potato_deinit },
  // { "token.txt", "/start", startbot_callback, NULL, NULL },
  // { "bottoken.txt", "/start", startbot_callback, NULL, NULL },
  // { "token.txt", "/python", pybot_callback, NULL, NULL },
  // { "pybottoken.txt", "*/python", pybot_callback, NULL, NULL },
  { NULL }
};

int processCommand(cJSON* json, const Command* rom){
  ParsedCommand* res = NULL;
  char* textDup = NULL;
  int retval = -1;

  int type = E(cJSON_GetArrayItem(json, 0))->valueint;
  if(type != 5 && type != 4)return 0;
  int chatid = E(cJSON_GetArrayItem(json, 3))->valueint;
  if(chatid < 0)return 0; // тут мы уходим чтобы боты другдруга не перебивали

  char* text = E(cJSON_GetStringValue(E(cJSON_GetArrayItem(json, 5))));
  bool isAuto = rom->cmd[0] == '*';
  if(!isAuto && !STARTS_WITH(text, rom->cmd))return 0; // todo: check for space

  res = malloc(sizeof(ParsedCommand));
  res->argv = NULL;
  res->user = res->chat = chatid;
  res->token = rom->token;
  res->text = text;
  // res->rom = rom;
  if(chatid > 2000000000){
    res->user = atoi(E(cJSON_GetStringValue(E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 6)), "from")))));
  }

  // parse argv
  // todo: do something about <br> tags and double spaces
  res->argc = 1;
  for(int i = 0; text[i]; i++){
    if(text[i] <= ' ')res->argc++;
  }
  res->argc += isAuto;
  res->argv = malloc(sizeof(char*) * (res->argc+1));
  int argCount = 0;
  if(isAuto)res->argv[argCount++] = rom->cmd + 1;
  res->argv[argCount++] = textDup = strdup(text);
  for(int i = 0; text[i]; i++){
    if(text[i] <= ' '){
      res->argv[argCount++] = textDup + i + 1;
      textDup[i] = '\0';
    }
  }
  res->argv[res->argc] = NULL; // argv is null-terminated

  // print all the args
  printf("Running command \x1b[36m%s\x1b[0m as \x1b[33m%s\x1b[0m with argv = [", rom->cmd, rom->token);
  for(int i = 0; i < res->argc; i++){
    printf("\x1b[32m\"%s\"\x1b[0m%s", res->argv[i], (i == res->argc-1)?"]\n":", ");
  }
  fflush(stdout);

  CommandCallback* cb = rom->callback;
  cb(res);
  retval = 1; // or 0?
  finally:;
  if(res)free(res->argv);
  free(res);
  free(textDup);
  return retval;
}

void callback(cJSON* json, void* arg){
  char* token = arg;
  for(int i = 0; rom[i].token; i++){
    if(strcmp(rom[i].token, token))continue;
    if(rom[i].cmd == NULL){
      JSONCallback* cb = rom[i].callback;
      cb(json);
      continue;
    }
    if(processCommand(json, rom+i) == 1)break; // we can only trigger one command at a time (or can we¿?)
  }
}

void* startLongpoll_thread(void* arg){
  char* token = arg;
  longpollEx(token, callback, token);
  return NULL;
}

int main(){
  mallopt(M_CHECK_ACTION, 0b001);

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
