/* MAIN.DLL: the Flights menu's C routines, .text+0xc560..+0xed10 (MSVC C).
 *
 * They sit over the situation reader and writer (slice 2) and are reached
 * from the host's menus, FACIL and the English\ dialog modules. */

/* .text+0xcdb0 == Linkage +0xb0: save the flight under string 0x2201's
 * title, with the description at .data+0x7fd0 (slice 17).  FACIL calls it.
 * The title buffer starts as the 17 bytes at .data+0x3634 ("Temporary
 * Flight" and its terminator) and zeros to 0x104, so a string the host
 * cannot load leaves that title.  The save is .text+0x120aa, Linkage +0x40,
 * called directly as retail calls it; its result is the answer. */
static uint32_t FS98_STDCALL flight_quicksave(void)
{
    uint8_t title[0x104];
    uint32_t i;

    for (i = 0; i < 17; i++)
        title[i] = M8(V(0x20803634) + i);
    for (; i < sizeof title; i++)
        title[i] = 0;
    H_LOADSTRING(0x2201, ADDR(title), 0x104);
    return save_flight(ADDR(title), V(0x20807fd0));
}

/* ---- slice 27: rewriting a situation file, and the Flights dialogs -------------- */

#define H_SPLIT     HOST_(0x188, f5)   /* (path, drive, dir, name, &ext) */
#define H_DELETE    HOST_(0x14c, f2)   /* (path, ext) */
#define H_RENAME    HOST_(0x16c, f4)   /* (to, to_ext, from, from_ext) */
#define H_PATHLEN   HOST_(0x170, f3)   /* (path, ext, &length) */
#define H_TEMPNAME  HOST_(0x174, f2)   /* (out, dir) */
#define H_NAMEOK    HOST_(0x178, f1)   /* (name): 0 if it will do as a file name */
#define H_STRCMP    HOST_(0x2bc, f2)
#define H_STRICMP   HOST_(0x2c0, f2)
#define H_ATTR_GET  HOST_(0x134, f3)   /* (&flags, path, ext) */
#define H_DIALOG    HOST_(0x10c, f2)   /* (id, descriptor) */
typedef uint32_t (FS98_STDCALL *f9)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);
#define UTIL_LIST   CALL(UTIL, 0x98, f9)
#define FLIGHT_REC(list, i)  ((list) + (i) * 0xa08u)

/* .text+0x13500: a record's header: its tag (a word, into the low half of *tag)
 * and, unless it is the end tag, its length, whose size the tag says.  Answers
 * the host's error, or -1 for a short read. */
static uint32_t rewrite_header(uint32_t file, uint32_t tag, uint32_t length)
{
    uint32_t got, r;
    uint16_t word;

    r = H_FILE_READ(file, tag, 2, ADDR(&got));
    if (r != 0)
        return r;
    if (got != 2)
        return 0xffffffffu;
    if (M16(tag) == 0xffff) {
        M32(length) = 0;
        return 0;
    }
    if (M16(tag) < 1000) {
        r = H_FILE_READ(file, ADDR(&word), 2, ADDR(&got));
        if (r != 0)
            return r;
        if (got != 2)
            return 0xffffffffu;
        M32(length) = word;
        return 0;
    }
    r = H_FILE_READ(file, length, 4, ADDR(&got));
    if (r != 0)
        return r;
    return got == 4 ? 0 : 0xffffffffu;
}

/* .text+0x13612: a record's body into the buffer *buffer of size *capacity,
 * growing it (a free and an allocation) when it will not fit. */
static uint32_t rewrite_body(uint32_t file, uint32_t tag, uint32_t length, uint32_t buffer,
                             uint32_t capacity)
{
    uint32_t n = length - 4, got, r;

    if (tag >= 1000)
        n -= 2;
    if (M32(buffer) == 0 || n > M32(capacity)) {
        if (M32(buffer) != 0) {
            M32(capacity) = 0;
            r = H_FREE(buffer);
            if (r != 0)
                return r;
        }
        r = H_ALLOC(1, n, buffer, 1);
        if (r != 0)
            return r;
        M32(capacity) = n;
    }
    r = H_FILE_READ(file, M32(buffer), n, ADDR(&got));
    if (r != 0)
        return r;
    return got == n ? 0 : 0xffffffffu;
}

