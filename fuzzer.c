#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xjson.h"

__AFL_FUZZ_INIT();

int main()
{

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF; // must be after __AFL_INIT
                                                  // and before __AFL_LOOP!
    char pool[65536];
    xj_error  error;

    while(__AFL_LOOP(10000)) 
    {
        int len = __AFL_FUZZ_TESTCASE_LEN;

        xj_alloc *alloc = xj_alloc_using(pool, sizeof(pool), 4096, NULL);
        assert(alloc != NULL);
        
        xj_decode(buf, len, alloc, &error);
        
        xj_alloc_del(alloc);
    }
    return 0;
}
