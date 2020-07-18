#include "LinkedDict.h"
#include <stdlib.h>

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
  while(d != NULL){
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
  while(d->next){
    if(d->key == key && d->val != NULL)return d->val;
    d = d->next;
  }
  return NULL;
}
