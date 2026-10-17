/* Differential test for FE Linkage +0x78, the Scenery Library.
 *
 * Like FACIL's dialogs, +0x78 is not window code: FE fills parameter blocks
 * in its .data and runs English\World.DLL's dialogs through host +0x10c, and
 * World calls back through function pointers in the blocks.  Here host
 * +0x10c is a stand-in for World that runs a seeded script against each
 * block -- typing paths, titles, layers and positions, moving the selection,
 * pressing the buttons (the callbacks) and finally OK or Cancel -- and logs
 * the block and its rows after every callback.  The alerts go through
 * retail UTIL to host dialog 2, which answers from the same script.
 *
 * The Library edits world files, so each side runs against a fresh copy of
 * a writable fixture: the real world.vis and synthetic ones carrying the
 * booster caches, protected areas and layer ties the CD's never does.  Drive
 * letters are sandboxed: W: holds the game directory (the host's base
 * directory is W:\FS98\, as FLTSIM98 makes it from its module path), M: a
 * synthetic tree of add-on folders, cache directories and BSTINFO files, and
 * K: the real CD's Boosters directory when it is there.  Every other letter
 * maps to a directory that does not exist, so nothing reaches a real drive.
 * Directory listings are sorted, so both sides see one order whatever the
 * file system does after a rewrite.
 *
 * Compared: the transcript (every host and UTIL call that matters, logged
 * string comparisons, the stand-in's view of each block, and the world
 * files left on disk, by name, attributes and digest), FE's .data, GLOBAL
 * and the heap, pointer-aware.  Callbacks run over a painted stack. */
/* A drive search of the CD's boosters lists thousands of BGLs, and the stub
 * list grows by copying. */
#define ARENA_SIZE (256u << 20)
#define FACILDIFF_MAIN facildiff_reference_main
#include "facildiff.c"

#define FESL_D(va)       ((uint32_t)(va) - 0x206ce000u)
#define FESL_MAX_FINDS   16
#define FESL_FIND_MAX    4096
#define FESL_WORLDS      8

/* Text the module hands over is logged; a pointer that cannot be read is
 * logged as such and counted, rather than taking the harness down. */
static uint32_t fesl_bad_pointers;

static int fesl_readable(const void *value)
{
    return value != NULL && !IsBadReadPtr(value, 1);
}

static void fesl_put_checked(const char *value, uint32_t limit)
{
    if (value != NULL && !fesl_readable(value)) {
        fe_put("BAD(%#x)", fe_ptr(value));
        fesl_bad_pointers++;
        return;
    }
    query_put_text(value, limit);
}
#define query_put_text fesl_put_checked

static uint32_t STDCALL fesl_strlen(const char *value)
{
    fe_put(" len(");
    query_put_text(value, 64);
    fe_put(")");
    return fesl_readable(value) ? (uint32_t)strlen(value) : 0;
}

static int32_t STDCALL fesl_stricmp(const char *left, const char *right)
{
    fe_put(" icmp(");
    query_put_text(left, 64);
    fe_put(",");
    query_put_text(right, 64);
    fe_put(")");
    if (!fesl_readable(left) || !fesl_readable(right))
        return 1;
    return _strcmpi(left, right);
}

static int32_t STDCALL fesl_memcmp(const void *left, const void *right,
                                   uint32_t size)
{
    fe_put(" mcmp(");
    query_put_text((const char *)left, size < 64 ? size : 64);
    fe_put(",");
    query_put_text((const char *)right, size < 64 ? size : 64);
    fe_put(",%#x)", size);
    if (size != 0 && (!fesl_readable(left) || !fesl_readable(right)))
        return 1;
    return memcmp(left, right, size);
}

/* ---- the sandbox ------------------------------------------------------- */

static char fesl_root[MAX_PATH * 2];          /* %TEMP%\OpenFS98-fesldiff-<pid> */
static char fesl_drive[26][MAX_PATH * 2];     /* each letter's directory */
static char fesl_game[MAX_PATH * 2];          /* W:\FS98 on disk */
static char fesl_scenery[MAX_PATH * 2];       /* ...\SCENERY */
static uint32_t fesl_temp_counter;

struct fesl_world {
    const char *name;
    uint8_t data[0x2000];
    uint32_t size;
};

static struct fesl_world fesl_worlds[FESL_WORLDS];
static uint32_t fesl_world_count;

/* Each letter has a directory; a path on it is that directory plus the
 * rest.  "X:name", relative to X's current directory, is taken from its
 * root. */
static const char *fesl_map(const char *path, char *out, uint32_t size)
{
    int letter;

    if (path == NULL)
        return NULL;
    if (((path[0] | 0x20) < 'a' || (path[0] | 0x20) > 'z') || path[1] != ':')
        return path;
    letter = (path[0] | 0x20) - 'a';
    if (path[2] == '\\' || path[2] == 0)
        snprintf(out, size, "%s%s", fesl_drive[letter], path + 2);
    else
        snprintf(out, size, "%s\\%s", fesl_drive[letter], path + 2);
    return out;
}

/* The host's extension table, 0x417f20, in id order. */
static const char *const fesl_extensions[] = {
    "", ".BGL", ".STN", ".MOD", ".R8", ".R81", ".R82", ".0", ".1", ".PAL",
    ".HAZ", ".PCX", ".BMP", ".PIX", ".VIS", ".AIR", ".GAU", ".ICN", ".BIN",
    ".WAV", ".MDL", ".DLL", ".FS6", ".BND", ".NDL", ".PNL", ".ADV", ".SIM",
    ".LOG", ".VID", ".TMP", ".TXR", ".LES", ".CHL", ".DRV", ".SC1", ".CFG"
};
#define FESL_EXTENSIONS (int32_t)(sizeof(fesl_extensions) / sizeof(fesl_extensions[0]))

static const char *fesl_extension_text(int32_t id)
{
    if (id == -2)
        return "";
    if (id < 0 || id >= FESL_EXTENSIONS)
        return NULL;
    return fesl_extensions[id];
}

static int32_t fesl_extension_id(const char *extension)
{
    int32_t id;

    if (extension == NULL || extension[0] == 0)
        return 0;
    for (id = 1; id < FESL_EXTENSIONS; id++)
        if (_strcmpi(extension, fesl_extensions[id]) == 0)
            return id;
    return -1;
}

/* path + the extension's text, on the sandbox. */
static int fesl_full(char *out, uint32_t size, const char *path,
                     int32_t extension)
{
    char mapped[MAX_PATH * 2];
    const char *text = fesl_extension_text(extension);
    const char *where;

    if (path == NULL || text == NULL)
        return 0;
    where = fesl_map(path, mapped, sizeof(mapped));
    return (uint32_t)snprintf(out, size, "%s%s", where, text) < size;
}

/* ---- host services ------------------------------------------------------ */

/* Pointers into either side's stack frames differ by construction. */
static void fesl_put_pointer(uint32_t address)
{
    uint32_t here = fe_ptr(&address);

    if (address - (here - 0x100000) < 0x200000)
        fe_put("stack");
    else
        fe_put_pointer(address);
}

/* +0x130: FLTSIM98 makes it from its own path with _makepath, so it ends in
 * a backslash -- which FE's "=:\=\" resolution depends on. */
static int32_t STDCALL fesl_basedir(char *out)
{
    strcpy(out, "W:\\FS98\\");
    fe_put(" basedir");
    return 0;
}

static int32_t STDCALL fesl_loadstring(uint32_t id, char *out, int32_t size)
{
    static const struct { uint32_t id; const char *text; } strings[] = {
        { 0x9003, "SCENERY" }, { 0x9006, "TEXTURE" }, { 0x9011, "=:" },
        { 0x9012, "\\=\\" }, { 0x9013, "BSTINFO" }, { 0x9015, ".BGL" },
        { 0x9100, "N S E W" },
        { 0xf440, "If you are not sure how to use the Scenery Library, choose the" },
        { 0xf441, "CANCEL button." }, { 0xf442, "Pilot's Help." },
        { 0xf443, "" },
        { 0xf444, "Warning!  Searching a drive may take a while." },
        { 0xf445, "\nPress OK to proceed, or CANCEL to abort the scan." },
        { 0xf446, "Scanning drive " }, { 0xf447, "\nPlease wait." },
        { 0xf448, "No description available." },
        { 0xf449, "Make sure the correct CD-ROM is in the CD-ROM drive." },
        { 0xf44a, "Are you sure you want to delete the selected performance booster?" },
        { 0xf44b, "Are you sure you want to delete the selected scenery file(s)?" }
    };
    const char *text = "";
    uint32_t index;

    for (index = 0; index < sizeof(strings) / sizeof(strings[0]); index++)
        if (strings[index].id == id)
            text = strings[index].text;
    if (size > 0) {
        strncpy(out, text, (size_t)size - 1);
        out[size - 1] = 0;
    }
    fe_put(" string(%#x,%d)", id, size);
    return (int32_t)strlen(text);
}

static int32_t STDCALL fesl_strcmp(const char *left, const char *right)
{
    fe_put(" cmp(");
    query_put_text(left, 64);
    fe_put(",");
    query_put_text(right, 64);
    fe_put(")");
    if (!fesl_readable(left) || !fesl_readable(right))
        return 1;
    return host_strcmp(left, right);
}

static char *STDCALL fesl_strcpy(char *dst, const char *src)
{
    fe_put(" cpy(");
    query_put_text(src, 64);
    fe_put(")");
    if (!fesl_readable(src) || IsBadWritePtr(dst, strlen(src) + 1)) {
        fe_put("BAD(%#x)", fe_ptr(dst));
        fesl_bad_pointers++;
        return dst;
    }
    return strcpy(dst, src);
}

static char *STDCALL fesl_strcat(char *dst, const char *src)
{
    fe_put(" cat(");
    query_put_text(src, 64);
    fe_put(")");
    if (!fesl_readable(src) || !fesl_readable(dst)) {
        fesl_bad_pointers++;
        return dst;
    }
    return strcat(dst, src);
}

/* +0x2c4: fit a string to `size` characters.  The host's elides the middle
 * through a heap copy; this keeps the tail behind "...", which is all the
 * Library needs (it shows the result and keeps its own full copy). */
static int32_t STDCALL fesl_fit(char *dst, const char *src, uint32_t size)
{
    uint32_t length = (uint32_t)strlen(src);
    char copy[0x400];

    fe_put(" fit(%#x)", size);
    if (size > length) {
        memmove(dst, src, length + 1);
        return 0;
    }
    if (size <= 3)
        return -1;
    snprintf(copy, sizeof(copy), "...%s", src + length - (size - 4));
    strcpy(dst, copy);
    return 0;
}

