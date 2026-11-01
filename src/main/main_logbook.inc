/* MAIN.DLL: command 0x1407, the logbook (dialogs 0x1407, 0x14070001, 0x14070002), and
 * Linkage +0x90, which writes the flight just ended into it. .text+0xf2f0..+0x11cc9 in
 * MSVC C, with the file and number helpers (+0x11480..+0x11b81) in assembly.
 *
 * A logbook is a text file: its title, a blank line, the column heads, a blank line,
 * rows of 0x50 characters and CR LF, a line of totals. Rows in memory are 0x4f bytes,
 * NUL-separated. Two growable arrays: .data+0x3ac8 the logbooks (0x124 each), +0x3ae8
 * the rows. A file that opens but cannot be read leaves the counts below on the stack
 * uninitialised in retail (.text+0xfd70); ours has them 0. */

#define LB_DLG    V(0x20803b08)
#define LB_BOOKS  V(0x20803ac8)
#define LB_ROWS   V(0x20803ae8)
#define LB_COUNT  V(0x20803b24)
#define LB_WHICH  V(0x20803b28)
#define LB_LIST   V(0x20803b2c)
#define LB_DEFAULT V(0x208090d0)
#define LB_BOOK(b, i)  (M32((b) + 0x24) + (i) * 0x124)
#define H_GETCWD  HOST_(0x130, f1)
#define H_STRCAT  HOST_(0x2b0, f2)
#define A_ADD     HOST_(0x2f8, f1)
#define A_REMOVE  HOST_(0x300, f2)
#define A_CLEAR   HOST_(0x304, f1)
#define ALERT     CALL(UTIL, 0xa4, f8)

static const char lb_head[0x50] = "  DATE     AIRCRAFT MODEL   TO/FROM/REMARKS             DAY  NIGHT  INST.  TOTAL";
static const char lb_totals[0x34] = "TOTALS                                              ";

/* .text+0x11884: tenths as "dddd.d", spaces for leading zeros */
static void FS98_STDCALL lb_tenths(uint32_t v, uint32_t out)
{
    uint8_t d[5];
    int i;

    M32(V(0x20803fa7)) = 3;
    for (i = 0; i < 5; i++) {
        d[i] = (uint8_t)((v % 10) | 0x30);
        v /= 10;
    }
    i = 4;
    while (i >= 1 && d[i] == 0x30) {
        M8(out++) = 0x20;
        i--;
    }
    if (i == 0) {
        M8(out - 1) = 0x30;
    } else {
        for (; i >= 1; i--)
            M8(out++) = d[i];
    }
    M8(out++) = 0x2e;
    M8(out) = d[0];
}

/* .text+0x1193c: and back, tidying the field in place to a tenths place */
static uint32_t FS98_STDCALL lb_parse(uint32_t p)
{
    uint32_t a = 0, b = 0, at = p, i, seen = 0;
    int32_t n;
    uint8_t c;

    for (i = 0; i < 6; i++) {
        if (M8(p + i) == 0x2e) {
            seen = 1;
            break;
        }
    }
    if (!seen) {
        for (i = 0; i < 4; i++)
            M8(p + i) = M8(p + i + 2);
        M8(p + 4) = 0x2e;
        M8(p + 5) = 0x30;
    } else if (i == 5) {
        for (i = 0; i < 5; i++)
            M8(p + i) = M8(p + i + 1);
        M8(p + 5) = 0x30;
    }
    for (n = 6; n != 0; n--, at++) {
        c = M8(at);
        if (c == 0x20)
            continue;
        if (c == 0x2e)
            break;
        c = (uint8_t)(c - 0x30);
        if (c > 9)
            c = 0;
        a = a * 10 + c;
    }
    at++;
    n--;
    if (n > 0)
        for (; n != 0; n--, at++) {
            c = M8(at);
            if (c == 0x20)
                break;
            c = (uint8_t)(c - 0x30);
            if (c > 9)
                c = 0;
            b = b * 10 + c;
        }
    if (b > 9)
        b = 0;
    return a * 10 + b;
}

/* .text+0x118e2: a row's four columns */
static void lb_columns(uint32_t out, uint32_t line)
{
    M32(V(0x20803fa7)) = 3;
    M32(out) = lb_parse(line + 0x35);
    M32(out + 4) = lb_parse(line + 0x3c);
    M32(out + 8) = lb_parse(line + 0x43);
    M32(out + 0xc) = lb_parse(line + 0x4a);
}

/* .text+0x11824: their totals over n lines of the file */
static void lb_totals_of(uint32_t out, uint32_t lines, uint32_t n)
{
    uint32_t t[4], k;

    M32(V(0x20803fa7)) = 3;
    M32(out) = 0;
    M32(out + 4) = 0;
    M32(out + 8) = 0;
    M32(out + 0xc) = 0;
    for (; n != 0; n--, lines += 0x52) {
        lb_columns(ADDR(t), lines);
        for (k = 0; k < 4; k++)
            M32(out + k * 4) += t[k];
    }
}

/* .text+0x117e3 */
static void lb_file_free(void)
{
    if (M8(V(0x20803c86)) != 0) {
        H_FREE(V(0x20803c93));
        M8(V(0x20803c86)) = 0;
    }
}

