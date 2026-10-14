/* UTIL.DLL's data-exchange engine: slots 0x8c and 0x90.
 *
 * These move values between GLOBAL variables and an array of per-control
 * records, driven by a table of 20-byte descriptors -- what MAIN's options
 * dialogs use to fill their controls from the sim's state (+0x90) and to write
 * the user's edits back (+0x8c).  The same descriptor table drives both.
 *
 *   +0x00  kind        0..8: which handler; 4 and 8 are the text ones
 *   +0x04  offset      of this control's record from the caller's base
 *   +0x08  subtype     what the variable is, see below
 *   +0x0c  variable    a pointer -- or, while the next field is 1, an offset
 *                      into GLOBAL
 *   +0x10  relocate    1: add GLOBAL's address to +0x0c, once, and clear this
 *
 * A record is two dwords: a status word and a value.  For text controls the
 * value is a char * to the control's text.
 *
 * Subtypes:
 *
 *   0x2002..0x200d   a dword, word, byte, dword, word, byte, ... variable
 *   0x2001           a callback, stdcall (kind, base, mode)
 *   0x2000           kinds 5..8 on read: the variable's address itself
 *   0x200e           kind 4: a string, strcpy'd
 *   other, kind 4/8  a NUMERIC format id + 0x1000: formatted by NUMERIC +0x18
 *                    on read, parsed by NUMERIC +0x1c on write
 *   0x41100000+m     kinds 0 and 5: a single-bit flag, m a mask of one bit --
 *   0x42200000+m     in a byte, a word or a dword respectively
 *   0x44400000+m
 *
 * Retail is hand-written assembly, and its dispatch is a single table in
 * .text: nine kind handlers followed by twelve width handlers, indexed by kind
 * on the way in and by (subtype - 0x1ff9) on the way down.  Every kind uses
 * the same formula for the second index, so the "per-kind" sub-tables are
 * really one table, and both indices can land anywhere in it:
 *
 *   - kinds 9..20 are the width handlers called directly;
 *   - a subtype 0x1ffd..0x2000 reaches a kind handler as if it were a width;
 *   - a subtype 0x1ff9..0x1ffc re-enters the kind handler that dispatched it,
 *     forever, and retail dies of stack overflow;
 *   - anything past either end of the table jumps into code bytes.
 *
 * The first two are reproduced.  The last two crash retail, and are inert
 * here: they do nothing and return 0.
 *
 * The callbacks are told the direction: 0 for a read, and for a write 1 to
 * commit or 2 to validate.  A write's `mode` argument is the other way round
 * -- 1 means validate -- and its handlers do not agree on what to do about it.
 * The width handlers skip their store only when mode is exactly 1; NUMERIC
 * parses into a scratch buffer for ANY non-zero mode; and the string and
 * bit-flag handlers ignore it and always write.  Reproduced as found.
 */

#include <stdint.h>

#include <fs98/host.h>

#include "util_imports.h"


void *fs98_util_global;     /* the harness's stand-in for the GLOBAL link */
void *fs98_util_numeric;    /* ... and for NUMERIC's */

void fs98_util_set_global(void *global)   { fs98_util_global  = global; }
void fs98_util_set_numeric(void *numeric) { fs98_util_numeric = numeric; }

/* NUMERIC +0x18 formats a value as text; +0x1c parses text into a value and
 * returns an error code.  Both stdcall, both three arguments by their `ret`. */
typedef char   *(FS98_STDCALL *fs98_numeric_format_fn)(const void *value,
                                                       uint32_t format, char *out);
typedef int32_t (FS98_STDCALL *fs98_numeric_parse_fn)(const char *text,
                                                      uint32_t format, void *out);
#define NUMERIC_FORMAT(n) (*(fs98_numeric_format_fn *)((unsigned char *)(n) + 0x18))
#define NUMERIC_PARSE(n)  (*(fs98_numeric_parse_fn  *)((unsigned char *)(n) + 0x1c))

typedef int32_t (FS98_STDCALL *fs98_util_xcb)(uint32_t kind, void *base, int32_t mode);

struct fs98_util_xdesc {
    uint32_t kind;
    uint32_t offset;
    uint32_t subtype;
    uint32_t var;
    uint32_t relocate;
};

struct fs98_util_xrec {
    uint32_t status;
    uint32_t value;
};

#define X_TABLE  21                 /* 9 kinds, 12 widths */
#define X_CALLBACK 0x2001u
#define X_ADDRESS  0x2000u
#define X_STRING   0x200eu
#define X_BITS8    0x41100000u
#define X_BITS16   0x42200000u
#define X_BITS32   0x44400000u

