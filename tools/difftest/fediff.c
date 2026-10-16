/* Differential test: retail FE.DLL against the OpenFS98 build.
 *
 * Both DLLs are loaded into one process and driven through Linkage over the
 * installed Scenery tree.  The host passes file and directory operations
 * through to that fixture, allocates from a deterministic arena, and models
 * the services used by FE's assembly engine and compiled-C file services.
 * Retail UTIL supplies FE's arithmetic.  CHAIN, BOOSTER and FACIL are logging
 * stubs.  Self-checking synthetic fixtures cover compressed textures and BGL
 * section selection independently of either DLL's result.
 *
 * A scenario starts with a pristine FE image and GLOBAL, then exercises one
 * caller-shaped operation: G3D's scene stream, MAIN's searches and navaid or
 * ground lookups, FACIL's listing, DYNAMIC's blocks, object lookup, box tests,
 * BOOSTER's register/unregister callbacks, HG2D's texture loader, or FACIL's
 * BGL file and section services.  Compared after every scenario: the import
 * transcript, outputs, GLOBAL, the deterministic arena and FE's .data image.
 * Module-local pointers compare by image offset.  The Scenery Library dialog
 * handler pointers (generated as FE_REL_TEXT entries) and the live
 * CRITICAL_SECTION are outside this verification and are ignored explicitly.
 *
 * Build: make fediff     Run: make fediff-run
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <stdint.h>

#define STDCALL __attribute__((stdcall))

#define FE_IMAGE_SIZE       0x62e0u
#define FE_DATA_START       0x00d4u
#define FE_CS_START         0x1734u
#define FE_CS_END           0x174cu
#define GLOBAL_SIZE         0x1300u
#ifndef ARENA_SIZE
#define ARENA_SIZE          (16u << 20)
#endif
#define TRANSCRIPT_SIZE     (8u << 20)
#define OUTPUT_SIZE         (256u << 10)
#define MAX_HOST_FILES      16
#define MAX_HOST_FINDS      8
#define MAX_STEPS           4096

struct fs98_import {
    uint32_t id;
    void *link;
};

typedef uint32_t (STDCALL *fe_fn0)(void);
typedef uint32_t (STDCALL *fe_fn1)(uint32_t arg0);
typedef uint32_t (STDCALL *fe_fn2)(uint32_t arg0, uint32_t arg1);
typedef uint32_t (STDCALL *fe_fn3)(uint32_t arg0, uint32_t arg1, uint32_t arg2);
typedef uint32_t (STDCALL *fe_fn4)(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                 uint32_t arg3);
typedef uint32_t (STDCALL *fe_fn5)(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                 uint32_t arg3, uint32_t arg4);

struct fe_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved0c;
    uint32_t reserved10;
    void *dialogs;
    void *slots[29];
};

struct fe_side {
    const char *name;
    HMODULE module;
    struct fe_linkage *link;
    uint8_t *image;
    uint8_t *module_lo;
    uint8_t *module_hi;
    uint8_t pristine[FE_IMAGE_SIZE];
};

struct host_file {
    HANDLE file;
    int open;
    char path[MAX_PATH * 2];
};

struct host_find {
    HANDLE find;
    WIN32_FIND_DATAA data;
    int open;
    int more;
};

struct host_find_entry {
    char name[0x104];
    int32_t type;
    uint32_t size;
    int32_t writable;
};

struct fe_scenario {
    uint32_t number;
    uint32_t operation;
    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t radius;
    uint32_t level;
    uint16_t latitude_lo;
    uint16_t longitude_lo;
    uint16_t altitude_lo;
    uint16_t flags;
    uint16_t frequency;
    uint8_t channel;
};

enum {
    FE_OP_SCENE,
    FE_OP_NEXT,
    FE_OP_NAVAID,
    FE_OP_GROUND,
    FE_OP_LIST,
    FE_OP_DYNAMIC,
    FE_OP_OBJECT,
    FE_OP_BOX,
    FE_OP_REGISTER,
    FE_OP_TEXTURE,
    FE_OP_FACIL,
    FE_OP_COUNT
};

static struct fe_side retail;
static struct fe_side ours;
static struct fe_side *current;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t booster[0x24];
static uint8_t chain[0x24];
static uint8_t facil[0x34];
static uint8_t g3d[0x20];
static uint8_t numeric[0x20];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t output[OUTPUT_SIZE] __attribute__((aligned(16)));
static struct host_file host_files[MAX_HOST_FILES];
static struct host_find host_finds[MAX_HOST_FINDS];
static char fixture[MAX_PATH * 2];
static char original_dir[MAX_PATH * 2];
static char part3_root[MAX_PATH * 2];
static char transcript[TRANSCRIPT_SIZE];
static uint32_t transcript_len;
static void (STDCALL *frame)(uint32_t arg0, uint32_t arg1);
static int counting;
static uint32_t rng = 0x18fe98a5u;
static uint32_t coverage[FE_OP_COUNT];
static uint32_t files_opened;
static uint32_t records;
static uint32_t dynamic_oversize;
static uint32_t texture_loads;
static uint32_t facil_reads;
static uint32_t part3_fixture_failures;
static volatile uint32_t service_step;
static uint32_t current_scenario;
static uint32_t current_operation;
static fe_fn4 scene_call;
static fe_fn1 reset_call;

/* Absolute storage lets the assembly boundary recover even if a retail call
 * returns with ESP or EBP displaced.  These are globals because the inline
 * assembly names them directly. */
void *guard_call;
uint32_t guard_args[4];
uint32_t guard_result;
uint32_t guard_stack;
uint32_t guard_ebp;
int32_t guard_stack_delta;
int32_t guard_ebp_delta;
uint32_t guard_in_ebx;
uint32_t guard_in_esi;
uint32_t guard_in_edi;
uint32_t guard_ebx;
uint32_t guard_esi;
uint32_t guard_edi;

void fe_guard_call4_asm(void);

__asm__(
    ".text\n"
    ".globl _fe_guard_call4_asm\n"
    "_fe_guard_call4_asm:\n"
    "pushl %ebp\n"
    "movl %esp, %ebp\n"
    "pushl %ebx\n"
    "pushl %esi\n"
    "pushl %edi\n"
    "movl %esp, _guard_stack\n"
    "movl %ebp, _guard_ebp\n"
    "movl _guard_in_ebx, %ebx\n"
    "movl _guard_in_esi, %esi\n"
    "movl _guard_in_edi, %edi\n"
    "pushl _guard_args+12\n"
    "pushl _guard_args+8\n"
    "pushl _guard_args+4\n"
    "pushl _guard_args\n"
    "call *_guard_call\n"
    "movl %eax, _guard_result\n"
    "movl %ebx, _guard_ebx\n"
    "movl %esi, _guard_esi\n"
    "movl %edi, _guard_edi\n"
    "movl %esp, %eax\n"
    "subl _guard_stack, %eax\n"
    "movl %eax, _guard_stack_delta\n"
    "movl %ebp, %eax\n"
    "subl _guard_ebp, %eax\n"
    "movl %eax, _guard_ebp_delta\n"
    "movl _guard_stack, %esp\n"
    "movl _guard_ebp, %ebp\n"
    "popl %edi\n"
    "popl %esi\n"
    "popl %ebx\n"
    "leave\n"
    "ret\n"
);

#define FE_REL_DATA(offset, target)
#define FE_REL_TEXT(offset, rva) (uint16_t)(offset),
#define FE_RELOCS
static const uint16_t text_relocs[] = {
#include "fe_data.inc"
};
#undef FE_RELOCS
#undef FE_REL_TEXT
#undef FE_REL_DATA

static uint32_t fe_rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint32_t fe_ptr(const void *value)
{
    return (uint32_t)(uintptr_t)value;
}

static void fe_put(const char *format, ...)
{
    va_list args;
    int written;

    if (transcript_len >= sizeof(transcript) - 512)
        return;
    va_start(args, format);
    written = vsnprintf(transcript + transcript_len,
                         sizeof(transcript) - transcript_len,
                         format, args);
    va_end(args);
    if (written > 0)
        transcript_len += (uint32_t)written;
}

static void fe_put_string(const char *value)
{
    if (value == NULL)
        fe_put("null");
    else
        fe_put("'%.160s'", value);
}

static int fe_in_image(const struct fe_side *side, uint32_t address)
{
    const uint8_t *ptr = (const uint8_t *)(uintptr_t)address;
    return ptr >= side->image && ptr < side->image + FE_IMAGE_SIZE;
}

static void fe_put_pointer(uint32_t address)
{
    const uint8_t *ptr = (const uint8_t *)(uintptr_t)address;

    if (address == 0)
        fe_put("null");
    else if (ptr >= arena && ptr < arena + ARENA_SIZE)
        fe_put("arena+%#x", (uint32_t)(ptr - arena));
    else if (ptr >= global && ptr < global + GLOBAL_SIZE)
        fe_put("G+%#x", (uint32_t)(ptr - global));
    else if (ptr >= output && ptr < output + OUTPUT_SIZE)
        fe_put("output+%#x", (uint32_t)(ptr - output));
    else if (current != NULL && fe_in_image(current, address))
        fe_put("image+%#x", (uint32_t)(ptr - current->image));
    else
        fe_put("%#x", address);
}

