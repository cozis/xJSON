typedef double    xj_f64;
typedef long long xj_i64;
typedef _Bool     xj_bool;

_Static_assert(sizeof(xj_f64) == 8);
_Static_assert(sizeof(xj_i64) == 8);

enum {
    XJ_NULL,
    XJ_BOOL,
    XJ_INT,
    XJ_FLOAT,
    XJ_ARRAY,
    XJ_OBJECT,
    XJ_STRING,
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

typedef struct {
    xj_bool occurred;
    xj_bool truncated;
    char message[128];
} xj_error;

typedef struct xj_alloc xj_alloc;
xj_alloc *xj_newAlloc(int size, int ext);
xj_alloc *xj_newAllocUsing(void *mem, int size, int ext, void (*free)(void*));
void      xj_freeAlloc(xj_alloc *alloc);
void     *xj_bpMalloc(xj_alloc *alloc, unsigned int size);
xj_value *xj_newNull(xj_alloc *alloc);
xj_value *xj_newBool(xj_bool val, xj_alloc *alloc);
xj_value *xj_newInt(xj_i64 val, xj_alloc *alloc);
xj_value *xj_newFloat(xj_f64 val, xj_alloc *alloc);
xj_value *xj_newArray(xj_value *head, xj_alloc *alloc);
xj_value *xj_newArray__nocheck(xj_value *head, int count, xj_alloc *alloc);
xj_value *xj_newObject(xj_value *head, xj_alloc *alloc);
xj_value *xj_newObject__nocheck(xj_value *head, int count, xj_alloc *alloc);
xj_value *xj_newString(const char *str, int len, xj_alloc *alloc);
char     *xj_strdup(const char *str, int len, xj_alloc *alloc);
xj_value *xj_decode(const char *str, int len, xj_alloc *alloc, xj_error *error);
char     *xj_encode(xj_value *value, int *len);