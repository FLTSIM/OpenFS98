/* ACONTAIN.DLL -- module id 23: the aircraft container.
 *
 * An aircraft is a folder under AIRCRAFT\ holding an aircraft.cfg, whose
 * [fltsim.N] sections are its variants: a title, the .AIR file, and the
 * model, panel, sound and texture folders.  ACONTAIN finds one by title,
 * resolves those paths for whoever asks (+0x30 -- PANELS, SOUND, WINDOW and
 * CONTROLS all do), loads the flight model (a .SIM, as a host module) and the
 * visual model (a .MDL, likewise) and the model's colours, and lists every
 * complete aircraft for the selection dialog.  MAIN drives it.
 *
 * Retail is compiled C, transliterated here function by function; the
 * comments carry retail's addresses and docs/modules/acontain.md has the walk
 * through.  Before calling into it, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x38, and the code addresses its
 *     tables and strings by retail's offsets.  The two string pools at +0x39c
 *     and +0x3a0 hold real heap pointers, and the aircraft and model records
 *     are 32-bit layouts.
 *   - The aircraft record GLOBAL keeps at +0x11e0 is the one MAIN loads into;
 *     MAIN keeps a second one of its own and calls +0x20..+0x3c on that too.
 *   - Retail reads three stack buffers it never set.  They start empty here:
 *     see the notes at ac_load and ac_enumerate.
 *   - Every path buffer is retail's size, 0x100 or 0x104 bytes, and the
 *     host's path join writes up to 0x104.  Retail overruns a 0x100 buffer on
 *     a long enough path, into whatever the compiler put next; this does too,
 *     but the neighbour may differ.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define AC_DATA_VA    0x20604000u   /* retail .data */
#define AC_IMAGE_SIZE 0x3b0

typedef uint16_t __attribute__((may_alias)) ac_u16;
typedef uint32_t __attribute__((may_alias)) ac_u32;
typedef int32_t  __attribute__((may_alias)) ac_s32;

/* GLOBAL +0x11e0, and MAIN's second one: who the aircraft is. */
struct ac_aircraft {
    uint8_t *header;            /* +0x0: the visual model's, set by its +0x18 */
    uint8_t *state;             /* +0x4: model state, header +0x18 bytes */
    struct ac_record *rec;      /* +0x8: null until an aircraft is selected */
};

/* 0x14 bytes, zero-filled by the host. */
struct ac_record {
    int32_t  variant;           /* the N of [fltsim.N] */
    int32_t  model;             /* 1 normal, 2 crashed, 0 none loaded */
    void    *mdl;               /* the .MDL module's Linkage */
    int32_t  unused_0c;
    char    *dir;               /* AIRCRAFT\<folder>, from the directory pool */
};

/* A pooled string: the pool is a sentinel node whose `next` is the list. */
struct ac_node {
    struct ac_node *next;
    int32_t refs;
    char str[];
};

typedef int32_t (FS98_STDCALL *ac_visit_fn)(const char *dir, void *ctx);

struct ac_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void    *dialogs;                                                      /* +0x14: none */
    int32_t (FS98_STDCALL *draw)(struct ac_aircraft *a);                   /* +0x18 */
    int32_t (FS98_STDCALL *list)(int32_t *count, uint8_t **entries);       /* +0x1c */
    int32_t (FS98_STDCALL *select)(struct ac_aircraft *a, const char *title); /* +0x20 */
    int32_t (FS98_STDCALL *release)(struct ac_aircraft *a);                /* +0x24 */
    int32_t (FS98_STDCALL *load)(const char *title);                       /* +0x28 */
    int32_t (*unload)(void);                                               /* +0x2c */
    int32_t (FS98_STDCALL *query)(struct ac_aircraft *a, int32_t key,
                                  char *buf, uint32_t len);                /* +0x30 */
    int32_t (FS98_STDCALL *model_load)(struct ac_aircraft *a, int32_t kind); /* +0x34 */
    int32_t (FS98_STDCALL *model_unload)(struct ac_aircraft *a);           /* +0x38 */
    int32_t (FS98_STDCALL *model_kind)(struct ac_aircraft *a);             /* +0x3c */
};

struct ac_image {
    struct fs98_import import[7];       /* +0x000 ImportTable */
    struct ac_linkage  linkage;         /* +0x038 Linkage */
    uint8_t            data[AC_IMAGE_SIZE - 0x78]; /* +0x078 on */
};

extern struct ac_image ac_image;

typedef char (*ac_layout_check)[sizeof(struct ac_image) == AC_IMAGE_SIZE &&
                                sizeof(struct ac_linkage) == 0x40 &&
                                sizeof(struct ac_record) == 0x14 ? 1 : -1];

/* ---- the image and its imports ------------------------------------------------ */

#define D           ((uint8_t *)&ac_image)
#define D32(o)      (*(ac_u32 *)(D + (o)))
#define DSTR(o)     ((const char *)(D + (o)))