static uint32_t fe_digest(const void *buffer, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint32_t digest = 0x811c9dc5u;
    uint32_t index;

    for (index = 0; index < size; index++)
        digest = (digest ^ bytes[index]) * 0x01000193u;
    return digest;
}

static void fe_put_blob(const void *data, uint32_t size)
{
    if (data == NULL) {
        fe_put("null");
        return;
    }
    fe_put("%#x/%08x", size, fe_digest(data, size));
}

static void fe_reset_handles(void)
{
    int index;

    for (index = 0; index < MAX_HOST_FILES; index++) {
        if (host_files[index].open)
            CloseHandle(host_files[index].file);
        memset(&host_files[index], 0, sizeof(host_files[index]));
        host_files[index].file = INVALID_HANDLE_VALUE;
    }
    for (index = 0; index < MAX_HOST_FINDS; index++) {
        if (host_finds[index].open)
            FindClose(host_finds[index].find);
        memset(&host_finds[index], 0, sizeof(host_finds[index]));
        host_finds[index].find = INVALID_HANDLE_VALUE;
    }
}

static void *fe_arena_get(uint32_t size)
{
    uint32_t total = 16 + ((size + 15) & ~15u);
    uint8_t *block;

    if (size > ARENA_SIZE || arena_used + total > ARENA_SIZE) {
        printf("arena exhausted: %#x more after %#x, scenario %u op %u\n",
               size, arena_used, current_scenario, current_operation);
        fflush(stdout);
        return NULL;
    }
    block = arena + arena_used;
    arena_used += total;
    *(uint32_t *)block = size;
    return block + 16;
}

static int32_t STDCALL host_alloc(int32_t flags, uint32_t size, void **out,
                                  int32_t zero)
{
    void *block = fe_arena_get(size);

    fe_put(" alloc(%d,%#x,%d)=", flags, size, zero);
    fe_put_pointer(fe_ptr(block));
    if (block == NULL) {
        fe_put(" ARENA_EXHAUSTED");
        printf("arena exhausted (%#x bytes) in scenario %u op %u\n", size,
               current_scenario, current_operation);
        fflush(stdout);
        ExitProcess(2);
    }
    memset(block, zero ? 0 : 0xa5, size);
    *out = block;
    return 0;
}

static int32_t STDCALL host_realloc(int32_t flags, uint32_t size, void **value)
{
    uint8_t *old = (uint8_t *)*value;
    uint8_t *new = (uint8_t *)fe_arena_get(size);
    uint32_t old_size = old != NULL ? *(uint32_t *)(old - 16) : 0;

    fe_put(" realloc(%d,%#x,", flags, size);
    fe_put_pointer(fe_ptr(old));
    fe_put(")=");
    fe_put_pointer(fe_ptr(new));
    if (new == NULL)
        ExitProcess(2);
    memset(new, 0xa5, size);
    if (old != NULL)
        memcpy(new, old, old_size < size ? old_size : size);
    *value = new;
    return 0;
}

static int32_t STDCALL host_free(void **value)
{
    fe_put(" free(");
    fe_put_pointer(fe_ptr(*value));
    fe_put(")");
    *value = NULL;
    return 0;
}

static int32_t STDCALL host_memset(void *dst, int32_t byte, uint32_t size)
{
    memset(dst, byte, size);
    return 0;
}

static int32_t STDCALL host_memmove(void *dst, const void *src, uint32_t size)
{
    memmove(dst, src, size);
    return 0;
}

static int32_t STDCALL host_memcmp(const void *left, const void *right,
                                   uint32_t size)
{
    return memcmp(left, right, size);
}

static int32_t STDCALL host_basedir(char *out)
{
    strcpy(out, fixture);
    fe_put(" basedir=");
    fe_put_string(out);
    return 0;
}

static int32_t STDCALL host_loadstring(uint32_t id, char *out, int32_t size)
{
    const char *text = "";

    if (id == 0x9003)
        text = "SCENERY";
    else if (id == 0x9006)
        text = "TEXTURE";
    else if (id == 0xf424)
        text = "Insert media";
    else if (id == 0xf427)
        text = "Cache directory not found";
    if (size > 0) {
        strncpy(out, text, (size_t)size - 1);
        out[size - 1] = 0;
    }
    fe_put(" string(%#x)='%.40s'", id, text);
    return (int32_t)strlen(text);
}

static uint32_t STDCALL host_strlen(const char *value)
{
    return value != NULL ? (uint32_t)strlen(value) : 0;
}

static char *STDCALL host_strcpy(char *dst, const char *src)
{
    return strcpy(dst, src);
}

static int32_t STDCALL host_strcmp(const char *left, const char *right)
{
    const uint8_t *left_bytes = (const uint8_t *)left;
    const uint8_t *right_bytes = (const uint8_t *)right;

    for (;;) {
        if (left_bytes[0] != right_bytes[0])
            return left_bytes[0] < right_bytes[0] ? -1 : 1;
        if (left_bytes[0] == 0)
            return 0;
        if (left_bytes[1] != right_bytes[1])
            return left_bytes[1] < right_bytes[1] ? -1 : 1;
        if (left_bytes[1] == 0)
            return 0;
        left_bytes += 2;
        right_bytes += 2;
    }
}

static int32_t STDCALL host_stricmp(const char *left, const char *right)
{
    return _strcmpi(left, right);
}

static int32_t STDCALL host_fit(char *dst, const char *src, uint32_t size)
{
    if (size == 0)
        return 0;
    strncpy(dst, src, size - 1);
    dst[size - 1] = 0;
    return 0;
}

static int32_t STDCALL host_dialog(uint32_t id, void *param)
{
    (void)param;
    fe_put(" dialog(%#x)=0x10", id);
    return 0x10;
}

static int32_t STDCALL host_join(char *out, const char *drive,
                               const char *dir, const char *name)
{
    char buffer[0x208];
    size_t length;

    memset(buffer, 0, sizeof(buffer));
    if (out == NULL)
        return -1;
    if (drive != NULL && drive[0] != 0) {
        strcat(buffer, drive);
        if (strlen(buffer) == 1)
            strcat(buffer, ":");
        length = strlen(buffer);
        if (buffer[length - 1] == '\\')
            buffer[length - 1] = 0;
    }
    if (dir != NULL && dir[0] != 0) {
        if (dir[0] != '\\' && buffer[0] != 0)
            strcat(buffer, "\\");
        strcat(buffer, dir);
        length = strlen(buffer);
        if (buffer[length - 1] == '\\')
            buffer[length - 1] = 0;
    }
    if (name != NULL && name[0] != 0) {
        if (name[0] != '\\' && buffer[0] != 0)
            strcat(buffer, "\\");
        strcat(buffer, name);
    }
    if (strlen(buffer) >= 0x104) {
        out[0] = 0;
        return -1;
    }
    strcpy(out, buffer);
    fe_put(" join=");
    fe_put_string(out);
    return 0;
}

static int32_t fe_extension_id(const char *extension)
{
    if (extension == NULL || extension[0] == 0)
        return 0;
    if (_strcmpi(extension, ".BGL") == 0)
        return 1;
    if (_strcmpi(extension, ".VIS") == 0)
        return 0;
    return -1;
}

static const char *fe_extension_text(int32_t extension)
{
    if (extension == 0 || extension == -2)
        return "";
    if (extension == 1)
        return ".BGL";
    if (extension == 9)
        return ".PAL";
    if (extension == 10)
        return ".HAZ";
    return NULL;
}

static int32_t STDCALL host_split(const char *path, char *drive, char *dir,
                                  char *name, int32_t *extension_id)
{
    char extension[0x100];

    if (path == NULL)
        return -1;
    extension[0] = 0;
    _splitpath(path, drive, dir, name, extension);
    if (extension_id != NULL)
        *extension_id = fe_extension_id(extension);
    fe_put(" split(");
    fe_put_string(path);
    fe_put(")");
    return 0;
}

static struct host_file *fe_new_host_file(void)
{
    int index;

    for (index = 0; index < MAX_HOST_FILES; index++)
        if (!host_files[index].open)
            return &host_files[index];
    return NULL;
}