/* .text+0x13710: write a record: tag, length (counting its header), body. */
static uint32_t rewrite_record(uint32_t file, uint32_t tag, uint32_t length, uint32_t body)
{
    uint32_t n = length - 4, wrote, r;
    uint16_t word = (uint16_t)tag;

    if (tag >= 1000)
        n -= 2;
    r = H_FILE_WRITE(file, ADDR(&word), 2, ADDR(&wrote));
    if (r != 0)
        return r;
    if (wrote != 2)
        return 0xffffffffu;
    if (tag < 1000) {
        word = (uint16_t)length;
        r = H_FILE_WRITE(file, ADDR(&word), 2, ADDR(&wrote));
        if (r != 0)
            return r;
        if (wrote != 2)
            return 0xffffffffu;
    } else {
        r = H_FILE_WRITE(file, ADDR(&length), 4, ADDR(&wrote));
        if (r != 0)
            return r;
        if (wrote != 4)
            return 0xffffffffu;
    }
    r = H_FILE_WRITE(file, body, n, ADDR(&wrote));
    if (r != 0)
        return r;
    return wrote == n ? 0 : 0xffffffffu;
}

/* .text+0x13105 == Linkage +0x50, assembly: replace record `tag` of situation
 * file `path` (extension 2) with `data`, `total` counting its header, appending
 * it before the end tag if it is not there, through a temporary file in the same
 * directory that then takes the original's name. */
static uint32_t FS98_STDCALL flight_set_record(uint32_t path, uint32_t tag, uint32_t data,
                                               uint32_t total)
{
    char drive[0x100], dir[0x100], temp[0x104], name[0x100];
    uint32_t in = 0, out = 0, buffer = 0, capacity = 0, replaced = 0, record = 0, length, r, wrote;
    static const uint16_t end_tag = 0xffff;

    r = H_EXISTS(path, 2);
    if (r != 0)
        return r;
    r = H_SPLIT(path, ADDR(drive), ADDR(dir), 0, 0);
    if (r != 0)
        goto closing;
    r = H_JOIN(ADDR(temp), ADDR(drive), ADDR(dir), 0);
    if (r != 0)
        goto closing;
    r = H_TEMPNAME(ADDR(name), ADDR(dir));
    if (r != 0)
        goto closing;
    r = H_JOIN(ADDR(temp), 0, 0, ADDR(name));
    if (r != 0)
        goto closing;
    r = H_FILE_OPEN(ADDR(&in), path, 2, 0);
    if (r != 0)
        goto closing;
    r = H_FILE_CREATE(ADDR(&out), ADDR(temp), 2);
    if (r != 0)
        goto closing;
    for (;;) {
        r = rewrite_header(in, ADDR(&record), ADDR(&length));
        if (r != 0)
            goto closing;
        if (record == 0xffff)
            break;
        r = rewrite_body(in, record, length, ADDR(&buffer), ADDR(&capacity));
        if (r != 0)
            goto closing;
        if (record != tag) {
            r = rewrite_record(out, record, length, buffer);
        } else {
            r = rewrite_record(out, tag, total, data);
            replaced = r == 0;
        }
        if (r != 0)
            goto closing;
    }
    if (!replaced) {
        r = rewrite_record(out, tag, total, data);
        if (r != 0)
            goto closing;
    }
    r = H_FILE_WRITE(out, ADDR(&end_tag), 2, ADDR(&wrote));
    if (r == 0 && wrote != 2)
        r = 0xffffffffu;
closing:
    if (in != 0)
        H_FILE_CLOSE(in);
    if (out != 0)
        H_FILE_CLOSE(out);
    if (buffer != 0)
        H_FREE(ADDR(&buffer));
    if (r != 0)
        return r;
    r = H_DELETE(path, 2);
    if (r != 0)
        return r;
    return H_RENAME(path, 2, ADDR(temp), 2);
}

/* .text+0x1680: the callback nobody needs: `xor eax, eax; ret 4`. */
static uint32_t FS98_STDCALL main_nothing(uint32_t a)
{
    (void)a;
    return 0;
}

/* .text+0xd180 and +0xd1f0: the flight list's title (record 0) and description
 * (record 2), through the finder (slice 1).  Answer 1 with the text, or 0 with
 * an empty one when the file has none. */
