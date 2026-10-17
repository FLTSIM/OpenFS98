/* BOOSTER.DLL -- module id 34: the performance boosters, CD scenery cached
 * to disk.
 *
 * FE's frame callback hands BOOSTER each booster area the aircraft is in
 * (+0x18) and takes it back when the aircraft leaves (+0x1c).  For each one
 * BOOSTER reads the cache directory's bstinfo file, a list of records that
 * say which scenery and texture files on the CD cover which boxes, and queues
 * every file whose box is in reach, nearest first.  A worker thread copies
 * them from the CD to the cache, registering each new BGL with FE +0x70.
 * Files that fall out of reach again are deleted (and unregistered, +0x74),
 * and the cache is held under the size the world file gives it.  The cache
 * is rescanned every frame, one booster a frame, and on four CHAIN list 0x14
 * messages.
 *
 * Retail is MSVC C++ (operator new and a template priority queue) with a
 * worker thread, a mutex the thread holds while it copies, a critical section
 * around the queue, and an event the main thread waits on until the queue
 * drains.  This is a transliteration of all of it.  The complete 0x904-byte
 * .data image is kept at retail's offsets so the harness can compare it.
 *
 * In an FE booster record (0x220 bytes; see src/fe/fe.c):
 *   +0x012  the cache directory -- bstinfo is read here, files copied to it
 *   +0x116  the source directory on the CD
 *   +0x21a  0 or 1: the cache is emptied when its booster is taken back
 *   +0x21c  the cache's size, in KB
 *   +0x21f  1 once copying is cancelled; BOOSTER stops scanning it
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define BOOSTER_IMAGE_SIZE 0x904
#define BOOSTER_IMPORTS    0x000
#define BOOSTER_LINKAGE    0x038

typedef uint16_t __attribute__((may_alias)) booster_u16;
typedef uint32_t __attribute__((may_alias)) booster_u32;

struct booster_image {
    uint8_t bytes[BOOSTER_IMAGE_SIZE];
};

extern struct booster_image booster_image;

#define D              (booster_image.bytes)
#define D8(offset)     (*(uint8_t *)(D + (offset)))
#define D16(offset)    (*(booster_u16 *)(D + (offset)))
#define D32(offset)    (*(booster_u32 *)(D + (offset)))
#define DPTR(offset)   (*(void **)(D + (offset)))

#define IMPORT(index)  (*(void **)(D + BOOSTER_IMPORTS + 8 * (index) + 4))
#define HOST           IMPORT(0)
#define G              ((uint8_t *)IMPORT(1))
#define G8(offset)     (*(uint8_t *)(G + (offset)))
#define G32(offset)    (*(booster_u32 *)(G + (offset)))
#define CHAIN          IMPORT(2)
#define UTIL           IMPORT(4)
#define FE             IMPORT(5)

#define SLOT(module, offset, type) \
    (*(type *)((uint8_t *)(module) + (offset)))

#define P8(p, offset)  (*(uint8_t *)((uint8_t *)(p) + (offset)))
#define P16(p, offset) (*(booster_u16 *)((uint8_t *)(p) + (offset)))
#define P32(p, offset) (*(booster_u32 *)((uint8_t *)(p) + (offset)))

/* ---- the state, at retail's offsets ---------------------------------------- */

#define B_BSTINFO   ((char *)D + 0x058)     /* "bstinfo" */
#define B_FACTORS   ((const double *)(D + 0x060))   /* 0.5, 1, 1.18, 1.36, 1.54 */
#define B_BACKSLASH ((char *)D + 0x0b0)
#define B_FMT_ERROR ((char *)D + 0x0b4)     /* "%s\n%s" */
#define B_FMT_DEST  ((char *)D + 0x0bc)     /* "%s%s%s%s" */
#define B_FMT_NAME  ((char *)D + 0x0c8)     /* "%c%.3u%.3u" */
#define B_FMT_SRC   ((char *)D + 0x0d4)     /* "%s%s%s%.3u%\%c%.3u%.3u%s" */
#define B_FILES     ((char *)D + 0x0f0)
#define B_GRID      ((char *)D + 0x0f8)
#define B_EXT_TXR   ((char *)D + 0x100)
#define B_EXT_BGL   ((char *)D + 0x108)
#define B_EXT_PAL   ((char *)D + 0x110)
#define B_EXT_HAZ   ((char *)D + 0x118)
#define B_TEXTURE   ((char *)D + 0x120)     /* "\TEXTURE\" */
#define B_ENTRY     ((struct fs98_host_find_entry *)(D + 0x228))
#define B_CAPTION   ((char *)D + 0x338)     /* string 3 */
#define B_THREAD    DPTR(0x358)
#define B_THREAD_ID D32(0x35c)
#define B_EVENT     DPTR(0x360)
#define B_DEST      ((char *)D + 0x368)     /* the cache file */
#define B_CS        ((LPCRITICAL_SECTION)(D + 0x470))
#define B_IS_BGL    D32(0x488)
#define B_SOURCE    ((char *)D + 0x490)     /* the CD file */
#define B_MUTEX     DPTR(0x594)
#define B_SCENERY   ((char *)D + 0x598)     /* "\SCENERY\" */
#define B_ERROR     ((char *)D + 0x6a0)     /* string 0xf429 */
#define B_NAME      ((char *)D + 0x7a0)     /* the file's name, no extension */
#define B_CUR       (*(struct booster_node **)(D + 0x8a0))
#define B_RING      (*(struct booster_node **)(D + 0x8a4))
#define B_FIND      (*(struct fs98_host_find **)(D + 0x8a8))
#define B_SOUTH     D32(0x8ac)
#define B_NORTH     D32(0x8b0)
#define B_WEST      D32(0x8b4)
#define B_EAST      D32(0x8b8)
#define B_ALT_LO    D32(0x8bc)
#define B_ALT_HI    D32(0x8c0)
#define B_RADIUS    D32(0x8c4)
#define B_INDEX     D16(0x8c8)              /* the file's number */
#define B_KIND      D16(0x8cc)              /* type << 12 | directory number */
#define B_GRIDDED   D16(0x8d0)              /* G names, GRID directories */
#define B_DIST      (D + 0x8d8)             /* a double: the distance squared */
#define B_QUEUE     (*(struct booster_queue **)(D + 0x8e4))
#define B_REC       (*(uint8_t **)(D + 0x8e8))  /* the bstinfo record */
#define B_GOT       D32(0x8ec)
#define B_ADDED     D32(0x8f4)              /* a BGL was registered */
#define B_TICK      D32(0x8f8)

#define WM_BOOSTER  0x401