/* .text+0x11480: read logbook `name`, past its heads. A read that fails leaves it open. */
static void lb_file_read(uint32_t name, uint32_t lines, uint32_t count)
{
    uint32_t at, used, left;
    int32_t rows;
    uint8_t c;

    if (M8(V(0x20803c86)) != 0)
        return;
    M32(V(0x20803fa7)) = 3;
    M32(V(0x20803c87)) = 0xffffffffu;
    H_LOADSTRING(0x9002, V(0x20803c97), 0x103);
    H_JOIN(V(0x20803e9f), 0, V(0x20803c97), name);
    if (H_FILE_OPEN(V(0x20803c8f), V(0x20803e9f), 0x1c, 0) != 0)
        return;
    H_FILE_SIZE(M32(V(0x20803c8f)), V(0x20803c8b));
    if (H_ALLOC(1, M32(V(0x20803c8b)), V(0x20803c93), 0) != 0)
        return;
    M8(V(0x20803c86)) = 1;
    if (H_FILE_READ(M32(V(0x20803c8f)), M32(V(0x20803c93)), M32(V(0x20803c8b)), V(0x20803fa3)) != 0)
        return;
    H_FILE_CLOSE(M32(V(0x20803c8f)));
    at = M32(V(0x20803c93));
    used = 0;
    left = 0x1e;
    do {
        c = M8(at);
        at++;
        used++;
        left--;
    } while (c != 0xd && left != 0);
    at += 3;
    used += 3;
    left = M32(V(0x20803c8b)) - used;
    do {
        c = M8(at);
        at++;
        used++;
        left--;
    } while (c != 0xd && left != 0);
    if ((uint16_t)left == 0)
        return;
    at += 3;
    used += 3;
    M32(V(0x20803c87)) = used;
    M32(lines) = at;
    rows = (int32_t)(M32(V(0x20803c8b)) - used - 0x52);
    if (rows < 0)
        rows = 0;
    M32(count) = (uint32_t)rows / 0x52;
}

/* .text+0x11695: write it back; mode 2 makes it new. */
static void lb_file_write(uint32_t path, uint32_t size, uint32_t mode, uint32_t data)
{
    M32(V(0x20803fa7)) = 3;
    H_LOADSTRING(0x9002, V(0x20803c97), 0x103);
    H_JOIN(V(0x20803e9f), 0, V(0x20803c97), V(0x20803d9b));
    if (mode == 2)
        H_FILE_CREATE(V(0x20803c8f), path, 0x1c);
    else
        H_FILE_OPEN(V(0x20803c8f), path, 0x1c, mode);
    H_FILE_WRITE(M32(V(0x20803c8f)), data, size, V(0x20803fa3));
    H_FILE_CLOSE(M32(V(0x20803c8f)));
}

/* .text+0x11aff */
static uint32_t lb_day_ok(int16_t month, int16_t day, uint16_t year)
{
    if (day == 0 || day > 0x1f || month == 0 || month > 0xc)
        return 0xffffffffu;
    if (month == 2) {
        if (day > ((year & 3) == 0 ? 0x1d : 0x1c))
            return 0xffffffffu;
    } else if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 0x1e) {
        return 0xffffffffu;
    }
    return 0;
}

/* .text+0x119f1: "mm-dd-yy" */
static uint32_t lb_date_ok(uint32_t p)
{
#define LB_DIGIT(n, low, high)  do { c = (int8_t)M8(p + (n)); if (c < (low) || c > (high)) return 0xffffffffu; } while (0)
    int8_t c;
    int16_t m, d, y;

    LB_DIGIT(0, 0x30, 0x31);
    m = (int16_t)((c & 0xf) * 10);
    LB_DIGIT(1, 0x30, 0x39);
    m = (int16_t)(m + (c & 0xf));
    if (M8(p + 2) != 0x2d)
        return 0xffffffffu;
    LB_DIGIT(3, 0x30, 0x33);
    d = (int16_t)((c & 0xf) * 10);
    LB_DIGIT(4, 0x30, 0x39);
    d = (int16_t)(d + (c & 0xf));
    if (M8(p + 5) != 0x2d)
        return 0xffffffffu;
    LB_DIGIT(6, 0x30, 0x39);
    y = (int16_t)((c & 0xf) * 10);
    LB_DIGIT(7, 0x30, 0x39);
    y = (int16_t)(y + (c & 0xf));
    return lb_day_ok(m, d, (uint16_t)y) == 0xffffffffu ? 0xffffffffu : 0;
#undef LB_DIGIT
}

/* .text+0x11b81: a column as typed: tidied in place to six right-aligned characters */
static uint32_t lb_field(uint32_t text, uint32_t out)
{
    uint32_t at = text, dot = 0, i;
    int32_t left;
    int8_t c;

    for (i = 0; i < 6; i++, at++) {
        c = (int8_t)M8(at);
        if (c > 0x39)
            goto bad;
        if (c >= 0x30 || c == 0x20)
            continue;
        if (c == 0x2e) {
            if (dot != 0)
                goto bad;
            dot = at;
            continue;
        }
        if (c == 0)
            goto scanned;
        goto bad;
    }
    if (M8(at) != 0)
        goto bad;
scanned:
    if (dot != 0 && (int32_t)(dot - text) <= 3)
        M8(dot + 2) = 0;
    at = text;
    left = 6;
    while (M8(at) != 0) {
        at++;
        left--;
    }
    if (left != 0) {
        uint32_t from, k, stop;

        if (left == 6) {
            M8(at) = 0x20;
            at++;
            M8(at) = 0;
            left = 5;
        }
        from = at;
        stop = (uint32_t)left - 1;
        k = 6;
        do {
            M8(text + k) = M8(from);
            from--;
            k--;
        } while (k != stop);
        for (;;) {
            M8(text + k) = 0x20;
            if (k == 0)
                break;
            k--;
            if (k == 0)
                break;
        }
        M8(text) = 0x20;
    }
    at = text;
    for (i = 0; i < 6; i++, at++) {
        if (M8(at) == 0x20)
            continue;
        if (M8(at) != 0x30 || M8(at + 1) == 0x2e)
            break;
        M8(at) = 0x20;
    }
    at = text;
    for (i = 0; i < 6 && M8(at) == 0x20; i++)
        at++;
    if (i == 6) {
        at = text;
        M8(text + 3) = 0x30;
        M8(text + 4) = 0x2e;
        M8(text + 5) = 0x30;
        M8(text + 6) = 0;
    }
    if (M8(at) == 0x2e)
        M8(at - 1) = 0x30;
    i = lb_parse(text);
    M32(out) = i;
    return (int32_t)i > 0x3e7 || (int32_t)i < 0 ? 0xffffffffu : 0;
bad:
    M32(out) = 0;
    return 0xffffffffu;
}

