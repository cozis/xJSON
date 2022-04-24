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

xj_alloc *xj_alloc_new(int size, int ext)
{
    assert(size >= 0 && ext >= 0);
    
    int allocated = sizeof(xj_alloc) + sizeof(chunk_t) + size;
    void *temp = malloc(allocated);
    
    if(temp == NULL)
        return NULL;

    return xj_alloc_using(temp, allocated, ext, free);
}

xj_alloc *xj_alloc_using(void *mem, int size, int ext, void (*free)(void*))
{
    assert(mem != NULL && size >= 0 && ext >= 0);

    if((unsigned int) size < sizeof(xj_alloc) + sizeof(chunk_t))
        return NULL;

    xj_alloc *alloc = mem;
    alloc->free = free;
    alloc->tail = (chunk_t*) (alloc + 1);
    alloc->tail->prev = NULL;
    alloc->tail_used = 0;
    alloc->tail_size = size - (sizeof(xj_alloc) + sizeof(chunk_t));
    alloc->ext_size = ext;
    return alloc;
}

void xj_alloc_del(xj_alloc *alloc)
{
    // Free all of the allocator's chunks,
    // with exception of the first one,
    // which is allocated with the allocator's
    // header and must be deallocated with
    // the user-provided callback.
    chunk_t *curr = alloc->tail;
    while(curr->prev != NULL)
    {
        chunk_t *prev = curr->prev;
        free(curr);
        curr = prev;
    }

    // Free the allocator header and first
    // chunk.
    if(alloc->free != NULL)
        alloc->free(alloc);
}

// Returns [n] if it's multiple of 8, else the 
// first multiple of 8 after it.
unsigned long long next_aligned(unsigned long long n)
{
    return (n & 7) ? (n & ~7) + 8 : n;
}

void *xj_bpalloc(xj_alloc *alloc, int size)
{
    assert(size >= 0);

    // Make sure the returned memory is aligned
    // to 8 bytes boundaries, which is assumed 
    // to be the a valid alignment for anything.
    alloc->tail_used = next_aligned(alloc->tail_used);
    
    // If there's not enough memory in the 
    // current chunk, allocate an extension.    
    if(alloc->tail_used + size > alloc->tail_size)
        {
            // When the user instanciated the allocator,
            // he specified an extension size of 0, which
            // means that he doesn't want the allocator
            // to grow. Therefore, we just wen out of 
            // memory!
            if(alloc->ext_size == 0)
                return NULL;

            // Either allocate a chunk of the size specified
            // by the user during the instanciation of the
            // allocator, or a bigger one if the current 
            // allocation wouldn't fit in it.
            int new_chunk_size = alloc->ext_size;

            if(new_chunk_size < size)
                new_chunk_size = size;

            chunk_t *chunk = malloc(sizeof(chunk_t) + new_chunk_size);

            if(chunk == NULL)
                return NULL;

            chunk->prev = alloc->tail;
            alloc->tail = chunk;
            alloc->tail_used = 0;
            alloc->tail_size = new_chunk_size;
        }

    // Do the bump-pointer's bumping of the pointer.
    void *addr = alloc->tail->body + alloc->tail_used;
    
    alloc->tail_used += size;

    return addr;
}

static void xj_preport(xj_error *error, const char *src, int off, const char *fmt, ...)
{
    if(error != NULL)
    {
        int row, col;
        if(src != NULL)
        {
            // Calculate column and row given
            // the source string and an index
            // in it.
            assert(off >= 0);
            col = 0;
            row = 0;
            int i = 0;
            while(i < off)
            {
                if(src[i] == '\n')
                {
                    row += 1;
                    col = 0;
                }
                else
                    col += 1;
                i += 1;
            }
        }

        int k;
        va_list va;
        va_start(va, fmt);
        k = vsnprintf(error->message, sizeof(error->message), fmt, va);
        va_end(va);

        assert(k >= 0);

        error->truncated = (k >= (int) sizeof(error->message)-1);
        error->occurred = 1;
        error->off = off;
        error->row = row;
        error->col = col;
    }
}

#define xj_report(error, fmt, ...) xj_preport(error, NULL, -1, fmt, ## __VA_ARGS__)

