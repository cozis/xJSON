#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "xjson.h"

typedef struct chunk_t chunk_t;
struct chunk_t {
    chunk_t *prev;
    _Alignas(void*) char body[];
};

struct xj_alloc {
    void (*free)(void*);
    chunk_t *tail;
    int tail_used;
    int tail_size;
    int  ext_size;
};

xj_alloc *xj_newAlloc(int size, int ext)
{
    assert(size >= 0 && ext >= 0);
    
    void *temp = malloc(sizeof(xj_alloc) + sizeof(chunk_t) + size);
    
    if(temp == NULL)
        return NULL;

    return xj_newAllocUsing(temp, sizeof(xj_alloc) + sizeof(chunk_t) + size, ext, free);
}

xj_alloc *xj_newAllocUsing(void *mem, int size, int ext, void (*free)(void*))
{
    assert(mem != NULL && size >= 0 && ext >= 0);

    if((unsigned int) size < sizeof(xj_alloc) + sizeof(chunk_t))
        return NULL;

    xj_alloc *alloc = mem;
    alloc->free = free;
    alloc->tail = (chunk_t*) (alloc + 1);
    alloc->tail->prev = NULL;
    alloc->tail_used = 0;
    alloc->tail_size = size - sizeof(xj_alloc) - sizeof(chunk_t);
    alloc->ext_size = ext;
    return alloc;
}

void xj_freeAlloc(xj_alloc *alloc)
{
    if(alloc->free != NULL)
        alloc->free(alloc);
}

unsigned long long next_aligned(unsigned long long n)
{
    return n & 7 ? n & ~7 : n;
}

void *xj_bpalloc(xj_alloc *alloc, int size)
{
    assert(size >= 0);

    alloc->tail_used = next_aligned(alloc->tail_used);

    if(alloc->tail_used + size > alloc->tail_size)
        {
            // Create a new chunk and append it
            // to the allocator.
            
            if(alloc->ext_size == 0)
                return NULL;

            chunk_t *chunk = malloc(sizeof(chunk_t) + alloc->ext_size);

            if(chunk == NULL)
                return NULL;

            chunk->prev = alloc->tail;
            alloc->tail = chunk;
            alloc->tail_used = 0;
            alloc->tail_size = alloc->ext_size;
        }

    return alloc->tail->body + (alloc->tail_used += size);
}

xj_value *xj_newNull(xj_alloc *alloc)
{
    xj_value *x = xj_bpalloc(alloc, sizeof(xj_value));
    if(x != NULL)
    {
        x->type = XJ_NULL;
        x->size = -1;
        x->next = NULL;
        x->key = NULL;
    }
    return x;
}

xj_value *xj_newBool(xj_bool val, xj_alloc *alloc)
{
    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_BOOL;
        x->as_bool = val;
    }
    return x;
}

xj_value *xj_newInt(xj_i64 val, xj_alloc *alloc)
{
    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_INT;
        x->as_int = val;
    }
    return x;
}

xj_value *xj_newFloat(xj_f64 val, xj_alloc *alloc)
{
    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_FLOAT;
        x->as_float = val;
    }
    return x;
}

xj_value *xj_newString(const char *str, int len, xj_alloc *alloc)
{
    if(str == NULL) str = "";
    if(len < 0) len = strlen(str);

    char *copy = xj_strdup(str, len, alloc);
    
    if(copy == NULL) 
        return NULL;

    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_STRING;
        x->size = len;
        x->as_string = copy;
    }
    return x;
}

xj_value *xj_newArray__nocheck(xj_value *head, int count, xj_alloc *alloc)
{
    if(count < 0)
    {
        count = 0;
        xj_value *curs = head;
        while(curs != NULL)
        {
            count += 1;
            curs = curs->next;
        }
    }

    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_ARRAY;
        x->size = count;
        x->as_array = head;
    }
    return x;
}

xj_value *xj_newArray(xj_value *head, xj_alloc *alloc)
{
    int count = 0;
    xj_value *curs = head;
    while(curs != NULL)
    {
        if(curs->key != NULL)
        {
            /* Array child has a 
               key associated to it? */
            return NULL;
        }
        count += 1;
        curs = curs->next;
    }

    return xj_newArray__nocheck(head, count, alloc);
}

