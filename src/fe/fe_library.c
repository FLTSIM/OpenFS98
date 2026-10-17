/* FE's dialog half: .text+0x57a0..+0xb470 -- Linkage +0x78, the Scenery
 * Library.  Included by fe.c.
 *
 * Like FACIL's dialogs this is not window code.  FE imports nothing from
 * USER32; it fills parameter blocks in .data and runs dialogs that
 * English\World.DLL registers through host +0x10c, and World calls back
 * through function pointers in the blocks.  Everything here is those
 * callbacks and what they need: the world file's reader and writer, the area
 * and cache lists, a directory browser and the path checks.
 *
 *   0x1306      the world files            block .data+0x2e28
 *   0x13080001  the area list              block .data+0x2ec8
 *   0x13080002  add areas from a drive     block .data+0x2f70
 *   0x13080003  add one area               block .data+0x3170
 *   0x13080004  edit one area              block .data+0x3138
 *   0x13080005  area priority              block .data+0x2e78
 *   0x13080006  the cache list             block .data+0x3000
 *   0x13080007  edit one cache entry       block .data+0x30e8
 *   0x13080008  add one cache entry        block .data+0x3118
 *
 * Addresses are written as retail's (VA(0x206d1218)) so that they can be
 * matched against the disassembly; locals that retail parks in an argument
 * slot are ordinary locals here, which nothing can observe. */

#define VA(va)          (D + ((uint32_t)(va) - 0x206ce000u))
#define L32(va)         M32(VA(va))
#define L16(va)         M16(VA(va))
#define L8(va)          M8(VA(va))
#define VA_PTR(va)      ((const uint8_t *)(uintptr_t)VA(va))

/* No C runtime here: byte loops, through a volatile pointer so that they are
 * not turned back into library calls. */
static void fe_lib_fill(void *to, uint8_t value, uint32_t size)
{
    volatile uint8_t *p = (volatile uint8_t *)to;

    while (size-- != 0)
        *p++ = value;
}

static void fe_lib_copy(void *to, const void *from, uint32_t size)
{
    volatile uint8_t *p = (volatile uint8_t *)to;
    const uint8_t *q = (const uint8_t *)from;

    while (size-- != 0)
        *p++ = *q++;
}

#define NUM_FORMAT      CALL(6, 0x18, fe_f3)   /* (value, id, out) */
#define UTIL_PUT        CALL(7, 0x8c, fe_f4)   /* (table, n, records, mode) */
#define UTIL_GET        CALL(7, 0x90, fe_f3)   /* (table, n, records) */
#define UTIL_TREE       CALL(7, 0x94, fe_f5)   /* (&list, &count, dir, pattern, flag) */
#define UTIL_IS_DIR     CALL(7, 0xa0, fe_f1)   /* (path) */
#define UTIL_OPEN_ALERT CALL(7, 0xa8, fe_f8)   /* (&handle, tag, p1..p5, lines) */
#define UTIL_CLOSE_ALERT CALL(7, 0xac, fe_f1)  /* (&handle) */

#define H_GET_ATTR      HOST(0x134, fe_f3)     /* (&attr, path, ext) */
#define H_SET_ATTR      HOST(0x138, fe_f3)     /* (attr, path, ext) */
#define H_CREATE        HOST(0x148, fe_f3)     /* (&file, path, ext) */
#define H_DELETE        HOST(0x14c, fe_f2)     /* (path, ext) */
#define H_FREE_SPACE    HOST(0x150, fe_f2)     /* (path, &u64): 0 if the drive answers */
#define H_EXISTS        HOST(0x158, fe_f2)     /* (path, ext): 0 if it exists */
#define H_MOVE          HOST(0x16c, fe_f4)     /* (to, toext, from, fromext) */
#define H_TEMP          HOST(0x174, fe_f2)     /* (out, dir) */
#define H_PATH_CHECK    HOST(0x18c, fe_f1)     /* 0, 0x1003 empty, -1 a bad character */
#define H_TRUNCATE      HOST(0x1a8, fe_f2)     /* (file, size) */
#define H_WRITE         HOST(0x1b0, fe_f4)     /* (file, buf, n, &wrote) */
#define H_QSORT         HOST(0x22c, fe_f4)     /* (base, n, size, stdcall compare) */
#define H_SWAP          HOST(0x230, fe_f3)     /* (a, b, size) */
#define H_STRCAT        HOST(0x2b0, fe_f2)
#define H_LIST_CLEAR    HOST(0x304, fe_f1)     /* (list): count 0, nothing freed */

/* Strings loaded at open (.text+0x8f30): 0x9011 at 0x206d3660 (the "same
 * drive" marker), 0x9012 at 0x206d3760 (the "host directory" prefix), 0x9013
 * at 0x206d3868, 0x9003 at 0x206d3968 ("SCENERY"), 0x9006 at 0x206d3a70
 * ("TEXTURE"), 0x9015 at 0x206d3b78. */

typedef uint32_t (FS98_STDCALL *fe_cb)(uint32_t);

static uint32_t FS98_STDCALL fe_lib_nothing(uint32_t block);
static uint32_t fe_lib_edit_areas(uint32_t changed);
static uint32_t fe_lib_edit_caches(uint32_t changed);

/* A zeroed path buffer whose first byte is .data+0x3c9c's, as retail
 * initialises every scratch buffer in the path code. */
#define SCRATCH(name, size) \
    uint8_t name[size]; \
    fe_lib_fill(name, 0, sizeof(name)); \
    name[0] = L8(0x206d3c9c)

/* ---- small helpers ------------------------------------------------------ */

/* .text+0xb450 -- 0 for "X:" not followed by a backslash. */
static uint32_t fe_lib_rooted(uint32_t path)
{
    if (M8(path + 1) == 0x3a && M8(path + 2) != 0x5c)
        return 0;
    return 1;
}

/* .text+0xb410 -- the extension id is 1 (a BGL). */
static uint32_t fe_lib_is_bgl(uint32_t path)
{
    uint32_t ext = 0;

    if (H_SPLIT(path, 0, 0, 0, ADDR(&ext)) != 0)
        return 0;
    return ext == 1;
}

/* .text+0xaf70 -- "*" or an empty name. */
static uint32_t fe_lib_is_star(uint32_t name)
{
    if (M8(name) != 0x2a)
        return 0;
    return H_STRLEN(name) <= 1;
}

/* .text+0xa940 -- <drive>\<dir>\<name>\<0x9013 string>, the folder a
 * scenery path names. */
static uint32_t fe_lib_scenery_folder(uint32_t out, uint32_t path)
{
    uint8_t drive[0x108];
    uint8_t dir[0x108];
    uint8_t name[0x104];

    if (H_SPLIT(path, ADDR(drive), ADDR(dir), ADDR(name), 0) != 0)
        return 0xffffffffu;
    if (H_JOIN(ADDR(dir), 0, ADDR(dir), ADDR(name)) != 0)
        return 0xffffffffu;
    return H_JOIN(out, ADDR(drive), ADDR(dir), VA(0x206d3868)) != 0 ?
           0xffffffffu : 0;
}

/* .text+0xafa0 -- that folder exists. */
static uint32_t fe_lib_folder_exists(uint32_t path)
{
    uint8_t folder[0x108];

    fe_lib_scenery_folder(ADDR(folder), path);
    return H_EXISTS(ADDR(folder), 0) == 0;
}

/* .text+0xafe0 / +0xb020 -- <path>\SCENERY, <path>\TEXTURE are directories. */
static uint32_t fe_lib_has_scenery(uint32_t path)
{
    uint8_t joined[0x108];

    H_JOIN(ADDR(joined), 0, path, VA(0x206d3968));
    return UTIL_IS_DIR(ADDR(joined));
}

static uint32_t fe_lib_has_texture(uint32_t path)
{
    uint8_t joined[0x108];

    H_JOIN(ADDR(joined), 0, path, VA(0x206d3a70));
    return UTIL_IS_DIR(ADDR(joined));
}

/* .text+0xa7c0 -- a path made absolute: a drive equal to the 0x9011 marker
 * becomes the host's drive, and a directory starting with the 0x9012 prefix
 * is taken under the host's directory. */
static uint32_t fe_lib_resolve(uint32_t out, uint32_t path)
{
    uint8_t base[0x104];
    uint8_t base_drive[0x104];
    uint8_t base_dir[0x104];
    uint8_t base_name[0x100];
    uint8_t drive[0x104];
    uint8_t dir[0x104];
    uint8_t name[0x104];
    uint32_t use_drive = ADDR(base_drive);
    uint32_t use_dir = ADDR(base_dir);
    uint32_t length;

    if (H_BASEDIR(ADDR(base)) != 0)
        return 0xffffffffu;
    if (H_SPLIT(ADDR(base), ADDR(base_drive), ADDR(base_dir),
                ADDR(base_name), 0) != 0)
        return 0xffffffffu;
    if (H_SPLIT(path, ADDR(drive), ADDR(dir), ADDR(name), 0) != 0)
        return 0xffffffffu;
    if (H_STRICMP(ADDR(drive), VA(0x206d3660)) != 0)
        use_drive = ADDR(drive);
    length = H_STRLEN(VA(0x206d3760));
    if (H_MEMCMP(ADDR(dir), VA(0x206d3760), length) != 0) {
        use_dir = ADDR(dir);
    } else {
        length = H_STRLEN(VA(0x206d3760));
        if (H_JOIN(ADDR(base_dir), 0, ADDR(base_dir), ADDR(dir) + length) != 0)
            return 0xffffffffu;
    }
    return H_JOIN(out, use_drive, use_dir, ADDR(name)) != 0 ? 0xffffffffu : 0;
}

/* .text+0xb2f0 -- the path's last directory is SCENERY (after dropping a
 * trailing .data+0x3210 suffix, "\").  An empty directory makes retail
 * compare from the byte below its buffer: the top byte of the esi it saved,
 * which is the caller's -- `leftover` here.  In the area checks that is the
 * block's address, so the byte is the module's own top address byte, and the
 * suffix would match (and the zero be stored there) only for a module loaded
 * at 0x5c000000.  The browser's callers never get here with an empty
 * directory, since UTIL's listing joins every entry to its root. */
static uint32_t fe_lib_is_scenery_dir(uint32_t path, uint32_t leftover)
{
    uint8_t padded[0x10 + 0x108];
    uint8_t *dir = padded + 0x10;
    SCRATCH(name, 0x104);
    uint32_t suffix;
    int32_t at;

    fe_lib_fill(padded, 0, sizeof(padded));
    padded[0xf] = (uint8_t)(leftover >> 24);
    dir[0] = L8(0x206d3c9c);
    suffix = H_STRLEN(VA(0x206d1210));
    if (H_SPLIT(path, 0, ADDR(dir), 0, 0) != 0)
        return 0;
    at = (int32_t)H_STRLEN(ADDR(dir)) - (int32_t)suffix;
    if (at >= -0x10 &&
        H_STRICMP(ADDR(dir) + (uint32_t)at, VA(0x206d1210)) == 0)
        dir[H_STRLEN(ADDR(dir)) - suffix] = 0;
    if (H_SPLIT(ADDR(dir), 0, 0, ADDR(name), 0) != 0)
        return 0;
    return H_STRICMP(ADDR(name), VA(0x206d3968)) == 0;
}

/* .text+0xb060 -- a stored path made absolute: drive, directory, SCENERY,
 * name, then the 0x9015 string. */
static uint32_t fe_lib_expand(uint32_t out, uint32_t path)
{
    SCRATCH(drive, 0x104);
    SCRATCH(dir, 0x108);
    SCRATCH(name, 0x104);

    if (H_SPLIT(path, ADDR(drive), ADDR(dir), ADDR(name), 0) != 0)
        return 0xffffffffu;
    if (H_JOIN(ADDR(dir), 0, ADDR(dir), VA(0x206d3968)) != 0)
        return 0xffffffffu;
    if (H_JOIN(out, ADDR(drive), ADDR(dir), ADDR(name)) != 0)
        return 0xffffffffu;
    H_STRCAT(out, VA(0x206d3b78));
    return 0;
}

/* .text+0xb170 -- the reverse, for writing: drop a trailing .data+0x3210
 * from the directory, then directory's parent, name and the 0x9015 string.
 * An empty directory reads below the buffer as fe_lib_is_scenery_dir's does;
 * the caller's esi is the world file's handle. */
static uint32_t fe_lib_contract(uint32_t out, uint32_t path, uint32_t leftover)
{
    uint8_t padded[0x10 + 0x108];
    uint8_t *dir = padded + 0x10;
    SCRATCH(drive, 0x104);
    SCRATCH(name, 0x104);
    SCRATCH(parent, 0x108);
    uint32_t suffix;
    int32_t at;

    fe_lib_fill(padded, 0, sizeof(padded));
    padded[0xf] = (uint8_t)(leftover >> 24);
    dir[0] = L8(0x206d3c9c);
    suffix = H_STRLEN(VA(0x206d1210));
    if (H_SPLIT(path, ADDR(drive), ADDR(dir), ADDR(name), 0) != 0)
        return 0xffffffffu;
    at = (int32_t)H_STRLEN(ADDR(dir)) - (int32_t)suffix;
    if (at >= -0x10 &&
        H_STRICMP(ADDR(dir) + (uint32_t)at, VA(0x206d1210)) == 0)
        dir[H_STRLEN(ADDR(dir)) - suffix] = 0;
    if (H_SPLIT(ADDR(dir), 0, ADDR(parent), 0, 0) != 0)
        return 0xffffffffu;
    if (H_JOIN(out, ADDR(drive), ADDR(parent), ADDR(name)) != 0)
        return 0xffffffffu;
    H_STRCAT(out, VA(0x206d3b78));
    return 0;
}

