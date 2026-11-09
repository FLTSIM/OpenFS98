/* The module system: the list of modules (0x417b30, a growable array of struct module), loading
 * them from the modules directory and the configuration's AutoInitModules, resolving each one's
 * ImportTable against the others' Linkage, and initialising and deinitialising them in the
 * order of their Linkage's level. docs/architecture.md describes the ABI. */
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

/* repz cmpsb over count bytes */
static int bytes_equal(const char *a, const char *b, uint32_t count)
{
    while (count--)
        if (*a++ != *b++)
            return 0;
    return 1;
}

/* repnz scasb: the length */
static uint32_t length_of(const char *text)
{
    uint32_t length = 0;
    while (text[length] != 0)
        ++length;
    return length;
}

/* 0x407a30: the module whose name is name (its pointer at +0 points at a match, the
 * terminator included). */
struct module *SC fs98_host_rt_module_named(const char *name)
{
    return (struct module *)LIST_FIND(MODULES, 0, &name, SERVICE(0x2b8, uint32_t, (const char *))(name) + 1, 1);
}

/* 0x407a60: the module with an id. */
struct module *SC fs98_host_rt_module_with_id(uint32_t id)
{
    return (struct module *)LIST_FIND(MODULES, 4, &id, 4, 0);
}

/* 0x407a80: the module a Linkage is. */
struct module *SC fs98_host_rt_module_with_linkage(uint32_t *linkage)
{
    return (struct module *)LIST_FIND(MODULES, 0xc, &linkage, 4, 0);
}

/* 0x408090: a module's ImportTable ({id, Linkage} pairs to a zero id) filled in, each module
 * it names acquired. */
void SC fs98_host_rt_resolve_imports(struct module *module)
{
    uint32_t *table = (uint32_t *)CALL_STD(GetProcAddress, void *, (uint32_t, const char *))(module->library, STR(0x417b7c));
    if (table == 0 || table[0] == 0)
        return;
    do {
        table[1] = (uint32_t)fs98_host_rt_acquire_id(table[0]);
        table += 2;
    } while (table[0] != 0);
}

/* 0x4080d0: the modules a module's ImportTable names released. */
void SC fs98_host_rt_release_imports(struct module *module)
{
    uint32_t *table = (uint32_t *)CALL_STD(GetProcAddress, void *, (uint32_t, const char *))(module->library, STR(0x417b7c));
    if (table == 0 || table[0] == 0)
        return;
    do {
        fs98_host_rt_release_id(table[0]);
        table += 2;
    } while (table[0] != 0);
}

/* 0x407fe0: a module acquired: the first time loaded (its Linkage found, its imports
 * acquired, its init called and cleared), after that counted. Its Linkage, or 0 for none. */
uint32_t *SC fs98_host_rt_acquire(struct module *module)
{
    linkage_call init;
    if (module == 0)
        return 0;
    if (module->users != 0) {
        ++module->users;
        return module->linkage;
    }
    module->users = 1;
    module->library = CALL_STD(LoadLibraryA, uint32_t, (const char *))(module->name);
    module->linkage = (uint32_t *)CALL_STD(GetProcAddress, void *, (uint32_t, const char *))(module->library, STR(0x417b60));
    fs98_host_rt_resolve_imports(module);
    init = (linkage_call)module->linkage[LINKAGE_INIT];
    if (init != 0) {
        init();
        module->linkage[LINKAGE_INIT] = 0;
    }
    return module->linkage;
}

/* 0x408050: a module released: the last user's deinit called, its imports released, the
 * library freed. */
void SC fs98_host_rt_release(struct module *module)
{
    linkage_call deinit;
    if (module == 0 || --module->users != 0)
        return;
    deinit = (linkage_call)module->linkage[LINKAGE_DEINIT];
    if (deinit != 0)
        deinit();
    fs98_host_rt_release_imports(module);
    CALL_STD(FreeLibrary, int32_t, (uint32_t))(module->library);
    module->library = 0;
    module->linkage = 0;
}

/* 0x407aa0, 0x407ac0, 0x407ae0, 0x407b00: acquire by name or id, release by id or Linkage. */
uint32_t *SC fs98_host_rt_acquire_named(const char *name)
{
    return fs98_host_rt_acquire(fs98_host_rt_module_named(name));
}

