#include "botlib.h"
#include <unistd.h>
#include <malloc.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

// compiletime magic
#define _NUM(x) #x
#define NUM(x) _NUM(x)

#define STARTS_WITH(haystack, needle) (strlen(haystack) >= strlen(needle) && strncmp(haystack, needle, strlen(needle)) == 0)

#define CR_COMMAND(cmd, type) \
  else if(STARTS_WITH(line, cmd)){ \
    if(attachment.len)Buffer$appendChar(&attachment, ','); \
    char* path = line + strlen(cmd); \
    while(strlen(path) && path[strlen(path)-1] < ' ')path[strlen(path)-1] = '\0'; \
    free(Buffer$appendString(&attachment, E(uploadFile(path, type)))); \
  }

struct option longOptionRom[] = {
  {"to", required_argument, 0, 't'},
  {"token", required_argument, 0, 'T'},
  {"file", required_argument, 0, 'f'},
  {"type", required_argument, 0, 'x'},
  {"slurp", optional_argument, 0, 's'},
  {"notify", no_argument, 0, 'n'},
  {"help", no_argument, 0, 'h'},
  {0, 0, 0, 0}
};

char* token = "token.txt";
char* destination = NULL;
char* filepath = NULL;
char* filetype = "doc";
char* message = NULL;
bool slurp = false;
FILE* slurpfile;

void parseArgv(int argc, char** argv){
  while(1){
    int optionIndex = 0;
    int c = getopt_long(argc, argv, "hnt:f:T:s::x:", longOptionRom, &optionIndex);
    if(c == -1)break;
    switch(c){
      case 0:
        fprintf(stderr, "option %s has no short version for some reason\n", longOptionRom[optionIndex].name);
        break;
      case '?':
        exit(1);
      case 't':
        destination = optarg; // todo: strcmp(itoa(atoi(p)), p) == 0
        break;
      case 'T':
        token = optarg;
        break;
      case 'f':
        filepath = optarg;
        break;
      case 's':
        slurp = true;
        slurpfile = stdin;
        if(optarg)slurpfile = fopen(optarg, "r");
        break;
      case 'n':
        token = "bottoken.txt";
        destination = NUM(MY_ID);
        break;
      case 'x':
        filetype = optarg;
        break;
      case 'h':
        printf(
          "Usage: send [options] <message>\n"
          "\n"
          "Options:\n"
          "  -t, --to <peer_id>  Where are we going to send the message\n"
          "  -T, --token <file>  Path to token file (default: token.txt)\n"
          "  -f, --file <name>   The file to attach\n"
          "  -x, --type <name>   File type (doc|photo|graffiti)\n"
          "  -s, --slurp[=file]  Read messages from file (or stdin)\n"
          "  -n, --notify        Equivalent to -T bottoken.txt -t " NUM(MY_ID) "\n"
          "  -h, --help          Output usage information\n"
          // "  -V, --version       output the version number\n"
          "\n"
          "peer_id это:\n"
          "  Для пользователя: id пользователя.\n"
          "  Для групповой беседы: 2000000000 + id беседы.\n"
          "  Для сообщества: -id сообщества.\n"
        );
        exit(0);
      default:
        fprintf(stderr, "option -%c is somewhat unexpected\n", c);
        break;
    }
  }

  if(optind < argc){
    Buffer b = Buffer$new();
    while(optind < argc)Buffer$printf(&b, "%s ", argv[optind++]);
    Buffer$trimEnd(&b);
    message = Buffer$toString(&b);
  }

  if(!(message || filepath || slurp)){
    //todo: нам тут нечиго делать
  }
  if(!destination){
    //todo: нам тут некуда слать сообщения
  }
}

char* uploadFile(char* path, char* type){
  Buffer result = Buffer$new();
  cJSON* res = NULL;
  bool isPhoto = strcmp(type, "photo") == 0;
  if(isPhoto){
    res = E(apiRequest("photos.getMessagesUploadServer", token, "peer_id", destination, NULL));
  }else{
    if(strcmp(path + strlen(path) - 4, ".mp3") == 0)type = "audio_message";
    res = E(apiRequest("docs.getMessagesUploadServer", token, "type", type, "peer_id", destination, NULL));
  }

  // todo: cache upload servers
  char* upload_url = E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "upload_url"))));
  cJSON* res2 = E(postFile(upload_url, isPhoto?"photo":"file", path));
  cJSON_Delete(res);
  res = res2;

  if(isPhoto){
    Buffer$printf(&result, "%d", E(cJSON_GetObjectItemCaseSensitive(res, "server"))->valueint);
    res2 = E(apiRequest("photos.saveMessagesPhoto", token,
      "photo", E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "photo")))),
      "hash", E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "hash")))),
      "server", Buffer$toString(&result),
      NULL
    ));
    Buffer$reset(&result);
    cJSON_Delete(res);
    res = res2;
    cJSON* arr = E(cJSON_GetArrayItem(res, 0));
    Buffer$printf(&result, "photo%d_%d_%s",
      E(cJSON_GetObjectItemCaseSensitive(arr, "owner_id"))->valueint,
      E(cJSON_GetObjectItemCaseSensitive(arr, "id"))->valueint,
      E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(arr, "access_key"))))
    );
  }else{
    res2 = E(apiRequest("docs.save", token, "file", E(cJSON_GetStringValue(E(cJSON_GetObjectItemCaseSensitive(res, "file")))), NULL));
    cJSON_Delete(res);
    res = res2;
    cJSON* arr = E(cJSON_GetObjectItemCaseSensitive(res, type));
    Buffer$printf(&result, "doc%d_%d",
      E(cJSON_GetObjectItemCaseSensitive(arr, "owner_id"))->valueint,
      E(cJSON_GetObjectItemCaseSensitive(arr, "id"))->valueint
    );
  }
  cJSON_Delete(res);
  return Buffer$toString(&result);

  finally:
  Buffer$delete(&result);
  cJSON_Delete(res);
  return NULL;
}

bool slurpMessage(){
  Buffer message = Buffer$new();
  Buffer attachment = Buffer$new();
  bool retval = false;
  char* line = NULL;
  size_t len = 0;
  while(getline(&line, &len, slurpfile) != -1){
    if(strcmp(line, "\r\n") == 0)break;
    CR_COMMAND("\rfile ", "doc")
    CR_COMMAND("\rdoc ", "doc")
    CR_COMMAND("\rphoto ", "photo")
    CR_COMMAND("\rgraffiti ", "graffiti")
    else Buffer$appendString(&message, line);
  }

  if(message.len || attachment.len){
    sendMessage(token, destination, "message", Buffer$toString(&message), "attachment", Buffer$toString(&attachment));
  }

  retval = !feof(slurpfile);
  finally:
  Buffer$delete(&message);
  Buffer$delete(&attachment);
  free(line);
  return retval;
}

int main(int argc, char** argv){
  mallopt(M_CHECK_ACTION, 0b001);

  parseArgv(argc, argv);
  E(destination);

  if(message || filepath){
    Buffer b = Buffer$new();
    if(filepath)free(Buffer$appendString(&b, E(uploadFile(filepath, filetype))));
    sendMessage(token, destination, "message", message ?: "", "attachment", Buffer$toString(&b));
    Buffer$delete(&b);
    free(message);
  }

  if(slurp){
    while(slurpMessage());
    fclose(slurpfile);
  }

  return 0;
  finally:
  return 2;
}
