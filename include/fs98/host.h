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
#define FS98_HOST_S_POST_COMMAND 0x03c  /* PostMessageA(main, WM_COMMAND, a, b) */
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
#define FS98_HOST_S_MEMCMP      0x228
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

/* +0x03c -- PostMessageA(main_hwnd, WM_COMMAND, command, parameter).
 * The host discards PostMessageA's result and returns zero. */
typedef int32_t (FS98_STDCALL *fs98_host_post_command_fn)(uint32_t command,
                                                          uint32_t parameter);

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

/* +0x228 -- MSVCRT memcmp. */
typedef int32_t (FS98_STDCALL *fs98_host_memcmp_fn)(const void *a, const void *b,
                                                    uint32_t size);

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

/* ---- read for SOUND ------------------------------------------------------ */

/* +0x050 -- the busy cursor: 1 shows the hourglass, 0 the arrow.  Does
 * nothing if the state is unchanged. */
typedef void (FS98_STDCALL *fs98_host_busy_fn)(int32_t on);

/* +0x0b8 -- a number from the sim's FLTSIM98.CFG: GetPrivateProfileStringA
 * into 30 bytes, then strtod into *out -- left alone if the key is missing
 * or empty.  Always 0. */
typedef int32_t (FS98_STDCALL *fs98_host_cfg_double_fn)(const char *section,
                                                        const char *key, double *out);

/* +0x0bc -- the other way: sprintf("%f") and WritePrivateProfileStringA.
 * Always 0. */
typedef int32_t (FS98_STDCALL *fs98_host_cfg_put_double_fn)(const char *section,
                                                            const char *key, double v);

/* +0x0d0 -- GetPrivateProfileIntA on `path` + extension, *inout as the
 * default.  -1 if the path cannot be resolved, else 0. */
typedef int32_t (FS98_STDCALL *fs98_host_profile_int_fn)(const char *section,
                                                         const char *key, int32_t *inout,
                                                         const char *path, int32_t ext);

/* +0x0e0 -- a number, as +0x0b8 but from `path` + extension. */
typedef int32_t (FS98_STDCALL *fs98_host_profile_double_fn)(const char *section,
                                                            const char *key, double *inout,
                                                            const char *path, int32_t ext);

/* +0x0e8 -- `n` numbers.  Reads into a buffer of n * 30 bytes; if anything
 * came back, strtod `n` times regardless, skipping ", " after each, so a
 * short list fills the rest with 0.0.  An empty value leaves `out` alone. */
typedef int32_t (FS98_STDCALL *fs98_host_profile_doubles_fn)(const char *section,
                                                             const char *key, double *out,
                                                             uint32_t n, const char *path,
                                                             int32_t ext);

/* +0x2b4 -- strdup through the host allocator (not zeroed, fatal flag);
 * null for null. */
typedef char *(FS98_STDCALL *fs98_host_strdup_fn)(const char *s);

/* The voice engine, +0x2cc..+0x2f4: DirectSound buffers behind 0x16c-byte
 * host blocks.  A handle is the block's address; every service but +0x2cc
 * and +0x2d0 first checks it against the host's list, and most return 0
 * early when the engine is off.  Errors are 0x9xxxx codes. */

/* +0x2cc -- 1 if the host's sound state (0x41a3cc) is 0, else 0.  The state
 * is 1 when there is no sound, and SOUND stays silent unless this says 1. */
typedef int32_t (FS98_STDCALL *fs98_host_sound_off_fn)(void);

/* +0x2d0 -- switch every voice on (1) or off (0). */
typedef int32_t (FS98_STDCALL *fs98_host_sound_enable_fn)(int32_t on);

/* +0x2d4 -- a new voice.  Types 2, 4 and 5 load the file `src` names
 * (.WAV appended); 6 and 7 copy the voice `src` is.  0 or an error; *out
 * is nulled on failure -- except that 0x90003, a buffer that would not be
 * created with no device present, comes back with the voice kept. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_load_fn)(const void *src, int32_t type,
                                                        uint32_t *out);

/* +0x2d8 -- append voice `src`'s samples to `dst`; with src and type 0 and
 * `final` 1, finish `dst` into a playable buffer.  0, or -1 for a bad
 * handle; 0 at once with no device. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_append_fn)(uint32_t src, int32_t type,
                                                          uint32_t dst, int32_t final);

/* +0x2dc -- release a voice: reference-counted, *h nulled.  0x90019 for a
 * bad handle, which is left as it was. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_free_fn)(uint32_t *h);

/* +0x2e0 -- play, looping if `loop`, at `freq` times the natural rate (0.0
 * keeps it).  A callback still pending from the last play is called first;
 * the new one is called with the handle when this one finishes -- or at
 * once with no device. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_play_fn)(uint32_t h, int32_t loop,
                                                        double freq,
                                                        void (FS98_STDCALL *done)(uint32_t h));

/* +0x2e4 -- the rate, as a multiple of the natural one. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_freq_fn)(uint32_t h, double freq);

/* +0x2ec -- the volume, 0..10000. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_volume_fn)(uint32_t h, int32_t v);

/* +0x2f0 -- the pan, -10000..10000. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_pan_fn)(uint32_t h, int32_t pan);

/* +0x2f4 -- stop. */
typedef int32_t (FS98_STDCALL *fs98_host_voice_stop_fn)(uint32_t h);

