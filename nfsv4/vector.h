
typedef buffer vector;

static inline void *vector_get(vector v, int offset)
{
    void *res;
    memcpy(&res, v->contents + v->start + offset * sizeof(void *), sizeof(void *));
    return res;
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
    void *res;
    buffer_extend(v, sizeof(void *));
    memcpy(&res, v->contents + v->start, sizeof(void *));
    v->start += sizeof(void *);
    return res;
}

// really should be buffer input
static vector split(heap h, buffer source, char divider)
{
    vector result = allocate_vector(h, 10);
    buffer each = allocate_buffer(h, 10);
    forchar(i, source) {
        if (i == divider)  {
            vector_push(result, each);
            each = allocate_buffer(h, 10);
        } else {
            push_char(each, i);
        }
    }
    if (length(each) > 0)  vector_push(result, each);
    return result;
}

  

// should probably be string between
static buffer join(heap h, vector source, char between)
{
    buffer out = allocate_buffer(h, 100);
    for (int i = 0; i < vector_length(source); i++){
        if (i) push_char(out, between);
        buffer_concat(out, vector_get(source, i));
    }
    return out;
}

#define foreach(__i, __v) for(u32 _i = 0, _len = vector_length(__v); _i< _len && (__i = vector_get(__v, _i), 1); _i++)