/* .text+0x10480: a row of the file into memory; the last column's end must be CR */
static uint32_t lb_row_in(uint32_t rec, uint32_t line)
{
    uint32_t i;

    for (i = 0; i < 8; i++)
        M8(rec + i) = M8(line + i);
    M8(rec + 8) = 0;
    for (i = 0; i < 0x10; i++)
        M8(rec + 9 + i) = M8(line + 0xb + i);
    M8(rec + 0x19) = 0;
    for (i = 0; i < 0x18; i++)
        M8(rec + 0x1a + i) = M8(line + 0x1c + i);
    M8(rec + 0x32) = 0;
    lb_columns(V(0x2080d150), line);
    for (i = 0; i < 6; i++)
        M8(rec + 0x33 + i) = M8(line + 0x35 + i);
    M8(rec + 0x39) = 0;
    for (i = 0; i < 6; i++)
        M8(rec + 0x3a + i) = M8(line + 0x3c + i);
    M8(rec + 0x40) = 0;
    for (i = 0; i < 6; i++)
        M8(rec + 0x41 + i) = M8(line + 0x43 + i);
    M8(rec + 0x47) = 0;
    for (i = 0; i < 6; i++)
        M8(rec + 0x48 + i) = M8(line + 0x4a + i);
    M8(rec + 0x4e) = 0;
    return M8(line + 0x50) == 0xd;
}

/* .text+0xfd70: the chosen logbook's rows and totals */
static uint32_t lb_show(void)
{
    uint32_t lines = 0, n = 0, first, i;

    if (M32(LB_COUNT) == 0)
        return 0;
    lb_file_read(M32(LB_LIST) + M32(LB_WHICH) * 0x124 + 0x1f, ADDR(&lines), ADDR(&n));
    first = lines;
    M32(V(0x20803b54)) = 0;
    M32(V(0x20803b50)) = n;
    for (i = 0; i < n; i++) {
        if (lb_row_in(A_ADD(LB_ROWS), lines) == 0)
            return 0xffffffffu;
        lines += 0x52;
    }
    M32(V(0x20803b58)) = M32(V(0x20803b00));
    lb_totals_of(V(0x2080d150), first, n);
    lb_tenths(M32(V(0x2080d150)), M32(V(0x20803c08)));
    lb_tenths(M32(V(0x2080d154)), M32(V(0x20803c1c)));
    lb_tenths(M32(V(0x2080d158)), M32(V(0x20803c30)));
    lb_tenths(M32(V(0x2080d15c)), M32(V(0x20803c44)));
    lb_file_free();
    return 0;
}

/* .text+0xfe70: a title: up to n characters to the first CR. A title of nothing but a
 * CR at once still copies one character. */
static uint32_t lb_title(uint32_t data, uint32_t out, uint32_t n)
{
    uint32_t k = 0;
    uint8_t c;

    if (n != 0) {
        c = M8(data);
        do {
            M8(out + k) = c;
            c = M8(data + k + 1);
            k++;
        } while (c != 0xd && k < n);
    }
    M8(out + k) = 0;
    return n >= k;
}

/* .text+0xf590: the logbooks on disk */
static uint32_t lb_scan(void)
{
    char dir[0x100];
    uint8_t found[0x110];
    uint32_t find = 0, file = 0, data = 0, r, elem, path;

    M32(LB_COUNT) = 0;
    M32(LB_WHICH) = 0;
    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    if (HOST_(0x164, f4)(ADDR(&find), ADDR(dir), 0, 0x1c) == 0) {
        if (HOST_(0x160, f2)(find, ADDR(found)) == 0) {
            do {
                M32(LB_COUNT) += 1;
                elem = A_ADD(LB_BOOKS);
                path = elem + 0x1f;
                r = H_JOIN(path, 0, ADDR(dir), ADDR(found));
                if (r != 0)
                    return r;
                if (H_FILE_OPEN(ADDR(&file), path, 0x1c, 0) != 0)
                    return 0xffffffffu;
                if (H_ALLOC(1, *(uint32_t *)(found + 0x108), ADDR(&data), 0) != 0) {
                    H_FILE_CLOSE(file);
                    return 0xffffffffu;
                }
                if (H_FILE_READ(file, data, *(uint32_t *)(found + 0x108), 0) != 0) {
                    H_FILE_CLOSE(file);
                    H_FREE(ADDR(&data));
                    return 0xffffffffu;
                }
                H_FILE_CLOSE(file);
                H_STRCPY(path, ADDR(found));
                M32(elem + 0x120) = H_STRICMP(path, LB_DEFAULT) != 0;
                if (lb_title(data, V(0x20808ef8), 0x1e) == 0) {
                    H_FREE(ADDR(&data));
                    return 0xffffffffu;
                }
                H_STRCPY(elem, V(0x20808ef8));
                H_FREE(ADDR(&data));
            } while (HOST_(0x160, f2)(find, ADDR(found)) == 0);
        }
        if (M32(LB_COUNT) != 0)
            HOST_(0x15c, f1)(ADDR(&find));
    }
    M32(LB_LIST) = M32(V(0x20803ae0));
    return 0;
}