#define HOST        ((void *)ac_image.import[0].link)
#define G           ((uint8_t *)ac_image.import[1].link)
#define G16(o)      (*(ac_u16 *)(G + (o)))
#define G32(o)      (*(ac_u32 *)(G + (o)))

#define P16(p, o)   (*(ac_u16 *)((uint8_t *)(p) + (o)))
#define P32(p, o)   (*(ac_u32 *)((uint8_t *)(p) + (o)))
#define PPTR(p, o)  (*(void **)((uint8_t *)(p) + (o)))

#define SLOT(i, off, type) (*(type *)((uint8_t *)ac_image.import[i].link + (off)))

/* Every prototype read from the callee's `ret N`. */
typedef uint32_t (FS98_STDCALL *chain_dispatch_fn)(uint32_t list, uint32_t a,
                                                   uint32_t b);           /* ret 0xc */
typedef void     (FS98_STDCALL *g3d_draw_fn)(uint32_t object, void *state); /* ret 8 */
typedef void     (*g3d_flush_fn)(void);                                    /* ret */
typedef void     (FS98_STDCALL *panels_attach_fn)(struct ac_aircraft *a);  /* ret 4 */
typedef void     (*panels_detach_fn)(void);                                /* ret */
typedef int32_t  (FS98_STDCALL *util_alert_fn)(int32_t flags, int32_t tag,
                                               uint32_t text, const char *arg,
                                               uint32_t p3, uint32_t p4,
                                               uint32_t p5, int32_t lines); /* ret 0x20 */
/* the visual model's own Linkage */
typedef void     (FS98_STDCALL *mdl_attach_fn)(struct ac_aircraft *a);     /* ret 4 */
typedef void     (FS98_STDCALL *mdl_state_fn)(uint8_t *state);             /* ret 4 */
typedef void     (*mdl_detach_fn)(void);                                   /* ret */

#define CHAIN_DISPATCH   SLOT(2, 0x20, chain_dispatch_fn)
#define G3D_DRAW         SLOT(3, 0x38, g3d_draw_fn)
#define G3D_FLUSH        SLOT(3, 0x58, g3d_flush_fn)
#define PANELS_ATTACH    SLOT(4, 0x18, panels_attach_fn)
#define PANELS_DETACH    SLOT(4, 0x1c, panels_detach_fn)
#define UTIL_ALERT       SLOT(5, 0xa4, util_alert_fn)

#define MDL_ATTACH(l)    (*(mdl_attach_fn *)((uint8_t *)(l) + 0x18))
#define MDL_STATE(l)     (*(mdl_state_fn *)((uint8_t *)(l) + 0x1c))
#define MDL_DETACH(l)    (*(mdl_detach_fn *)((uint8_t *)(l) + 0x20))

/* The image, at retail's offsets. */
#define AC_KEY(k)       (D + 0x78 + 12 * (k))   /* { string id, type, alias id } */
#define AC_KEY_ID(k)    P32(AC_KEY(k), 0)
#define AC_KEY_TYPE(k)  (*(ac_s32 *)(AC_KEY(k) + 4))
#define AC_KEY_ALIAS(k) P32(AC_KEY(k), 8)
#define AC_POOL_TEXTURES ((struct ac_node *)D32(0x39c))
#define AC_POOL_DIRS     ((struct ac_node *)D32(0x3a0))

/* String ids, from ENGLISH.FLL. */
#define S_AIRCRAFT_CFG  0x2310      /* "aircraft" */
#define S_FLTSIM        0x2311      /* "FltSim" */
#define S_ALIAS         0x231d      /* "Alias" */
#define S_TITLE         0x231e      /* "Title" */
#define S_SIM_DIR       0x9054      /* "SIM" */
#define S_AIRCRAFT_DIR  0x9055      /* "AIRCRAFT" */

static const struct { uint16_t name, id; } ac_colour_table[] = {
#include "acontain_colours.inc"
};

/* ---- the string pools: .text+0x1400..+0x1571 -------------------------------- */

/* .text+0x1500 */
static struct ac_node *FS98_STDCALL ac_node_new(const char *str)
{
    void *p;

    FS98_HOST_ALLOC(HOST)(1, FS98_HOST_STRLEN(HOST)(str) + 9, &p, 0);
    ((struct ac_node *)p)->next = NULL;
    ((struct ac_node *)p)->refs = 1;
    FS98_HOST_STRCPY(HOST)(((struct ac_node *)p)->str, str);
    return p;
}

/* .text+0x1560 */
static void FS98_STDCALL ac_node_free(struct ac_node *n)
{
    FS98_HOST_FREE(HOST)((void **)&n);
}

/* .text+0x1400 -- a pool is a sentinel node holding "" */
static void FS98_STDCALL ac_pool_init(ac_u32 *pool)
{
    *pool = (uint32_t)ac_node_new(DSTR(0x3a4));
}