/* One booster BOOSTER has, in a ring.  0x24 bytes, from the host's
 * allocator, not zeroed. */
struct booster_node {
    uint8_t *rec;                   /* FE's record */
    uint32_t area;                  /* FE's area, for +0x70/+0x74 */
    uint8_t *info;                  /* the cache's bstinfo, whole */
    uint32_t levels;                /* quota * 2 / info +0x16, less 1 */
    uint32_t used;                  /* bytes in the cache */
    uint32_t free_lo;               /* free space on its drive */
    uint32_t free_hi;
    uint8_t level;                  /* how far the reach has shrunk, 0..5 */
    uint8_t count;                  /* frames since the cache filled */
    uint16_t pad;
    struct booster_node *next;
};

/* One file to copy.  0x14 bytes, from operator new. */
struct __attribute__((packed)) booster_job {
    struct booster_node *node;      /* null once its booster is taken back */
    uint8_t *rec;
    uint8_t dist[8];                /* the queue's key, a double */
    uint16_t index;
};

/* The priority queue, nearest first.  data[1] is the first element. */
struct booster_queue {
    uint32_t capacity;
    uint32_t grow;
    uint32_t count;
    struct booster_job **data;
    uint32_t flags;                 /* 1: has data; 2: owns the jobs */
};

typedef char assert_node_size[sizeof(struct booster_node) == 0x24 ? 1 : -1];
typedef char assert_job_size[sizeof(struct booster_job) <= 0x14 ? 1 : -1];
typedef char assert_queue_size[sizeof(struct booster_queue) == 0x14 ? 1 : -1];

/* ---- imports ---------------------------------------------------------------- */

typedef void (FS98_STDCALL *booster_callback_fn)(uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *chain_change_fn)(uint32_t, booster_callback_fn);
typedef void (FS98_STDCALL *host_void_fn)(void);
typedef int32_t (FS98_STDCALL *host_size_fn)(struct fs98_host_file *, uint32_t *);
typedef int32_t (FS98_STDCALL *host_delete_fn)(const char *, int32_t);
typedef int32_t (FS98_STDCALL *host_free_space_fn)(const char *, uint32_t *);
typedef void (FS98_STDCALL *util_convert_fn)(const void *, const void *, void *);
typedef uint32_t (FS98_STDCALL *util_alert_fn)(uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t);
typedef void (FS98_STDCALL *util_alert_open_fn)(void **, uint32_t, uint32_t,
                                                uint32_t, uint32_t, uint32_t,
                                                uint32_t, uint32_t);
typedef void (FS98_STDCALL *util_alert_close_fn)(void **);
typedef uint32_t (FS98_STDCALL *fe_file_fn)(uint32_t, const char *);
typedef void (FS98_STDCALL *booster_each_fn)(const char *, int32_t);

#define CHAIN_ADD       SLOT(CHAIN, 0x18, chain_change_fn)
#define CHAIN_REMOVE    SLOT(CHAIN, 0x1c, chain_change_fn)
#define H_BUSY_ON       SLOT(HOST, 0x02c, host_void_fn)
#define H_BUSY_OFF      SLOT(HOST, 0x030, host_void_fn)
#define H_CLOSE         FS98_HOST_FILE_CLOSE(HOST)
#define H_DELETE        SLOT(HOST, 0x14c, host_delete_fn)
#define H_FREE_SPACE    SLOT(HOST, 0x150, host_free_space_fn)
#define H_FIND_CLOSE    FS98_HOST_FIND_CLOSE(HOST)
#define H_FIND_NEXT     FS98_HOST_FIND_NEXT(HOST)
#define H_FIND_FIRST    FS98_HOST_FIND_FIRST(HOST)
#define H_OPEN          FS98_HOST_FILE_OPEN(HOST)
#define H_JOIN          FS98_HOST_PATH_JOIN(HOST)
#define H_SPLIT         FS98_HOST_PATH_SPLIT(HOST)
#define H_READ          FS98_HOST_FILE_READ(HOST)
#define H_SIZE          SLOT(HOST, 0x1a4, host_size_fn)
#define H_ALLOC         FS98_HOST_ALLOC(HOST)
#define H_REALLOC       FS98_HOST_REALLOC(HOST)
#define H_FREE          FS98_HOST_FREE(HOST)
#define H_LOADSTRING    FS98_HOST_LOADSTRING(HOST)
#define H_STRCPY        FS98_HOST_STRCPY(HOST)
#define H_STRCAT        FS98_HOST_STRCAT(HOST)
#define UTIL_TO_LON     SLOT(UTIL, 0x44, util_convert_fn)   /* metres to longitude */
#define UTIL_TO_M       SLOT(UTIL, 0x48, util_convert_fn)   /* and back */
#define UTIL_ALERT      SLOT(UTIL, 0xa4, util_alert_fn)
#define UTIL_ALERT_OPEN SLOT(UTIL, 0xa8, util_alert_open_fn)
#define UTIL_ALERT_CLOSE SLOT(UTIL, 0xac, util_alert_close_fn)
#define FE_REGISTER     SLOT(FE, 0x70, fe_file_fn)
#define FE_UNREGISTER   SLOT(FE, 0x74, fe_file_fn)

/* Retail allocates the queue and its jobs with MSVCRT's operator new/delete.
 * mingw's import library does not carry those mangled names, so they are
 * bound by name; the routines are the ones retail calls. */
typedef void *(__cdecl *booster_new_fn)(uint32_t);
typedef void (__cdecl *booster_delete_fn)(void *);
static booster_new_fn booster_new;
static booster_delete_fn booster_delete;

static void booster_bind_crt(void)
{
    HMODULE crt;

    if (booster_new != NULL)
        return;
    crt = LoadLibraryA("MSVCRT.dll");
    booster_new = (booster_new_fn)(void *)GetProcAddress(crt, "??2@YAPAXI@Z");
    booster_delete = (booster_delete_fn)(void *)GetProcAddress(crt,
                                                         "??3@YAXPAX@Z");
}

/* MSVCRT's _ftol: truncation towards zero, 64 bits, the x87's indefinite
 * integer out of range. */
static int64_t booster_ftol(long double value)
{
    if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;
    return (int64_t)value;
}

/* A 48-bit value here is the low word at +2 and the high dword at +4. */
static void booster_set48(uint8_t *value, uint32_t high)
{
    P16(value, 0) = 0;
    P16(value, 2) = 0;
    P32(value, 4) = high;
}

/* The fcomp tests: C0 is set when the left is below the right or the two are
 * unordered. */
static int booster_below(const uint8_t *left, const uint8_t *right)
{
    double a, b;

    __builtin_memcpy(&a, left, 8);
    __builtin_memcpy(&b, right, 8);
    return !(a >= b);
}

