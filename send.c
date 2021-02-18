#include "botlib.h"
#include "extralib.h"
#include <unistd.h>
#include <malloc.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

#define CR_COMMAND_BASE(cmd, cb) \
  else if(startsWith(line, cmd)){ \
    char* path = line + strlen(cmd); \
    int len = strlen(path); \
    while(len && path[len-1] < ' ')path[--len] = '\0'; \
    cb; \
  }

#define CR_COMMAND_ATTACHMENT(cmd, free, cb) CR_COMMAND_BASE(cmd, { \
  if(attachment.len)Buffer$appendChar(&attachment, ','); \
  free(Buffer$appendString(&attachment, E(cb))); \
})

#define CR_COMMAND(cmd, type) CR_COMMAND_ATTACHMENT(cmd, free, uploadFile(path, type, destination, token))

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

char* parseDestination_cli(char* src){
  static char res[20];
  return E(parseDestination(src, NULL, NULL, res));

  finally:
  fprintf(stderr, "invalid destination '%s'\n", src);
  fflush(stderr);
  return NULL;
}

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
        destination = parseDestination_cli(optarg);
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
        if(optarg){
          slurpfile = fopen(optarg, "r");
          if(!slurpfile){
            perror(optarg);
            exit(1);
          }
        }
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
          "\n"
          "slurp mode commands:\n"
          "  \x1b[33m\\r\x1b[0mfile <file>\n"
          "  \x1b[33m\\r\x1b[0mdoc <file>\n"
          "  \x1b[33m\\r\x1b[0mphoto <file.png>\n"
          "  \x1b[33m\\r\x1b[0mgraffiti <file.png>\n"
          "  \x1b[33m\\r\x1b[0mattachment <type><uid>_<id>\n"
          "  \x1b[33m\\r\x1b[0mto <peer_id>\n"
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
    fprintf(stderr, "One of -s, -f or <message> must be specified\n");
    exit(1);
  }
  if(!destination){
    fprintf(stderr, "Destination (-t) must be specified\n");
    exit(1);
  }
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
    CR_COMMAND_ATTACHMENT("\rattachment ",, path)
    CR_COMMAND_BASE("\rto ", destination = parseDestination_cli(path))
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
    if(filepath)free(Buffer$appendString(&b, E(uploadFile(filepath, filetype, destination, token))));
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