/* .text+0xace0 -- a usable scenery path: an existing file, or for a "*"
 * name a folder holding at least one entry. */
static uint32_t fe_lib_area_exists(uint32_t path)
{
    SCRATCH(resolved, 0x108);
    SCRATCH(drive, 0x104);
    SCRATCH(dir, 0x108);
    SCRATCH(name, 0x104);
    SCRATCH(joined, 0x108);
    SCRATCH(drive2, 0x104);
    SCRATCH(dir2, 0x108);
    SCRATCH(parent, 0x108);
    uint8_t entry[0x110];
    uint32_t find = 0;

    if (fe_lib_resolve(ADDR(resolved), path) != 0)
        return 0;
    H_SPLIT(ADDR(resolved), ADDR(drive), ADDR(dir), ADDR(name), 0);
    if (!fe_lib_is_star(ADDR(name)))
        return H_EXISTS(ADDR(resolved), 1) == 0;
    H_JOIN(ADDR(joined), ADDR(drive), ADDR(dir), 0);
    if (H_SPLIT(ADDR(joined), ADDR(drive2), ADDR(dir2), 0, 0) != 0)
        return 0;
    if (H_JOIN(ADDR(parent), ADDR(drive2), ADDR(dir2), 0) != 0)
        return 0;
    if (fe_lib_folder_exists(ADDR(parent)))
        return 1;
    H_JOIN(ADDR(joined), ADDR(drive), ADDR(dir), 0);
    if (H_FIND_FIRST(ADDR(&find), ADDR(joined), 0, 1) != 0)
        return 0;
    if (H_FIND_NEXT(find, ADDR(entry)) != 0) {
        H_FIND_CLOSE(ADDR(&find));
        return 0;
    }
    H_FIND_CLOSE(ADDR(&find));
    return 1;
}

/* .text+0xaab0 -- `path` names the same place as a row other than `skip`
 * (1-based; 0 checks nothing): same drive and directory, and the same name
 * or a "*" on either side. */
static uint32_t fe_lib_conflicts(uint32_t path, uint32_t count, uint32_t rows,
                                 uint32_t skip)
{
    SCRATCH(resolved, 0x108);
    SCRATCH(drive, 0x104);
    SCRATCH(dir, 0x108);
    SCRATCH(name, 0x104);
    SCRATCH(other, 0x108);
    SCRATCH(other_drive, 0x104);
    SCRATCH(other_dir, 0x108);
    SCRATCH(other_name, 0x104);
    uint32_t index = 0;
    uint32_t row;

    if (fe_lib_resolve(ADDR(resolved), path) != 0)
        return 1;
    H_SPLIT(ADDR(resolved), ADDR(drive), ADDR(dir), ADDR(name), 0);
    if (skip == 0 || count == 0)
        return 0;
    row = rows + 4;
    do {
        fe_lib_resolve(ADDR(other), M32(row));
        H_SPLIT(ADDR(other), ADDR(other_drive), ADDR(other_dir),
                ADDR(other_name), 0);
        if (H_STRICMP(ADDR(drive), ADDR(other_drive)) == 0 &&
            H_STRICMP(ADDR(dir), ADDR(other_dir)) == 0) {
            if (name[0] == 0x2a || other_name[0] == 0x2a)
                return 1;
            if (H_STRICMP(ADDR(name), ADDR(other_name)) == 0)
                return 1;
        }
        index++;
        row += 0x40;
        if (index == skip) {
            index++;
            row += 0x40;
        }
    } while (index < count);
    return 0;
}

/* .text+0xaa30 -- move element `from` to `to`, by adjacent swaps. */
static uint32_t fe_lib_move(uint32_t from, uint32_t to, uint32_t base,
                            uint32_t size)
{
    uint32_t at = from;

    if (from < to) {
        uint32_t steps = to - from;
        uint32_t cursor = size * from + base;

        at = to;
        do {
            uint32_t next = size + cursor;
            H_SWAP(cursor, next, size);
            cursor = next;
        } while (--steps != 0);
    }
    if (at > to) {
        uint32_t offset = size * at;
        uint32_t steps = at - to;

        do {
            H_SWAP(base + offset, base + offset - size, size);
            offset -= size;
        } while (--steps != 0);
    }
    return 0;
}

/* ---- the world file ------------------------------------------------------ */

/* .text+0xa510 -- write a word at `offset`, then return to where the file
 * was.  Returns the seek's result. */
static uint32_t fe_lib_patch(uint32_t file, uint32_t offset, uint32_t value)
{
    uint32_t saved;
    uint32_t wrote;

    if (H_TELL(file, ADDR(&saved)) != 0)
        return 0xffffffffu;
    if (H_SEEK(file, offset & 0xffff, 0) != 0)
        return 0xffffffffu;
    if (H_WRITE(file, value, 2, ADDR(&wrote)) != 0)
        return 0xffffffffu;
    if (wrote != 2)
        return 0xffffffffu;
    return H_SEEK(file, saved, 0);
}

/* .text+0xa4b0 -- a NUL-terminated string, byte by byte. */
static uint32_t fe_lib_read_string(uint32_t file, uint32_t out)
{
    uint8_t c = 0xff;
    uint32_t got;
    uint32_t at = 0;

    for (;;) {
        if (H_READ(file, ADDR(&c), 1, ADDR(&got)) != 0) {
            M8(out + at) = 0;
            return 0xffffffffu;
        }
        M8(out + at) = c;
        at++;
        if (c == 0)
            return 0;
    }
}

/* .text+0xa420 -- the next record's marker byte is 1; the file is put back. */
static uint32_t fe_lib_record_follows(uint32_t file)
{
    uint32_t position;
    uint8_t marker;
    uint32_t got;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0;
    if (H_READ(file, ADDR(&marker), 1, ADDR(&got)) != 0)
        return 0;
    if (got != 1)
        return 0;
    if (H_SEEK(file, position, 0) != 0)
        return 0;
    return marker == 1;
}

/* .text+0x8fe0 -- the 0x20-byte header: eight words to .data+0x3238, then
 * sixteen bytes read and dropped. */
static uint32_t fe_lib_read_header(uint32_t file, uint32_t out)
{
    uint8_t tail[0x10];
    uint32_t got;
    uint32_t at;

    for (at = 0; at < 0x10; at += 2)
        if (H_READ(file, VA(0x206d3238) + at, 2, ADDR(&got)) != 0)
            return 0xffffffffu;
    if (H_READ(file, ADDR(tail), 0x10, ADDR(&got)) != 0)
        return 0xffffffffu;
    M32(out) = VA(0x206d3238);
    return 0;
}

/* .text+0x9160 -- the header back: the eight words, then the sixteen bytes
 * at .data+0x3c88. */
static uint32_t fe_lib_write_header(uint32_t file, uint32_t header)
{
    uint32_t tail[4];
    uint32_t wrote;
    uint32_t at;

    tail[0] = L32(0x206d3c88);
    tail[1] = L32(0x206d3c8c);
    tail[2] = L32(0x206d3c90);
    tail[3] = L32(0x206d3c94);
    if (H_SEEK(file, 0, 0) != 0)
        return 0xffffffffu;
    for (at = 0; at < 0x10; at += 2)
        if (H_WRITE(file, header + at, 2, ADDR(&wrote)) != 0)
            return 0xffffffffu;
    return H_WRITE(file, ADDR(tail), 0x10, ADDR(&wrote)) != 0 ?
           0xffffffffu : 0;
}

/* .text+0x9310 / +0x93d0 -- the two titles, at the header's words 2 and 3. */
static uint32_t fe_lib_read_title(uint32_t file, uint32_t header, uint32_t out)
{
    if (H_SEEK(file, M16(header + 4), 0) != 0)
        return 0xffffffffu;
    return fe_lib_read_string(file, out) != 0 ? 0xffffffffu : 0;
}

static uint32_t fe_lib_read_title2(uint32_t file, uint32_t header, uint32_t out)
{
    if (H_SEEK(file, M16(header + 6), 0) != 0)
        return 0xffffffffu;
    return fe_lib_read_string(file, out) != 0 ? 0xffffffffu : 0;
}

/* .text+0x9350 -- the first title, here, and its offset into word 2. */
static uint32_t fe_lib_write_title(uint32_t file, uint32_t text)
{
    uint32_t position;
    uint32_t value;
    uint32_t wrote;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    value = position;
    if (fe_lib_patch(file, 4, ADDR(&value)) != 0)
        return 0xffffffffu;
    return H_WRITE(file, text, H_STRLEN(text) + 1, ADDR(&wrote)) != 0 ?
           0xffffffffu : 0;
}

/* .text+0x9410 -- the second, as a fixed zero-padded 0x100 bytes. */
static uint32_t fe_lib_write_title2(uint32_t file, uint32_t text)
{
    uint32_t copy = 0;
    uint32_t position;
    uint32_t value;
    uint32_t wrote;

    if (H_ALLOC(5, 0x100, ADDR(&copy), 1) != 0)
        return 0xffffffffu;
    H_STRCPY(copy, text);
    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    value = position;
    if (fe_lib_patch(file, 6, ADDR(&value)) != 0)
        return 0xffffffffu;
    if (H_WRITE(file, copy, 0x100, ADDR(&wrote)) != 0)
        return 0xffffffffu;
    return FREE(ADDR(&copy)) != 0 ? 0xffffffffu : 0;
}

/* .text+0x9f80 / +0xa040 -- the bytes between the header and the first
 * title, kept and written back unread. */
static uint32_t fe_lib_read_blob(uint32_t file, uint32_t header)
{
    uint32_t position;
    uint32_t got;

    (void)header;
    if (L32(0x206d3c78) != 0) {
        L32(0x206d3c7c) = 0;
        if (FREE(VA(0x206d3c78)) != 0)
            return 0xffffffffu;
    }
    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    L32(0x206d3c7c) = (L32(0x206d3dc4) & 0xffff) - position;
    if (H_ALLOC(5, L32(0x206d3c7c), VA(0x206d3c78), 1) != 0)
        return 0xffffffffu;
    return H_READ(file, L32(0x206d3c78), L32(0x206d3c7c), ADDR(&got)) != 0 ?
           0xffffffffu : 0;
}

static uint32_t fe_lib_write_blob(uint32_t file)
{
    uint32_t wrote;

    return H_WRITE(file, L32(0x206d3c78), L32(0x206d3c7c), ADDR(&wrote)) != 0 ?
           0xffffffffu : 0;
}

/* .text+0xa070 / +0xa1b0 -- the zero-terminated word list at word 4. */
static uint32_t fe_lib_read_words(uint32_t file, uint32_t header)
{
    uint16_t word;
    uint32_t got;
    uint32_t count = 0;

    (void)header;
    if (L32(0x206d3c80) != 0) {
        L32(0x206d3c84) = 0;
        if (FREE(VA(0x206d3c80)) != 0)
            return 0xffffffffu;
    }
    if (H_SEEK(file, L32(0x206d3dc8) & 0xffff, 0) != 0)
        return 0xffffffffu;
    if (H_READ(file, ADDR(&word), 2, ADDR(&got)) != 0)
        return 0xffffffffu;
    for (;;) {
        count++;
        if (word == 0)
            break;
        if (H_READ(file, ADDR(&word), 2, ADDR(&got)) != 0)
            return 0xffffffffu;
    }
    count += count;
    if (H_ALLOC(5, count, VA(0x206d3c80), 1) != 0)
        return 0xffffffffu;
    L32(0x206d3c84) = count;
    if (H_SEEK(file, L32(0x206d3dc8) & 0xffff, 0) != 0)
        return 0xffffffffu;
    return H_READ(file, L32(0x206d3c80), L32(0x206d3c84), ADDR(&got)) != 0 ?
           0xffffffffu : 0;
}

static uint32_t fe_lib_write_words(uint32_t file)
{
    uint32_t position;
    uint32_t value;
    uint32_t wrote;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    value = position;
    if (H_WRITE(file, L32(0x206d3c80), L32(0x206d3c84), ADDR(&wrote)) != 0)
        return 0xffffffffu;
    return fe_lib_patch(file, 8, ADDR(&value)) != 0 ? 0xffffffffu : 0;
}

/* Records start at a header word, each behind a 1 marker; the lists end at a
 * byte that is not 1.  A read returns the record in a .data buffer, or null
 * at the end. */

/* .text+0x9590 -- a path record (word 7): marker, one byte dropped, string. */
static uint32_t fe_lib_read_path(uint32_t file, uint32_t header, uint32_t out)
{
    uint32_t position;
    uint8_t dropped;
    uint32_t got;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    if (position < M16(header + 0xe) &&
        H_SEEK(file, M16(header + 0xe), 0) != 0)
        return 0xffffffffu;
    if (!fe_lib_record_follows(file)) {
        M32(out) = 0;
        return 0;
    }
    if (H_READ(file, ADDR(&dropped), 1, ADDR(&got)) != 0)
        return 0xffffffffu;
    if (fe_lib_read_string(file, VA(0x206d2f28)) != 0)
        return 0xffffffffu;
    M32(out) = VA(0x206d2f28);
    return 0;
}