static void booster_scan(void);
static void booster_enqueue(void);
static uint32_t booster_remove_file(void);
static uint32_t booster_remove_pair(void);
static uint32_t booster_copy_one(void);
static uint32_t booster_in_range(void);
static void FS98_STDCALL booster_frame(uint32_t first, uint32_t second);
static void FS98_STDCALL booster_message(uint32_t message, uint32_t second);
static DWORD WINAPI booster_thread(LPVOID unused);

/* ---- init and deinit: Linkage +4, +8 ---------------------------------------- */

/* .text+0x20.  Retail registers an exception frame for the queue's
 * construction; nothing in it can throw, so it is left out. */
static void booster_init(void)
{
    struct booster_queue *queue;
    char text[0x108];

    booster_bind_crt();
    CHAIN_ADD(3, booster_frame);
    CHAIN_ADD(0x14, booster_message);
    queue = (struct booster_queue *)booster_new(0x14);
    if (queue != NULL) {
        uint32_t flags = queue->flags;

        queue->capacity = 0x200;
        queue->grow = 0x80;
        queue->count = 0;
        queue->data = NULL;
        queue->flags = (flags & ~2u) | 1;
        H_ALLOC(1, 0x800, (void **)&queue->data, 0);
        queue->data--;
    }
    B_QUEUE = queue;
    H_LOADSTRING(0x9003, text, 0x104);
    H_STRCPY(B_SCENERY, B_BACKSLASH);
    H_STRCAT(B_SCENERY, text);
    H_STRCAT(B_SCENERY, B_BACKSLASH);
    H_LOADSTRING(0x9006, text, 0x104);
    H_STRCPY(B_TEXTURE, B_BACKSLASH);
    H_STRCAT(B_TEXTURE, text);
    H_STRCAT(B_TEXTURE, B_BACKSLASH);
    InitializeCriticalSection(B_CS);
    B_MUTEX = CreateMutexA(NULL, FALSE, NULL);
    B_EVENT = CreateEventA(NULL, FALSE, FALSE, NULL);
    B_THREAD = CreateThread(NULL, 0, booster_thread, NULL, 0,
                            (DWORD *)(D + 0x35c));
    H_LOADSTRING(0xf429, B_ERROR, 0x100);
    H_LOADSTRING(3, B_CAPTION, 0x20);
}

static void booster_flush(void);

/* .text+0x220.  The queue's pointer is left stale, and its jobs are not
 * deleted: bit 1 is never set. */
static void booster_deinit(void)
{
    struct booster_queue *queue;
    struct booster_node *start, *next;

    PostThreadMessageA(B_THREAD_ID, WM_QUIT, 0, 0);
    WaitForSingleObject(B_THREAD, INFINITE);
    CloseHandle(B_THREAD);
    CloseHandle(B_EVENT);
    CloseHandle(B_MUTEX);
    DeleteCriticalSection(B_CS);
    CHAIN_REMOVE(3, booster_frame);
    CHAIN_REMOVE(0x14, booster_message);
    queue = B_QUEUE;
    if (queue != NULL) {
        if (queue->flags & 1) {
            queue->data++;
            if (queue->flags & 2) {
                for (;;) {
                    uint32_t count = queue->count;

                    queue->count = count - 1;
                    if (count == 0)
                        break;
                    booster_delete(queue->data[count - 1]);
                }
            }
            H_FREE((void **)&queue->data);
        }
        booster_delete(queue);
    }
    if (B_RING != NULL) {
        start = B_RING;
        do {
            if (P8(B_RING->rec, 0x21a) == 0) {
                B_CUR = B_RING;
                booster_flush();
            }
            H_FREE((void **)&B_RING->info);
            next = B_RING->next;
            H_FREE((void **)&B_RING);
            B_RING = next;
        } while (next != start);
    }
}

/* ---- the cache directory ------------------------------------------------------ */

/* .text+0xc00 -- every file in `dir` with extension id `ext` through `each`,
 * as dir + name and its extension id.  An extension the host has not
 * registered leaves the entry's type as the last file's. */
static void FS98_STDCALL booster_each_file(const char *dir, int32_t ext,
                                           booster_each_fn each)
{
    char path[0x100];

    if (H_FIND_FIRST(&B_FIND, dir, NULL, ext) == 0 &&
        H_FIND_NEXT(B_FIND, B_ENTRY) == 0) {
        do {
            if (B_ENTRY->type != -2) {
                H_JOIN(path, NULL, dir, B_ENTRY->name);
                each(path, B_ENTRY->type);
            }
        } while (H_FIND_NEXT(B_FIND, B_ENTRY) == 0);
    }
    H_FIND_CLOSE(&B_FIND);
}

/* .text+0xcc0 -- the same over the cache's SCENERY, then TEXTURE. */
static void FS98_STDCALL booster_each_cached(int32_t ext, booster_each_fn each)
{
    char dir[0x104];

    H_JOIN(dir, NULL, (char *)B_CUR->rec + 0x12, B_SCENERY);
    booster_each_file(dir, ext, each);
    H_JOIN(dir, NULL, (char *)B_CUR->rec + 0x12, B_TEXTURE);
    booster_each_file(dir, ext, each);
}

/* .text+0xba0 -- add one cached file's size. */
static void FS98_STDCALL booster_add_size(const char *path, int32_t ext)
{
    struct fs98_host_file *file;
    uint32_t size;

    if (H_OPEN(&file, path, ext, 0) == 0) {
        if (H_SIZE(file, &size) == 0)
            B_CUR->used += size;
        H_CLOSE(file);
    }
}

/* .text+0xad0 -- delete one cached file.  A BGL is unregistered first if
 * its name is one of BOOSTER's own, F or G and six digits; a BGL by any
 * other name is left alone. */
static void FS98_STDCALL booster_delete_cached(const char *path, int32_t ext)
{
    char name[0x100];
    int i;

    if (ext == 1) {
        H_SPLIT(path, NULL, NULL, name, NULL);
        if (name[0] != 'f' && name[0] != 'g' && name[0] != 'F' &&
            name[0] != 'G')
            return;
        for (i = 1; i < 7; i++)
            if (name[i] < '0' || name[i] > '9')
                return;
        if (name[7] != 0)
            return;
        FE_UNREGISTER(B_CUR->area, name);
    }
    H_DELETE(path, ext);
}

/* .text+0xaa0 -- empty the cache, unless GLOBAL +0x444 has 0x10 without
 * 0x20.  FE is told to look again (bit 8). */
