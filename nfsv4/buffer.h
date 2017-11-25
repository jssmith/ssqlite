typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;
typedef u32 bytes;
typedef void *heap;
#include <mcheck.h>

static char nibbles[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static inline u32 pad(u32 a, u32 to)
{
    return ((((a-1)/to)+1)*to);
}

typedef struct buffer {
    bytes start;
    bytes end;
    bytes length;
    void *contents;
} *buffer;

typedef void *heap;

#define allocate(__h, __b) (malloc(__b))
#define deallocate(__h, __x, __len) (free(__x))


static bytes length(buffer b) 
{
    return b->end - b->start;
}
    

static buffer allocate_buffer(heap h, bytes length){
    buffer b = allocate(h, sizeof(struct buffer));
    memset(b, 0, sizeof(struct buffer));    
    b->length = length + 100;
    b->contents = allocate(h, b->length);
    return b;
}



static inline void buffer_extend(buffer b, bytes len)
{
    // pad to pagesize

    if (b->length < (b->end + len)) {
        bytes oldlen = b->length;
        b->length = 2*(oldlen+len);
        void *new =  allocate(b->h, b->length);

        memcpy(new, b->contents + b->start, length(b));        
        deallocate(b->h, b->contents, oldlen);
        b->end = b->end - b->start;
        b->start = 0;
        b->contents = new;
    }
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

static inline void push_be32(buffer b, u32 w) {
    buffer_extend(b, 4);
    *(u32 *)(b->contents + b->end) = htonl(w);
    b->end += 4;
}

static inline void push_be64(buffer b, u64 w) {
    buffer_extend(b, 8);
    *(u32 *)(b->contents + b->end) = htonl(w>>32);
    *(u32 *)(b->contents + b->end + 4) = htonl(w&0xffffffffull);
    b->end += 8;
}

static u32 read_beu32(buffer b)
{
    if (length(b) < 4 ) {
        printf("out of data!\n");
    }
    u32 v = ntohl(*(u32*)(b->contents + b->start));
    b->start += 4;
    return v;
}

static u64 read_beu64(buffer b)
{
    if (length(b) < 8 ) {
        printf("out of data!\n");
    }
    u64 v = ntohl(*(u32*)(b->contents + b->start));
    u64 v2 = ntohl(*(u32*)(b->contents + b->start + 4));    
    b->start += 8;
    
    return v<<32 | v2;
}
