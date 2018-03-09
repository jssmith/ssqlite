#ifndef TCBL_RUNTIME_H
#define TCBL_RUNTIME_H

#include <stdlib.h>

#define tcbl_malloc(__h, __b) (malloc(__b))
#define tcbl_free(__h, __x, __len) (free(__x))


#endif //TCBL_RUNTIME_H
