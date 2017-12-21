typedef struct codepoint {
    char *description;
    u32 value;
} *codepoint;

// only one in flight at a time :/
static char temp_set[1024];

static inline char *codepoint_set_string(codepoint set, u64 flags)
{
    int offset = 0;
    for (int i=0; (set[i].description != "") ; i++) {
        if (flags&set[i].value) {
            int len = strlen(set[i].description);
            if (offset) temp_set[offset++] = '|';
            memcpy(temp_set + offset,  set[i].description, len);
            offset += len;
        }
    }
    temp_set[offset] = 0;
    return temp_set;
}

static inline char *codestring(codepoint set, u32 value)
{
    int i;
    for (i=0; (set[i].description != "") && (set[i].value != value) ; i++);
    if (set[i].description == ""){
        return "unknown error";
    } else {
        return set[i].description;
    }
}