/* .text+0x1420 */
static void FS98_STDCALL ac_pool_free(ac_u32 *pool)
{
    struct ac_node *n = (struct ac_node *)*pool, *next;

    while (n != NULL) {
        next = n->next;
        ac_node_free(n);
        n = next;
    }
    *pool = 0;
}

/* .text+0x1450 -- the pooled copy of `str`, one more reference to it */
static char *FS98_STDCALL ac_intern(struct ac_node *pool, const char *str)
{
    struct ac_node *n;

    for (n = pool->next; n != NULL; n = n->next)
        if (FS98_HOST_STRCMP(HOST)(n->str, str) == 0) {
            n->refs++;
            return n->str;
        }
    n = ac_node_new(str);
    n->next = pool->next;
    pool->next = n;
    return n->str;
}

/* .text+0x14b0 -- drop a reference, by the string's text, not its address */
static void FS98_STDCALL ac_release_string(struct ac_node *pool, const char *str)
{
    struct ac_node *prev = pool, *n;

    for (n = pool->next; n != NULL; prev = n, n = n->next)
        if (FS98_HOST_STRCMP(HOST)(n->str, str) == 0) {
            if (--n->refs == 0) {
                prev->next = n->next;
                ac_node_free(n);
            }
            return;
        }
}

/* ---- paths: .text+0x320, +0x1f0, +0xd0 --------------------------------------- */

/* .text+0x320 -- the path or value `key` names for variant `variant` of the
 * aircraft in `dir`, into `out` (0x104 bytes), or "" if there is none.
 *
 *   0       dir\aircraft                 (.CFG), if it exists
 *   1       [fltsim.N] title             as text
 *   2       [fltsim.N] sim               dir\<sim> (.AIR)
 *   3/4     model folder / its model.cfg
 *   5/6     panel ...
 *   7/8     sound ...
 *   9/10    texture ...
 *   11      [fltsim.N] checklists        dir\<checklists> (.CFG)
 *
 * The folder for 3-10 is dir\model, or dir\model.<value> when the variant
 * names one.  If that folder's model.cfg has an [fltsim] alias=, the folder
 * is AIRCRAFT\<alias>\ if its model.cfg exists, else <alias>\ if that one
 * does, else nothing at all. */
static void FS98_STDCALL ac_resolve(const char *dir, int32_t variant, int32_t key, char *out)
{
    char section[0x10];         /* esp+0x0c */
    char name[0x10];            /* esp+0x1c */
    char path[0x100];           /* esp+0x2c */
    char value[0x100];          /* esp+0x12c */
    char folder[0x100];         /* esp+0x22c */
    int32_t type;

    out[0] = 0;
    value[0] = 0;
    if (key >= 12)
        return;
    FS98_HOST_LOADSTRING(HOST)(S_AIRCRAFT_CFG, path, 0x100);
    FS98_HOST_PATH_JOIN(HOST)(path, NULL, dir, path);
    if (key == 0) {
        if (FS98_HOST_EXISTS(HOST)(path, FS98_HOST_EXT_CFG) != 0)
            return;
        FS98_HOST_STRCPY(HOST)(out, path);
        return;
    }
    FS98_HOST_LOADSTRING(HOST)(S_FLTSIM, section, 0x10);
    wsprintfA(section, DSTR(0x10c), section, variant);
    FS98_HOST_LOADSTRING(HOST)(AC_KEY_ID(key), name, 0x10);
    FS98_HOST_PROFILE_STRING(HOST)(section, name, value, 0x100, path, FS98_HOST_EXT_CFG);

    if (AC_KEY_ALIAS(key) == 0) {
        if (value[0] == 0)
            return;
        if (AC_KEY_TYPE(key) == 0) {
            FS98_HOST_STRCPY(HOST)(out, value);
            return;
        }
        FS98_HOST_PATH_JOIN(HOST)(out, NULL, dir, value);
        return;
    }

    FS98_HOST_LOADSTRING(HOST)(S_ALIAS, name, 0x10);
    FS98_HOST_LOADSTRING(HOST)(AC_KEY_ALIAS(key), path, 0x100);
    FS98_HOST_PATH_JOIN(HOST)(folder, NULL, dir, path);
    if (value[0] != 0) {
        FS98_HOST_STRCAT(HOST)(folder, DSTR(0x108));
        FS98_HOST_STRCAT(HOST)(folder, value);
    }
    FS98_HOST_PATH_JOIN(HOST)(path, NULL, folder, path);
    FS98_HOST_LOADSTRING(HOST)(S_FLTSIM, section, 0x10);
    value[0] = 0;
    FS98_HOST_PROFILE_STRING(HOST)(section, name, value, 0x100, path, FS98_HOST_EXT_CFG);
    if (value[0] != 0) {
        FS98_HOST_LOADSTRING(HOST)(S_AIRCRAFT_DIR, folder, 0x100);
        FS98_HOST_LOADSTRING(HOST)(AC_KEY_ALIAS(key), path, 0x100);
        FS98_HOST_PATH_JOIN(HOST)(folder, NULL, folder, value);
        FS98_HOST_PATH_JOIN(HOST)(path, NULL, folder, path);
        if (FS98_HOST_EXISTS(HOST)(path, FS98_HOST_EXT_CFG) != 0) {
            FS98_HOST_LOADSTRING(HOST)(AC_KEY_ALIAS(key), path, 0x100);
            FS98_HOST_PATH_JOIN(HOST)(folder, NULL, value, NULL);
            FS98_HOST_PATH_JOIN(HOST)(path, NULL, folder, path);
            if (FS98_HOST_EXISTS(HOST)(path, FS98_HOST_EXT_CFG) != 0)
                return;
        }
    }

    type = AC_KEY_TYPE(key);
    if (type == FS98_HOST_EXT_CFG)
        FS98_HOST_STRCPY(HOST)(out, path);
    else if (type == FS98_HOST_TYPE_DIR)
        FS98_HOST_STRCPY(HOST)(out, folder);
}