static int32_t fe_open_file(struct host_file **out, const char *path,
                            int32_t extension_id, int32_t mode, int fatal)
{
    static const DWORD access[4] = {
        GENERIC_READ, GENERIC_READ | GENERIC_WRITE, GENERIC_WRITE, GENERIC_WRITE
    };
    const char *extension = fe_extension_text(extension_id);
    struct host_file *file;
    char full[MAX_PATH * 2];

    *out = NULL;
    if (extension == NULL || mode < 0 || mode > 3)
        return -1;
    snprintf(full, sizeof(full), "%s%s", path, extension);
    file = fe_new_host_file();
    if (file == NULL)
        ExitProcess(2);
    file->file = CreateFileA(full, access[mode], FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    fe_put(" open%s(", fatal ? "!" : "");
    fe_put_string(full);
    if (file->file == INVALID_HANDLE_VALUE) {
        fe_put(")=-1");
        return -1;
    }
    file->open = 1;
    strncpy(file->path, full, sizeof(file->path) - 1);
    file->path[sizeof(file->path) - 1] = 0;
    if (mode == 3)
        SetFilePointer(file->file, 0, NULL, FILE_END);
    *out = file;
    if (counting)
        files_opened++;
    fe_put(")=file%u", (uint32_t)(file - host_files));
    return 0;
}

static int32_t STDCALL host_open(struct host_file **file, const char *path,
                               int32_t extension, int32_t mode)
{
    return fe_open_file(file, path, extension, mode, 0);
}

static int32_t STDCALL host_open_fatal(struct host_file **file, const char *path,
                                     int32_t extension, int32_t mode)
{
    return fe_open_file(file, path, extension, mode, 1);
}

static int32_t fe_close_file(struct host_file *file, int fatal)
{
    int32_t result;

    fe_put(" close%s", fatal ? "!" : "");
    if (file == NULL || !file->open)
        return -1;
    result = CloseHandle(file->file) ? 0 : -1;
    file->file = INVALID_HANDLE_VALUE;
    file->open = 0;
    return result;
}

static int32_t STDCALL host_close(struct host_file *file)
{
    return fe_close_file(file, 0);
}

static int32_t STDCALL host_close_fatal(struct host_file *file)
{
    return fe_close_file(file, 1);
}

static int32_t fe_seek_file(struct host_file *file, uint32_t offset, int32_t whence,
                          int fatal)
{
    DWORD result;

    if (file == NULL || !file->open)
        return -1;
    if (GetFileSize(file->file, NULL) < offset)
        return -1;
    result = SetFilePointer(file->file, (LONG)offset, NULL, (DWORD)whence);
    fe_put(" seek%s(%#x,%d)", fatal ? "!" : "", offset, whence);
    return result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ? -1 : 0;
}

static int32_t STDCALL host_seek(struct host_file *file, uint32_t offset, int32_t whence)
{
    return fe_seek_file(file, offset, whence, 0);
}

static int32_t STDCALL host_seek_fatal(struct host_file *file, uint32_t offset, int32_t whence)
{
    return fe_seek_file(file, offset, whence, 1);
}

static int32_t fe_read_file(struct host_file *file, void *buffer, uint32_t size,
                            uint32_t *out_read, int fatal)
{
    DWORD bytes_read = 0;
    BOOL ok;

    if (out_read != NULL)
        *out_read = 0;
    if (file == NULL || !file->open)
        return -1;
    ok = ReadFile(file->file, buffer, size, &bytes_read, NULL);
    if (out_read != NULL)
        *out_read = bytes_read;
    fe_put(" read%s(%#x)=%lu", fatal ? "!" : "", size,
           (unsigned long)bytes_read);
    return ok ? 0 : -1;
}

static int32_t STDCALL host_read(struct host_file *file, void *buffer, uint32_t size,
                               uint32_t *read)
{
    return fe_read_file(file, buffer, size, read, 0);
}

static int32_t STDCALL host_read_fatal(struct host_file *file, void *buffer,
                                     uint32_t size, uint32_t *read)
{
    return fe_read_file(file, buffer, size, read, 1);
}

static int32_t STDCALL host_tell(struct host_file *file, uint32_t *out_position)
{
    DWORD position;

    if (file == NULL || !file->open)
        return -1;
    position = SetFilePointer(file->file, 0, NULL, FILE_CURRENT);
    if (position == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return -1;
    *out_position = position;
    fe_put(" tell=%#lx", (unsigned long)position);
    return 0;
}

static int32_t STDCALL host_size(struct host_file *file, uint32_t *size)
{
    if (file == NULL || !file->open)
        return -1;
    *size = GetFileSize(file->file, NULL);
    fe_put(" size=%#x", *size);
    return *size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ? -1 : 0;
}

static struct host_find *fe_new_host_find(void)
{
    int index;

    for (index = 0; index < MAX_HOST_FINDS; index++)
        if (!host_finds[index].open)
            return &host_finds[index];
    return NULL;
}

static int32_t STDCALL host_find_first(struct host_find **out, const char *dir,
                                       const char *pattern,
                                       int32_t extension_id)
{
    struct host_find *find = fe_new_host_find();
    const char *extension = fe_extension_text(extension_id);
    char path[MAX_PATH * 2];
    size_t length;

    if (find == NULL || (extension == NULL && extension_id != -1))
        return -1;
    path[0] = 0;
    if (dir != NULL) {
        strcpy(path, dir);
        length = strlen(path);
        if (length == 0 || path[length - 1] != '\\')
            strcat(path, "\\");
    }
    strcat(path, pattern != NULL ? pattern : "*");
    if (extension_id == -1)
        strcat(path, ".*");
    else
        strcat(path, extension);
    find->find = FindFirstFileA(path, &find->data);
    fe_put(" findfirst(");
    fe_put_string(path);
    if (find->find == INVALID_HANDLE_VALUE) {
        fe_put(")=-1");
        return -1;
    }
    find->open = 1;
    find->more = 1;
    *out = find;
    fe_put(")=find%u", (uint32_t)(find - host_finds));
    return 0;
}

static int32_t STDCALL host_find_next(struct host_find *find,
                                    struct host_find_entry *entry)
{
    for (;;) {
        int got = 0;
        char extension[0x100];

        if (find == NULL || !find->open || !find->more)
            return -1;
        if (find->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(find->data.cFileName, ".") != 0 &&
                strcmp(find->data.cFileName, "..") != 0) {
                strcpy(entry->name, find->data.cFileName);
                entry->type = -2;
                entry->size = 0;
                got = 1;
            }
        } else {
            _splitpath(find->data.cFileName, NULL, NULL, entry->name,
                       extension);
            entry->type = fe_extension_id(extension);
            entry->size = find->data.nFileSizeLow;
            entry->writable =
                (find->data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : 1;
            got = entry->type >= 0;
        }
        find->more = FindNextFileA(find->find, &find->data) ? 1 : 0;
        if (got) {
            fe_put(" findnext('");
            fe_put("%.80s',%d)", entry->name, entry->type);
            return 0;
        }
    }
}

static int32_t STDCALL host_find_close(struct host_find **find_ref)
{
    struct host_find *find = find_ref != NULL ? *find_ref : NULL;

    if (find == NULL || !find->open)
        return -1;
    FindClose(find->find);
    find->find = INVALID_HANDLE_VALUE;
    find->open = 0;
    *find_ref = NULL;
    fe_put(" findclose");
    return 0;
}

struct host_list {
    uint32_t count;
    uint32_t capacity;
    uint32_t grow;
    uint32_t elem_size;
    uint32_t iterating;
    uint32_t pending;
    uint8_t *data;
};

static void *STDCALL host_list_add(struct host_list *list)
{
    uint8_t *element;

    if (list->count == list->capacity) {
        uint32_t capacity = list->capacity + list->grow;
        uint32_t size = capacity * list->elem_size;
        uint8_t *data = (uint8_t *)fe_arena_get(size);

        if (data == NULL)
            ExitProcess(2);
        memset(data, 0xa5, size);
        if (list->data != NULL)
            memcpy(data, list->data, list->count * list->elem_size);
        list->data = data;
        list->capacity = capacity;
    }
    element = list->data + list->count * list->elem_size;
    memset(element, 0, list->elem_size);
    list->count++;
    fe_put(" listadd=");
    fe_put_pointer(fe_ptr(element));
    return element;
}

static void STDCALL host_list_remove(struct host_list *list, void *element)
{
    uint32_t index = (uint32_t)((uint8_t *)element - list->data) /
                     list->elem_size;

    fe_put(" listremove(%u)", index);
    if (index >= list->count)
        return;
    if (index + 1 < list->count)
        memmove(list->data + index * list->elem_size,
                list->data + (index + 1) * list->elem_size,
                (list->count - index - 1) * list->elem_size);
    list->count--;
}

static int32_t STDCALL host_list_free(void *block)
{
    uint8_t *list = (uint8_t *)block;
    void **data = (void **)(list + 0x18);

    if (*data != NULL)
        host_free(data);
    fe_put(" listfree");
    return 0;
}

static int32_t STDCALL chain_add(uint32_t list,
                               void (STDCALL *callback)(uint32_t arg0, uint32_t arg1))
{
    fe_put(" chain.add(%u,", list);
    if (current != NULL && (uint8_t *)callback >= current->module_lo &&
        (uint8_t *)callback < current->module_hi)
        fe_put("fe.frame");
    else
        fe_put_pointer(fe_ptr(callback));
    fe_put(")");
    if (list == 3)
        frame = callback;
    return 0;
}

static int32_t STDCALL chain_remove(uint32_t list,
                                  void (STDCALL *callback)(uint32_t arg0, uint32_t arg1))
{
    (void)callback;
    fe_put(" chain.remove(%u)", list);
    if (list == 3)
        frame = NULL;
    return 0;
}

static uint32_t STDCALL booster_add(uint32_t area, uint32_t index)
{
    fe_put(" booster.add(");
    fe_put_pointer(area);
    fe_put(",%u)", index);
    return 0;
}

static uint32_t STDCALL booster_drop(uint32_t area)
{
    fe_put(" booster.drop(");
    fe_put_pointer(area);
    fe_put(")");
    return 0;
}

static uint32_t STDCALL facil_index(void)
{
    fe_put(" facil.index");
    return 0;
}

#define SET_SLOT(table, offset, value) \
    (*(void **)((table) + (offset)) = (void *)(value))

static void fe_build_stubs(void)
{
    memset(host, 0, sizeof(host));
    memset(booster, 0, sizeof(booster));
    memset(chain, 0, sizeof(chain));
    memset(facil, 0, sizeof(facil));
    memset(g3d, 0, sizeof(g3d));
    memset(numeric, 0, sizeof(numeric));
    *(uint32_t *)host = 1;
    *(uint32_t *)booster = 34;
    *(uint32_t *)chain = 4;
    *(uint32_t *)facil = 37;
    *(uint32_t *)g3d = 14;
    *(uint32_t *)numeric = 17;

    SET_SLOT(host, 0x10c, host_dialog);
    SET_SLOT(host, 0x130, host_basedir);
    SET_SLOT(host, 0x13c, host_close);
    SET_SLOT(host, 0x140, host_close_fatal);
    SET_SLOT(host, 0x15c, host_find_close);
    SET_SLOT(host, 0x160, host_find_next);
    SET_SLOT(host, 0x164, host_find_first);
    SET_SLOT(host, 0x17c, host_open);
    SET_SLOT(host, 0x180, host_open_fatal);
    SET_SLOT(host, 0x184, host_join);
    SET_SLOT(host, 0x188, host_split);
    SET_SLOT(host, 0x190, host_tell);
    SET_SLOT(host, 0x194, host_seek);
    SET_SLOT(host, 0x198, host_seek_fatal);
    SET_SLOT(host, 0x19c, host_read);
    SET_SLOT(host, 0x1a0, host_read_fatal);
    SET_SLOT(host, 0x1a4, host_size);
    SET_SLOT(host, 0x20c, host_alloc);
    SET_SLOT(host, 0x210, host_realloc);
    SET_SLOT(host, 0x214, host_free);
    SET_SLOT(host, 0x218, host_memset);
    SET_SLOT(host, 0x21c, host_memmove);
    SET_SLOT(host, 0x228, host_memcmp);
    SET_SLOT(host, 0x2a0, host_loadstring);
    SET_SLOT(host, 0x2a8, host_strcpy);
    SET_SLOT(host, 0x2b8, host_strlen);
    SET_SLOT(host, 0x2bc, host_strcmp);
    SET_SLOT(host, 0x2c0, host_stricmp);
    SET_SLOT(host, 0x2c4, host_fit);
    SET_SLOT(host, 0x2f8, host_list_add);
    SET_SLOT(host, 0x2fc, host_list_free);
    SET_SLOT(host, 0x300, host_list_remove);

    SET_SLOT(booster, 0x18, booster_add);
    SET_SLOT(booster, 0x1c, booster_drop);
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    SET_SLOT(facil, 0x2c, facil_index);
}

static uint8_t *fe_load_support(const char *path, uint32_t id)
{
    HMODULE module = LoadLibraryA(path);
    struct fs98_import *import;
    uint8_t *link;

    if (module == NULL) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        exit(2);
    }
    link = (uint8_t *)GetProcAddress(module, "Linkage");
    import = (struct fs98_import *)GetProcAddress(module, "ImportTable");
    if (link == NULL || *(uint32_t *)link != id || import == NULL) {
        printf("%s: wrong support module\n", path);
        exit(2);
    }
    for (; import->id != 0; import++) {
        if (import->id == 1)
            import->link = host;
        else if (import->id == 2)
            import->link = global;
        else if (import->id == 17)
            import->link = numeric;
        else {
            printf("%s: unexpected support import %u\n", path, import->id);
            exit(2);
        }
    }
    return link;
}

static void fe_bind_imports(struct fs98_import *import, uint8_t *util)
{
    for (; import->id != 0; import++) {
        switch (import->id) {
        case 1:  import->link = host; break;
        case 2:  import->link = global; break;
        case 4:  import->link = chain; break;
        case 14: import->link = g3d; break;
        case 17: import->link = numeric; break;
        case 20: import->link = util; break;
        case 34: import->link = booster; break;
        case 37: import->link = facil; break;
        default:
            printf("FE: unexpected import %u\n", import->id);
            exit(2);
        }
    }
}

static int fe_load_side(struct fe_side *side, const char *path,
                        const char *name, uint8_t *util)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    struct fs98_import *import;

    memset(side, 0, sizeof(*side));
    side->name = name;
    side->module = LoadLibraryA(path);
    if (side->module == NULL) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        return 0;
    }
    side->link = (struct fe_linkage *)GetProcAddress(side->module, "Linkage");
    import = (struct fs98_import *)GetProcAddress(side->module, "ImportTable");
    if (side->link == NULL || import == NULL || side->link->id != 18) {
        printf("%s: not FE\n", path);
        return 0;
    }
    side->image = (uint8_t *)side->link - 0x48;
    if ((uint8_t *)import != side->image) {
        printf("%s: ImportTable is not Linkage-0x48\n", path);
        return 0;
    }
    dos = (IMAGE_DOS_HEADER *)side->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)side->module + dos->e_lfanew);
    side->module_lo = (uint8_t *)side->module;
    side->module_hi = side->module_lo + nt->OptionalHeader.SizeOfImage;
    fe_bind_imports(import, util);
    memcpy(side->pristine, side->image, FE_IMAGE_SIZE);
    return 1;
}