/* .text+0xf400: the dialog's state. +0x3b40 shrinks by the path's length each time, for
 * good. */
static uint32_t lb_load(void)
{
    char cwd[0x104], dir[0x100], path[0x200];
    uint32_t n, i, r;

    M32(LB_DLG) = 0;
    M32(V(0x20803c54)) = 0;
    if (H_GETCWD(ADDR(cwd)) != 0)
        return 0xffffffffu;
    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    n = 0xfffffffcu - H_STRLEN(ADDR(cwd));
    n -= H_STRLEN(ADDR(dir));
    n -= H_STRLEN(V(0x2080361c));
    M32(V(0x20803b40)) += n;
    H_LOADSTRING(0xf4b0, LB_DEFAULT, 0x100);
    if (H_JOIN(ADDR(path), 0, ADDR(dir), LB_DEFAULT) == 0 && H_EXISTS(ADDR(path), 0x1c) == 0) {
        r = H_ATTR_SET(0, ADDR(path), 0x1c);
        if (r != 0)
            return r;
    }
    r = lb_scan();
    if (r != 0)
        return r;
    r = lb_show();
    if (r != 0)
        return r;
    for (i = 0; i < M32(LB_COUNT); i++)
        if (H_STRICMP(G + 0x12c, M32(LB_LIST) + i * 0x124 + 0x1f) == 0)
            break;
    M32(LB_WHICH) = i != M32(LB_COUNT) ? i : 0;
    return 0;
}

/* .text+0xf2f0 */
static uint32_t lb_close(void)
{
    char dir[0x100], path[0x200];
    uint32_t r;

    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    r = H_JOIN(ADDR(path), 0, ADDR(dir), LB_DEFAULT);
    if (r == 0) {
        r = H_EXISTS(ADDR(path), 0x1c);
        if (r == 0)
            H_ATTR_SET(2, ADDR(path), 0x1c);
    }
    if (M32(LB_WHICH) != 0xffffffffu)
        H_STRCPY(G + 0x12c, M32(LB_LIST) + M32(LB_WHICH) * 0x124 + 0x1f);
    A_CLEAR(LB_BOOKS);
    A_CLEAR(LB_ROWS);
    return r;
}

/* .text+0xf3d0, command 0x1407 */
static void command_1407(void)
{
    if (lb_load() == 0) {
        H_DIALOG(0x1407, LB_DLG);
        lb_close();
    }
}

/* .text+0x100d0: rows by date, a year of 00..02 being 2000..2002. Dates that do not scan
 * compare what the stack held in retail; ours has zeros, and the suite gives retail's
 * sscanf the same. */
static int32_t lb_date_cmp(uint32_t a, uint32_t b)
{
    int32_t m1 = 0, d1 = 0, y1 = 0, m2 = 0, d2 = 0, y2 = 0;

    sscanf((const char *)(uintptr_t)a, (const char *)(uintptr_t)V(0x20803c5c), &m1, &d1, &y1);
    sscanf((const char *)(uintptr_t)b, (const char *)(uintptr_t)V(0x20803c5c), &m2, &d2, &y2);
    y1 += 0x76c;
    y2 += 0x76c;
    if (y1 == 0x76c || y1 == 0x76d || y1 == 0x76e)
        y1 += 0x3e8;
    if (y2 == 0x76c || y2 == 0x76d || y2 == 0x76e)
        y2 += 0x3e8;
    if (y1 != y2)
        return y1 - y2;
    if (m1 != m2)
        return m1 - m2;
    return d1 - d2;
}

/* .text+0x10040: a bubble sort */
static void lb_sort(uint32_t rows, uint32_t n)
{
    uint8_t tmp[0x4f];
    uint32_t left, k, at, next;
    int sorted;

    if (n == 0)
        return;
    left = n - 1;
    do {
        sorted = 1;
        at = rows;
        for (k = left; k != 0; k--) {
            next = at + 0x4f;
            if (lb_date_cmp(at, next) > 0) {
                H_MEMMOVE(ADDR(tmp), at, 0x4f);
                H_MEMMOVE(at, next, 0x4f);
                H_MEMMOVE(next, ADDR(tmp), 0x4f);
                sorted = 0;
            }
            at = next;
        }
        left--;
    } while (!sorted);
}

/* .text+0x10190 */
static uint32_t lb_delete(uint32_t name)
{
    char dir[0x100], path[0x208];
    uint32_t r;

    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    r = H_JOIN(ADDR(path), 0, ADDR(dir), name);
    if (r != 0)
        return r;
    return H_DELETE(ADDR(path), 0x1c) != 0 ? 0xffffffffu : 0;
}

