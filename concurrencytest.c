#include "botlib.h"
#include <unistd.h>

void* sendTestMessage(void* arg){
  long i = (long)arg;
  Buffer b = Buffer$new();
  Buffer$printf(&b, "сообщение №%d", i);
  sendMessage("token.txt", "560101729", "message", Buffer$toString(&b));
  Buffer$delete(&b);
  finally:
  return NULL;
}

int main(){
  START_THREAD(sendTestMessage, (void*)1);
  START_THREAD(sendTestMessage, (void*)2);
  START_THREAD(sendTestMessage, (void*)3);
  START_THREAD(sendTestMessage, (void*)4);
  START_THREAD(sendTestMessage, (void*)5);
  START_THREAD(sendTestMessage, (void*)6);
  sleep(20);
  finally:
  return 0;
}
