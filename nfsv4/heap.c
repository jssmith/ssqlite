#include <runtime.h>


static void *sys_malloc(heap h, bytes size)
{
    return malloc(size);
}


static void sys_free(heap h, void *x, bytes size)
{
    free(x);
}

static void sys_destroy(heap h)
{
    // get all the children
}

heap init_heap()
{
    heap h = malloc(sizeof(struct heap));
    h->alloc = sys_malloc;
    h->dealloc = sys_free;
    return h; 
}
