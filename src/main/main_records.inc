/* Situation records, .text+0x15c80..+0x15fff: the layer under the
 * situation reader and writer (.text+0x11e3d..+0x12f4f, assembly).
 *
 * A situation file is a run of records, each a 16-bit tag and then a
 * length that counts the header too: 16 bits for a tag below 1000, 32 bits
 * otherwise; tag 0xffff ends the file.  Reading, the records MAIN does not
 * know itself go out on CHAIN list 0x14 as message 0x22, and their owners
 * take the body through +0x58.  Writing, list 0x14 hears message 0x23 and
 * each module writes its own records through +0x5c, +0x60 and +0x64.
 */

#define S_FIND_PATH   V(0x208040dc)   /* the flight file +0x6c looks in */
#define S_RECORD      V(0x20809754)   /* the record being read, while it is */
#define S_READ_AT     V(0x20809758)   /* its body, from where +0x58 has got to */
#define S_FILE        V(0x2080975c)   /* the file being written, while it is */
#define S_PENDING     V(0x20809760)   /* what the record being written still owes */
#define S_READ_LEFT   V(0x2080d144)   /* what is left of the body being read */

/* .text+0x15ed0 -- +0x64: the rest of the record being written, as zeros.
 * Returns 0 (the host's free, or the empty count). */
static uint32_t FS98_STDCALL record_end(void)
{
    uint32_t zeros, r;

    if (M32(S_PENDING) == 0)
        return 0;
    H_ALLOC(1, M32(S_PENDING), ADDR(&zeros), 1);
    H_FILE_WRITE(M32(S_FILE), zeros, M32(S_PENDING), 0);
    r = H_FREE(ADDR(&zeros));
    M32(S_PENDING) = 0;
    return r;
}

/* .text+0x15cc0 -- +0x54: the record being read, through `out`.  -1 when
 * there is none. */
static uint32_t FS98_STDCALL record_current(uint32_t out)
{
    if (M32(S_RECORD) == 0)
        return 0xffffffffu;
    M32(out) = M32(S_RECORD);
    return 0;
}

/* .text+0x15ce0 -- +0x58: up to `n` bytes of the record being read into
 * `dst`, the count through `got` (if any).  -1, with 0 through `got`, when
 * nothing is left or nothing is asked for. */
static uint32_t FS98_STDCALL record_read(uint32_t dst, uint32_t n, uint32_t got)
{
    uint32_t take = M32(S_READ_LEFT);

    if (take == 0 || n == 0) {
        if (got != 0)
            M32(got) = 0;
        return 0xffffffffu;
    }
    if (take >= n)
        take = n;
    H_MEMMOVE(dst, M32(S_READ_AT), take);
    M32(S_READ_AT) = M32(S_READ_AT) + take;
    M32(S_READ_LEFT) = M32(S_READ_LEFT) - take;
    if (got != 0)
        M32(got) = take;
    return 0;
}

/* .text+0x15c80 -- a record the reader does not know, out to its owner:
 * `rec` is { ..., +8 length, +0xc body }.  Returns what CHAIN left in eax. */
static uint32_t dispatch_record(uint32_t rec)
{
    uint32_t r;

    M32(S_RECORD) = rec;
    M32(S_READ_LEFT) = M32(rec + 8);
    M32(S_READ_AT) = M32(rec + 0xc);
    r = CHAIN_SEND(0x14, 0x22, rec);
    M32(S_RECORD) = 0;
    return r;
}

/* .text+0x15d50 -- every module's records written into `file`. */
static uint32_t save_records(uint32_t file)
{
    uint32_t r;

    M32(S_FILE) = file;
    CHAIN_SEND(0x14, 0x23, 0);
    r = record_end();
    M32(S_FILE) = 0;
    return r;
}

/* .text+0x15d80 -- +0x5c: a record `tag` of `size` bytes begun, the last
 * one finished first.  -1 when no file is being written or `size` is 0.
 * Both header writes report into `size`'s own slot, which is why it is
 * read first. */