static void fe_put_position(uint8_t *position, const struct fe_scenario *scenario)
{
    memset(position, 0, 0x18);
    *(uint16_t *)(position + 2) = scenario->latitude_lo;
    *(uint32_t *)(position + 4) = scenario->latitude;
    *(uint16_t *)(position + 0xa) = scenario->longitude_lo;
    *(uint32_t *)(position + 0xc) = scenario->longitude;
    *(uint16_t *)(position + 0x12) = scenario->altitude_lo;
    *(uint32_t *)(position + 0x14) = scenario->altitude;
}

static void fe_make_global(const struct fe_scenario *scenario, const uint8_t *position)
{
    memset(global, 0, sizeof(global));
    /* Mode 2 is the normal full-world path.  Other values deliberately stop
     * read_areas after the first scenery area. */
    *(uint32_t *)(global + 0x390) = 2;
    *(uint16_t *)(global + 0x3e0) = scenario->flags;
    strcpy((char *)global + 0x446, "Scenery\\world.vis");
    *(uint16_t *)(global + 0x562) = *(const uint16_t *)(position + 2);
    *(uint32_t *)(global + 0x564) = *(const uint32_t *)(position + 4);
    *(uint16_t *)(global + 0x56a) = *(const uint16_t *)(position + 0xa);
    *(uint32_t *)(global + 0x56c) = *(const uint32_t *)(position + 0xc);
    *(uint16_t *)(global + 0x572) = *(const uint16_t *)(position + 0x12);
    *(uint32_t *)(global + 0x574) = *(const uint32_t *)(position + 0x14);
    *(uint32_t *)(global + 0xcb4) = 4;
}

static void *fe_slot(struct fe_side *side, uint32_t offset)
{
    return *(void **)((uint8_t *)side->link + offset);
}

static void fe_set_search(struct fe_side *side, uint8_t *position,
                          const struct fe_scenario *scenario)
{
    fe_fn3 search = (fe_fn3)fe_slot(side, 0x54);
    uint32_t result = search(fe_ptr(position), scenario->radius,
                                fe_ptr(global + 0x3e4));
    fe_put(" SEARCH=%#x/%08x", result,
           fe_digest(global + 0x3e4, 0x10));
}

static uint32_t fe_call4_guarded_regs(fe_fn4 call, uint32_t arg0, uint32_t arg1,
                                    uint32_t arg2, uint32_t arg3, uint32_t ebx,
                                    uint32_t esi, uint32_t edi)
{
    guard_call = (void *)call;
    guard_args[0] = arg0;
    guard_args[1] = arg1;
    guard_args[2] = arg2;
    guard_args[3] = arg3;
    guard_in_ebx = ebx;
    guard_in_esi = esi;
    guard_in_edi = edi;
    fe_guard_call4_asm();
    return guard_result;
}