/* +0x18c, .text+0x9f10: 0x1003 for an empty path, -1 for one holding any of
 * " & * / < > ? |, else 0. */
static int32_t STDCALL fesl_path_check(const char *path)
{
    int32_t result;

    if (path == NULL)
        result = -1;
    else if (path[0] == 0)
        result = 0x1003;
    else
        result = strpbrk(path, "\"&*/<>?|") != NULL ? -1 : 0;
    fe_put(" pathcheck(");
    query_put_text(path, 64);
    fe_put(")=%d", result);
    return result;
}

/* +0x150: GetDiskFreeSpace of the path's drive. */
static int32_t STDCALL fesl_free_space(const char *path, uint32_t *out)
{
    char drive[4] = "?:\\";
    char mapped[MAX_PATH * 2];
    DWORD attributes;

    out[0] = 0;
    out[1] = 0;
    drive[0] = path != NULL ? path[0] : 'W';
    fesl_map(drive, mapped, sizeof(mapped));
    attributes = GetFileAttributesA(mapped);
    fe_put(" freespace(%c)", drive[0]);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        fe_put("=-1");
        return -1;
    }
    out[0] = 0x10000000u;
    return 0;
}

/* +0x158: 0 if the file or directory is there. */
static int32_t STDCALL fesl_exists(const char *path, int32_t extension)
{
    char full[MAX_PATH * 2];
    int32_t result = -1;

    if (fesl_full(full, sizeof(full), path, extension) &&
        GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES)
        result = 0;
    fe_put(" exists(");
    query_put_text(path, 80);
    fe_put(",%d)=%d", extension, result);
    return result;
}

static int32_t fesl_open_common(struct host_file **out, const char *path,
                                 int32_t extension, int32_t mode, int fatal)
{
    char full[MAX_PATH * 2];

    if (!fesl_full(full, sizeof(full), path, extension)) {
        *out = NULL;
        fe_put(" open(bad)");
        return -1;
    }
    fe_put(" @");
    query_put_text(path, 80);
    return fe_open_file(out, full, -2, mode, fatal);
}

static int32_t STDCALL fesl_open(struct host_file **out, const char *path,
                                 int32_t extension, int32_t mode)
{
    return fesl_open_common(out, path, extension, mode, 0);
}

static int32_t STDCALL fesl_open_fatal(struct host_file **out,
                                       const char *path, int32_t extension,
                                       int32_t mode)
{
    return fesl_open_common(out, path, extension, mode, 1);
}

/* +0x134 / +0x138, .text+0x9240 / +0x92c0: the host's own attribute bits --
 * 1 normal, 2 read-only, 4 archive, 8 hidden, 0x10 directory. */
static int32_t STDCALL fesl_get_attributes(int32_t *out, const char *path,
                                           int32_t extension)
{
    char full[MAX_PATH * 2];
    DWORD attributes;

    if (!fesl_full(full, sizeof(full), path, extension))
        return -1;
    attributes = GetFileAttributesA(full);
    fe_put(" getattr(");
    query_put_text(path, 80);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        fe_put(")=-1");
        return -1;
    }
    *out = 0;
    if (attributes & FILE_ATTRIBUTE_NORMAL)
        *out = 1;
    if (attributes & FILE_ATTRIBUTE_ARCHIVE)
        *out |= 4;
    if (attributes & FILE_ATTRIBUTE_READONLY)
        *out |= 2;
    if (attributes & FILE_ATTRIBUTE_HIDDEN)
        *out |= 8;
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        *out |= 0x10;
    fe_put(")=%#x", *out);
    return 0;
}

static int32_t STDCALL fesl_set_attributes(int32_t value, const char *path,
                                           int32_t extension)
{
    char full[MAX_PATH * 2];
    DWORD attributes = 0;
    BOOL ok;

    if (!fesl_full(full, sizeof(full), path, extension))
        return -1;
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES)
        return -1;
    if (value & 1)
        attributes = FILE_ATTRIBUTE_NORMAL;
    if (value & 4)
        attributes |= FILE_ATTRIBUTE_ARCHIVE;
    if (value & 2)
        attributes |= FILE_ATTRIBUTE_READONLY;
    if (value & 8)
        attributes |= FILE_ATTRIBUTE_HIDDEN;
    if (value & 0x10)
        attributes |= FILE_ATTRIBUTE_DIRECTORY;
    ok = SetFileAttributesA(full, attributes);
    fe_put(" setattr(");
    query_put_text(path, 80);
    fe_put(",%#x)", value);
    return ok ? 0 : -1;
}

static int32_t STDCALL fesl_create(struct host_file **out, const char *path,
                                   int32_t extension)
{
    struct host_file *file;
    char full[MAX_PATH * 2];

    *out = NULL;
    fe_put(" create(");
    query_put_text(path, 80);
    fe_put(",%d)", extension);
    if (!fesl_full(full, sizeof(full), path, extension))
        return -1;
    file = fe_new_host_file();
    if (file == NULL)
        return -1;
    file->file = CreateFileA(full, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file->file == INVALID_HANDLE_VALUE) {
        fe_put("=-1");
        return -1;
    }
    file->open = 1;
    strncpy(file->path, full, sizeof(file->path) - 1);
    *out = file;
    fe_put("=file%u", (uint32_t)(file - host_files));
    return 0;
}

static int32_t STDCALL fesl_delete(const char *path, int32_t extension)
{
    char full[MAX_PATH * 2];
    int32_t result = -1;

    if (fesl_full(full, sizeof(full), path, extension) && DeleteFileA(full))
        result = 0;
    fe_put(" delete(");
    query_put_text(path, 80);
    fe_put(",%d)=%d", extension, result);
    return result;
}

static int32_t STDCALL fesl_move(const char *to, int32_t to_extension,
                                 const char *from, int32_t from_extension)
{
    char to_full[MAX_PATH * 2];
    char from_full[MAX_PATH * 2];
    int32_t result = -1;

    if (fesl_full(to_full, sizeof(to_full), to, to_extension) &&
        fesl_full(from_full, sizeof(from_full), from, from_extension) &&
        MoveFileA(from_full, to_full))
        result = 0;
    fe_put(" move(");
    query_put_text(from, 80);
    fe_put(",%d,", from_extension);
    query_put_text(to, 80);
    fe_put(",%d)=%d", to_extension, result);
    return result;
}

/* +0x174, .text+0x8de0: GetTempFileName in the directory, the file deleted
 * again and ".tmp" cut off.  Here the name is a counter. */
static int32_t STDCALL fesl_temp(char *out, const char *dir)
{
    fesl_temp_counter++;
    if (dir != NULL && dir[0] != 0)
        snprintf(out, MAX_PATH, "%s\\%X", dir, 0x1000u + fesl_temp_counter);
    else
        snprintf(out, MAX_PATH, "%X", 0x1000u + fesl_temp_counter);
    fe_put(" temp=");
    query_put_text(out, 80);
    return 0;
}

/* +0x1a8, .text+0x9430: SetEndOfFile at `size`, the position kept. */
static int32_t STDCALL fesl_truncate(struct host_file *file, uint32_t size)
{
    DWORD position;

    if (file == NULL)
        return -1;
    position = SetFilePointer(file->file, 0, NULL, FILE_CURRENT);
    SetFilePointer(file->file, (LONG)size, NULL, FILE_BEGIN);
    SetEndOfFile(file->file);
    SetFilePointer(file->file, (LONG)position, NULL, FILE_BEGIN);
    fe_put(" truncate(%#x)", size);
    return 0;
}

/* +0x230: three copies through a heap block. */
static int32_t STDCALL fesl_swap(void *left, void *right, uint32_t size)
{
    uint8_t buffer[0x1000];

    fe_put(" swap(");
    fesl_put_pointer(fe_ptr(left));
    fe_put(",");
    fesl_put_pointer(fe_ptr(right));
    fe_put(",%#x)", size);
    if (size > sizeof(buffer))
        return -1;
    memcpy(buffer, left, size);
    memcpy(left, right, size);
    memcpy(right, buffer, size);
    return 0;
}

/* +0x304: the count to zero; nothing freed. */
static int32_t STDCALL fesl_list_clear(uint32_t *list)
{
    fe_put(" listclear(");
    fesl_put_pointer(fe_ptr(list));
    fe_put(")");
    list[0] = 0;
    return 0;
}

/* Find-first/next/close over a sorted snapshot of the directory. */
struct fesl_find {
    int open;
    uint32_t count;
    uint32_t next;
    WIN32_FIND_DATAA *entries;
};

static struct fesl_find fesl_finds[FESL_MAX_FINDS];
static WIN32_FIND_DATAA fesl_find_data[FESL_MAX_FINDS][FESL_FIND_MAX];

static int fesl_find_order(const void *left, const void *right)
{
    return _strcmpi(((const WIN32_FIND_DATAA *)left)->cFileName,
                    ((const WIN32_FIND_DATAA *)right)->cFileName);
}

static int32_t STDCALL fesl_find_first(struct fesl_find **out,
                                       const char *dir, const char *pattern,
                                       int32_t extension)
{
    const char *text = fesl_extension_text(extension);
    char path[MAX_PATH * 2];
    char mapped[MAX_PATH * 2];
    struct fesl_find *find = NULL;
    HANDLE handle;
    uint32_t index;
    size_t length;

    fe_put(" findfirst(");
    query_put_text(dir, 80);
    fe_put(",");
    query_put_text(pattern, 16);
    fe_put(",%d)", extension);
    for (index = 0; index < FESL_MAX_FINDS; index++)
        if (!fesl_finds[index].open) {
            find = &fesl_finds[index];
            break;
        }
    if (find == NULL || (text == NULL && extension != -1)) {
        fe_put("=-1");
        return -1;
    }
    path[0] = 0;
    if (dir != NULL) {
        snprintf(path, sizeof(path), "%s",
                 fesl_map(dir, mapped, sizeof(mapped)));
        length = strlen(path);
        if (length == 0 || path[length - 1] != '\\')
            strcat(path, "\\");
    }
    strcat(path, pattern != NULL ? pattern : "*");
    strcat(path, extension == -1 ? ".*" : text);
    find->entries = fesl_find_data[index];
    find->count = 0;
    handle = FindFirstFileA(path, &find->entries[0]);
    if (handle == INVALID_HANDLE_VALUE) {
        fe_put("=-1");
        return -1;
    }
    do {
        if (strcmp(find->entries[find->count].cFileName, ".") != 0 &&
            strcmp(find->entries[find->count].cFileName, "..") != 0)
            find->count++;
        if (find->count == FESL_FIND_MAX)
            break;
    } while (FindNextFileA(handle, &find->entries[find->count]));
    FindClose(handle);
    qsort(find->entries, find->count, sizeof(find->entries[0]),
          fesl_find_order);
    find->open = 1;
    find->next = 0;
    *out = find;
    fe_put("=find%u/%u", index, find->count);
    return 0;
}

