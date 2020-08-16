#include "botlib.h"

#include <malloc.h>
#include <curl/curl.h>

void callback(cJSON* json){
  printJson(json);
}

int main(int argc, char** argv){
  mallopt(M_CHECK_ACTION, 0b001);

  cJSON* json = E(apiRequest("users.get", argc>1 ? argv[1] : "token.txt", NULL));
  printf("%d\n", E(cJSON_GetObjectItem(E(cJSON_GetArrayItem(json, 0)), "id"))->valueint);
  cJSON_Delete(json);

  finally:
  return 0;
}