#define REC(d, base) ((struct fs98_util_xrec *)((unsigned char *)(base) + (d)->offset))
#define VAR(d)       ((unsigned char *)(uintptr_t)(d)->var)
#define CB(d)        ((fs98_util_xcb)(uintptr_t)(d)->var)

/* +0x1dc4: the one-time relocation both entry points start with.  It edits the
 * caller's table, and only for a flag of exactly 1. */
static void fs98_util_relocate(struct fs98_util_xdesc *d, uint32_t n)
{
    for (; n != 0; n--, d++)
        if (d->relocate == 1) {
            d->var += (uint32_t)(uintptr_t)FS98_UTIL_GLOBAL;
            d->relocate = 0;
        }
}

/* The second-level index: one formula for every kind. */
static uint32_t fs98_util_xindex(uint32_t subtype)
{
    return subtype - 0x1ff9u;
}

/* Is v a single set bit?  Retail's `bsr` / `bsf` / `cmp` -- and `bsr` of zero
 * sets ZF, which every caller tests first. */
static int fs98_util_one_bit(uint32_t v, unsigned *bit)
{
    unsigned lo = 0, hi = 31;
    if (v == 0)
        return -1;
    while (!(v >> lo & 1)) lo++;
    while (!(v >> hi & 1)) hi--;
    *bit = hi;
    return lo == hi;
}

/* The flag subtypes, decoded the way both bit handlers do it: try the byte
 * base, then the word, then the dword.  A difference of zero stops the search
 * at once; a single bit that is out of range for its width stops it too,
 * rather than trying the next width.  Returns the width in bytes, or 0. */
static unsigned fs98_util_bits(uint32_t subtype, uint32_t *mask)
{
    unsigned bit;
    int one;

    *mask = subtype - X_BITS8;
    one = fs98_util_one_bit(*mask, &bit);
    if (one < 0) return 0;
    if (one)     return bit < 8 ? 1 : 0;

    *mask = subtype - X_BITS16;
    one = fs98_util_one_bit(*mask, &bit);
    if (one < 0) return 0;
    if (one)     return bit < 16 ? 2 : 0;

    *mask = subtype - X_BITS32;
    one = fs98_util_one_bit(*mask, &bit);
    return one > 0 ? 4 : 0;
}

/* ---- slot 0x8c: records into variables ----------------------------------- */

typedef int32_t (*fs98_util_put_fn)(struct fs98_util_xdesc *, void *, int32_t);
static const fs98_util_put_fn fs98_util_put[X_TABLE];

static int32_t fs98_util_put_sub(struct fs98_util_xdesc *d, void *base, int32_t mode)
{
    uint32_t i = fs98_util_xindex(d->subtype);
    if (i < 4 || i >= X_TABLE)
        return 0;                               /* retail crashes */
    return fs98_util_put[i](d, base, mode);
}

/* the callback convention: commit is 1, validate is 2 */
static int32_t fs98_util_put_cb(struct fs98_util_xdesc *d, void *base, int32_t mode)
{
    return CB(d)(d->kind, base, mode == 1 ? 2 : 1);
}

/* +0x1ecb.  Ignores `mode`: a flag is always written. */
static int32_t fs98_util_put_bits(struct fs98_util_xdesc *d, void *base, int32_t mode)
{
    struct fs98_util_xrec *r = REC(d, base);
    uint32_t m;
    unsigned w = fs98_util_bits(d->subtype, &m);
    (void)mode;

    if (w == 1) {
        if (r->value == 1) *VAR(d) |= (uint8_t)m;
        else               *VAR(d) &= (uint8_t)~m;
    } else if (w == 2) {
        if (r->value == 1) *(uint16_t *)VAR(d) |= (uint16_t)m;
        else               *(uint16_t *)VAR(d) &= (uint16_t)~m;
    } else if (w == 4) {
        if (r->value == 1) *(uint32_t *)VAR(d) |= m;
        else               *(uint32_t *)VAR(d) &= ~m;
    }
    r->status = 0;
    return 0;
}

/* kinds 0..3: identical but for kind 0's flags */
static int32_t fs98_util_put_kind(struct fs98_util_xdesc *d, void *base, int32_t mode,
                                  int bits)
{
    int32_t r;
    if (bits && d->subtype >= X_BITS8)
        return fs98_util_put_bits(d, base, mode);
    if (d->subtype == X_CALLBACK)
        return fs98_util_put_cb(d, base, mode);
    r = fs98_util_put_sub(d, base, mode);
    REC(d, base)->status = (uint32_t)r;
    return r;
}

static int32_t fs98_util_put_k0(struct fs98_util_xdesc *d, void *b, int32_t m) { return fs98_util_put_kind(d, b, m, 1); }
static int32_t fs98_util_put_k1(struct fs98_util_xdesc *d, void *b, int32_t m) { return fs98_util_put_kind(d, b, m, 0); }

