#include <pthread.h>

typedef struct Node {
	char *name;
	char *value;
	struct Node *lchild;
	struct Node *rchild;
} node_t;

extern node_t head;

#ifdef COARSE_LOCK
extern pthread_rwlock_t rwlock;
#endif

void interpret_command(char *, char *, int);