/* +0x2fc -- free a list's data.  Nothing else in the header is touched. */
typedef int32_t (FS98_STDCALL *fs98_host_list_free_fn)(struct fs98_host_list *l);

/* +0x308 -- call fn(element, ctx) for each element in order until one
 * returns non-zero, and return that (or 0).  Removals during the walk are
 * queued on a host-wide list and applied when it ends. */
typedef int32_t (FS98_STDCALL *fs98_host_list_walk_fn)(struct fs98_host_list *l, void *ctx,
                                                       int32_t (FS98_STDCALL *fn)(void *elem,
                                                                                  void *ctx));

/* +0x310 -- the first element whose `len` bytes at `offset` match `key`
 * (or, with `indirect`, whose pointer there points at bytes matching what
 * `key` points at); null if none. */
typedef void *(FS98_STDCALL *fs98_host_list_find_fn)(struct fs98_host_list *l, uint32_t offset,
                                                     const void *key, uint32_t len,
                                                     int32_t indirect);

#define FS98_HOST_S_BUSY           0x050
#define FS98_HOST_S_CFG_DOUBLE     0x0b8
#define FS98_HOST_S_CFG_PUT_DOUBLE 0x0bc
#define FS98_HOST_S_PROFILE_INT    0x0d0
#define FS98_HOST_S_PROFILE_DOUBLE 0x0e0
#define FS98_HOST_S_PROFILE_DOUBLES 0x0e8
#define FS98_HOST_S_STRDUP         0x2b4
#define FS98_HOST_S_SOUND_OFF      0x2cc
#define FS98_HOST_S_SOUND_ENABLE   0x2d0
#define FS98_HOST_S_VOICE_LOAD     0x2d4
#define FS98_HOST_S_VOICE_APPEND   0x2d8
#define FS98_HOST_S_VOICE_FREE     0x2dc
#define FS98_HOST_S_VOICE_PLAY     0x2e0
#define FS98_HOST_S_VOICE_FREQ     0x2e4
#define FS98_HOST_S_VOICE_VOLUME   0x2ec
#define FS98_HOST_S_VOICE_PAN      0x2f0
#define FS98_HOST_S_VOICE_STOP     0x2f4
#define FS98_HOST_S_LIST_FREE      0x2fc
#define FS98_HOST_S_LIST_WALK      0x308
#define FS98_HOST_S_LIST_FIND      0x310

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

/* ---- windows: +0x020..+0x094 (confirmed by WINDOW) ------------------------- */

/* A window is a caller's 0xa4-byte record (see docs/modules/window.md); the
 * host keeps a 0x90-byte block of its own at record +0xa0, and the record's
 * render size at +0x84/+0x88 and surface size at +0x60/+0x64, which its
 * WM_SIZE handler sets: halved for +0x10000, clamped by +0x68..+0x74, made
 * odd for a record with a view block (+0x1c) that is not a map (type 2). */

/* +0x020 -- a host global (0x41719c); WINDOW paints nothing while it is 0. */
typedef int32_t (FS98_STDCALL *fs98_host_active_fn)(void);

/* +0x064 -- give the record its native windows (CreateWindowExA twice) and
 * put it at the front of its group (+0x78) in the host's z-order.  -1 for a
 * null record or a failed create, else 0. */
typedef int32_t (FS98_STDCALL *fs98_host_win_create_fn)(void *rec);

/* +0x068 -- out of the z-order, DestroyWindow, free the block. */
typedef int32_t (FS98_STDCALL *fs98_host_win_destroy_fn)(void *rec);

/* +0x06c / +0x070 -- the DC for painting the record, and its release. */
typedef int32_t (FS98_STDCALL *fs98_host_paint_begin_fn)(void *rec);
typedef int32_t (FS98_STDCALL *fs98_host_paint_end_fn)(void *rec);

