/* MAIN.DLL slice 32: the kneeboard, MSVC C.
 *
 * .text+0x1320 makes its view window at start (the bitmap, 101, is its page's
 * background, by LoadImageA and GetObjectA) with +0xb80 its window routine and
 * +0x1590 its close box; +0x1940 saves its place in the CFG and takes it down.
 * +0xfc0 hears CHAIN list 0x14: message 0x60 (a new aircraft) reads the pages out
 * of the aircraft's kneeboard file (+0xc30, section "Kneeboard" and its numbered
 * keys, with +0xaf0 cutting at commas and +0xa80 trimming) and message 0x70
 * draws the page again (+0x1620, with +0x15c0 the rules under each line and
 * +0x11b0 the window's shape from the bitmap's transparent runs).
 *
 * Retail details, reproduced:
 *   - +0x1620 hands the host's +0x318 a RECT whose bottom it never set, the first
 *     time; the harness does not read it.
 *   - +0x11b0 reads one byte past a row when it ends in transparent pixels. */

#define MENU_SET   HOST_(0x254, f2)       /* (id, state): bit 1 greyed, bit 0 checked */
#define KB_REC   M32(V(0x20807fc4))
#define KB_PAGE  M32(V(0x20807fc8))
#define WINDOW_  M32(V(0x208000b4))
#define MODULE23 M32(V(0x20800014))

/* .text+0xa80 */
static void FS98_STDCALL kb_trim(char *s)
{
    char *p, *d;

    if (s == NULL)
        return;
    p = s;
    while (*p == ' ' || *p == '\t')
        p = CharNextA(p);
    d = s;
    while (*p != 0) {
        *d = *p;
        p = CharNextA(p);
        d = CharNextA(d);
    }
    *d = 0;
    for (p = CharPrevA(s, d); *p == ' ' || *p == '\t'; p = CharPrevA(s, p))
        *p = 0;
}

/* .text+0xaf0: the field up to `sep` copied to out and trimmed; where the next
 * starts, or 0 if this was the last. */
static char *kb_field(char *s, char *out, uint8_t sep)
{
    char *end, *d;

    if (s == NULL)
        return NULL;
    end = strchr(s, sep);
    if (end == NULL) {
        HOST_(0x2a8, f2)(ADDR(out), ADDR(s));
        kb_trim(out);
        return NULL;
    }
    if (out != NULL) {
        d = out;
        while (s != end) {
            *d = *s;
            s = CharNextA(s);
            d = CharNextA(d);
        }
        *d = 0;
        kb_trim(out);
    }
    return end + 1;
}

/* .text+0xb80 */
static uint32_t FS98_STDCALL kb_window(uint32_t message, uint32_t rec, uint32_t w, uint32_t l)
{
    uint32_t flags, x, y;

    (void)l;
    if (message != 2 || view_active() != 1)
        return message;
    flags = M32(rec + 8);
    if (flags & 0x40000000u)
        return message;
    y = M32(rec + 0x24) + M32(rec + 0x2c);
    x = M32(rec + 0x20) + M32(rec + 0x28);
    if (x > 0x1f9c && x < 0x2064 && y > 0x179c && y < 0x1864) {
        if ((w & 0xffff0000u) == 0 || (flags & 0x5000000u))
            return message;
        M32(rec + 8) = flags | 0x5000000u;
        return HOST_(0x7c, f1)(rec);
    }
    if (w == 0 && (flags & 0x5000000u))
        M32(rec + 8) = flags & 0xfaffffffu;
    return message;
}

typedef uint32_t (FS98_STDCALL *f6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f7)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
#define H_INI_READ  HOST_(0xf0, f6)      /* (section, key, out, size, path, ext) */
#define H_DRAW_TEXT HOST_(0x318, f7)     /* (rec, &rc, ?, ?, text, flags, 0) */

