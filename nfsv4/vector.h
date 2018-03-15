
typedef buffer vector;

static inline void *vector_get(vector v, int offset)
{
    void *res;
    bytes base = v->start + offset * sizeof(void *);
    if ((base + sizeof(void *)) > v->end) 
        panic("out of bounds vector reference");
    
    memcpy(&res, v->contents + base, sizeof(void *));
    return res;
}

static void extend_total(buffer b, int offset)
{
    if (offset > b->end) {
        buffer_extend(b, offset - b->end);
        memset(b->contents + b->end, 0, offset - b->end);
        b->end = offset;
    }
}

static inline void vector_set(vector v, int offset, void *value)
{
    extend_total(v, (offset + 1) *sizeof(void *));
    memcpy(v->contents + offset * sizeof(void*), &value, sizeof(void *));
}

static inline int vector_length(vector v)
{
    return length(v)/sizeof(void *);
}

static vector allocate_vector(heap h, int length)
{
    return allocate_buffer(h, length * sizeof (void *));
}

static void vector_push(vector v, void *i)
{
    buffer_extend(v, sizeof(void *));
    memcpy(v->contents + v->end, &i, sizeof(void *));
    v->end += sizeof(void *);
}

static void *vector_pop(vector v)
{
    if ((v->end - v->start) < sizeof(void *))
        panic("out of bounds vector reference");
    
    void *res;
    memcpy(&res, v->contents + v->start, sizeof(void *));
    v->start += sizeof(void *);
    return res;
}

static status split(vector dest, heap h, buffer source, char divider)
{
    buffer each = allocate_buffer(h, 10);
    foreach_character(i, source) {
        if (i == divider)  {
            vector_push(dest, each);
            each = allocate_buffer(h, 10);
        } else {
            push_character(each, i);
        }
    }
    if (length(each) > 0)  vector_push(dest, each);
    return NFS4_OK;
}

static status join(buffer dest, vector source, char between)
{
    for (int i = 0; i < vector_length(source); i++){
        if (i) push_character(dest, between);
        buffer_concat(dest, vector_get(source, i));
    }
    return NFS4_OK;
}

#define vector_foreach(__i, __v) for(u32 _i = 0, _len = vector_length(__v); _i< _len && (__i = vector_get(__v, _i), 1); _i++)

static inline void bitvector_set(buffer b, int position)
{
    extend_total(b, pad(position, 8)>>3);
    ((u8 *)b->contents)[position>>3] |= (1<<(position & 7));
}
