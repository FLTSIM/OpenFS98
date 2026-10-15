/* OpenFS98 -- the host's service table (FLTSIM98.EXE, module id 1).
 *
 * The host publishes a service table exactly as the numbered modules do: id 1,
 * then 194 slots running to +0x31c.  A module reaches it through the pointer
 * the loader deposits in that module's ImportTable.
 *
 * EVERY PROTOTYPE BELOW WAS READ FROM THE CALLEE, NOT FROM A CALL SITE.
 *
 * That distinction is not pedantry.  The optimiser interleaves argument pushes
 * across calls: UTIL's dialog builder pushes two arguments destined for
 * FS98_HOST_ALLOC, then calls FS98_HOST_STRLEN in the middle of doing it, and
 * because strlen is `ret 4` it pops only its own argument while the other two
 * survive to become alloc's.  Counting pushes before the `call` makes strlen
 * look like it takes three arguments and alloc two.  Both readings are wrong,
 * and both would compile.  So: when adding a service here, disassemble the
 * target and read its `ret N`.
 *
 * Slots whose prototype is not yet confirmed are deliberately absent rather
 * than guessed -- an absent declaration is a compile error, a wrong one is a
 * crash at a random later point.
 */
#ifndef FS98_HOST_H
#define FS98_HOST_H

#include <stdint.h>
#include <stddef.h>

#ifndef FS98_STDCALL
#  define FS98_STDCALL __attribute__((stdcall))
#endif

/* The table's absolute address in the retail image.  FLTSIM98.EXE has no
 * .reloc, so it is only ever mapped at base 0x400000 and this is a constant. */
#define FS98_HOST_TABLE 0x00417ba0u

/* ---- slot numbers -------------------------------------------------------- */
#define FS98_HOST_S_QUIT        0x024   /* WM_CLOSE to the main window */
#define FS98_HOST_S_TICKS       0x038   /* timeGetTime, straight through the IAT */
#define FS98_HOST_S_DIALOG      0x10c   /* run a registered dialog, by id */
#define FS98_HOST_S_DIALOG_REG  0x110   /* a forwarder to +0x10c, nothing more */
#define FS98_HOST_S_DIALOG_TICK 0x114   /* service the modeless dialogs */
#define FS98_HOST_S_FIND_CLOSE  0x15c
#define FS98_HOST_S_FIND_NEXT   0x160
#define FS98_HOST_S_FIND_FIRST  0x164
#define FS98_HOST_S_PATH_JOIN   0x184
#define FS98_HOST_S_PATH_SPLIT  0x188
#define FS98_HOST_S_ALLOC       0x20c
#define FS98_HOST_S_REALLOC     0x210
#define FS98_HOST_S_FREE        0x214
#define FS98_HOST_S_MEMMOVE     0x21c
#define FS98_HOST_S_QSORT       0x22c   /* MSVCRT qsort, stdcall comparator */
#define FS98_HOST_S_LOADSTRING  0x2a0
#define FS98_HOST_S_STRCPY      0x2a8
#define FS98_HOST_S_STRCAT      0x2b0
#define FS98_HOST_S_STRLEN      0x2b8
#define FS98_HOST_S_STRICMP     0x2c0   /* MSVCRT _strcmpi */
#define FS98_HOST_S_LIST_ADD    0x2f8
#define FS98_HOST_S_LIST_REMOVE 0x300

/* ---- confirmed prototypes ------------------------------------------------ */

/* +0x038 -- milliseconds; UTIL's init seeds both generators from it. */
typedef uint32_t (FS98_STDCALL *fs98_host_ticks_fn)(void);

/* +0x2b8 -- strlen by `repnz scasb`, returning 0 for a null pointer. */
typedef uint32_t (FS98_STDCALL *fs98_host_strlen_fn)(const char *s);

/* +0x2a8 / +0x2b0 -- strcpy and strcat, each returning its destination. */
typedef char *(FS98_STDCALL *fs98_host_strcpy_fn)(char *dst, const char *src);
typedef char *(FS98_STDCALL *fs98_host_strcat_fn)(char *dst, const char *src);