static uint32_t FS98_STDCALL record_begin(uint32_t tag, uint32_t size)
{
    uint32_t want = size, word;

    if (M32(S_FILE) == 0 || want == 0)
        return 0xffffffffu;
    record_end();
    word = tag;
    H_FILE_WRITE(M32(S_FILE), ADDR(&word), 2, ADDR(&size));
    if ((int32_t)tag < 1000) {
        word = want + 4;
        H_FILE_WRITE(M32(S_FILE), ADDR(&word), 2, ADDR(&size));
    } else {
        word = want + 6;
        H_FILE_WRITE(M32(S_FILE), ADDR(&word), 4, ADDR(&size));
    }
    M32(S_PENDING) = want;
    return 0;
}

/* .text+0x15e50 -- +0x60: `n` bytes of the record being written, no more
 * than it still owes; the count written through `wrote` (if any).  -1, with
 * 0 through `wrote`, when no record is open. */
static uint32_t FS98_STDCALL record_write(uint32_t src, uint32_t n, uint32_t wrote)
{
    uint32_t owed = M32(S_PENDING), done;

    if (M32(S_FILE) == 0 || owed == 0) {
        if (wrote != 0)
            M32(wrote) = 0;
        return 0xffffffffu;
    }
    if (n < owed) {
        M32(S_PENDING) = owed - n;
    } else {
        n = owed;
        M32(S_PENDING) = 0;
    }
    done = n;
    H_FILE_WRITE(M32(S_FILE), src, n, ADDR(&done));
    if (wrote != 0)
        M32(wrote) = done;
    return 0;
}

/* .text+0x15f50 -- +0x68: record `tag` of the situation file `path`, read
 * whole and copied into a block of its own through `out`, its length
 * through `size` (if any).  -1, with null through `out`, when the file is
 * not there or has no such record.
 *
 * The walk takes each record's length on trust: a length of 0 never moves
 * on (retail spins), one below the header's size steps back, and a huge
 * one wraps the index; past the end of the file it reads whatever follows
 * the block. */
static uint32_t FS98_STDCALL record_find(uint32_t path, uint32_t tag, uint32_t out,
                                         uint32_t size)
{
    uint32_t file, length, buf, at, id, body;

    if (size != 0)
        M32(size) = 0;
    M32(out) = 0;
    if (H_EXISTS(path, 2) != 0)
        return 0xffffffffu;
    if (H_FILE_OPEN(ADDR(&file), path, 2, 0) != 0)
        return 0xffffffffu;
    H_FILE_SIZE(file, ADDR(&length));
    H_ALLOC(1, length, ADDR(&buf), 0);
    H_FILE_READ(file, buf, length, 0);
    H_FILE_CLOSE(file);
    at = 0;
    if (length != 0) {
        do {
            id = ((uint32_t)M8(buf + at + 1) << 8) + M8(buf + at);
            at += 2;
            if ((int32_t)id < 1000) {
                body = ((uint32_t)M8(buf + at + 1) << 8) + M8(buf + at) - 4;
                at += 2;
            } else if (id == 0xffff) {
                break;
            } else {
                body = ((((((uint32_t)M8(buf + at + 3) << 8) + M8(buf + at + 2)) << 8) +
                         M8(buf + at + 1)) << 8) + M8(buf + at) - 6;
                at += 4;
            }
            if (id == tag) {
                H_ALLOC(1, body, out, 0);
                H_MEMMOVE(M32(out), buf + at, body);
                if (size != 0)
                    M32(size) = body;
                H_FREE(ADDR(&buf));
                return 0;
            }
            at += body;
        } while (at < length);
    }
    H_FREE(ADDR(&buf));
    return 0xffffffffu;
}

/* .text+0x15f30 -- +0x6c: the same in the flight file at +0x40dc. */
static uint32_t FS98_STDCALL record_find_flight(uint32_t tag, uint32_t out, uint32_t size)
{
    return record_find(S_FIND_PATH, tag, out, size);
}