static uint32_t fe_call4_guarded(fe_fn4 call, uint32_t arg0, uint32_t arg1,
                               uint32_t arg2, uint32_t arg3)
{
    return fe_call4_guarded_regs(call, arg0, arg1, arg2, arg3,
                                 0x11223344u, 0x55667788u, 0x99aabbccu);
}

static void fe_put_guard(void)
{
    if (guard_stack_delta != 0 || guard_ebp_delta != 0 ||
        guard_ebx != guard_in_ebx || guard_esi != guard_in_esi ||
        guard_edi != guard_in_edi) {
        fe_put(",abi=%d/%d/", guard_stack_delta, guard_ebp_delta);
        if (guard_ebx == guard_ebp)
            fe_put("ebp");
        else
            fe_put("%08x", guard_ebx);
        fe_put("/%08x/%08x", guard_esi, guard_edi);
    }
}

static void fe_run_scene(struct fe_side *side, uint8_t *position,
                         const struct fe_scenario *scenario)
{
    fe_fn1 level = (fe_fn1)fe_slot(side, 0x48);
    fe_set_search(side, position, scenario);
    fe_put(" LEVEL=%#x", level(scenario->level));
    scene_call = (fe_fn4)fe_slot(side, 0x18);
    reset_call = (fe_fn1)fe_slot(side, 0x44);
    for (service_step = 0; service_step < MAX_STEPS; service_step++) {
        uint32_t length = 0xccccccccu;
        uint32_t area = 0xddddddddu;
        uint32_t result;

        memset(output, 0x5a, sizeof(output));
        result = fe_call4_guarded(scene_call, fe_ptr(output),
                                    OUTPUT_SIZE, fe_ptr(&length),
                                    fe_ptr(&area));
        fe_put(" SCENE[%u]=%#x,len=%#x,area=", (uint32_t)service_step,
               result, length);
        fe_put_pointer(area);
        fe_put_guard();
        fe_put(",data=");
        if (result == 1 && length <= OUTPUT_SIZE)
            fe_put_blob(output, length);
        else
            fe_put("-");
        if (counting && result == 1)
            records++;
        if (result == 0 || result == 0xffffffffu)
            break;
    }
    fe_put(" RESET=%#x", reset_call(0));
}

static void fe_run_next(struct fe_side *side, uint8_t *position,
                        const struct fe_scenario *scenario)
{
    fe_fn3 next = (fe_fn3)fe_slot(side, 0x20);
    uint32_t step;

    fe_set_search(side, position, scenario);
    for (step = 0; step < 24; step++) {
        uint32_t data = 0;
        uint32_t length = 0;
        uint32_t result = next(step == 0 ? 0 : 1, fe_ptr(&data),
                                  fe_ptr(&length));
        fe_put(" NEXT[%u]=%#x,ptr=", step, result);
        fe_put_pointer(data);
        fe_put(",data=");
        if (result == 1 && data != 0 && length < 0x10000)
            fe_put_blob((const void *)(uintptr_t)data, length);
        else
            fe_put("-");
        if (counting && result == 1)
            records++;
        if (result == 0)
            break;
    }
    fe_put(" NEXTMODE=%#x/%#x", next(2, 0, 0), next(3, 0, 0));
}

static void fe_run_navaid(struct fe_side *side, uint8_t *position,
                          const struct fe_scenario *scenario)
{
    fe_fn3 vor = (fe_fn3)fe_slot(side, 0x24);
    fe_fn4 ils = (fe_fn4)fe_slot(side, 0x28);
    fe_fn3 beacon = (fe_fn3)fe_slot(side, 0x2c);
    uint32_t record = 0;
    uint32_t result;

    memset(output, 0x5a, sizeof(output));
    result = vor(scenario->frequency, fe_ptr(position), fe_ptr(&record));
    fe_put(" VOR=%#x,ptr=", result);
    fe_put_pointer(record);
    if (record != 0 && current != NULL && fe_in_image(current, record))
        fe_put(",data=%08x", fe_digest((const void *)(uintptr_t)record, 0x80));

    memset(output, 0x5a, sizeof(output));
    result = ils(scenario->frequency, scenario->channel,
                      fe_ptr(position), fe_ptr(output));
    fe_put(" ILS=%#x,data=", result);
    fe_put_blob(output, 0x2f);

    memset(output, 0x5a, sizeof(output));
    result = beacon(scenario->frequency, fe_ptr(position),
                         fe_ptr(output));
    fe_put(" BEACON=%#x,data=", result);
    fe_put_blob(output, 0x4c);
}

static void fe_run_ground(struct fe_side *side, uint8_t *position)
{
    fe_fn1 ground = (fe_fn1)fe_slot(side, 0x30);
    fe_put(" GROUND=%#x", ground(fe_ptr(position)));
}

static void fe_run_list(struct fe_side *side)
{
    fe_fn0 begin = (fe_fn0)fe_slot(side, 0x4c);
    fe_fn2 next = (fe_fn2)fe_slot(side, 0x34);
    fe_fn3 at = (fe_fn3)fe_slot(side, 0x50);
    fe_fn2 read = (fe_fn2)fe_slot(side, 0x38);
    uint32_t first_at = 0;
    uint32_t first_area = 0;
    uint32_t first_index = 0;
    uint32_t step;

    fe_put(" LISTBEGIN=%#x", begin());
    for (step = 0; step < 24; step++) {
        uint32_t data = 0;
        uint32_t length = 0;
        uint32_t result = next(fe_ptr(&data), fe_ptr(&length));
        fe_put(" LISTNEXT[%u]=%#x,ptr=", step, result);
        fe_put_pointer(data);
        fe_put(",data=");
        if (result == 1 && data != 0 && length < 0x10000) {
            fe_put_blob((const void *)(uintptr_t)data, length + 12);
            if (step == 0) {
                first_area = *(uint32_t *)(uintptr_t)(data + 0);
                first_index = *(uint32_t *)(uintptr_t)(data + 4);
                first_at = *(uint32_t *)(uintptr_t)(data + 8);
            }
        } else {
            fe_put("-");
        }
        if (counting && result == 1)
            records++;
        if (result == 0)
            break;
    }
    if (first_at != 0) {
        fe_put(" LISTAT=%#x", at(first_at, first_area, first_index));
        for (step = 0; step < 8; step++) {
            uint32_t data = 0;
            uint32_t length = 0;
            uint32_t result = read(fe_ptr(&data), fe_ptr(&length));
            fe_put(" LISTREAD[%u]=%#x,ptr=", step, result);
            fe_put_pointer(data);
            fe_put(",data=");
            if (result == 1 && data != 0 && length < 0x10000)
                fe_put_blob((const void *)(uintptr_t)data, length);
            else
                fe_put("-");
            if (result == 0)
                break;
        }
    }
}

static void fe_run_dynamic(struct fe_side *side, uint8_t *position,
                           const struct fe_scenario *scenario)
{
    fe_fn4 dynamic = (fe_fn4)fe_slot(side, 0x40);
    uint32_t needed = 0xccccccccu;
    uint32_t result;
    uint32_t step;

    fe_set_search(side, position, scenario);
    memset(output, 0x5a, sizeof(output));
    result = fe_call4_guarded_regs(dynamic, 0, fe_ptr(output), 1,
                                     fe_ptr(&needed), 0x11223344u,
                                     0x55667788u, fe_ptr(output));
    fe_put(" DYNAMIC_PROBE=%#x,need=%#x", result, needed);
    fe_put_guard();
    fe_put(",data=");
    if (result == 0xffffffffu && needed <= OUTPUT_SIZE)
        fe_put_blob(output, needed);
    else
        fe_put("-");
    if (counting && result == 0xffffffffu)
        dynamic_oversize++;
    if (result == 0)
        return;

    for (step = 0; step < 15; step++) {
        needed = 0;

        memset(output, 0x5a, sizeof(output));
        result = fe_call4_guarded(dynamic, 1, fe_ptr(output),
                                    OUTPUT_SIZE, fe_ptr(&needed));
        fe_put(" DYNAMIC[%u]=%#x,need=%#x", step, result, needed);
        fe_put_guard();
        fe_put(",data=");
        if (result == 1)
            fe_put_blob(output, 0x100);
        else
            fe_put("-");
        if (counting && result == 1)
            records++;
        if (result == 0 || result == 0xffffffffu)
            break;
    }
}

static void fe_run_object(struct fe_side *side, const struct fe_scenario *scenario)
{
    fe_fn5 object = (fe_fn5)fe_slot(side, 0x3c);
    uint32_t id[4];
    uint32_t got = 0;
    uint32_t result;

    id[0] = scenario->latitude;
    id[1] = scenario->longitude;
    id[2] = scenario->number;
    id[3] = scenario->number ^ 0x87654321u;
    memset(output, 0x5a, sizeof(output));
    result = object(fe_ptr(id), fe_ptr(output), 0,
                         OUTPUT_SIZE, fe_ptr(&got));
    fe_put(" OBJECT=%#x,got=%#x,data=", result, got);
    if (result == 1 && got <= OUTPUT_SIZE)
        fe_put_blob(output, got);
    else
        fe_put("-");
}

