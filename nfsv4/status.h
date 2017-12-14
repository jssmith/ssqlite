

typedef struct status *status;


#define STATUS_OK ((void *)0)

static inline boolean is_ok(status s) {
    return s == STATUS_OK;
}

char *status_description(status s);
char *status_string(status x);