/* +0x07c -- message 0x467 and SetWindowTextA: the record's flags and
 * rectangle (+0x20..+0x2c: x and width in 0x2000ths of the screen's width,
 * y and height in 0x1800ths of its height) taken up. */
typedef int32_t (FS98_STDCALL *fs98_host_win_update_fn)(void *rec);

/* +0x080 -- SetWindowTextA from the record's title, on both windows. */
typedef int32_t (FS98_STDCALL *fs98_host_win_title_fn)(void *rec);

/* +0x084 -- message 0x466 if the record is shown (+0x08 bit 31). */
typedef int32_t (FS98_STDCALL *fs98_host_win_flush_fn)(void *rec);

/* +0x08c -- draw `text` in the record at place `where` (0..0xe: corners,
 * edges and middle) in colour `colour`, opaque or not.  -1 for a null
 * record, else 0. */
typedef int32_t (FS98_STDCALL *fs98_host_text_fn)(void *rec, uint32_t where, uint32_t colour,
                                                  const char *text, int32_t opaque);

/* +0x090 -- to the front of its group.  -1 if it is not in the z-order. */
typedef int32_t (FS98_STDCALL *fs98_host_raise_fn)(void *rec);

/* +0x094 -- the nth shown record of group `group`, front first, into *out;
 * null and -1 if there are not that many. */
typedef int32_t (FS98_STDCALL *fs98_host_nth_fn)(void **out, uint32_t group, uint32_t n);

/* +0x244 -- a menu item `id` (only its low word is kept) under `command`
 * with `text`; +0x25c takes item `id` out, and +0x264 renames it.  Each
 * looks the item up by the whole 32 bits of `id`. */
typedef int32_t (FS98_STDCALL *fs98_host_menu_add_fn)(uint32_t command, uint32_t id,
                                                      const char *text, uint32_t zero);
typedef int32_t (FS98_STDCALL *fs98_host_menu_remove_fn)(uint32_t id);
typedef int32_t (FS98_STDCALL *fs98_host_menu_text_fn)(uint32_t id, const char *text);

/* +0x254 -- a menu item's state, by the whole 32 bits of `id`: flag 1
 * checks it, flag 2 greys it (CheckMenuItem / EnableMenuItem by position,
 * 0x40d3c0).  0, or -1 if there is no such item. */
typedef int32_t (FS98_STDCALL *fs98_host_menu_state_fn)(uint32_t id, uint32_t flags);

/* +0x054 -- the main window and the instance through the two pointers
 * (0x41880c, 0x418828).  Returns the instance. */
typedef uint32_t (FS98_STDCALL *fs98_host_window_instance_fn)(uint32_t *wnd,
                                                              uint32_t *instance);

/* +0x298 -- the module whose Linkage is `linkage` onto the host's stack of
 * running modules (0x4182e0), and its HMODULE through `out`.  -1 with *out
 * null if it is not loaded.  +0x29c pops the stack (its argument is
 * overwritten by the pop and otherwise unused); -1 if it was empty. */
typedef int32_t (FS98_STDCALL *fs98_host_module_enter_fn)(const void *linkage, uint32_t *out);
typedef int32_t (FS98_STDCALL *fs98_host_module_leave_fn)(uint32_t unused);

/* +0x2a4 -- string `id` (up to 0x400 bytes) as a host copy through `out`
 * (null if there is no such string); returns its length. */
typedef int32_t (FS98_STDCALL *fs98_host_string_fn)(uint32_t id, char **out);

#define FS98_HOST_S_ACTIVE      0x020
#define FS98_HOST_S_WIN_CREATE  0x064
#define FS98_HOST_S_WIN_DESTROY 0x068
#define FS98_HOST_S_PAINT_BEGIN 0x06c
#define FS98_HOST_S_PAINT_END   0x070
#define FS98_HOST_S_WIN_UPDATE  0x07c
#define FS98_HOST_S_WIN_TITLE   0x080
#define FS98_HOST_S_WIN_FLUSH   0x084
#define FS98_HOST_S_TEXT        0x08c
#define FS98_HOST_S_RAISE       0x090
#define FS98_HOST_S_NTH         0x094
#define FS98_HOST_S_MENU_ADD    0x244
#define FS98_HOST_S_MENU_REMOVE 0x25c
#define FS98_HOST_S_MENU_TEXT   0x264
#define FS98_HOST_S_STRING      0x2a4

/* Not a service: the sim's main loop runs under a __try (0x412140) whose
 * filter (0x4119a0) repairs integer divide faults.  It decodes the F6/F7
 * (and a 0x66 prefix), saturates -- div: all ones; idiv: 0x7f..ff, or
 * 0x80..01 when the divisor and the dividend's top half differ in sign,
 * the remainder 0x7f..ff -- and carries on after the instruction.  Retail
 * modules divide into overflow and rely on it; see docs/modules/window.md. */