/* .text+0x10200: the file's text */
static void lb_text(uint32_t buf, uint32_t b, uint32_t n)
{
    static const struct { uint8_t off, n, gap; } cols[7] = {
        { 0, 8, 3 }, { 9, 0x10, 1 }, { 0x1a, 0x18, 1 }, { 0x33, 6, 1 }, { 0x3a, 6, 1 }, { 0x41, 6, 1 }, { 0x48, 6, 0 },
    };
    uint32_t at = 0, i, k, c, rows;

#define LB_PUT(v)  M8(buf + at++) = (uint8_t)(v)
    for (i = 0; i < n; i++)
        LB_PUT(M8(LB_BOOK(b, M32(b + 0x20)) + i));
    LB_PUT(0xd); LB_PUT(0xa); LB_PUT(0xd); LB_PUT(0xa);
    for (i = 0; i < 0x50; i++)
        LB_PUT(lb_head[i]);
    LB_PUT(0xd); LB_PUT(0xa); LB_PUT(0xd); LB_PUT(0xa);
    rows = buf + at;
    for (i = 0; i < M32(b + 0x48); i++) {
        for (c = 0; c < 7; c++) {
            for (k = 0; k < cols[c].n; k++)
                LB_PUT(M8(M32(b + 0x50) + i * 0x4f + cols[c].off + k));
            for (k = 0; k < cols[c].gap; k++)
                LB_PUT(0x20);
        }
        LB_PUT(0xd);
        LB_PUT(0xa);
    }
    LB_PUT(0xd); LB_PUT(0xa);
    for (i = 0; i < 0x34; i++)
        LB_PUT(lb_totals[i]);
    if (M32(b + 0x48) == 0) {
        M32(V(0x2080d150)) = 0;
        M32(V(0x2080d154)) = 0;
        M32(V(0x2080d158)) = 0;
        M32(V(0x2080d15c)) = 0;
    } else {
        lb_totals_of(V(0x2080d150), rows, M32(b + 0x48));
    }
    for (c = 0; c < 4; c++) {
        LB_PUT(0x20);
        lb_tenths(M32(V(0x2080d150) + c * 4), V(0x20808f20));
        for (k = 0; k < 6; k++)
            LB_PUT(M8(V(0x20808f20) + k));
    }
    LB_PUT(0xd);
    M8(buf + at) = 0xa;
    M8(buf + at + 1) = 0x1a;
#undef LB_PUT
}

/* .text+0xfeb0: write logbook `name` out, its rows sorted. An empty title takes the
 * name's first 0x1e characters. */
static uint32_t lb_write(uint32_t b, uint32_t name, uint32_t mode)
{
    char dir[0x100], path[0x204];
    uint32_t data = 0, n, size, i;

    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    if (H_JOIN(ADDR(path), 0, ADDR(dir), name) != 0)
        return 0xffffffffu;
    H_MEMSET(V(0x20808ef8), 0, 0x1e);
    if (H_STRCMP(LB_BOOK(b, M32(b + 0x20)), V(0x20807fd0)) == 0) {
        for (i = 0; i < 0x1e; i++) {
            M8(V(0x20808ef8) + i) = M8(LB_BOOK(b, M32(b + 0x20)) + i + 0x1f);
            if (M8(V(0x20808ef8) + i) == 0)
                break;
        }
        H_STRCPY(LB_BOOK(b, M32(b + 0x20)), V(0x20808ef8));
    }
    n = H_STRLEN(LB_BOOK(b, M32(b + 0x20)));
    size = n + M32(b + 0x48) * 0x52 + 0xad;
    if (H_ALLOC(1, size, ADDR(&data), 0) != 0)
        return 0xffffffffu;
    lb_sort(M32(b + 0x50), M32(b + 0x48));
    lb_text(data, b, n);
    lb_file_write(ADDR(path), size, mode, data);
    H_FREE(ADDR(&data));
    return 0;
}

/* .text+0x10a50 */
static uint32_t lb_aircraft(uint32_t b)
{
    CALL(ACONTAIN, 0x1c, f2)(b + 0xf0, b + 0xec);
    M32(b + 0xf4) = 0;
    return 0;
}

/* .text+0x10df0: today in the game, "mm-dd-yy" */
static void lb_today(uint32_t out)
{
    uint8_t month = 0;
    uint16_t day = 0;

    M16(V(0x20808ee0)) = M16(G + 0x240);
    M16(V(0x20808eec)) = M16(G + 0x23e);
    date_from_yday(V(0x20808eec), V(0x20808ee0), ADDR(&month), ADDR(&day));
    N_PUT(V(0x20808ee0), 0, V(0x20808ef0));
    N_PUT(ADDR(&month), 7, V(0x20808f18));
    N_PUT(ADDR(&day), 0, V(0x20808ee4));
    M8(out) = M8(V(0x20808f19));
    M8(out + 1) = M8(V(0x20808f1a));
    M8(out + 2) = 0x2d;
    M8(out + 3) = M8(V(0x20808ee7));
    M8(out + 4) = M8(V(0x20808ee8));
    M8(out + 5) = 0x2d;
    M8(out + 6) = M8(V(0x20808ef3));
    M8(out + 7) = M8(V(0x20808ef4));
    M8(out + 8) = 0;
}

