#include <nfs4_internal.h>

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

status parse_attrmask(void *z, buffer dest)
{
    buffer b = z;
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
    if (p->mask & NFS4_PROP_USER) {
        format_number(u, p->user, 10, 1);
        len += 4 + pad(buffer_length(u), 4);
    }
    if (p->mask & NFS4_PROP_GROUP) {
        format_number(g, p->group, 10, 1);
        len += 4 + pad(buffer_length(g), 4);
    }
    push_be32(r->b, len);
    
    if (p->mask & NFS4_PROP_TYPE) push_be32(r->b, p->type);
    if (p->mask & NFS4_PROP_MODE) push_be32(r->b, p->mode);
    if (p->mask & NFS4_PROP_SIZE) push_be64(r->b, p->size);    
    if (p->mask & NFS4_PROP_USER)  push_string(r->b, u->contents, buffer_length(u));
    if (p->mask & NFS4_PROP_GROUP)  push_string(r->b, g->contents, buffer_length(g));

    return NFS4_OK;
}


status parse_fattr(void *z, buffer b)
{
    nfs4_properties p = z;
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
            case NFS4_PROP_USER:
                check(parse_int(b, &p->user));
                break;
            case NFS4_PROP_GROUP:
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


void merge_properties(nfs4_properties dest,  nfs4_properties specified, nfs4_properties backing)
{
    dest->mode = (specified->mask &NFS4_PROP_MODE) ? specified->mode : backing->mode;
    dest->user = (specified->mask &NFS4_PROP_USER) ? specified->user : backing->user;
    dest->group = (specified->mask &NFS4_PROP_GROUP) ? specified->group : backing->group;            
}