uint32_t *SC fs98_host_rt_acquire_id(uint32_t id)
{
    return fs98_host_rt_acquire(fs98_host_rt_module_with_id(id));
}

void SC fs98_host_rt_release_id(uint32_t id)
{
    fs98_host_rt_release(fs98_host_rt_module_with_id(id));
}

void SC fs98_host_rt_release_linkage(uint32_t *linkage)
{
    fs98_host_rt_release(fs98_host_rt_module_with_linkage(linkage));
}

/* 0x409c10: a module's registration table (12-byte records to a zero one: key, unused, value)
 * added to 0x417ee0, each as {key, id, index, value}. */
void SC fs98_host_rt_register(uint32_t id, uint32_t *records)
{
    uint32_t index = 0;
    while (records[0] != 0) {
        uint32_t *entry = (uint32_t *)LIST_APPEND(VA(0x417ee0));
        entry[1] = id;
        entry[2] = index;
        entry[0] = records[0];
        entry[3] = records[2];
        records += 3;
        ++index;
    }
}

/* 0x407b20: a module added to the list by its path, unless it is there already: loaded for
 * its Linkage's id (and its registrations), then kept loaded (keep: counted as one user) or
 * freed until it is acquired. 0 if it does not load, else 1.
 * RETAIL BUG (kept): MODULE-NO-LINKAGE -- a DLL without a Linkage export is read through null. */
int32_t SC fs98_host_rt_add_module(const char *path, int32_t keep)
{
    uint32_t library;
    struct module *module;
    uint32_t *linkage;
    if (fs98_host_rt_module_named(path) != 0)
        return 1;
    library = CALL_STD(LoadLibraryA, uint32_t, (const char *))(path);
    if (library == 0)
        return 0;
    module = (struct module *)LIST_APPEND(MODULES);
    module->name = CALL_C(_strdup, char *, (const char *))(path);
    module->library = 0;
    module->linkage = 0;
    module->users = 0;
    linkage = (uint32_t *)CALL_STD(GetProcAddress, void *, (uint32_t, const char *))(library, STR(0x417b60));
    if (linkage != 0)
        module->id = linkage[LINKAGE_ID];
    if (((volatile uint32_t *)linkage)[LINKAGE_REGISTER] != 0)
        fs98_host_rt_register(linkage[LINKAGE_ID], (uint32_t *)linkage[LINKAGE_REGISTER]);
    if (keep) {
        module->library = library;
        module->linkage = linkage;
        ++module->users;
        return 1;
    }
    CALL_STD(FreeLibrary, int32_t, (uint32_t))(library);
    return 1;
}

/* 0x407be0: the host itself as a module, "FS6API - Internal": its service table (0x417ba0)
 * is its Linkage, always loaded. */
void fs98_host_rt_add_host(void)
{
    struct module *module = (struct module *)LIST_APPEND(MODULES);
    module->name = CALL_C(_strdup, char *, (const char *))(STR(0x417b68));
    module->library = 0;
    module->id = G32(0x417ba0);
    module->linkage = (uint32_t *)VA(0x417ba0);
    module->users = 1;
}

/* 0x407c20: every file (not directory) in a directory matching a pattern added as a module
 * (0x41c480 is the path built). */
void SC fs98_host_rt_add_directory(const char *directory, const char *pattern, int32_t keep)
{
    uint8_t found[0x140];               /* WIN32_FIND_DATAA */
    uint32_t search;
    JOIN_PATH((char *)VA(0x41c480), 0, directory, pattern);
    search = CALL_STD(FindFirstFileA, uint32_t, (const char *, void *))(STR(0x41c480), found);
    if (search == 0xffffffffu)
        return;
    do {
        if (!(found[0] & 0x10)) {
            JOIN_PATH((char *)VA(0x41c480), 0, directory, (const char *)found + 0x2c);
            fs98_host_rt_add_module(STR(0x41c480), keep);
        }
    } while (CALL_STD(FindNextFileA, int32_t, (uint32_t, void *))(search, found) != 0);
    CALL_STD(FindClose, int32_t, (uint32_t))(search);
}

/* 0x407920: the modules found: the host itself; the modules directory (string 0x9050) kept
 * loaded; SIMSUI.DLL beside the game, acquired; the second directory (string 0x9052) listed
 * only. */