static void booster_flush(void)
{
    uint16_t flags = *(booster_u16 *)(G + 0x444);

    if (!(flags & 0x20) && (flags & 0x10))
        return;
    booster_each_cached(-1, booster_delete_cached);
    G8(0x444) |= 8;
}

/* .text+0x930 -- the cache drive's free space; 0xffff if it will not say. */
static void booster_free_space(void)
{
    if (H_FREE_SPACE((char *)B_CUR->rec + 0x12, &B_CUR->free_lo) != 0) {
        B_CUR->free_hi = 0;
        B_CUR->free_lo = 0xffff;
    }
}

/* .text+0x860 -- read the cache's bstinfo, whole. */
static void booster_read_info(void)
{
    struct fs98_host_file *file = NULL;
    uint32_t size;
    char path[0x104];

    H_JOIN(path, NULL, (char *)B_CUR->rec + 0x12, B_BSTINFO);
    if (H_OPEN(&file, path, 0, 0) != 0)
        return;
    H_SIZE(file, &size);
    if (H_ALLOC(1, size, (void **)&B_CUR->info, 0) == 0)
        H_READ(file, B_CUR->info, size, (uint32_t *)(D + 0x8ec));
    H_CLOSE(file);
}

/* ---- +0x18, +0x1c: FE hands a booster over and takes it back ----------------- */

/* .text+0x360 -- add booster `rec`, in FE area `area`: scan it, and wait
 * while the thread copies what is in reach.  A cache short of its quota's
 * free space says so ("not enough disk space", 0xf421). */
static void FS98_STDCALL booster_add(uint8_t *rec, uint32_t area)
{
    struct booster_node *node;
    struct booster_node *cur;
    void *alert;
    uint16_t width;

    H_BUSY_ON();
    UTIL_ALERT_OPEN(&alert, 0, 0xf420, 0, 0, 0, 0, 0);
    WaitForSingleObject(B_MUTEX, INFINITE);
    H_ALLOC(1, 0x24, (void **)&node, 0);
    if (B_RING == NULL) {
        B_CUR = node;
        node->next = node;
        B_RING = B_CUR;
    } else {
        node->next = B_RING->next;
        B_RING->next = node;
    }
    B_CUR = B_RING->next;
    B_CUR->rec = rec;
    B_CUR->area = area;
    B_CUR->used = 0;
    booster_each_cached(-1, booster_add_size);
    booster_free_space();
    B_CUR->level = 0;
    B_CUR->count = 0;
    booster_read_info();
    cur = B_CUR;
    width = P16(cur->info, 0x16);
    if (width == 0)
        cur->levels = 1;
    else
        cur->levels = (uint32_t)((int32_t)((uint32_t)P16(cur->rec, 0x21c) << 1) /
                                 (int32_t)width - 1);
    booster_scan();
    cur = B_CUR;
    if (cur->free_hi == 0 &&
        ((uint32_t)P16(cur->rec, 0x21c) << 10) > cur->free_lo)
        UTIL_ALERT(2, 0, 0xf421, 0, 0, 0, 0, 1);
    ResetEvent(B_EVENT);
    PostThreadMessageA(B_THREAD_ID, WM_BOOSTER, 0, 0);
    ReleaseMutex(B_MUTEX);
    WaitForSingleObject(B_EVENT, INFINITE);
    UTIL_ALERT_CLOSE(&alert);
    H_BUSY_OFF();
    if (B_ADDED != 0) {
        B_ADDED = 0;
        G8(0x444) |= 4;
    }
}

/* .text+0x520 */
static void FS98_STDCALL booster_unlink_job(struct booster_job *job,
                                            struct booster_node *node)
{
    if (job->node == node)
        job->node = NULL;
}

/* .text+0x540 -- take booster `rec` back: its cache emptied if +0x21a is 0
 * or 1, its queued files dropped.  The thread is woken, not waited for.
 * Not found leaves the node after the ring's head current. */
static void FS98_STDCALL booster_drop(uint8_t *rec)
{
    struct booster_node *ring, *prev;
    struct booster_queue *queue;
    struct booster_node *node;
    uint32_t i;
    uint8_t keep;

    WaitForSingleObject(B_MUTEX, INFINITE);
    ring = B_RING;
    prev = ring;
    B_CUR = ring->next;
    while (B_CUR->rec != rec) {
        prev = B_CUR;
        B_CUR = B_CUR->next;
        if (prev == ring)
            goto wake;
    }
    keep = P8(B_CUR->rec, 0x21a);
    if (keep == 0 || keep == 1)
        booster_flush();
    H_FREE((void **)&B_CUR->info);
    queue = B_QUEUE;
    node = B_CUR;
    for (i = 1; i <= queue->count; i++)
        booster_unlink_job(queue->data[i], node);
    prev->next = B_CUR->next;
    if (prev == B_CUR)
        B_RING = NULL;
    else if (B_RING == B_CUR)
        B_RING = B_CUR->next;
    H_FREE((void **)&B_CUR);
wake:
    if (B_QUEUE->count != 0)
        PostThreadMessageA(B_THREAD_ID, WM_BOOSTER, 0, 0);
    ReleaseMutex(B_MUTEX);
}

/* ---- the CHAIN callbacks -------------------------------------------------------- */

/* .text+0x670 -- CHAIN list 3, every frame: rescan one booster and move the
 * ring on.  Skipped while the thread is copying.  Bit 8 of GLOBAL +0x444 is
 * set at most every 0x96 ticks of +0x2ac once a BGL has been registered. */
static void FS98_STDCALL booster_frame(uint32_t first, uint32_t second)
{
    (void)first;
    (void)second;
    if (B_RING == NULL)
        return;
    if (WaitForSingleObject(B_MUTEX, 0) == WAIT_TIMEOUT)
        return;
    if (B_ADDED != 0 && G32(0x2ac) > B_TICK) {
        G8(0x444) |= 8;
        B_ADDED = 0;
        B_TICK = G32(0x2ac) + 0x96;
    }
    if (P8(B_RING->rec, 0x21f) == 0) {
        B_CUR = B_RING;
        booster_scan();
    }
    B_RING = B_RING->next;
    if (B_QUEUE->count != 0)
        PostThreadMessageA(B_THREAD_ID, WM_BOOSTER, 0, 0);
    ReleaseMutex(B_MUTEX);
}

/* .text+0x740 -- rescan every booster and wait while the thread copies. */
static void booster_rescan_all(void)
{
    struct booster_node *start;

    if (B_RING == NULL)
        return;
    WaitForSingleObject(B_MUTEX, INFINITE);
    start = B_RING;
    do {
        if (P8(B_RING->rec, 0x21f) == 0) {
            B_CUR = B_RING;
            booster_scan();
        }
        B_RING = B_RING->next;
    } while (B_RING != start);
    ResetEvent(B_EVENT);
    PostThreadMessageA(B_THREAD_ID, WM_BOOSTER, 0, 0);
    ReleaseMutex(B_MUTEX);
    WaitForSingleObject(B_EVENT, INFINITE);
}