/* .text+0x9820 -- an area record (word 5): two bytes, the path made
 * absolute, the title.  The record is the 0x20c bytes at .data+0x3028. */
static uint32_t fe_lib_read_area(uint32_t file, uint32_t header, uint32_t out)
{
    uint32_t position;
    uint8_t dropped;
    uint32_t got;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    if (position < M16(header + 0xa) &&
        H_SEEK(file, M16(header + 0xa), 0) != 0)
        return 0xffffffffu;
    if (!fe_lib_record_follows(file)) {
        M32(out) = 0;
        return 0;
    }
    if (H_READ(file, ADDR(&dropped), 1, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d302c), 1, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d302d), 1, ADDR(&got)) != 0)
        return 0xffffffffu;
    if (fe_lib_read_string(file, VA(0x206d302e)) != 0)
        return 0xffffffffu;
    if (fe_lib_expand(VA(0x206d302e), VA(0x206d302e)) != 0)
        return 0xffffffffu;
    if (fe_lib_read_string(file, VA(0x206d3132)) != 0)
        return 0xffffffffu;
    M32(out) = VA(0x206d3028);
    return 0;
}

/* .text+0xa5b0 -- a BGL's title, its long name and its layer: the strings
 * at the header's words 8 and 9, and the word at 0x16. */
static uint32_t fe_lib_bgl_titles(uint32_t path, uint32_t name, uint32_t text,
                                  uint32_t layer)
{
    uint8_t folder[0x108];
    uint32_t file;
    uint16_t offset;
    uint32_t got;

    if (fe_lib_scenery_folder(ADDR(folder), path) != 0)
        return 0xffffffffu;
    if (H_OPEN(ADDR(&file), ADDR(folder), 0, 0) != 0)
        return 0xffffffffu;
    if (H_SEEK(file, 0x10, 0) != 0 ||
        H_READ(file, ADDR(&offset), 2, ADDR(&got)) != 0 ||
        H_SEEK(file, offset, 0) != 0 ||
        fe_lib_read_string(file, name) != 0 ||
        H_SEEK(file, 0x12, 0) != 0 ||
        H_READ(file, ADDR(&offset), 2, ADDR(&got)) != 0 ||
        H_SEEK(file, offset, 0) != 0 ||
        fe_lib_read_string(file, text) != 0 ||
        H_SEEK(file, 0x16, 0) != 0 ||
        H_READ(file, layer, 2, ADDR(&got)) != 0)
        return 0xffffffffu;
    return H_CLOSE(file) != 0 ? 0xffffffffu : 0;
}

/* .text+0x9bc0 -- a cache record (word 6) into the 0x410 bytes at
 * .data+0x3248, with its BGL's titles or, failing that, the path and the
 * 0xf448 string at layer 1000. */
static uint32_t fe_lib_read_cache(uint32_t file, uint32_t header, uint32_t out)
{
    uint32_t position;
    uint8_t dropped;
    uint32_t got;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    if (position < M16(header + 0xc) &&
        H_SEEK(file, M16(header + 0xc), 0) != 0)
        return 0xffffffffu;
    if (!fe_lib_record_follows(file)) {
        M32(out) = 0;
        return 0;
    }
    if (H_READ(file, ADDR(&dropped), 1, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d3448), 1, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d344a), 2, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d344e), 1, ADDR(&got)) != 0 ||
        H_READ(file, VA(0x206d344f), 1, ADDR(&got)) != 0)
        return 0xffffffffu;
    if (fe_lib_read_string(file, VA(0x206d3450)) != 0 ||
        fe_lib_read_string(file, VA(0x206d3554)) != 0)
        return 0xffffffffu;
    if (fe_lib_bgl_titles(VA(0x206d3554), VA(0x206d3248), VA(0x206d3348),
                          VA(0x206d344c)) != 0) {
        H_STRCPY(VA(0x206d3248), VA(0x206d3450));
        H_LOADSTRING(0xf448, VA(0x206d3348), 0x100);
        L16(0x206d344c) = 0x3e8;
    }
    M32(out) = VA(0x206d3248);
    return 0;
}

/* .text+0x94e0, +0x9770, +0x9b10 -- each list from the file, after emptying
 * it. */
