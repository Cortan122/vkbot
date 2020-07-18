typedef struct LinkedDict {
  struct LinkedDict* next;
  struct LinkedDict* last; // this is correct only when this is the first node
  int key;
  char* val;
} LinkedDict;

LinkedDict* LinkedDict$new(int key, char* val);
void LinkedDict$delete(LinkedDict* d);
void LinkedDict$add(LinkedDict* d, int key, char* val);
char* LinkedDict$get(LinkedDict* d, int key);