/* .text+0x7d0 -- CHAIN list 0x14: messages 0x21 and 0x33..0x35 rescan
 * everything at once. */
static void FS98_STDCALL booster_message(uint32_t message, uint32_t second)
{
    void *alert;

    (void)second;
    if (B_RING == NULL)
        return;
    if (message != 0x21 && (message <= 0x32 || message > 0x35))
        return;
    H_BUSY_ON();
    UTIL_ALERT_OPEN(&alert, 0, 0xf420, 0, 0, 0, 0, 0);
    booster_rescan_all();
    UTIL_ALERT_CLOSE(&alert);
    H_BUSY_OFF();
    if (B_ADDED != 0) {
        B_ADDED = 0;
        G8(0x444) |= 4;
    }
}

/* ---- scanning a booster's bstinfo ----------------------------------------------
 *
 * The records start at bstinfo +0x14's offset; each is a type byte and its
 * length (a word), and a zero type ends them:
 *   1, 5   a list of files around one point (5: each a BGL and a TXR)
 *   2, 4   a grid of cells, one file each (4: a BGL and a TXR)
 *   3      the altitude band the records after it apply to
 * Points and boxes are 24-bit: latitude at +3 (north) and +6 (south),
 * shifted left 1, longitude at +9 (east) and +0xc (west), shifted left 8.
 * +0xf is the reach, shifted left 8; +0x11 the kind, a type nibble and a
 * directory number.  A list's file numbers follow from +0x13, bit 0x2000
 * set once cached; a grid's columns and rows are at +0x13/+0x15 and its
 * cells' bits from +0x17, a nibble a cell: 1 present, 2 cached. */

/* .text+0x1760 -- the reach, scaled down as the cache fills. */
static void booster_scale(void)
{
    uint32_t n = B_CUR->levels - B_CUR->level;

    if (n > 4)
        n = 4;
    B_RADIUS = (uint32_t)booster_ftol((long double)(uint64_t)B_RADIUS *
                                      (long double)B_FACTORS[n]);
}

/* .text+0x1d40 -- a type 1 or 5 record: queued whole when in reach, its
 * cached files deleted when out of reach by a quarter more. */
static void booster_point(void)
{
    uint8_t *r = B_REC;
    uint32_t step, n, i, e;

    B_KIND = P16(r, 0x11);
    B_GRIDDED = 0;
    B_RADIUS = (uint32_t)P16(r, 0xf) << 8;
    step = B_RADIUS >> 2;
    B_SOUTH = (((uint32_t)(int32_t)(int8_t)r[8] << 16) | P16(r, 6)) << 1;
    B_NORTH = (((uint32_t)(int32_t)(int8_t)r[5] << 16) | P16(r, 3)) << 1;
    B_WEST = (((uint32_t)r[0xe] << 16) | P16(r, 0xc)) << 8;
    B_EAST = (((uint32_t)r[0xb] << 16) | P16(r, 9)) << 8;
    booster_scale();
    if (booster_in_range()) {
        booster_enqueue();
        return;
    }
    B_RADIUS += step;
    if (booster_in_range())
        return;
    n = ((uint32_t)P16(B_REC, 1) - 0x13) >> 1;
    for (i = n; i-- != 0;) {
        uint16_t word = P16(B_REC, 0x13 + 2 * i);

        if (!(word & 0x2000))
            continue;
        B_INDEX = word & 0xfff;
        e = B_REC[0] == 1 ? booster_remove_file() : booster_remove_pair();
        if (e != 0)
            P16(B_REC, 0x13 + 2 * i) = P16(B_REC, 0x13 + 2 * i) | 0x2000;
        else
            P16(B_REC, 0x13 + 2 * i) = P16(B_REC, 0x13 + 2 * i) & 0xdfff;
    }
}

/* .text+0x1e70 -- delete a BGL and its TXR.  Always 0. */
static uint32_t booster_remove_pair(void)
{
    B_KIND = (B_KIND & 0xfff) | 0x1000;
    booster_remove_file();
    B_KIND &= 0xfff;
    booster_remove_file();
    return 0;
}

/* .text+0x2650 -- one cell of a grid, as booster_point does a record. */
static void booster_cell(void)
{
    uint32_t step = B_RADIUS >> 2;
    uint32_t e;
    uint8_t *bits;

    booster_scale();
    if (booster_in_range()) {
        booster_enqueue();
        return;
    }
    B_RADIUS += step;
    if (booster_in_range())
        return;
    bits = B_REC + 0x17 + (B_INDEX >> 1);
    if (!(*bits & ((B_INDEX & 1) ? 0x20 : 0x02)))
        return;
    e = B_REC[0] == 2 ? booster_remove_file() : booster_remove_pair();
    bits = B_REC + 0x17 + (B_INDEX >> 1);
    if (B_INDEX & 1)
        *bits = e != 0 ? (uint8_t)(*bits | 0x20) : (uint8_t)(*bits & 0xdf);
    else
        *bits = e != 0 ? (uint8_t)(*bits | 0x02) : (uint8_t)(*bits & 0xfd);
}

/* .text+0x24f0 -- a type 2 or 4 record: its present cells, row by row from
 * the north.  Columns and rows are words, so 0 of either is 65536; rows
 * divide signed, and columns unsigned. */
static void booster_grid(void)
{
    uint8_t *r = B_REC;
    uint32_t radius, north, south, west, east, lat_step, lon_step;
    int32_t rows_signed;
    uint16_t rows, columns;

    B_KIND = P16(r, 0x11);
    radius = P16(r, 0xf);
    B_GRIDDED = 1;
    north = (((uint32_t)(int32_t)(int8_t)r[5] << 16) | P16(r, 3)) << 1;
    south = (((uint32_t)(int32_t)(int8_t)r[8] << 16) | P16(r, 6)) << 1;
    columns = P16(r, 0x13);
    rows_signed = (int16_t)P16(r, 0x15);
    __asm__ ("cltd\n\tidivl %2"
             : "=a" (lat_step)
             : "0" (north - south), "rm" (rows_signed)
             : "edx", "cc");
    radius <<= 8;
    west = (((uint32_t)r[0xe] << 16) | P16(r, 0xc)) << 8;
    east = (((uint32_t)r[0xb] << 16) | P16(r, 9)) << 8;
    B_INDEX = 0;
    __asm__ ("xorl %%edx,%%edx\n\tdivl %2"
             : "=a" (lon_step)
             : "0" (east - west), "rm" ((uint32_t)columns)
             : "edx", "cc");
    B_NORTH = north;
    B_SOUTH = north;
    rows = P16(r, 0x15);
    do {
        south = north - lat_step;
        B_WEST = west;
        B_EAST = west;
        B_SOUTH = south;
        columns = P16(B_REC, 0x13);
        do {
            uint8_t mask;

            B_EAST = B_EAST + lon_step;
            mask = (B_INDEX & 1) ? 0x10 : 0x01;
            if (B_REC[0x17 + (B_INDEX >> 1)] & mask) {
                B_RADIUS = radius;
                booster_cell();
            }
            B_WEST = B_EAST;
            B_INDEX++;
        } while (--columns != 0);
        rows--;
        north = B_SOUTH;
        B_NORTH = north;
    } while (rows != 0);
}