/* +0x20c -- malloc(size) stored through `out`, zero-filled when `zero` is
 * non-zero (rep stosd/stosb at 0x40ce36); otherwise the block is whatever
 * malloc returned.
 *
 * On failure the behaviour depends on `flags`: 4 returns -1 and lets the caller
 * cope, anything else retries once after a purge hook and then, if that fails
 * too, loads string id 9 into an MB_ICONHAND message box and exits the
 * process.  Most of the game therefore treats allocation failure as fatal by
 * choice rather than by omission.  Returns 0 on success. */
typedef int32_t (FS98_STDCALL *fs98_host_alloc_fn)(int32_t flags, uint32_t size,
                                                   void **out, int32_t zero);

/* +0x210 -- realloc(*pp, size) stored back through `pp`.  Failure follows
 * +0x20c's rule: flags 4 restores the old pointer and returns -1, anything
 * else is fatal.  Returns 0 on success. */
typedef int32_t (FS98_STDCALL *fs98_host_realloc_fn)(int32_t flags, uint32_t size,
                                                     void **pp);

/* +0x21c -- MSVCRT memmove(dst, src, size), stdcall; always returns 0. */
typedef int32_t (FS98_STDCALL *fs98_host_memmove_fn)(void *dst, const void *src,
                                                     uint32_t size);

/* +0x214 -- frees `*pp` if non-null, then nulls it.  Always returns 0. */
typedef int32_t (FS98_STDCALL *fs98_host_free_fn)(void **pp);

/* +0x2a0 -- LoadStringA onto a caller buffer. */
typedef int32_t (FS98_STDCALL *fs98_host_loadstring_fn)(uint32_t id, char *buf,
                                                        int32_t len);

/* +0x10c, and +0x110 which only forwards to it -- run dialog `id`.
 *
 * The first argument is NOT a mode.  It is a key into the host's dialog
 * registry, which is filled at startup from every module's Linkage+0x14: a
 * table of 12-byte { id, create, flags } entries ending at id 0.  The owning
 * module is loaded (reference-counted) and `create(main_hwnd, param)` makes the
 * window.  Then, by the entry's flags:
 *
 *   4  modeless: the window joins the host's modeless list and this returns
 *   2  `create` returned an array of windows rather than one
 *   8  leave the main window and the sim's pause state alone
 *
 * and otherwise it runs a modal loop until the window is destroyed.  Returns 0,
 * or -1 if the id is not registered or no window was created.  The answer of
 * a modal dialog comes back through `param`, not the return value.
 *
 * UTIL's alerts use ids 2 and 3, both English\Alerts.DLL's. */
typedef int32_t (FS98_STDCALL *fs98_host_dialog_fn)(uint32_t id, void *param);

/* +0x114 -- service the modeless dialogs: reap any whose window has gone,
 * and send the rest WM_USER+100 (0x464), synchronously.  Nullary. */
typedef void (FS98_STDCALL *fs98_host_dialog_tick_fn)(void);

/* +0x024 -- set the quit flag and send WM_CLOSE to the main window.  Nullary;
 * returns 1. */
typedef int32_t (FS98_STDCALL *fs98_host_quit_fn)(void);

/* ---- strings, sorting -------------------------------------------------- */

/* +0x2c0 -- MSVCRT _strcmpi.  Case-insensitive, so "a" and "A" compare 0. */
typedef int32_t (FS98_STDCALL *fs98_host_stricmp_fn)(const char *a, const char *b);

/* +0x22c -- MSVCRT qsort with a __stdcall comparator.  The host parks the
 * comparator in a global (0x0041a24c) and hands qsort a cdecl trampoline that
 * forwards to it, so the comparator MUST be stdcall -- the trampoline does not
 * clean up after it -- and the service is not reentrant. */
typedef int32_t (FS98_STDCALL *fs98_host_cmp_fn)(const void *a, const void *b);
typedef void (FS98_STDCALL *fs98_host_qsort_fn)(void *base, uint32_t count,
                                                uint32_t width, fs98_host_cmp_fn cmp);

