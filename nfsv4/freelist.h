
// there should be a merge between this and heap, but its not timely to
// look for it. the issue lies around compound objects like rpc
typedef struct freelist {
    heap parent;
    // external linkage is a drag, but we dont want to mess up the guys
    vector f;
    void *(*alloc)(void *);
    void *a;
} *freelist;
    
static inline void freelist_deallocate(freelist f, void *x)
{
    vector_push(f->f, x);
}

static inline void *freelist_allocate(freelist f)
{
    void *r = vector_pop(f->f);
    if (r) return r;
    return f->alloc(f->a);
}

static inline freelist create_freelist(heap parent, void *(*a)(void *), void *aa)
{
    freelist f = allocate(parent, sizeof(struct freelist));
    f->alloc = a;
    f->a = aa;
    f->f = allocate_vector(parent, 10);
    return f;
}

