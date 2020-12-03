#include "botlib.h"

#include <curl/curl.h>

int main(int argc, char** argv){
  if(argc < 2 || argc % 2 == 1){
    fprintf(stderr, "usage: ./vkapi endpoint [key value]...\n");
    return 1;
  }

  char* endpoint = argv[1];
  char* token = "token.txt"; // todo: option
  int returnCode = 1;

  cJSON* res = NULL;
  char* str = NULL;
  res = E(apiRequest_argv(endpoint, token, argv+2));
  str = E(cJSON_Print(res));

  printf("%s\n", str);

  returnCode = 0;
  finally:
  free(str);
  cJSON_Delete(res);
  return returnCode;
}
