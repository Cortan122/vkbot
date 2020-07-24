#include "cJSON.h"
#include "Buffer.h"
#include <signal.h>

#define MY_ID 560101729

typedef void (JSONCallback)(cJSON*);

char* getTimeString();
void waitForInternet();

char* request(char* url, int post);
int printJson(cJSON* json);
cJSON* apiRequest(char* endpoint, char* token, ...);
void longpoll(char* token, JSONCallback callback);
char* getRandomId();

volatile extern sig_atomic_t longpollFlag;

#define sendMessage(token, peer_id, ...) cJSON_Delete(E(apiRequest("messages.send", token, "peer_id", peer_id, "random_id", getRandomId(), __VA_ARGS__, NULL)))
