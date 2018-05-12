
#define push_boolean(__b, __x) push_be32(__b, __x) 

#define push_be32(__b, __w) {\
    u32 __v = __w;\
    buffer_extend(__b, 4);\
    *(u32 *)(__b->contents + __b->end) = htonl(__v);\
    __b->end += 4;\
  }

#define push_be64(__b, __w) {\
    buffer_extend(__b, 8);\
    *(u32 *)(__b->contents + __b->end) = htonl(__w>>32);\
    *(u32 *)(__b->contents + __b->end + 4) = htonl(__w&0xffffffffull);\
    __b->end += 8;\
}

#define read_beu32(__b) ({\
    if ((__b->end - __b->start) < 4 ) return error(NFS4_PROTOCOL, "out of data"); \
    u32 v = ntohl(*(u32*)(__b->contents + __b->start));\
    __b->start += 4;\
    v;})

#define read_beu64(__b) ({\
    if ((__b->end - __b->start) < 8 ) return error(NFS4_PROTOCOL, "out of data"); \
    u64 v = ntohl(*(u32*)(__b->contents + __b->start));\
    u64 v2 = ntohl(*(u32*)(__b->contents + __b->start + 4));    \
    __b->start += 8;                                        \
    v<<32 | v2;})


static inline void push_fixed_string(buffer b, char *x, u32 length) {
    u32 plen = pad(length, 4) - length;
    buffer_extend(b, length + plen + 4);
    memcpy(b->contents + b->end, x, length);
    b->end += length;
    if (plen) {
        memset(b->contents + b->end, 0, plen);
        b->end += plen;
    }
}

static inline void push_string(buffer b, char *x, u32 length)
{
    u32 plen = pad(length, 4) - length;
    buffer_extend(b, 4);
    push_be32(b, length);
    push_fixed_string(b, x, length);
}

static inline status read_buffer(buffer b, void *dest, u32 len)
{
    if (dest != (void *)0) memcpy(dest, b->contents + b->start, len);
    b->start += len;
    return NFS4_OK;
}

#define verify_and_adv(__b , __v) { u32 v2 = read_beu32(__b); if (__v != v2) return error(NFS4_PROTOCOL, "encoding mismatch expected %x got %x at %s:%d", __v, v2, __FILE__, (u64)__LINE__);}

status parse_dirent(buffer b, nfs4_properties p, int *more, u64 *cookie);
void push_lock(rpc r, stateid sid, int loctype, bytes offset, bytes length, stateid dest);
void push_unlock(rpc r, stateid sid, int loctype, bytes offset, bytes length);
status read_time(buffer b, ticks *dest);
status parse_attrmask(void *, buffer dest);
void push_auth_null(buffer b);
void push_auth_sys(buffer b, u32 uid, u32 gid);
u64 push_read(rpc r, bytes offset, buffer b, stateid sid);
u64 push_write(rpc r, bytes offset, buffer b, stateid sid);
status push_create(rpc r, nfs4_properties p);
status push_fattr(rpc r, nfs4_properties p);
status parse_fattr(void *, buffer b);
status parse_filehandle(void *z, buffer b);
status push_fattr_mask(rpc r, attrmask m);
void push_stateid(rpc r, stateid s);
status parse_stateid(void *, buffer);
void push_sequence(rpc r);
void push_session_id(rpc r, u8 *session);
status discard_string(buffer b);