void fs98_host_rt_find_modules(void)
{
    char path[0x104];
    fs98_host_rt_add_host();
    LOAD_STRING(0x9050, path, 0x103);
    fs98_host_rt_add_directory(path, STR(0x417b58), 1);
    JOIN_PATH(path, 0, STR(0x41bca0), STR(0x417b4c));
    fs98_host_rt_add_module(path, 0);
    fs98_host_rt_acquire_named(path);
    LOAD_STRING(0x9052, path, 0x103);
    fs98_host_rt_add_directory(path, STR(0x417b58), 0);
}

/* 0x407f20: the configuration's [AutoInitModules] read (its buffer grown by 0x400 until it
 * fits) and each "MODULE=" line's module added, kept loaded. */
void fs98_host_rt_auto_init_modules(void)
{
    char *section;
    uint32_t size = 0, length;
    for (;;) {
        size += 0x400;
        ALLOCATE(1, size, &section, 0);
        length = CALL_STD(GetPrivateProfileSectionA, uint32_t, (const char *, char *, uint32_t, const char *))(
            STR(0x417b90), section, size, STR(0x41c120));
        if (length != size - 2)
            break;
        FREE(&section);
    }
    if (length != 0 && section[0] != 0) {
        char *line = section;
        do {
            CALL_C(_strupr, char *, (char *))(line);
            if (bytes_equal(line, STR(0x417b88), 7)) {
                line += 7;
                fs98_host_rt_add_module(line, 1);
            }
            line += length_of(line) + 1;
        } while (*line != 0);
    }
    FREE(&section);
}

/* 0x407da0: a walk's step: a loaded module whose Linkage has a level placed in the order,
 * before the first with a lower one. 0, walk on. */
int32_t SC fs98_host_rt_order_module(struct module *module, uint32_t **order)
{
    uint32_t at = 0, count;
    if (module->linkage == 0 || module->linkage[LINKAGE_LEVEL] == 0)
        return 0;
    count = ++ORDERED;
    if (order[0] != 0) {
        do {
            if (module->linkage[LINKAGE_LEVEL] > order[at][LINKAGE_LEVEL]) {
                /* the size is ((at + 1) << 30) - (at + 1) + count, times 4, in 32 bits: (count - at - 1) * 4 */
                CALL_C(memmove, void *, (void *, const void *, uint32_t))(&order[at + 1], &order[at],
                    ((((at + 1) << 30) - (at + 1)) + count) << 2);
                order[at] = module->linkage;
                return 0;
            }
            ++at;
        } while (order[at] != 0);
    }
    order[at] = module->linkage;
    return 0;
}

/* 0x407d60: the order built: an array (zeroed) of as many Linkages as there are modules, and
 * the modules with a level placed in it. */
void fs98_host_rt_order_modules(void)
{
    ALLOCATE(1, G32(0x417b30) * 4, VA(0x41bb28), 1);
    ORDERED = 0;
    LIST_WALK(MODULES, ORDER, (walk_fn)fs98_host_rt_order_module);
}

/* 0x407cc0: the ordered modules initialised, highest level first: each init called once. */
void fs98_host_rt_init_ordered(void)
{
    uint32_t at;
    fs98_host_rt_order_modules();
    for (at = 0; (int32_t)at < (int32_t)ORDERED; ++at) {
        struct module *module = fs98_host_rt_module_with_linkage(((uint32_t **)ORDER)[at]);
        if (module->linkage != 0 && module->linkage[LINKAGE_INIT] != 0) {
            ((linkage_call)module->linkage[LINKAGE_INIT])();
            module->linkage[LINKAGE_INIT] = 0;
        }
    }
}

/* 0x407d10: the ordered modules deinitialised, lowest level first, and the order freed. */
void fs98_host_rt_deinit_ordered(void)
{
    uint32_t at = ORDERED;
    for (; at != 0; --at) {
        struct module *module = fs98_host_rt_module_with_linkage(((uint32_t **)ORDER)[at - 1]);
        if (module->linkage != 0 && module->linkage[LINKAGE_DEINIT] != 0) {
            ((linkage_call)module->linkage[LINKAGE_DEINIT])();
            module->linkage[LINKAGE_DEINIT] = 0;
        }
    }
    FREE(VA(0x41bb28));
}

