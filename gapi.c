#include "botlib.h"

#include <curl/curl.h>

int main(int argc, char** argv){
  if(argc != 3){
    fprintf(stderr, "usage: ./gapi table range\n");
    return 1;
  }
  int returnCode = 1;
  cJSON* res = NULL;
  char* str = NULL;
  res = E(gapiRequest(argv[1], argv[2], NULL));
  str = E(cJSON_Print(res));

  printf("%s\n", str);

  returnCode = 0;
  finally:
  free(str);
  cJSON_Delete(res);
  return returnCode;
}