/* .text+0x970 -- scan booster B_CUR.  A cache that is emptied when taken
 * back has its free space read again; once the cache has filled, the
 * counter runs to 60 frames and the reach shrinks a level, to 5, unless
 * the cache has room again and half a megabyte is free. */
static void booster_scan(void)
{
    struct booster_node *cur = B_CUR;
    uint8_t *info;

    B_ALT_LO = 0x80000000u;
    B_ALT_HI = 0x7fffffffu;
    if (P8(cur->rec, 0x21a) == 0) {
        booster_free_space();
        cur = B_CUR;
        if (cur->count != 0) {
            if (((uint32_t)P16(cur->rec, 0x21c) << 10) > cur->used &&
                (cur->free_hi != 0 || cur->free_lo > 0x80000)) {
                cur->level = 0;
                B_CUR->count = 0;
            }
            cur = B_CUR;
            if (cur->count != 0) {
                cur->count++;
                if (B_CUR->count >= 0x3c) {
                    B_CUR->level++;
                    if (B_CUR->level >= 5)
                        B_CUR->level = 5;
                    B_CUR->count = 1;
                }
            }
        }
    }
    info = B_CUR->info;
    B_REC = info + P16(info, 0x14);
    while (B_REC[0] != 0) {
        switch (B_REC[0]) {
        case 1:
        case 5:
            booster_point();
            break;
        case 2:
        case 4:
            booster_grid();
            break;
        case 3:
            B_ALT_LO = P32(B_REC, 3);
            B_ALT_HI = P32(B_REC, 7);
            break;
        default:
            return;
        }
        B_REC += P16(B_REC, 1);
    }
}

/* .text+0x2bd0 -- is the box SOUTH..NORTH, WEST..EAST within RADIUS of the
 * aircraft, inside the altitude band?  Latitude is compared against the
 * reach as it stands, longitude against it converted through UTIL at the
 * aircraft's latitude.  The distance squared, for the queue, is left in
 * B_DIST -- for a box level with the aircraft, the north edge's even when
 * the south edge is further. */
static uint32_t booster_in_range(void)
{
    uint8_t in[8], out[8], in2[8], out2[8];
    int32_t lat, dlat_s, dlat_n, dlon_w, dlon_e, reach;
    uint32_t lon, south, dlat, dlon;
    uint8_t quad;
    long double x, sum, r;
    double d;

    if ((int32_t)G32(0x574) < (int32_t)B_ALT_LO ||
        (int32_t)G32(0x574) > (int32_t)B_ALT_HI)
        return 0;
    lat = (int32_t)G32(0x564);
    dlat_s = (int32_t)(B_SOUTH - (uint32_t)lat);
    dlat_n = (int32_t)(B_NORTH - (uint32_t)lat);
    lon = G32(0x56c);
    dlon_w = (int32_t)(B_WEST - lon);
    dlon_e = (int32_t)(B_EAST - lon);
    booster_set48(in, B_RADIUS);
    UTIL_TO_LON(in, G + 0x560, out);
    reach = (int32_t)P32(out, 4);
    if (B_WEST >= G32(0x56c)) {
        lat = (int32_t)G32(0x564);
        south = B_SOUTH;
        quad = 1;
        if ((int32_t)south < lat) {
            quad = 2;
            if (lat < (int32_t)B_NORTH) {
                if (dlon_w > reach)
                    return 0;
                booster_set48(in2, (uint32_t)dlon_w);
                UTIL_TO_M(in2, G + 0x560, out2);
                goto edge;
            }
        }
    } else if (B_EAST > G32(0x56c)) {
        x = (long double)dlat_s;
        d = (double)(x * x);
        __builtin_memcpy(B_DIST, &d, 8);
        if (dlat_s > (int32_t)B_RADIUS)
            return 0;
        x = (long double)dlat_n;
        d = (double)(x * x);
        __builtin_memcpy(B_DIST, &d, 8);
        if (dlat_n < -(int32_t)B_RADIUS)
            return 0;
        return 1;
    } else {
        lat = (int32_t)G32(0x564);
        south = B_SOUTH;
        quad = 3;
        if ((int32_t)south < lat) {
            quad = 4;
            if (lat < (int32_t)B_NORTH) {
                if (-dlon_e > reach)
                    return 0;
                booster_set48(in2, (uint32_t)dlon_e);
                UTIL_TO_M(in2, G + 0x560, out2);
                goto edge;
            }
        }
    }
    /* A corner: latitude as it is, longitude in metres. */
    lon = G32(0x56c);
    switch (quad) {
    case 1:
        dlat = B_SOUTH - (uint32_t)lat;
        dlon = B_WEST - lon;
        break;
    case 2:
        dlat = (uint32_t)lat - B_NORTH;
        dlon = B_WEST - lon;
        break;
    case 3:
        dlat = B_SOUTH - (uint32_t)lat;
        dlon = lon - B_EAST;
        break;
    default:
        dlat = (uint32_t)lat - B_NORTH;
        dlon = lon - B_EAST;
        break;
    }
    booster_set48(in2, dlon);
    UTIL_TO_M(in2, G + 0x560, out2);
    x = (long double)P32(out2, 4);
    sum = (long double)dlat * (long double)dlat + x * x;
    r = (long double)B_RADIUS;
    if (sum > r * r)
        return 0;
    d = (double)sum;
    __builtin_memcpy(B_DIST, &d, 8);
    return 1;
edge:
    x = (long double)(int32_t)P32(out2, 4);
    d = (double)(x * x);
    __builtin_memcpy(B_DIST, &d, 8);
    return 1;
}

/* ---- the queue ------------------------------------------------------------------- */