xj_value *xj_newObject__nocheck(xj_value *head, int count, xj_alloc *alloc)
{
    if(count < 0)
    {
        count = 0;
        xj_value *curs = head;
        while(curs != NULL)
        {
            count += 1;
            curs = curs->next;
        }
    }

    xj_value *x = xj_newNull(alloc);
    if(x != NULL)
    {
        x->type = XJ_OBJECT;
        x->size = count;
        x->as_object = head;
    }
    return x;
}

xj_value *xj_newObject(xj_value *head, xj_alloc *alloc)
{
    int count = 0;
    xj_value *curs = head;
    while(curs != NULL)
    {
        if(curs->key == NULL)
        {
            /* Object child has no 
               key associated to it! */
            return NULL;
        }

        xj_value *curs2 = head;
        while(curs2 != curs)
        {
            if(!strcmp(curs->key, curs2->key))
            {
                /* Duplicate key. */
                return NULL;
            }
            curs2 = curs2->next;
        }

        count += 1;
        curs = curs->next;
    }

    return xj_newObject__nocheck(head, count, alloc);
}

char *xj_strdup(const char *str, int len, xj_alloc *alloc)
{
    assert(str != NULL);

    if(len < 0)
        len = strlen(str);

    char *copy = xj_bpalloc(alloc, len+1);

    if(copy != NULL)
    {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

static void xj_report(xj_error *error, const char *fmt, ...)
{
    if(error != NULL)
    {
        int k;
        va_list va;
        va_start(va, fmt);
        k = vsnprintf(error->message, sizeof(error->message), fmt, va);
        va_end(va);

        assert(k >= 0);

        error->truncated = (k >= (int) sizeof(error->message)-1);
        error->occurred = 1;
    }
}

typedef struct {
    const char *str;
    int      i, len;
    xj_alloc *alloc;
    xj_error *error;
} context_t;

static void *parseString(context_t *ctx, _Bool raw)
{
    assert(ctx->i < ctx->len && ctx->str[ctx->i] == '"');

    ctx->i += 1; // Skip '"'.

    int start = ctx->i;

    while(ctx->i < ctx->len && ctx->str[ctx->i] != '"')
        ctx->i += 1;

    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended inside a string");
        return NULL;
    }

    int end = ctx->i;

    ctx->i += 1; // Skip '"'.

    return raw ? (void*)    xj_strdup(ctx->str + start, end - start, ctx->alloc) 
               : (void*) xj_newString(ctx->str + start, end - start, ctx->alloc);
}

static xj_value *parseNumber(context_t *ctx)
{
    assert(ctx->i < ctx->len && isdigit(ctx->str[ctx->i]));

    xj_i64 parsed = 0;

    while(ctx->i < ctx->len && isdigit(ctx->str[ctx->i]))
    {
        if(parsed > (INT64_MAX - ctx->str[ctx->i] + '0') / 10)
            {
                /* Overflow */
                xj_report(ctx->error, "Integer would overflow");
                return NULL;
            }

        parsed = parsed * 10 + ctx->str[ctx->i] - '0';

        ctx->i += 1;
    }

    xj_bool followed_by_dot = ctx->i+1 < ctx->len && ctx->str[ctx->i] == '.' && isdigit(ctx->str[ctx->i+1]);

    if(followed_by_dot)
    {
        ctx->i += 1; // Skip '.'.

        xj_f64 parsed2 = parsed, f = 1.0;

        while(ctx->i < ctx->len && isdigit(ctx->str[ctx->i]))
        {
            f /= 10;
            parsed2 += f * (ctx->str[ctx->i] - '0');
            ctx->i += 1;
        }

        return xj_newFloat(parsed2, ctx->alloc);
    }

    return xj_newInt(parsed, ctx->alloc);
}

static xj_value *parseValue(context_t *ctx);

static xj_value *parseArray(context_t *ctx)
{
    assert(ctx->i < ctx->len && ctx->str[ctx->i] == '[');

    ctx->i += 1; // Skip ']'.

    // Skip whitespace.
    while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
        ctx->i += 1;

    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended inside an array, right after the first '['");
        return NULL;
    }

    if(ctx->str[ctx->i] == ']') /* Empty array */
        return xj_newArray__nocheck(NULL, 0, ctx->alloc);

    xj_value  *head = NULL;
    xj_value **tail = &head;
    int count = 0;

    while(1)
    {
        xj_value *child = parseValue(ctx);

        if(child == NULL)
            return NULL;

        // Skip whitespace.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside an array, right after the %dth child", count+1);
            return NULL;
        }

        *tail = child;
        tail = &child->next;
        count += 1;

        if(ctx->str[ctx->i] == ']')
            break;
        
        if(ctx->str[ctx->i] != ',')
        {
            xj_report(ctx->error, "Bad character '%c' inside of an array", ctx->str[ctx->i]);
            return NULL;
        }

        ctx->i += 1; // Skip ','.

        // Skip whitespace.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside an array, right after the ',' after the %dth child", count+1);
            return NULL;
        }
    }

    ctx->i += 1; // Skip ']'.

    return xj_newArray__nocheck(head, count, ctx->alloc);
}