/* kind 4, text: a string copy, a callback, or a NUMERIC parse */
static int32_t fs98_util_put_k4(struct fs98_util_xdesc *d, void *base, int32_t mode)
{
    struct fs98_util_xrec *r = REC(d, base);
    static unsigned char scratch[32];           /* retail's .data+0x780 */
    int32_t e;

    if (d->subtype == X_STRING) {
        FS98_HOST_STRCPY(FS98_UTIL_HOST)((char *)VAR(d),
                                         (const char *)(uintptr_t)r->value);
        r->status = 0;
        return 0;
    }
    if (d->subtype == X_CALLBACK)
        return fs98_util_put_cb(d, base, mode);
    e = NUMERIC_PARSE(FS98_UTIL_NUMERIC)((const char *)(uintptr_t)r->value,
                                         d->subtype - 0x1000,
                                         mode != 0 ? (void *)scratch : (void *)VAR(d));
    r->status = (uint32_t)e;
    return e;
}

/* kinds 5..8 are read-only: their write handlers do nothing */
static int32_t fs98_util_put_none(struct fs98_util_xdesc *d, void *b, int32_t m)
{
    (void)d; (void)b; (void)m;
    return 0;
}

/* the twelve width handlers, dword / word / byte four times over */
static int32_t fs98_util_put_width(struct fs98_util_xdesc *d, void *base, int32_t mode,
                                   unsigned w)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (mode != 1) {
        if (w == 4)      *(uint32_t *)VAR(d) = r->value;
        else if (w == 2) *(uint16_t *)VAR(d) = (uint16_t)r->value;
        else             *VAR(d)             = (uint8_t)r->value;
    }
    r->status = 0;
    return 0;
}

static int32_t fs98_util_put_w4(struct fs98_util_xdesc *d, void *b, int32_t m) { return fs98_util_put_width(d, b, m, 4); }
static int32_t fs98_util_put_w2(struct fs98_util_xdesc *d, void *b, int32_t m) { return fs98_util_put_width(d, b, m, 2); }
static int32_t fs98_util_put_w1(struct fs98_util_xdesc *d, void *b, int32_t m) { return fs98_util_put_width(d, b, m, 1); }

static const fs98_util_put_fn fs98_util_put[X_TABLE] = {
    fs98_util_put_k0, fs98_util_put_k1, fs98_util_put_k1, fs98_util_put_k1,
    fs98_util_put_k4, fs98_util_put_none, fs98_util_put_none, fs98_util_put_none,
    fs98_util_put_none,
    fs98_util_put_w4, fs98_util_put_w2, fs98_util_put_w1,
    fs98_util_put_w4, fs98_util_put_w2, fs98_util_put_w1,
    fs98_util_put_w4, fs98_util_put_w2, fs98_util_put_w1,
    fs98_util_put_w4, fs98_util_put_w2, fs98_util_put_w1,
};

/* Returns 1 if any handler returned non-zero -- a NUMERIC parse error or a
 * callback's objection -- else 0. */
int32_t FS98_STDCALL fs98_util_exchange_put(void *descs, uint32_t n, void *base,
                                            int32_t mode)
{
    struct fs98_util_xdesc *d = descs;
    uint32_t any = 0;

    fs98_util_relocate(d, n);
    for (; n != 0; n--, d++)
        if (d->kind < X_TABLE)                  /* else retail crashes */
            any |= (uint32_t)fs98_util_put[d->kind](d, base, mode);
    return any != 0;
}

/* ---- slot 0x90: variables into records ----------------------------------- */

typedef void (*fs98_util_get_fn)(struct fs98_util_xdesc *, void *);
static const fs98_util_get_fn fs98_util_get[X_TABLE];

static void fs98_util_get_sub(struct fs98_util_xdesc *d, void *base)
{
    uint32_t i = fs98_util_xindex(d->subtype);
    if (i < 4 || i >= X_TABLE)
        return;                                 /* retail crashes */
    fs98_util_get[i](d, base);
}

/* +0x24f7 and +0x28c3: the flag as 0 or 1, into the value or the status */
static void fs98_util_get_bits(struct fs98_util_xdesc *d, uint32_t *out)
{
    uint32_t m;
    unsigned w = fs98_util_bits(d->subtype, &m);

    if (w == 1)      *out = (*VAR(d) & (uint8_t)m) != 0;
    else if (w == 2) *out = (*(uint16_t *)VAR(d) & (uint16_t)m) != 0;
    else if (w == 4) *out = (*(uint32_t *)VAR(d) & m) != 0;
}