/* .text+0x10ec0: the entry dialog (0x14070002): mode 5 edits a row, 4 a new one. */
static uint32_t lb_entry(uint32_t b)
{
    char date[0x10], a[0x20], c[0x20], path[0x104];
    uint32_t mode, i = 0, off, r, row;
    int found = 1;

    M32(b + 0x54) = 0;
    if (M32(b + 0xec) == 0)
        lb_aircraft(b);
    mode = M32(b + 0xe8);
    if (mode == 5) {
        row = M32(b + 0x50) + M32(b + 0x4c) * 0x4f;
        H_STRCPY(M32(b + 0x60), row);
        H_STRCPY(M32(b + 0x88), M32(b + 0x50) + M32(b + 0x4c) * 0x4f + 0x1a);
        H_STRCPY(M32(b + 0x9c), M32(b + 0x50) + M32(b + 0x4c) * 0x4f + 0x33);
        H_STRCPY(M32(b + 0xb0), M32(b + 0x50) + M32(b + 0x4c) * 0x4f + 0x3a);
        H_STRCPY(M32(b + 0xc4), M32(b + 0x50) + M32(b + 0x4c) * 0x4f + 0x41);
        H_STRCPY(M32(b + 0xd8), M32(b + 0x50) + M32(b + 0x4c) * 0x4f + 0x48);
        H_MEMSET(ADDR(a), 0, 0x20);
        H_STRCPY(ADDR(a), M32(b + 0x74));
        for (off = 0; i < M32(b + 0xf0); i++, off += 0xa08) {
            H_MEMSET(ADDR(c), 0, 0x20);
            H_STRCPY(ADDR(c), M32(b + 0xec) + off);
            c[0x10] = 0;
            if (H_STRICMP(ADDR(c), ADDR(a)) == 0)
                break;
        }
    } else if (mode == 4) {
        memset(date, 0, sizeof date);
        lb_today(ADDR(date));
        H_STRCPY(M32(b + 0x60), ADDR(date));
        if (M32(b + 0xf8) != 0) {
            found = 0;
        } else {
            H_MEMSET(M32(b + 0x88), 0x20, 0x18);
            lb_tenths(0, M32(b + 0x9c));
            lb_tenths(0, M32(b + 0xb0));
            lb_tenths(0, M32(b + 0xc4));
            lb_tenths(0, M32(b + 0xd8));
            CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, ADDR(path), 0x104);
            for (off = 0; i < M32(b + 0xf0); i++, off += 0xa08)
                if (H_STRICMP(ADDR(path), M32(b + 0xec) + off) == 0)
                    break;
        }
    } else {
        return 0xffffffffu;
    }
    if (found)
        M32(b + 0xf4) = i;
    r = H_DIALOG(0x14070002, b);
    H_FREE(b + 0xec);
    return r;
}

/* .text+0x10d80: the new logbook dialog (0x14070001) */
static uint32_t lb_new(uint32_t b)
{
    uint32_t r;

    M32(b + 0x28) = 0;
    r = H_DIALOG(0x14070001, b);
    if (M32(b + 0x28) == 1 && r == 0 && M32(b + 0x30) == 0x1004) {
        M32(b + 0x30) = 0;
        M32(b + 0x44) = 0;
        lb_delete(M32(b + 0x34));
        A_CLEAR(LB_BOOKS);
        A_CLEAR(LB_ROWS);
        return lb_load();
    }
    return r;
}

/* .text+0x10a80: its name's check */
static uint32_t FS98_STDCALL lb_new_check(uint32_t s)
{
    char dir[0x100], path[0x200];
    uint32_t r = H_NAMEOK(M32(s + 0xc)), size = 0;

    if (r != 0) {
        M32(s + 8) = r == 0x1003 ? r : 0x100d;
    } else {
        H_LOADSTRING(0x9002, ADDR(dir), 0x100);
        r = H_JOIN(ADDR(path), 0, ADDR(dir), M32(s + 0xc));
        if (r != 0) {
            M32(s + 8) = 0x1007;
        } else if (H_PATHLEN(ADDR(path), 0x1c, ADDR(&size)) != 0 || size >= 0x104) {
            M32(s + 8) = 0x100d;
            r = 0xffffffffu;
        } else if (H_EXISTS(ADDR(path), 0x1c) == 0) {
            M32(s + 8) = 0x1004;
            r = 0xffffffffu;
        } else {
            M32(s + 8) = 0;
            r = 0;
        }
    }
    if (M32(s + 8) == 0x1004 && ALERT(0xc0, 1, 0xf4b5, 0xf4b6, 0xf4b7, 0, 0, 1) == 0x40)
        M32(s + 0x1c) = 1;
    return r;
}

/* .text+0x10bc0: the entry's check */
static uint32_t FS98_STDCALL lb_entry_check(uint32_t s)
{
#define LB_COLUMN(text, status, out)                                                  \
    do {                                                                              \
        if (M8(M32(s + (text))) == 0) {                                               \
            M32(s + (status)) = 9;                                                    \
            return 0;                                                                 \
        }                                                                             \
        r = lb_field(M32(s + (text)), ADDR(&(out)));                                  \
        if (r != 0) {                                                                 \
            M32(s + (status)) = (out) > 0x3e7 ? 0xb : 4;                              \
            return r;                                                                 \
        }                                                                             \
        M32(s + (status)) = 0;                                                        \
    } while (0)
    uint32_t day = s, night = 0, total_in = 0, r, sum;

    if (M8(M32(s + 0xc)) == 0) {
        M32(s + 8) = 0x1003;
        return 0xffffffffu;
    }
    r = lb_date_ok(M32(s + 0xc));
    if (r != 0) {
        M32(s + 8) = 0x100f;
        return r;
    }
    M32(s + 8) = 0;
    LB_COLUMN(0x48, 0x44, day);
    LB_COLUMN(0x5c, 0x58, night);
    LB_COLUMN(0x70, 0x6c, total_in);
    sum = night + day;
    if (total_in > sum) {
        M32(s + 0x6c) = 0x1013;
        return 0xffffffffu;
    }
    if (sum > 0x7cf) {
        lb_tenths(0x7cf, M32(s + 0x84));
        M32(s + 0x80) = 0xb;
        return 0xffffffffu;
    }
    lb_tenths(sum, M32(s + 0x84));
    M32(s + 0x80) = 0;
    return 0;
#undef LB_COLUMN
}

