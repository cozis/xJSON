#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xjson.h"

#define TEST(src_) { .line = __LINE__, .src = src_ }

static const struct {
    int        line;
    const char *src;
} tests[] = {
    TEST("null"),
    TEST("\t\n null \t\n"),
    TEST("true"),
    TEST("\t\n true \t\n"),
    TEST("false"),
    TEST("\t\n false \t\n"),
};

#undef TEST

_Bool json_strings_match(const char *A, const char *B)
{
    int Ai = 0, Bi = 0;

    do
    {
        while(isspace(A[Ai])) Ai += 1;
        while(isspace(B[Bi])) Bi += 1;
        if(A[Ai] != B[Bi]) return 0;
        Ai += 1;
        Bi += 1;
    } 
    while(A[Ai] != '\0');
    return 1;
}

int main()
{
    char pool[65536];
    xj_error  error;

    int total = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for(int i = 0; i < total; i += 1)
    {
        xj_alloc *alloc = xj_newAllocUsing(pool, sizeof(pool), 0, NULL);
        assert(alloc != NULL);

        xj_value *val = xj_decode(tests[i].src, -1, alloc, &error);

        if(val == NULL)
        {
            fprintf(stderr, "Failed to parse (%s)\n", error.message);
            return 1;
        }
        
        int size;
        char *serialized = xj_encode(val, &size);
        assert(serialized != NULL);

        if(json_strings_match(serialized, tests[i].src))
        {
            //fprintf(stdout, "Passed.\n");
            passed += 1;
        }
        else
        {
            //fprintf(stderr, "Failed.\n");
        }

        free(serialized);
    }

    fprintf(stdout, "passed: %d, failed: %d, total: %d\n", passed, total - passed, total);
    return 0;
}
