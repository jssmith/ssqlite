#include <nfs4_internal.h>

typedef u8 character;
static char *hex_digits="0123456789abcdef";

extern struct codepoint nfserrs[];

void format_number(buffer s, u64 x, int base, int pad)
{
    if ((x > 0) || (pad > 0)) {
        format_number(s, x/base, base, pad - 1);
        push_character(s, hex_digits[x%base]);
    }
}

void empty_buffer(buffer b)
{
    b->start = b->end = 0;
}

// should entertain a registration method with a type and a character and a function pointer
// or maybe just float this up to runtime

void vbprintf(buffer s, buffer fmt, va_list ap)
{
    character i;
    int state = 0;
    int base = 0;
    int pad;
    int count = 0;

    foreach_character(i, fmt) {
        switch (state){
        case 2:
            for (int j = 0; j < count; j++)
                push_character(s, i);
            state = 0;
            break;

        case 0:
            base = 10;
            pad = 0;
            if (i == '%') state = 3;
            else push_character(s, i);
            break;

        case 1:
            if ((i >= '0') && (i <= '9')) {
                pad = pad * 10 + (i - '0');
                break;
            } else {
                state = 3;
            }

        case 3:
            switch (i) {
            case '0':
                state = 1;
                break;

            case '%':
                push_character(s, '\%');
                break;

                //            case 't':
                //                print_time(s, varg(ap, ticks));
                //                break;

            case 'b':
                buffer_concat(s, va_arg(ap, buffer));
                break;

            case 'n':
                count = va_arg(ap, unsigned int);
                state = 2;
                break;

            case 'c':
                push_character(s, va_arg(ap, int));
                break;

            case 's':
                {
                    char *c = va_arg(ap, char *);
                    if (!c) c = (char *)"(null)";
                    int len = strlen(c);
                    for (int i =0 ; i < pad; i++)
                        push_character(s, ' ');
                    pad = 0;
                    for (; *c; c++)
                        push_character(s, *c);
                }
                break;

            case 'S':
                {
                    unsigned int x = va_arg(ap, unsigned int);
                    for (int i =0 ; i < x; i++) push_character(s, ' ');
                    break;
                }

            // vector of strings
            case 'v':
                {
                    vector v = va_arg(ap, buffer);
                    buffer i;
                    boolean start=true;
                    push_character(s, '[');
                    vector_foreach(i, v) {
                        if (start) {
                            start = false;
                        } else {
                            push_character(s, ',');
                            push_character(s, ' ');
                        }
                        buffer_concat(s, i);
                    }
                    push_character(s, ']');
                    break;
                }
                
            case 'p':
                pad = 16;
                unsigned long x = va_arg(ap, unsigned long);
                format_number(s, x, 16, pad?pad:1);
                break;

            case 'l':
                pad = 0;
                unsigned long z = va_arg(ap, unsigned long);
                format_number(s, z, 10, pad?pad:1);
                break;

            case 'x':
                base=16;

            case 'o':
                if (base == 10) base=8;
                
            case 'u':
                {
                    unsigned int x = va_arg(ap, unsigned int);
                    format_number(s, x, base, pad?pad:1);
                    break;
                }

            case 'C':
                {
                    codepoint c = va_arg(ap, codepoint);
                    u64 v = va_arg(ap, u64);
                    char *cp = codestring(c, v);
                    push_bytes(s, cp, strlen(cp));
                    break;
                }
                
            case 'X':
                {
                    buffer b = va_arg(ap, buffer);
                    for (int i = 0; i < buffer_length(b); i++) {
                        u8 c = *(u8 *)(b->contents + b->start + i);
                        push_character(s, hex_digits[c>>4]);
                        push_character(s, hex_digits[c&15]);
                    }
                    break;
                }
            case 'e':
                {
                    status st = va_arg(ap, status);
                    if (st) {
                        char *cp = codestring(nfserrs, st->error);
                        push_bytes(s, cp, strlen(cp));
                        push_character(s, ' ');
                        push_bytes(s, st->description->contents, buffer_length(st->description));
                    } else  push_bytes(s, "OK", 2);
                    break;                    
                }
                
            case 'd': case 'i':
                {
                    int x = va_arg(ap, int);
                    if (x <0){
                        push_character(s, '-');
                        x = -x;
                    }
                    format_number(s, (unsigned int)x, base, pad?pad:1);
                    break;
                }
            default:
                break;
            }
            // badness
            if (state == 3)
                state = 0;
            break;
        }
    }
}


buffer aprintf(heap h, char *fmt, ...)
{
    buffer b = allocate_buffer(h, 80);
    va_list ap;
    buffer f = alloca_wrap_buffer(fmt, strlen(fmt));
    va_start (ap, fmt);
    vbprintf(b, f, ap);
    va_end(ap);
    return(b);
}

void bprintf(buffer b, char *fmt, ...)
{
    va_list ap;
    buffer f = alloca_wrap_buffer(fmt, strlen(fmt));
    va_start (ap, fmt);
    vbprintf(b, f, ap);
    va_end(ap);
}

void bbprintf(buffer b, buffer fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vbprintf(b, fmt, ap);
    va_end(ap);
}
