
#define STATUS_OK 0

static char nibbles[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static inline u32 pad(u32 a, u32 to)
{
    return ((((a-1)/to)+1)*to);
}

typedef struct buffer {
    heap h;
    bytes start;
    bytes end;
    bytes capacity;
    void *contents;
    int reference_count;
} *buffer;


#define alloca_buffer(__l) ({\
        buffer b = alloca(sizeof(struct buffer));    \
        b->contents =(void *)alloca(__l);            \
        b->end = 0;                  \
        b->start  =0 ;                               \
        b->h = 0;                                    \
        b;                                           \
})
    
                      
#define alloca_wrap_buffer(__b, __l) ({           \
            buffer b = alloca(sizeof(struct buffer));   \
            b->contents =(void *) __b;                  \
            b->end = b->capacity = __l;\
            b->start  =0 ;\
            b->h = 0;\
            b;\
        })

#define wrap_string(__h, __s) ({                         \
            buffer b = allocate(__h, sizeof(struct buffer));     \
            b->contents =(void *) __s;                  \
            b->end = b->capacity = strlen(__s);         \
            b->start  =0 ;\
            b->h = __h;\
            b;\
        })

#define alloca_wrap_string(__s) ({                         \
            buffer b = alloca(sizeof(struct buffer));     \
            b->contents =(void *) __s;                  \
            b->end = b->capacity = strlen(__s);         \
            b->start  =0 ;\
            b->h = 0;     \
            b;\
        })


static bytes buffer_length(buffer b) 
{
    return b->end - b->start;
}

static inline void *buffer_ref(buffer b, u64 offset)
{
    return b->contents + b->start;
}

static buffer allocate_buffer(heap h, bytes capacity)
{
    buffer b = allocate(h, sizeof(struct buffer));
    memset(b, 0, sizeof(struct buffer));
    b->h = h;
    b->capacity = capacity;
    b->reference_count = 1;
    b->contents = allocate(h, b->capacity);
    if (!b->contents) return 0;
    return b;
}

#define allocate_buffer_check(__h, __capacity)({                \
            buffer __b = allocate_buffer(__h, (__capacity));    \
            if (!__b) return error(NFS4_ENOMEM, "");            \
            __b;                                                \
        })

static inline void reset_buffer(buffer b)
{
    b->start = b->end = 0;
    b->reference_count = 1;
}

static inline status buffer_extend(buffer b, bytes len)
{
    if (b->capacity < (b->end + len)) {
        bytes oldcap = b->capacity;
        b->capacity = MAX(2 * oldcap, oldcap+len);
        void *new =  allocate(b->h, b->capacity);

        memcpy(new, b->contents, buffer_length(b));        
        deallocate(b->h, b->contents, oldcap);
        b->end = b->end - b->start;
        b->start = 0;
        b->contents = new;
    }
    return STATUS_OK;
}

static inline status push_bytes(buffer b, void *x, int length)
{
    check(buffer_extend(b, length));
    memcpy(b->contents + b->end, x, length);
    b->end += length;
}

// utf
#define push_character(__b, __x)\
    {buffer_extend((__b), 1);                           \
        *(u8 *)((__b)->contents + (__b)->end++) = __x;}

#define check_push_char(__b, __x)({\
    check(buffer_extend(__b, 1));\
    *(u8 *)(__b->contents + __b->end++) = __x;\
    STATUS_OK;\
    })

static status print_buffer_u32(buffer dest, buffer b)
{
    int len = buffer_length(b);
    //    buffer out = allocate_buffer_check(h, len*2 + len/4 + 2);
    for (int i = b->start; i< b->end; i++) {
        if ((i>b->start) && !(i % 4)) push_character(dest, '\n');
        u8 x = *(u8*)(b->contents + i);
        push_character(dest, nibbles[x >> 4]);
        push_character(dest, nibbles[x & 15]);
    }
    push_character(dest, '\n');
    return 0;
}

static inline void buffer_concat(buffer dest, buffer source)
{
    int len = buffer_length(source);
    buffer_extend(dest, len);
    memcpy(dest->contents + dest->end, source->contents + source->start, len);
    dest->end += len;
}

#define foreach_character(__c, __b) for(char __c= 1;__c ;__c = 0) for(u32 __j = 0, _len = buffer_length(__b); (__j< _len) && ((__c = *(char *)(__b->contents+__b->start + __j)), 1); __j++)

static inline void deallocate_buffer(buffer b)
{
    if (!--b->reference_count) {
        // support contig
        deallocate(b->h, b->contents, b->capacity);
        deallocate(b->h, b, sizeof(struct buffer));
    }
}

static inline boolean buffer_compare(buffer a, buffer b)
{
    if (buffer_length(a) != buffer_length(b)) return false;
    if (memcmp(buffer_ref(a, 0), buffer_ref(b, 0), buffer_length(a))) return false;
    return true;
}