static uint32_t flight_list_text(uint32_t path, uint32_t text, uint32_t tag)
{
    uint32_t found, size;

    M8(text) = 0;
    record_find(path, tag, ADDR(&found), ADDR(&size));
    if (found == 0)
        return 0;
    H_MEMMOVE(text, found, size);
    M8(text + size) = 0;
    H_FREE(ADDR(&found));
    return 1;
}

static uint32_t FS98_STDCALL flight_list_title(uint32_t path, uint32_t text)
{
    return flight_list_text(path, text, 0);
}

static uint32_t FS98_STDCALL flight_list_description(uint32_t path, uint32_t text)
{
    return flight_list_text(path, text, 2);
}

/* .text+0xeb00: blank a string from `size` on down to its terminator. */
static void flight_blank(uint32_t text, uint32_t size)
{
    uint32_t n = H_STRLEN(text);

    if (size <= n) {
        M8(size + text) = 0;
        return;
    }
    do {
        M8(size + text) = 0;
        size--;
    } while (size > n);
}

/* .text+0xea80: a flight's title (record 0, 0x1f bytes zero-padded) */
static uint32_t flight_set_title(uint32_t path, uint32_t title)
{
    char copy[0x104];

    H_STRCPY(ADDR(copy), title);
    flight_blank(ADDR(copy), 0x1f);
    return flight_set_record(path, 0, ADDR(copy), 0x23) != 0 ? 0xffffffffu : 0;
}

/* .text+0xead0: and its description (record 2, in place, 0x100 bytes) */
static uint32_t flight_set_description(uint32_t path, uint32_t text)
{
    flight_blank(text, 0x100);
    return flight_set_record(path, 2, text, 0x104) != 0 ? 0xffffffffu : 0;
}

/* .text+0xeb30: is this the flight running (GLOBAL +0x24)? */
static uint32_t flight_is_current(uint32_t path)
{
    return H_STRCMP(path, G + 0x24) == 0 ? 1 : 0;
}

/* .text+0xd260: the Open Flight dialog's Delete.  A stock flight (+0xa04 clear)
 * cannot be; deleting the default flight or the one running gets a refusal; an
 * answer of 0x40 to the question deletes it and lists the flights again, the
 * selection pulled back if it fell off the end.  { answer, nothing, delete,
 * edit, set_default, selection, count, list } */
static uint32_t FS98_STDCALL flight_dialog_delete(uint32_t block)
{
    uint32_t rec = FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14));

    if (M32(rec + 0xa04) == 0)
        return 0xffffffffu;
    if (H_STRCMP(rec + 0x900, V(0x208040dc)) == 0) {
        UTIL_ALERT(1, 2, 0xf400, 0, 0, 0, 0, 0);
        return 0;
    }
    if (H_EXISTS(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0x900, 2) != 0) {
        UTIL_ALERT(2, 1, 0xf460, 0, 0, 0, 0, 0);
        return 0;
    }
    if (flight_is_current(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0x900) != 0) {
        UTIL_ALERT(2, 1, 0xf4d0, 0, 0, 0, 0, 1);
        return 0;
    }
    if (UTIL_ALERT(0xc0, 3, 0xf401, FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)), 0, 0, 0, 1) == 0x40) {
        H_DELETE(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0x900, 2);
        H_FREE(block + 0x1c);
        UTIL_LIST(0x9004, 0x9002, 2, block + 0x18, block + 0x1c, ADDR(flight_list_title),
                             ADDR(flight_list_description), 0, 1);
        if (M32(block + 0x14) >= M32(block + 0x18))
            M32(block + 0x14) = M32(block + 0x18) - 1;
    }
    return 0;
}

/* .text+0xd400: its Edit: a new title renames the file and rewrites record 0, a
 * new description record 2.  Answers a dialog error code: 0x1005 past the list,
 * 0x1010 a stock or read-only flight, 0x100d a bad name, 0x1001 too long a path,
 * 0x1004 one that exists; -1 a failure. */