// Create an [xj_value] that represents the [null] JSON value.
xj_value *xj_value_null(xj_alloc *alloc, xj_error *error)
{
    xj_value *x = xj_bpalloc(alloc, sizeof(xj_value));
    if(x == NULL)
        xj_report(error, "Out of memory");
    else
    {
        x->type = XJ_NULL;
        x->size = -1;
        x->next = NULL;
        x->key = NULL;
    }
    return x;
}

// Create an [xj_value] that represents a boolean value.
xj_value *xj_value_bool(xj_bool val, xj_alloc *alloc, xj_error *error)
{
    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_BOOL;
        x->as_bool = val;
    }
    return x;
}

xj_value *xj_value_int(xj_i64 val, xj_alloc *alloc, xj_error *error)
{
    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_INT;
        x->as_int = val;
    }
    return x;
}

xj_value *xj_value_float(xj_f64 val, xj_alloc *alloc, xj_error *error)
{
    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_FLOAT;
        x->as_float = val;
    }
    return x;
}

xj_value *xj_value_string(const char *str, int len, xj_alloc *alloc, xj_error *error)
{
    if(str == NULL) str = "";
    if(len < 0) len = strlen(str);

    char *copy = xj_strdup(str, len, alloc, error);
    
    if(copy == NULL) 
        return NULL;

    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_STRING;
        x->size = len;
        x->as_string = copy;
    }
    return x;
}

xj_value *xj_value_array__nocheck(xj_value *head, int count, xj_alloc *alloc, xj_error *error)
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

    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_ARRAY;
        x->size = count;
        x->as_array = head;
    }
    return x;
}

xj_value *xj_value_array(xj_value *head, xj_alloc *alloc, xj_error *error)
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

    return xj_value_array__nocheck(head, count, alloc, error);
}

xj_value *xj_value_object__nocheck(xj_value *head, int count, xj_alloc *alloc, xj_error *error)
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

    xj_value *x = xj_value_null(alloc, error);
    if(x != NULL)
    {
        x->type = XJ_OBJECT;
        x->size = count;
        x->as_object = head;
    }
    return x;
}

xj_value *xj_value_object(xj_value *head, xj_alloc *alloc, xj_error *error)
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

    return xj_value_object__nocheck(head, count, alloc, error);
}

