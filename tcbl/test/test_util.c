#include "test_runtime.h"

void prep_data(void* data, size_t data_len, uint64_t seed)
{
    // TODO make sure this is good
    uint64_t m = ((uint64_t) 2)<<32;
    uint64_t a = 1103515245;
    uint64_t c = 12345;
    for (int i = 0; i < data_len; i++) {
        seed = (a * seed + c) % m;
        ((char*) data)[i] = (char) seed;
    }
}