/* .text+0x1f0 -- does variant `variant` of `dir` have its .AIR file and its
 * model, panel, sound and texture folders?  The types passed are the key
 * table's own, read from the image as retail reads them. */
static int32_t FS98_STDCALL ac_complete(const char *dir, int32_t variant)
{
    static const uint8_t keys[] = { 2, 3, 5, 7, 9 };
    char path[0x104];
    unsigned i;

    for (i = 0; i < sizeof keys; i++) {
        ac_resolve(dir, variant, keys[i], path);
        if (FS98_HOST_EXISTS(HOST)(path, AC_KEY_TYPE(keys[i])) != 0)
            return 0;
    }
    return 1;
}

/* .text+0xd0 -- call `visit(AIRCRAFT\<folder>, ctx)` for each folder under
 * AIRCRAFT, or with `complete`, each whose variant 0 is complete, until it
 * returns 0.  Returns find-first's -1 or find-close's 0.
 *
 * Retail never sets the entry's type before the first find-next, and the host
 * leaves it alone for a file with an extension it does not know -- so a first
 * entry like that is taken for a folder if the stack happened to hold -2.  It
 * starts at 0 here, which is what retail sees when the stack is clean. */
static int32_t FS98_STDCALL ac_enumerate(ac_visit_fn visit, void *ctx, int32_t complete)
{
    struct fs98_host_find *h;
    struct fs98_host_find_entry e;
    char path[0x100];           /* esp+0x4 */
    char root[0x100];           /* esp+0x214 */
    int32_t r;

    path[0] = 0;
    e.type = 0;
    FS98_HOST_LOADSTRING(HOST)(S_AIRCRAFT_DIR, root, 0x100);
    r = FS98_HOST_FIND_FIRST(HOST)(&h, root, NULL, FS98_HOST_EXT_ANY);
    if (r != 0)
        return r;
    if (FS98_HOST_FIND_NEXT(HOST)(h, &e) == 0) {
        do {
            if (e.type != FS98_HOST_TYPE_DIR)
                continue;
            FS98_HOST_PATH_JOIN(HOST)(path, NULL, root, e.name);
            if (complete != 0 && ac_complete(path, 0) == 0)
                continue;
            if (visit(path, ctx) == 0)
                break;
        } while (FS98_HOST_FIND_NEXT(HOST)(h, &e) == 0);
    }
    return FS98_HOST_FIND_CLOSE(HOST)(&h);
}

/* ---- selecting an aircraft: .text+0x6f0, +0x60 (+0x20), +0x920 (+0x24) -------- */

struct ac_find {
    struct ac_aircraft *aircraft;
    const char *title;
};

/* .text+0x6f0 -- the visitor +0x20 uses.  The folder's own name matches as
 * variant 0; otherwise the first [fltsim.N] whose title matches, case
 * insensitively, counting N up from 0 until a section has no title.  A
 * complete match is taken: the aircraft gets a fresh record, and the walk
 * stops. */
