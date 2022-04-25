#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xjson.h"

static char *load_file(const char *path, int *len);

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Error: Missing file\n");
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int   size;
    char *data;

    data = load_file(argv[1], &size);
    if(data == NULL)
    {
        fprintf(stderr, "Error: Failed to load file\n");
        return 1;
    }

    char pool[65536];
    xj_alloc *alloc;

    alloc = xj_alloc_using(pool, sizeof(pool), 4096, NULL);
    assert(alloc != NULL);

    xj_error error;
    xj_value *val = xj_decode(data, size, alloc, &error);

    if(val == NULL)
    {
        if(error.off < 0)
            fprintf(stderr, "Error: %s\n", error.message);
        else
            fprintf(stderr, "Error %s:%d:%d: %s\n", argv[1], error.row+1, error.col+1, error.message);
    }
    else
        fprintf(stderr, "OK\n");

    xj_alloc_del(alloc);
    free(data);
    return 0;
}

static char *load_file(const char *path, int *len)
{
    FILE *fp = fopen(path, "rb");

    if(fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    int len_ = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = malloc(len_ + 1);

    if(data == NULL)
    {
        fclose(fp);
        return NULL;
    }

    assert(len_ >= 0);
    if(fread(data, 1, len_, fp) != (unsigned int) len_)
    {
        free(data);
        fclose(fp);
        return NULL;
    }

    data[len_] = '\0';
    fclose(fp);

    if(len)
        *len = len_;
    return data;
}