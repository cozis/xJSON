# xJSON
xJSON is a lightweight library that implements a JSON encoder, decoder and other utility functions.

## Usage
To use xJSON, just add `xjson.c` and `xjson.h` to your files, then include `xjson.h` where you want to use it and compile `xjson.c` with your other files.

## Overview
The main functions implemented by xJSON are
```c
xj_value *xj_decode(const char *str, int len, 
                    xj_alloc *alloc, xj_error *error);

char *xj_encode(xj_value *value, int *len);
```
which let you transform a JSON-encoded UTF-8 string to an `xj_value` and transform an `xj_value` to a string.

### Object model
The `xj_value` structure represents a generic JSON value. It's definition is made public, so that you can access it directly to read and modify it
```c
enum {
    XJ_NULL, XJ_BOOL, XJ_INT, XJ_FLOAT,
    XJ_ARRAY, XJ_OBJECT, XJ_STRING,
};

typedef struct xj_value xj_value;
struct xj_value {
    int       type;
    int       size;
    xj_value *next;
    char     *key;
    union {
        xj_i64    as_int;
        xj_bool   as_bool;
        xj_f64    as_float;
        xj_value *as_array;
        xj_value *as_object;
        char     *as_string;
    };
};
```
objects and arrays are represented as linked lists of `xj_value`s.

Although the user can make as many `xj_value` nodes as he wants, some constructor functions are also provided

```c
xj_value *xj_value_null(xj_alloc *alloc, xj_error *error);

xj_value *xj_value_bool (xj_bool val, xj_alloc *alloc, xj_error *error);
xj_value *xj_value_int  (xj_i64  val, xj_alloc *alloc, xj_error *error);
xj_value *xj_value_float(xj_f64  val, xj_alloc *alloc, xj_error *error);

xj_value *xj_value_array (xj_value *head, xj_alloc *alloc, xj_error *error);
xj_value *xj_value_object(xj_value *head, xj_alloc *alloc, xj_error *error);

xj_value *xj_value_string(const char *str, int len, xj_alloc *alloc, xj_error *error);
```

### Error handling
You may have noticed many functions require you to specify a `xj_error` object. Whenever an error occurres, the error structure is used to inform the caller of the context of the failure. This is optional, so whenever a function expects an error pointer, you can provide a `NULL`. 

The structure is public and defined as following
```c
typedef struct {
    xj_bool occurred;
    xj_bool truncated;
    int off, row, col;
    char message[128];
} xj_error;
```
when an error occurres you can read it's fields directly.

### Memory management
Many JSON libraries handle memory using reference counting. xJSON uses a different approach where all nodes are stored in a single memory pool and then freed up at the same time. Assuming most objects have the same lifetime, this makes it both faster and easier to manage many objects.

An allocator is instanciated using one of
```c
xj_alloc *xj_alloc_using(void *mem, int size, int ext, void (*free)(void*));
xj_alloc *xj_alloc_new(int size, int ext);
```
the first lets you specify the memory that the allocator will use to operate, while the second tells the allocator to call `malloc` to get the memory he needs.

When an `xj_alloc` is instanciated, you can call all of the functions that require you to provide an `xj_alloc*`. The objects that those functions return will be stored in the allocator. You deallocate all of the nodes by freeing up the whole allocator using
```c
void xj_alloc_del(xj_alloc *alloc);
```