#include "LinkedDict.h"
#include <stdlib.h>
#include <stdio.h>

LinkedDict* LinkedDict$new(int key, char* val){
  LinkedDict* res = malloc(sizeof(LinkedDict));
  res->next = NULL;
  res->last = res;
  res->key = key;
  res->val = val;
  return res;
}

void LinkedDict$delete(LinkedDict* d){
  LinkedDict* next;
  while(d){
    next = d->next;
    free(d->val);
    free(d);
    d = next;
  }
}

void LinkedDict$add(LinkedDict* d, int key, char* val){
  if(val == NULL)return;
  d->last = d->last->next = LinkedDict$new(key, val);
}

char* LinkedDict$get(LinkedDict* d, int key){
  for(; d; d = d->next){
    if(d->key == key && d->val != NULL)return d->val;
  }
  return NULL;
}

void LinkedDict$puts(LinkedDict* d, char* name){
  if(name)printf("%s = {\n", name);
  else printf("{\n");
  // foreach: `for(LinkedDict* d = chatDict; d; d = d->next)`
  for(; d; d = d->next){
    if(d->val)printf("  \x1b[33m%d\x1b[0m: \x1b[32m\"%s\"\x1b[0m%c\n", d->key, d->val, d->next?',':' ');
    else printf("  \x1b[33m%d\x1b[0m: \x1b[35mnull\x1b[0m%c\n", d->key, d->next?',':' ');
  }
  printf("}\n");
  fflush(stdout);
}