static int32_t FS98_STDCALL ac_find_title(const char *dir, void *ctx)
{
    struct ac_find *f = ctx;
    const char *title = f->title;
    char section[0x10];         /* esp+0x4 */
    char name[0x10];            /* esp+0x14 */
    char fltsim[0x10];          /* esp+0x24 */
    char path[0x100];           /* esp+0x34 */
    char value[0x100];          /* esp+0x134 */
    char folder[0x104];         /* esp+0x234 */
    struct ac_aircraft *a;
    struct ac_record *r;
    int32_t variant;

    FS98_HOST_LOADSTRING(HOST)(S_AIRCRAFT_CFG, path, 0x100);
    FS98_HOST_PATH_JOIN(HOST)(path, NULL, dir, path);
    if (FS98_HOST_EXISTS(HOST)(path, FS98_HOST_EXT_CFG) != 0)
        return 1;
    FS98_HOST_PATH_SPLIT(HOST)(dir, NULL, NULL, folder, NULL);
    variant = 0;
    if (FS98_HOST_STRICMP(HOST)(folder, title) != 0) {
        FS98_HOST_LOADSTRING(HOST)(S_FLTSIM, fltsim, 0x10);
        FS98_HOST_LOADSTRING(HOST)(S_TITLE, name, 0x10);
        value[0] = 0;
        wsprintfA(section, DSTR(0x10c), fltsim, 0);
        if (FS98_HOST_PROFILE_STRING(HOST)(section, name, value, 0x100, path,
                                           FS98_HOST_EXT_CFG) == 0)
            return 1;
        while (FS98_HOST_STRICMP(HOST)(value, title) != 0) {
            variant++;
            value[0] = 0;
            wsprintfA(section, DSTR(0x10c), fltsim, variant);
            if (FS98_HOST_PROFILE_STRING(HOST)(section, name, value, 0x100, path,
                                               FS98_HOST_EXT_CFG) == 0)
                return 1;
        }
    }
    if (ac_complete(dir, variant) == 0)
        return 1;

    a = f->aircraft;
    a->header = NULL;
    a->state = NULL;
    FS98_HOST_ALLOC(HOST)(1, 0x14, (void **)&a->rec, 1);
    r = a->rec;
    r->variant = variant;
    r->model = 0;
    r->mdl = NULL;
    r->dir = ac_intern(AC_POOL_DIRS, dir);
    return 0;
}

/* .text+0x60 -- +0x20: select the aircraft titled `title` into `a`, which
 * must not hold one.  An empty title clears `a` instead.  1 if found. */
static int32_t FS98_STDCALL ac_select(struct ac_aircraft *a, const char *title)
{
    struct ac_find f;

    if (a == NULL || a->rec != NULL)
        return 0;
    if (title == NULL || title[0] == 0) {
        a->header = NULL;
        a->state = NULL;
        a->rec = NULL;
        return 0;
    }
    f.aircraft = a;
    f.title = title;
    ac_enumerate(ac_find_title, &f, 0);
    return a->rec != NULL;
}

static int32_t FS98_STDCALL ac_model_unload(struct ac_aircraft *a);

/* .text+0x920 -- +0x24: let go of the aircraft in `a`, model and all.  Reads
 * `a` without checking it, so a null one faults.  Always 1. */
static int32_t FS98_STDCALL ac_release(struct ac_aircraft *a)
{
    struct ac_record *r = a->rec;

    if (r != NULL) {
        if (r->model != 0)
            ac_model_unload(a);
        ac_release_string(AC_POOL_DIRS, r->dir);
        FS98_HOST_FREE(HOST)((void **)&a->rec);
    }
    return 1;
}

/* ---- what is in it: .text+0xc50 (+0x30), +0x1180 ------------------------------ */

/* .text+0xc50 -- +0x30: key `key` (see ac_resolve) of the aircraft in `a`,
 * strncpy'd into `buf` -- so unterminated if it fills `len`.  1, or 0 with
 * `buf` untouched if there is no aircraft or no such thing. */
static int32_t FS98_STDCALL ac_query(struct ac_aircraft *a, int32_t key, char *buf, uint32_t len)
{
    char tmp[0x104];

    if (a == NULL || a->rec == NULL)
        return 0;
    tmp[0] = 0;
    ac_resolve(a->rec->dir, a->rec->variant, key, tmp);
    if (tmp[0] == 0)
        return 0;
    FS98_HOST_STRNCPY(HOST)(buf, tmp, len);
    return 1;
}

/* .text+0x1180 -- the payload of the first chunk tagged `id` in the .AIR file
 * `path`, into `out`.  The file is { tag, size, size bytes } after a tag;
 * nothing bounds `size` against `out`.  A short read zeroes `size` bytes of
 * `out`.  1 if read whole, else 0. */
static int32_t FS98_STDCALL ac_read_chunk(const char *path, int32_t id, void *out)
{
    struct fs98_host_file *f;
    uint32_t size;              /* esp+0x4 */
    int32_t tag;                /* esp+0x8 */
    uint32_t got;               /* the `path` argument's slot */
    int32_t found = 0;

    if (FS98_HOST_FILE_OPEN(HOST)(&f, path, FS98_HOST_EXT_AIR, 0) != 0)
        return 0;
    FS98_HOST_FILE_READ(HOST)(f, &tag, 4, &got);
    if (got == 4) {
        for (;;) {
            FS98_HOST_FILE_READ(HOST)(f, &size, 4, &got);
            if (got != 4)
                break;
            if (tag == id) {
                FS98_HOST_FILE_READ(HOST)(f, out, size, &got);
                if (got == size)
                    found = 1;
                else
                    FS98_HOST_MEMSET(HOST)(out, 0, size);
                break;
            }
            if (FS98_HOST_FILE_SEEK(HOST)(f, size, FILE_CURRENT) != 0)
                break;
            FS98_HOST_FILE_READ(HOST)(f, &tag, 4, &got);
            if (got != 4)
                break;
        }
    }
    FS98_HOST_FILE_CLOSE(HOST)(f);
    return found;
}