/* ---- the growable array: +0x2f8 and +0x300 ------------------------------- */

/* A vector of fixed-size elements, owned by the caller and initialised by
 * hand: count and capacity 0, `grow` and `elem_size` set, the rest 0. */
struct fs98_host_list {
    int32_t  count;
    int32_t  capacity;
    int32_t  grow;          /* capacity step when full */
    int32_t  elem_size;
    int32_t  iterating;     /* the host's for-each nesting depth; while it is
                               non-zero, removals are queued, not applied */
    int32_t  pending;       /* set when a removal was queued */
    void    *data;          /* one host allocation; moves when it grows */
};

/* +0x2f8 -- append a zeroed element and return it.  Grows by `grow` through
 * the host allocator with the fatal flag, so it never returns null -- but it
 * may move `data`, so earlier element pointers do not survive a call. */
typedef void *(FS98_STDCALL *fs98_host_list_add_fn)(struct fs98_host_list *l);

/* +0x300 -- remove the element `elem` points at, closing the gap.  The index
 * is an unsigned divide of the pointer's offset by `elem_size`. */
typedef void (FS98_STDCALL *fs98_host_list_remove_fn)(struct fs98_host_list *l,
                                                      void *elem);

/* ---- directory enumeration: +0x164, +0x160, +0x15c ----------------------- */

/* Opaque: a 0x14c-byte heap block holding the FindFirstFileA handle, a
 * WIN32_FIND_DATAA, the extension id, and a has-another-entry flag. */
struct fs98_host_find;

/* What +0x160 fills in.  0x110 bytes. */
struct fs98_host_find_entry {
    char     name[0x104];   /* a directory's full name; a file's name
                               WITHOUT its extension */
    int32_t  type;          /* -2 for a directory, else the extension id --
                               and left untouched for an extension the host
                               has not registered */
    uint32_t size;          /* nFileSizeLow; 0 for a directory */
    int32_t  writable;      /* files only: 0 if read-only, else 1.  Left
                               untouched for a directory */
};

#define FS98_HOST_EXT_ANY  (-1)     /* +0x164: append ".*" */
#define FS98_HOST_EXT_NONE (-2)     /* +0x164: append nothing */
#define FS98_HOST_TYPE_DIR (-2)     /* entry.type for a directory */

/* +0x164 -- open a search for  dir "\" pattern ext.
 *
 * `dir` gets a backslash unless it already ends in one, and may be null.
 * `pattern` defaults to "*".  `ext` is an extension id: -1 appends ".*", -2
 * nothing, otherwise the registered extension's text (id 2 is ".STN").
 * Returns 0 and stores the handle, or -1 with *out untouched.
 *
 * An EMPTY `dir` is not the same as a null one: the ends-in-a-backslash test
 * reads the byte before the buffer, finds no backslash there, and adds one --
 * so "" searches the root of the current drive, not the current directory. */
typedef int32_t (FS98_STDCALL *fs98_host_find_first_fn)(struct fs98_host_find **out,
                                                        const char *dir,
                                                        const char *pattern,
                                                        int32_t ext);

/* +0x160 -- next entry, skipping "." and "..".  0, or -1 when exhausted. */
typedef int32_t (FS98_STDCALL *fs98_host_find_next_fn)(struct fs98_host_find *h,
                                                       struct fs98_host_find_entry *e);

/* +0x15c -- FindClose, free the block, null the handle.  Always 0. */
typedef int32_t (FS98_STDCALL *fs98_host_find_close_fn)(struct fs98_host_find **h);

/* ---- paths: +0x184 and +0x188 -------------------------------------------- */

/* +0x184 -- join up to three parts into `out` (MAX_PATH, 0x104 bytes).
 *
 * A one-character drive gets a colon.  Backslashes are inserted between parts
 * that lack them, and a trailing backslash is stripped after the drive and
 * after the directory, but not after the name.  Returns 0, or -1 with out[0]
 * cleared when the result would not fit; -1 also for a null `out`. */
typedef int32_t (FS98_STDCALL *fs98_host_path_join_fn)(char *out,
                                                       const char *drive,
                                                       const char *dir,
                                                       const char *name);