/* .text+0x111d0: a new title */
static uint32_t lb_retitle(uint32_t b)
{
    uint32_t i;

    H_MEMSET(LB_BOOK(b, M32(b + 0x20)), 0, 0x1e);
    for (i = 0; i <= 0x1e; i++) {
        M8(LB_BOOK(b, M32(b + 0x20)) + i) = M8(M32(b + 0xc) + i);
        if (M8(M32(b + 0xc) + i) == 0)
            break;
    }
    M32(b + 8) = 0;
    return 0;
}

/* .text+0x11230: enough disk? Over 5000 bytes, or says not */
static uint32_t lb_room(uint32_t which)
{
    char drive[0x100], cwd[0x104];
    uint32_t space[2];

    if (H_GETCWD(ADDR(cwd)) != 0)
        return 0;
    if (H_SPLIT(ADDR(cwd), ADDR(drive), 0, 0, 0) != 0)
        return 0;
    if (HOST_(0x150, f2)(ADDR(drive), ADDR(space)) != 0)
        return 0;
    if (space[1] == 0 && space[0] < 0x1388) {
        ALERT(2, 1, which == 1 ? 0xf4b8 : 0xf4b9, 0xf4a1, 0, 0, 0, 0);
        return 0;
    }
    return 1;
}

/* A row from the entry dialog's fields */
static void lb_row_from(uint32_t b, uint32_t row, int fresh)
{
    H_STRCPY(row, M32(b + 0x60));
    H_STRCPY(row + 9, M32(b + 0x74));
    if (fresh)
        M8(row + 0x19) = 0;
    H_STRCPY(row + 0x1a, M32(b + 0x88));
    H_STRCPY(row + 0x33, M32(b + 0x9c));
    H_STRCPY(row + 0x3a, M32(b + 0xb0));
    H_STRCPY(row + 0x41, M32(b + 0xc4));
    H_STRCPY(row + 0x48, M32(b + 0xd8));
}

/* .text+0xf800: the buttons, all by this one routine at block +0x150. A failed write
 * on 6 and 8 answers -1 only if it succeeded: that test is the wrong way round. */
static uint32_t FS98_STDCALL lb_button(uint32_t op, uint32_t i, uint32_t b)
{
    uint32_t r, elem, n;

    switch (op) {
    case 0:
        return 0;
    case 1:
        if (lb_room(1) == 0)
            return 0;
        r = lb_new(b);
        if (r != 0)
            return r;
        if (M32(b + 0x28) == 1) {
            M32(b + 0x20) = M32(b + 0x1c);
            elem = A_ADD(LB_BOOKS);
            M32(b + 0x24) = M32(V(0x20803ae0));
            H_STRCPY(elem + 0x1f, M32(b + 0x34));
            H_MEMSET(elem, 0, 0x1e);
            if (M32(b + 0x1c) == 0) {
                M32(b + 0x24) = M32(V(0x20803ae0));
                lb_show();
            }
            M32(b + 0x48) = 0;
            M32(b + 0x4c) = 0;
            M32(b + 0x1c) += 1;
            A_CLEAR(LB_ROWS);
            r = lb_write(b, M32(b + 0x34), 2);
            M32(LB_BOOK(b, M32(b + 0x20)) + 0x120) = 1;
            if (r != 0)
                return r;
        }
        ((f3)(uintptr_t)M32(b + 0x150))(3, M32(b + 0x20), LB_DLG);
        return 0;
    case 2:
        if (i >= M32(b + 0x1c))
            return 0xffffffffu;
        if (ALERT(0xc0, 1, 0xf470, 0xf4b1, 0, 0, 0, 0) != 0x40)
            return 0;
        if (M32(LB_BOOK(b, i) + 0x120) != 1)
            return 0;
        lb_delete(LB_BOOK(b, i) + 0x1f);
        A_REMOVE(LB_BOOKS, LB_BOOK(b, i));
        A_CLEAR(LB_ROWS);
        n = M32(b + 0x1c) - 1;
        M32(b + 0x1c) = n;
        if (M32(b + 0x20) >= n)
            M32(b + 0x20) = n == 0 ? 0 : n - 1;
        lb_show();
        return 0;
    case 3:
        if (i >= M32(b + 0x1c))
            return 0xffffffffu;
        M32(b + 0x20) = i;
        A_CLEAR(LB_ROWS);
        M32(b + 0x4c) = 0;
        lb_show();
        return 0;
    case 4:
        if (lb_room(4) == 0)
            return 0;
        if (i >= M32(b + 0x1c))
            return 0xffffffffu;
        M32(b + 0xe8) = 4;
        r = lb_entry(b);
        if (r != 0)
            return r;
        if (M32(b + 0x54) != 1)
            return 0;
        M32(b + 0x4c) = M32(b + 0x48);
        M32(b + 0x48) += 1;
        lb_row_from(b, A_ADD(LB_ROWS), 1);
        M32(b + 0x50) = M32(V(0x20803b00));
        return lb_write(b, LB_BOOK(b, i) + 0x1f, 1);
    case 5:
        if (i >= M32(b + 0x48))
            return 0xffffffffu;
        M32(b + 0xe8) = 5;
        r = lb_entry(b);
        if (r != 0)
            return r;
        if (M32(b + 0x54) != 1)
            return 0;
        lb_row_from(b, M32(b + 0x50) + i * 0x4f, 0);
        return lb_write(b, LB_BOOK(b, M32(b + 0x20)) + 0x1f, 1);
    case 6:
        if (i >= M32(b + 0x48))
            return 0xffffffffu;
        if (ALERT(0xc0, 1, 0xf470, 0xf4b2, 0, 0, 0, 0) == 0x40) {
            A_REMOVE(LB_ROWS, M32(b + 0x50) + i * 0x4f);
            n = M32(b + 0x48) - 1;
            M32(b + 0x48) = n;
            if (M32(b + 0x4c) >= n)
                M32(b + 0x4c) = n;
        }
        return lb_write(b, LB_BOOK(b, M32(b + 0x20)) + 0x1f, 2) != 0 ? 0 : 0xffffffffu;
    case 7:
        if (i >= M32(b + 0x48))
            return 0xffffffffu;
        M32(b + 0x4c) = i;
        return 0;
    case 8:
        if (i > M32(b + 0x1c))
            return 0xffffffffu;
        M32(b + 0x20) = i;
        lb_retitle(b);
        return lb_write(b, LB_BOOK(b, M32(b + 0x20)) + 0x1f, 2) != 0 ? 0 : 0xffffffffu;
    }
    return 0xffffffffu;
}