static void fe_run_box(struct fe_side *side, uint8_t *position,
                       const struct fe_scenario *scenario)
{
    fe_fn2 overlap = (fe_fn2)fe_slot(side, 0x58);
    fe_fn2 in_box = (fe_fn2)fe_slot(side, 0x5c);
    uint32_t box[4];
    uint32_t other[4];
    uint32_t point[2];

    fe_set_search(side, position, scenario);
    memcpy(box, global + 0x3e4, sizeof(box));
    memcpy(other, box, sizeof(other));
    other[0] += scenario->number & 1 ? scenario->radius : 0;
    other[3] -= scenario->number & 2 ? scenario->radius * 64 : 0;
    point[0] = scenario->latitude;
    point[1] = scenario->longitude;
    fe_put(" OVERLAP=%#x INBOX=%#x", overlap(fe_ptr(box), fe_ptr(other)),
           in_box(fe_ptr(box), fe_ptr(point)));
}

static void fe_run_register(struct fe_side *side)
{
    fe_fn0 begin = (fe_fn0)fe_slot(side, 0x4c);
    fe_fn2 add = (fe_fn2)fe_slot(side, 0x70);
    fe_fn2 remove = (fe_fn2)fe_slot(side, 0x74);
    uint32_t areas;
    uint32_t area;
    uint32_t files;
    char name[0x104];

    fe_put(" REG_BEGIN=%#x", begin());
    areas = *(uint32_t *)(side->image + 0x2d1);
    if (areas == 0)
        return;
    area = areas;
    files = *(uint32_t *)(uintptr_t)area;
    if (files == 0 || *(uint32_t *)(uintptr_t)(area + 8) == 0)
        return;
    strncpy(name, (const char *)(uintptr_t)files, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    fe_put(" UNREG('%s')=%#x", name,
           remove(0, fe_ptr(name)));
    fe_put(" REG('%s')=%#x", name,
           add(0, fe_ptr(name)));
}

static void fe_run_texture(struct fe_side *side)
{
    fe_fn4 load = (fe_fn4)fe_slot(side, 0x1c);
    uint32_t result;

    memset(output, 0x5a, sizeof(output));
    result = load(fe_ptr(output), fe_ptr("A S P H A L T . R 8"),
                  fe_ptr(fixture), 0);
    fe_put(" TEX_RAW=%#x,data=", result);
    fe_put_blob(output, 0x10000);
    if (counting) {
        if (result == 1)
            texture_loads++;
        else
            part3_fixture_failures++;
    }

    memset(output, 0x5a, sizeof(output));
    result = load(fe_ptr(output), fe_ptr("fs5.pal"), fe_ptr(part3_root), 9);
    fe_put(" TEX_PAL=%#x,data=", result);
    fe_put_blob(output, 0xc0);
    if (counting) {
        if (result == 1)
            texture_loads++;
        else
            part3_fixture_failures++;
    }

    memset(output, 0x5a, sizeof(output));
    result = load(fe_ptr(output), fe_ptr("fediff-packed.bin"),
                  fe_ptr(part3_root), 0);
    fe_put(" TEX_PACKED=%#x,data=", result);
    fe_put_blob(output, 6);
    if (counting) {
        if (result == 1 && memcmp(output, "ABCZZZ", 6) == 0)
            texture_loads++;
        else
            part3_fixture_failures++;
    }

    memset(output, 0x5a, sizeof(output));
    *(uint16_t *)(global + 0xcd2) = 0x7b7b;
    result = load(fe_ptr(output), fe_ptr("fediff-missing"),
                  fe_ptr(part3_root), 10);
    fe_put(" TEX_HAZ=%#x,flag=%#x,data=", result,
           *(uint16_t *)(global + 0xcd2));
    fe_put_blob(output, 0x1000);
    if (counting) {
        if (result == 1 && *(uint16_t *)(global + 0xcd2) == 0)
            texture_loads++;
        else
            part3_fixture_failures++;
    }

    result = load(fe_ptr(output), fe_ptr("fediff-missing"),
                  fe_ptr(part3_root), 0);
    fe_put(" TEX_MISSING=%#x", result);
    if (counting && result != 0)
        part3_fixture_failures++;
}

static void fe_setup_facil_state(struct fe_side *side)
{
    static const uint8_t id[16] = {
        'F', 'E', '-', 'D', 'I', 'F', 'F', '-', 'P', 'A', 'R', 'T', '3', 0, 0, 0
    };
    uint8_t *area = (uint8_t *)fe_arena_get(0x10);
    uint8_t *record = (uint8_t *)fe_arena_get(0x1ae);
    size_t path_size = strlen(part3_root) + 1;
    char *path = (char *)fe_arena_get((uint32_t)path_size);

    if (area == NULL || record == NULL || path == NULL)
        ExitProcess(2);
    memset(area, 0, 0x10);
    memset(record, 0, 0x1ae);
    memcpy(path, part3_root, path_size);
    strcpy((char *)record, "fediff_part3");
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

static void fe_run_facil(struct fe_side *side, const struct fe_scenario *scenario)
{
    fe_fn2 list = (fe_fn2)fe_slot(side, 0x7c);
    fe_fn2 open = (fe_fn2)fe_slot(side, 0x80);
    fe_fn5 read = (fe_fn5)fe_slot(side, 0x84);
    fe_fn5 close = (fe_fn5)fe_slot(side, 0x88);
    uint32_t ids = 0;
    uint32_t count = 0;
    uint32_t data = 0;
    uint32_t size = 0;
    uint8_t missing_id[16];
    uint32_t result;

    result = list(fe_ptr(&ids), fe_ptr(&count));
    fe_put(" FACIL_EMPTY=%#x,count=%u,ptr=", result, count);
    fe_put_pointer(ids);
    if (counting && (result != 1 || count != 0 || ids != 0))
        part3_fixture_failures++;

    fe_setup_facil_state(side);
    *(uint16_t *)(global + 0x1288) =
        scenario->number & 1 ? 0x456 : 0x999;

    result = list(fe_ptr(&ids), fe_ptr(&count));
    fe_put(" FACIL_LIST=%#x,count=%u,data=", result, count);
    fe_put_blob((const void *)(uintptr_t)ids, count * 0x10);
    if (counting && (result != 1 || ids == 0 || count != 1))
        part3_fixture_failures++;
    if (ids == 0 || count == 0)
        return;

    result = open(ids, fe_ptr(&data));
    fe_put(" FACIL_OPEN=%#x,ptr=", result);
    fe_put_pointer(data);
    fe_put(",data=");
    if (data != 0)
        fe_put_blob((const void *)(uintptr_t)data,
                    *(uint32_t *)(uintptr_t)(data + 4) + 8);
    else
        fe_put("-");
    if (counting &&
        (result != 1 || data == 0 ||
         *(uint16_t *)(uintptr_t)(data + 8) !=
             (*(uint16_t *)(global + 0x1288) == 0x456 ? 0x456 : 0x409)))
        part3_fixture_failures++;
    if (data != 0)
        host_free((void **)&data);

    result = read(ids, 0x200, 0x10003, fe_ptr(&data), fe_ptr(&size));
    fe_put(" FACIL_READ=%#x,size=%#x,data=", result, size);
    fe_put_blob((const void *)(uintptr_t)data, size);
    if (counting) {
        if (result == 1 && data != 0 && size == 0x15)
            facil_reads++;
        else
            part3_fixture_failures++;
    }
    if (data != 0)
        host_free((void **)&data);

    result = read(ids, 0x200, 0x20001, fe_ptr(&data), fe_ptr(&size));
    fe_put(" FACIL_ABSENT=%#x,size=%#x,ptr=", result, size);
    fe_put_pointer(data);
    if (counting && (result != 1 || size != 0 || data != 0))
        part3_fixture_failures++;

    result = read(ids, 0x380, 0x10003, fe_ptr(&data), fe_ptr(&size));
    fe_put(" FACIL_LEGACY=%#x,size=", result);
    fe_put_pointer(size);
    fe_put(",ptr=");
    fe_put_pointer(data);
    if (counting &&
        (result != 1 || data != 0 ||
         (uint8_t *)(uintptr_t)size < arena ||
         (uint8_t *)(uintptr_t)size >= arena + ARENA_SIZE))
        part3_fixture_failures++;

    result = close(ids, 0x300, 0x20001, fe_ptr(&data), fe_ptr(&size));
    fe_put(" FACIL_CLOSE=%#x,size=%#x,data=", result, size);
    fe_put_blob((const void *)(uintptr_t)data, size);
    if (counting) {
        if (result == 0 && data != 0 && size == 0x15)
            facil_reads++;
        else
            part3_fixture_failures++;
    }
    if (data != 0)
        host_free((void **)&data);

    memset(missing_id, 0xcc, sizeof(missing_id));
    result = read(fe_ptr(missing_id), 0x200, 0x10003,
                  fe_ptr(&data), fe_ptr(&size));
    fe_put(" FACIL_MISSING=%#x,size=%#x,ptr=", result, size);
    fe_put_pointer(data);
    if (counting && (result != 0 || size != 0 || data != 0))
        part3_fixture_failures++;

    result = open(fe_ptr(missing_id), fe_ptr(&data));
    fe_put(" FACIL_OPEN_MISSING=%#x,ptr=", result);
    fe_put_pointer(data);
    if (counting && (result != 1 || data != 0))
        part3_fixture_failures++;
    host_free((void **)&ids);
}

static void fe_run_operation(struct fe_side *side, const struct fe_scenario *scenario,
                             uint8_t *position)
{
    switch (scenario->operation) {
    case FE_OP_SCENE:    fe_run_scene(side, position, scenario); break;
    case FE_OP_NEXT:     fe_run_next(side, position, scenario); break;
    case FE_OP_NAVAID:   fe_run_navaid(side, position, scenario); break;
    case FE_OP_GROUND:   fe_run_ground(side, position); break;
    case FE_OP_LIST:     fe_run_list(side); break;
    case FE_OP_DYNAMIC:  fe_run_dynamic(side, position, scenario); break;
    case FE_OP_OBJECT:   fe_run_object(side, scenario); break;
    case FE_OP_BOX:      fe_run_box(side, position, scenario); break;
    case FE_OP_REGISTER: fe_run_register(side); break;
    case FE_OP_TEXTURE:  fe_run_texture(side); break;
    case FE_OP_FACIL:    fe_run_facil(side, scenario); break;
    }
}

static void fe_make_scenario(struct fe_scenario *scenario, uint32_t number)
{
    static const uint32_t radius[] = { 5000, 100000, 312368, 25000, 750000 };
    static const uint16_t frequency[] = {
        0x1390, 0x1050, 0x1130, 0x0950, 0x1170, 0x0830, 0x1080, 0x1260
    };
    int32_t lat_delta = (int32_t)(fe_rnd() % 400001) - 200000;
    int32_t lon_delta = (int32_t)(fe_rnd() % 400001) - 200000;

    memset(scenario, 0, sizeof(*scenario));
    scenario->number = number;
    scenario->operation = number % FE_OP_COUNT;
    scenario->latitude = 4652043u + (uint32_t)lat_delta;
    scenario->longitude = 0xc1b33f34u + (uint32_t)(lon_delta * 64);
    scenario->altitude = fe_rnd() % 3000;
    scenario->radius = radius[number %
        (sizeof(radius) / sizeof(radius[0]))];
    scenario->level = 8 + number % 10;
    scenario->latitude_lo = (uint16_t)fe_rnd();
    scenario->longitude_lo = (uint16_t)fe_rnd();
    scenario->altitude_lo = (uint16_t)fe_rnd();
    scenario->flags = (uint16_t)((number % 11 == 0 ? 1 : 0) |
                                  (number % 17 == 0 ? 4 : 0) |
                                  (number % 23 == 0 ? 8 : 0));
    scenario->frequency = frequency[number %
        (sizeof(frequency) / sizeof(frequency[0]))];
    scenario->channel = (uint8_t)(number % 40);
    if (scenario->operation == FE_OP_DYNAMIC) {
        scenario->latitude = 4652043u;
        scenario->longitude = 0xc1b33f34u;
        scenario->radius = 750000u;
    }
}

static void fe_prepare_run(struct fe_side *side, const struct fe_scenario *scenario,
                           uint8_t *position)
{
    current = side;
    current_scenario = scenario->number;
    current_operation = scenario->operation;
    transcript_len = 0;
    transcript[0] = 0;
    arena_used = 0;
    memset(arena, 0xa5, sizeof(arena));
    memset(output, 0x5a, sizeof(output));
    fe_reset_handles();
    frame = NULL;
    memcpy(side->image, side->pristine, FE_IMAGE_SIZE);
    fe_put_position(position, scenario);
    fe_make_global(scenario, position);
}

static void fe_run_side(struct fe_side *side, const struct fe_scenario *scenario,
                        uint8_t *image_out, uint8_t *global_out,
                        uint8_t *arena_out, uint32_t *arena_used_out)
{
    uint8_t position[0x18];

    fe_prepare_run(side, scenario, position);
    fe_put("scenario=%u op=%u", scenario->number, scenario->operation);
    side->link->init();
    fe_run_operation(side, scenario, position);
    if (frame != NULL) {
        fe_put(" FRAME");
        frame(0x11223344u, 0x55667788u);
    }
    memcpy(image_out, side->image, FE_IMAGE_SIZE);
    memcpy(global_out, global, GLOBAL_SIZE);
    memcpy(arena_out, arena, arena_used);
    *arena_used_out = arena_used;
    side->link->deinit();
    fe_reset_handles();
}

static int fe_skip_image_byte(uint32_t offset)
{
    uint32_t index;

    if (offset >= FE_CS_START && offset < FE_CS_END)
        return 1;
    for (index = 0; index < sizeof(text_relocs) / sizeof(text_relocs[0]);
         index++)
        if (offset >= text_relocs[index] &&
            offset < (uint32_t)text_relocs[index] + 4)
            return 1;
    return 0;
}

static int fe_same_image_pointer(uint32_t retail_value, uint32_t ours_value)
{
    if (!fe_in_image(&retail, retail_value) ||
        !fe_in_image(&ours, ours_value))
        return 0;
    return (uint8_t *)(uintptr_t)retail_value - retail.image ==
           (uint8_t *)(uintptr_t)ours_value - ours.image;
}

static int32_t fe_compare_image(const uint8_t *retail, const uint8_t *ours)
{
    uint32_t offset = FE_DATA_START;

    while (offset < FE_IMAGE_SIZE) {
        if (fe_skip_image_byte(offset)) {
            offset++;
            continue;
        }
        if (offset + 4 <= FE_IMAGE_SIZE &&
            !fe_skip_image_byte(offset + 1) &&
            !fe_skip_image_byte(offset + 2) &&
            !fe_skip_image_byte(offset + 3) &&
            fe_same_image_pointer(*(const uint32_t *)(retail + offset),
                                  *(const uint32_t *)(ours + offset))) {
            offset += 4;
            continue;
        }
        if (retail[offset] == ours[offset]) {
            offset++;
            continue;
        }
        return (int32_t)offset;
    }
    return -1;
}

static void fe_dump_difference(const char *what, const uint8_t *retail,
                               const uint8_t *ours, uint32_t size)
{
    uint32_t offset;
    uint32_t shown = 0;

    for (offset = 0; offset < size && shown < 12; offset++) {
        if (retail[offset] != ours[offset]) {
            printf("  %s+%#x: retail %02x ours %02x\n", what, offset,
                   retail[offset], ours[offset]);
            shown++;
        }
    }
}

static void fe_dump_transcript_difference(const char *retail, const char *ours)
{
    uint32_t offset = 0;
    uint32_t start;

    while (retail[offset] == ours[offset] && retail[offset] != 0)
        offset++;
    start = offset > 240 ? offset - 240 : 0;
    printf("  transcript+%#x differs\n", offset);
    printf("retail: %.1000s\nours:   %.1000s\n",
           retail + start, ours + start);
}

static LONG WINAPI fe_unhandled(EXCEPTION_POINTERS *exception)
{
    printf("FAULT %#lx at %p in %s scenario %u op %u step %u\n",
           (unsigned long)exception->ExceptionRecord->ExceptionCode,
           exception->ExceptionRecord->ExceptionAddress,
           current != NULL ? current->name : "setup",
           current_scenario, current_operation, (uint32_t)service_step);
    fflush(stdout);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int fe_validate_fixture(const char *root)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\Scenery\\world.vis", root);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        printf("fixture has no %s\n", path);
        return 0;
    }
    snprintf(path, sizeof(path), "%s\\Texture\\fs5.pal", root);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        printf("fixture has no %s\n", path);
        return 0;
    }
    return 1;
}