/* +0x188 -- _splitpath, three ways different.  A "\\server" UNC prefix is
 * returned as the drive; an empty path empties every output; and the
 * extension comes back as an id rather than text -- REGISTERING it, as a side
 * effect, if the host has not seen it before.  Any output may be null.
 * Returns 0, or -1 for a null path. */
typedef int32_t (FS98_STDCALL *fs98_host_path_split_fn)(const char *path,
                                                        char *drive, char *dir,
                                                        char *name, int32_t *ext);

/* ---- files and profiles: what ACONTAIN reaches for ------------------------ */

/* Every service here that takes a `path` and an extension id first resolves
 * them (0x40b350) into a heap copy of path + the extension's text -- ".CFG"
 * for 0x24, ".AIR" for 0x0f, nothing for -2 -- and fails with -1 if the id is
 * not one the host knows.  The path is otherwise used as given, so a relative
 * one is relative to the current directory. */

/* +0x0c8 -- GetPrivateProfileStringA(section, key, buf, buf, size) on the
 * sim's own FLTSIM98.CFG.  `buf` is the default as well as the output, so
 * whatever it held comes back when the key is missing.  Returns the count. */
typedef int32_t (FS98_STDCALL *fs98_host_cfg_string_fn)(const char *section,
                                                        const char *key,
                                                        char *buf, uint32_t size);

/* +0x0f0 -- the same on `path` + extension `ext`, after resolving them.
 * Returns the count, or -1 if the path cannot be resolved. */
typedef int32_t (FS98_STDCALL *fs98_host_profile_string_fn)(const char *section,
                                                            const char *key,
                                                            char *buf, uint32_t size,
                                                            const char *path,
                                                            int32_t ext);

/* +0x158 -- 0 if GetFileAttributesA finds `path` + extension -- a directory
 * counts -- else -1. */
typedef int32_t (FS98_STDCALL *fs98_host_exists_fn)(const char *path, int32_t ext);

/* Opaque: a 0x10-byte heap block of { HANDLE, char *path, mode, ext }. */
struct fs98_host_file;

/* +0x17c -- CreateFileA on `path` + extension, OPEN_EXISTING and unshared.
 * Mode 0 reads, 1 reads and writes, 2 writes, 3 writes at the end; any other
 * mode fails.  Returns 0 and stores the block, or -1 with *out nulled. */
typedef int32_t (FS98_STDCALL *fs98_host_file_open_fn)(struct fs98_host_file **out,
                                                       const char *path,
                                                       int32_t ext, int32_t mode);

/* +0x13c -- CloseHandle and free the block.  Takes the block BY VALUE, so the
 * caller's copy is left dangling.  0, or -1 if CloseHandle failed. */
typedef int32_t (FS98_STDCALL *fs98_host_file_close_fn)(struct fs98_host_file *f);

/* +0x194 -- SetFilePointer(offset, whence).  Fails with -1 when the file's
 * total size is below `offset` taken unsigned -- whatever `whence` is, so a
 * relative seek is checked against the size, not against what is left. */
typedef int32_t (FS98_STDCALL *fs98_host_file_seek_fn)(struct fs98_host_file *f,
                                                       uint32_t offset,
                                                       int32_t whence);

/* +0x19c -- ReadFile.  *got is zeroed first, then set to the count read.
 * Returns 0, or -1 if ReadFile failed or `f` is null.  A short read at the
 * end of the file is a success. */
typedef int32_t (FS98_STDCALL *fs98_host_file_read_fn)(struct fs98_host_file *f,
                                                       void *buf, uint32_t n,
                                                       uint32_t *got);

/* +0x218 -- memset(dst, byte, n) by rep stosd/stosb.  Always 0. */
typedef int32_t (FS98_STDCALL *fs98_host_memset_fn)(void *dst, int32_t byte, uint32_t n);

/* +0x2ac -- MSVCRT strncpy; returns dst.  No terminator when src is n long. */
typedef char *(FS98_STDCALL *fs98_host_strncpy_fn)(char *dst, const char *src, uint32_t n);