/* kinds 0..3, and every path ends by clearing the status */
static void fs98_util_get_kind(struct fs98_util_xdesc *d, void *base, int bits)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (bits && d->subtype >= X_BITS8)
        fs98_util_get_bits(d, &r->value);
    else if (d->subtype == X_CALLBACK)
        CB(d)(d->kind, base, 0);
    else
        fs98_util_get_sub(d, base);
    r->status = 0;
}

static void fs98_util_get_k0(struct fs98_util_xdesc *d, void *b) { fs98_util_get_kind(d, b, 1); }
static void fs98_util_get_k1(struct fs98_util_xdesc *d, void *b) { fs98_util_get_kind(d, b, 0); }

/* kind 4, text: into the buffer the record's value points at */
static void fs98_util_get_k4(struct fs98_util_xdesc *d, void *base)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (d->subtype == X_STRING)
        FS98_HOST_STRCPY(FS98_UTIL_HOST)((char *)(uintptr_t)r->value,
                                         (const char *)VAR(d));
    else if (d->subtype == X_CALLBACK)
        CB(d)(d->kind, base, 0);
    else
        NUMERIC_FORMAT(FS98_UTIL_NUMERIC)(VAR(d), d->subtype - 0x1000,
                                          (char *)(uintptr_t)r->value);
    r->status = 0;
}

/* kinds 5..7 write the STATUS word, and do not clear it afterwards */
static void fs98_util_get_state(struct fs98_util_xdesc *d, void *base, int bits)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (bits && d->subtype >= X_BITS8)
        fs98_util_get_bits(d, &r->status);
    else if (d->subtype == X_CALLBACK)
        CB(d)(d->kind, base, 0);
    else if (d->subtype == X_ADDRESS)
        r->status = d->var;
    else
        r->status = *(uint32_t *)VAR(d);
}

static void fs98_util_get_k5(struct fs98_util_xdesc *d, void *b) { fs98_util_get_state(d, b, 1); }
static void fs98_util_get_k6(struct fs98_util_xdesc *d, void *b) { fs98_util_get_state(d, b, 0); }

/* kind 8: text into the buffer the record's STATUS word points at */
static void fs98_util_get_k8(struct fs98_util_xdesc *d, void *base)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (d->subtype == X_CALLBACK)
        CB(d)(d->kind, base, 0);
    else if (d->subtype == X_ADDRESS)
        FS98_HOST_STRCPY(FS98_UTIL_HOST)((char *)(uintptr_t)r->status,
                                         (const char *)VAR(d));
    else
        NUMERIC_FORMAT(FS98_UTIL_NUMERIC)(VAR(d), d->subtype - 0x1000,
                                          (char *)(uintptr_t)r->status);
}

static void fs98_util_get_width(struct fs98_util_xdesc *d, void *base, unsigned w)
{
    struct fs98_util_xrec *r = REC(d, base);
    if (w == 4)      r->value = *(uint32_t *)VAR(d);
    else if (w == 2) r->value = *(uint16_t *)VAR(d);
    else             r->value = *VAR(d);
    r->status = 0;
}

static void fs98_util_get_w4(struct fs98_util_xdesc *d, void *b) { fs98_util_get_width(d, b, 4); }
static void fs98_util_get_w2(struct fs98_util_xdesc *d, void *b) { fs98_util_get_width(d, b, 2); }
static void fs98_util_get_w1(struct fs98_util_xdesc *d, void *b) { fs98_util_get_width(d, b, 1); }

static const fs98_util_get_fn fs98_util_get[X_TABLE] = {
    fs98_util_get_k0, fs98_util_get_k1, fs98_util_get_k1, fs98_util_get_k1,
    fs98_util_get_k4, fs98_util_get_k5, fs98_util_get_k6, fs98_util_get_k6,
    fs98_util_get_k8,
    fs98_util_get_w4, fs98_util_get_w2, fs98_util_get_w1,
    fs98_util_get_w4, fs98_util_get_w2, fs98_util_get_w1,
    fs98_util_get_w4, fs98_util_get_w2, fs98_util_get_w1,
    fs98_util_get_w4, fs98_util_get_w2, fs98_util_get_w1,
};

/* Retail returns whatever the last handler left in EAX, which on some paths is
 * a bit index or the kind number.  Every caller in the retail modules ignores
 * it, so this returns nothing. */
void FS98_STDCALL fs98_util_exchange_get(void *descs, uint32_t n, void *base)
{
    struct fs98_util_xdesc *d = descs;

    fs98_util_relocate(d, n);
    for (; n != 0; n--, d++)
        if (d->kind < X_TABLE)                  /* else retail crashes */
            fs98_util_get[d->kind](d, base);
}