/* Extension ids, as the host's table at 0x417f20 has them. */
#define FS98_HOST_EXT_AIR  0x0f
#define FS98_HOST_EXT_WAV  0x13
#define FS98_HOST_EXT_MDL  0x14
#define FS98_HOST_EXT_SIM  0x1b
#define FS98_HOST_EXT_CFG  0x24

/* ---- reaching them ------------------------------------------------------- */

/* A slot holds a function pointer, so fetching one is a load from
 * host + slot.  Use the typed wrappers rather than casting at each site. */
#define FS98_HOST_SLOT(h, slot, type) \
    (*(type *)((unsigned char *)(h) + (slot)))

#define FS98_HOST_TICKS(h)      FS98_HOST_SLOT(h, FS98_HOST_S_TICKS,      fs98_host_ticks_fn)
#define FS98_HOST_POST_COMMAND(h) FS98_HOST_SLOT(h, FS98_HOST_S_POST_COMMAND, fs98_host_post_command_fn)
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
#define FS98_HOST_MEMCMP(h)      FS98_HOST_SLOT(h, FS98_HOST_S_MEMCMP,      fs98_host_memcmp_fn)
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
#define FS98_HOST_BUSY(h)        FS98_HOST_SLOT(h, FS98_HOST_S_BUSY,        fs98_host_busy_fn)
#define FS98_HOST_CFG_DOUBLE(h)  FS98_HOST_SLOT(h, FS98_HOST_S_CFG_DOUBLE,  fs98_host_cfg_double_fn)
#define FS98_HOST_CFG_PUT_DOUBLE(h) FS98_HOST_SLOT(h, FS98_HOST_S_CFG_PUT_DOUBLE, fs98_host_cfg_put_double_fn)
#define FS98_HOST_PROFILE_INT(h) FS98_HOST_SLOT(h, FS98_HOST_S_PROFILE_INT, fs98_host_profile_int_fn)
#define FS98_HOST_PROFILE_DOUBLE(h) FS98_HOST_SLOT(h, FS98_HOST_S_PROFILE_DOUBLE, fs98_host_profile_double_fn)
#define FS98_HOST_PROFILE_DOUBLES(h) FS98_HOST_SLOT(h, FS98_HOST_S_PROFILE_DOUBLES, fs98_host_profile_doubles_fn)
#define FS98_HOST_STRDUP(h)      FS98_HOST_SLOT(h, FS98_HOST_S_STRDUP,      fs98_host_strdup_fn)
#define FS98_HOST_SOUND_OFF(h)   FS98_HOST_SLOT(h, FS98_HOST_S_SOUND_OFF,   fs98_host_sound_off_fn)
#define FS98_HOST_SOUND_ENABLE(h) FS98_HOST_SLOT(h, FS98_HOST_S_SOUND_ENABLE, fs98_host_sound_enable_fn)
#define FS98_HOST_VOICE_LOAD(h)  FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_LOAD,  fs98_host_voice_load_fn)
#define FS98_HOST_VOICE_APPEND(h) FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_APPEND, fs98_host_voice_append_fn)
#define FS98_HOST_VOICE_FREE(h)  FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_FREE,  fs98_host_voice_free_fn)
#define FS98_HOST_VOICE_PLAY(h)  FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_PLAY,  fs98_host_voice_play_fn)
#define FS98_HOST_VOICE_FREQ(h)  FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_FREQ,  fs98_host_voice_freq_fn)
#define FS98_HOST_VOICE_VOLUME(h) FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_VOLUME, fs98_host_voice_volume_fn)
#define FS98_HOST_VOICE_PAN(h)   FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_PAN,   fs98_host_voice_pan_fn)
#define FS98_HOST_VOICE_STOP(h)  FS98_HOST_SLOT(h, FS98_HOST_S_VOICE_STOP,  fs98_host_voice_stop_fn)
#define FS98_HOST_LIST_FREE(h)   FS98_HOST_SLOT(h, FS98_HOST_S_LIST_FREE,   fs98_host_list_free_fn)
#define FS98_HOST_LIST_WALK(h)   FS98_HOST_SLOT(h, FS98_HOST_S_LIST_WALK,   fs98_host_list_walk_fn)
#define FS98_HOST_LIST_FIND(h)   FS98_HOST_SLOT(h, FS98_HOST_S_LIST_FIND,   fs98_host_list_find_fn)

/* The alloc flag that means "give me -1, I will handle it". */
#define FS98_HOST_ALLOC_NONFATAL 4

#endif /* FS98_HOST_H */
