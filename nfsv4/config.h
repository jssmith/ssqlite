#include <stdlib.h>

static inline int config_boolean(char *name, int def)
{
    // xxx parse this! so we can turn off default true
    return getenv(name)?1:def;
}

static inline unsigned long config_u64(char *name, unsigned long def)
{
    bytes result = 0;
    char *x = getenv(name);
    if (!x) return def;
    for (char *i = x; *i; i++) result = result + 10 * (*i - '0');
    return result;
}