static xj_value *parseObject(context_t *ctx)
{
    assert(ctx->i < ctx->len && ctx->str[ctx->i] == '{');

    ctx->i += 1; // Skip '{'.

    // Skip whitespace.
    while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
        ctx->i += 1;

    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended inside an object, right after the first '{'");
        return NULL;
    }

    if(ctx->str[ctx->i] == '}') /* Empty object */
        return xj_newObject__nocheck(NULL, 0, ctx->alloc);

    xj_value  *head = NULL;
    xj_value **tail = &head;
    int count = 0;

    while(1)
    {
        if(ctx->str[ctx->i] != '"')
        {
            xj_report(ctx->error, "Bad character '%c' where a string was expected");
            return NULL;
        }

        char *key = parseString(ctx, 1);

        if(key == NULL)
            return NULL;

        // Skip whitespace before ':'.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside an object, right after the %dth child's key", count+1);
            return NULL;
        }

        if(ctx->str[ctx->i] != ':')
        {
            xj_report(ctx->error, "Bad character '%c' where ':' was expected");
            return NULL;
        }

        ctx->i += 1; // Skip the ':'.

        // Skip whitespace after ':'.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        xj_value *child = parseValue(ctx);

        if(child == NULL)
            return NULL;

        // Skip whitespace.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside an object, right after the %dth child", count+1);
            return NULL;
        }

        child->key = key;

        *tail = child;
        tail = &child->next;
        count += 1;

        if(ctx->str[ctx->i] == '}')
            break;
        
        if(ctx->str[ctx->i] != ',')
        {
            xj_report(ctx->error, "Bad character '%c' inside of an object", ctx->str[ctx->i]);
            return NULL;
        }

        ctx->i += 1; // Skip ','.

        // Skip whitespace.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside an object, right after the ',' after the %dth child", count+1);
            return NULL;
        }
    }

    ctx->i += 1; // Skip '}'.

    return xj_newObject__nocheck(head, count, ctx->alloc);
}

static xj_value *parseValue(context_t *ctx)
{
    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended where a value was expected");
        return NULL;
    }
    
    assert(!isspace(ctx->str[ctx->i]));

    char c = ctx->str[ctx->i];

    if(c == '"')
        return parseString(ctx, 0);

    if(isdigit(c))
        return parseNumber(ctx);

    if(c == '[')
        return parseArray(ctx);

    if(c == '{')
        return parseObject(ctx);

    static const char kword_null [] = "null";
    static const char kword_true [] = "true";
    static const char kword_false[] = "false";
    const char *kword;
    int         kwlen;

    if(c == 'n')
    {
        kword = kword_null;
        kwlen = sizeof(kword_null)-1;
    }

    if(c == 't')
    {
        kword = kword_true;
        kwlen = sizeof(kword_true)-1;
    }

    if(c == 'f')
    {
        kword = kword_false;
        kwlen = sizeof(kword_false)-1;
    }

    if(ctx->i + kwlen <= ctx->len && !strncmp(ctx->str + ctx->i, kword, kwlen))
    {
        ctx->i += kwlen;
        switch(c)
        {
            case 'n': return xj_newNull(ctx->alloc);
            case 't': return xj_newBool(1, ctx->alloc);
            case 'f': return xj_newBool(0, ctx->alloc);
        }
        /* UNREACHABLE */
    }

    if(ctx->i + kwlen > ctx->len)
    {
        xj_report(ctx->error, "String ended unexpectedly");
        return NULL;
    }

    {
        int p = 0;
        while(kword[p] == ctx->str[ctx->i+p])
            p += 1;
        ctx->i += p;
    }

    xj_report(ctx->error, "Bad character '%c'", ctx->str[ctx->i]);
    return NULL;
}