static int fe_write_fixture_file(const char *path, const void *data,
                                 uint32_t size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok;

    if (file == INVALID_HANDLE_VALUE)
        return 0;
    ok = WriteFile(file, data, size, &written, NULL);
    CloseHandle(file);
    return ok && written == size;
}

static uint32_t fe_pack_codes(uint8_t *out, const uint16_t *codes,
                              uint32_t count)
{
    uint32_t accumulator = 0;
    uint32_t bits = 0;
    uint32_t used = 0;
    uint32_t index;

    for (index = 0; index < count; index++) {
        accumulator = (accumulator << 9) | codes[index];
        bits += 9;
        while (bits >= 8) {
            bits -= 8;
            out[used++] = (uint8_t)(accumulator >> bits);
            if (bits == 0)
                accumulator = 0;
            else
                accumulator &= (1u << bits) - 1;
        }
    }
    if (bits != 0)
        out[used++] = (uint8_t)(accumulator << (8 - bits));
    return used;
}

static int fe_make_part3_fixture(void)
{
    static const uint16_t codes[] = {
        0x83, 'A', 'B', 'C', 0x03, 'Z', 0x1ff
    };
    uint8_t packed[32];
    uint8_t bgl[0x400];
    char temp[MAX_PATH * 2];
    char path[MAX_PATH * 2];
    uint32_t offset;
    uint32_t packed_size;

    if (GetTempPathA(sizeof(temp), temp) == 0)
        return 0;
    snprintf(part3_root, sizeof(part3_root), "%sOpenFS98-fediff-%lu", temp,
             (unsigned long)GetCurrentProcessId());
    CreateDirectoryA(part3_root, NULL);
    snprintf(path, sizeof(path), "%s\\Scenery", part3_root);
    CreateDirectoryA(path, NULL);

    packed_size = fe_pack_codes(packed, codes,
                                sizeof(codes) / sizeof(codes[0]));
    snprintf(path, sizeof(path), "%s\\fediff-packed.bin", part3_root);
    if (!fe_write_fixture_file(path, packed, packed_size))
        return 0;

    memset(bgl, 0, sizeof(bgl));
    offset = 0x100;
    *(uint32_t *)(bgl + offset) = 0x30001;
    *(uint32_t *)(bgl + offset + 4) = 0x10;
    *(uint16_t *)(bgl + offset + 8) = 0x409;
    *(uint32_t *)(bgl + offset + 12) = 0x11223344;
    offset += 0x18;
    *(uint32_t *)(bgl + offset) = 0x30001;
    *(uint32_t *)(bgl + offset + 4) = 0x10;
    *(uint16_t *)(bgl + offset + 8) = 0x456;
    *(uint32_t *)(bgl + offset + 12) = 0x55667788;

    *(uint32_t *)(bgl + 0x200) = 1;
    offset = 0x210;
    *(uint32_t *)(bgl + offset) = 0x10003;
    *(uint32_t *)(bgl + offset + 4) = 3;
    memcpy(bgl + offset + 8, "abc", 3);
    offset += 11;
    *(uint32_t *)(bgl + offset) = 0x10004;
    *(uint32_t *)(bgl + offset + 4) = 4;
    memcpy(bgl + offset + 8, "DEFG", 4);
    offset += 12;
    *(uint32_t *)(bgl + offset) = 0x10003;
    *(uint32_t *)(bgl + offset + 4) = 2;
    memcpy(bgl + offset + 8, "hi", 2);

    *(uint32_t *)(bgl + 0x300) = 1;
    *(uint32_t *)(bgl + 0x304) = 5;
    *(uint32_t *)(bgl + 0x308) = 0x20001;
    *(uint32_t *)(bgl + 0x30c) = 0x89abcdef;
    memcpy(bgl + 0x310, "close", 5);
    *(uint32_t *)(bgl + 0x380) = 2;

    snprintf(path, sizeof(path), "%s\\Scenery\\fediff_part3.BGL",
             part3_root);
    return fe_write_fixture_file(path, bgl, sizeof(bgl));
}

