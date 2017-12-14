static inline boolean config_boolean(char *name, boolean def)
{
    // xxx parse this! so we can turn off default true
    return getenv(name)?true:def;
}

static inline u64 config_u64(char *name, u64 def)
{
    u64 result = 0;
    char *x = getenv(name);
    if (!x) return def;
    for (char *i = x; *i; i++) result = result + 10 * (*i - '0');
    return result;
}