/* ---- the visual model: .text+0x13a0, +0x12f0, +0xcd0 (+0x34), +0xf00 (+0x38) --- */

/* .text+0x13a0 -- a colour name's id, or 0.  The table is alphabetical, and
 * the search gives up at the first name that sorts after `name` -- but only
 * if the host's _strcmpi says exactly -1 there, not just "below". */
static uint16_t FS98_STDCALL ac_colour(const char *name)
{
    unsigned i;
    int32_t r;

    if (name == NULL || name[0] == 0)
        return 0;
    for (i = 0; i < sizeof ac_colour_table / sizeof *ac_colour_table; i++) {
        r = FS98_HOST_STRICMP(HOST)(name, DSTR(ac_colour_table[i].name));
        if (r == -1)
            return 0;
        if (r == 0)
            return ac_colour_table[i].id;
    }
    return 0;
}

/* .text+0x12f0 -- [colors] 00..15 of the model.cfg into the model state's
 * sixteen words at +0x40. */
static int32_t FS98_STDCALL ac_colours(struct ac_aircraft *a)
{
    char key[4];                /* esp+0x0 */
    char value[0x20];           /* esp+0x4 */
    char cfg[0x104];            /* esp+0x24 */
    int32_t i, off;

    if (ac_query(a, 4, cfg, 0x104) == 0)
        return 0;
    for (i = 0, off = 0x40; off < 0x60; i++, off += 2) {
        wsprintfA(key, DSTR(0x394), i);
        value[0] = 0;
        FS98_HOST_PROFILE_STRING(HOST)(DSTR(0x38c), key, value, 0x20, cfg,
                                       FS98_HOST_EXT_CFG);
        P16(a->state, off) = ac_colour(value);
    }
    return 1;
}

/* .text+0xcd0 -- +0x34: load visual model `kind` -- 1 [models] normal, 2
 * crash -- for the aircraft in `a`, replacing any other.  The .MDL's +0x18
 * must leave a header in `a` that starts with 0x24 and is tagged FSIM or
 * AAF5 at +0x1c, or it is refused ("This is not a FS98 aircraft model.").
 * The model state is header +0x18 bytes; it gets the colours, the texture
 * folder and GLOBAL +0x11b6, and goes to the .MDL's +0x1c.  1 if loaded. */
static int32_t FS98_STDCALL ac_model_load(struct ac_aircraft *a, int32_t kind)
{
    char dir[0x100];            /* esp+0x10 */
    char name[0x100];           /* esp+0x110 */
    char cfg[0x100];            /* esp+0x210 */
    struct ac_record *r;
    const char *key;
    void **mdl;
    uint8_t *hdr;

    if (a == NULL || (r = a->rec) == NULL)
        return 0;
    if (r->model == kind)
        return 1;
    ac_model_unload(a);
    if (ac_query(a, 3, dir, 0x100) == 0)
        return 0;
    if (ac_query(a, 4, cfg, 0x100) == 0)
        return 0;
    if (kind == 1)
        key = DSTR(0x12c);
    else if (kind == 2)
        key = DSTR(0x134);
    else
        return 0;
    name[0] = 0;
    FS98_HOST_PROFILE_STRING(HOST)(DSTR(0x124), key, name, 0x100, cfg, FS98_HOST_EXT_CFG);
    FS98_HOST_PATH_JOIN(HOST)(dir, NULL, dir, name);
    mdl = &r->mdl;
    FS98_HOST_MODULE_LOAD(HOST)(mdl, dir, FS98_HOST_EXT_MDL);
    if (*mdl == NULL)
        return 0;
    MDL_ATTACH(*mdl)(a);

    hdr = a->header;
    if (P32(hdr, 0) != 0x24 ||
        !((hdr[0x1c] == 'F' && hdr[0x1d] == 'S' && hdr[0x1e] == 'I' && hdr[0x1f] == 'M') ||
          (hdr[0x1c] == 'A' && hdr[0x1d] == 'A' && hdr[0x1e] == 'F' && hdr[0x1f] == '5'))) {
        UTIL_ALERT(4, 1, 0xf544, dir, 0, 0, 0, 0);
        a->header = NULL;
        FS98_HOST_MODULE_FREE(HOST)(mdl);
        return 0;
    }
    if (P32(hdr, 0x18) != 0) {
        FS98_HOST_ALLOC(HOST)(1, P32(hdr, 0x18), (void **)&a->state, 1);
        P32(a->state, 0) = P32(a->header, 0x18);
        ac_colours(a);
        if (ac_query(a, 9, dir, 0x100) != 0)
            PPTR(a->state, 0x64) = ac_intern(AC_POOL_TEXTURES, dir);
        P16(a->state, 0x60) = G16(0x11b6);
    }
    MDL_STATE(*mdl)(a->state);
    r->model = kind;
    return 1;
}

