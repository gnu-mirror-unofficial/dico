#ifndef _Cystack_h
#define _Cystack_h

#define STACK_UP 0
#define STACK_DN 1

typedef struct {                /* queue structure */
    void **queue;
    int head;
    int tail;
    int hlen;
    int entsiz;
} Cystack;

#define cystack_size(qptr) (qptr)->hlen
#define cystack_entrylen(qptr) (qptr)->entsiz

Cystack *cystack_create (int qsize, int esize);
void  cystack_clear (Cystack *qptr);
int   cystack_length (Cystack *qptr);
void  cystack_push (Cystack *qptr, void *str);
int   cystack_pop (Cystack *qptr, void *str, int dir);
int   cystack_get (Cystack *qptr, void *str, int index);

#endif