/* +0x2bc -- strcmp, two bytes a turn; -1, 0 or 1. */
typedef int32_t (FS98_STDCALL *fs98_host_strcmp_fn)(const char *a, const char *b);

/* +0x26c -- load `path` + extension as a module through the host's own
 * loader -- reference-counted, so a module loaded twice is one module -- and
 * store its Linkage.  -1, with *out untouched, if the file is missing or will
 * not load.  The .SIM flight models and .MDL visual models are loaded so. */
typedef int32_t (FS98_STDCALL *fs98_host_module_load_fn)(void **out, const char *path,
                                                         int32_t ext);

/* +0x270 -- release a module +0x26c loaded, by its Linkage; nulls *link.
 * Always 0. */
typedef int32_t (FS98_STDCALL *fs98_host_module_free_fn)(void **link);

#define FS98_HOST_S_CFG_STRING     0x0c8
#define FS98_HOST_S_PROFILE_STRING 0x0f0
#define FS98_HOST_S_FILE_CLOSE     0x13c
#define FS98_HOST_S_EXISTS         0x158
#define FS98_HOST_S_FILE_OPEN      0x17c
#define FS98_HOST_S_FILE_SEEK      0x194
#define FS98_HOST_S_FILE_READ      0x19c
#define FS98_HOST_S_MEMSET         0x218
#define FS98_HOST_S_MODULE_LOAD    0x26c
#define FS98_HOST_S_MODULE_FREE    0x270
#define FS98_HOST_S_STRNCPY        0x2ac
#define FS98_HOST_S_STRCMP         0x2bc

/* Extension ids, as the host's table at 0x417f20 has them. */
#define FS98_HOST_EXT_AIR  0x0f
#define FS98_HOST_EXT_MDL  0x14
#define FS98_HOST_EXT_SIM  0x1b
#define FS98_HOST_EXT_CFG  0x24

/* ---- reaching them ------------------------------------------------------- */

/* A slot holds a function pointer, so fetching one is a load from
 * host + slot.  Use the typed wrappers rather than casting at each site. */
#define FS98_HOST_SLOT(h, slot, type) \
    (*(type *)((unsigned char *)(h) + (slot)))