static uint32_t FS98_STDCALL flight_dialog_edit(uint32_t block, uint32_t edit)
{
    char drive[0x100], dir[0x100], newpath[0x104], name[0x100];
    uint32_t flags = 0, length, rec, text;

    if (M32(block + 0x14) >= M32(block + 0x18))
        return 0x1005;
    if (M32(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0xa04) == 0)
        return 0x1010;
    H_ATTR_GET(ADDR(&flags), edit + 0x900, 2);
    if (flags & 2)
        return 0x1010;
    if (M8(edit) == 0) {
        text = edit + 0x100;
        if (M8(text) != 0) {
            if (flight_set_description(edit + 0x900, text) != 0)
                return 0xffffffffu;
            H_STRCPY(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0x100, text);
        }
        return 0;
    }
    if (H_NAMEOK(edit) != 0)
        return 0x100d;
    if (H_SPLIT(edit + 0x900, ADDR(drive), ADDR(dir), ADDR(name), 0) != 0)
        return 0x100d;
    if (H_JOIN(ADDR(newpath), ADDR(drive), ADDR(dir), edit) != 0)
        return 0x100d;
    if (H_PATHLEN(ADDR(newpath), 2, ADDR(&length)) != 0)
        return 0xffffffffu;
    if (length >= 0x104)
        return 0x1001;
    if (H_EXISTS(ADDR(newpath), 2) == 0)
        return 0x1004;
    rec = FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14));
    if (H_RENAME(ADDR(newpath), 2, rec + 0x900, 2) != 0)
        return 0xffffffffu;
    if (flight_set_title(ADDR(newpath), edit) != 0)
        return 0xffffffffu;
    H_STRCPY(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)) + 0x900, ADDR(newpath));
    H_STRCPY(FLIGHT_REC(M32(block + 0x1c), M32(block + 0x14)), edit);
    return 0;
}

/* .text+0xc6e0: the Open Flight dialog over the flights (0x9004 stock, 0x9002
 * the user's), its selection starting on `current`: the chosen path into `path`,
 * and into GLOBAL +0x24 when the block's +0x10 says it is the new default.
 * With no flights, an alert: retail then answers what the stack held; ours 0. */
static uint32_t FS98_STDCALL flight_dialog(uint32_t path, uint32_t current)
{
    volatile uint32_t block[8];
    uint32_t i;

    block[0] = 0;
    block[6] = 0;
    block[7] = 0;
    UTIL_LIST(0x9004, 0x9002, 2, ADDR(&block[6]), ADDR(&block[7]), ADDR(flight_list_title),
                         ADDR(flight_list_description), 0, 1);
    if (block[6] == 0) {
        UTIL_ALERT(8, 4, 0xf463, 0, 0, 0, 0, 0);
        return 0;
    }
    block[5] = 0;
    if (current != 0 && M8(current) != 0) {
        for (i = 0; (int32_t)i < (int32_t)block[6]; i++)
            if (H_STRICMP(current, FLIGHT_REC(block[7], i) + 0x900) == 0) {
                block[5] = i;
                break;
            }
    }
    block[1] = ADDR(main_nothing);
    block[2] = ADDR(flight_dialog_delete);
    block[3] = ADDR(flight_dialog_edit);
    block[4] = 0;
    H_DIALOG(0x1102, ADDR(block));
    if (block[0] != 0) {
        H_STRCPY(path, FLIGHT_REC(block[7], block[5]) + 0x900);
        if (block[4] != 0)
            H_STRCPY(G + 0x24, FLIGHT_REC(block[7], block[5]) + 0x900);
    }
    H_FREE(ADDR(&block[7]));
    return block[0];
}

/* .text+0xc560 == Linkage +0xc4: choose a flight and load it. */
static uint32_t FS98_STDCALL flight_open(void)
{
    struct { uint32_t ext; char path[0x104]; } flight;
    uint32_t r = flight_dialog(ADDR(flight.path), V(0x208040dc));

    if (r != 1)
        return r;
    flight.ext = 2;
    if (H_EXISTS(ADDR(flight.path), 2) != 0) {
        UTIL_ALERT(1, 1, 0xf460, 0, 0, 0, 0, 0);
        return r;
    }
    load_flight(ADDR(&flight));
    return r;
}

/* .text+0xc5e0, command 0x1106: the challenges (0x900a), dialog 0x1107.
 * { answer, nothing, count, selection, list } */