/* .text+0xc30: the pages, from the aircraft's kneeboard file. */
static uint32_t kb_load(void)
{
    char path[0x104], buf[0x400], tmp[0x100], key[12];
    uint32_t n = 0, i, page, at, list, line;
    char *rest;

    if (M32(V(0x2080fda4)) != 0)
        for (i = 0; (int32_t)M32(V(0x2080fda4)) >= (int32_t)i; i++)
            HOST_(0x25c, f1)(i + 0x300);
    if (CALL(MODULE23, 0x30, f4)(G + 0x11e0, 0xb, ADDR(path), 0x104) == 0) {
        M32(V(0x2080fda4)) = 0;
        return 0;
    }
    HOST_(0x2a8, f2)(V(0x2080dca0), V(0x20800264));
    for (at = V(0x2080dda4); at < V(0x2080eda4); at += 0x100) {
        M8(at + 0x1000) = 0;
        M8(at) = 0;
    }
    memset(buf, 0, sizeof buf);
    buf[0] = (char)M8(V(0x20807fd0));
    _itoa(0, key, 10);
    if (H_INI_READ(V(0x2080dca0), ADDR(key), ADDR(buf), 0x400, ADDR(path), 0x24) != 0xffffffffu) {
        at = V(0x2080eda4);
        for (;;) {
            rest = kb_field(buf, tmp, 0x2c);
            HOST_(0x2a8, f2)(at, ADDR(tmp));
            if (rest != NULL) {
                kb_field(rest, tmp, 0x2c);
                HOST_(0x2a8, f2)(at - 0x1000, ADDR(tmp));
            }
            if (M8(at) == 0 || (int32_t)at >= (int32_t)V(0x2080fda4))
                break;
            memset(buf, 0, sizeof buf);
            buf[0] = (char)M8(V(0x20807fd0));
            n++;
            at += 0x100;
            _itoa((int)n, key, 10);
            if (H_INI_READ(V(0x2080dca0), ADDR(key), ADDR(buf), 0x400, ADDR(path), 0x24) == 0xffffffffu)
                break;
        }
    }
    M32(V(0x2080fda4)) = n;
    if (n >= 1) {
        list = V(0x2080fda8);
        for (page = 1; page <= M32(V(0x2080fda4)); page++) {
            HOST_(0x2a8, f2)(list, ADDR(_itoa((int)(page - 1), key, 10)));
            for (i = 0; i < 0x10; i++)
                M8(list + 0x1104 + i * 0x100) = 0;
            line = 0xffffffffu;
            at = list + 0x1004;
            do {
                line++;
                _itoa((int)line, key, 10);
                at += 0x100;
                H_INI_READ(list, ADDR(key), at, 0x100, ADDR(path), 0x24);
            } while (M8(at) != 0);
            M32(list + 0x2104) = line;
            list += 0x2108;
        }
    }
    if (M32(V(0x2080fda4)) == 0) {
        MENU_SET(0x1208, 2);
        return 0;
    }
    MENU_SET(0x1208, 0);
    at = V(0x2080eda4);
    for (i = 0; (int32_t)i <= (int32_t)(M32(V(0x2080fda4)) - 1); i++) {
        HOST_(0x244, f4)(0x1208, i + 0x300, at, 0);
        at += 0x100;
    }
    return 1;
}

/* .text+0x11b0: the window's shape: a rectangle per opaque run of the bitmap,
 * for the host. */
static uint32_t FS98_STDCALL kb_shape(uint32_t bm)
{
    uint32_t p, q, out, y, left, x, row, count;
    uint8_t clear;

    if (H_ALLOC(1, M32(bm + 8) * 25 * 8 + 0x14, ADDR(&p), 0) != 0)
        return 0;
    H_MEMMOVE(p, V(0x20800284), 8);
    M32(p + 8) = 1;
    M32(p + 0xc) = 0;
    out = p + 0x10;
    for (y = 0; (int32_t)y < (int32_t)M32(bm + 8); y++) {
        left = M32(bm + 4);
        row = M32(M32(bm + 0x10) + y * 4);
        x = 0;
        while (left != 0) {
            clear = M8(M32(HG2D + 0x90));
            if (M8(row) == clear) {
                while (left != 0) {
                    uint8_t c = M8(row + 1);

                    left--;
                    x++;
                    row++;
                    if (c != clear)
                        break;
                }
            }
            count = x;
            if (M8(row) != clear) {
                while (left != 0) {
                    left--;
                    x++;
                    row++;
                    if (M8(row) == clear)
                        break;
                }
            }
            if (count != x) {
                M32(out) = 1;
                M32(out + 4) = count;
                M32(out + 8) = y;
                M32(out + 0xc) = x;
                M32(out + 0x10) = y + 1;
                out += 0x14;
                M32(p + 0xc) += 1;
            }
        }
    }
    count = (M32(p + 0xc) + 1) * 20;
    if (H_ALLOC(1, count, ADDR(&q), 0) != 0)
        return p;
    H_MEMMOVE(q, p, count);
    H_FREE(ADDR(&p));
    return q;
}

/* .text+0x15c0: the rule under a line: four rows of it cleared. */
static uint32_t FS98_STDCALL kb_rule(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    uint32_t rec = KB_REC, y = top + 5, i;

    (void)right;
    (void)bottom;
    for (i = 0; i < 4; i++, y++) {
        if (y < M32(rec + 0x3c) - 1) {
            H_MEMSET(M32(M32(rec + 0x90) + y * 4) + left - 0xa, 0, 4);
            rec = KB_REC;
        }
    }
    return 0;
}

