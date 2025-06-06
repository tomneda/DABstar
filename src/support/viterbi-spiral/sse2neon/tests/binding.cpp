#include "binding.h"

#include <cstdio>
#include <stdlib.h>

namespace SSE2NEON
{
void *platformAlignedAlloc(size_t size)
{
    void *address;
    i32 ret = posix_memalign(&address, 16, size);
    if (ret != 0) {
        fprintf(stderr, "Error at File %s line number %d\n", __FILE__,
                __LINE__);
        exit(EXIT_FAILURE);
    }
    return address;
}

void platformAlignedFree(void *ptr)
{
    free(ptr);
}

}  // namespace SSE2NEON