static int32_t STDCALL fesl_find_next(struct fesl_find *find,
                                      struct host_find_entry *entry)
{
    while (find != NULL && find->open && find->next < find->count) {
        WIN32_FIND_DATAA *data = &find->entries[find->next++];
        char extension[0x100];

        if (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            strcpy(entry->name, data->cFileName);
            entry->type = -2;
            entry->size = 0;
        } else {
            _splitpath(data->cFileName, NULL, NULL, entry->name, extension);
            entry->type = fesl_extension_id(extension);
            entry->size = data->nFileSizeLow;
            entry->writable =
                (data->dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : 1;
            if (entry->type < 0)
                continue;
        }
        fe_put(" next('%.60s',%d)", entry->name, entry->type);
        return 0;
    }
    return -1;
}

static int32_t STDCALL fesl_find_close(struct fesl_find **find)
{
    if (find == NULL || *find == NULL || !(*find)->open)
        return -1;
    (*find)->open = 0;
    *find = NULL;
    fe_put(" findclose");
    return 0;
}

/* UTIL +0xa8/+0xac, the modeless "scanning" alert: logged, never shown. */
static int32_t STDCALL fesl_alert_open(void **handle, uint32_t tag,
                                       uint32_t p1, uint32_t p2, uint32_t p3,
                                       uint32_t p4, uint32_t p5, uint32_t lines)
{
    const uint32_t pieces[5] = { p1, p2, p3, p4, p5 };
    uint32_t index;

    *handle = arena + ARENA_SIZE - 16;
    fe_put(" alertopen(%#x,%u", tag, lines);
    for (index = 0; index < 5; index++) {
        fe_put(",");
        if (pieces[index] >= 0x10000)
            query_put_text((const char *)(uintptr_t)pieces[index], 40);
        else
            fe_put("%#x", pieces[index]);
    }
    fe_put(")");
    return 0;
}

static int32_t STDCALL fesl_alert_close(void **handle)
{
    fe_put(" alertclose");
    *handle = NULL;
    return 0;
}

/* ---- the fixture -------------------------------------------------------- */

static int fesl_mkdir(const char *relative)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\%s", fesl_root, relative);
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int fesl_file(const char *relative, const void *data, uint32_t size)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\%s", fesl_root, relative);
    return fe_write_fixture_file(path, data, size);
}

/* A BSTINFO: the title's offset at 0x10, the description's at 0x12 and the
 * layer at 0x16, as the CD's. */
static int fesl_bstinfo(const char *relative, const char *title,
                        const char *text, uint16_t layer, uint16_t bad)
{
    uint8_t data[0x200];
    uint32_t size;

    memset(data, 0, sizeof(data));
    *(uint16_t *)(data + 0x10) = bad != 0 ? bad : 0x18;
    *(uint16_t *)(data + 0x12) = (uint16_t)(0x18 + strlen(title) + 1);
    *(uint16_t *)(data + 0x16) = layer;
    strcpy((char *)data + 0x18, title);
    strcpy((char *)data + 0x18 + strlen(title) + 1, text);
    size = (uint32_t)(0x18 + strlen(title) + 1 + strlen(text) + 1);
    return fesl_file(relative, data, size);
}

struct fesl_area {
    uint8_t layer;
    uint8_t flags;
    const char *path;
    const char *title;
};

struct fesl_cache {
    uint8_t flag;
    uint16_t layer;
    uint8_t b6;
    uint8_t b7;
    const char *dir;
    const char *source;
};

static void fesl_put_string(uint8_t *data, uint32_t *at, const char *text)
{
    uint32_t length = (uint32_t)strlen(text) + 1;

    memcpy(data + *at, text, length);
    *at += length;
}

/* A world file as FE writes one: the header's eight words (size, magic,
 * title, second title, word list, areas, caches, paths) and sixteen bytes,
 * the bytes up to the title kept unread, the titles, the word list and the
 * three record lists, each ended by a byte that is not 1. */
static uint32_t fesl_build_world(uint8_t *data, uint32_t blob,
                                 const char *title, const char *title2,
                                 const uint16_t *words,
                                 const char *const *paths,
                                 const struct fesl_area *areas,
                                 const struct fesl_cache *caches)
{
    uint16_t *header = (uint16_t *)data;
    uint32_t at = 0x20;
    uint32_t index;

    memset(data, 0, 0x2000);
    header[1] = 0x524d;
    for (index = 0; index < 0x10; index++)
        data[0x10 + index] = (uint8_t)(0x31 * index + 7);
    for (index = 0; index < blob; index++)
        data[at++] = (uint8_t)(index * 13 + 1);
    header[2] = (uint16_t)at;
    fesl_put_string(data, &at, title);
    header[3] = (uint16_t)at;
    strcpy((char *)data + at, title2);
    at += 0x100;
    header[4] = (uint16_t)at;
    for (index = 0; words[index] != 0; index++) {
        *(uint16_t *)(data + at) = words[index];
        at += 2;
    }
    *(uint16_t *)(data + at) = 0;
    at += 2;
    header[7] = (uint16_t)at;
    for (index = 0; paths[index] != NULL; index++) {
        data[at++] = 1;
        data[at++] = 0x5a;
        fesl_put_string(data, &at, paths[index]);
    }
    data[at++] = 0;
    header[5] = (uint16_t)at;
    for (index = 0; areas[index].path != NULL; index++) {
        data[at++] = 1;
        data[at++] = areas[index].layer;
        data[at++] = areas[index].flags;
        fesl_put_string(data, &at, areas[index].path);
        fesl_put_string(data, &at, areas[index].title);
    }
    data[at++] = 0;
    header[6] = (uint16_t)at;
    for (index = 0; caches[index].dir != NULL; index++) {
        data[at++] = 1;
        data[at++] = caches[index].flag;
        *(uint16_t *)(data + at) = caches[index].layer;
        at += 2;
        data[at++] = caches[index].b6;
        data[at++] = caches[index].b7;
        fesl_put_string(data, &at, caches[index].dir);
        fesl_put_string(data, &at, caches[index].source);
    }
    data[at++] = 0;
    header[0] = (uint16_t)at;
    return at;
}

static void fesl_add_world(const char *name, const uint8_t *data,
                           uint32_t size)
{
    struct fesl_world *world = &fesl_worlds[fesl_world_count++];

    world->name = name;
    memcpy(world->data, data, size);
    world->size = size;
}

static int fesl_make_worlds(void)
{
    static const uint16_t words[] = { 0x1234, 0x0001, 0xfffe, 0 };
    static const uint16_t no_words[] = { 0 };
    static const char *const no_paths[] = { NULL };
    static const char *const paths[] = { "=:\\=\\PATHONE", "M:\\X", NULL };
    static const struct fesl_area no_areas[] = { { 0, 0, NULL, NULL } };
    static const struct fesl_cache no_caches[] = { { 0, 0, 0, 0, NULL, NULL } };
    static const struct fesl_area boost_areas[] = {
        { 30, 1, "=:\\=\\scenery\\chicago\\*.BGL", "Chicago" },
        { 40, 3, "M:\\ADDON1\\*.BGL", "Add-on one" },
        { 40, 1, "M:\\ADDON2\\SUB\\*.BGL", "Add-on two" },
        { 20, 0, "K:\\hawaii\\*.BGL", "Hawaii" },
        { 20, 3, "=:\\=\\*.BGL", "FS98 Default World - Metro" },
        { 10, 3, "=:\\=\\scenery\\world\\*.BGL", "FS98 Default World" },
        { 0, 0, NULL, NULL }
    };
    static const struct fesl_cache boost_caches[] = {
        { 1, 5500, 0, 1, "M:\\CACHE1", "K:\\africa" },
        { 0, 1000, 2, 0, "M:\\CACHE2", "K:\\europe" },
        { 1, 2000, 0, 0, "M:\\CACHE3", "M:\\BADINFO" },
        { 1, 1000, 1, 1, "M:\\CACHE4", "Q:\\NOWHERE" },
        { 0, 0, 0, 0, NULL, NULL }
    };
    static const char *const many_paths[] = {
        "=:\\=\\scenery\\chicago\\*.BGL", "M:\\ADDON1\\*.BGL",
        "M:\\ADDON1\\a1.BGL", "M:\\ADDON2\\SUB\\*.BGL",
        "M:\\ADDON2\\SUB2\\b2.BGL", "M:\\BOOST\\*.BGL", "M:\\EMPTY\\*.BGL",
        "K:\\africa\\*.BGL", "K:\\Lasvegas\\*.BGL", "=:\\=\\*.BGL",
        "=:\\=\\scenery\\world\\*.BGL", "Q:\\GONE\\*.BGL"
    };
    static const char *const many_titles[] = {
        "Alpha", "Bravo", "alpha", "Charlie", "", "Delta", "Echo"
    };
    static const uint8_t many_layers[] = { 10, 20, 30, 40, 50, 20, 30, 7 };
    struct fesl_area many[26];
    static uint8_t data[0x2000];
    char path[MAX_PATH * 2];
    FILE *file;
    uint32_t size;
    uint32_t index;

    snprintf(path, sizeof(path), "%s\\Scenery\\world.vis", fixture);
    file = fopen(path, "rb");
    if (file == NULL)
        return 0;
    size = (uint32_t)fread(data, 1, sizeof(data), file);
    fclose(file);
    fesl_add_world("WORLD", data, size);

    size = fesl_build_world(data, 4, "Boosters and add-ons",
                            "Synthetic: booster caches and add-on areas.",
                            words, paths, boost_areas, boost_caches);
    fesl_add_world("BOOST", data, size);

    for (index = 0; index < 25; index++) {
        many[index].layer = many_layers[index % 8];
        many[index].flags = (uint8_t)((index * 7) % 4);
        many[index].path = many_paths[index % 12];
        many[index].title = many_titles[(index * 3) % 7];
    }
    many[25].path = NULL;
    size = fesl_build_world(data, 0x24, "Many areas",
                            "Synthetic: layer, flag and title ties.",
                            no_words, no_paths, many, boost_caches + 2);
    fesl_add_world("MANY", data, size);

    size = fesl_build_world(data, 1, "Empty", "", no_words, no_paths,
                            no_areas, no_caches);
    fesl_add_world("EMPTY", data, size);

    /* The titles past the end: the scan fails on it. */
    size = fesl_build_world(data, 4, "Broken", "", no_words, no_paths,
                            no_areas, no_caches);
    *(uint16_t *)(data + 4) = 0x7000;
    fesl_add_world("BROKEN", data, size);
    return 1;
}