/* .text+0xf00 -- +0x38: unload the visual model, if there is one.  1 if
 * there was. */
static int32_t FS98_STDCALL ac_model_unload(struct ac_aircraft *a)
{
    struct ac_record *r;

    if (a == NULL || (r = a->rec) == NULL || r->mdl == NULL)
        return 0;
    G3D_FLUSH();
    MDL_DETACH(r->mdl)();
    FS98_HOST_MODULE_FREE(HOST)(&r->mdl);
    if (a->state != NULL) {
        ac_release_string(AC_POOL_TEXTURES, PPTR(a->state, 0x64));
        FS98_HOST_FREE(HOST)((void **)&a->state);
    }
    a->header = NULL;
    r->model = 0;
    return 1;
}

/* .text+0xf80 -- +0x3c: which model is loaded, 0 if none. */
static int32_t FS98_STDCALL ac_model_kind(struct ac_aircraft *a)
{
    if (a == NULL || a->rec == NULL)
        return 0;
    return a->rec->model;
}

/* .text+0x12b0 -- +0x18: hand the model to G3D to draw.  1 if there is one. */
static int32_t FS98_STDCALL ac_draw(struct ac_aircraft *a)
{
    if (a == NULL || a->rec == NULL || a->rec->model == 0)
        return 0;
    G3D_DRAW(P32(a->header, 4), a->state);
    return 1;
}

/* ---- the user's aircraft: .text+0x970 (+0x28), +0xbf0 (+0x2c) ------------------ */

static int32_t ac_unload(void);

/* .text+0x970 -- +0x28: make `title` the aircraft, in GLOBAL +0x11e0.
 *
 * If no folder or variant has that title, FLTSIM98.CFG's [AIRtoCONTAINER]
 * maps it -- an old .AIR name, by itself or with its path and extension
 * stripped -- to one.  Failing that, it puts up "Aircraft initialization
 * failure" and goes back to the aircraft it had.  Then it loads the flight
 * model named by the .AIR's chunk 0x100 from SIM\ into GLOBAL +0x11d4, the
 * normal visual model, and the panel, and tells CHAIN list 20.  1 if all of
 * that worked.
 *
 * Retail leaves `previous` unset when there was no aircraft, and `sim` unset
 * when the .AIR has no chunk 0x100 or will not open, and goes on to use both
 * as strings.  They start empty here: retail's behaviour on a clean stack. */
static int32_t FS98_STDCALL ac_load(const char *title)
{
    struct ac_aircraft *a = (struct ac_aircraft *)(G + 0x11e0);
    char wanted[0x100];         /* esp+0x8 */
    char name[0x100];           /* esp+0x108 */
    char sim[0x104];            /* esp+0x208 */
    char previous[0x104];       /* esp+0x30c */
    char simdir[0x104];         /* esp+0x410 */

    previous[0] = 0;
    sim[0] = 0;
    if (a != NULL && a->rec != NULL) {
        ac_query(a, 1, previous, 0x104);
        ac_unload();
    }
    FS98_HOST_STRCPY(HOST)(wanted, title);
    if (ac_select(a, wanted) == 0) {
        wanted[0] = 0;
        FS98_HOST_CFG_STRING(HOST)(DSTR(0x114), title, wanted, 0x100);
        if (wanted[0] == 0) {
            FS98_HOST_PATH_SPLIT(HOST)(title, NULL, NULL, name, NULL);
            FS98_HOST_CFG_STRING(HOST)(DSTR(0x114), name, wanted, 0x100);
        }
        if (ac_select(a, wanted) == 0) {
            UTIL_ALERT(1, 4, 0xf540, title, 0, 0, 0, 1);
            if (ac_select(a, previous) == 0) {
                UTIL_ALERT(1, 4, 0xf540, previous, 0, 0, 0, 1);
                return 0;
            }
            FS98_HOST_STRCPY(HOST)(wanted, previous);
        }
    }
    ac_query(a, 2, name, 0x100);
    ac_read_chunk(name, 0x100, sim);
    FS98_HOST_LOADSTRING(HOST)(S_SIM_DIR, simdir, 0x104);
    FS98_HOST_PATH_JOIN(HOST)(sim, NULL, simdir, sim);
    if (FS98_HOST_MODULE_LOAD(HOST)((void **)(G + 0x11d4), sim, FS98_HOST_EXT_SIM) != 0) {
        UTIL_ALERT(1, 4, 0xf541, wanted, 0, 0, 0, 1);
        return 0;
    }
    if (ac_model_load(a, 1) == 0) {
        UTIL_ALERT(1, 4, 0xf542, wanted, 0, 0, 0, 1);
        return 0;
    }
    PANELS_ATTACH(a);
    CHAIN_DISPATCH(0x14, 0x60, 0);
    return 1;
}