/* .text+0x10560 == Linkage +0x90: the flight into the current logbook, if it is on,
 * there is room, and any time was flown. */
static uint32_t FS98_STDCALL main_log_flight(void)
{
    char z[0x30], msg[0x150], dir[0x100], t[0x20], q[0x104], p[0x200];
    uint32_t i, off, gp, book;

    memset(z, 0, sizeof z);
    z[0] = (char)M8(V(0x20807fd0));
    memset(msg, 0, sizeof msg);
    msg[0] = (char)M8(V(0x20807fd0));
    if (M32(G + 0x128) == 0 || lb_room(4) == 0)
        return 0;
    if (H_STRCMP(G + 0x12c, V(0x20807fd0)) == 0) {
        if (M8(LB_DEFAULT) == 0)
            H_LOADSTRING(0xf4b0, LB_DEFAULT, 0x100);
        H_STRCPY(G + 0x12c, LB_DEFAULT);
    }
    H_LOADSTRING(0x9002, ADDR(dir), 0x100);
    if (H_JOIN(ADDR(p), 0, ADDR(dir), G + 0x12c) != 0 || H_EXISTS(ADDR(p), 0x1c) != 0) {
        H_LOADSTRING(0xf4b3, ADDR(msg), 0x100);
        H_STRCAT(ADDR(msg), G + 0x12c);
        H_LOADSTRING(0xf4b4, ADDR(t), 0x20);
        H_STRCAT(ADDR(msg), ADDR(t));
        ALERT(1, 1, 0xf490, ADDR(msg), 0xf492, 0xf493, 0xf494, 0);
        return 0;
    }
    if (H_STRICMP(G + 0x12c, LB_DEFAULT) == 0) {
        H_LOADSTRING(0x9002, ADDR(dir), 0x100);
        if (H_JOIN(ADDR(p), 0, ADDR(dir), LB_DEFAULT) != 0)
            return 0;
        H_ATTR_SET(0, ADDR(p), 0x1c);
    }
    gp = G;
    M16(gp + 0x236) = (uint16_t)(M16(gp + 0x230) + M16(gp + 0x234));
    if (M16(G + 0x236) == 0)
        return 0;
    if (M32(LB_LIST) == 0)
        lb_load();
    for (book = 0, off = 0; book < M32(LB_COUNT); book++, off += 0x124)
        if (H_STRICMP(M32(LB_LIST) + off + 0x1f, G + 0x12c) == 0)
            break;
    lb_aircraft(LB_DLG);
    lb_button(3, book, LB_DLG);
    H_MEMSET(M32(V(0x20803b90)), 0x20, 0x18);
    M32(V(0x20803c00)) = 1;
    gp = G;
    if (M16(gp + 0x230) > 0x3e7)
        M16(gp + 0x230) = 0x3e7;
    if (M16(gp + 0x234) > 0x3e7)
        M16(gp + 0x234) = 0x3e7;
    if (M16(gp + 0x232) > 0x3e7)
        M16(gp + 0x232) = 0x3e7;
    if (M16(gp + 0x236) > 0x7cf)
        M16(gp + 0x236) = 0x7cf;
    lb_tenths(M16(G + 0x230), M32(V(0x20803ba4)));
    lb_tenths(M16(G + 0x234), M32(V(0x20803bb8)));
    lb_tenths(M16(G + 0x232), M32(V(0x20803bcc)));
    lb_tenths(M16(G + 0x236), M32(V(0x20803be0)));
    CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, ADDR(q), 0x104);
    for (off = 0, i = 0; i < M32(V(0x20803bf8)); i++, off += 0xa08)
        if (H_STRICMP(ADDR(q), M32(V(0x20803bf4)) + off) == 0)
            break;
    M32(V(0x20803bfc)) = i;
    H_STRCPY(ADDR(z), M32(V(0x20803bf4)) + off);
    z[0x10] = 0;
    H_STRCPY(M32(V(0x20803b7c)), ADDR(z));
    M32(V(0x20803b5c)) = 0;
    M32(V(0x20803bf0)) = 4;
    lb_button(4, book, LB_DLG);
    /* the directory alone, not the logbook's path (+0x10a13) */
    if (H_STRICMP(G + 0x12c, LB_DEFAULT) == 0)
        H_ATTR_SET(2, ADDR(dir), 0x1c);
    H_FREE(V(0x20803bf4));
    return 0;
}
