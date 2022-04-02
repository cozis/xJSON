#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "xjson.h"

int main()
{
    char pool[4096];
    xj_error  error;
    xj_alloc *alloc;

    alloc = xj_newAllocUsing(pool, sizeof(pool), 0, NULL);
    assert(alloc != NULL);

    xj_value *val = xj_decode("{}", -1, alloc, &error);

    if(val == NULL)
    {
        fprintf(stderr, "Failed to parse (%s)\n", error.message);
        return 1;
    }
    fprintf(stdout, "Parsed.\n");

    int size;
    char *serialized = xj_encode(val, &size);
    fprintf(stdout, "(%d) %s\n", size, serialized);
    free(serialized);
    return 0;
}