xj_value *xj_decode(const char *str, int len, 
                    xj_alloc *alloc, xj_error *error)
{
    if(str == NULL) str = "";
    if(len < 0) len = strlen(str);

    int i = 0;

    // Skip whitespace
    while(i < len && isspace(str[i]))
        i += 1;

    if(i == len)
    {
        xj_report(error, "The string only contains whitespace");
        return NULL;
    }

    context_t ctx = { 
        .str = str, .i = i, .len = len, 
        .alloc = alloc, .error = error };
    return parseValue(&ctx);
}

typedef struct bucket_t bucket_t;
struct bucket_t {
    bucket_t *next;
    char      body[4096-sizeof(void*)];
};

typedef struct {
    int size, used;
    bucket_t *tail, head;
} buffer_t;

static xj_bool appendString(buffer_t *buff, const char *str, int len)
{
    assert(str != NULL && len >= 0);

    if(buff->used + len > (int) sizeof(buff->tail->body))
    {
        bucket_t *buck = malloc(sizeof(bucket_t));

        if(buck == NULL)
            return 0;

        buck->next = NULL;
        buff->tail->next = buck;
        buff->tail = buck;
        buff->used = 0;
    }
    memcpy(buff->tail->body + buff->used, str, len);
    buff->used += len;
    buff->size += len;
    return 1;
}

static _Bool encodeString(const char *str, int len, buffer_t *buff)
{
    if(!appendString(buff, "\"", 1))
        return 0;

    if(!appendString(buff, str, len))
        return 0;

    if(!appendString(buff, "\"", 1))
        return 0;

    return 1;
}

static _Bool encodeValue(xj_value *val, buffer_t *buff)
{
    switch(val == NULL ? XJ_NULL : val->type)
    {
        case XJ_NULL: 
        return appendString(buff, "null", 4);
        
        case XJ_BOOL: 
        return val->as_bool 
            ? appendString(buff, "true", 4) 
            : appendString(buff, "false", 5);

        case XJ_INT:
        {
            char temp[32];
            int k = snprintf(temp, sizeof(temp), 
                            "%lld", val->as_int);
            assert(k >= 0 && k < (int) sizeof(temp));
            if(!appendString(buff, temp, k))
                return 0;
            return 1;
        }

        case XJ_FLOAT: 
        {
            char temp[32];
            int k = snprintf(temp, sizeof(temp), 
                            "%g", val->as_float);
            assert(k >= 0 && k < (int) sizeof(temp));
            if(!appendString(buff, temp, k))
                return 0;
            return 1;
        }

        case XJ_ARRAY:
        {
            if(!appendString(buff, "[", 1))
                return 0;

            xj_value *child = val->as_object;
            while(child != NULL)
            {
                if(!encodeValue(child, buff))
                    return 0;

                child = child->next;

                if(child != NULL)
                    if(!appendString(buff, ", ", 2))
                        return 0;
            }

            if(!appendString(buff, "]", 1))
                return 0;
            return 1;
        }

        case XJ_OBJECT:
        {
            if(!appendString(buff, "{", 1))
                return 0;

            xj_value *child = val->as_object;
            while(child != NULL)
            {
                if(!encodeString(child->key, strlen(child->key), buff))
                    return 0;

                if(!appendString(buff, ": ", 2))
                    return 0;

                if(!encodeValue(child, buff))
                    return 0;

                child = child->next;

                if(child != NULL)
                    if(!appendString(buff, ", ", 2))
                        return 0;
            }

            if(!appendString(buff, "}", 1))
                return 0;
            return 1;
        }

        case XJ_STRING:
        return encodeString(val->as_string, val->size, buff);
    }
    return 0;
}

char *xj_encode(xj_value *value, int *len)
{
    buffer_t buff;
    buff.size = 0;
    buff.used = 0;
    buff.tail = &buff.head;
    buff.head.next = NULL;

    _Bool ok = encodeValue(value, &buff);
    
    char *serialized = NULL;

    if(ok)
    {
        /* Serialize */

        serialized = malloc(buff.size+1);

        if(serialized != NULL)
        {
            int copied = 0;

            bucket_t *curs = &buff.head;
            while(curs->next != NULL)
            {
                memcpy(serialized + copied, 
                    curs->body, sizeof(curs->body));

                copied += sizeof(curs->body);
                curs = curs->next;
            }

            memcpy(serialized + copied, 
                curs->body, buff.used);

            serialized[buff.size] = '\0';

            if(len) 
                *len = buff.size;
        }
    }

    /* Free the buffer */
    bucket_t *curs = buff.head.next;
    while(curs != NULL)
    {
        bucket_t *next = curs->next;
        free(curs);
        curs = next;
    }

    return serialized;
}