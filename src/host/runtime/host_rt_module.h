/* The module list's element, a Linkage as the host reads it, and the services the module
 * system calls (host_rt_modules.c, host_rt_module_load.c). */
#ifndef FS98_HOST_RT_MODULE_H
#define FS98_HOST_RT_MODULE_H

#include "host_rt.h"

#define MODULES ((void *)VA(0x417b30))
#define ORDER G_PTR(0x41bb28)                   /* the Linkages with a level, highest first */
#define ORDERED G32(0x41889c)                   /* how many */

/* an element of the module list (0x14 bytes) */
struct module {
    char *name;             /* the DLL's path, _strdup'd */
    uint32_t id;            /* its Linkage's first word */
    uint32_t library;       /* the HMODULE while loaded */
    uint32_t *linkage;      /* its Linkage while loaded */
    uint32_t users;         /* how many have it loaded */
};

/* a module's Linkage, as far as the host reads it */
#define LINKAGE_ID 0
#define LINKAGE_INIT 1          /* called once, then cleared */
#define LINKAGE_DEINIT 2        /* likewise */
#define LINKAGE_LEVEL 4         /* initialised in order of it, highest first; 0 is not ordered */
#define LINKAGE_REGISTER 5      /* a table of records registered under the module's id */

typedef void (*linkage_call)(void);
typedef int32_t (SC *walk_fn)(void *element, void *context);

#define LIST_APPEND(list) SERVICE(0x2f8, void *, (void *))(list)
#define LIST_FREE(list) SERVICE(0x2fc, uint32_t, (void *))(list)
#define LIST_WALK(list, context, fn) SERVICE(0x308, int32_t, (void *, void *, walk_fn))(list, context, fn)
#define LIST_FIND(list, offset, key, length, indirect) \
    SERVICE(0x310, void *, (void *, uint32_t, const void *, uint32_t, uint32_t))(list, offset, key, length, indirect)
#define ALLOCATE(fatal, size, out, zeroed) SERVICE(0x20c, void *, (uint32_t, uint32_t, void *, uint32_t))(fatal, size, out, zeroed)
#define FREE(pointer_at) SERVICE(0x214, uint32_t, (void *))(pointer_at)
#define JOIN_PATH(out, drive, directory, name) \
    SERVICE(0x184, uint32_t, (char *, const char *, const char *, const char *))(out, drive, directory, name)
#define LOAD_STRING(id, out, size) SERVICE(0x2a0, uint32_t, (uint32_t, char *, uint32_t))(id, out, size)

#endif
