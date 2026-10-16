/* Differential test for FACILITIES.DLL's non-UI facility index.
 *
 * FACIL is circularly coupled to FE: FE calls FACIL +0x2c after loading the
 * scenery library, while FACIL reads the facility directories through FE
 * +0x7c..+0x88.  Each side therefore gets its own retail FE instance.  The
 * shared fediff host supplies scenery I/O; this file adds the profile, cache
 * creation and qsort services FACIL needs.
 *
 * The first phase deliberately supports a retail-vs-retail oracle run as well
 * as retail-vs-OpenFS98.  It rebuilds INDEX_FILE from scratch, calls +0x2c a
 * second time to exercise cache validation, and compares the complete host
 * transcript, cache bytes, GLOBAL, deterministic arena, FE state and FACIL
 * state.  The three Linkage dialog slots remain outside this harness and go to
 * ofstest.
 */
#define main fediff_reference_main
#include "fediff.c"
#undef main

#define FACIL_IMAGE_SIZE 0x2d1cu
#define FACIL_LINKAGE    0x0040u
#define FACIL_CACHE_MAX  (16u << 20)

struct facil_linkage {
    uint32_t id;
    uint32_t (STDCALL *init)(void);
    uint32_t (STDCALL *deinit)(void);
    uint32_t reserved0c;
    uint32_t reserved10;
    void *dialogs;
    fe_fn4 search;
    fe_fn3 names;
    fe_fn3 record;
    fe_fn1 airport_dialog;
    fe_fn0 directory_dialog;
    fe_fn0 rebuild;
    fe_fn0 scenery_dialog;
};

struct facil_side {
    const char *name;
    HMODULE module;
    struct facil_linkage *link;
    uint8_t *image;
    uint8_t *module_lo;
    uint8_t *module_hi;
    uint8_t pristine[FACIL_IMAGE_SIZE];
    struct fe_side *fe;
};

static struct facil_side facil_retail;
static struct facil_side facil_ours;
static struct facil_side *facil_current;
static uint8_t main_stub[0xd0];
static char cache_root[MAX_PATH * 2];
static int facil_index_active;
static uint32_t fake_ticks;
static int32_t (STDCALL *sort_compare)(const void *, const void *);
static char facil_fixture_root[MAX_PATH * 2];

static int32_t STDCALL facil_util_alert(void **handle, uint32_t mode,
                                       uint32_t string_id, uint32_t arg3,
                                       uint32_t arg4, uint32_t arg5,
                                       uint32_t arg6, uint32_t arg7)
{
    (void)mode;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    *handle = arena + ARENA_SIZE - 16;
    fe_put(" alert(%#x)", string_id);
    return 0;
}

static int32_t STDCALL facil_util_alert_close(void **handle)
{
    fe_put(" alert.close");
    *handle = NULL;
    return 0;
}

#define FACIL_REL_DATA(offset, target) (uint16_t)(offset),
#define FACIL_REL_TEXT(offset, rva)    (uint16_t)(offset),
#define FACIL_RELOCS
static const uint16_t facil_relocations[] = {
#include "../../src/facil/facil_data.inc"
};
#undef FACIL_RELOCS
#undef FACIL_REL_TEXT
#undef FACIL_REL_DATA

static int facil_resolve_path(char *out, uint32_t size, const char *path,
                              int32_t extension)
{
    const char *suffix;

    if (path == NULL)
        return 0;
    if (_strcmpi(path, "INDEX_FILE") == 0) {
        if (extension != 0 && extension != -2)
            return 0;
        if ((uint32_t)snprintf(out, size, "%s\\INDEX_FILE", cache_root) >= size)
            return 0;
        return 1;
    }
    if (_strcmpi(path, "FACIL-TEMP") == 0) {
        if (extension != 0 && extension != -2)
            return 0;
        if ((uint32_t)snprintf(out, size, "%s\\FACIL-TEMP", cache_root) >= size)
            return 0;
        return 1;
    }
    suffix = fe_extension_text(extension);
    if (suffix == NULL ||
        (uint32_t)snprintf(out, size, "%s%s", path, suffix) >= size)
        return 0;
    return 1;
}

static uint32_t STDCALL facil_host_ticks(void)
{
    fake_ticks += 1000;
    return fake_ticks;
}

static int32_t STDCALL facil_host_cfg_int(const char *section,
                                          const char *key, int32_t *value)
{
    fe_put(" cfgint(");
    fe_put_string(section);
    fe_put(",");
    fe_put_string(key);
    fe_put(")=%d", value != NULL ? *value : 0);
    return 0;
}

static int32_t STDCALL facil_host_cfg_put_int(const char *section,
                                              const char *key, int32_t value)
{
    fe_put(" putint(");
    fe_put_string(section);
    fe_put(",");
    fe_put_string(key);
    fe_put(",%d)", value);
    return 0;
}

static int32_t STDCALL facil_host_cfg_string(const char *section,
                                             const char *key, char *value,
                                             uint32_t size)
{
    fe_put(" cfgstr(");
    fe_put_string(section);
    fe_put(",");
    fe_put_string(key);
    fe_put(")=");
    fe_put_string(value);
    if (value != NULL && size != 0)
        value[size - 1] = 0;
    return value != NULL ? (int32_t)strlen(value) : 0;
}

static int32_t STDCALL facil_host_cfg_put_string(const char *section,
                                                 const char *key,
                                                 const char *value)
{
    fe_put(" putstr(");
    fe_put_string(section);
    fe_put(",");
    fe_put_string(key);
    fe_put(",");
    fe_put_string(value);
    fe_put(")");
    return 0;
}

static int32_t STDCALL facil_host_basedir(char *out)
{
    strcpy(out, facil_index_active ? cache_root : fixture);
    fe_put(" basedir=");
    fe_put_string(out);
    return 0;
}

static int32_t STDCALL facil_host_loadstring(uint32_t id, char *out,
                                             int32_t size)
{
    const char *text;

    if (id != 0x9101)
        return host_loadstring(id, out, size);
    text = "INDEX_FILE";
    if (size > 0) {
        strncpy(out, text, (size_t)size - 1);
        out[size - 1] = 0;
    }
    fe_put(" string(%#x)='INDEX_FILE'", id);
    return 10;
}

static int32_t STDCALL facil_host_loadstring_alloc(uint32_t id, char **out)
{
    char value[0x104];
    uint32_t length;

    value[0] = 0;
    facil_host_loadstring(id, value, sizeof(value));
    length = (uint32_t)strlen(value) + 1;
    if (host_alloc(1, length, (void **)out, 0) != 0)
        return -1;
    strcpy(*out, value);
    return 0;
}