static int fesl_make_fixture(const char *cd)
{
    static const char *const dirs[] = {
        "W", "W\\FS98", "W\\FS98\\SCENERY", "W\\FS98\\SCENERY\\chicago",
        "W\\FS98\\SCENERY\\chicago\\scenery", "W\\FS98\\SCENERY\\world",
        "W\\FS98\\SCENERY\\world\\scenery", "W\\FS98\\TEXTURE",
        "M", "M\\ADDON1", "M\\ADDON1\\SCENERY", "M\\ADDON1\\TEXTURE",
        "M\\ADDON2", "M\\ADDON2\\SUB", "M\\ADDON2\\SUB\\SCENERY",
        "M\\ADDON2\\SUB2", "M\\ADDON2\\SUB2\\SCENERY", "M\\EMPTY",
        "M\\EMPTY\\SCENERY", "M\\NOTSCN", "M\\BOOST", "M\\BOOST\\SCENERY",
        "M\\BOOST\\TEXTURE", "M\\CACHE1", "M\\CACHE1\\SCENERY",
        "M\\CACHE1\\TEXTURE", "M\\CACHE2", "M\\CACHE2\\SCENERY",
        "M\\CACHE2\\TEXTURE", "M\\NOTEX", "M\\NOTEX\\SCENERY", "M\\BADINFO",
        "M\\BADINFO\\SCENERY", "M\\BADINFO\\TEXTURE", "M\\NEW",
        "M\\NEW\\SCENERY", "M\\NEW\\TEXTURE"
    };
    static const char *const files[] = {
        "W\\FS98\\SCENERY\\CHICAGO5.BGL", "W\\FS98\\SCENERY\\world1.bgl",
        "W\\FS98\\SCENERY\\afd_na.bgl",
        "W\\FS98\\SCENERY\\chicago\\scenery\\chiatis.bgl",
        "W\\FS98\\SCENERY\\chicago\\scenery\\chiils.bgl",
        "W\\FS98\\SCENERY\\world\\scenery\\wrld1.bgl",
        "M\\ADDON1\\SCENERY\\a1.bgl", "M\\ADDON1\\SCENERY\\a2.BGL",
        "M\\ADDON1\\SCENERY\\notes.txt",
        "M\\ADDON2\\SUB\\SCENERY\\b1.bgl", "M\\ADDON2\\SUB2\\SCENERY\\b2.bgl",
        "M\\ADDON2\\SUB2\\SCENERY\\b3.bgl", "M\\NOTSCN\\c.bgl",
        "M\\BOOST\\SCENERY\\x.bgl", "M\\NOTEX\\SCENERY\\n.bgl",
        "M\\CACHE1\\SCENERY\\cache1.bgl"
    };
    char path[MAX_PATH * 2];
    char temp[MAX_PATH * 2];
    uint32_t index;
    int letter;

    if (GetTempPathA(sizeof(temp), temp) == 0)
        return 0;
    snprintf(fesl_root, sizeof(fesl_root), "%sOpenFS98-fesldiff-%lu", temp,
             (unsigned long)GetCurrentProcessId());
    if (!CreateDirectoryA(fesl_root, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    for (index = 0; index < sizeof(dirs) / sizeof(dirs[0]); index++)
        if (!fesl_mkdir(dirs[index]))
            return 0;
    for (index = 0; index < sizeof(files) / sizeof(files[0]); index++)
        if (!fesl_file(files[index], files[index], 8 + index))
            return 0;
    if (!fesl_bstinfo("M\\BOOST\\BSTINFO", "Boost", "A local booster.", 1500, 0) ||
        !fesl_bstinfo("M\\CACHE1\\BSTINFO", "Cache one", "The first cache.", 2000, 0) ||
        !fesl_bstinfo("M\\CACHE2\\BSTINFO", "Cache two", "The second cache.", 1000, 0) ||
        !fesl_bstinfo("M\\NOTEX\\BSTINFO", "No texture", "No TEXTURE folder.", 3000, 0) ||
        !fesl_bstinfo("M\\BADINFO\\BSTINFO", "Bad", "Title past the end.", 4000, 0x7fff) ||
        !fesl_bstinfo("M\\NEW\\BSTINFO", "New", "An empty cache.", 1000, 0))
        return 0;
    for (letter = 0; letter < 26; letter++)
        snprintf(fesl_drive[letter], sizeof(fesl_drive[letter]),
                 "%s\\none-%c", fesl_root, 'A' + letter);
    snprintf(fesl_drive['w' - 'a'], sizeof(fesl_drive[0]), "%s\\W", fesl_root);
    snprintf(fesl_drive['m' - 'a'], sizeof(fesl_drive[0]), "%s\\M", fesl_root);
    if (cd != NULL) {
        char boosters[MAX_PATH * 2];

        snprintf(boosters, sizeof(boosters), "%s\\Boosters", cd);
        if (GetFullPathNameA(boosters, sizeof(path), path, NULL) != 0 &&
            GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            snprintf(fesl_drive['k' - 'a'], sizeof(fesl_drive[0]), "%s", path);
    }
    snprintf(fesl_game, sizeof(fesl_game), "%s\\W\\FS98", fesl_root);
    snprintf(fesl_scenery, sizeof(fesl_scenery), "%s\\SCENERY", fesl_game);
    return fesl_make_worlds();
}

/* Every .VIS and temporary file out of SCENERY, then the scenario's set. */
static void fesl_clear_worlds(void)
{
    WIN32_FIND_DATAA data;
    char path[MAX_PATH * 2];
    HANDLE handle;

    snprintf(path, sizeof(path), "%s\\*", fesl_scenery);
    handle = FindFirstFileA(path, &data);
    if (handle == INVALID_HANDLE_VALUE)
        return;
    do {
        char extension[0x100];

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        _splitpath(data.cFileName, NULL, NULL, NULL, extension);
        if (_strcmpi(extension, ".BGL") == 0)
            continue;
        snprintf(path, sizeof(path), "%s\\%s", fesl_scenery, data.cFileName);
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(path);
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
}

static uint32_t fesl_world_set;
static uint32_t fesl_readonly_set;

static void fesl_write_worlds(void)
{
    char path[MAX_PATH * 2];
    uint32_t index;

    fesl_clear_worlds();
    for (index = 0; index < fesl_world_count; index++) {
        if (!(fesl_world_set & (1u << index)))
            continue;
        snprintf(path, sizeof(path), "%s\\%s.VIS", fesl_scenery,
                 fesl_worlds[index].name);
        fe_write_fixture_file(path, fesl_worlds[index].data,
                              fesl_worlds[index].size);
        if (fesl_readonly_set & (1u << index))
            SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
    }
}

/* What the Library left in SCENERY, into the transcript. */
static uint32_t fesl_rewrites;

static void fesl_put_worlds(void)
{
    WIN32_FIND_DATAA data[64];
    char path[MAX_PATH * 2];
    uint32_t count = 0;
    uint32_t index;
    HANDLE handle;

    snprintf(path, sizeof(path), "%s\\*", fesl_scenery);
    handle = FindFirstFileA(path, &data[0]);
    if (handle == INVALID_HANDLE_VALUE)
        return;
    do {
        char extension[0x100];

        if (data[count].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        _splitpath(data[count].cFileName, NULL, NULL, NULL, extension);
        if (_strcmpi(extension, ".BGL") == 0)
            continue;
        if (++count == 64)
            break;
    } while (FindNextFileA(handle, &data[count]));
    FindClose(handle);
    qsort(data, count, sizeof(data[0]), fesl_find_order);
    fe_put(" FILES");
    for (index = 0; index < count; index++) {
        static uint8_t bytes[0x4000];
        FILE *file;
        uint32_t size = 0;
        uint32_t world;

        snprintf(path, sizeof(path), "%s\\%s", fesl_scenery,
                 data[index].cFileName);
        file = fopen(path, "rb");
        if (file != NULL) {
            size = (uint32_t)fread(bytes, 1, sizeof(bytes), file);
            fclose(file);
        }
        fe_put(" %s/%#lx/", data[index].cFileName,
               (unsigned long)(data[index].dwFileAttributes &
                               (FILE_ATTRIBUTE_READONLY |
                                FILE_ATTRIBUTE_HIDDEN)));
        fe_put_blob(bytes, size);
        for (world = 0; world < fesl_world_count; world++) {
            char name[MAX_PATH];

            snprintf(name, sizeof(name), "%s.VIS", fesl_worlds[world].name);
            if (_strcmpi(name, data[index].cFileName) == 0 &&
                (size != fesl_worlds[world].size ||
                 memcmp(bytes, fesl_worlds[world].data, size) != 0) &&
                counting)
                fesl_rewrites++;
        }
    }
}

/* ---- the World stand-in ------------------------------------------------- */

struct fesl_stats {
    uint32_t dialogs[10];
    uint32_t callbacks;
    uint32_t accepts[10];
    uint32_t refused;
    uint32_t alerts[2];
    uint32_t checks_passed;
    uint32_t checks_failed;
    uint32_t scans;
    uint32_t entries;
    uint32_t results[3];
};

static struct fesl_stats fesl_stats;
static int fesl_counting;
static uint32_t fesl_depth;

#define FESL_COUNT(field) do { if (fesl_counting) fesl_stats.field++; } while (0)

static uint8_t *fesl_image(void)
{
    return current->image;
}

#define FESL_L32(va) (*(uint32_t *)(fesl_image() + FESL_D(va)))
#define B32(off)     (*(uint32_t *)(block + (off)))

/* A string the module owns, if it is one. */
static void fesl_put_text(uint32_t address, uint32_t limit)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)address;

    if (address == 0) {
        fe_put("null");
        return;
    }
    if ((p >= arena && p < arena + ARENA_SIZE) ||
        (p >= current->image && p < current->image + FE_IMAGE_SIZE) ||
        (p >= global && p < global + GLOBAL_SIZE))
        query_put_text((const char *)p, limit);
    else
        fe_put("?");
}

static void fesl_put_area_rows(uint32_t count, uint32_t rows)
{
    uint32_t index;

    fe_put(" areas{%u,", count);
    fesl_put_pointer(rows);
    if (rows != 0 && count < 0x1000)
        for (index = 0; index < count && index < 40; index++) {
            uint8_t *row = (uint8_t *)(uintptr_t)(rows + index * 0x40);
            fe_put(" [%#x:", *(uint32_t *)row);
            fesl_put_text(*(uint32_t *)(row + 4), 72);
            fe_put(" %#x:", *(uint32_t *)(row + 0x14));
            fesl_put_text(*(uint32_t *)(row + 0x18), 40);
            fe_put(" %#x:%d %#x:%#x %#x:%#x]", *(uint32_t *)(row + 0x28),
                   *(int32_t *)(row + 0x2c), *(uint32_t *)(row + 0x30),
                   *(uint32_t *)(row + 0x34), *(uint32_t *)(row + 0x38),
                   *(uint32_t *)(row + 0x3c));
        }
    fe_put("}");
}

static void fesl_put_cache_row(uint8_t *row)
{
    fe_put(" [%#x:", *(uint32_t *)row);
    fesl_put_text(*(uint32_t *)(row + 4), 72);
    fe_put(" %#x:", *(uint32_t *)(row + 0x14));
    fesl_put_text(*(uint32_t *)(row + 0x18), 72);
    fe_put(" %#x:", *(uint32_t *)(row + 0x28));
    fesl_put_text(*(uint32_t *)(row + 0x2c), 40);
    fe_put(" %#x:", *(uint32_t *)(row + 0x3c));
    fesl_put_text(*(uint32_t *)(row + 0x40), 40);
    fe_put(" %#x:%#x %#x:%d %#x %#x:%#x]", *(uint32_t *)(row + 0x50),
           *(uint32_t *)(row + 0x54), *(uint32_t *)(row + 0x58),
           *(int32_t *)(row + 0x5c), *(uint32_t *)(row + 0x74),
           *(uint32_t *)(row + 0x78), *(uint32_t *)(row + 0x7c));
}

static void fesl_put_cache_rows(uint32_t count, uint32_t rows)
{
    uint32_t index;

    fe_put(" caches{%u,", count);
    fesl_put_pointer(rows);
    if (rows != 0 && count < 0x1000)
        for (index = 0; index < count && index < 40; index++)
            fesl_put_cache_row((uint8_t *)(uintptr_t)(rows + index * 0x80));
    fe_put("}");
}

/* Calls and successes per button, for the coverage line. */
static const char *fesl_button_names[32];
static uint32_t fesl_button_calls[32][2];

static uint32_t fesl_call(uint8_t *block, uint32_t slot, const char *name)
{
    uint32_t result;
    uint32_t index;

    fe_put(" | %s", name);
    result = dialog_invoke(*(void **)(block + slot), block);
    fe_put(" =%#x", result);
    FESL_COUNT(callbacks);
    if (fesl_counting)
        for (index = 0; index < 32; index++)
            if (fesl_button_names[index] == NULL ||
                fesl_button_names[index] == name) {
                fesl_button_names[index] = name;
                fesl_button_calls[index][result == 0]++;
                break;
            }
    return result;
}

/* Typing into a control: the text cut to the buffer the module gave. */
static void fesl_type(uint32_t buffer, uint32_t size, const char *text)
{
    if (buffer == 0 || size == 0)
        return;
    strncpy((char *)(uintptr_t)buffer, text, size - 1);
    ((char *)(uintptr_t)buffer)[size - 1] = 0;
    fe_put(" type(");
    query_put_text(text, 72);
    fe_put(")");
}

static const char *const fesl_area_paths[] = {
    "=:\\=\\scenery\\chicago\\SCENERY\\*.BGL",
    "W:\\FS98\\SCENERY\\CHICAGO\\SCENERY\\*.BGL",
    "W:\\FS98\\SCENERY\\world\\SCENERY\\wrld1.BGL",
    "M:\\ADDON1\\SCENERY\\*.BGL", "M:\\ADDON1\\SCENERY\\a1.BGL",
    "M:\\ADDON1\\SCENERY\\zz.BGL", "M:\\ADDON2\\SUB\\SCENERY\\*.BGL",
    "M:\\ADDON2\\SUB2\\SCENERY\\b3.BGL", "M:\\EMPTY\\SCENERY\\*.BGL",
    "M:\\BOOST\\SCENERY\\*.BGL", "M:\\NOTSCN\\*.BGL",
    "M:\\ADDON1\\SCENERY\\a1.TXT", "M:ADDON1\\SCENERY\\*.BGL",
    "M:\\AD|DON\\SCENERY\\*.BGL", "", "K:\\africa\\SCENERY\\*.BGL",
    "K:\\hawaii\\SCENERY\\*.BGL", "Q:\\X\\SCENERY\\*.BGL",
    "SCENERY\\*.BGL", "M:\\ADDON1\\SCENERY\\a1", "M:\\ADDON1\\SCENERY\\",
    "M:\\NEW\\SCENERY\\*.BGL", "=:\\=\\*.BGL", "M:\\ADDON1\\scenery\\A2.bgl"
};

static const char *const fesl_titles[] = {
    "Add-on", "", "Chicago", "alpha", "Zulu", "Alpha",
    "A rather long title for a scenery area, to fill the control"
};

static const char *const fesl_cache_sources[] = {
    "K:\\africa", "K:\\europe", "K:\\hawaii", "K:\\Lasvegas", "K:\\Facility",
    "M:\\CACHE1", "M:\\BADINFO", "M:\\NOTEX", "M:\\BOOST", "M:\\ADDON1",
    "M:CACHE1", "M:\\CA|CHE", "", "Q:\\NONE", "M:\\NEW"
};

static const char *const fesl_cache_dirs[] = {
    "M:\\CACHE1", "M:\\CACHE2", "M:\\NOTEX", "M:\\BADINFO", "M:\\NEW",
    "M:CACHE2", "", "M:\\C|X", "K:\\africa", "M:\\BOOST", "Q:\\NONE"
};

#define PICK(table) (table[fe_rnd() % (sizeof(table) / sizeof(table[0]))])

static const int32_t fesl_area_layers[] = {
    10, 20, 30, 40, 50, 0, 5, 15, 60, 1000, -1
};

static const int32_t fesl_cache_layers[] = {
    1000, 999, 2500, 2501, 1500, 3750, 5500, 13750, 13751, 0, 4000, 10000
};

static void fesl_select(uint8_t *block, uint32_t count, uint32_t selection)
{
    uint32_t n = B32(count);

    B32(selection) = fe_rnd() % 8 == 0 ? n : (n != 0 ? fe_rnd() % n : 0);
    fe_put(" sel=%u", B32(selection));
}

static void fesl_finish(uint8_t *block, uint32_t accept, uint32_t kind)
{
    B32(0) = accept;
    fe_put(" answer=%u", accept);
    if (accept != 0 && fesl_counting)
        fesl_stats.accepts[kind]++;
}

/* 0x1306, .data+0x2e28: the world files.  +0x10 count, +0x14 rows of 0x40
 * (+8 path, +0x1c title, +0x30 second title), +0x18 selection; +4 nothing,
 * +8 edit areas, +0xc edit caches. */
static void world_files(uint8_t *block)
{
    uint32_t steps = fe_rnd() % 4;
    uint32_t index;

    fe_put(" worlds{%u", B32(0x10));
    for (index = 0; index < B32(0x10) && index < 16; index++) {
        uint8_t *row = (uint8_t *)(uintptr_t)(B32(0x14) + index * 0x40);
        fe_put(" [%u:", *(uint32_t *)row);
        fesl_put_text(*(uint32_t *)(row + 8), 40);
        fe_put(" ");
        fesl_put_text(*(uint32_t *)(row + 0x1c), 40);
        fe_put(" ");
        fesl_put_text(*(uint32_t *)(row + 0x30), 40);
        fe_put("]");
    }
    fe_put("} sel=%u", B32(0x18));
    while (steps-- != 0) {
        uint32_t choice = fe_rnd() % 9;

        fesl_select(block, 0x10, 0x18);
        if (choice < 4)
            fesl_call(block, 8, "areas");
        else if (choice < 8)
            fesl_call(block, 0xc, "caches");
        else
            fesl_call(block, 4, "nothing");
    }
    /* OK leaves a real row selected: the list control has no other. */
    if (B32(0x10) != 0 && (fe_rnd() % 3 != 0 || B32(0x18) >= B32(0x10)))
        B32(0x18) = fe_rnd() % B32(0x10);
    fe_put(" sel=%u", B32(0x18));
    fesl_finish(block, fe_rnd() % 3 != 0 ? 1 : 0, 0);
}

/* 0x13080003 (.data+0x3170) and 0x13080004 (+0x3138): one area.  +8 title,
 * +0xc the row, +0x10 position status, +0x14 position, +0x2c count; +4
 * checks. */
static void world_area(uint8_t *block, uint32_t kind)
{
    uint32_t tries = 1 + fe_rnd() % 3;
    uint32_t result = 1;
    uint8_t *row = (uint8_t *)(uintptr_t)B32(0xc);

    fe_put(" area{");
    fesl_put_text(B32(8), 40);
    fe_put(" count=%u pos=%u}", B32(0x2c), B32(0x14));
    fesl_put_area_rows(1, B32(0xc));
    while (tries-- != 0) {
        if (fe_rnd() % 4 != 0)
            fesl_type(*(uint32_t *)(row + 4), 0x104, PICK(fesl_area_paths));
        if (fe_rnd() % 3 == 0)
            fesl_type(*(uint32_t *)(row + 0x18), 0x100, PICK(fesl_titles));
        if (fe_rnd() % 2 == 0)
            *(int32_t *)(row + 0x2c) = PICK(fesl_area_layers);
        if (fe_rnd() % 4 == 0)
            *(uint32_t *)(row + 0x34) ^= 1;
        if (fe_rnd() % 2 == 0)
            B32(0x14) = fe_rnd() % (B32(0x2c) + 3);
        result = fesl_call(block, 4, "check");
        if (result == 0)
            FESL_COUNT(checks_passed);
        else
            FESL_COUNT(checks_failed);
        fe_put(" status=%#x pos=%u", B32(0x10), B32(0x14));
        fesl_put_area_rows(1, B32(0xc));
        if (result == 0 && fe_rnd() % 3 != 0)
            break;
    }
    fesl_finish(block, result == 0 && fe_rnd() % 5 != 0 ? 1 : 0, kind);
}

/* 0x13080005, .data+0x2e78: priority.  +0x18 title, +0x1c count, +0x20
 * selection, +0x24 rows, +0x28 status, +0x2c typed position; +4 check, +8
 * up, +0xc down, +0x10 sort, +0x14 move. */
static void world_priority(uint8_t *block)
{
    uint32_t steps = 1 + fe_rnd() % 6;
    uint32_t result = 0;

    fesl_put_area_rows(B32(0x1c), B32(0x24));
    while (steps-- != 0) {
        uint32_t choice = fe_rnd() % 6;

        fesl_select(block, 0x1c, 0x20);
        switch (choice) {
        case 0: fesl_call(block, 8, "up"); break;
        case 1: fesl_call(block, 0xc, "down"); break;
        case 2: fesl_call(block, 0x10, "sort"); break;
        case 3:
            B32(0x2c) = fe_rnd() % (B32(0x1c) + 2);
            fe_put(" to=%u", B32(0x2c));
            fesl_call(block, 0x14, "move");
            break;
        default:
            B32(0x2c) = fe_rnd() % (B32(0x1c) + 2);
            fe_put(" to=%u", B32(0x2c));
            result = fesl_call(block, 4, "priority-check");
            fe_put(" status=%#x", B32(0x28));
            break;
        }
        fesl_put_area_rows(B32(0x1c), B32(0x24));
    }
    fesl_finish(block, result == 0 && fe_rnd() % 4 != 0 ? 1 : 0, 5);
}

/* 0x13080002, .data+0x2f70: areas from a drive.  +0x1c title, a row at
 * +0x20 (+0x24 the shown path, +0x38 the title, +0x4c layer), +0x64
 * position, +0x68 drive count, +0x6c drive, +0x70 drives, +0x74..+0x80 the
 * buttons' states; +4 check, +8 add, +0xc add all, +0x10 search the drive,
 * +0x14 next, +0x18 next folder. */
static void fesl_put_browse(uint8_t *block)
{
    uint32_t index;

    fe_put(" browse{");
    fesl_put_text(B32(0x24), 72);
    fe_put(" ");
    fesl_put_text(B32(0x38), 40);
    fe_put(" %d pos=%u drives=%u:%u[", (int32_t)B32(0x4c), B32(0x64),
           B32(0x68), B32(0x6c));
    for (index = 0; index < B32(0x68) && index < 26; index++)
        fesl_put_text(B32(0x70) + index * 4, 4);
    fe_put("] %u%u%u%u status=%#x cursor=%u/%u}", B32(0x74), B32(0x78),
           B32(0x7c), B32(0x80), B32(0x20), FESL_L32(0x206d2808),
           FESL_L32(0x206d2804));
}

static void world_browse(uint8_t *block)
{
    uint32_t steps = 1 + fe_rnd() % 8;
    uint32_t result = 0;

    fesl_put_browse(block);
    while (steps-- != 0) {
        uint32_t choice = fe_rnd() % 12;

        if (choice < 3) {
            B32(0x6c) = fe_rnd() % 6 == 0 ? B32(0x68) :
                        (B32(0x68) != 0 ? fe_rnd() % B32(0x68) : 0);
            fe_put(" drive=%u", B32(0x6c));
            fesl_call(block, 0x10, "search");
            FESL_COUNT(scans);
            if (fesl_counting)
                fesl_stats.entries += FESL_L32(0x206d2804);
        } else if (choice < 6) {
            fesl_call(block, 0x14, "next");
        } else if (choice < 8) {
            fesl_call(block, 0x18, "nextdir");
        } else if (choice < 10) {
            if (fe_rnd() % 2 == 0)
                fesl_type(B32(0x38), 0x100, PICK(fesl_titles));
            if (fe_rnd() % 3 == 0)
                B32(0x4c) = (uint32_t)PICK(fesl_area_layers);
            B32(0x64) = fe_rnd() % 12;
            fesl_call(block, 8, "browse-add");
        } else if (choice < 11) {
            B32(0x64) = fe_rnd() % 12;
            fesl_call(block, 0xc, "browse-addall");
        } else {
            result = fesl_call(block, 4, "browse-check");
        }
        fesl_put_browse(block);
    }
    result = fesl_call(block, 4, "browse-check");
    fesl_finish(block, result == 0 && fe_rnd() % 4 != 0 ? 1 : 0, 2);
}

/* 0x13080001, .data+0x2ec8: the area list.  +0x1c title, +0x20 count, +0x24
 * rows, +0x28 selection; +4 nothing, +8 add, +0xc edit, +0x10 delete, +0x14
 * browse, +0x18 priority. */
static void world_areas(uint8_t *block)
{
    uint32_t steps = fe_rnd() % 7;

    fe_put(" list{");
    fesl_put_text(B32(0x1c), 40);
    fe_put("}");
    fesl_put_area_rows(B32(0x20), B32(0x24));
    while (steps-- != 0) {
        uint32_t choice = fe_rnd() % 10;
        uint32_t count = B32(0x20);
        uint8_t *row = count != 0 ?
            (uint8_t *)(uintptr_t)(B32(0x24) + (fe_rnd() % count) * 0x40) :
            NULL;

        fesl_select(block, 0x20, 0x28);
        switch (choice) {
        case 0:
            if (row != NULL) {
                *(uint32_t *)(row + 0x34) ^= 1;
                fe_put(" toggle");
            }
            break;
        case 1:
            if (row != NULL) {
                *(int32_t *)(row + 0x2c) = PICK(fesl_area_layers);
                fe_put(" layer=%d", *(int32_t *)(row + 0x2c));
            }
            break;
        case 2: case 3: fesl_call(block, 8, "add"); break;
        case 4: fesl_call(block, 0xc, "edit"); break;
        case 5: fesl_call(block, 0x10, "delete"); break;
        case 6: fesl_call(block, 0x14, "browse"); break;
        case 7: fesl_call(block, 0x18, "priority"); break;
        case 8: fesl_call(block, 0xc, "edit"); break;
        default: fesl_call(block, 4, "nothing"); break;
        }
        fesl_put_area_rows(B32(0x20), B32(0x24));
    }
    fesl_finish(block, fe_rnd() % 4 != 0 ? 1 : 0, 1);
}

/* 0x13080007 (.data+0x30e8) and 0x13080008 (+0x3118): one cache row.  +8
 * title, +0xc the row, +0x10 the list's block; +4 checks. */
static void world_cache(uint8_t *block, uint32_t kind)
{
    uint32_t tries = 1 + fe_rnd() % 3;
    uint32_t result = 1;
    uint8_t *row = (uint8_t *)(uintptr_t)B32(0xc);

    fe_put(" cache{");
    fesl_put_text(B32(8), 40);
    fe_put("}");
    fesl_put_cache_row(row);
    while (tries-- != 0) {
        if (fe_rnd() % 4 != 0)
            fesl_type(*(uint32_t *)(row + 4), 0x104, PICK(fesl_cache_sources));
        if (fe_rnd() % 3 != 0)
            fesl_type(*(uint32_t *)(row + 0x18), 0x104, PICK(fesl_cache_dirs));
        if (fe_rnd() % 2 == 0)
            *(int32_t *)(row + 0x5c) = PICK(fesl_cache_layers);
        if (fe_rnd() % 4 == 0)
            *(uint32_t *)(row + 0x54) ^= 1;
        result = fesl_call(block, 4, "cache-check");
        if (result == 0)
            FESL_COUNT(checks_passed);
        else
            FESL_COUNT(checks_failed);
        fesl_put_cache_row(row);
        fe_put(" range=%d..%d", (int32_t)FESL_L32(kind == 7 ? 0x206d2ca8 :
                                                           0x206d2dd8),
               (int32_t)FESL_L32(kind == 7 ? 0x206d2cac : 0x206d2ddc));
        if (result == 0 && fe_rnd() % 3 != 0)
            break;
    }
    fesl_finish(block, result == 0 && fe_rnd() % 5 != 0 ? 1 : 0, kind);
}

/* 0x13080006, .data+0x3000: the cache list.  +0x18 title, +0x1c count,
 * +0x20 selection, +0x24 rows; +4 nothing, +8 add, +0xc edit, +0x10 delete,
 * +0x14 flush. */
static void world_caches(uint8_t *block)
{
    uint32_t steps = fe_rnd() % 7;

    fesl_put_cache_rows(B32(0x1c), B32(0x24));
    while (steps-- != 0) {
        uint32_t choice = fe_rnd() % 9;
        uint32_t count = B32(0x1c);
        uint8_t *row = count != 0 ?
            (uint8_t *)(uintptr_t)(B32(0x24) + (fe_rnd() % count) * 0x80) :
            NULL;

        fesl_select(block, 0x1c, 0x20);
        switch (choice) {
        case 0:
            if (row != NULL) {
                *(uint32_t *)(row + 0x54) ^= 1;
                fe_put(" toggle");
            }
            break;
        case 1: case 2: fesl_call(block, 8, "cache-add"); break;
        case 3: case 4: fesl_call(block, 0xc, "cache-edit"); break;
        case 5: fesl_call(block, 0x10, "cache-delete"); break;
        case 6: fesl_call(block, 0x14, "flush"); break;
        case 7:
            if (row != NULL) {
                *(int32_t *)(row + 0x5c) = PICK(fesl_cache_layers);
                fe_put(" layer=%d", *(int32_t *)(row + 0x5c));
            }
            break;
        default: fesl_call(block, 4, "nothing"); break;
        }
        fesl_put_cache_rows(B32(0x1c), B32(0x24));
    }
    fe_put(" flush=%u", FESL_L32(0x206d2ca0));
    fesl_finish(block, fe_rnd() % 4 != 0 ? 1 : 0, 6);
}

/* Dialog 2 is UTIL's modal alert: the answer goes to the descriptor's
 * +0xc, which UTIL never initialises -- so it is always written. */
static void world_alert(uint8_t *block)
{
    static const uint32_t answers[] = { 2, 2, 2, 1, 0, 3 };
    uint32_t answer = PICK(answers);

    fe_put(" alert(%#x,%#x,", B32(8), B32(0x10));
    fesl_put_text(B32(0x14), 72);
    fe_put(")=%u", answer);
    B32(0xc) = answer;
    if (fesl_counting)
        fesl_stats.alerts[(answer & 2) != 0]++;
}

static int32_t STDCALL fesl_dialog(uint32_t id, void *param)
{
    uint8_t *block = (uint8_t *)param;
    uint32_t kind;

    fe_put(" DIALOG(%#x,", id);
    fesl_put_pointer(fe_ptr(param));
    fe_put(")");
    if (id == 2) {
        world_alert(block);
        return 0;
    }
    kind = id == 0x1306 ? 0 : (id & 0xf);
    if (kind > 9 || (id != 0x1306 && (id & ~0xfu) != 0x13080000u)) {
        fe_put(" unknown");
        return -1;
    }
    if (fesl_counting)
        fesl_stats.dialogs[kind]++;
    if (fesl_depth >= 4 || fe_rnd() % 40 == 0) {
        fe_put(" refused");
        FESL_COUNT(refused);
        return -1;
    }
    fesl_depth++;
    switch (id) {
    case 0x1306:     world_files(block); break;
    case 0x13080001: world_areas(block); break;
    case 0x13080002: world_browse(block); break;
    case 0x13080003: world_area(block, 3); break;
    case 0x13080004: world_area(block, 4); break;
    case 0x13080005: world_priority(block); break;
    case 0x13080006: world_caches(block); break;
    case 0x13080007: world_cache(block, 7); break;
    case 0x13080008: world_cache(block, 8); break;
    }
    fesl_depth--;
    fe_put(" END(%#x)", id);
    return 0;
}

/* ---- running a side ----------------------------------------------------- */

/* Linkage +0x78 takes no arguments: paint the stack below and call. */
uint32_t fesl_invoke0(void *function);
__asm__(".globl _fesl_invoke0\n"
        "_fesl_invoke0:\n\t"
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %edi\n\t"
        "leal -4(%esp), %edi\n\t"
        "movl $0x1800, %ecx\n\t"
        "movl $0xc5c5c5c5, %eax\n\t"
        "std\n\t"
        "rep stosl\n\t"
        "cld\n\t"
        "call *8(%ebp)\n\t"
        "leal -4(%ebp), %esp\n\t"
        "popl %edi\n\t"
        "popl %ebp\n\t"
        "ret\n");

/* A fault while the Library runs is an outcome, compared like any other:
 * the vectored handler logs it and unwinds to fesl_run_side.  It counts in
 * the host stubs too, since the real host would fault on the same pointer --
 * Next after a cancelled drive search hands host +0x188 a path in the freed
 * listing, at 0x108. */
static void *fesl_fault_jb[5];
static volatile int fesl_in_module;
static uint32_t fesl_faults;

static void fesl_fault_land(void)
{
    __builtin_longjmp(fesl_fault_jb, 1);
}

static LONG WINAPI fesl_vectored(EXCEPTION_POINTERS *exception)
{
    uint8_t *ip = (uint8_t *)(uintptr_t)exception->ContextRecord->Eip;

    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        fesl_in_module && current != NULL) {
        fe_put(" FAULT(%s,%lu,", ip >= current->module_lo &&
                                 ip < current->module_hi ? "fe" : "host",
               (unsigned long)
               exception->ExceptionRecord->ExceptionInformation[0]);
        fesl_put_pointer((uint32_t)
                         exception->ExceptionRecord->ExceptionInformation[1]);
        fe_put(")");
        fesl_in_module = 0;
        exception->ContextRecord->Eip = (DWORD)(uintptr_t)fesl_fault_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

struct fesl_state {
    uint8_t fe[FE_IMAGE_SIZE];
    uint8_t global[GLOBAL_SIZE];
    uint32_t arena_size;
};

static uint32_t fesl_arena_high;

static void fesl_bind_numeric(struct fe_side *side)
{
    struct fs98_import *import = (struct fs98_import *)side->image;
    struct fs98_import *pristine = (struct fs98_import *)side->pristine;

    for (; import->id != 0; import++, pristine++)
        if (import->id == 17)
            pristine->link = import->link = numeric_retail;
}

static void fesl_run_side(struct fe_side *side, uint32_t scenario,
                          struct fesl_state *state, uint8_t *arena_image)
{
    static const char *const selected[] = {
        "SCENERY\\WORLD.VIS", "Scenery\\world.vis", "SCENERY\\BOOST.VIS",
        "SCENERY\\MANY.VIS", "SCENERY\\NONE.VIS", ""
    };
    struct fe_scenario setup;
    uint8_t position[0x18];
    uint32_t result;

    rng = 0x9e3779b9u * (scenario + 1) ^ 0x5eedu;
    if (rng == 0)
        rng = 1;
    current = side;
    current_scenario = scenario;
    transcript_len = 0;
    transcript[0] = 0;
    /* What the last run dirtied: its heap, and a margin for writes past the
     * end of the last block (a priority move from past the list does). */
    memset(arena, 0xa5, fesl_arena_high + 0x100000u < ARENA_SIZE ?
                        fesl_arena_high + 0x100000u : ARENA_SIZE);
    arena_used = 0;
    fe_reset_handles();
    memset(fesl_finds, 0, sizeof(fesl_finds));
    fesl_temp_counter = 0;
    fesl_depth = 0;
    frame = NULL;
    memcpy(side->image, side->pristine, FE_IMAGE_SIZE);
    memset(&setup, 0, sizeof(setup));
    setup.latitude = 4652043u;
    setup.longitude = 0xc1b33f34u;
    setup.altitude = 1000;
    fe_put_position(position, &setup);
    fe_make_global(&setup, position);

    /* The scenario: which world files exist, which are read-only, which one
     * is in use, and whether the first-time warning has been seen. */
    fesl_world_set = (fe_rnd() & 0xe) | 1;
    if (fe_rnd() % 12 == 0)
        fesl_world_set |= 0x10;
    fesl_readonly_set = fe_rnd() % 6 == 0 ? fe_rnd() & 0xf : 0;
    fesl_write_worlds();
    strcpy((char *)global + 0x446, PICK(selected));
    *(uint32_t *)(side->image + FESL_D(0x206d133c)) = fe_rnd() % 5 != 0;
    fe_put("scenario=%u worlds=%#x ro=%#x use=", scenario, fesl_world_set,
           fesl_readonly_set);
    query_put_text((const char *)global + 0x446, 40);

    side->link->init();
    /* init sets 0x32 in GLOBAL +0x444; by the time the Library runs the sim
     * may have consumed those flags. */
    if (fe_rnd() % 2 == 0)
        *(uint16_t *)(global + 0x444) = (uint16_t)(fe_rnd() & 0x81);
    fesl_in_module = 1;
    if (__builtin_setjmp(fesl_fault_jb)) {
        result = 0xfa017u;
        if (fesl_counting)
            fesl_faults++;
    } else {
        result = fesl_invoke0(side->link->slots[(0x78 - 0x18) / 4]);
    }
    fesl_in_module = 0;
    fe_put(" LIBRARY=%#x G365=%#x G444=%#x G446=", result, global[0x365],
           global[0x444]);
    query_put_text((const char *)global + 0x446, 80);
    if (fesl_counting)
        fesl_stats.results[result == 0 ? 0 : result == 0xffffffffu ? 1 : 2]++;
    fesl_put_worlds();
    if (transcript_len >= sizeof(transcript) - 512)
        fe_put(" TRANSCRIPT_FULL");
    /* After a fault the Library's own lists are left mid-flight; deinit
     * frees only the world file's. */
    side->link->deinit();
    fe_put(" DEINIT");
    memcpy(state->fe, side->image, FE_IMAGE_SIZE);
    memcpy(state->global, global, GLOBAL_SIZE);
    memcpy(arena_image, arena, arena_used);
    state->arena_size = arena_used;
    fesl_arena_high = arena_used;
    fe_reset_handles();
}

/* FE keeps two block fields pointing at a caller's stack copy -- the new
 * cache row (+0x3124) and the area being edited (+0x3144) -- and the two
 * sides' frames differ.  Words that are stack addresses on both sides at
 * those offsets compare equal. */
static int32_t fesl_compare_fe(const uint8_t *retail_fe, const uint8_t *ours_fe)
{
    static uint8_t left[FE_IMAGE_SIZE];
    static uint8_t right[FE_IMAGE_SIZE];
    static const uint32_t stack_words[] = {
        0x206d1124, 0x206d1144
    };
    uint32_t here = fe_ptr(&left);
    uint32_t index;

    (void)here;
    memcpy(left, retail_fe, FE_IMAGE_SIZE);
    memcpy(right, ours_fe, FE_IMAGE_SIZE);
    for (index = 0; index < sizeof(stack_words) / sizeof(stack_words[0]);
         index++) {
        uint32_t offset = FESL_D(stack_words[index]);
        uint32_t a = *(uint32_t *)(left + offset);
        uint32_t b = *(uint32_t *)(right + offset);
        uint32_t stack = fe_ptr(&index);

        if (a - (stack - 0x100000) < 0x200000 &&
            b - (stack - 0x100000) < 0x200000) {
            *(uint32_t *)(left + offset) = 0;
            *(uint32_t *)(right + offset) = 0;
        }
    }
    return fe_compare_image(left, right);
}

static int fesl_compare(uint32_t scenario, const char *retail_transcript,
                        const struct fesl_state *retail_state,
                        const uint8_t *retail_arena,
                        const struct fesl_state *ours_state,
                        const uint8_t *ours_arena)
{
    int32_t difference;
    int failed = 0;

    if (strcmp(retail_transcript, transcript) != 0) {
        printf("library %u: transcript differs\n", scenario);
        fe_dump_transcript_difference(retail_transcript, transcript);
        failed = 1;
    }
    if (strstr(retail_transcript, "TRANSCRIPT_FULL") != NULL) {
        printf("library %u: transcript full\n", scenario);
        failed = 1;
    }
    difference = fesl_compare_fe(retail_state->fe, ours_state->fe);
    if (difference >= 0) {
        printf("library %u: FE image differs at +%#x (.data %#x): %02x/%02x\n",
               scenario, (uint32_t)difference,
               (uint32_t)difference + 0x206ce000u,
               retail_state->fe[difference], ours_state->fe[difference]);
        failed = 1;
    }
    if (memcmp(retail_state->global, ours_state->global, GLOBAL_SIZE) != 0) {
        printf("library %u: GLOBAL differs\n", scenario);
        fe_dump_difference("GLOBAL", retail_state->global, ours_state->global,
                           GLOBAL_SIZE);
        failed = 1;
    }
    if (retail_state->arena_size != ours_state->arena_size) {
        printf("library %u: arena size differs: retail %#x comparison %#x\n",
               scenario, retail_state->arena_size, ours_state->arena_size);
        failed = 1;
    } else {
        difference = dialog_compare_arena(retail_arena, ours_arena,
                                          retail_state->arena_size);
        if (difference >= 0) {
            printf("library %u: arena differs at +%#x: %08x/%08x\n", scenario,
                   (uint32_t)difference,
                   *(uint32_t *)(retail_arena + difference),
                   *(uint32_t *)(ours_arena + difference));
            failed = 1;
        }
    }
    return failed;
}

static void fesl_remove_tree(const char *path)
{
    WIN32_FIND_DATAA data;
    char pattern[MAX_PATH * 2];
    HANDLE handle;

    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            char child[MAX_PATH * 2];

            if (strcmp(data.cFileName, ".") == 0 ||
                strcmp(data.cFileName, "..") == 0)
                continue;
            snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                fesl_remove_tree(child);
            } else {
                SetFileAttributesA(child, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(child);
            }
        } while (FindNextFileA(handle, &data));
        FindClose(handle);
    }
    RemoveDirectoryA(path);
}

static void fesl_bind_stubs(void)
{
    SET_SLOT(host, 0x10c, fesl_dialog);
    SET_SLOT(host, 0x130, fesl_basedir);
    SET_SLOT(host, 0x134, fesl_get_attributes);
    SET_SLOT(host, 0x138, fesl_set_attributes);
    SET_SLOT(host, 0x148, fesl_create);
    SET_SLOT(host, 0x14c, fesl_delete);
    SET_SLOT(host, 0x150, fesl_free_space);
    SET_SLOT(host, 0x158, fesl_exists);
    SET_SLOT(host, 0x15c, fesl_find_close);
    SET_SLOT(host, 0x160, fesl_find_next);
    SET_SLOT(host, 0x164, fesl_find_first);
    SET_SLOT(host, 0x16c, fesl_move);
    SET_SLOT(host, 0x174, fesl_temp);
    SET_SLOT(host, 0x17c, fesl_open);
    SET_SLOT(host, 0x180, fesl_open_fatal);
    SET_SLOT(host, 0x18c, fesl_path_check);
    SET_SLOT(host, 0x1a8, fesl_truncate);
    SET_SLOT(host, 0x228, fesl_memcmp);
    SET_SLOT(host, 0x230, fesl_swap);
    SET_SLOT(host, 0x2a0, fesl_loadstring);
    SET_SLOT(host, 0x2a8, fesl_strcpy);
    SET_SLOT(host, 0x2b0, fesl_strcat);
    SET_SLOT(host, 0x2b8, fesl_strlen);
    SET_SLOT(host, 0x2bc, fesl_strcmp);
    SET_SLOT(host, 0x2c0, fesl_stricmp);
    SET_SLOT(host, 0x2c4, fesl_fit);
    SET_SLOT(host, 0x304, fesl_list_clear);
    query_logging = 1;
}

static LONG WINAPI fesl_unhandled(EXCEPTION_POINTERS *exception)
{
    uint32_t *frame_pointer = (uint32_t *)(uintptr_t)exception->ContextRecord->Ebp;
    uint32_t *stack = (uint32_t *)(uintptr_t)exception->ContextRecord->Esp;
    int depth;

    printf("fault in %s scenario %u, transcript %#x bytes: %.200s\n...%s\n",
           current != NULL ? current->name : "?", current_scenario,
           transcript_len, transcript,
           transcript + (transcript_len > 1500 ? transcript_len - 1500 : 0));
    printf("eip %#lx esp %#lx address %#lx ebp chain:",
           (unsigned long)exception->ContextRecord->Eip,
           (unsigned long)exception->ContextRecord->Esp,
           (unsigned long)exception->ExceptionRecord->ExceptionInformation[1]);
    for (depth = 0; depth < 8 && frame_pointer != NULL; depth++) {
        printf(" %#x", frame_pointer[1]);
        frame_pointer = (uint32_t *)(uintptr_t)frame_pointer[0];
    }
    printf("\nstack:");
    for (depth = 0; depth < 24; depth++)
        printf(" %#x", stack[depth]);
    printf("\n");
    return fe_unhandled(exception);
}

int main(int argc, char **argv)
{
    static struct fesl_state retail_state;
    static struct fesl_state ours_state;
    static char retail_transcript[TRANSCRIPT_SIZE];
    uint8_t *retail_arena;
    uint8_t *ours_arena;
    uint8_t *util;
    uint32_t scenarios;
    uint32_t scenario;
    int32_t difference;
    int bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    SetUnhandledExceptionFilter(fesl_unhandled);
    AddVectoredExceptionHandler(1, fesl_vectored);
    if (argc < 3) {
        printf("usage: fesldiff.exe scenarios fixture-root [cd-root]\n");
        return 2;
    }
    scenarios = (uint32_t)strtoul(argv[1], NULL, 0);
    if (GetFullPathNameA(argv[2], sizeof(fixture), fixture, NULL) == 0 ||
        GetCurrentDirectoryA(sizeof(original_dir), original_dir) == 0 ||
        !fe_validate_fixture(fixture))
        return 2;
    if (!fesl_make_fixture(argc > 3 ? argv[3] : NULL)) {
        printf("cannot create the Scenery Library fixture\n");
        return 2;
    }
    printf("K: %s\n", strstr(fesl_drive['k' - 'a'], "none-") != NULL ?
           "absent (no CD Boosters)" : fesl_drive['k' - 'a']);
    memset(arena, 0xa5, ARENA_SIZE);
    retail_arena = (uint8_t *)malloc(ARENA_SIZE);
    ours_arena = (uint8_t *)malloc(ARENA_SIZE);
    if (retail_arena == NULL || ours_arena == NULL)
        return 2;

    facil_build_stubs();
    util = fe_load_support("UTIL_retail.dll", 20);
    SET_SLOT(util, 0x0a8, fesl_alert_open);
    SET_SLOT(util, 0x0ac, fesl_alert_close);
    if (!fe_load_side(&retail, "FE_retail.dll", "retail FE", util) ||
        !fe_load_side(&ours, "FE_sl_ours.dll", "comparison FE", util))
        return 2;
    dialog_load_numeric(util);
    fesl_bind_numeric(&retail);
    fesl_bind_numeric(&ours);
    {
        struct fs98_import *import = (struct fs98_import *)GetProcAddress(
            GetModuleHandleA("UTIL_retail.dll"), "ImportTable");
        for (; import != NULL && import->id != 0; import++)
            if (import->id == 17)
                import->link = numeric_retail;
    }
    fesl_bind_stubs();
    if (!SetCurrentDirectoryA(fesl_game)) {
        printf("cannot enter the fixture's game directory\n");
        return 2;
    }
    difference = fe_compare_image(retail.pristine, ours.pristine);
    if (difference >= 0) {
        printf("initial FE images differ at +%#x\n", (uint32_t)difference);
        return 2;
    }

    for (scenario = 0; scenario < scenarios && bad < 5; scenario++) {
        counting = 1;
        fesl_counting = 1;
        fesl_run_side(&retail, scenario, &retail_state, retail_arena);
        memcpy(retail_transcript, transcript, transcript_len + 1);
        counting = 0;
        fesl_counting = 0;
        fesl_run_side(&ours, scenario, &ours_state, ours_arena);
        bad += fesl_compare(scenario, retail_transcript, &retail_state,
                            retail_arena, &ours_state, ours_arena);
        if (getenv("FESLDIFF_TRACE") != NULL &&
            strtoul(getenv("FESLDIFF_TRACE"), NULL, 0) == scenario)
            printf("%s\n", retail_transcript);
        if ((scenario + 1) % 25 == 0)
            printf("%u/%u\r", scenario + 1, scenarios);
    }
    printf("\nLIBRARY %u scenarios: results ok/-1/fault %u/%u/%u; dialogs "
           "worlds %u, areas %u, browse %u, add %u, edit %u, priority %u, "
           "caches %u, edit cache %u, new cache %u; accepted %u/%u/%u/%u/%u/"
           "%u/%u/%u/%u; %u refused; %u callbacks; checks passed/failed "
           "%u/%u; alerts no/yes %u/%u; %u drive searches, %u entries; %u "
           "world files rewritten; %d differ\n",
           scenario, fesl_stats.results[0], fesl_stats.results[1],
           fesl_stats.results[2], fesl_stats.dialogs[0],
           fesl_stats.dialogs[1], fesl_stats.dialogs[2],
           fesl_stats.dialogs[3], fesl_stats.dialogs[4],
           fesl_stats.dialogs[5], fesl_stats.dialogs[6],
           fesl_stats.dialogs[7], fesl_stats.dialogs[8],
           fesl_stats.accepts[0], fesl_stats.accepts[1],
           fesl_stats.accepts[2], fesl_stats.accepts[3],
           fesl_stats.accepts[4], fesl_stats.accepts[5],
           fesl_stats.accepts[6], fesl_stats.accepts[7],
           fesl_stats.accepts[8], fesl_stats.refused, fesl_stats.callbacks,
           fesl_stats.checks_passed, fesl_stats.checks_failed,
           fesl_stats.alerts[0], fesl_stats.alerts[1], fesl_stats.scans,
           fesl_stats.entries, fesl_rewrites, bad);
    printf("buttons (failed/ok):");
    for (scenario = 0; scenario < 32 && fesl_button_names[scenario] != NULL;
         scenario++)
        printf(" %s %u/%u", fesl_button_names[scenario],
               fesl_button_calls[scenario][0], fesl_button_calls[scenario][1]);
    printf("\nfaults %u\n", fesl_faults);
    if (fesl_bad_pointers != 0)
        printf("%u unreadable pointers passed to the host\n",
               fesl_bad_pointers);
    if (bad == 0)
        printf("IDENTICAL\n");
    SetCurrentDirectoryA(original_dir);
    fesl_remove_tree(fesl_root);
    free(retail_arena);
    free(ours_arena);
    return bad != 0;
}