/* .text+0x1620: page `page` drawn over the bitmap. */
static uint32_t FS98_STDCALL kb_draw(uint32_t page)
{
    BITMAP bm;
    int32_t rc[4], full[4];
    uint32_t rec, y, x, src, dst, stride, text, list;
    int32_t height;

    GetObjectA((HGDIOBJ)(uintptr_t)M32(V(0x2080dc84)), sizeof bm, &bm);
    stride = ((uint32_t)bm.bmWidth + 3) & 0xfffffffcu;
    rec = KB_REC;
    height = (int32_t)M32(rec + 0x3c);
    for (y = 0; (int32_t)y < height; y++) {
        src = ADDR(bm.bmBits) + y * stride;
        dst = M32(M32(rec + 0x90) + y * 4);
        for (x = 0; (int32_t)x < (int32_t)M32(rec + 0x38); x++)
            M8(dst + x) = M8(M32(HG2D + 0x90) + M8(src + x));
    }
    if (M32(rec + 0x10) == 0) {
        M32(KB_REC + 0x10) = kb_shape(rec + 0x80);
        rec = KB_REC;
    }
    text = V(0x2080dda4) + (page << 8);
    rc[0] = 0x14;
    rc[1] = 0x28;
    rc[2] = (int32_t)M32(rec + 0x38) - 0xa;
    rc[3] = 0;
    H_DRAW_TEXT(rec, ADDR(rc), 0xffffffffu, 0xffffffffu, text, 3, 0);
    rec = KB_REC;
    rc[0] = (rc[0] - rc[2] + (int32_t)M32(rec + 0x38)) / 2;
    rc[2] = (int32_t)M32(rec + 0x38) - 0x14;
    H_DRAW_TEXT(rec, ADDR(rc), 0, 0xffffffffu, text, 3, 0);
    rc[0] = 0x14;
    rc[3] += 8;
    list = page * 0x2108;
    text = V(0x20810eac) + list;
    for (y = 0; (int32_t)y < (int32_t)M32(V(0x20811eac) + list); y++) {
        rec = KB_REC;
        rc[1] = rc[3];
        H_DRAW_TEXT(rec, ADDR(rc), 0xffffffffu, 0xffffffffu, text, 2, 0);
        rc[2] = (int32_t)M32(KB_REC + 0x38) - 0x14;
        rc[3] += 8;
        kb_rule((uint32_t)rc[0], (uint32_t)rc[1], (uint32_t)rc[2], (uint32_t)rc[3]);
        H_DRAW_TEXT(KB_REC, ADDR(rc), 0, 0xffffffffu, text, 2, 0);
        text += 0x100;
    }
    rec = KB_REC;
    full[0] = 0;
    full[1] = 0;
    full[2] = (int32_t)M32(rec + 0x60);
    full[3] = (int32_t)M32(rec + 0x64);
    return HOST_(0x88, f2)(rec, ADDR(full));
}

/* .text+0x1890 */
static void kb_show(uint32_t on)
{
    if (on != 0)
        CALL(WINDOW_, 0x68, f1)(KB_REC);
    else
        CALL(WINDOW_, 0x6c, f1)(KB_REC);
}

/* .text+0xfc0: CHAIN list 0x14. */
static uint32_t FS98_STDCALL kb_listener(uint32_t message, uint32_t arg)
{
    char buf[0x400], tmp[0x100];
    uint32_t was, shown = 0;
    int32_t x, y;
    char *rest;

    (void)arg;
    if (message == 0x70) {
        if (view_active())
            kb_draw(KB_PAGE);
        return 0;
    }
    if (message != 0x60)
        return 0;
    memset(buf, 0, sizeof buf);
    buf[0] = (char)M8(V(0x20807fd0));
    was = view_active();
    KB_PAGE = 0;
    kb_show(0);
    if (kb_load() == 1) {
        if (M32(V(0x20807fcc)) == 0) {
            if (HOST_(0xc8, f4)(V(0x2080025c), V(0x20800278), ADDR(buf), 0x400) != 0) {
                rest = kb_field(buf, tmp, 0x2c);
                shown = (uint32_t)atoi(tmp);
                rest = kb_field(rest, tmp, 0x2c);
                x = atoi(tmp);
                kb_field(rest, tmp, 0x2c);
                y = atoi(tmp);
                if (x >= 0 || y >= 0) {
                    M32(KB_REC + 0x20) = (uint32_t)x;
                    M32(KB_REC + 0x24) = (uint32_t)y;
                    M32(KB_REC + 8) &= 0xfeffffffu;
                    M32(KB_REC + 8) &= 0xfbffffffu;
                }
            } else {
                shown = 1;
                wsprintfA(buf, (const char *)(uintptr_t)V(0x2080026c), 1, -1, -1);
                HOST_(0xcc, f3)(V(0x2080025c), V(0x20800278), ADDR(buf));
            }
            if (shown != 0) {
                kb_draw(KB_PAGE);
                kb_show(1);
            }
            M32(V(0x20807fcc)) = 1;
        } else if (was == 1) {
            kb_draw(KB_PAGE);
            kb_show(was);
        }
    }
    M32(G + 0x129c) = KB_PAGE;
    return 0;
}