/* .text+0xbf0 -- +0x2c: drop the aircraft: the panel, then -- only if a
 * flight model is loaded -- the visual model and the flight model, then the
 * record.  Always 1. */
static int32_t ac_unload(void)
{
    PANELS_DETACH();
    if (G32(0x11d4) != 0) {
        ac_model_unload((struct ac_aircraft *)(G + 0x11e0));
        FS98_HOST_MODULE_FREE(HOST)((void **)(G + 0x11d4));
    }
    ac_release((struct ac_aircraft *)(G + 0x11e0));
    return 1;
}

/* ---- the selection list: .text+0xfa0 (+0x1c), +0x1050, +0x1160 ---------------- */

/* One 0xa08-byte entry: the title, then the .AIR's chunks 2 and 3. */
#define AC_ENTRY 0xa08

struct ac_listing {
    int32_t count;
    uint8_t *entries;
};

/* .text+0x1050 -- the visitor +0x1c uses: an entry for each titled variant,
 * from N = 0 until one has no title. */
static int32_t FS98_STDCALL ac_list_visit(const char *dir, void *ctx)
{
    struct ac_listing *l = ((struct ac_listing **)ctx)[1];
    char title[0x104];          /* esp+0x0 */
    char air[0x100];            /* esp+0x104 */
    int32_t variant = 0;
    uint8_t *e;

    ac_resolve(dir, 0, 1, title);
    while (title[0] != 0) {
        l->count++;
        FS98_HOST_REALLOC(HOST)(1, l->count * AC_ENTRY, (void **)&l->entries);
        e = l->entries + l->count * AC_ENTRY - AC_ENTRY;
        e[0] = 0;
        e[0x900] = 0;
        e[0x100] = 0;
        e[0x500] = 0;
        P32(e, 0xa04) = 0;
        FS98_HOST_STRCPY(HOST)((char *)e, title);
        ac_resolve(dir, variant, 2, air);
        ac_read_chunk(air, 2, e + 0x100);
        ac_read_chunk(air, 3, e + 0x500);
        variant++;
        ac_resolve(dir, variant, 1, title);
    }
    return 1;
}

/* .text+0x1160 -- by title, case sensitively */
static int32_t FS98_STDCALL ac_list_order(const void *x, const void *y)
{
    return FS98_HOST_STRCMP(HOST)(x, y);
}

/* .text+0xfa0 -- +0x1c: every titled variant of every complete aircraft,
 * sorted, as one host block of 0xa08-byte entries the caller frees.  1, or 0
 * with both outputs zeroed when there are none. */
static int32_t FS98_STDCALL ac_list(int32_t *count, uint8_t **entries)
{
    struct ac_listing l;
    struct ac_listing *ctx[2];

    l.count = 0;
    FS98_HOST_ALLOC(HOST)(1, 0, (void **)&l.entries, 0);
    ctx[0] = NULL;
    ctx[1] = &l;
    ac_enumerate(ac_list_visit, ctx, 1);
    if (l.count == 0) {
        *count = 0;
        *entries = NULL;
        FS98_HOST_FREE(HOST)((void **)&l.entries);
        return 0;
    }
    FS98_HOST_QSORT(HOST)(l.entries, l.count, AC_ENTRY, ac_list_order);
    *count = l.count;
    *entries = l.entries;
    return 1;
}

/* ---- the module ------------------------------------------------------------------- */

/* .text+0x20 -- +0x04 */
static void ac_init(void)
{
    ac_pool_init(&D32(0x39c));
    ac_pool_init(&D32(0x3a0));
}

/* .text+0x40 -- +0x08 */
static void ac_deinit(void)
{
    ac_pool_free(&D32(0x3a0));
    ac_pool_free(&D32(0x39c));
}

struct ac_image ac_image = {
    {
        { FS98_MOD_HOST,   0 },
        { FS98_MOD_GLOBAL, 0 },
        { FS98_MOD_CHAIN,  0 },
        { FS98_MOD_G3D,    0 },
        { FS98_MOD_PANELS, 0 },
        { FS98_MOD_UTIL,   0 },
        { 0, 0 }
    },
    {
        FS98_MOD_ACONTAIN, ac_init, ac_deinit, 0, 0, 0,
        ac_draw, ac_list, ac_select, ac_release, ac_load, ac_unload,
        ac_query, ac_model_load, ac_model_unload, ac_model_kind
    },
    {
#include "acontain_data.inc"
    }
};

/* The two exports are the image's first 0x38 bytes and the 0x40 at +0x38,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _ac_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _ac_image + 0x38");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