#define FS98_HOST_TICKS(h)      FS98_HOST_SLOT(h, FS98_HOST_S_TICKS,      fs98_host_ticks_fn)
#define FS98_HOST_STRLEN(h)     FS98_HOST_SLOT(h, FS98_HOST_S_STRLEN,     fs98_host_strlen_fn)
#define FS98_HOST_STRCPY(h)     FS98_HOST_SLOT(h, FS98_HOST_S_STRCPY,     fs98_host_strcpy_fn)
#define FS98_HOST_STRCAT(h)     FS98_HOST_SLOT(h, FS98_HOST_S_STRCAT,     fs98_host_strcat_fn)
#define FS98_HOST_ALLOC(h)      FS98_HOST_SLOT(h, FS98_HOST_S_ALLOC,      fs98_host_alloc_fn)
#define FS98_HOST_FREE(h)       FS98_HOST_SLOT(h, FS98_HOST_S_FREE,       fs98_host_free_fn)
#define FS98_HOST_LOADSTRING(h) FS98_HOST_SLOT(h, FS98_HOST_S_LOADSTRING, fs98_host_loadstring_fn)
#define FS98_HOST_DIALOG(h)     FS98_HOST_SLOT(h, FS98_HOST_S_DIALOG,     fs98_host_dialog_fn)
#define FS98_HOST_DIALOG_REG(h) FS98_HOST_SLOT(h, FS98_HOST_S_DIALOG_REG, fs98_host_dialog_fn)
#define FS98_HOST_DIALOG_TICK(h) FS98_HOST_SLOT(h, FS98_HOST_S_DIALOG_TICK, fs98_host_dialog_tick_fn)
#define FS98_HOST_REALLOC(h)     FS98_HOST_SLOT(h, FS98_HOST_S_REALLOC,     fs98_host_realloc_fn)
#define FS98_HOST_MEMMOVE(h)     FS98_HOST_SLOT(h, FS98_HOST_S_MEMMOVE,     fs98_host_memmove_fn)
#define FS98_HOST_QUIT(h)        FS98_HOST_SLOT(h, FS98_HOST_S_QUIT,        fs98_host_quit_fn)
#define FS98_HOST_STRICMP(h)     FS98_HOST_SLOT(h, FS98_HOST_S_STRICMP,     fs98_host_stricmp_fn)
#define FS98_HOST_QSORT(h)       FS98_HOST_SLOT(h, FS98_HOST_S_QSORT,       fs98_host_qsort_fn)
#define FS98_HOST_LIST_ADD(h)    FS98_HOST_SLOT(h, FS98_HOST_S_LIST_ADD,    fs98_host_list_add_fn)
#define FS98_HOST_LIST_REMOVE(h) FS98_HOST_SLOT(h, FS98_HOST_S_LIST_REMOVE, fs98_host_list_remove_fn)
#define FS98_HOST_FIND_FIRST(h)  FS98_HOST_SLOT(h, FS98_HOST_S_FIND_FIRST,  fs98_host_find_first_fn)
#define FS98_HOST_FIND_NEXT(h)   FS98_HOST_SLOT(h, FS98_HOST_S_FIND_NEXT,   fs98_host_find_next_fn)
#define FS98_HOST_FIND_CLOSE(h)  FS98_HOST_SLOT(h, FS98_HOST_S_FIND_CLOSE,  fs98_host_find_close_fn)
#define FS98_HOST_PATH_JOIN(h)   FS98_HOST_SLOT(h, FS98_HOST_S_PATH_JOIN,   fs98_host_path_join_fn)
#define FS98_HOST_PATH_SPLIT(h)  FS98_HOST_SLOT(h, FS98_HOST_S_PATH_SPLIT,  fs98_host_path_split_fn)
#define FS98_HOST_CFG_STRING(h)  FS98_HOST_SLOT(h, FS98_HOST_S_CFG_STRING,  fs98_host_cfg_string_fn)
#define FS98_HOST_PROFILE_STRING(h) FS98_HOST_SLOT(h, FS98_HOST_S_PROFILE_STRING, fs98_host_profile_string_fn)
#define FS98_HOST_EXISTS(h)      FS98_HOST_SLOT(h, FS98_HOST_S_EXISTS,      fs98_host_exists_fn)
#define FS98_HOST_FILE_OPEN(h)   FS98_HOST_SLOT(h, FS98_HOST_S_FILE_OPEN,   fs98_host_file_open_fn)
#define FS98_HOST_FILE_CLOSE(h)  FS98_HOST_SLOT(h, FS98_HOST_S_FILE_CLOSE,  fs98_host_file_close_fn)
#define FS98_HOST_FILE_SEEK(h)   FS98_HOST_SLOT(h, FS98_HOST_S_FILE_SEEK,   fs98_host_file_seek_fn)
#define FS98_HOST_FILE_READ(h)   FS98_HOST_SLOT(h, FS98_HOST_S_FILE_READ,   fs98_host_file_read_fn)
#define FS98_HOST_MEMSET(h)      FS98_HOST_SLOT(h, FS98_HOST_S_MEMSET,      fs98_host_memset_fn)
#define FS98_HOST_MODULE_LOAD(h) FS98_HOST_SLOT(h, FS98_HOST_S_MODULE_LOAD, fs98_host_module_load_fn)
#define FS98_HOST_MODULE_FREE(h) FS98_HOST_SLOT(h, FS98_HOST_S_MODULE_FREE, fs98_host_module_free_fn)
#define FS98_HOST_STRNCPY(h)     FS98_HOST_SLOT(h, FS98_HOST_S_STRNCPY,     fs98_host_strncpy_fn)
#define FS98_HOST_STRCMP(h)      FS98_HOST_SLOT(h, FS98_HOST_S_STRCMP,      fs98_host_strcmp_fn)

/* The alloc flag that means "give me -1, I will handle it". */
#define FS98_HOST_ALLOC_NONFATAL 4

#endif /* FS98_HOST_H */