static uint32_t command_1106(void)
{
    volatile uint32_t block[5];
    struct { uint32_t ext; char path[0x104]; } flight;

    block[2] = 0;
    block[4] = 0;
    UTIL_LIST(0x900a, 0, 2, ADDR(&block[2]), ADDR(&block[4]), ADDR(flight_list_title),
                         ADDR(flight_list_description), 0, 1);
    block[0] = 0;
    block[1] = ADDR(main_nothing);
    block[3] = 0;
    H_DIALOG(0x1107, ADDR(block));
    if (block[0] != 0) {
        H_STRCPY(ADDR(flight.path), FLIGHT_REC(block[4], block[3]) + 0x900);
        flight.ext = 2;
        if (H_EXISTS(ADDR(flight.path), 2) != 0)
            UTIL_ALERT(1, 1, 0xf460, 0, 0, 0, 0, 0);
        else
            load_flight(ADDR(&flight));
    }
    H_FREE(ADDR(&block[4]));
    return block[0];
}

/* .text+0xd6b0: the Save Flight dialog's check of its file name (record +0xc),
 * into its status at +8: 0x1003 empty, 0x100d bad, 0x1001 too long, 0x1004 there
 * already; the last failure is the answer. */
static uint32_t FS98_STDCALL save_dialog_check(uint32_t records)
{
    char dir[0x104], path[0x108];
    uint32_t result = 0, length;

    M32(records + 8) = 0;
    if (M8(M32(records + 0xc)) == 0) {
        result = 0x1003;
        M32(records + 8) = 0x1003;
    }
    if (H_NAMEOK(M32(records + 0xc)) != 0) {
        M32(records + 8) = 0x100d;
        result = 0x100d;
    }
    H_LOADSTRING(0x9002, ADDR(dir), 0x104);
    if (H_JOIN(ADDR(path), 0, ADDR(dir), M32(records + 0xc)) != 0) {
        M32(records + 8) = 0x100d;
        result = 0x100d;
    }
    if (H_PATHLEN(ADDR(path), 2, ADDR(&length)) != 0) {
        M32(records + 8) = 0x100d;
        result = 0x100d;
    }
    if (length >= 0x104) {
        M32(records + 8) = 0x100d;
        result = 0x1001;
    }
    if (H_EXISTS(ADDR(path), 2) == 0) {
        result = 0x1004;
        M32(records + 8) = 0x1004;
    }
    return result;
}

/* .text+0xc870 == Linkage +0xac: Save Flight.  Refuses with under 5000 bytes
 * free; the file name's room comes down by how long the directories are. */
static uint32_t FS98_STDCALL flight_save_dialog(void)
{
    char cwd[0x104], dir[0x104], path[0x104];
    uint32_t space[2], r;

    if (HOST_(0x130, f1)(ADDR(cwd)) != 0)
        return 0xffffffffu;
    if (H_SPLIT(ADDR(cwd), ADDR(dir), 0, 0, 0) != 0)
        return 0xffffffffu;
    if (HOST_(0x150, f2)(ADDR(dir), ADDR(space)) != 0)
        return 0xffffffffu;
    if (space[1] == 0 && space[0] < 0x1388) {
        UTIL_ALERT(2, 1, 0xf4a0, 0xf4a1, 0, 0, 0, 0);
        return 0xffffffffu;
    }
    M32(V(0x208035f8)) = 0x104;
    M32(V(0x208035e8)) = 0;
    M32(V(0x20803618)) = 0;
    H_LOADSTRING(0x9002, ADDR(dir), 0x104);
    r = 0xfffffffcu - H_STRLEN(ADDR(cwd));
    r -= H_STRLEN(ADDR(dir));
    r -= H_STRLEN(V(0x2080361c));
    M32(V(0x208035f8)) = M32(V(0x208035f8)) + r;
    H_DIALOG(0x1104, V(0x208035e8));
    if (M32(V(0x208035e8)) != 0) {
        r = save_flight(V(0x208085c0), V(0x208086c8));
        if (r != 0) {
            if (r == 0x1001)
                UTIL_ALERT(2, 1, 0xf4a0, 0xf4a1, 0, 0, 0, 0);
            return 0xffffffffu;
        }
        if (M32(V(0x20803618)) != 0) {
            H_LOADSTRING(0x9002, ADDR(cwd), 0x104);
            H_JOIN(ADDR(path), 0, ADDR(cwd), V(0x208085c0));
            H_STRCPY(G + 0x24, ADDR(path));
        }
    }
    r = M32(V(0x208035e8));
    M8(V(0x208086c8)) = 0;
    return r;
}