static int32_t STDCALL facil_host_exists(const char *path, int32_t extension)
{
    char full[MAX_PATH * 2];
    DWORD attributes;

    if (!facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    attributes = GetFileAttributesA(full);
    fe_put(" exists(");
    fe_put_string(full);
    fe_put(")=%d", attributes == INVALID_FILE_ATTRIBUTES ? -1 : 0);
    return attributes == INVALID_FILE_ATTRIBUTES ? -1 : 0;
}

static int32_t facil_host_open_common(struct host_file **out,
                                      const char *path, int32_t extension,
                                      int32_t mode, int fatal)
{
    char full[MAX_PATH * 2];

    if (!facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    return fe_open_file(out, full, -2, mode, fatal);
}

static int32_t STDCALL facil_host_open(struct host_file **out,
                                       const char *path, int32_t extension,
                                       int32_t mode)
{
    return facil_host_open_common(out, path, extension, mode, 0);
}

static int32_t STDCALL facil_host_open_fatal(struct host_file **out,
                                             const char *path,
                                             int32_t extension, int32_t mode)
{
    return facil_host_open_common(out, path, extension, mode, 1);
}

static int32_t STDCALL facil_host_get_attributes(int32_t *out,
                                                 const char *path,
                                                 int32_t extension)
{
    char full[MAX_PATH * 2];
    DWORD attributes;

    if (out == NULL ||
        !facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    attributes = GetFileAttributesA(full);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return -1;
    *out = (int32_t)attributes;
    fe_put(" getattr=%#x", *out);
    return 0;
}

static int32_t STDCALL facil_host_set_attributes(int32_t attributes,
                                                 const char *path,
                                                 int32_t extension)
{
    char full[MAX_PATH * 2];

    if (!facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    /* FACIL passes 1 while preparing an old index for replacement.  In the
     * host's flag vocabulary that means writable, not FILE_ATTRIBUTE_READONLY. */
    if (attributes == 1)
        attributes = FILE_ATTRIBUTE_NORMAL;
    fe_put(" setattr(%#x)", attributes);
    return SetFileAttributesA(full, (DWORD)attributes) ? 0 : -1;
}

static int32_t STDCALL facil_host_create(struct host_file **out,
                                         const char *path,
                                         int32_t extension)
{
    struct host_file *file;
    char full[MAX_PATH * 2];

    if (out == NULL ||
        !facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    *out = NULL;
    file = fe_new_host_file();
    if (file == NULL)
        return -1;
    file->file = CreateFileA(full, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    fe_put(" create(");
    fe_put_string(full);
    if (file->file == INVALID_HANDLE_VALUE) {
        fe_put(")=-1");
        return -1;
    }
    file->open = 1;
    strncpy(file->path, full, sizeof(file->path) - 1);
    file->path[sizeof(file->path) - 1] = 0;
    *out = file;
    fe_put(")=file%u", (uint32_t)(file - host_files));
    return 0;
}

static int32_t STDCALL facil_host_delete(const char *path,
                                         int32_t extension)
{
    char full[MAX_PATH * 2];

    if (!facil_resolve_path(full, sizeof(full), path, extension))
        return -1;
    fe_put(" delete(");
    fe_put_string(full);
    fe_put(")");
    return DeleteFileA(full) ? 0 : -1;
}

/* The host service takes the destination pair first and source pair second. */
static int32_t STDCALL facil_host_move(const char *destination,
                                       int32_t destination_extension,
                                       const char *source,
                                       int32_t source_extension)
{
    char destination_path[MAX_PATH * 2];
    char source_path[MAX_PATH * 2];

    if (!facil_resolve_path(destination_path, sizeof(destination_path),
                            destination, destination_extension) ||
        !facil_resolve_path(source_path, sizeof(source_path), source,
                            source_extension))
        return -1;
    fe_put(" move(");
    fe_put_string(source_path);
    fe_put(",");
    fe_put_string(destination_path);
    fe_put(")");
    return MoveFileExA(source_path, destination_path,
                       MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
}

static int32_t STDCALL facil_host_temp(char *out, int32_t extension)
{
    (void)extension;
    snprintf(out, MAX_PATH, "%s\\FACIL-TEMP", cache_root);
    DeleteFileA(out);
    fe_put(" temp=");
    fe_put_string(out);
    return 0;
}

static int32_t STDCALL facil_host_write(struct host_file *file,
                                        const void *buffer, uint32_t size,
                                        uint32_t *written)
{
    DWORD count = 0;
    BOOL ok;

    if (written != NULL)
        *written = 0;
    if (file == NULL || !file->open)
        return -1;
    ok = WriteFile(file->file, buffer, size, &count, NULL);
    if (written != NULL)
        *written = count;
    fe_put(" write(%#x)=%lu", size, (unsigned long)count);
    return ok ? 0 : -1;
}

static char *STDCALL facil_host_strdup(const char *value)
{
    char *copy;
    uint32_t length;

    if (value == NULL)
        return NULL;
    length = (uint32_t)strlen(value) + 1;
    if (host_alloc(1, length, (void **)&copy, 0) != 0)
        return NULL;
    strcpy(copy, value);
    return copy;
}

static void facil_sort_swap(uint8_t *left, uint8_t *right, uint32_t width)
{
    uint32_t index;

    for (index = 0; index < width; index++) {
        uint8_t value = left[index];
        left[index] = right[index];
        right[index] = value;
    }
}

static void STDCALL facil_host_qsort(void *base, uint32_t count,
                                     uint32_t width,
                                     int32_t (STDCALL *compare)(const void *,
                                                                const void *))
{
    uint8_t *bytes = (uint8_t *)base;
    uint32_t index;

    sort_compare = compare;
    /* Insertion sort is sufficient for the CD fixture and, unlike the CRT's
     * cdecl qsort, calls retail's stdcall comparator with the right ABI. */
    for (index = 1; index < count; index++) {
        uint32_t cursor = index;
        while (cursor != 0 &&
               sort_compare(bytes + cursor * width,
                            bytes + (cursor - 1) * width) < 0) {
            facil_sort_swap(bytes + cursor * width,
                            bytes + (cursor - 1) * width, width);
            cursor--;
        }
    }
}

static void facil_build_stubs(void)
{
    fe_build_stubs();
    memset(main_stub, 0, sizeof(main_stub));
    *(uint32_t *)main_stub = 3;
    SET_SLOT(host, 0x038, facil_host_ticks);
    SET_SLOT(host, 0x0a8, facil_host_cfg_int);
    SET_SLOT(host, 0x0ac, facil_host_cfg_put_int);
    SET_SLOT(host, 0x0c8, facil_host_cfg_string);
    SET_SLOT(host, 0x0cc, facil_host_cfg_put_string);
    SET_SLOT(host, 0x130, facil_host_basedir);
    SET_SLOT(host, 0x134, facil_host_get_attributes);
    SET_SLOT(host, 0x138, facil_host_set_attributes);
    SET_SLOT(host, 0x148, facil_host_create);
    SET_SLOT(host, 0x14c, facil_host_delete);
    SET_SLOT(host, 0x158, facil_host_exists);
    SET_SLOT(host, 0x16c, facil_host_move);
    SET_SLOT(host, 0x174, facil_host_temp);
    SET_SLOT(host, 0x17c, facil_host_open);
    SET_SLOT(host, 0x180, facil_host_open_fatal);
    SET_SLOT(host, 0x1b0, facil_host_write);
    SET_SLOT(host, 0x22c, facil_host_qsort);
    SET_SLOT(host, 0x2a0, facil_host_loadstring);
    SET_SLOT(host, 0x2a4, facil_host_loadstring_alloc);
    SET_SLOT(host, 0x2b4, facil_host_strdup);
}

static void facil_bind_imports(struct facil_side *side, uint8_t *util)
{
    struct fs98_import *import = (struct fs98_import *)
        GetProcAddress(side->module, "ImportTable");

    for (; import->id != 0; import++) {
        switch (import->id) {
        case 1:  import->link = host; break;
        case 2:  import->link = global; break;
        case 3:  import->link = main_stub; break;
        case 4:  import->link = chain; break;
        case 17: import->link = numeric; break;
        case 18: import->link = side->fe->link; break;
        case 20: import->link = util; break;
        default:
            printf("FACIL: unexpected import %u\n", import->id);
            exit(2);
        }
    }
}

static int facil_load_side(struct facil_side *side, const char *path,
                           const char *name, struct fe_side *fe_side,
                           uint8_t *util)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    struct fs98_import *imports;

    memset(side, 0, sizeof(*side));
    side->name = name;
    side->fe = fe_side;
    side->module = LoadLibraryA(path);
    if (side->module == NULL) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        return 0;
    }
    side->link = (struct facil_linkage *)
        GetProcAddress(side->module, "Linkage");
    imports = (struct fs98_import *)
        GetProcAddress(side->module, "ImportTable");
    if (side->link == NULL || imports == NULL || side->link->id != 37) {
        printf("%s: not FACIL\n", path);
        return 0;
    }
    side->image = (uint8_t *)side->link - FACIL_LINKAGE;
    if ((uint8_t *)imports != side->image) {
        printf("%s: ImportTable is not Linkage-0x40\n", path);
        return 0;
    }
    dos = (IMAGE_DOS_HEADER *)side->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)side->module + dos->e_lfanew);
    side->module_lo = (uint8_t *)side->module;
    side->module_hi = side->module_lo + nt->OptionalHeader.SizeOfImage;
    facil_bind_imports(side, util);
    memcpy(side->pristine, side->image, FACIL_IMAGE_SIZE);
    return 1;
}

static int facil_read_cache(uint8_t *out, uint32_t *size)
{
    char path[MAX_PATH * 2];
    HANDLE file;
    DWORD file_size;
    DWORD got;

    snprintf(path, sizeof(path), "%s\\INDEX_FILE", cache_root);
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    file_size = GetFileSize(file, NULL);
    if (file_size == INVALID_FILE_SIZE || file_size > FACIL_CACHE_MAX) {
        CloseHandle(file);
        return 0;
    }
    got = 0;
    if (!ReadFile(file, out, file_size, &got, NULL) || got != file_size) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    *size = file_size;
    return 1;
}

static void facil_delete_cache(void)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\INDEX_FILE", cache_root);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%s\\FACIL-TEMP", cache_root);
    DeleteFileA(path);
}

static int facil_make_fixture(void)
{
    static const char *names[] = {
        "REGION_ALPHA", "COUNTRY_ALPHA", "STATE_ALPHA", "CITY_ALPHA",
        "AIRPORT_ALPHA"
    };
    static const uint32_t kinds[] = {
        0x10001, 0x20001, 0x30001, 0x40001, 0x50001
    };
    uint8_t bgl[0x400];
    uint8_t *record;
    char directory[MAX_PATH * 2];
    char path[MAX_PATH * 2];
    uint32_t index;

    snprintf(facil_fixture_root, sizeof(facil_fixture_root),
             "%s\\Facility", cache_root);
    CreateDirectoryA(facil_fixture_root, NULL);
    snprintf(directory, sizeof(directory), "%s\\SCENERY",
             facil_fixture_root);
    CreateDirectoryA(directory, NULL);
    memset(bgl, 0, sizeof(bgl));
    *(uint32_t *)(bgl + 0x100) = 0x30001;
    *(uint32_t *)(bgl + 0x104) = 0xe0;
    *(uint16_t *)(bgl + 0x108) = 0x409;
    *(uint32_t *)(bgl + 0x114) = 5;
    record = bgl + 0x118;
    for (index = 0; index < 5; index++) {
        *(uint32_t *)(record + 4) = 0x20;
        *(uint32_t *)(record + 8) = kinds[index];
        *(uint32_t *)(record + 0xc) = index + 1;
        strcpy((char *)record + 0x10, names[index]);
        record += 0x28;
    }
    /* An airport directory entry carries its source record offset, not a
     * hierarchy id.  A minimal type-1 record with no 0x10006 subsections
     * exercises FACIL's fallback airport-name path. */
    *(uint32_t *)(bgl + 0x118 + 4 * 0x28 + 0xc) = 0x300;
    *(uint32_t *)(bgl + 0x300) = 1;
    snprintf(path, sizeof(path), "%s\\facildiff.BGL", directory);
    return fe_write_fixture_file(path, bgl, sizeof(bgl));
}

static void facil_remove_fixture(void)
{
    char path[MAX_PATH * 2];
    char directory[MAX_PATH * 2];

    snprintf(directory, sizeof(directory), "%s\\SCENERY",
             facil_fixture_root);
    snprintf(path, sizeof(path), "%s\\facildiff.BGL", directory);
    DeleteFileA(path);
    RemoveDirectoryA(directory);
    RemoveDirectoryA(facil_fixture_root);
}

static void facil_install_fe_fixture(struct fe_side *side)
{
    static const uint8_t id[16] = {
        'F', 'A', 'C', 'I', 'L', '-', 'D', 'I', 'F', 'F', 0, 0, 0, 0, 0, 1
    };
    uint8_t *area = (uint8_t *)fe_arena_get(0x10);
    uint8_t *record = (uint8_t *)fe_arena_get(0x1ae);
    uint32_t path_size = (uint32_t)strlen(facil_fixture_root) + 1;
    char *path = (char *)fe_arena_get(path_size);

    if (area == NULL || record == NULL || path == NULL)
        ExitProcess(2);
    memset(area, 0, 0x10);
    memset(record, 0, 0x1ae);
    memcpy(path, facil_fixture_root, path_size);
    strcpy((char *)record, "facildiff");
    memcpy(record + 0x174, id, sizeof(id));
    *(uint32_t *)(record + 0x190) = 0x100;
    *(uint32_t *)(area + 0) = fe_ptr(record);
    *(uint32_t *)(area + 4) = 0x1ae;
    *(uint32_t *)(area + 8) = 1;
    *(uint32_t *)(area + 0xc) = fe_ptr(path);
    *(uint32_t *)(side->image + 0x2d1) = fe_ptr(area);
    *(uint32_t *)(side->image + 0x2d5) = 0x10;
    *(uint32_t *)(side->image + 0x2d9) = 1;
    *(uint32_t *)(side->image + 0x5ca0) = 1;
}

static void facil_reset_fe_binding(struct fe_side *fe_side)
{
    struct fs98_import *import = (struct fs98_import *)
        GetProcAddress(fe_side->module, "ImportTable");

    for (; import->id != 0; import++)
        if (import->id == 37)
            import->link = facil;
}

static void facil_run_side(struct facil_side *side, uint8_t *facil_image,
                           uint8_t *fe_image, uint8_t *global_image,
                           uint8_t *arena_image, uint32_t *arena_size,
                           uint8_t *cache, uint32_t *cache_size,
                           uint32_t *first_result, uint32_t *second_result)
{
    struct fe_scenario scenario;
    uint8_t position[0x18];

    memset(&scenario, 0, sizeof(scenario));
    scenario.flags = 0;
    scenario.latitude = 4652043u;
    scenario.longitude = 0xc1b33f34u;
    scenario.altitude = 1000;
    scenario.radius = 750000;
    scenario.level = 12;
    current = side->fe;
    facil_current = side;
    current_scenario = 0;
    current_operation = FE_OP_FACIL;
    transcript_len = 0;
    transcript[0] = 0;
    arena_used = 0;
    fake_ticks = 0;
    memset(arena, 0xa5, sizeof(arena));
    memset(output, 0x5a, sizeof(output));
    fe_reset_handles();
    frame = NULL;
    memcpy(side->fe->image, side->fe->pristine, FE_IMAGE_SIZE);
    memcpy(side->image, side->pristine, FACIL_IMAGE_SIZE);
    fe_put_position(position, &scenario);
    fe_make_global(&scenario, position);
    facil_delete_cache();

    facil_reset_fe_binding(side->fe);
    fe_put("FACIL index");
    side->link->init();
    side->fe->link->init();
    /* FE loads world.vis lazily.  Start one caller-shaped listing while its
     * FACIL import still points at the logging stub; otherwise +0x7c quite
     * correctly reports an empty registry. */
    ((fe_fn0)fe_slot(side->fe, 0x4c))();
    facil_install_fe_fixture(side->fe);
    facil_index_active = 1;
    *first_result = side->link->rebuild();
    *second_result = side->link->rebuild();
    facil_index_active = 0;
    fe_put(" results=%#x,%#x", *first_result, *second_result);
    if (!facil_read_cache(cache, cache_size)) {
        printf("%s did not produce INDEX_FILE\n", side->name);
        printf("%.5000s\n",
               transcript + (transcript_len > 5000 ? transcript_len - 5000 : 0));
        *cache_size = 0;
    } else {
        uint8_t *record = NULL;
        uint32_t record_size = 0;
        uint32_t record_result;

        /* +0x20 consumes an offset returned by the search/list services.  Pin
         * the known synthetic airport record directly so this first harness
         * slice tests the copy contract without coupling it to the unfinished
         * UI query machinery. */
        *(uint32_t *)(side->image + 0x2b58) = fe_ptr(cache);
        record_result = side->link->record(0x11e, fe_ptr(&record),
                                           fe_ptr(&record_size));
        fe_put(" RECORD=%#x,size=%#x,data=", record_result, record_size);
        fe_put_blob(record, record_size);
        host_free((void **)&record);
        *(uint32_t *)(side->image + 0x2b58) = 0;
    }
    side->fe->link->deinit();
    side->link->deinit();
    memcpy(facil_image, side->image, FACIL_IMAGE_SIZE);
    memcpy(fe_image, side->fe->image, FE_IMAGE_SIZE);
    memcpy(global_image, global, GLOBAL_SIZE);
    memcpy(arena_image, arena, arena_used);
    *arena_size = arena_used;
    fe_reset_handles();
    facil_delete_cache();
}

static int facil_in_image(const struct facil_side *side, uint32_t address)
{
    const uint8_t *pointer = (const uint8_t *)(uintptr_t)address;
    return pointer >= side->image &&
           pointer < side->image + FACIL_IMAGE_SIZE;
}

static int facil_is_relocation(uint32_t offset)
{
    uint32_t index;

    for (index = 0;
         index < sizeof(facil_relocations) / sizeof(facil_relocations[0]);
         index++)
        if (offset >= facil_relocations[index] &&
            offset < (uint32_t)facil_relocations[index] + 4)
            return 1;
    return 0;
}

static int32_t facil_compare_image(const uint8_t *retail_image,
                                   const uint8_t *ours_image)
{
    uint32_t offset = 0x74;

    while (offset < FACIL_IMAGE_SIZE) {
        uint32_t retail_value;
        uint32_t ours_value;

        if (facil_is_relocation(offset)) {
            offset++;
            continue;
        }
        if (offset + 4 <= FACIL_IMAGE_SIZE &&
            !facil_is_relocation(offset + 1) &&
            !facil_is_relocation(offset + 2) &&
            !facil_is_relocation(offset + 3)) {
            retail_value = *(const uint32_t *)(retail_image + offset);
            ours_value = *(const uint32_t *)(ours_image + offset);
            if (facil_in_image(&facil_retail, retail_value) &&
                facil_in_image(&facil_ours, ours_value) &&
                (uint8_t *)(uintptr_t)retail_value - facil_retail.image ==
                (uint8_t *)(uintptr_t)ours_value - facil_ours.image) {
                offset += 4;
                continue;
            }
        }
        if (retail_image[offset] != ours_image[offset])
            return (int32_t)offset;
        offset++;
    }
    return -1;
}

/* ---- the query half: +0x18, +0x1c and +0x20 over synthetic caches --------
 *
 * Each scenario writes a seeded INDEX_FILE straight to the cache directory
 * and runs one seeded sequence of searches, node descriptions, record copies
 * and deinits on each side.  The rebuild path never produces flag 1 or
 * flag-2 runs on ordinary nodes, and +0x18 maps whatever file is there, so
 * the cache is generated rather than rebuilt: random five-level trees with
 * placeholder kinds, mixed-case and prefix names for the case-insensitive
 * comparisons and the case-sensitive prefix match, airport/name-record pairs,
 * stray flags, empty and missing caches, and sibling runs that cross the
 * queue's 0x200 and 0x280 capacity steps.
 *
 * Filter kinds stay within levels 1..6 and every node has a parent one level
 * up: an entry below region level would walk +0x18's ancestor match past the
 * root into the header, in retail as well.  While this phase runs, strlen,
 * stricmp and memcmp are logged with their string arguments, so the order of
 * comparisons is compared and not only their outcomes. */

#define QUERY_CACHE_MAX  0x100000u
#define QUERY_NODES_MAX  0x2000u
#define QUERY_OPS_MAX    8u

struct query_filter {
    uint32_t kind;
    const char *name;
};

struct query_filter_list {
    uint32_t count;
    struct query_filter *entries;
};

struct query_result {
    char *name;
    uint32_t kind;
    uint32_t node;
};

struct query_results {
    uint32_t count;
    struct query_result *records;
};

struct query_op {
    uint32_t type;
    uint32_t dedupe;
    uint32_t pick;
    uint32_t from_result;
    struct query_filter filter;
    struct query_filter entries[6];
    struct query_filter_list list;
    char names[7][16];
};

struct query_stats {
    uint32_t searches;
    uint32_t search_hits;
    uint32_t records;
    uint32_t most;
    uint32_t over_0x200;
    uint32_t over_0x280;
    uint32_t dedupe_drops;
    uint32_t describes;
    uint32_t describe_records;
    uint32_t copies;
    uint32_t empty;
    uint32_t missing;
};

static const char *const query_names[] = {
    "", "A", "a", "AB", "ab", "Ab", "aB", "ABC", "abc", "AbC", "B", "b",
    "BA", "ba", "BAC", "Z", "z", "ZZ", "zz", "ZZZ", "KSEA", "ksea", "KSEATAC",
    "SEATTLE", "Seattle", "seattle", "SEA"
};

static uint8_t query_cache[QUERY_CACHE_MAX];
static uint32_t query_cache_size;
static uint32_t query_nodes[QUERY_NODES_MAX];
static uint32_t query_node_count;
static uint32_t query_wide_level;
static uint32_t query_wide_count;
static int query_logging;
static struct query_op query_ops[QUERY_OPS_MAX];
static uint32_t query_op_count;
static struct query_stats query_stats;
static uint32_t query_last_nodes[16];
static uint32_t query_last_count;
static uint32_t query_arena_high;

#define QNODE(offset, field) (*(uint32_t *)(query_cache + (offset) + (field)))

static const char *query_name(void)
{
    return query_names[fe_rnd() %
                       (sizeof(query_names) / sizeof(query_names[0]))];
}

static uint32_t query_write_node(uint32_t parent, uint32_t kind,
                                 uint32_t flags, const char *name)
{
    uint32_t offset = query_cache_size;
    uint32_t name_size = (uint32_t)strlen(name) + 1;
    uint32_t sources = fe_rnd() % 3;
    uint32_t index;

    if (offset + 0x20 + name_size + sources * 0x14 > QUERY_CACHE_MAX ||
        query_node_count >= QUERY_NODES_MAX)
        return 0;
    memset(query_cache + offset, 0, 0x20);
    QNODE(offset, 0x00) = parent;
    QNODE(offset, 0x0c) = kind;
    QNODE(offset, 0x10) = name_size + sources * 0x14 + 0x0c;
    QNODE(offset, 0x14) = flags;
    QNODE(offset, 0x18) = name_size;
    QNODE(offset, 0x1c) = sources;
    memcpy(query_cache + offset + 0x20, name, name_size);
    for (index = 0; index < sources * 0x14; index++)
        query_cache[offset + 0x20 + name_size + index] = (uint8_t)fe_rnd();
    query_cache_size = offset + 0x20 + name_size + sources * 0x14;
    query_nodes[query_node_count++] = offset;
    return offset;
}

static void query_write_level(uint32_t parent, uint32_t level, int first)
{
    uint32_t count;
    uint32_t previous = 0;
    uint32_t index;

    if (first && level == query_wide_level)
        count = query_wide_count;
    else if (query_wide_level != 0)
        count = 1;
    else
        count = 1 + fe_rnd() % (level == 5 ? 4 : 3);
    for (index = 0; index < count; index++) {
        uint32_t kind;
        uint32_t flags = 0;
        uint32_t node;

        if (level == 5)
            kind = 0x50001 + fe_rnd() % 4;
        else
            kind = (level << 16) | (fe_rnd() % 5 != 0 ? 1 : 0);
        if (fe_rnd() % 8 == 0)
            flags |= 1;
        if (level != 5 && fe_rnd() % 10 == 0)
            flags |= 2;
        if (kind == 0x50001)
            flags |= 2;
        node = query_write_node(parent, kind, flags,
                                (kind & 0xffff) == 0 && fe_rnd() % 2 ?
                                "" : query_name());
        if (node == 0)
            return;
        if (previous != 0)
            QNODE(previous, 0x04) = node;
        else if (parent != 0)
            QNODE(parent, 0x08) = node;
        else
            QNODE(0, 0x04) = node;
        previous = node;
        if (kind == 0x50001) {
            node = query_write_node(parent, 0x50002,
                                    fe_rnd() % 8 == 0 ? 1 : 0, query_name());
            if (node == 0)
                return;
            QNODE(previous, 0x04) = node;
            previous = node;
        }
        if (level < 5)
            query_write_level(node, level + 1, first && index == 0);
    }
}

static void query_make_cache(void)
{
    uint32_t ids = fe_rnd() % 4;
    uint32_t index;

    memset(query_cache, 0, 12);
    query_node_count = 0;
    query_cache_size = 12 + ids * 16;
    *(uint32_t *)(query_cache + 8) = ids;
    for (index = 0; index < ids * 16; index++)
        query_cache[12 + index] = (uint8_t)fe_rnd();
    query_wide_level = 0;
    query_wide_count = 0;
    if (fe_rnd() % 25 == 0) {
        static const uint32_t widths[] = {
            0x1ff, 0x200, 0x201, 0x27f, 0x280, 0x281, 0x2c0
        };
        query_wide_level = 1 + fe_rnd() % 5;
        query_wide_count = widths[fe_rnd() % 7];
        /* An airport level doubles its run with name records. */
        if (query_wide_level == 5)
            query_wide_count /= 2;
    }
    if (fe_rnd() % 30 != 0)
        query_write_level(0, 1, 1);
    *(uint32_t *)(query_cache + 0) = query_cache_size;
    if (*(uint32_t *)(query_cache + 4) == 0)
        *(uint32_t *)(query_cache + 4) = query_cache_size;
}

static void query_make_filter(struct query_filter *filter)
{
    uint32_t level = 1 + fe_rnd() % 5;
    uint32_t choice = fe_rnd() % 10;

    if (choice < 6)
        filter->kind = level << 16;
    else if (choice < 9)
        filter->kind = level == 5 ? 0x50001 + fe_rnd() % 4 :
                       (level << 16) | (fe_rnd() % 2);
    else
        filter->kind = 6u << 16;
    choice = fe_rnd() % 10;
    filter->name = choice < 3 ? NULL : choice < 4 ? "" : query_name();
}

/* Steer onto names that exist: a real node's name, case-flipped at one
 * position half the time (stricmp still matches, the prefix memcmp may not),
 * and for +0x18's own filter truncated to a prefix half the time. */
static const char *query_steer_name(char *buffer, uint32_t node, int prefix)
{
    const char *name = (const char *)query_cache + node + 0x20;
    uint32_t length = (uint32_t)strlen(name);

    if (length > 15)
        length = 15;
    memcpy(buffer, name, length);
    buffer[length] = 0;
    if (length != 0 && fe_rnd() % 2) {
        uint32_t at = fe_rnd() % length;
        buffer[at] ^= 0x20;
    }
    if (prefix && length != 0 && fe_rnd() % 2)
        buffer[fe_rnd() % (length + 1)] = 0;
    return buffer;
}

static uint32_t query_ancestor(uint32_t node, uint32_t level)
{
    while (node != 0 && (QNODE(node, 0x0c) >> 16) > level)
        node = QNODE(node, 0x00);
    return node;
}

/* +0x18 scans only the first sibling run at the filter's level: the one
 * reached by following first children down from the root. */
static uint32_t query_first_run_node(void)
{
    uint32_t node = *(uint32_t *)(query_cache + 4);
    uint32_t depth = fe_rnd() % 5;
    uint32_t steps;

    while (depth-- != 0 && QNODE(node, 0x08) != 0)
        node = QNODE(node, 0x08);
    for (steps = fe_rnd() % 8; steps != 0 && QNODE(node, 0x04) != 0; steps--)
        node = QNODE(node, 0x04);
    return node;
}

static void query_steer_op(struct query_op *op)
{
    uint32_t node = fe_rnd() % 2 ? query_first_run_node() :
                    query_nodes[fe_rnd() % query_node_count];
    uint32_t level = QNODE(node, 0x0c) >> 16;
    uint32_t entry;

    op->filter.kind = fe_rnd() % 3 == 0 ? QNODE(node, 0x0c) : level << 16;
    op->filter.name = fe_rnd() % 4 == 0 ? NULL :
                      query_steer_name(op->names[6], node, 1);
    if (fe_rnd() % 3 == 0)
        return;
    /* The dialogs' list: usually the same node's own ancestry. */
    if (fe_rnd() % 3 == 0)
        node = query_nodes[fe_rnd() % query_node_count];
    op->list.count = 4;
    for (entry = 0; entry < 4; entry++) {
        uint32_t above = query_ancestor(node, entry + 1);
        uint32_t choice = fe_rnd() % 10;

        op->entries[entry].kind = (entry + 1) << 16;
        if (above != 0 && (QNODE(above, 0x0c) >> 16) == entry + 1 &&
            choice < 6)
            op->entries[entry].name =
                query_steer_name(op->names[entry], above, 0);
        else
            op->entries[entry].name = choice < 8 ? NULL : query_name();
    }
}

static void query_make_ops(void)
{
    uint32_t index;

    query_op_count = 1 + fe_rnd() % QUERY_OPS_MAX;
    for (index = 0; index < query_op_count; index++) {
        struct query_op *op = &query_ops[index];
        uint32_t choice = fe_rnd() % 20;
        uint32_t entry;

        memset(op, 0, sizeof(*op));
        op->type = choice < 10 ? 0 : choice < 15 ? 1 : choice < 18 ? 2 : 3;
        op->dedupe = fe_rnd() % 2;
        op->pick = fe_rnd();
        op->from_result = fe_rnd() % 3 == 0;
        query_make_filter(&op->filter);
        op->list.entries = op->entries;
        if (fe_rnd() % 2) {
            /* The dialogs' shape: region..city in order. */
            op->list.count = 4;
            for (entry = 0; entry < 4; entry++) {
                uint32_t choice = fe_rnd() % 10;
                op->entries[entry].kind = (entry + 1) << 16;
                op->entries[entry].name =
                    choice < 4 ? NULL : choice < 5 ? "" : query_name();
            }
        } else {
            op->list.count = fe_rnd() % 7;
            for (entry = 0; entry < op->list.count; entry++)
                query_make_filter(&op->entries[entry]);
        }
        if (query_node_count != 0 && fe_rnd() % 3 != 0)
            query_steer_op(op);
    }
    /* A wide cache is only worth having if its run reaches the queue. */
    if (query_wide_level != 0) {
        query_ops[0].type = 0;
        query_ops[0].filter.kind = query_wide_level << 16;
        query_ops[0].filter.name = NULL;
        query_ops[0].list.count = 0;
    }
}

static int query_write_file(int present)
{
    char path[MAX_PATH * 2];

    facil_delete_cache();
    if (!present)
        return 1;
    snprintf(path, sizeof(path), "%s\\INDEX_FILE", cache_root);
    return fe_write_fixture_file(path, query_cache, query_cache_size);
}

static void query_put_text(const char *value, uint32_t limit)
{
    if (value == NULL)
        fe_put("null");
    else
        fe_put("'%.*s'", (int)limit, value);
}

static uint32_t STDCALL query_host_strlen(const char *value)
{
    uint32_t length = host_strlen(value);

    if (query_logging) {
        fe_put(" len(");
        query_put_text(value, 64);
        fe_put(")");
    }
    return length;
}

static int32_t STDCALL query_host_stricmp(const char *left, const char *right)
{
    if (query_logging) {
        fe_put(" icmp(");
        query_put_text(left, 64);
        fe_put(",");
        query_put_text(right, 64);
        fe_put(")");
    }
    return host_stricmp(left, right);
}

static int32_t STDCALL query_host_memcmp(const void *left, const void *right,
                                         uint32_t size)
{
    if (query_logging) {
        fe_put(" mcmp(");
        query_put_text((const char *)left, size < 64 ? size : 64);
        fe_put(",");
        query_put_text((const char *)right, size < 64 ? size : 64);
        fe_put(",%#x)", size);
    }
    return host_memcmp(left, right, size);
}

static void query_put_results(const char *what, uint32_t result,
                              struct query_results *out)
{
    uint32_t index;

    fe_put(" %s=%#x,count=%#x,records=", what, result, out->count);
    fe_put_pointer(fe_ptr(out->records));
    if (out->records == NULL)
        return;
    query_last_count = 0;
    for (index = 0; index < out->count; index++) {
        struct query_result *record = &out->records[index];

        fe_put(" [");
        query_put_text(record->name, 64);
        fe_put(",%#x,%#x]", record->kind, record->node);
        if (query_last_count < 16)
            query_last_nodes[query_last_count++] = record->node;
        host_free((void **)&record->name);
    }
    host_free((void **)&out->records);
}

static uint32_t query_pick_node(const struct query_op *op)
{
    if (op->from_result && query_last_count != 0)
        return query_last_nodes[op->pick % query_last_count];
    if (query_node_count == 0)
        return *(uint32_t *)(query_cache + 4);
    return query_nodes[op->pick % query_node_count];
}

static uint32_t query_count_text(const char *text, const char *pattern)
{
    uint32_t count = 0;

    while ((text = strstr(text, pattern)) != NULL) {
        count++;
        text++;
    }
    return count;
}

static int query_mapped(const struct facil_side *side)
{
    return *(uint32_t *)(side->image + 0x2d08) != 0;
}

/* The view copies, the queue and the handles are addresses or handle values
 * private to a side.  Compare whether each is set, and the view copies
 * against each other. */
static void query_normalize_image(uint8_t *image)
{
    static const uint32_t private_fields[] = {
        0x2b50, 0x2b54, 0x2b58, 0x2d04, 0x2d0c, 0x2d10
    };
    uint32_t view = *(uint32_t *)(image + 0x2b58);
    uint32_t index;

    for (index = 0; index < 6; index++) {
        uint32_t *field = (uint32_t *)(image + private_fields[index]);

        if (index < 2)
            *field = *field == view ? 0x7e7e0000u : *field - view;
        else if (*field != 0 && *field != 0xffffffffu)
            *field = 0x7e7e0001u;
    }
}

static void query_run_side(struct facil_side *side, uint32_t scenario,
                           int present, uint8_t *image,
                           uint8_t *arena_image, uint32_t *arena_size,
                           struct query_stats *stats)
{
    uint32_t index;

    current = side->fe;
    facil_current = side;
    current_scenario = scenario;
    transcript_len = 0;
    transcript[0] = 0;
    arena_used = 0;
    /* Allocation padding is never written; reset what earlier runs used. */
    memset(arena, 0xa5, query_arena_high);
    memcpy(side->image, side->pristine, FACIL_IMAGE_SIZE);
    query_last_count = 0;
    if (!query_write_file(present)) {
        printf("cannot write synthetic INDEX_FILE\n");
        exit(2);
    }
    facil_index_active = 1;
    fe_put("QUERY %u", scenario);
    side->link->init();
    query_logging = 1;
    for (index = 0; index < query_op_count; index++) {
        struct query_op *op = &query_ops[index];
        struct query_results out = { 0x5a5a5a5au, (void *)0x5a5a5a5a };
        uint32_t result;
        uint32_t node;
        uint32_t start;

        current_operation = 1000 + index;
        switch (op->type) {
        case 0:
            fe_put(" | search(%#x,", op->filter.kind);
            query_put_text(op->filter.name, 64);
            fe_put(",list=%u,dedupe=%u)", op->list.count, op->dedupe);
            start = transcript_len;
            result = side->link->search(fe_ptr(&op->list),
                                        fe_ptr(&op->filter), fe_ptr(&out),
                                        op->dedupe);
            /* A successful search frees each popped record, the name of each
             * dropped one, and its two filter copies. */
            if (stats != NULL && result == 0 && op->dedupe &&
                out.records != NULL) {
                uint32_t frees = query_count_text(transcript + start,
                                                  " free(");
                if (frees > out.count + 2)
                    stats->dedupe_drops += (frees - out.count - 2) / 2;
            }
            if (stats != NULL) {
                stats->searches++;
                if (out.count != 0)
                    stats->search_hits++;
                stats->records += out.count;
                if (out.count > stats->most)
                    stats->most = out.count;
            }
            query_put_results("SEARCH", result, &out);
            break;
        case 1:
            if (!query_mapped(side))
                break;
            node = query_pick_node(op);
            fe_put(" | describe(%#x,list=%u)", node, op->list.count);
            result = side->link->names(node, fe_ptr(&op->list), fe_ptr(&out));
            if (stats != NULL) {
                stats->describes++;
                stats->describe_records += out.count;
            }
            query_put_results("DESCRIBE", result, &out);
            break;
        case 2: {
            uint8_t *record = NULL;
            uint32_t size = 0;

            if (!query_mapped(side))
                break;
            node = query_pick_node(op);
            fe_put(" | record(%#x)", node);
            result = side->link->record(node, fe_ptr(&record), fe_ptr(&size));
            fe_put(" RECORD=%#x,size=%#x,data=", result, size);
            fe_put_blob(record, size);
            host_free((void **)&record);
            if (stats != NULL)
                stats->copies++;
            break;
        }
        default:
            fe_put(" | deinit");
            fe_put(" DEINIT=%#x", side->link->deinit());
            break;
        }
    }
    query_logging = 0;
    fe_put(" | final DEINIT=%#x", side->link->deinit());
    facil_index_active = 0;
    if (transcript_len >= sizeof(transcript) - 512)
        fe_put(" TRANSCRIPT_FULL");
    memcpy(image, side->image, FACIL_IMAGE_SIZE);
    query_normalize_image(image);
    memcpy(arena_image, arena, arena_used);
    *arena_size = arena_used;
    if (arena_used > query_arena_high)
        query_arena_high = arena_used;
}


static int query_run(uint32_t scenarios, uint8_t *retail_arena,
                     uint8_t *ours_arena)
{
    static uint8_t retail_image[FACIL_IMAGE_SIZE];
    static uint8_t ours_image[FACIL_IMAGE_SIZE];
    static char retail_transcript[TRANSCRIPT_SIZE];
    uint32_t retail_arena_size;
    uint32_t ours_arena_size;
    uint32_t scenario;
    int32_t difference;
    int bad = 0;

    SET_SLOT(host, 0x2b8, query_host_strlen);
    SET_SLOT(host, 0x2c0, query_host_stricmp);
    SET_SLOT(host, 0x228, query_host_memcmp);
    memset(&query_stats, 0, sizeof(query_stats));
    memset(arena, 0xa5, ARENA_SIZE);
    query_arena_high = 0;
    for (scenario = 0; scenario < scenarios && bad < 5; scenario++) {
        int present;
        int failed = 0;

        rng = 0x9e3779b9u * (scenario + 1) ^ 0xfac11u;
        if (rng == 0)
            rng = 1;
        query_make_cache();
        present = fe_rnd() % 40 != 0;
        if (!present)
            query_stats.missing++;
        else if (*(uint32_t *)(query_cache + 0) ==
                 *(uint32_t *)(query_cache + 4))
            query_stats.empty++;
        query_make_ops();

        counting = 1;
        query_run_side(&facil_retail, scenario, present, retail_image,
                       retail_arena, &retail_arena_size, &query_stats);
        memcpy(retail_transcript, transcript, transcript_len + 1);
        counting = 0;
        query_stats.over_0x200 +=
            query_count_text(retail_transcript, " realloc(1,0xa00,");
        query_stats.over_0x280 +=
            query_count_text(retail_transcript, " realloc(1,0xc00,");
        query_run_side(&facil_ours, scenario, present, ours_image,
                       ours_arena, &ours_arena_size, NULL);

        if (strcmp(retail_transcript, transcript) != 0) {
            printf("scenario %u: transcript differs\n", scenario);
            fe_dump_transcript_difference(retail_transcript, transcript);
            failed = 1;
        }
        if (strstr(retail_transcript, "TRANSCRIPT_FULL") != NULL) {
            printf("scenario %u: transcript full\n", scenario);
            failed = 1;
        }
        difference = facil_compare_image(retail_image, ours_image);
        if (difference >= 0) {
            printf("scenario %u: FACIL image differs at +%#x: %02x/%02x\n",
                   scenario, (uint32_t)difference, retail_image[difference],
                   ours_image[difference]);
            failed = 1;
        }
        if (retail_arena_size != ours_arena_size ||
            memcmp(retail_arena, ours_arena, retail_arena_size) != 0) {
            printf("scenario %u: arena differs: retail %#x comparison %#x\n",
                   scenario, retail_arena_size, ours_arena_size);
            failed = 1;
            if (retail_arena_size == ours_arena_size)
                fe_dump_difference("arena", retail_arena, ours_arena,
                                   retail_arena_size);
        }
        bad += failed;
    }
    printf("QUERY %u scenarios: %u searches (%u with results, %u records, "
           "most %u, %u/%u queue growths past 0x200/0x280, %u dedupe drops), "
           "%u describes (%u records), %u record copies, "
           "%u empty and %u missing caches, %d differ\n",
           scenario, query_stats.searches, query_stats.search_hits,
           query_stats.records, query_stats.most, query_stats.over_0x200,
           query_stats.over_0x280, query_stats.dedupe_drops,
           query_stats.describes,
           query_stats.describe_records, query_stats.copies,
           query_stats.empty, query_stats.missing, bad);
    SET_SLOT(host, 0x2b8, host_strlen);
    SET_SLOT(host, 0x2c0, host_stricmp);
    SET_SLOT(host, 0x228, host_memcmp);
    return bad;
}

int main(int argc, char **argv)
{
    static uint8_t retail_facil[FACIL_IMAGE_SIZE];
    static uint8_t ours_facil[FACIL_IMAGE_SIZE];
    static uint8_t retail_fe[FE_IMAGE_SIZE];
    static uint8_t ours_fe[FE_IMAGE_SIZE];
    static uint8_t retail_global[GLOBAL_SIZE];
    static uint8_t ours_global[GLOBAL_SIZE];
    static uint8_t retail_cache[FACIL_CACHE_MAX];
    static uint8_t ours_cache[FACIL_CACHE_MAX];
    static char retail_transcript[TRANSCRIPT_SIZE];
    uint8_t *retail_arena;
    uint8_t *ours_arena;
    uint8_t *util;
    uint32_t retail_arena_size;
    uint32_t ours_arena_size;
    uint32_t retail_cache_size;
    uint32_t ours_cache_size;
    uint32_t retail_first;
    uint32_t retail_second;
    uint32_t ours_first;
    uint32_t ours_second;
    int32_t image_difference;
    char temp[MAX_PATH * 2];
    uint32_t scenarios;
    int query_bad;
    int bad = 0;

    SetUnhandledExceptionFilter(fe_unhandled);
    if (argc < 4) {
        printf("usage: facildiff.exe retail-facil comparison-facil "
               "fixture-root [query-scenarios]\n");
        return 2;
    }
    if (GetFullPathNameA(argv[3], sizeof(fixture), fixture, NULL) == 0 ||
        GetCurrentDirectoryA(sizeof(original_dir), original_dir) == 0 ||
        !fe_validate_fixture(fixture))
        return 2;
    if (GetTempPathA(sizeof(temp), temp) == 0)
        return 2;
    snprintf(cache_root, sizeof(cache_root), "%sOpenFS98-facildiff-%lu",
             temp, (unsigned long)GetCurrentProcessId());
    if (!CreateDirectoryA(cache_root, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        printf("cannot create cache directory\n");
        return 2;
    }
    if (!facil_make_fixture()) {
        printf("cannot create FACIL BGL fixture\n");
        return 2;
    }
    retail_arena = (uint8_t *)malloc(ARENA_SIZE);
    ours_arena = (uint8_t *)malloc(ARENA_SIZE);
    if (retail_arena == NULL || ours_arena == NULL)
        return 2;

    facil_build_stubs();
    util = fe_load_support("UTIL_retail.dll", 20);
    SET_SLOT(util, 0x0a8, facil_util_alert);
    SET_SLOT(util, 0x0ac, facil_util_alert_close);
    if (!fe_load_side(&retail, "FE_facil_retail.dll", "retail FE", util) ||
        !fe_load_side(&ours, "FE_facil_ours.dll", "comparison FE", util) ||
        !facil_load_side(&facil_retail, argv[1], "retail FACIL", &retail,
                         util) ||
        !facil_load_side(&facil_ours, argv[2], "comparison FACIL", &ours,
                         util))
        return 2;
    if (!SetCurrentDirectoryA(fixture)) {
        printf("cannot enter fixture root\n");
        return 2;
    }

    counting = 1;
    facil_run_side(&facil_retail, retail_facil, retail_fe, retail_global,
                   retail_arena, &retail_arena_size, retail_cache,
                   &retail_cache_size, &retail_first, &retail_second);
    strncpy(retail_transcript, transcript, sizeof(retail_transcript) - 1);
    retail_transcript[sizeof(retail_transcript) - 1] = 0;
    counting = 0;
    facil_run_side(&facil_ours, ours_facil, ours_fe, ours_global, ours_arena,
                   &ours_arena_size, ours_cache, &ours_cache_size,
                   &ours_first, &ours_second);

    if (retail_first != ours_first || retail_second != ours_second) {
        printf("results differ: retail %#x,%#x comparison %#x,%#x\n",
               retail_first, retail_second, ours_first, ours_second);
        bad++;
    }
    if (strcmp(retail_transcript, transcript) != 0) {
        fe_dump_transcript_difference(retail_transcript, transcript);
        bad++;
    }
    if (retail_cache_size != ours_cache_size ||
        memcmp(retail_cache, ours_cache,
               retail_cache_size < ours_cache_size ?
               retail_cache_size : ours_cache_size) != 0) {
        printf("INDEX_FILE differs: retail %#x comparison %#x bytes\n",
               retail_cache_size, ours_cache_size);
        if (retail_cache_size == ours_cache_size)
            fe_dump_difference("INDEX_FILE", retail_cache, ours_cache,
                               retail_cache_size);
        bad++;
    }
    image_difference = facil_compare_image(retail_facil, ours_facil);
    if (image_difference >= 0) {
        printf("FACIL image differs at +%#x: %02x/%02x\n",
               (uint32_t)image_difference,
               retail_facil[image_difference], ours_facil[image_difference]);
        bad++;
    }
    image_difference = fe_compare_image(retail_fe, ours_fe);
    if (image_difference >= 0) {
        printf("FE image differs at +%#x: %02x/%02x\n",
               (uint32_t)image_difference,
               retail_fe[image_difference], ours_fe[image_difference]);
        bad++;
    }
    if (memcmp(retail_global, ours_global, GLOBAL_SIZE) != 0) {
        fe_dump_difference("GLOBAL", retail_global, ours_global, GLOBAL_SIZE);
        bad++;
    }
    if (retail_arena_size != ours_arena_size ||
        (retail_arena_size != 0 &&
         memcmp(retail_arena, ours_arena, retail_arena_size) != 0)) {
        printf("arena differs: retail %#x comparison %#x bytes\n",
               retail_arena_size, ours_arena_size);
        bad++;
    }

    printf("INDEX_FILE %#x bytes, arena %#x bytes, %d differences\n",
           retail_cache_size, retail_arena_size, bad);
    scenarios = argc > 4 ? (uint32_t)strtoul(argv[4], NULL, 0) : 20000;
    query_bad = query_run(scenarios, retail_arena, ours_arena);
    bad += query_bad;
    if (bad == 0)
        printf("IDENTICAL\n");
    SetCurrentDirectoryA(original_dir);
    facil_delete_cache();
    facil_remove_fixture();
    RemoveDirectoryA(cache_root);
    free(retail_arena);
    free(ours_arena);
    return bad != 0;
}
