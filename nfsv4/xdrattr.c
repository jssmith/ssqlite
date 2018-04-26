#include <nfs4_internal.h>

static inline s8 digit_of(u8 x)
{
    if ((x <= 'f') && (x >= 'a')) return(x - 'a' + 10);
    if ((x <= 'F') && (x >= 'A')) return(x - 'A' + 10);
    if ((x <= '9') && (x >= '0')) return(x - '0');
    return(-1);
}

status parse_int(buffer b, u32 *result)
{
  *result = 0;
  int len = read_beu32(b);

  for (int i = 0; i < len ; i++) {
      int v = digit_of(*(u8 *) (b->contents + b->start + i));
      if (v >= 0) 
          *result = (*result * 10) + v;
      else
          return error(NFS4_EINVAL, "bad digit");
  }
  b->start += pad(len, 4);
  return NFS4_OK;
}

status parse_attrmask(buffer b, buffer dest)
{
    u32 count = read_beu32(b);
    for (int i = 0 ; i < count; i++) {
        u32 m = read_beu32(b);
        if (dest) {
            for (int j = 0 ; j < 32; j++) {
                if (m  & (1 << j)) bitvector_set(dest, i*32 + j);
            }
        }
    }
    return NFS4_OK;
}

status push_fattr_mask(rpc r, u64 mask)
{
    int count = 0;
    if (mask) count++;
    if (mask > (1ull<<32)) count ++;
    push_be32(r->b, count);
    u64 k = mask;
    for (int i = 0; i < count ; i++) {
        push_be32(r->b, (u32)k);
        k>>=32;
    }
    return NFS4_OK;
}


    
// just like in stat, we never set ino, size, access time or modify time
status push_fattr(rpc r, nfs4_properties p)
{
    buffer u = alloca_buffer(20);
    buffer g = alloca_buffer(20);    
    push_fattr_mask(r, p->mask);
    u64 len = 0;

    // a more general map of length functions, or an intermediate buffer
    if (p->mask & NFS4_PROP_MODE) len += 4;
    if (p->mask & NFS4_PROP_TYPE) len += 4;
    if (p->mask & NFS4_PROP_SIZE) len += 8;        
    if (p->mask & NFS4_PROP_UID) {
        format_number(u, p->user, 10, 1);
        len += 4 + pad(length(u), 4);
    }
    if (p->mask & NFS4_PROP_GID) {
        format_number(g, p->group, 10, 1);
        len += 4 + pad(length(g), 4);
    }
    push_be32(r->b, len);
    
    if (p->mask & NFS4_PROP_TYPE) push_be32(r->b, p->type);
    if (p->mask & NFS4_PROP_MODE) push_be32(r->b, p->mode);
    if (p->mask & NFS4_PROP_SIZE) push_be64(r->b, p->size);    
    if (p->mask & NFS4_PROP_UID)  push_string(r->b, u->contents, length(u));
    if (p->mask & NFS4_PROP_GID)  push_string(r->b, g->contents, length(g));

    return NFS4_OK;
}


status parse_fattr(buffer b, nfs4_properties p)
{
    int masklen = read_beu32(b);
    u64 maskword = 0; // bitstring
    // need a table of throwaway lengths
    // can destructive use first bit set to cut this down
    for (int i = 0; i < masklen; i++) {
        u64 f = read_beu32(b);
        maskword |=  f << (32*i);
    }
    read_beu32(b); // the opaque length
    for (int j = 0; j < 64; j++) {
        if (maskword & (1ull<<j)) {
            // more general typecase
            switch(1ull<<j) {
            case NFS4_PROP_MODE:
                p->mode = read_beu32(b);
                break;
                // nfs4 sends these as strings, but expects them in rpc
                // as ints..and they are always strings of ints
            case NFS4_PROP_UID:
                check(parse_int(b, &p->user));
                break;
            case NFS4_PROP_GID:
                check(parse_int(b, &p->group));
                break;
            case NFS4_PROP_SIZE:
                p->size = read_beu64(b);
                break;
            case NFS4_PROP_TYPE:
                p->type = read_beu32(b);
                break;                
            case NFS4_PROP_ACCESS_TIME:
                check(read_time(b, (ticks *)&p->access_time));
                break;
            case NFS4_PROP_MODIFY_TIME:
                check(read_time(b, (ticks *)&p->modify_time));
                break;
            default:
                printf ("why not supporto\n");
                return error(NFS4_EINVAL, "remote attribute not suported");
            }
            
        }
    }
    return NFS4_OK;
}

status parse_getattr(buffer b, nfs4_properties p)
{    
    verify_and_adv(b, OP_GETATTR);
    verify_and_adv(b, 0);
    return parse_fattr(b, p);
}
