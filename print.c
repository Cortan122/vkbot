#include "botlib.h"

#include <malloc.h>

void callback(cJSON* json){
  printJson(json);
}

int main(){
  mallopt(M_CHECK_ACTION, 0b001);

  longpoll("token.txt", callback);

  // finally:
  return 0;
}