static uint32_t fe_lib_read_list(uint32_t file, uint32_t list, uint32_t size,
                                 uint32_t (*read)(uint32_t, uint32_t, uint32_t))
{
    uint32_t record = 0;

    H_LIST_CLEAR(list);
    if (H_SEEK(file, 0, 0) != 0)
        return 0xffffffffu;
    if (read(file, VA(0x206d3238), ADDR(&record)) != 0)
        return 0xffffffffu;
    while (record != 0) {
        uint32_t slot = H_LIST_ADD(M32(list == VA(0x206d11a8) ?
                                       VA(0x206d1204) :
                                       list == VA(0x206d11c8) ?
                                       VA(0x206d1208) : VA(0x206d120c)));
        H_MEMMOVE(slot, record, size);
        if (record == 0)
            break;
        if (read(file, VA(0x206d3238), ADDR(&record)) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x9700 / +0x9a10 / +0x9e30 -- one record out. */
static uint32_t fe_lib_write_path(uint32_t file, uint32_t record)
{
    uint8_t marker = 1;
    uint32_t wrote;

    if (H_WRITE(file, ADDR(&marker), 1, ADDR(&wrote)) != 0)
        return 0xffffffffu;
    return H_WRITE(file, record, H_STRLEN(record) + 1, ADDR(&wrote)) != 0 ?
           0xffffffffu : 0;
}

static uint32_t fe_lib_write_area(uint32_t file, uint32_t record)
{
    uint8_t marker = 1;
    uint32_t wrote;

    if (H_WRITE(file, ADDR(&marker), 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 4, 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 5, 1, ADDR(&wrote)) != 0)
        return 0xffffffffu;
    /* The list's own copy of the path is rewritten in place. */
    if (fe_lib_contract(record + 6, record + 6, file) != 0)
        return 0xffffffffu;
    if (H_WRITE(file, record + 6, H_STRLEN(record + 6) + 1, ADDR(&wrote)) != 0)
        return 0xffffffffu;
    return H_WRITE(file, record + 0x10a, H_STRLEN(record + 0x10a) + 1,
                   ADDR(&wrote)) != 0 ? 0xffffffffu : 0;
}

static uint32_t fe_lib_write_cache(uint32_t file, uint32_t record)
{
    uint8_t marker = 1;
    uint32_t wrote;

    if (H_WRITE(file, ADDR(&marker), 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 0x200, 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 0x202, 2, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 0x206, 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 0x207, 1, ADDR(&wrote)) != 0 ||
        H_WRITE(file, record + 0x208, H_STRLEN(record + 0x208) + 1,
                ADDR(&wrote)) != 0)
        return 0xffffffffu;
    return H_WRITE(file, record + 0x30c, H_STRLEN(record + 0x30c) + 1,
                   ADDR(&wrote)) != 0 ? 0xffffffffu : 0;
}

/* .text+0x9660, +0x9970, +0x9d90 -- a list out at the current position, its
 * offset into header word `slot`, then a zero byte.  Returns the last
 * write's result as it is. */
static uint32_t fe_lib_write_list(uint32_t file, uint32_t list, uint32_t slot,
                                  uint32_t size,
                                  uint32_t (*write)(uint32_t, uint32_t))
{
    uint32_t count = M32(list);
    uint32_t record = M32(list + 0x18);
    uint32_t position;
    uint32_t value;
    uint32_t wrote;
    uint32_t index;

    if (H_TELL(file, ADDR(&position)) != 0)
        return 0xffffffffu;
    value = position;
    if (fe_lib_patch(file, slot, ADDR(&value)) != 0)
        return 0xffffffffu;
    for (index = 0; index < count; index++, record += size)
        if (write(file, record) != 0)
            return 0xffffffffu;
    return H_WRITE(file, VA(0x206d365c), 1, ADDR(&wrote));
}

/* .text+0xa230 -- the whole world file into .data. */
static uint32_t fe_lib_read_world(uint32_t file)
{
    uint32_t header = 0;

    if (fe_lib_read_header(file, ADDR(&header)) != 0)
        return 0xffffffffu;
    if (header == 0)
        return 0xffffffffu;
    if (H_MEMMOVE(VA(0x206d3dc0), VA(0x206d3238), 0x10) != 0)
        return 0xffffffffu;
    if (fe_lib_read_blob(file, VA(0x206d3dc0)) != 0 ||
        fe_lib_read_title(file, VA(0x206d3dc0), VA(0x206d3ee0)) != 0 ||
        fe_lib_read_title2(file, VA(0x206d3dc0), VA(0x206d3de0)) != 0 ||
        fe_lib_read_words(file, VA(0x206d3dc0)) != 0 ||
        fe_lib_read_list(file, VA(0x206d11a8), 0x100, fe_lib_read_path) != 0 ||
        fe_lib_read_list(file, VA(0x206d11c8), 0x20c, fe_lib_read_area) != 0)
        return 0xffffffffu;
    return fe_lib_read_list(file, VA(0x206d11e8), 0x410, fe_lib_read_cache);
}

/* .text+0xa330 -- and back out, the total size into word 0, truncated
 * there.  Refuses if either kept block is missing. */
static uint32_t fe_lib_write_world(uint32_t file)
{
    uint32_t size;

    if (L32(0x206d3c80) == 0 || L32(0x206d3c78) == 0)
        return 0xffffffffu;
    if (fe_lib_write_header(file, VA(0x206d3dc0)) != 0 ||
        fe_lib_write_blob(file) != 0 ||
        fe_lib_write_title(file, VA(0x206d3ee0)) != 0 ||
        fe_lib_write_title2(file, VA(0x206d3de0)) != 0 ||
        fe_lib_write_words(file) != 0 ||
        fe_lib_write_list(file, L32(0x206d1204), 0xe, 0x100,
                          fe_lib_write_path) != 0 ||
        fe_lib_write_list(file, L32(0x206d1208), 0xa, 0x20c,
                          fe_lib_write_area) != 0 ||
        fe_lib_write_list(file, L32(0x206d120c), 0xc, 0x410,
                          fe_lib_write_cache) != 0)
        return 0xffffffffu;
    if (H_TELL(file, ADDR(&size)) != 0)
        return 0xffffffffu;
    if (fe_lib_patch(file, 0, ADDR(&size)) != 0)
        return 0xffffffffu;
    return H_TRUNCATE(file, size) != 0 ? 0xffffffffu : 0;
}

/* .text+0xa9f0 -- free both kept blocks; always -1. */
static uint32_t fe_lib_free_blocks(void)
{
    if (L32(0x206d3c80) != 0 && FREE(VA(0x206d3c80)) != 0)
        return 0xffffffffu;
    if (L32(0x206d3c78) != 0)
        FREE(VA(0x206d3c78));
    return 0xffffffffu;
}

/* ---- opening and replacing ---------------------------------------------- */

/* .text+0x6060 -- the world file under the host's directory. */
static uint32_t fe_lib_open_world(uint32_t path, uint32_t out)
{
    uint32_t file;

    if (H_SPLIT(path, VA(0x206d1c68), VA(0x206d1d68), VA(0x206d1e68), 0) != 0)
        return 0xffffffffu;
    if (H_BASEDIR(VA(0x206d1f68)) != 0)
        return 0xffffffffu;
    if (H_JOIN(VA(0x206d2070), 0, VA(0x206d1f68), path) != 0)
        return 0xffffffffu;
    if (H_OPEN(ADDR(&file), VA(0x206d2070), 0, 0) != 0)
        return 0xffffffffu;
    M32(out) = file;
    return 0;
}

/* .text+0x5f70 -- a temporary file in the world file's directory.  The
 * joined name at .data+0x3b60 is built and not used. */
static uint32_t fe_lib_create_temp(uint32_t path, uint32_t temp,
                                   uint32_t scratch, uint32_t out)
{
    uint32_t file;

    if (H_SPLIT(path, VA(0x206d1758), VA(0x206d1858), VA(0x206d1958), 0) != 0)
        return 0xffffffffu;
    if (H_JOIN(scratch, VA(0x206d1758), VA(0x206d1858), 0) != 0)
        return 0xffffffffu;
    if (H_TEMP(temp, scratch) != 0)
        return 0xffffffffu;
    if (H_BASEDIR(VA(0x206d1a58)) != 0)
        return 0xffffffffu;
    if (H_JOIN(VA(0x206d1b60), 0, VA(0x206d1a58), path) != 0)
        return 0xffffffffu;
    if (H_CREATE(ADDR(&file), temp, 0xe) != 0)
        return 0xffffffffu;
    M32(out) = file;
    return 0;
}

/* .text+0x6110 -- the temporary over the original, attributes kept. */
static uint32_t fe_lib_replace(uint32_t path, uint32_t temp)
{
    uint32_t attributes;

    if (H_GET_ATTR(ADDR(&attributes), path, 0) != 0 ||
        H_SET_ATTR(1, path, 0) != 0 ||
        H_DELETE(path, 0) != 0 ||
        H_MOVE(path, 0, temp, 0xe) != 0 ||
        H_SET_ATTR(attributes, path, 0) != 0)
        return 0xffffffffu;
    return 0;
}

/* ---- dialog 0x1306: the world files ------------------------------------- */

/* .text+0x8f30 */
static uint32_t fe_lib_load_strings(void)
{
    H_LOADSTRING(0x9011, VA(0x206d3660), 0x100);
    H_LOADSTRING(0x9012, VA(0x206d3760), 0x104);
    H_LOADSTRING(0x9013, VA(0x206d3868), 0x100);
    H_LOADSTRING(0x9003, VA(0x206d3968), 0x104);
    H_LOADSTRING(0x9006, VA(0x206d3a70), 0x104);
    H_LOADSTRING(0x9015, VA(0x206d3b78), 0x100);
    return 0;
}

/* .text+0x5960 -- every world file in SCENERY, as 0x40-byte rows of the
 * list at .data+0x3218: path, title and second title. */
static uint32_t fe_lib_scan_worlds(void)
{
    uint8_t dir[0x104];
    uint8_t entry[0x110];
    uint32_t find = 0;
    uint32_t file;
    uint32_t header;
    uint32_t row;

    H_LOADSTRING(0x9003, ADDR(dir), 0x100);
    if (H_FIND_FIRST(ADDR(&find), ADDR(dir), 0, 0xe) != 0)
        return 0xffffffffu;
    while (H_FIND_NEXT(find, ADDR(entry)) == 0) {
        L32(0x206d0e38)++;
        row = H_LIST_ADD(VA(0x206d1218));
        if (H_ALLOC(5, 0x101, row + 8, 1) != 0 ||
            H_ALLOC(5, 0x101, row + 0x1c, 1) != 0 ||
            H_ALLOC(5, 0x101, row + 0x30, 1) != 0)
            return 0xffffffffu;
        H_JOIN(M32(row + 8), 0, ADDR(dir), ADDR(entry));
        if (H_OPEN(ADDR(&file), M32(row + 8), 0xe, 0) != 0)
            return 0xffffffffu;
        H_STRCAT(M32(row + 8), VA(0x206d0e48));
        header = 0;
        fe_lib_read_header(file, ADDR(&header));
        if (header == 0)
            return 0xffffffffu;
        if (fe_lib_read_title(file, header, M32(row + 0x1c)) != 0 ||
            fe_lib_read_title2(file, header, M32(row + 0x30)) != 0)
            return 0xffffffffu;
        M32(row) = 1;
        if (H_CLOSE(file) != 0)
            return 0xffffffffu;
    }
    H_FIND_CLOSE(ADDR(&find));
    return 0;
}

/* .text+0x5870 -- the block's list, selecting the world now in use. */
static uint32_t fe_lib_worlds(void)
{
    int32_t index;

    if (fe_lib_load_strings() != 0)
        return 0xffffffffu;
    if (fe_lib_scan_worlds() != 0)
        return 0xffffffffu;
    L32(0x206d0e38) = L32(0x206d1218);
    L32(0x206d0e3c) = L32(0x206d1230);
    L32(0x206d0e40) = 0;
    for (index = 0; index < (int32_t)L32(0x206d0e38); index++)
        if (H_STRICMP(M32(L32(0x206d0e3c) + (uint32_t)index * 0x40 + 8),
                      G + 0x446) == 0)
            L32(0x206d0e40) = (uint32_t)index;
    return 0;
}

/* .text+0x5b60 */
static uint32_t fe_lib_free_worlds(void)
{
    uint32_t count = L32(0x206d1218);
    uint32_t row;

    while (count != 0) {
        count--;
        row = L32(0x206d1230) + count * 0x40;
        if (M32(row + 8) != 0 && FREE(row + 8) != 0)
            return 0xffffffffu;
        if (M32(row + 0x1c) != 0 && FREE(row + 0x1c) != 0)
            return 0xffffffffu;
        if (M32(row + 0x30) != 0 && FREE(row + 0x30) != 0)
            return 0xffffffffu;
    }
    return H_LIST_FREE(VA(0x206d1218));
}

/* .text+0x5900 -- the blocks' do-nothing callback. */
static uint32_t FS98_STDCALL fe_lib_nothing(uint32_t block)
{
    (void)block;
    return 0;
}

/* Shared by the world list's two edit buttons (.text+0x5c00, +0x5db0): read
 * the selected world file, run an editor, and if it changed anything write
 * a temporary copy and put it in place. */
static uint32_t fe_lib_edit_world(uint32_t block, uint32_t areas)
{
    uint32_t row;
    uint32_t file;
    uint32_t temp;
    uint32_t changed = 0;

    if ((int32_t)M32(block + 0x18) >= (int32_t)M32(block + 0x10))
        return 0xffffffffu;
    row = M32(block + 0x14) + M32(block + 0x18) * 0x40;
    if (M32(row) == 0)
        return 0xffffffffu;
    if (fe_lib_open_world(M32(row + 8), ADDR(&file)) != 0)
        return 0xffffffffu;
    if (fe_lib_read_world(file) != 0) {
        if (areas)
            return 0xffffffffu;
        H_CLOSE(file);
        return 0xffffffffu;
    }
    if (H_CLOSE(file) != 0)
        return 0xffffffffu;
    if ((areas ? fe_lib_edit_areas(ADDR(&changed)) :
                 fe_lib_edit_caches(ADDR(&changed))) != 0)
        return 0xffffffffu;
    if (changed != 0) {
        row = M32(block + 0x14) + M32(block + 0x18) * 0x40;
        if (fe_lib_create_temp(M32(row + 8),
                               areas ? VA(0x206d1448) : VA(0x206d1658),
                               areas ? VA(0x206d1340) : VA(0x206d1550),
                               ADDR(&temp)) != 0)
            return 0xffffffffu;
        if (fe_lib_write_world(temp) != 0) {
            if (H_CLOSE(temp) != 0)
                return 0xffffffffu;
            H_DELETE(areas ? VA(0x206d1448) : VA(0x206d1658), 0xe);
            return 0xffffffffu;
        }
        if (H_CLOSE(temp) != 0)
            return 0xffffffffu;
        row = M32(block + 0x14) + M32(block + 0x18) * 0x40;
        if (fe_lib_replace(M32(row + 8),
                           areas ? VA(0x206d1448) : VA(0x206d1658)) != 0)
            return 0xffffffffu;
        row = M32(block + 0x14) + M32(block + 0x18) * 0x40;
        H_STRCPY(G + 0x446, M32(row + 8));
        M8(G + 0x444) |= areas ? 0x20 : 0x30;
    }
    fe_lib_free_blocks();
    return 0;
}

static uint32_t FS98_STDCALL fe_lib_world_areas(uint32_t block)
{
    return fe_lib_edit_world(block, 1);
}

static uint32_t FS98_STDCALL fe_lib_world_caches(uint32_t block)
{
    return fe_lib_edit_world(block, 0);
}

/* .text+0x5910 -- OK: make the selected world the one to load. */
static uint32_t fe_lib_choose_world(void)
{
    H_STRCPY(G + 0x446,
             M32(L32(0x206d0e3c) + (L32(0x206d0e40) << 6) + 8));
    M8(G + 0x365) = 0xf;
    return 0;
}

/* .text+0x57a0 -- Linkage +0x78.  The first time, a warning must be
 * accepted. */
static uint32_t FS98_STDCALL fe_library(void)
{
    uint32_t result;

    if (L32(0x206d133c) == 0 &&
        !(UTIL_ALERT(6, 1, 0xf440, 0xf441, 0xf442, 0xf443, 0, 0) & 2))
        return 0;
    L32(0x206d1218) = 0;
    L32(0x206d121c) = 0;
    L32(0x206d1220) = 0x20;
    L32(0x206d1224) = 0x40;
    L32(0x206d1228) = 0;
    L32(0x206d122c) = 0;
    L32(0x206d1230) = 0;
    L32(0x206d133c) = 1;
    result = fe_lib_worlds();
    if (result == 0) {
        result = H_DIALOG(0x1306, VA(0x206d0e28));
        if (result == 0 && L32(0x206d0e28) == 1)
            result = fe_lib_choose_world();
    }
    fe_lib_free_worlds();
    H_LIST_CLEAR(VA(0x206d1218));
    return result;
}

/* ---- dialog 0x13080005: area priority ----------------------------------- */

/* .text+0x6210 -- a working copy of `count` rows at .data+0x2e9c. */
static uint32_t fe_lib_copy_rows(uint32_t rows, uint32_t count)
{
    if (H_ALLOC(5, count << 6, VA(0x206d0e9c), 1) != 0)
        return 0xffffffffu;
    return H_MEMMOVE(L32(0x206d0e9c), rows, count << 6) != 0 ?
           0xffffffffu : 0;
}

/* .text+0x62d0 */
static uint32_t fe_lib_copy_back(uint32_t rows, uint32_t count)
{
    return H_MEMMOVE(rows, L32(0x206d0e9c), count << 6) != 0 ?
           0xffffffffu : 0;
}

/* .text+0x6190 */
static uint32_t fe_lib_priority(uint32_t title, uint32_t rows, uint32_t count)
{
    uint32_t result = fe_lib_copy_rows(rows, count);

    if (result == 0) {
        L32(0x206d0e94) = count;
        L32(0x206d0e90) = title;
        result = H_DIALOG(0x13080005, VA(0x206d0e78));
        if (result == 0 && L32(0x206d0e78) == 1)
            result = fe_lib_copy_back(rows, count);
    }
    FREE(VA(0x206d0e9c));
    return result;
}

/* .text+0x6260 -- validate the "move to" number: within 1..count, and not
 * below .data+0x2e70. */
static uint32_t FS98_STDCALL fe_lib_priority_check(uint32_t block)
{
    uint32_t result = 0;

    M32(block + 0x28) = 0;
    if ((int32_t)M32(block + 0x2c) > (int32_t)M32(block + 0x1c)) {
        M32(block + 0x28) = 0xb;
        result = 0xffffffffu;
    }
    if ((int32_t)M32(block + 0x2c) < (int32_t)L32(0x206d0e70)) {
        M32(block + 0x28) = 0xc;
        result = 0xffffffffu;
    }
    NUM_FORMAT(VA(0x206d0e70), 0x1001a, VA(0x206d217c));
    NUM_FORMAT(block + 0x1c, 0x1001a, VA(0x206d2174));
    return result;
}

/* .text+0x6300 / +0x6340 -- move the selection up or down by one. */
static uint32_t FS98_STDCALL fe_lib_priority_up(uint32_t block)
{
    uint32_t at = M32(block + 0x20);

    if ((int32_t)at >= (int32_t)M32(block + 0x1c))
        return 0xffffffffu;
    if (at != 0) {
        H_SWAP(M32(block + 0x24) + (at << 6),
               M32(block + 0x24) + (at << 6) - 0x40, 0x40);
        M32(block + 0x20)--;
    }
    return 0;
}

static uint32_t FS98_STDCALL fe_lib_priority_down(uint32_t block)
{
    uint32_t at = M32(block + 0x20);

    if ((int32_t)at >= (int32_t)M32(block + 0x1c))
        return 0xffffffffu;
    if (at != M32(block + 0x1c) - 1) {
        H_SWAP(M32(block + 0x24) + (at << 6),
               M32(block + 0x24) + ((at + 1) << 6), 0x40);
        M32(block + 0x20)++;
    }
    return 0;
}

/* .text+0x6400 -- by layer, highest first; equal layers by protected flag
 * (+0x3c) and then title, case-sensitively. */
static int32_t FS98_STDCALL fe_lib_priority_compare(const void *left,
                                                    const void *right)
{
    uint32_t a = ADDR(left);
    uint32_t b = ADDR(right);

    if ((int32_t)M32(a + 0x2c) < (int32_t)M32(b + 0x2c))
        return 1;
    if (M32(a + 0x2c) != M32(b + 0x2c))
        return -1;
    if (M32(a + 0x3c) == M32(b + 0x3c))
        return (int32_t)H_STRCMP(M32(a + 0x18), M32(b + 0x18));
    if (M32(a + 0x3c) != 0)
        return -1;
    return M32(b + 0x3c) != 0;
}

/* .text+0x6390 */
static uint32_t FS98_STDCALL fe_lib_priority_sort(uint32_t block)
{
    H_QSORT(M32(block + 0x24), M32(block + 0x1c), 0x40,
            ADDR(fe_lib_priority_compare));
    return 0;
}

/* .text+0x63c0 -- move the selection to the typed position. */
static uint32_t FS98_STDCALL fe_lib_priority_move(uint32_t block)
{
    uint32_t to = M32(block + 0x2c);

    if ((int32_t)to > (int32_t)M32(block + 0x1c) || to == 0)
        return 0xffffffffu;
    if (fe_lib_move(M32(block + 0x20), to - 1, L32(0x206d0e9c), 0x40) != 0)
        return 0xffffffffu;
    M32(block + 0x20) = M32(block + 0x2c) - 1;
    return 0;
}

/* ---- dialog 0x13080001: the area list ----------------------------------- */

/* .text+0x6560 -- each 0x20c-byte area through .data+0x4080 and UTIL's
 * exchange table at .data+0x2ef8 into a 0x40-byte row. */
static uint32_t fe_lib_area_rows(void)
{
    uint32_t list = L32(0x206d1208);
    uint32_t count = M32(list);
    uint32_t area = M32(list + 0x18);
    uint32_t index;
    uint32_t row;

    for (index = 0; index < count; index++, area += 0x20c) {
        if (H_MEMMOVE(VA(0x206d4080), area, 0x20c) != 0)
            return 0xffffffffu;
        row = H_LIST_ADD(VA(0x206d2188));
        if (H_ALLOC(5, 0x101, VA(0x206d42b8), 1) != 0 ||
            H_ALLOC(5, 0x101, VA(0x206d42a4), 1) != 0)
            return 0xffffffffu;
        UTIL_GET(VA(0x206d0ef8), 5, VA(0x206d42a0));
        if (H_MEMMOVE(row, VA(0x206d42a0), 0x40) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x6640 -- and back.  Fields the table does not cover come from
 * whatever .data+0x4080 last held. */
static uint32_t fe_lib_store_areas(void)
{
    uint32_t count = L32(0x206d0ee8);
    uint32_t row = L32(0x206d0eec);
    uint32_t index;
    uint32_t area;

    H_LIST_CLEAR(L32(0x206d1208));
    for (index = 0; index < count; index++, row += 0x40) {
        if (H_MEMMOVE(VA(0x206d42a0), row, 0x40) != 0)
            return 0xffffffffu;
        UTIL_PUT(VA(0x206d0ef8), 5, VA(0x206d42a0), 0);
        area = H_LIST_ADD(L32(0x206d1208));
        if (H_MEMMOVE(area, VA(0x206d4080), 0x20c) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x66e0 / +0x6740 -- the exchange callbacks for the two strings. */
static uint32_t FS98_STDCALL fe_lib_xfer_path(uint32_t a, uint32_t b,
                                              uint32_t mode)
{
    (void)a;
    (void)b;
    switch (mode) {
    case 0:
        H_STRCPY(L32(0x206d42a4), VA(0x206d4086));
        L32(0x206d42a8) = 0x104;
        return 0;
    case 1:
        H_STRCPY(VA(0x206d4086), L32(0x206d42a4));
        return 0;
    case 2:
        return 0;
    }
    return 0xffffffffu;
}

static uint32_t FS98_STDCALL fe_lib_xfer_title(uint32_t a, uint32_t b,
                                               uint32_t mode)
{
    (void)a;
    (void)b;
    switch (mode) {
    case 0:
        H_STRCPY(L32(0x206d42b8), VA(0x206d418a));
        L32(0x206d42bc) = 0x100;
        return 0;
    case 1:
        H_STRCPY(VA(0x206d418a), L32(0x206d42b8));
        return 0;
    case 2:
        return 0;
    }
    return 0xffffffffu;
}

/* .text+0x67a0 */
static uint32_t fe_lib_free_area_rows(void)
{
    uint32_t count = L32(0x206d2188);
    uint32_t row;

    while (count != 0) {
        count--;
        row = L32(0x206d21a0) + (count << 6);
        if (M32(row + 4) != 0 && FREE(row + 4) != 0)
            return 0xffffffffu;
        if (M32(row + 0x18) != 0 && FREE(row + 0x18) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x6520 */
static uint32_t fe_lib_area_list(void)
{
    if (fe_lib_area_rows() != 0)
        return 0xffffffffu;
    L32(0x206d0ee8) = L32(0x206d2188);
    L32(0x206d0eec) = L32(0x206d21a0);
    L32(0x206d0ef0) = 0;
    L32(0x206d0ec8) = 0;
    return 0;
}

/* .text+0x6460 */
static uint32_t fe_lib_edit_areas(uint32_t changed)
{
    uint32_t result;

    M32(changed) = 0;
    L32(0x206d2188) = 0;
    L32(0x206d218c) = 0;
    L32(0x206d2190) = 0x20;
    L32(0x206d2194) = 0x40;
    L32(0x206d2198) = 0;
    L32(0x206d219c) = 0;
    L32(0x206d21a0) = 0;
    result = fe_lib_area_list();
    if (result == 0) {
        L32(0x206d0ee4) = VA(0x206d3ee0);
        result = H_DIALOG(0x13080001, VA(0x206d0ec8));
        if (result == 0) {
            M32(changed) = L32(0x206d0ec8);
            if (L32(0x206d0ec8) == 1)
                result = fe_lib_store_areas();
            if (result == 0)
                result = fe_lib_free_area_rows();
        }
    }
    H_LIST_FREE(VA(0x206d2188));
    return result;
}

/* ---- dialogs 0x13080003 and 0x13080004: one area ------------------------- */

/* .text+0x8ce0 */
static uint32_t fe_lib_add_area_reset(void)
{
    uint32_t row = L32(0x206d117c);

    L32(0x206d1184) = 1;
    L32(0x206d1170) = 0;
    M32(row + 0x2c) = 0xa;
    M32(row + 0x34) = 0;
    M32(row + 0x3c) = 0;
    L32(0x206d1180) = 0;
    M32(row) = 0;
    M32(row + 0x14) = 0;
    M32(row + 0x28) = 0;
    M32(row + 0x30) = 0;
    M32(row + 0x38) = 0;
    return 0;
}

/* .text+0x8c50 */
static uint32_t fe_lib_add_area(uint32_t title, uint32_t count, uint32_t rows,
                                uint32_t done, uint32_t row, uint32_t at)
{
    uint32_t result;

    M32(done) = 0;
    L32(0x206d117c) = row;
    result = fe_lib_add_area_reset();
    L32(0x206d1178) = title;
    L32(0x206d119c) = count;
    L32(0x206d3fe8) = count;
    L32(0x206d3fe0) = rows;
    if (result == 0) {
        result = H_DIALOG(0x13080003, VA(0x206d1170));
        if (result == 0 && L32(0x206d1170) != 0) {
            result = fe_lib_nothing(row);
            M32(at) = L32(0x206d1184);
        }
    }
    M32(done) = L32(0x206d1170);
    return result;
}

/* .text+0x8a20 */
static uint32_t fe_lib_edit_area_reset(void)
{
    uint32_t row = L32(0x206d1144);

    L32(0x206d1148) = 0;
    M32(row) = 0;
    M32(row + 0x14) = 0;
    M32(row + 0x28) = 0;
    M32(row + 0x30) = 0;
    M32(row + 0x38) = 0;
    return 0;
}

/* .text+0x8990 */
static uint32_t fe_lib_edit_area(uint32_t title, uint32_t count, uint32_t rows,
                                 uint32_t done, uint32_t row, uint32_t at,
                                 uint32_t out)
{
    uint32_t result;

    L32(0x206d1144) = row;
    result = fe_lib_edit_area_reset();
    L32(0x206d1140) = title;
    L32(0x206d114c) = at;
    L32(0x206d3fe4) = at;
    L32(0x206d3fe0) = rows;
    L32(0x206d1164) = count;
    L32(0x206d3fe8) = count;
    if (result == 0) {
        result = H_DIALOG(0x13080004, VA(0x206d1138));
        if (result == 0 && L32(0x206d1138) == 1) {
            result = 0;
            M32(out) = L32(0x206d114c);
        }
    }
    M32(done) = L32(0x206d1138);
    return result;
}

/* .text+0x8a60 / +0x8d50 -- validate one area: the position, the path (a
 * valid, rooted BGL path in a SCENERY folder, not already listed, that
 * exists) and the layer (10, 20, 30, 40 or 50).  Statuses 5/6 out of range,
 * 0x1005 layer, 0x1006 a duplicate, 0x1007 a bad path.  The two differ in
 * the position bound, the duplicate check's rows, and whether a failed split
 * returns at once. */
static uint32_t fe_lib_check_area(uint32_t block, uint32_t minimum,
                                  int edit)
{
    uint8_t dir[0x104];
    uint32_t row;
    uint32_t result = 0;
    uint32_t layer;

    if (M32(block + 0x14) > M32(block + 0x2c)) {
        result = 0xffffffffu;
        M32(block + 0x10) = 6;
    } else if (M32(block + 0x14) < L32(minimum)) {
        result = 0xffffffffu;
        M32(block + 0x10) = 5;
    } else {
        M32(block + 0x10) = 0;
    }
    row = M32(block + 0xc);
    M32(row) = 0;
    if (H_SPLIT(M32(row + 4), 0, ADDR(dir), 0, 0) != 0) {
        if (edit)
            return 0xffffffffu;
        result = 0xffffffffu;
    }
    if (H_PATH_CHECK(ADDR(dir)) != 0) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    if (edit) {
        if (!fe_lib_is_bgl(M32(M32(block + 0xc) + 4))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc)) = 0x1007;
        }
        if (!fe_lib_rooted(M32(M32(block + 0xc) + 4))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc)) = 0x1007;
        }
    } else {
        if (!fe_lib_rooted(M32(M32(block + 0xc) + 4))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc)) = 0x1007;
        }
        if (!fe_lib_is_bgl(M32(M32(block + 0xc) + 4))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc)) = 0x1007;
        }
    }
    if (!fe_lib_is_scenery_dir(M32(M32(block + 0xc) + 4), block)) {
        M32(M32(block + 0xc)) = 0x1007;
        result = 0xffffffffu;
    } else if (fe_lib_conflicts(M32(M32(block + 0xc) + 4), L32(0x206d3fe8),
                                L32(0x206d3fe0),
                                edit ? L32(0x206d3fe4) - 1 :
                                       L32(0x206d3fe8) + 1)) {
        M32(M32(block + 0xc)) = 0x1006;
        result = 0xffffffffu;
    } else if (!fe_lib_area_exists(M32(M32(block + 0xc) + 4))) {
        M32(M32(block + 0xc)) = 0x1007;
        result = 0xffffffffu;
    }
    row = M32(block + 0xc);
    layer = M32(row + 0x2c);
    if (layer == 10 || layer == 20 || layer == 30 || layer == 40 ||
        layer == 50) {
        M32(row + 0x28) = 0;
    } else {
        M32(row + 0x28) = 0x1005;
        result = 0xffffffffu;
    }
    NUM_FORMAT(VA(minimum), 0x1001a,
               edit ? VA(0x206d2f0c) : VA(0x206d2f1c));
    NUM_FORMAT(block + 0x2c, 0x1001a,
               edit ? VA(0x206d2f04) : VA(0x206d2f14));
    return result;
}

static uint32_t FS98_STDCALL fe_lib_check_edit_area(uint32_t block)
{
    return fe_lib_check_area(block, 0x206d1130, 1);
}

static uint32_t FS98_STDCALL fe_lib_check_add_area(uint32_t block)
{
    return fe_lib_check_area(block, 0x206d1168, 0);
}

/* .text+0x6ca0 -- a row inserted at 1-based `at`. */
static uint32_t fe_lib_insert_row(uint32_t source, uint32_t at,
                                  uint32_t rows_out, uint32_t count_out)
{
    uint32_t row = H_LIST_ADD(VA(0x206d2188));

    if (H_MEMMOVE(row, source, 0x40) != 0)
        return 0xffffffffu;
    if (H_ALLOC(5, 0x100, row + 0x18, 1) != 0)
        return 0xffffffffu;
    if (H_ALLOC(5, 0x104, row + 4, 1) != 0)
        return 0xffffffffu;
    H_STRCPY(M32(row + 4), M32(source + 4));
    H_STRCPY(M32(row + 0x18), M32(source + 0x18));
    L32(0x206d0eec) = L32(0x206d21a0);
    L32(0x206d0ee8) = L32(0x206d2188);
    M32(rows_out) = L32(0x206d21a0);
    M32(count_out) = L32(0x206d0ee8);
    return fe_lib_move(L32(0x206d0ee8) - 1, at - 1, L32(0x206d0eec), 0x40);
}

/* .text+0x6810 -- Add: a new row through dialog 0x13080003, moved to the
 * position given, or removed again. */
static uint32_t FS98_STDCALL fe_lib_area_add(uint32_t block)
{
    uint32_t row = H_LIST_ADD(VA(0x206d2188));
    uint32_t done;
    uint32_t at;
    uint32_t result;

    if (H_MEMSET(row, 0, 0x40) != 0)
        return 0xffffffffu;
    M32(block + 0x24) = L32(0x206d21a0);
    M32(row + 0x1c) = 0x100;
    if (H_ALLOC(5, 0x101, row + 0x18, 1) != 0)
        return 0xffffffffu;
    M32(row + 0x1c) = 0x104;
    if (H_ALLOC(5, 0x105, row + 4, 1) != 0)
        return 0xffffffffu;
    result = fe_lib_add_area(M32(block + 0x1c), M32(block + 0x20),
                             M32(block + 0x24), ADDR(&done), row, ADDR(&at));
    if (result == 0 && done != 0) {
        fe_lib_move(M32(block + 0x20), at - 1, M32(block + 0x24), 0x40);
        M32(block + 0x20) = L32(0x206d2188);
        return result;
    }
    if (FREE(row + 0x18) != 0 || FREE(row + 4) != 0)
        return 0xffffffffu;
    H_LIST_REMOVE(VA(0x206d2188), row);
    M32(block + 0x24) = L32(0x206d21a0);
    M32(block + 0x20) = L32(0x206d2188);
    if (M32(block + 0x28) == L32(0x206d2188))
        M32(block + 0x28)--;
    return result;
}

/* .text+0x6990 -- Edit: a copy through dialog 0x13080004, then into the
 * row (keeping its own string blocks) and moved to the position given. */
static uint32_t FS98_STDCALL fe_lib_area_edit(uint32_t block)
{
    uint32_t selected = M32(block + 0x28);
    uint32_t row;
    uint8_t copy[0x40];
    uint32_t done;
    uint32_t at;
    uint32_t result;

    if ((int32_t)selected >= (int32_t)M32(block + 0x20))
        return 0xffffffffu;
    row = M32(block + 0x24) + (selected << 6);
    if (H_MEMMOVE(ADDR(copy), row, 0x40) != 0)
        return 0xffffffffu;
    if (H_ALLOC(5, 0x100, ADDR(copy) + 0x18, 1) != 0)
        return 0xffffffffu;
    if (H_ALLOC(5, 0x104, ADDR(copy) + 4, 1) != 0)
        return 0xffffffffu;
    H_STRCPY(M32(ADDR(copy) + 0x18), M32(row + 0x18));
    H_STRCPY(M32(ADDR(copy) + 4), M32(row + 4));
    result = fe_lib_edit_area(M32(block + 0x1c), M32(block + 0x20),
                              M32(block + 0x24), ADDR(&done), ADDR(copy),
                              M32(block + 0x28) + 1, ADDR(&at));
    if (result == 0 && done != 0) {
        uint32_t title = M32(row + 0x18);
        uint32_t path = M32(row + 4);

        if (H_MEMMOVE(row, ADDR(copy), 0x40) != 0)
            return 0xffffffffu;
        H_STRCPY(title, M32(row + 0x18));
        H_STRCPY(path, M32(row + 4));
        M32(row + 0x18) = title;
        M32(row + 4) = path;
        fe_lib_move(M32(block + 0x28), at - 1, L32(0x206d0eec), 0x40);
        L32(0x206d0ee8) = L32(0x206d2188);
    }
    if (FREE(ADDR(copy) + 0x18) != 0 || FREE(ADDR(copy) + 4) != 0)
        return 0xffffffffu;
    return result;
}

/* .text+0x6b70 -- Delete, after a confirmation; protected rows (+0x3c) not
 * at all. */
static uint32_t FS98_STDCALL fe_lib_area_delete(uint32_t block)
{
    uint32_t row;

    if ((int32_t)M32(block + 0x28) >= (int32_t)M32(block + 0x20))
        return 0xffffffffu;
    row = M32(block + 0x24) + (M32(block + 0x28) << 6);
    if (M32(row + 0x3c) != 0)
        return 0xffffffffu;
    if (!(UTIL_ALERT(6, 1, 0xf44b, VA(0x206d0ec0), M32(row + 0x18), 0, 0, 0) &
          2))
        return 0;
    row = M32(block + 0x24) + (M32(block + 0x28) << 6);
    if (M32(row + 0x18) != 0 && FREE(row + 0x18) != 0)
        return 0xffffffffu;
    row = M32(block + 0x24) + (M32(block + 0x28) << 6);
    if (M32(row + 4) != 0 && FREE(row + 4) != 0)
        return 0xffffffffu;
    H_LIST_REMOVE(VA(0x206d2188), M32(block + 0x24) + (M32(block + 0x28) << 6));
    M32(block + 0x24) = L32(0x206d21a0);
    M32(block + 0x20) = L32(0x206d2188);
    return 0;
}

static uint32_t fe_lib_browse(uint32_t title, uint32_t rows, uint32_t count);

/* .text+0x6c60 / +0x6c80 -- Browse and Priority. */
static uint32_t FS98_STDCALL fe_lib_area_browse(uint32_t block)
{
    return fe_lib_browse(M32(block + 0x1c), M32(block + 0x24),
                         M32(block + 0x20));
}

static uint32_t FS98_STDCALL fe_lib_area_priority(uint32_t block)
{
    return fe_lib_priority(M32(block + 0x1c), M32(block + 0x24),
                           M32(block + 0x20));
}

/* ---- dialog 0x13080002: areas from a drive ------------------------------- */

/* UTIL +0x94 lists every directory under a drive into 0x108-byte entries at
 * .data+0x4800 (count +0x4804, cursor +0x4808); the dialog steps through the
 * ones that are SCENERY folders. */
#define BROWSE_ENTRY(i) (L32(0x206d2800) + (i) * 0x108)

/* .text+0x7710 -- the cursor's path, fitted to 0x41 characters for show. */
static uint32_t fe_lib_browse_fit(uint32_t block)
{
    H_STRCPY(VA(0x206d26f8), M32(block + 0x24));
    H_FIT(M32(block + 0x24), M32(block + 0x24), 0x41);
    return 0;
}

/* .text+0x75f0 -- enable Next unless the cursor is the last SCENERY folder,
 * and Next Folder unless the next one shares its directory. */
static uint32_t fe_lib_browse_buttons(uint32_t block)
{
    uint8_t here[0x104];
    uint8_t next[0x104];
    uint32_t index = L32(0x206d2808);

    if (index == L32(0x206d2804) - 1) {
        M32(block + 0x7c) = 0;
        M32(block + 0x80) = 0;
    }
    do {
        index++;
        if (index >= L32(0x206d2804))
            break;
    } while (!fe_lib_is_scenery_dir(BROWSE_ENTRY(index), index));
    if (index == L32(0x206d2804)) {
        M32(block + 0x7c) = 0;
        M32(block + 0x80) = 0;
        return 0;
    }
    if (H_SPLIT(BROWSE_ENTRY(L32(0x206d2808)), 0, ADDR(here), 0, 0) != 0)
        return 0xffffffffu;
    if (H_SPLIT(BROWSE_ENTRY(index), 0, ADDR(next), 0, 0) != 0)
        return 0xffffffffu;
    if (H_STRICMP(ADDR(here), ADDR(next)) != 0)
        M32(block + 0x7c) = 0;
    return 0;
}

/* .text+0x7530 -- show the cursor's entry, or nothing past the end. */
static uint32_t fe_lib_browse_show(uint32_t block)
{
    if (L32(0x206d2808) < L32(0x206d2804)) {
        H_STRCPY(M32(block + 0x24), BROWSE_ENTRY(L32(0x206d2808)));
        H_STRCAT(M32(block + 0x24), VA(0x206d0f64));
        M32(block + 0x7c) = 1;
        M32(block + 0x80) = 1;
        if (fe_lib_browse_buttons(block) != 0)
            return 0xffffffffu;
    } else {
        M8(M32(block + 0x24)) = 0;
        L32(0x206d2808) = 0;
        L32(0x206d2804) = 0;
        M32(block + 0x74) = 0;
        M32(block + 0x78) = 0;
        M32(block + 0x7c) = 0;
        M32(block + 0x80) = 0;
        if (L32(0x206d2800) != 0 && FREE(VA(0x206d2800)) != 0)
            return 0xffffffffu;
    }
    return fe_lib_browse_fit(block) != 0 ? 0xffffffffu : 0;
}

/* .text+0x6f30 -- the drives that answer +0x150, as "X:\" entries. */
static uint32_t fe_lib_drives(void)
{
    uint32_t drive = L32(0x206d0ff4);
    uint32_t space[2];
    uint32_t left;

    H_LIST_CLEAR(VA(0x206d24c8));
    for (left = 0x1a; left != 0; left--) {
        if (H_FREE_SPACE(ADDR(&drive), ADDR(space)) == 0)
            H_STRCPY(H_LIST_ADD(VA(0x206d24c8)), ADDR(&drive));
        drive = (drive & 0xffffff00u) | (uint8_t)(drive + 1);
    }
    return 0;
}

/* .text+0x6e60 */
static uint32_t fe_lib_browse_reset(void)
{
    if (L32(0x206d2800) != 0 && FREE(VA(0x206d2800)) != 0)
        return 0xffffffffu;
    L32(0x206d2804) = 0;
    L32(0x206d2808) = 0;
    L32(0x206d0fdc) = 0;
    L32(0x206d0fe4) = 0;
    L32(0x206d0fe8) = 0;
    L32(0x206d0fec) = 0;
    L32(0x206d0ff0) = 0;
    fe_lib_drives();
    L32(0x206d0fbc) = 0xa;
    M8(L32(0x206d0f94)) = 0;
    M8(L32(0x206d0fa8)) = 0;
    L32(0x206d0fc4) = 0;
    L32(0x206d0fb8) = 0;
    L32(0x206d0f90) = 0;
    L32(0x206d0fa4) = 0;
    L32(0x206d0fc0) = 0;
    L32(0x206d0fcc) = 0;
    L32(0x206d0fd8) = L32(0x206d24c8);
    L32(0x206d0fe0) = L32(0x206d24e0);
    return 0;
}

/* .text+0x6d90 */
static uint32_t fe_lib_browse(uint32_t title, uint32_t rows, uint32_t count)
{
    uint32_t result;

    L32(0x206d24d0) = 0x20;
    L32(0x206d24c8) = 0;
    L32(0x206d24cc) = 0;
    L32(0x206d24d4) = 4;
    L32(0x206d24d8) = 0;
    L32(0x206d24dc) = 0;
    L32(0x206d24e0) = 0;
    L32(0x206d24c0) = rows;
    L32(0x206d22a8) = count;
    result = fe_lib_browse_reset();
    if (result == 0) {
        L32(0x206d0f8c) = title;
        result = H_DIALOG(0x13080002, VA(0x206d0f70));
        if (result == 0) {
            if (L32(0x206d0f70) == 1)
                result = 0;
            if (result == 0 && L32(0x206d2800) != 0)
                result = FREE(VA(0x206d2800));
        }
    }
    H_LIST_FREE(VA(0x206d24c8));
    return result;
}

/* .text+0x6f10 -- refuse to close while a duplicate is flagged. */
static uint32_t FS98_STDCALL fe_lib_browse_check(uint32_t block)
{
    if (L32(0x206d280c) != 0) {
        M32(block + 0x20) = 0x1006;
        return 0xffffffffu;
    }
    return 0;
}

/* .text+0x6fb0 -- Add the shown folder at the typed position. */
static uint32_t FS98_STDCALL fe_lib_browse_add(uint32_t block)
{
    uint8_t shown[0x104];

    L32(0x206d280c) = 0;
    if (M32(block + 0x64) == 0 || M32(block + 0x64) > L32(0x206d22a8))
        return 0xffffffffu;
    if (fe_lib_conflicts(VA(0x206d26f8), L32(0x206d22a8), L32(0x206d24c0),
                         L32(0x206d22a8) + 1)) {
        L32(0x206d280c) = 1;
        return 0;
    }
    H_STRCPY(ADDR(shown), M32(block + 0x24));
    H_STRCPY(M32(block + 0x24), VA(0x206d26f8));
    if (fe_lib_insert_row(block + 0x20, M32(block + 0x64), VA(0x206d24c0),
                          VA(0x206d22a8)) != 0) {
        H_STRCPY(M32(block + 0x24), ADDR(shown));
        return 0xffffffffu;
    }
    H_STRCPY(M32(block + 0x24), ADDR(shown));
    M32(block + 0x74) = 0;
    M32(block + 0x78) = 0;
    return 0;
}

/* .text+0x70b0 -- Add the whole directory: the shown path with its name
 * replaced by .data+0x2ff8's. */
static uint32_t FS98_STDCALL fe_lib_browse_add_all(uint32_t block)
{
    uint8_t name[8];
    uint8_t drive[0x110];
    uint8_t dir[0x100];
    uint8_t shown[0x104];

    fe_lib_copy(name, VA_PTR(0x206d0ff8), 4);
    fe_lib_copy(name + 4, VA_PTR(0x206d0ffc), 2);
    L32(0x206d280c) = 0;
    if (H_SPLIT(VA(0x206d26f8), ADDR(drive), ADDR(dir), 0, 0) != 0)
        return 0xffffffffu;
    if (H_JOIN(VA(0x206d26f8), ADDR(drive), ADDR(dir), ADDR(name)) != 0)
        return 0xffffffffu;
    if (fe_lib_conflicts(VA(0x206d26f8), L32(0x206d22a8), L32(0x206d24c0),
                         L32(0x206d22a8) + 1)) {
        L32(0x206d280c) = 1;
        return 0;
    }
    H_STRCPY(ADDR(shown), M32(block + 0x24));
    H_STRCPY(M32(block + 0x24), VA(0x206d26f8));
    if (fe_lib_browse_add(block) != 0) {
        H_STRCPY(M32(block + 0x24), ADDR(shown));
        return 0xffffffffu;
    }
    H_STRCPY(M32(block + 0x24), ADDR(shown));
    M32(block + 0x74) = 0;
    M32(block + 0x78) = 0;
    return 0;
}

/* .text+0x71f0 -- Search the selected drive, behind a modeless alert, and
 * show the first SCENERY folder. */
static uint32_t FS98_STDCALL fe_lib_browse_drive(uint32_t block)
{
    uint8_t text[0x80];
    uint32_t alert;
    uint32_t length;

    if ((int32_t)M32(block + 0x6c) >= (int32_t)M32(block + 0x68))
        return 0xffffffffu;
    if (L32(0x206d2800) != 0 && FREE(VA(0x206d2800)) != 0)
        return 0xffffffffu;
    if (!(UTIL_ALERT(6, 1, 0xf444, 0xf445, 0, 0, 0, 0) & 2))
        return 0;
    H_LOADSTRING(0xf446, ADDR(text), 0x80);
    length = H_STRLEN(ADDR(text));
    text[length] = M8(M32(block + 0x70) + M32(block + 0x6c) * 4);
    text[length + 1] = 0x2e;
    text[length + 2] = 0;
    UTIL_OPEN_ALERT(ADDR(&alert), 1, ADDR(text), 0xf447, 0, 0, 0, 0);
    UTIL_TREE(VA(0x206d2800), VA(0x206d2804),
              M32(block + 0x70) + M32(block + 0x6c) * 4, VA(0x206d0f60), 1);
    L32(0x206d2808) = 0;
    UTIL_CLOSE_ALERT(ADDR(&alert));
    M32(block + 0x74) = 1;
    M32(block + 0x78) = 1;
    M32(block + 0x7c) = 1;
    M32(block + 0x80) = 1;
    while (L32(0x206d2808) < L32(0x206d2804) &&
           !fe_lib_is_scenery_dir(BROWSE_ENTRY(L32(0x206d2808)), block))
        L32(0x206d2808)++;
    if (fe_lib_browse_show(block) != 0)
        return 0xffffffffu;
    return 0;
}

/* .text+0x7390 -- Next. */
static uint32_t FS98_STDCALL fe_lib_browse_next(uint32_t block)
{
    if (L32(0x206d2808) < L32(0x206d2804)) {
        do {
            L32(0x206d2808)++;
            if (L32(0x206d2808) >= L32(0x206d2804))
                break;
        } while (!fe_lib_is_scenery_dir(BROWSE_ENTRY(L32(0x206d2808)), 0));
    }
    M8(M32(block + 0x38)) = 0;
    if (fe_lib_browse_show(block) != 0)
        return 0xffffffffu;
    M32(block + 0x74) = 1;
    M32(block + 0x78) = 1;
    return 0;
}

/* .text+0x7400 -- Next Folder: the next SCENERY folder in another
 * directory. */
static uint32_t FS98_STDCALL fe_lib_browse_next_dir(uint32_t block)
{
    uint32_t index = L32(0x206d2808);

    if (index >= L32(0x206d2804) - 1) {
        L32(0x206d2808) = L32(0x206d2804);
        return fe_lib_browse_show(block);
    }
    M8(M32(block + 0x38)) = 0;
    if (H_SPLIT(BROWSE_ENTRY(L32(0x206d2808)), 0, VA(0x206d23b8), 0, 0) != 0)
        return 0xffffffffu;
    index++;
    if (H_SPLIT(BROWSE_ENTRY(index), 0, VA(0x206d22b0), 0, 0) != 0)
        return 0xffffffffu;
    while (index < L32(0x206d2804)) {
        if (fe_lib_is_scenery_dir(BROWSE_ENTRY(index), index * 0x108) &&
            H_STRICMP(VA(0x206d23b8), VA(0x206d22b0)) != 0)
            break;
        index++;
        if (H_SPLIT(BROWSE_ENTRY(index), 0, VA(0x206d22b0), 0, 0) != 0)
            return 0xffffffffu;
    }
    L32(0x206d2808) = index;
    if (fe_lib_browse_show(block) != 0)
        return 0xffffffffu;
    M32(block + 0x74) = 1;
    M32(block + 0x78) = 1;
    return 0;
}

/* ---- dialog 0x13080006: the cache list ---------------------------------- */

/* .text+0x7b20 -- free the rows' four strings, last row first. */
static uint32_t fe_lib_free_cache_rows(void)
{
    uint32_t count = L32(0x206d1028);
    uint32_t row;

    while (count != 0) {
        count--;
        row = L32(0x206d1040) + (count << 7);
        if (M32(row + 0x2c) != 0 && FREE(row + 0x2c) != 0)
            return 0xffffffffu;
        if (M32(row + 0x40) != 0 && FREE(row + 0x40) != 0)
            return 0xffffffffu;
        if (M32(row + 4) != 0 && FREE(row + 4) != 0)
            return 0xffffffffu;
        if (M32(row + 0x18) != 0 && FREE(row + 0x18) != 0)
            return 0xffffffffu;
    }
    L32(0x206d101c) = 0;
    L32(0x206d1024) = 0;
    return 0;
}

/* .text+0x7840 -- each 0x410-byte cache entry through .data+0x2810 and the
 * table at .data+0x3048 into a 0x80-byte row. */
static uint32_t fe_lib_cache_rows(void)
{
    uint32_t list = L32(0x206d120c);
    uint32_t count = M32(list);
    uint32_t entry = M32(list + 0x18);
    uint32_t index;

    fe_lib_free_cache_rows();
    H_LIST_CLEAR(VA(0x206d1028));
    for (index = 0; index < count; index++, entry += 0x410) {
        if (H_MEMMOVE(VA(0x206d2810), entry, 0x410) != 0)
            return 0xffffffffu;
        if (H_ALLOC(5, 0x105, VA(0x206d2c38), 1) != 0 ||
            H_ALLOC(5, 0x105, VA(0x206d2c24), 1) != 0 ||
            H_ALLOC(5, 0x101, VA(0x206d2c4c), 1) != 0 ||
            H_ALLOC(5, 0x101, VA(0x206d2c60), 1) != 0)
            return 0xffffffffu;
        UTIL_GET(VA(0x206d1048), 8, VA(0x206d2c20));
        if (H_MEMMOVE(H_LIST_ADD(VA(0x206d1028)), VA(0x206d2c20), 0x80) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x7970 */
static uint32_t fe_lib_store_caches(void)
{
    uint32_t count = L32(0x206d101c);
    uint32_t row = L32(0x206d1024);
    uint32_t index;

    H_LIST_CLEAR(L32(0x206d120c));
    for (index = 0; index < count; index++, row += 0x80) {
        if (H_MEMMOVE(VA(0x206d2c20), row, 0x80) != 0)
            return 0xffffffffu;
        UTIL_PUT(VA(0x206d1048), 8, VA(0x206d2c20), 0);
        if (H_MEMMOVE(H_LIST_ADD(L32(0x206d120c)), VA(0x206d2810), 0x410) != 0)
            return 0xffffffffu;
    }
    return 0;
}

/* .text+0x7a20..+0x7ad0 -- the exchange callbacks for the cache strings.
 * The first two only fill the control. */
static uint32_t FS98_STDCALL fe_lib_xfer_cache_name(uint32_t a, uint32_t b,
                                                    uint32_t mode)
{
    (void)a;
    (void)b;
    if (mode == 0) {
        H_STRCPY(L32(0x206d2c4c), VA(0x206d2810));
        L32(0x206d2c50) = 0x100;
    }
    return 0;
}

static uint32_t FS98_STDCALL fe_lib_xfer_cache_text(uint32_t a, uint32_t b,
                                                    uint32_t mode)
{
    (void)a;
    (void)b;
    if (mode == 0) {
        H_STRCPY(L32(0x206d2c60), VA(0x206d2910));
        L32(0x206d2c64) = 0x100;
    }
    return 0;
}

static uint32_t FS98_STDCALL fe_lib_xfer_cache_bgl(uint32_t a, uint32_t b,
                                                   uint32_t mode)
{
    (void)a;
    (void)b;
    if (mode == 0) {
        H_STRCPY(L32(0x206d2c24), VA(0x206d2b1c));
        L32(0x206d2c28) = 0x104;
    } else if (mode == 1) {
        H_STRCPY(VA(0x206d2b1c), L32(0x206d2c24));
    }
    return 0;
}

static uint32_t FS98_STDCALL fe_lib_xfer_cache_dir(uint32_t a, uint32_t b,
                                                   uint32_t mode)
{
    (void)a;
    (void)b;
    if (mode == 0) {
        H_STRCPY(L32(0x206d2c38), VA(0x206d2a18));
        L32(0x206d2c3c) = 0x104;
    } else if (mode == 1) {
        H_STRCPY(VA(0x206d2a18), L32(0x206d2c38));
    }
    return 0;
}

/* .text+0x77c0 */
static uint32_t fe_lib_cache_list(void)
{
    L32(0x206d1024) = 0;
    L32(0x206d101c) = 0;
    L32(0x206d1018) = VA(0x206d3ee0);
    L32(0x206d1000) = 0;
    if (fe_lib_cache_rows() != 0)
        return 0xffffffffu;
    L32(0x206d1024) = L32(0x206d1040);
    L32(0x206d2ca0) = 0;
    L32(0x206d101c) = L32(0x206d1028);
    return 0;
}

/* .text+0x7810 -- store, and flag a cache flush if one was asked for. */
static uint32_t fe_lib_cache_store(void)
{
    if (fe_lib_store_caches() != 0)
        return 0xffffffffu;
    if (L32(0x206d2ca0) != 0)
        M8(G + 0x444) |= 0x10;
    return 0;
}

/* .text+0x7750 */
static uint32_t fe_lib_edit_caches(uint32_t changed)
{
    uint32_t result;

    M32(changed) = 0;
    result = fe_lib_cache_list();
    if (result == 0) {
        result = H_DIALOG(0x13080006, VA(0x206d1000));
        if (result == 0) {
            if (L32(0x206d1000) == 1)
                result = fe_lib_cache_store();
            if (result == 0)
                result = fe_lib_free_cache_rows();
        }
    }
    H_LIST_CLEAR(VA(0x206d1028));
    M32(changed) = L32(0x206d1000);
    return result;
}

/* .text+0x7ee0 / +0x7f50 -- a name used by another row (the selected one
 * counts only with 0x1234), compared with the row's +0x18 or +4. */
static uint32_t fe_lib_cache_used(uint32_t name, uint32_t flag, uint32_t block,
                                  uint32_t field)
{
    int32_t index = (int32_t)M32(block + 0x1c);

    while (index > 0) {
        index--;
        if ((uint32_t)index == M32(block + 0x20) && flag != 0x1234)
            continue;
        if (H_STRICMP(M32(M32(block + 0x24) + ((uint32_t)index << 7) + field),
                      name) == 0)
            return 1;
    }
    return 0;
}

/* .text+0x8040 -- a working copy of the row at .data+0x4000, with its own
 * strings, and the layer range for validation. */
static uint32_t fe_lib_cache_copy(uint32_t row)
{
    L32(0x206d10e8) = 0;
    if (H_MEMMOVE(VA(0x206d4000), row, 0x80) != 0)
        return 0xffffffffu;
    if (H_ALLOC(5, 0x105, VA(0x206d4018), 1) != 0 ||
        H_ALLOC(5, 0x105, VA(0x206d4004), 1) != 0 ||
        H_ALLOC(5, 0x105, VA(0x206d402c), 1) != 0 ||
        H_ALLOC(5, 0x101, VA(0x206d4040), 1) != 0)
        return 0xffffffffu;
    H_STRCPY(L32(0x206d4018), M32(row + 0x18));
    H_STRCPY(L32(0x206d4004), M32(row + 4));
    H_STRCPY(L32(0x206d402c), M32(row + 0x2c));
    H_STRCPY(L32(0x206d4040), M32(row + 0x40));
    L32(0x206d4060) = 0;
    L32(0x206d4064) = 0;
    L32(0x206d4068) = 0;
    L32(0x206d406c) = VA(0x206d2cb0);
    L32(0x206d4070) = VA(0x206d2cc0);
    H_STRCPY(VA(0x206d2cd0), M32(row + 0x40));
    L32(0x206d2ca8) = M16(row + 0x74);
    L32(0x206d2cac) = (uint32_t)((int32_t)(M16(row + 0x74) * 5) >> 1);
    return 0;
}

/* .text+0x8460 -- the copy back into the row, which keeps its strings. */
static uint32_t fe_lib_cache_apply(uint32_t row)
{
    uint32_t text = M32(row + 0x40);
    uint32_t bgl = M32(row + 4);
    uint32_t dir = M32(row + 0x18);
    uint32_t name = M32(row + 0x2c);

    if (H_MEMMOVE(row, VA(0x206d4000), 0x80) != 0)
        return 0xffffffffu;
    M32(row + 4) = bgl;
    M32(row + 0x18) = dir;
    M32(row + 0x2c) = name;
    M32(row + 0x40) = text;
    H_STRCPY(bgl, L32(0x206d4004));
    H_STRCPY(M32(row + 0x18), L32(0x206d4018));
    H_STRCPY(M32(row + 0x2c), L32(0x206d402c));
    H_STRCPY(M32(row + 0x40), L32(0x206d4040));
    return 0;
}

/* .text+0x8510 */
static uint32_t fe_lib_cache_free_copy(void)
{
    if (FREE(VA(0x206d4018)) != 0 || FREE(VA(0x206d4004)) != 0 ||
        FREE(VA(0x206d402c)) != 0)
        return 0xffffffffu;
    return FREE(VA(0x206d4040)) != 0 ? 0xffffffffu : 0;
}

/* .text+0x7fc0 -- dialog 0x13080007, edit one cache row. */
static uint32_t fe_lib_edit_cache(uint32_t title, uint32_t row, uint32_t done,
                                  uint32_t block)
{
    uint32_t result;

    L32(0x206d10f0) = title;
    L32(0x206d10f4) = VA(0x206d4000);
    L32(0x206d10f8) = block;
    M32(done) = 0;
    result = fe_lib_cache_copy(row);
    if (result != 0)
        return result;
    result = H_DIALOG(0x13080007, VA(0x206d10e8));
    if (result != 0)
        return result;
    if (L32(0x206d10e8) != 0) {
        M32(done) = 1;
        result = fe_lib_cache_apply(row);
        if (result != 0)
            return result;
    }
    return fe_lib_cache_free_copy();
}

/* .text+0x85e0 */
static uint32_t fe_lib_new_cache_reset(void)
{
    uint32_t row = L32(0x206d1124);

    L32(0x206d1118) = 0;
    M32(row + 0x54) = 0;
    M32(row + 0x5c) = 0x3e8;
    M16(row + 0x74) = 0x3e8;
    M32(row + 0x7c) = 0;
    H_LOADSTRING(0xf448, M32(row + 0x40), 0x100);
    M32(row + 0x28) = 0;
    M32(row + 0x3c) = 0;
    M32(row + 0x14) = 0;
    M32(row) = 0;
    M32(row + 0x50) = 0;
    M32(row + 0x58) = 0;
    M32(row + 0x78) = 0;
    M32(row + 0x60) = 0;
    M32(row + 0x64) = 0;
    M32(row + 0x68) = 0;
    M32(row + 0x6c) = VA(0x206d2ee8);
    M32(row + 0x70) = VA(0x206d2ef8);
    return 0;
}

/* .text+0x8570 -- dialog 0x13080008, a new cache row. */
static uint32_t fe_lib_new_cache(uint32_t title, uint32_t row, uint32_t done,
                                 uint32_t block)
{
    uint32_t result;

    M32(done) = 0;
    L32(0x206d1120) = title;
    L32(0x206d1124) = row;
    L32(0x206d1128) = block;
    result = fe_lib_new_cache_reset();
    if (result == 0) {
        result = H_DIALOG(0x13080008, VA(0x206d1118));
        if (result == 0 && L32(0x206d1118) != 0)
            result = 0;
    }
    M32(done) = L32(0x206d1118);
    return result;
}

/* .text+0x81c0 / +0x86b0 -- validate a cache row: the layer within
 * [.data+0x4ca8, +0x4cac] (or +0x4dd8/+0x4ddc), the BGL path and the cache
 * directory.  When the path is good the row takes its BGL's titles and
 * layer, resetting the range if the title changed; otherwise the row is
 * named after the directory at layer 1000. */
static uint32_t fe_lib_check_cache(uint32_t block, int edit)
{
    uint32_t low = edit ? 0x206d2ca8 : 0x206d2dd8;
    uint32_t high = edit ? 0x206d2cac : 0x206d2ddc;
    uint32_t title = edit ? 0x206d2cd0 : 0x206d2de0;
    uint32_t flag = edit ? 0x1235 : 0x1234;
    uint32_t row = M32(block + 0xc);
    uint32_t result = 0;
    uint32_t bound;

    M32(row + 0x58) = 0;
    bound = L32(low);
    L32(high) = (uint32_t)((int32_t)(M16(M32(block + 0xc) + 0x74) * 5) >> 1);
    if (M32(M32(block + 0xc) + 0x5c) < bound) {
        M32(M32(block + 0xc) + 0x58) = 5;
        result = 0xffffffffu;
    }
    if (M32(M32(block + 0xc) + 0x5c) > L32(high)) {
        M32(M32(block + 0xc) + 0x58) = 6;
        result = 0xffffffffu;
    }
    M32(M32(block + 0xc)) = 0;
    if (H_PATH_CHECK(M32(M32(block + 0xc) + 4)) != 0) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    if (!fe_lib_rooted(M32(M32(block + 0xc) + 4))) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    if (fe_lib_cache_used(M32(M32(block + 0xc) + 4), flag, M32(block + 0x10),
                          4)) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1006;
    }
    if (!fe_lib_folder_exists(M32(M32(block + 0xc) + 4))) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    if (!fe_lib_has_scenery(M32(M32(block + 0xc) + 4))) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    if (!fe_lib_has_texture(M32(M32(block + 0xc) + 4))) {
        result = 0xffffffffu;
        M32(M32(block + 0xc)) = 0x1007;
    }
    M32(M32(block + 0xc) + 0x14) = 0;
    if (edit) {
        if (!fe_lib_rooted(M32(M32(block + 0xc) + 0x18))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (H_PATH_CHECK(M32(M32(block + 0xc) + 0x18)) != 0) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (fe_lib_cache_used(M32(M32(block + 0xc) + 0x18), flag,
                              M32(block + 0x10), 0x18))
            M32(M32(block + 0xc) + 0x14) = 0x1006;
    } else {
        if (H_PATH_CHECK(M32(M32(block + 0xc) + 0x18)) != 0) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (!fe_lib_rooted(M32(M32(block + 0xc) + 0x18))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (fe_lib_cache_used(M32(M32(block + 0xc) + 0x18), flag,
                              M32(block + 0x10), 0x18))
            M32(M32(block + 0xc) + 0x14) = 0x1006;
        if (!fe_lib_folder_exists(M32(M32(block + 0xc) + 0x18))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (!fe_lib_has_scenery(M32(M32(block + 0xc) + 0x18))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
        if (!fe_lib_has_texture(M32(M32(block + 0xc) + 0x18))) {
            result = 0xffffffffu;
            M32(M32(block + 0xc) + 0x14) = 0x1007;
        }
    }
    row = M32(block + 0xc);
    if (M32(row) == 0) {
        fe_lib_bgl_titles(M32(row + 4), M32(row + 0x2c), M32(row + 0x40),
                          row + 0x74);
        row = M32(block + 0xc);
        if (H_STRCMP(VA(title), M32(row + 0x40)) != 0) {
            M32(row + 0x5c) = M16(row + 0x74);
            M32(M32(block + 0xc) + 0x58) = 0;
            L32(low) = M16(M32(block + 0xc) + 0x74);
            L32(high) = (uint32_t)((int32_t)(M16(M32(block + 0xc) + 0x74) * 5)
                                   >> 1);
        }
    } else {
        H_STRCPY(M32(row + 0x2c), M32(row + 0x18));
        H_LOADSTRING(0xf448, M32(M32(block + 0xc) + 0x40), 0x100);
        M16(M32(block + 0xc) + 0x74) = 0x3e8;
        M32(M32(block + 0xc) + 0x5c) = 0x3e8;
        M32(M32(block + 0xc) + 0x58) = 0;
        L32(low) = 0x3e8;
        L32(high) = 0x9c4;
    }
    H_STRCPY(VA(title), M32(M32(block + 0xc) + 0x40));
    NUM_FORMAT(VA(low), 0x1001b, edit ? VA(0x206d2cb0) : VA(0x206d2ee8));
    NUM_FORMAT(VA(high), 0x1001b, edit ? VA(0x206d2cc0) : VA(0x206d2ef8));
    return result;
}

static uint32_t FS98_STDCALL fe_lib_check_edit_cache(uint32_t block)
{
    return fe_lib_check_cache(block, 1);
}

static uint32_t FS98_STDCALL fe_lib_check_new_cache(uint32_t block)
{
    return fe_lib_check_cache(block, 0);
}

/* .text+0x7bd0 -- Add a cache row. */
static uint32_t FS98_STDCALL fe_lib_cache_add(uint32_t block)
{
    uint8_t row[0x80];
    uint32_t done;

    if (H_MEMSET(ADDR(row), 0, 0x80) != 0)
        return 0xffffffffu;
    M32(ADDR(row) + 0x30) = 0x100;
    if (H_ALLOC(5, 0x101, ADDR(row) + 0x2c, 1) != 0)
        return 0xffffffffu;
    M32(ADDR(row) + 0x44) = 0x100;
    if (H_ALLOC(5, 0x101, ADDR(row) + 0x40, 1) != 0)
        return 0xffffffffu;
    M32(ADDR(row) + 0x1c) = 0x104;
    if (H_ALLOC(5, 0x105, ADDR(row) + 0x18, 1) != 0)
        return 0xffffffffu;
    M32(ADDR(row) + 8) = 0x104;
    if (H_ALLOC(5, 0x105, ADDR(row) + 4, 1) != 0)
        return 0xffffffffu;
    if (fe_lib_new_cache(M32(block + 0x18), ADDR(row), ADDR(&done),
                         block) != 0)
        return 0xffffffffu;
    if (done != 0) {
        if (H_MEMMOVE(H_LIST_ADD(VA(0x206d1028)), ADDR(row), 0x80) != 0)
            return 0xffffffffu;
        M32(block + 0x1c) = L32(0x206d1028);
        M32(block + 0x20) = L32(0x206d1028) - 1;
        M32(block + 0x24) = L32(0x206d1040);
        return 0;
    }
    if (FREE(ADDR(row) + 0x2c) != 0 || FREE(ADDR(row) + 0x40) != 0 ||
        FREE(ADDR(row) + 0x18) != 0 || FREE(ADDR(row) + 4) != 0)
        return 0xffffffffu;
    return 0;
}

/* .text+0x7df0 -- Edit the selected cache row. */
static uint32_t FS98_STDCALL fe_lib_cache_edit(uint32_t block)
{
    uint32_t done;
    uint32_t row = M32(block + 0x24) + (M32(block + 0x20) << 7);

    if ((int32_t)M32(block + 0x20) >= (int32_t)M32(block + 0x1c))
        return 0xffffffffu;
    return fe_lib_edit_cache(M32(block + 0x18), row, ADDR(&done), block) != 0 ?
           0xffffffffu : 0;
}

/* .text+0x7e30 -- Delete it, after a confirmation; its strings leak. */
static uint32_t FS98_STDCALL fe_lib_cache_delete(uint32_t block)
{
    if ((int32_t)M32(block + 0x20) >= (int32_t)M32(block + 0x1c))
        return 0xffffffffu;
    if (UTIL_ALERT(6, 1, 0xf44a, 0, 0, 0, 0, 0) & 2) {
        H_LIST_REMOVE(VA(0x206d1028),
                      M32(block + 0x24) + (M32(block + 0x20) << 7));
        M32(block + 0x1c) = L32(0x206d1028);
    }
    return 0;
}

/* .text+0x7ea0 -- Flush: confirmed, it is done when the list is stored. */
static uint32_t FS98_STDCALL fe_lib_cache_flush(uint32_t block)
{
    (void)block;
    if (UTIL_ALERT(2, 1, 0xf449, 0, 0, 0, 0, 0) & 2)
        L32(0x206d2ca0) = 1;
    return 0;
}

/* The .text targets of .data's relocations: the blocks' callbacks and the
 * exchange tables' string callbacks. */
static uint32_t fe_lib_text_target(uint32_t rva)
{
    switch (rva) {
    case 0x6900: return ADDR(fe_lib_nothing);
    case 0x6c00: return ADDR(fe_lib_world_areas);
    case 0x6db0: return ADDR(fe_lib_world_caches);
    case 0x7260: return ADDR(fe_lib_priority_check);
    case 0x7300: return ADDR(fe_lib_priority_up);
    case 0x7340: return ADDR(fe_lib_priority_down);
    case 0x7390: return ADDR(fe_lib_priority_sort);
    case 0x73c0: return ADDR(fe_lib_priority_move);
    case 0x7810: return ADDR(fe_lib_area_add);
    case 0x7990: return ADDR(fe_lib_area_edit);
    case 0x7b70: return ADDR(fe_lib_area_delete);
    case 0x7c60: return ADDR(fe_lib_area_browse);
    case 0x7c80: return ADDR(fe_lib_area_priority);
    case 0x76e0: return ADDR(fe_lib_xfer_path);
    case 0x7740: return ADDR(fe_lib_xfer_title);
    case 0x7f10: return ADDR(fe_lib_browse_check);
    case 0x7fb0: return ADDR(fe_lib_browse_add);
    case 0x80b0: return ADDR(fe_lib_browse_add_all);
    case 0x81f0: return ADDR(fe_lib_browse_drive);
    case 0x8390: return ADDR(fe_lib_browse_next);
    case 0x8400: return ADDR(fe_lib_browse_next_dir);
    case 0x8bd0: return ADDR(fe_lib_cache_add);
    case 0x8df0: return ADDR(fe_lib_cache_edit);
    case 0x8e30: return ADDR(fe_lib_cache_delete);
    case 0x8ea0: return ADDR(fe_lib_cache_flush);
    case 0x8a80: return ADDR(fe_lib_xfer_cache_bgl);
    case 0x8ad0: return ADDR(fe_lib_xfer_cache_dir);
    case 0x8a20: return ADDR(fe_lib_xfer_cache_name);
    case 0x8a50: return ADDR(fe_lib_xfer_cache_text);
    case 0x91c0: return ADDR(fe_lib_check_edit_cache);
    case 0x96b0: return ADDR(fe_lib_check_new_cache);
    case 0x9a60: return ADDR(fe_lib_check_edit_area);
    case 0x9d50: return ADDR(fe_lib_check_add_area);
    }
    return 0;
}
