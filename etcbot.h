#include "botlib.h"

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
  int replyId; // 0 by default
  char* str_chat;
  char* str_user;
  char* str_replyId;
  int argc;
  char** argv;
  cJSON* event;
  // todo?: char** rest; (rest[i] = text + argv[i] - argv[0])
  // const Command* rom;
} ParsedCommand;

typedef void (CommandCallback)(ParsedCommand*);

void potato_init();
void potato_deinit();
void potato_callback(cJSON* json);

void tiktok_init();
void tiktok_deinit();
void tiktok_observer(ParsedCommand* cmd);
void poll_observer(ParsedCommand* cmd);
void antilink_observer(ParsedCommand* cmd);

void startbot_command(ParsedCommand* cmd);
void pybot_command(ParsedCommand* cmd);
void read_command(ParsedCommand* cmd);
void stats_command(ParsedCommand* cmd);
void friend_command(ParsedCommand* cmd);
void ded_command(ParsedCommand* cmd);
void ded_tg_command(ParsedCommand* cmd);
void tgrestart_command(ParsedCommand* cmd);
void wifirestart_command(ParsedCommand* cmd);
void email_command(ParsedCommand* cmd);
void banner_command(ParsedCommand* cmd);
void at_command(ParsedCommand* cmd);
void rev_command(ParsedCommand* cmd);