static void fe_remove_part3_fixture(void)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\fediff-packed.bin", part3_root);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%s\\Scenery\\fediff_part3.BGL",
             part3_root);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%s\\Scenery", part3_root);
    RemoveDirectoryA(path);
    RemoveDirectoryA(part3_root);
}

int main(int argc, char **argv)
{
    static struct fe_scenario scenario_data;
    static uint8_t retail_image[FE_IMAGE_SIZE];
    static uint8_t ours_image[FE_IMAGE_SIZE];
    static uint8_t retail_global[GLOBAL_SIZE];
    static uint8_t ours_global[GLOBAL_SIZE];
    static char retail_transcript[TRANSCRIPT_SIZE];
    uint8_t *retail_arena;
    uint8_t *ours_arena;
    uint8_t *util;
    uint32_t scenarios;
    uint32_t scenario;
    uint32_t bad = 0;
    uint32_t retail_arena_used;
    uint32_t ours_arena_used;
    int32_t image_difference;

    SetUnhandledExceptionFilter(fe_unhandled);
    scenarios = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 100;
    if (argc < 3) {
        printf("usage: fediff.exe [scenarios] fixture-root\n");
        return 2;
    }
    if (GetFullPathNameA(argv[2], sizeof(fixture), fixture, NULL) == 0 ||
        GetCurrentDirectoryA(sizeof(original_dir), original_dir) == 0) {
        printf("cannot resolve fixture root\n");
        return 2;
    }
    if (!fe_validate_fixture(fixture))
        return 2;
    if (!fe_make_part3_fixture()) {
        printf("cannot create part 3 fixture\n");
        return 2;
    }

    retail_arena = (uint8_t *)malloc(ARENA_SIZE);
    ours_arena = (uint8_t *)malloc(ARENA_SIZE);
    if (retail_arena == NULL || ours_arena == NULL) {
        printf("cannot allocate comparison arenas\n");
        return 2;
    }

    fe_build_stubs();
    util = fe_load_support("UTIL_retail.dll", 20);
    if (!fe_load_side(&retail, "FE_retail.dll", "retail", util) ||
        !fe_load_side(&ours, "FE_ours.dll", "openfs98", util))
        return 2;
    if (!SetCurrentDirectoryA(fixture)) {
        printf("cannot enter fixture root\n");
        return 2;
    }

    image_difference = fe_compare_image(retail.pristine, ours.pristine);
    if (image_difference >= 0) {
        printf("initial FE images differ at +%#x\n", (uint32_t)image_difference);
        return 2;
    }

    for (scenario = 0; scenario < scenarios; scenario++) {
        fe_make_scenario(&scenario_data, scenario);
        counting = 1;
        fe_run_side(&retail, &scenario_data, retail_image, retail_global,
                    retail_arena, &retail_arena_used);
        strncpy(retail_transcript, transcript, sizeof(retail_transcript) - 1);
        retail_transcript[sizeof(retail_transcript) - 1] = 0;

        counting = 0;
        fe_run_side(&ours, &scenario_data, ours_image, ours_global,
                    ours_arena, &ours_arena_used);
        image_difference = fe_compare_image(retail_image, ours_image);
        if (strcmp(retail_transcript, transcript) != 0 ||
            image_difference >= 0 ||
            memcmp(retail_global, ours_global, GLOBAL_SIZE) != 0 ||
            retail_arena_used != ours_arena_used ||
            (retail_arena_used != 0 &&
             memcmp(retail_arena, ours_arena, retail_arena_used) != 0)) {
            uint32_t offset;
            printf("scenario %u op %u differs\n", scenario,
                   scenario_data.operation);
            if (strcmp(retail_transcript, transcript) != 0)
                fe_dump_transcript_difference(retail_transcript, transcript);
            if (image_difference >= 0)
                printf("  image+%#x: retail %02x ours %02x\n",
                       (uint32_t)image_difference,
                       retail_image[image_difference], ours_image[image_difference]);
            if (memcmp(retail_global, ours_global, GLOBAL_SIZE) != 0)
                fe_dump_difference("GLOBAL", retail_global, ours_global,
                                   GLOBAL_SIZE);
            if (retail_arena_used != ours_arena_used)
                printf("  arena used: retail %#x ours %#x\n",
                       retail_arena_used, ours_arena_used);
            else if (retail_arena_used != 0 &&
                     memcmp(retail_arena, ours_arena, retail_arena_used) != 0) {
                for (offset = 0; offset < retail_arena_used; offset++)
                    if (retail_arena[offset] != ours_arena[offset]) {
                        printf("  arena+%#x: retail %02x ours %02x\n", offset,
                               retail_arena[offset], ours_arena[offset]);
                        break;
                    }
            }
            bad++;
            if (getenv("FESTOP") != NULL)
                break;
        }
        coverage[scenario_data.operation]++;
        if ((scenario + 1) % 25 == 0)
            printf("%u/%u\r", scenario + 1, scenarios);
    }

    printf("\noperations:");
    for (scenario = 0; scenario < FE_OP_COUNT; scenario++)
        printf(" %u", coverage[scenario]);
    printf("\nfiles opened %u, records returned %u, dynamic oversized probes %u\n",
           files_opened, records, dynamic_oversize);
    printf("texture loads %u, FACIL section reads %u\n",
           texture_loads, facil_reads);
    if (part3_fixture_failures != 0) {
        printf("part 3 fixture checks failed %u times\n",
               part3_fixture_failures);
        bad++;
    }
    if (coverage[FE_OP_DYNAMIC] != 0 && dynamic_oversize == 0) {
        printf("dynamic oversized path was not covered\n");
        bad++;
    }
    printf("%u scenarios, %u differ\n", scenarios, bad);
    if (bad == 0)
        printf("IDENTICAL\n");
    free(retail_arena);
    free(ours_arena);
    SetCurrentDirectoryA(original_dir);
    fe_remove_part3_fixture();
    return bad != 0;
}