/* .text+0xea0 -- queue the current file (or record), keyed on B_DIST. */
static void booster_enqueue(void)
{
    struct booster_job *job;
    struct booster_queue *queue;
    uint32_t i, parent;

    job = (struct booster_job *)booster_new(0x14);
    job->node = B_CUR;
    job->rec = B_REC;
    __builtin_memcpy(job->dist, B_DIST, 8);
    job->index = B_INDEX;
    EnterCriticalSection(B_CS);
    queue = B_QUEUE;
    if (queue->count == queue->capacity) {
        queue->capacity = queue->grow + queue->capacity;
        queue->data++;
        H_REALLOC(1, queue->capacity << 2, (void **)&queue->data);
        queue->data--;
    }
    i = ++queue->count;
    while (i > 1) {
        parent = i >> 1;
        if (!booster_below(job->dist, queue->data[parent]->dist))
            break;
        queue->data[i] = queue->data[parent];
        i = parent;
    }
    queue->data[i] = job;
    LeaveCriticalSection(B_CS);
}

/* .text+0x1ea0 -- move element i down to where it belongs.  An equal child
 * is taken over the parent. */
static void FS98_STDCALL booster_sift_down(struct booster_queue *queue,
                                           uint32_t i)
{
    for (;;) {
        uint32_t left = i * 2;
        uint32_t right = i * 2 + 1;
        uint32_t best = i;
        struct booster_job *swap;

        if (left <= queue->count &&
            !booster_below(queue->data[i]->dist, queue->data[left]->dist))
            best = left;
        if (right <= queue->count &&
            !booster_below(queue->data[best]->dist, queue->data[right]->dist))
            best = right;
        if (best == i)
            return;
        swap = queue->data[best];
        queue->data[best] = queue->data[i];
        queue->data[i] = swap;
        i = best;
    }
}

/* Inlined in the thread at .text+0x13d1: the nearest job, or null. */
static struct booster_job *booster_pop(struct booster_queue *queue)
{
    struct booster_job *job, *swap;
    uint32_t count, best;

    count = queue->count;
    if (count == 0)
        return NULL;
    job = queue->data[1];
    queue->data[1] = queue->data[count];
    count--;
    queue->count = count;
    best = 1;
    if (count >= 2 &&
        !booster_below(queue->data[1]->dist, queue->data[2]->dist))
        best = 2;
    if (count >= 3 &&
        !booster_below(queue->data[best]->dist, queue->data[3]->dist))
        best = 3;
    if (best != 1) {
        swap = queue->data[best];
        queue->data[best] = queue->data[1];
        queue->data[1] = swap;
        booster_sift_down(queue, best);
    }
    return job;
}

/* ---- the thread: copying ----------------------------------------------------------- */

/* .text+0x2b60 -- the cache directory for B_KIND: SCENERY for BGLs. */
static char *booster_dir(void)
{
    return (B_KIND >> 12) - 1 == 0 ? B_SCENERY : B_TEXTURE;
}

/* .text+0x2b80 -- the extension for B_KIND, by its type nibble. */
static char *booster_ext(void)
{
    B_IS_BGL = 0;
    switch (B_KIND >> 12) {
    case 0:
        return B_EXT_TXR;
    case 1:
        B_IS_BGL = 1;
        return B_EXT_BGL;
    case 2:
        return B_EXT_PAL;
    default:
        return B_EXT_HAZ;
    }
}

/* .text+0x2a70 -- B_NAME, and the cache file's path in B_DEST. */
static void booster_dest_path(void)
{
    char *ext, *dir;

    wsprintfA(B_NAME, B_FMT_NAME, B_GRIDDED != 0 ? 'G' : 'F', B_KIND & 0xfff,
              B_INDEX);
    ext = booster_ext();
    dir = booster_dir();
    wsprintfA(B_DEST, B_FMT_DEST, (char *)B_CUR->rec + 0x12, dir, B_NAME, ext);
}

/* .text+0x2ae0 -- the CD file's path in B_SOURCE: FILES or GRID, the
 * directory number, a backslash (wsprintf's "%\"), then the name. */
static void booster_source_path(void)
{
    uint32_t number = B_KIND & 0xfff;
    char *sub = B_GRIDDED != 0 ? B_GRID : B_FILES;
    char *ext = booster_ext();

    wsprintfA(B_SOURCE, B_FMT_SRC, (char *)B_CUR->rec + 0x116, booster_dir(),
              sub, number, B_GRIDDED != 0 ? 'G' : 'F', number, B_INDEX, ext);
}

/* .text+0x28d0 */
static uint32_t booster_dest_exists(void)
{
    return GetFileAttributesA(B_DEST) != INVALID_FILE_ATTRIBUTES;
}