char *xj_strdup(const char *str, int len, xj_alloc *alloc, xj_error *error)
{
    assert(str != NULL);

    if(len < 0)
        len = strlen(str);

    char *copy = xj_bpalloc(alloc, len+1);

    if(copy == NULL)
        xj_report(error, "Out of memory");
    else
    {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

typedef struct {
    const char *str;
    int      i, len;
    xj_alloc *alloc;
    xj_error *error;
} context_t;

typedef struct {
    char *buffer;
    int size, capacity;
    char maybe[256];
} string_parsing_context_t;

_Bool spc_append(string_parsing_context_t *spc, const char *str, int len)
{
    if(spc->size + len > spc->capacity)
    {
        // Grow the buffer.

        int new_capacity = spc->capacity * 2;

        if(new_capacity < (spc->size + len))
            new_capacity = (spc->size + len);

        char *temp;

        if(spc->maybe == spc->buffer)
        {
            temp = malloc(new_capacity);
            
            if(temp == NULL)
                return 0;

            memcpy(temp, spc->buffer, spc->size);
        }
        else
        {
            temp = realloc(spc->buffer, new_capacity);
        
            if(temp == NULL)
                return 0;
        }

        spc->buffer = temp;
        spc->capacity = new_capacity;
    }

    memcpy(spc->buffer + spc->size, str, len);
    spc->size += len;
    return 1;
}

void spc_free(string_parsing_context_t *spc)
{
    if(spc->maybe != spc->buffer)
        free(spc->buffer);
}

static void *parse_string(context_t *ctx, _Bool raw)
{
    assert(ctx->i < ctx->len && ctx->str[ctx->i] == '"');

    string_parsing_context_t spc;
    {
        spc.buffer = spc.maybe;
        spc.size = 0;
        spc.capacity = sizeof(spc.maybe);
    }

    ctx->i += 1; // Skip '"'.

    while(1)
    {
        int start = ctx->i;

        while(ctx->i < ctx->len && ctx->str[ctx->i] != '\\' 
                                && ctx->str[ctx->i] != '"')
            ctx->i += 1;

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside a string");
            spc_free(&spc);
            return NULL;
        }

        int end = ctx->i;

        if(!spc_append(&spc, ctx->str + start, end - start))
        {
            xj_report(ctx->error, "Out of memory");
            spc_free(&spc);
            return NULL;
        }

        if(ctx->str[ctx->i] == '"')
            break;

        assert(ctx->str[ctx->i] == '\\');

        ctx->i += 1; // Skip '\'.

        if(ctx->i == ctx->len)
        {
            xj_report(ctx->error, "String ended inside a string");
            spc_free(&spc);
            return NULL;
        }

        char c = ctx->str[ctx->i];

        switch(c)
        {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'r': c = '\r'; break;
            case 'u': xj_preport(ctx->error, ctx->str, ctx->i, 
                        "The \\uXXXX form isn't supported yet!"); 
                      break;
        }

        if(!spc_append(&spc, &c, 1))
        {
            xj_report(ctx->error, "Out of memory");
            spc_free(&spc);
            return NULL;
        }

        ctx->i += 1; // Skip the character after the '\'.
    }

    ctx->i += 1; // Skip '"'.

    void *p = raw ? (void*) xj_strdup(spc.buffer, spc.size, ctx->alloc, ctx->error) 
                  : (void*) xj_value_string(spc.buffer, spc.size, ctx->alloc, ctx->error);
    if(p == NULL)
        xj_report(ctx->error, "No memory");
    spc_free(&spc);
    return p;
}

static xj_value *parse_number(context_t *ctx)
{
    assert(ctx->i < ctx->len && isdigit(ctx->str[ctx->i]));

    xj_i64 parsed = 0;

    while(ctx->i < ctx->len && isdigit(ctx->str[ctx->i]))
    {
        if(parsed > (INT64_MAX - ctx->str[ctx->i] + '0') / 10)
            {
                /* Overflow */
                xj_preport(ctx->error, ctx->str, ctx->i, "Integer would overflow");
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

        return xj_value_float(parsed2, ctx->alloc, ctx->error);
    }

    return xj_value_int(parsed, ctx->alloc, ctx->error);
}

static xj_value *parse_value(context_t *ctx);

static xj_value *parse_array(context_t *ctx)
{
    assert(ctx->i < ctx->len && ctx->str[ctx->i] == '[');

    ctx->i += 1; // Skip '['.

    // Skip whitespace.
    while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
        ctx->i += 1;

    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended inside an array, right after the first '['");
        return NULL;
    }

    if(ctx->str[ctx->i] == ']') /* Empty array */
    {
        ctx->i += 1; // Skip ']'.
        return xj_value_array__nocheck(NULL, 0, ctx->alloc, ctx->error);
    }

    xj_value  *head = NULL;
    xj_value **tail = &head;
    int count = 0;

    while(1)
    {
        xj_value *child = parse_value(ctx);

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
            xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c' inside of an array", ctx->str[ctx->i]);
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

    return xj_value_array__nocheck(head, count, ctx->alloc, ctx->error);
}

static xj_value *parse_object(context_t *ctx)
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
    {
        ctx->i += 1; // Skip '}'.
        return xj_value_object__nocheck(NULL, 0, ctx->alloc, ctx->error);
    }

    xj_value  *head = NULL;
    xj_value **tail = &head;
    int count = 0;

    while(1)
    {
        if(ctx->str[ctx->i] != '"')
        {
            xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c' where a string was expected");
            return NULL;
        }

        char *key = parse_string(ctx, 1);

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
            xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c' where ':' was expected");
            return NULL;
        }

        ctx->i += 1; // Skip the ':'.

        // Skip whitespace after ':'.
        while(ctx->i < ctx->len && isspace(ctx->str[ctx->i]))
            ctx->i += 1;

        xj_value *child = parse_value(ctx);

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
            xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c' inside of an object", ctx->str[ctx->i]);
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

    return xj_value_object__nocheck(head, count, ctx->alloc, ctx->error);
}

static xj_value *parse_value(context_t *ctx)
{
    if(ctx->i == ctx->len)
    {
        xj_report(ctx->error, "String ended where a value was expected");
        return NULL;
    }
    
    assert(!isspace(ctx->str[ctx->i]));

    char c = ctx->str[ctx->i];

    if(c == '"')
        return parse_string(ctx, 0);

    if(isdigit(c))
        return parse_number(ctx);

    if(c == '[')
        return parse_array(ctx);

    if(c == '{')
        return parse_object(ctx);

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
    else if(c == 't')
    {
        kword = kword_true;
        kwlen = sizeof(kword_true)-1;
    }
    else if(c == 'f')
    {
        kword = kword_false;
        kwlen = sizeof(kword_false)-1;
    }
    else
    {
        xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c'", c);
        return NULL;
    }

    if(ctx->i + kwlen <= ctx->len && !strncmp(ctx->str + ctx->i, kword, kwlen))
    {
        ctx->i += kwlen;
        switch(c)
        {
            case 'n': return xj_value_null(ctx->alloc, ctx->error);
            case 't': return xj_value_bool(1, ctx->alloc, ctx->error);
            case 'f': return xj_value_bool(0, ctx->alloc, ctx->error);
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

    xj_preport(ctx->error, ctx->str, ctx->i, "Bad character '%c'", ctx->str[ctx->i]);
    return NULL;
}

xj_value *xj_decode(const char *str, int len, 
                    xj_alloc *alloc, xj_error *error)
{
    if(str == NULL) 
        str = "";
    
    if(len < 0) 
        len = strlen(str);

    if(error != NULL)
        memset(error, 0, sizeof(xj_error));

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
    return parse_value(&ctx);
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

static xj_bool append_string(buffer_t *buff, const char *str, int len)
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

static _Bool encode_string(const char *str, int len, buffer_t *buff)
{
    if(!append_string(buff, "\"", 1))
        return 0;

    if(!append_string(buff, str, len))
        return 0;

    if(!append_string(buff, "\"", 1))
        return 0;

    return 1;
}

static _Bool encode_value(xj_value *val, buffer_t *buff)
{
    switch(val == NULL ? XJ_NULL : val->type)
    {
        case XJ_NULL: 
        return append_string(buff, "null", 4);
        
        case XJ_BOOL: 
        return val->as_bool 
            ? append_string(buff, "true", 4) 
            : append_string(buff, "false", 5);

        case XJ_INT:
        {
            char temp[32];
            int k = snprintf(temp, sizeof(temp), 
                            "%lld", val->as_int);
            assert(k >= 0 && k < (int) sizeof(temp));
            if(!append_string(buff, temp, k))
                return 0;
            return 1;
        }

        case XJ_FLOAT: 
        {
            char temp[32];
            int k = snprintf(temp, sizeof(temp), 
                            "%g", val->as_float);
            assert(k >= 0 && k < (int) sizeof(temp));
            if(!append_string(buff, temp, k))
                return 0;
            return 1;
        }

        case XJ_ARRAY:
        {
            if(!append_string(buff, "[", 1))
                return 0;

            xj_value *child = val->as_object;
            while(child != NULL)
            {
                if(!encode_value(child, buff))
                    return 0;

                child = child->next;

                if(child != NULL)
                    if(!append_string(buff, ", ", 2))
                        return 0;
            }

            if(!append_string(buff, "]", 1))
                return 0;
            return 1;
        }

        case XJ_OBJECT:
        {
            if(!append_string(buff, "{", 1))
                return 0;

            xj_value *child = val->as_object;
            while(child != NULL)
            {
                if(!encode_string(child->key, strlen(child->key), buff))
                    return 0;

                if(!append_string(buff, ": ", 2))
                    return 0;

                if(!encode_value(child, buff))
                    return 0;

                child = child->next;

                if(child != NULL)
                    if(!append_string(buff, ", ", 2))
                        return 0;
            }

            if(!append_string(buff, "}", 1))
                return 0;
            return 1;
        }

        case XJ_STRING:
        return encode_string(val->as_string, val->size, buff);
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

    _Bool ok = encode_value(value, &buff);
    
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