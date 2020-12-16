#include "botlib.h"

#include <stdint.h>

char* getBestPhotoUrl(cJSON* arr);
char* parseDestination(char* src, char* chat, char* token, char* buf);
char* invertKeyboardLayout(char* text);

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12
typedef int (Utf8Callback)(uint32_t codepoint, uint8_t* buf, void* state);
int utf8Decode(int* state, uint32_t* codep, uint8_t byte);
int utf8ParseChar(uint8_t ch, Utf8Callback callback, void* state);
