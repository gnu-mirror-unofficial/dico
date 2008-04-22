/* $Id$
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <cystack.h>

Cystack *
cystack_create(int qsize, int esize)
{
    int     i;
    Cystack   *qptr;
    char    *ptr;

    qptr = malloc(sizeof(*qptr) + qsize * (sizeof(char*) + esize));
    if (!qptr)
        return NULL;
    qptr->queue = (void**)(qptr + 1);
    ptr = (char*)(qptr->queue + qsize);
    for (i = 0; i < qsize; i++) {
        qptr->queue[i] = ptr;
        ptr += esize;
    }
    qptr->hlen = i;
    qptr->head = qptr->tail = 0;
    qptr->entsiz = esize;
    return qptr;
}

void
cystack_clear(Cystack *qptr)
{
    qptr->head = qptr->tail = 0;
}

int
cystack_length(Cystack *qptr)
{
    int l;

    l = qptr->tail - qptr->head;
    if (l < 0)
        l += qptr->hlen;
    return l;
}

void
cystack_push_head(Cystack *qptr)
{
    if (qptr->head == qptr->tail)
	return;
    qptr->queue[ qptr->tail ] = qptr->queue[ qptr->head ];
    qptr->tail = (qptr->tail + 1) % qptr->hlen;
    qptr->head = (qptr->head + 1) % qptr->hlen;
}

void
cystack_push_tail(Cystack *qptr)
{
    void *tmp;
    
    if (qptr->head == qptr->tail)
	return;
    qptr->head--;
    if (qptr->head < 0)
	qptr->head = qptr->hlen - 1;

    tmp = qptr->queue[ qptr->head ];
    qptr->queue[ qptr->head ] = qptr->queue[ qptr->tail ];
    qptr->queue[ qptr->tail ] = tmp;
}

void
cystack_push(Cystack *qptr, void *str)
{
    if (cystack_length(qptr) == qptr->hlen - 1) {
       qptr->head = (qptr->head + 1) % (qptr->hlen);
    }
    memcpy(qptr->queue[qptr->tail], str, qptr->entsiz);
    qptr->tail = (qptr->tail + 1) % (qptr->hlen);
}

int
cystack_pop(Cystack *qptr, void *str, int dir)
{
    int n;
    
    if (qptr->head == qptr->tail)
       return -1;
    switch (dir) {
    case STACK_DN:  /* Moving from head to tail */
	n = qptr->tail;
	cystack_push_head(qptr);
	memcpy(str, qptr->queue[n], qptr->entsiz);
	break;

    case STACK_UP:  /* Moving from tail to head */
	qptr->tail--;
        if (qptr->tail < 0)
            qptr->tail = qptr->hlen - 1;
	memcpy(str, qptr->queue[ qptr->tail ], qptr->entsiz);
	cystack_push_tail(qptr);
    }
    return 0;
}

/* Reads a string from the queue at specified offset.
 */
int
cystack_get(Cystack *qptr, void *str, int index)
{
    int n;
    
    if (qptr->head == qptr->tail)
       return -1;
    n = qptr->tail-1-index;
    if (n < 0)
	n = qptr->hlen;
    memcpy(str, qptr->queue[n % qptr->hlen], qptr->entsiz);
    return 0;
}




