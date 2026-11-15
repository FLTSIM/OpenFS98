/* The module system's list (0x417b30, a growable array of struct module): finding a module by
 * name, id or Linkage, adding one by its path, acquiring and releasing it (loading it, its
 * ImportTable resolved against the others' Linkage) and registering its records. The
 * startup and shutdown around it are host_rt_module_load.c; docs/architecture.md describes
 * the ABI.
 *
 * Shared with the services: src/host/host.c calls these too, and its standalone builds compile
 * this file with HOST_RT_STANDALONE (host_rt.h), so there is one copy of each. */
#include "host_rt_module.h"

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

/* 0x407ac0 and 0x407ae0: acquire by id, release by id. (0x407aa0, by name, is in
 * host_rt_module_load.c; 0x407b00, release by Linkage, is reached only from a service.) */
uint32_t *SC fs98_host_rt_acquire_id(uint32_t id)
{
    return fs98_host_rt_acquire(fs98_host_rt_module_with_id(id));
}

void SC fs98_host_rt_release_id(uint32_t id)
{
    fs98_host_rt_release(fs98_host_rt_module_with_id(id));
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
