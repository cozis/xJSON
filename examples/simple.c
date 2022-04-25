#include <string.h>
#include <stdio.h>
#include <xjson.h>

int main()
{
    // Instanciate the allocator.
    xj_alloc *alloc = xj_alloc_new(65536, 4096);

    if(alloc == NULL)
        // Handle failure maybe?
        return 1;

    char *str = "{\"name\": \"Francesco\", \"age\": 23}";

    // Do the actual parsing..
    xj_value *val = xj_decode(str, -1, alloc, NULL);

    // ..error?
    if(val == NULL)
        fprintf(stderr, "Failed to parse!\n");
    
    char *name;
    int   age;

    // Now iterate over the fields to get the name
    // and age.
    xj_value *child = val->as_object;
    while(child != NULL)
    {
        if(!strcmp("name", child->key))
            name = child->as_string;
        else
            age = child->as_int;

        child = child->next;
    }

    printf("name: %s, age: %d\n", name, age);

    // Now free everything!
    xj_alloc_del(alloc);
    return 0;
}