/* .text+0x2800 -- the cache file's size, 0 if it will not open. */
static uint32_t booster_dest_size(void)
{
    HANDLE file;
    uint32_t size;

    file = CreateFileA(B_DEST, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    size = GetFileSize(file, NULL);
    CloseHandle(file);
    return size + 1 != 0 ? size : 0;
}

/* .text+0x2850 -- delete the cache file, if there, unregistering a BGL.
 * Always 0. */
static uint32_t booster_remove_file(void)
{
    uint32_t size;

    booster_dest_path();
    if (!booster_dest_exists())
        return 0;
    if (B_IS_BGL != 0)
        FE_UNREGISTER(B_CUR->area, B_NAME);
    size = booster_dest_size();
    if (DeleteFileA(B_DEST)) {
        B_CUR->used -= size;
        if (~B_CUR->free_lo < size)
            B_CUR->free_hi++;
        B_CUR->free_lo += size;
    }
    return 0;
}

/* .text+0x28f0 -- copy the file from the CD, retrying while the user says
 * so.  A new BGL is registered.  Cancel stops this booster for good and
 * empties the queue -- leaking the jobs, as bit 1 is never set. */
static uint32_t booster_copy(void)
{
    char text[0x200];
    uint32_t existed, size;
    struct booster_queue *queue;

    existed = booster_dest_exists();
    if (!existed) {
        for (;;) {
            if (CopyFileA(B_SOURCE, B_DEST, TRUE))
                break;
            wsprintfA(text, B_FMT_ERROR, B_ERROR, B_SOURCE);
            if (MessageBoxA(NULL, text, B_CAPTION, MB_RETRYCANCEL) == IDCANCEL)
                goto cancel;
            existed = booster_dest_exists();
            if (existed)
                break;
        }
    }
    SetFileAttributesA(B_DEST, FILE_ATTRIBUTE_NORMAL);
    size = booster_dest_size();
    if (B_IS_BGL != 0 && !existed) {
        FE_REGISTER(B_CUR->area, B_NAME);
        B_ADDED = 1;
    }
    B_CUR->used += size;
    if (B_CUR->free_lo < size)
        B_CUR->free_hi--;
    B_CUR->free_lo -= size;
    return 1;

cancel:
    P8(B_CUR->rec, 0x21f) = 1;
    EnterCriticalSection(B_CS);
    queue = B_QUEUE;
    if (queue->flags & 2) {
        while (queue->count > 0) {
            queue->count--;
            booster_delete(queue->data[queue->count + 1]);
        }
    } else {
        queue->count = 0;
    }
    LeaveCriticalSection(B_CS);
    return 0;
}

/* .text+0x27b0 -- copy the current file, unless the cache is full (or its
 * drive is under 256K): then the counter starts and nothing is copied
 * until booster_scan resets it. */
static uint32_t booster_copy_one(void)
{
    struct booster_node *cur = B_CUR;

    if (cur->count != 0)
        return 0;
    if (cur->used >= ((uint32_t)P16(cur->rec, 0x21c) << 10) ||
        (cur->free_hi == 0 && cur->free_lo < 0x40000)) {
        cur->count = 1;
        return 0;
    }
    booster_dest_path();
    booster_source_path();
    return booster_copy();
}

/* .text+0xf70 -- one job: copy what its record or cell needs and mark it
 * cached.  A cancelled booster stops a list part-way. */
static void FS98_STDCALL booster_run_job(struct booster_job *job)
{
    uint8_t *r = job->rec;
    uint32_t i, n;
    uint8_t *bits;
    uint8_t bit;

    B_REC = r;
    B_INDEX = job->index;
    B_CUR = job->node;
    B_KIND = P16(r, 0x11);
    switch (r[0]) {
    case 1:             /* a list, one file each */
        n = ((uint32_t)P16(r, 1) - 0x13) >> 1;
        B_GRIDDED = 0;
        for (i = n; i-- != 0;) {
            uint16_t word;

            if (P8(B_CUR->rec, 0x21f) != 0)
                return;
            word = P16(B_REC, 0x13 + 2 * i);
            if (word & 0x2000)
                continue;
            B_INDEX = word & 0xfff;
            if (booster_copy_one())
                P16(B_REC, 0x13 + 2 * i) = P16(B_REC, 0x13 + 2 * i) | 0x2000;
            else
                P16(B_REC, 0x13 + 2 * i) = P16(B_REC, 0x13 + 2 * i) & 0xdfff;
        }
        return;
    case 5:             /* a list, a BGL and its TXR each */
        n = ((uint32_t)P16(r, 1) - 0x13) >> 1;
        B_GRIDDED = 0;
        for (i = n; i-- != 0;) {
            uint16_t word;

            if (P8(B_CUR->rec, 0x21f) != 0)
                return;
            word = P16(B_REC, 0x13 + 2 * i);
            if (word & 0x2000)
                continue;
            B_INDEX = word & 0xfff;
            B_KIND = (B_KIND & 0xfff) | 0x1000;
            if (!booster_copy_one())
                continue;
            B_KIND &= 0xfff;
            if (booster_copy_one()) {
                P16(B_REC, 0x13 + 2 * i) |= 0x2000;
            } else {
                B_KIND |= 0x1000;
                booster_remove_file();
                P16(B_REC, 0x13 + 2 * i) &= 0xdfff;
            }
        }
        return;
    case 2:             /* a cell, one file */
        B_GRIDDED = 1;
        bit = (B_INDEX & 1) ? 0x20 : 0x02;
        if (r[0x17 + (B_INDEX >> 1)] & bit)
            return;
        bits = B_REC + 0x17 + (B_INDEX >> 1);
        if (booster_copy_one())
            *bits = (uint8_t)(*bits | bit);
        else
            *bits = (uint8_t)(*bits & ~bit);
        return;
    case 4:             /* a cell, a BGL and its TXR */
        B_GRIDDED = 1;
        bit = (B_INDEX & 1) ? 0x20 : 0x02;
        if (r[0x17 + (B_INDEX >> 1)] & bit)
            return;
        B_KIND = (B_KIND & 0xfff) | 0x1000;
        if (!booster_copy_one())
            return;
        B_KIND &= 0xfff;
        bits = B_REC + 0x17 + (B_INDEX >> 1);
        if (booster_copy_one()) {
            *bits = (uint8_t)(*bits | ((B_INDEX & 1) ? 0x20 : 0x02));
            return;
        }
        B_KIND |= 0x1000;
        booster_remove_file();
        bits = B_REC + 0x17 + (B_INDEX >> 1);
        *bits = (uint8_t)(*bits & ((B_INDEX & 1) ? 0xdf : 0xfd));
        return;
    default:
        return;
    }
}

/* .text+0x1350 -- the thread.  Message 0x401 empties the queue, nearest
 * first, holding the mutex for each job, then sets the event; a WM_QUIT
 * waiting behind it stops the emptying.  Jobs whose booster was taken back
 * are dropped. */
static DWORD WINAPI booster_thread(LPVOID unused)
{
    MSG msg;
    struct booster_job *job;

    (void)unused;
    if (!GetMessageA(&msg, NULL, 0, 0))
        return 0;
    for (;;) {
        if (msg.message != WM_BOOSTER) {
            DispatchMessageA(&msg);
        } else {
            for (;;) {
                if (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE) &&
                    msg.message == WM_QUIT)
                    break;
                WaitForSingleObject(B_MUTEX, INFINITE);
                EnterCriticalSection(B_CS);
                job = booster_pop(B_QUEUE);
                LeaveCriticalSection(B_CS);
                if (job == NULL) {
                    ReleaseMutex(B_MUTEX);
                    break;
                }
                if (job->node == NULL) {
                    ReleaseMutex(B_MUTEX);
                    booster_delete(job);
                    continue;
                }
                booster_run_job(job);
                booster_delete(job);
                ReleaseMutex(B_MUTEX);
            }
            SetEvent(B_EVENT);
        }
        if (!GetMessageA(&msg, NULL, 0, 0))
            return 0;
    }
}

/* ---- the image ----------------------------------------------------------------------- */

struct booster_image booster_image = {{
#include "booster_data.inc"
}};

/* Retail's exports point into its .data section. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _booster_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _booster_image + 0x38");

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        D32(BOOSTER_LINKAGE + 4) = (uint32_t)(uintptr_t)booster_init;
        D32(BOOSTER_LINKAGE + 8) = (uint32_t)(uintptr_t)booster_deinit;
        D32(BOOSTER_LINKAGE + 0x18) = (uint32_t)(uintptr_t)booster_add;
        D32(BOOSTER_LINKAGE + 0x1c) = (uint32_t)(uintptr_t)booster_drop;
    }
    return TRUE;
}