/* .text+0x1590: the close box. */
static uint32_t FS98_STDCALL kb_close(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    if (view_active() == 1)
        HOST_(0x3c, f2)(0x1625, 0);
    return 1;
}

/* .text+0x1320 */
static uint32_t kb_create(void)
{
    char buf[0x400];
    uint8_t info[0x54];
    uint32_t rec, r, bm;

    memset(buf, 0, sizeof buf);
    buf[0] = (char)M8(V(0x20807fd0));
    HOST_(0x2a0, f3)(0x1566, ADDR(buf), 0x400);
    CALL(WINDOW_, 0x78, f3)(V(0x20807fc4), ADDR(buf), 1);
    rec = KB_REC;
    M32(rec + 8) |= 0x15000000u;
    M32(rec + 0x20) = 0x1644;
    M32(rec + 0x24) = 0x3e8;
    M32(rec + 0x28) = 0x190;
    M32(rec + 0x2c) = 0x190;
    M32(rec + 0x18) = ADDR(kb_window);
    M32(rec + 0x78) = 0x5000;
    M32(rec + 0x7c) = 0;
    r = HOST_(0x64, f1)(rec);
    if (r != 0)
        return r;
    bm = ADDR(LoadImageA((HINSTANCE)(uintptr_t)M32(V(0x20807fc0)), MAKEINTRESOURCEA(0x65), 0, 0, 0, 0x2040));
    M32(V(0x2080dc84)) = bm;
    if (bm != 0) {
        GetObjectA((HGDIOBJ)(uintptr_t)bm, 0x54, info);
        rec = KB_REC;
        M32(rec + 0x68) = *(uint32_t *)(info + 4);
        M32(rec + 0x70) = *(uint32_t *)(info + 4);
        M32(rec + 0x6c) = *(uint32_t *)(info + 8);
        M32(rec + 0x74) = *(uint32_t *)(info + 8);
    }
    HOST_(0x7c, f1)(KB_REC);
    CHAIN_ADD(0x14, ADDR(kb_listener));
    rec = KB_REC;
    M32(V(0x2080fda4)) = 0;
    memset((void *)(uintptr_t)V(0x2080dc00), 0, 0x84);
    M32(V(0x2080dc00)) = 1;
    M32(V(0x2080dc04)) = M32(rec + 0x70) - 0x19;
    M32(V(0x2080dc08)) = M32(rec + 0x74) - 0x13;
    M32(V(0x2080dc0c)) = 0xc;
    M32(V(0x2080dc10)) = 0xc;
    M32(V(0x2080dc14)) = 5;
    M32(V(0x2080dc1c)) = 0x20000000u;
    M32(V(0x2080dc24)) = ADDR(kb_close);
    M32(V(0x2080dc2c)) = 3;
    M32(V(0x2080dc40)) = 0xffffffffu;
    M32(V(0x2080dc6c)) = 0xffffffffu;
    HOST_(0x27c, f4)(rec, V(0x2080dc00), 0, 0);
    return 0;
}

/* .text+0x1940 */
static void kb_destroy(void)
{
    char buf[0x400];
    uint32_t rec = KB_REC;

    memset(buf, 0, sizeof buf);
    buf[0] = (char)M8(V(0x20807fd0));
    if (view_active()) {
        if (M32(rec + 8) & 0x1000000u)
            wsprintfA(buf, (const char *)(uintptr_t)V(0x2080026c), KB_PAGE + 1, -1, -1);
        else
            wsprintfA(buf, (const char *)(uintptr_t)V(0x2080026c), KB_PAGE + 1, M32(rec + 0x20), M32(rec + 0x24));
    } else {
        wsprintfA(buf, (const char *)(uintptr_t)V(0x2080026c), 0, M32(rec + 0x20), M32(rec + 0x24));
    }
    HOST_(0xcc, f3)(V(0x2080025c), V(0x20800278), ADDR(buf));
    HOST_(0x284, f2)(KB_REC, V(0x2080dc00));
    CHAIN_REMOVE(0x14, ADDR(kb_listener));
    HOST_(0x68, f1)(KB_REC);
    H_FREE(KB_REC + 0x10);
    CALL(WINDOW_, 0x7c, f1)(V(0x20807fc4));
}
