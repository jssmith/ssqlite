#include <stdlib.h>
#include <stdio.h>

static inline int config_boolean(char *name, int def)
{
    char* value = getenv(name);
    if (value == 0) {
        return def;
    }

    if (strcmp(value, "0") == 0) {
        return 0;
    }

    if (strcmp(value, "1") == 0) {
        return 1;
    }

    return def;
}

static inline unsigned long config_u64(char *name, unsigned long def)
{
    bytes result = 0;
    char *x = getenv(name);
    if (!x) return def;
    for (char *i = x; *i; i++) result = result + 10 * (*i - '0');
    return result;
}
