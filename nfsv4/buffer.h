// move to a more general externally visible runtime header
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;
typedef u32 bytes;
typedef void *heap;

typedef void *heap;
#define allocate(__h, __b) (malloc(__b))
#define deallocate(__h, __x, __len) (free(__x))


#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a):(b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a):(b))
#endif



// buffer content
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
} *buffer;


static bytes length(buffer b) 
{
    return b->end - b->start;
}

static buffer allocate_buffer(heap h, bytes capacity){
    buffer b = allocate(h, sizeof(struct buffer));
    memset(b, 0, sizeof(struct buffer));    
    b->capacity = capacity;
    b->contents = allocate(h, b->capacity);
    return b;
}

static inline void buffer_extend(buffer b, bytes len)
{
    if (b->capacity < (b->end + len)) {
        bytes oldcap = b->capacity;
        b->capacity = MAX(2 * oldcap, len);
        void *new =  allocate(b->h, b->capacity);

        memcpy(new, b->contents + b->start, length(b));        
        deallocate(b->h, b->contents, oldlen);
        b->end = b->end - b->start;
        b->start = 0;
        b->contents = new;
    }
}

static inline void push_bytes(buffer b, void *x, int length)
{
    buffer_extend(b, length);
    memcpy(b->contents + b->end, x, length);
    b->end += length;
}

static void push_char(buffer b, char x)
{
    buffer_extend(b, 1);
    *(u8 *)(b->contents + b->end) = x;
    b->end ++ ;
}

static buffer print_buffer_u32(heap h, buffer b)
{
    int len = length(b);
    buffer out = allocate_buffer(h, len*2 + len/4 + 2);
    for (int i = b->start; i< b->end; i++) {
        if ((i>b->start) && !(i % 4)) push_char(out, '\n');
        u8 x = *(u8*)(b->contents + i);
        push_char(out, nibbles[x >> 4]);
        push_char(out, nibbles[x & 15]);
    }
    push_char(out, '\n');    
    return out;
}

static void buffer_concat(buffer dest, buffer source)
{
    int len = length(source);
    buffer_extend(dest, len);
    memcpy(dest->contents + dest->end, source->contents + source->start, len);
    dest->end += len;
}

#define forchar(__c, __b) for(char __c= 1;__c ;__c = 0) for(u32 __j = 0, _len = length(__b); (__j< _len) && ((__c = *(char *)(__b->contents+__b->start + __j)), 1); __j++)

static inline void deallocate_buffer(buffer b)
{
    deallocate(b->h, b->contents, b->capacity);
    deallocate(b->h, b, sizeof(struct buffer));
}