/* 0x407e40: a walk's step: a loaded module's ImportTable filled in with the Linkages of the
 * modules it names (those that are not on the list are left). 0. */
int32_t SC fs98_host_rt_link_imports(struct module *module, void *unused)
{
    uint32_t *table;
    (void)unused;
    if (module->library == 0)
        return 0;
    table = (uint32_t *)CALL_STD(GetProcAddress, void *, (uint32_t, const char *))(module->library, STR(0x417b7c));
    if (table == 0 || table[0] == 0)
        return 0;
    do {
        struct module *named = fs98_host_rt_module_with_id(table[0]);
        if (named != 0)
            table[1] = (uint32_t)named->linkage;
        table += 2;
    } while (table[0] != 0);
    return 0;
}

/* 0x407ec0: a walk's step: a loaded module's init called, if not yet. 0. */
int32_t SC fs98_host_rt_init_module(struct module *module, void *unused)
{
    (void)unused;
    if (module->library != 0 && module->linkage[LINKAGE_INIT] != 0) {
        ((linkage_call)module->linkage[LINKAGE_INIT])();
        module->linkage[LINKAGE_INIT] = 0;
    }
    return 0;
}

/* 0x407ef0: a walk's step: a loaded module without a level deinitialised, if not yet. 0. */
int32_t SC fs98_host_rt_deinit_unordered(struct module *module, void *unused)
{
    (void)unused;
    if (module->library != 0 && module->linkage[LINKAGE_LEVEL] == 0 && module->linkage[LINKAGE_DEINIT] != 0) {
        ((linkage_call)module->linkage[LINKAGE_DEINIT])();
        module->linkage[LINKAGE_DEINIT] = 0;
    }
    return 0;
}

/* 0x407e90: a walk's step: a loaded module's deinit called (if not yet) and its library
 * freed; its name freed. 0. */
int32_t SC fs98_host_rt_unload_module(struct module *module, void *unused)
{
    (void)unused;
    if (module->library != 0) {
        if (module->linkage[LINKAGE_DEINIT] != 0)
            ((linkage_call)module->linkage[LINKAGE_DEINIT])();
        CALL_STD(FreeLibrary, int32_t, (uint32_t))(module->library);
    }
    FREE(&module->name);
    return 0;
}

/* 0x408110: the modules the host itself imports (its table at 0x418800, {id, Linkage} pairs
 * to a zero id) acquired. */
void fs98_host_rt_408110(void)
{
    uint32_t *table = (uint32_t *)VA(0x418800);
    if (table[0] == 0)
        return;
    do {
        table[1] = (uint32_t)fs98_host_rt_acquire_id(table[0]);
        table += 2;
    } while (table[0] != 0);
}

/* 0x4079b0: the modules started: the host's own imports, AutoInitModules, every ImportTable linked, the
 * ordered modules initialised, then the rest. */
void fs98_host_rt_start_modules(void)
{
    fs98_host_rt_408110();
    fs98_host_rt_auto_init_modules();
    LIST_WALK(MODULES, 0, (walk_fn)fs98_host_rt_link_imports);
    fs98_host_rt_init_ordered();
    LIST_WALK(MODULES, 0, (walk_fn)fs98_host_rt_init_module);
}

/* 0x4079f0: and stopped: those without a level deinitialised, then the ordered ones, then
 * every module unloaded and the list freed. */
void fs98_host_rt_stop_modules(void)
{
    LIST_WALK(MODULES, 0, (walk_fn)fs98_host_rt_deinit_unordered);
    fs98_host_rt_deinit_ordered();
    LIST_WALK(MODULES, 0, (walk_fn)fs98_host_rt_unload_module);
    LIST_FREE(MODULES);
}

/* 0x407910: the list at 0x417ae8 freed. */
uint32_t fs98_host_rt_407910(void)
{
    return LIST_FREE(VA(0x417ae8));
}

/* 0x409c80: the lists at 0x417f00 and 0x417ee0 (the registrations) freed. */
uint32_t fs98_host_rt_409c80(void)
{
    LIST_FREE(VA(0x417f00));
    return LIST_FREE(VA(0x417ee0));
}
