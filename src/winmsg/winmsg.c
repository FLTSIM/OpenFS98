/* WINMSG.DLL -- module id 22: the on-screen message window.
 *
 * ATC, LANDME, DEMO and ADVDRV put text up through +0x18 in one of seven
 * slots, each drawn in its slot's colour.  Messages wait in a queue -- a
 * doubly linked list between two sentinel nodes -- and the frame callback
 * (CHAIN list 2) shows the first: a plain one for its count of frames, or,
 * in slots 1, 2, 3 and 6 with GLOBAL +0x1254 set, one that scrolls in from
 * the right and across.  A message too wide for the window is split at its
 * spaces into several, its time shared out by length.  When one is done,
 * WINMSG tells CHAIN list 0x14 (message 0x50, the node as +0x18 returned
 * it) and takes the next; with none left it hides the window.  A slot also
 * keeps the text shown in it, so that the append mode (+0x18's fourth
 * argument) can extend a message still on the queue rather than queue
 * another.  The window is WINDOW's record type 5, and its place is saved
 * and restored with a situation (CHAIN list 0x14, messages 0x22 and 0x23).
 *
 * Retail is MSVC C, transliterated here.  The whole 0x4a8-byte .data image
 * is kept at retail's offsets: a scrolling message in a window narrower
 * than 30 pixels starts before the line it is drawn from, over the slots'
 * texts, and the harness compares the image.  docs/modules/winmsg.md has
 * the message layout and the rest of retail's quirks.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define WM_IMAGE_SIZE 0x4a8
#define WM_IMPORTS    0x000
#define WM_LINKAGE    0x030

typedef volatile uint8_t  wm_v8;
typedef volatile uint16_t __attribute__((may_alias)) wm_v16;
typedef volatile uint32_t __attribute__((may_alias)) wm_v32;

struct winmsg_image {
    uint8_t bytes[WM_IMAGE_SIZE];
};

extern struct winmsg_image winmsg_image;

#define D       ((uint32_t)(uintptr_t)winmsg_image.bytes)
#define M8(a)   (*(wm_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(wm_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(wm_v32 *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

#define IMPORT(i)  M32(D + WM_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define CHAIN      IMPORT(2)
#define WINDOW     IMPORT(3)
#define MAIN       IMPORT(4)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_WIN_CREATE  HOST_(0x064, f1)   /* (rec): its native window */
#define H_WIN_DESTROY HOST_(0x068, f1)   /* (rec) */
#define H_WIN_UPDATE  HOST_(0x07c, f1)   /* (rec): flags and geometry */
#define H_WIN_FLUSH   HOST_(0x084, f1)   /* (rec) */
#define H_TEXT        HOST_(0x08c, f5)   /* (rec, where, colour, text, opaque) */
#define H_ALLOC       HOST_(0x20c, f4)   /* (flags, size, &out, zero) */
#define H_REALLOC     HOST_(0x210, f3)   /* (flags, size, &p) */
#define H_FREE        HOST_(0x214, f1)   /* (&p) */
#define H_MEMSET      HOST_(0x218, f3)   /* (dst, byte, n) */
#define H_MEMMOVE     HOST_(0x21c, f3)   /* (dst, src, n) */
#define H_STRCPY      HOST_(0x2a8, f2)
#define H_STRCAT      HOST_(0x2b0, f2)
#define H_STRDUP      HOST_(0x2b4, f1)
#define H_STRLEN      HOST_(0x2b8, f1)

#define CHAIN_ADD     CALL(CHAIN, 0x18, f2)   /* (list, callback) */
#define CHAIN_REMOVE  CALL(CHAIN, 0x1c, f2)
#define CHAIN_SEND    CALL(CHAIN, 0x20, f3)   /* (list, a, b) */
#define WINDOW_RECORD CALL(WINDOW, 0x78, f3)  /* (&out, title, type) */
#define WINDOW_FREE   CALL(WINDOW, 0x7c, f1)  /* (&rec) */
#define MAIN_SAVE_OPEN  CALL(MAIN, 0x5c, f2)  /* (tag, size) */
#define MAIN_SAVE_PUT   CALL(MAIN, 0x60, f3)  /* (src, size, 0) */
#define MAIN_SAVE_CLOSE CALL(MAIN, 0x64, f0)

#define GLB16(o) M16(G + (o))
#define GLB32(o) M32(G + (o))

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_COLOUR  (D + 0x058)    /* per slot, 0..6 */
#define S_FILL    (D + 0x074)    /* byte: what a frame clears the window to */
#define S_TEXTS   (D + 0x078)    /* per slot: the text shown in it, a host string */
#define S_LINE    (D + 0x098)    /* 0x400: a scrolling message's visible part */
#define S_REC     (D + 0x498)    /* the window, WINDOW's record */
#define S_QUEUE   (D + 0x4a4)    /* the messages */

/* the window record, as far as WINMSG touches it (docs/modules/window.md) */
#define R_FLAGS   0x08
#define R_WIDTH   0x38           /* in pixels, the host's */
#define R_SIZE    0x94           /* the surface's bytes and bits, the host's */
#define R_BITS    0x98

#define F_SHOWN     0x80000000u
#define F_NO_STATUS 0x40000000u

/* a queue: two sentinel nodes */
#define Q_HEAD    0x00
#define Q_TAIL    0x04

/* a node, 0xc bytes */
#define N_DATA    0x00
#define N_NEXT    0x04
#define N_PREV    0x08

/* a message, 0x2c bytes */
#define W_TEXT    0x00           /* its own copy */
#define W_LEN     0x04
#define W_SLOT    0x08
#define W_KEEP    0x0c           /* 1: scrolling until replaced (GLOBAL +0x1254 1) */
#define W_TIME    0x10           /* frames left */
#define W_FRESH   0x14           /* 1 until first drawn */
#define W_SPEED   0x18           /* pixels a frame */
#define W_SCROLL  0x1c           /* pixels scrolled off the left */
#define W_START   0x20           /* pixels still to come in from the right */
#define W_EXTENT  0x24           /* width (8 a character) and 0xd7 */

#define HEAD(q)   M32((q) + Q_HEAD)
#define TAIL(q)   M32((q) + Q_TAIL)
#define NEXT(n)   M32((n) + N_NEXT)
#define PREV(n)   M32((n) + N_PREV)

/* slots 1, 2, 3 and 6 scroll, in retail's order of testing */
#define SCROLLS(s) ((s) == 3 || (s) == 6 || (s) == 1 || (s) == 2)

/* ---- the queue: .text+0x1020..+0x123f ------------------------------------ */

/* .text+0x1020 */
static void q_init(uint32_t q)
{
    H_ALLOC(1, 0xc, q + Q_HEAD, 1);
    H_ALLOC(1, 0xc, q + Q_TAIL, 1);
    NEXT(HEAD(q)) = TAIL(q);
    PREV(TAIL(q)) = HEAD(q);
}

/* .text+0x1070 */
static void q_free(uint32_t q)
{
    H_FREE(q + Q_HEAD);
    H_FREE(q + Q_TAIL);
}

/* .text+0x10a0 */
static uint32_t q_empty(uint32_t q)
{
    return NEXT(HEAD(q)) == TAIL(q);
}

/* .text+0x2110 -- what the queue frees a node's message with: its text,
 * then the message, each through the word before it. */
static void msg_free(uint32_t node)
{
    H_FREE(M32(node + N_DATA));
    H_FREE(node + N_DATA);
}

/* .text+0x10c0 -- `node` after `pos`; in an empty queue only after the
 * head. */
static void q_insert(uint32_t q, uint32_t pos, uint32_t node)
{
    if (q_empty(q) && pos != HEAD(q))
        return;
    PREV(NEXT(pos)) = node;
    NEXT(node) = NEXT(pos);
    NEXT(pos) = node;
    PREV(node) = pos;
}

/* .text+0x1100 -- all of `sub` after `pos`, leaving `sub` empty.  An empty
 * `sub` links `pos` to its tail sentinel. */
static void q_splice(uint32_t q, uint32_t pos, uint32_t sub)
{
    if (q_empty(q) && pos != HEAD(q))
        return;
    PREV(NEXT(pos)) = PREV(TAIL(sub));
    NEXT(PREV(TAIL(sub))) = NEXT(pos);
    PREV(NEXT(HEAD(sub))) = pos;
    NEXT(pos) = NEXT(HEAD(sub));
    NEXT(HEAD(sub)) = TAIL(sub);
    PREV(TAIL(sub)) = HEAD(sub);
}

/* .text+0x1160 -- the first node out and freed */
static void q_pop(uint32_t q)
{
    uint32_t first, second;

    if (q_empty(q))
        return;
    first = NEXT(HEAD(q));
    second = NEXT(first);
    msg_free(first);
    H_FREE(HEAD(q) + N_NEXT);
    NEXT(HEAD(q)) = second;
    PREV(second) = HEAD(q);
}

/* .text+0x11a0 -- `node` out and freed */
static void q_erase(uint32_t q, uint32_t node)
{
    if (q_empty(q))
        return;
    NEXT(PREV(node)) = NEXT(node);
    PREV(NEXT(node)) = PREV(node);
    msg_free(node);
    H_FREE(ADDR(&node));
}

/* .text+0x11f0 -- every node freed; the head sentinel back */
static uint32_t q_clear(uint32_t q)
{
    uint32_t node = NEXT(HEAD(q)), at;

    if (node != TAIL(q)) {
        do {
            at = node;
            node = NEXT(node);
            msg_free(at);
            H_FREE(ADDR(&at));
        } while (node != TAIL(q));
    }
    NEXT(HEAD(q)) = TAIL(q);
    PREV(TAIL(q)) = HEAD(q);
    return HEAD(q);
}

/* ---- messages: .text+0x1430, +0x19f0..+0x1ccf ---------------------------- */

/* .text+0x1430 -- a text's extent: eight pixels a character, 0xd7 high */
static void measure(uint32_t text, uint32_t len, uint32_t out)
{
    (void)text;
    M32(out) = len * 8;
    M32(out + 4) = 0xd7;
}

/* the widest a message may be: the window less 30 pixels, at most 0x2000 */
static int32_t limit(void)
{
    int32_t w = (int32_t)M32(M32(S_REC) + R_WIDTH) - 0x1e;

    return w < 0x2000 ? w : 0x2000;
}

/* .text+0x19f0 -- from *pi on to the next space or the end, into *pi; 0 if
 * text[*pi] is the end.  The end counts as a word's end, so the next call
 * starts past it. */
static uint32_t next_word(uint32_t text, uint32_t pi)
{
    uint32_t i = M32(pi);
    uint8_t c = M8(text + i);

    if (c == 0)
        return 0;
    while (c != ' ') {
        c = M8(text + i + 1);
        i++;
        if (c == 0)
            break;
    }
    M32(pi) = i;
    return 1;
}

/* .text+0x1a20 -- a node for a new message */
static uint32_t make_msg(uint32_t text, uint32_t slot, uint32_t time)
{
    uint32_t msg, node, copy;
    int32_t w;

    H_ALLOC(1, 0xc, ADDR(&node), 1);
    H_ALLOC(1, 0x2c, ADDR(&msg), 1);
    M32(node + N_DATA) = msg;
    M32(msg + W_LEN) = H_STRLEN(text);
    H_ALLOC(1, M32(msg + W_LEN) + 1, ADDR(&copy), 1);
    M32(msg + W_TEXT) = copy;
    M32(msg + W_SLOT) = slot;
    H_STRCPY(M32(msg + W_TEXT), text);
    measure(text, M32(msg + W_LEN), msg + W_EXTENT);
    M32(msg + W_FRESH) = 1;
    if (!SCROLLS(M32(msg + W_SLOT))) {
        M32(msg + W_TIME) = time;
        M32(msg + W_SPEED) = 0;
        M32(msg + W_KEEP) = 0;
        return node;
    }
    M32(msg + W_SCROLL) = 0;
    w = limit();
    M32(msg + W_START) = (uint32_t)w;
    if (GLB32(0x1254) == 1) {
        M32(msg + W_TIME) = 0;
        M32(msg + W_KEEP) = 1;
    } else {
        M32(msg + W_TIME) = time;
        M32(msg + W_KEEP) = 0;
    }
    M32(msg + W_SPEED) = ((uint32_t)GLB16(0x362) >> 1) + 2;
    return node;
}

/* .text+0x1b90 -- a queue of messages, `text` broken at its spaces to fit,
 * `time` shared out by length.  The queue's header is the caller's to
 * splice and is never freed.  `cut`, the last space that fitted, is not
 * reset for the next line: a line whose first word is too wide is cut where
 * the line before was. */
static uint32_t split(uint32_t text, uint32_t slot, uint32_t time)
{
    uint32_t len, copy, list, pos, cut, p, part, node, ext[2];

    cut = 0;
    pos = 0;
    len = H_STRLEN(text);
    copy = H_STRDUP(text);
    p = copy;
    H_ALLOC(1, 8, ADDR(&list), 0);
    q_init(list);
    while (next_word(p, ADDR(&pos))) {
        measure(p, pos, ADDR(ext));
        if ((int32_t)ext[0] > limit()) {
            part = cut * time / len;
            M8(p + cut) = 0;
            node = make_msg(p, slot, part);
            q_insert(list, PREV(TAIL(list)), node);
            p = p + cut + 1;
            pos = 0;
        } else {
            cut = pos;
            pos = pos + 1;
        }
    }
    if (M8(p) != 0) {
        part = pos * time / len;
        node = make_msg(p, slot, part);
        q_insert(list, PREV(TAIL(list)), node);
    }
    H_FREE(ADDR(&copy));
    return list;
}

/* .text+0x1ce0 -- the slot's text: `prev` (a message's) and `text`, or
 * with no `prev` the slot's own and `text`. */
static uint32_t slot_text(uint32_t prev, uint32_t text, uint32_t slot)
{
    uint32_t pp = S_TEXTS + 4 * slot, a, b;

    if (prev == 0) {
        if (M32(pp) == 0) {
            M32(pp) = H_STRDUP(text);
            return M32(pp);
        }
        a = H_STRLEN(M32(pp));
        b = H_STRLEN(text);
        H_REALLOC(1, a + b + 1, pp);
        H_STRCAT(M32(pp), text);
        return M32(pp);
    }
    if (M32(pp) != 0)
        H_FREE(pp);
    a = H_STRLEN(prev);
    b = H_STRLEN(text);
    H_ALLOC(1, a + b + 1, pp, 0);
    H_STRCPY(M32(pp), prev);
    H_STRCAT(M32(pp), text);
    return M32(pp);
}

/* `text` queued after `pos`: a scrolling message whole, otherwise (with a
 * time) broken to fit -- what +0x18 and the append do at each of their
 * ends. */
static void queue_after(uint32_t pos, uint32_t text, uint32_t slot, uint32_t time)
{
    uint32_t node, sub, ext[2];

    if (GLB32(0x1254) != 0 && SCROLLS(slot)) {
        node = make_msg(text, slot, time);
        q_insert(M32(S_QUEUE), pos, node);
        return;
    }
    if (time == 0)
        return;
    measure(text, H_STRLEN(text), ADDR(ext));
    if ((int32_t)ext[0] > limit()) {
        sub = split(text, slot, time);
        q_splice(M32(S_QUEUE), pos, sub);
    } else {
        node = make_msg(text, slot, time);
        q_insert(M32(S_QUEUE), pos, node);
    }
}

/* .text+0x1dc0 -- +0x18's append mode.  The last message queued in the
 * slot is extended: a scrolling one that is not yet all on screen takes the
 * new text onto its own; one that is gets a new message after it with just
 * the new text; a plain one is replaced by one with both, its time the sum
 * of what it had left and `time`.  With none, the slot's whole text is
 * queued at the end. */
static void append_msg(uint32_t text, uint32_t slot, uint32_t time)
{
    uint32_t q = M32(S_QUEUE), node, head, prev, joined, more, msg, before;

    node = PREV(TAIL(q));
    head = HEAD(q);
    while (node != head && M32(M32(node + N_DATA) + W_SLOT) != slot)
        node = PREV(node);
    prev = node == head ? 0 : M32(M32(node + N_DATA) + W_TEXT);
    joined = slot_text(prev, text, slot);
    if (node == HEAD(M32(S_QUEUE))) {
        queue_after(PREV(TAIL(M32(S_QUEUE))), joined, slot, time);
        return;
    }
    more = H_STRLEN(text);
    msg = M32(node + N_DATA);
    if (M32(msg + W_KEEP) != 0) {
        if (node == NEXT(HEAD(M32(S_QUEUE)))) {
            int32_t w = limit();
            if ((int32_t)((M32(msg + W_LEN) << 3) - M32(msg + W_SCROLL) +
                          M32(msg + W_START)) <= w) {
                queue_after(node, text, slot, time);
                return;
            }
        }
        H_FREE(msg + W_TEXT);
        M32(msg + W_TEXT) = H_STRDUP(joined);
        M32(msg + W_LEN) = M32(msg + W_LEN) + more;
        return;
    }
    time = time + M32(msg + W_TIME);
    before = PREV(node);
    q_erase(M32(S_QUEUE), node);
    queue_after(before, joined, slot, time);
}

/* ---- the frame: .text+0x1770..+0x19e3 ------------------------------------- */

/* .text+0x1820 -- the window cleared to the fill */
static void blank(void)
{
    uint32_t rec = M32(S_REC);

    H_MEMSET(M32(rec + R_BITS), M8(S_FILL), M32(rec + R_SIZE));
}

/* .text+0x1850 -- the first message, scrolling: drawn from where it has
 * scrolled to, then moved on; done once all of it has gone off the left. */
static void show_scrolling(void)
{
    uint32_t node = NEXT(HEAD(M32(S_QUEUE))), msg = M32(node + N_DATA);
    uint32_t p, i, off, left, room, n;
    int32_t start, at;

    if (M32(msg + W_SCROLL) >> 3 >= M32(msg + W_LEN)) {
        CHAIN_SEND(0x14, 0x50, node);
        q_pop(M32(S_QUEUE));
        return;
    }
    H_MEMSET(S_LINE, 0x20, 0x400);
    M32(msg + W_SPEED) = ((uint32_t)GLB16(0x362) >> 1) + 2;
    start = (int32_t)M32(msg + W_START);
    at = start / 8;
    p = M32(msg + W_TEXT);
    i = 0;
    if ((int32_t)(M32(msg + W_SCROLL) >> 3) > 0) {
        do {
            if (M8(p) == 0)
                break;
            p = ADDR(CharNextA((LPCSTR)(uintptr_t)p));
            i++;
        } while ((int32_t)i < (int32_t)(M32(msg + W_SCROLL) >> 3));
    }
    off = p - M32(msg + W_TEXT);
    left = M32(msg + W_LEN) - off;
    room = 0x3ff - (uint32_t)at;
    n = left < room ? left : room;
    H_MEMMOVE(S_LINE + (uint32_t)at, p, n);
    M8(S_LINE + n + (uint32_t)at) = 0;
    if ((int32_t)M32(msg + W_START) > 0)
        M32(msg + W_START) = M32(msg + W_START) - M32(msg + W_SPEED);
    else
        M32(msg + W_SCROLL) = M32(msg + W_SCROLL) + M32(msg + W_SPEED);
    H_TEXT(M32(S_REC), 3, M32(S_COLOUR + 4 * M32(msg + W_SLOT)), S_LINE, 0);
}

/* .text+0x1970 -- the first message, plain: drawn once, counted down;
 * done at 0. */
static void show_plain(void)
{
    uint32_t node = NEXT(HEAD(M32(S_QUEUE))), msg = M32(node + N_DATA);

    if (M32(msg + W_TIME) == 0) {
        CHAIN_SEND(0x14, 0x50, node);
        q_pop(M32(S_QUEUE));
        return;
    }
    if (M32(msg + W_FRESH) == 1) {
        M32(msg + W_FRESH) = 0;
        blank();
        H_TEXT(M32(S_REC), 3, M32(S_COLOUR + 4 * M32(msg + W_SLOT)),
               M32(msg + W_TEXT), 0);
    }
    M32(msg + W_TIME) = M32(msg + W_TIME) - 1;
}

/* .text+0x1770 -- CHAIN list 2, each frame.  With nothing to show the
 * window is hidden, unless it has no status line (the flag kept by a
 * restored situation). */
static uint32_t FS98_STDCALL frame(uint32_t a, uint32_t b)
{
    uint32_t msg, rec, flags;

    (void)a;
    (void)b;
    msg = q_empty(M32(S_QUEUE)) ? 0 : M32(NEXT(HEAD(M32(S_QUEUE))) + N_DATA);
    if (msg == 0) {
        rec = M32(S_REC);
        flags = M32(rec + R_FLAGS);
        if (!(flags & F_SHOWN) || (flags & F_NO_STATUS))
            return flags;
        M32(rec + R_FLAGS) = flags & 0x7fffffffu;
        return H_WIN_UPDATE(M32(S_REC));
    }
    if (SCROLLS(M32(msg + W_SLOT)) && GLB32(0x1254) != 0) {
        blank();
        show_scrolling();
    } else {
        show_plain();
    }
    return H_WIN_FLUSH(M32(S_REC));
}

/* ---- the window and the situation: .text+0x1400, +0x1680 ------------------ */

/* .text+0x1400 -- message 0x37: the window gone.  Its title is freed
 * through the record's first word, then again by WINDOW. */
static uint32_t close_window(void)
{
    H_WIN_DESTROY(M32(S_REC));
    H_FREE(M32(S_REC));
    return WINDOW_FREE(S_REC);
}

/* .text+0x1680 -- CHAIN list 0x14: a situation's record 0x3df is the
 * window's flags and its two rectangles (0x22 in, 0x23 out). */
static uint32_t FS98_STDCALL situation(uint32_t message, uint32_t data)
{
    uint32_t src;

    if (message == 0x22) {
        if (M32(data) != 0x3df)
            return data;
        src = M32(data + 0xc);
        H_MEMMOVE(M32(S_REC) + 0x08, src, 4);
        src += 4;
        H_MEMMOVE(M32(S_REC) + 0x20, src, 0x10);
        src += 0x10;
        H_MEMMOVE(M32(S_REC) + 0x50, src, 0x10);
        return H_WIN_UPDATE(M32(S_REC));
    }
    if (message == 0x23) {
        MAIN_SAVE_OPEN(0x3df, 0x24);
        MAIN_SAVE_PUT(M32(S_REC) + 0x08, 4, 0);
        MAIN_SAVE_PUT(M32(S_REC) + 0x20, 0x10, 0);
        MAIN_SAVE_PUT(M32(S_REC) + 0x50, 0x10, 0);
        return MAIN_SAVE_CLOSE();
    }
    if (message == 0x37)
        return close_window();
    return message - 0x37;
}

/* ---- the Linkage: .text+0x1240..+0x167f ------------------------------------ */

/* .text+0x1240 -- +0x04 */
static uint32_t FS98_STDCALL winmsg_init(void)
{
    uint32_t rec;

    H_MEMSET(S_TEXTS, 0, 0x1c);
    H_ALLOC(1, 8, S_QUEUE, 0);
    q_init(M32(S_QUEUE));
    WINDOW_RECORD(S_REC, 0x9500, 5);
    M32(M32(S_REC) + R_FLAGS) = 0;
    M32(M32(S_REC) + 0x20) = 0x40;
    M32(M32(S_REC) + 0x24) = 0x32;
    M32(M32(S_REC) + 0x28) = 0x1f80;
    M32(M32(S_REC) + 0x2c) = 0x12c;
    M32(M32(S_REC) + 0x5c) = 0;
    M32(M32(S_REC) + 0x58) = 0;
    M32(M32(S_REC) + 0x54) = 0;
    M32(M32(S_REC) + 0x50) = 0;
    M32(M32(S_REC) + 0x18) = 0;
    M32(M32(S_REC) + 0x78) = 0x5000;
    M32(M32(S_REC) + 0x7c) = 0;
    H_WIN_CREATE(M32(S_REC));
    rec = M32(S_REC);
    M32(rec + 0x68) = 0x16;
    M32(M32(S_REC) + 0x70) = 0;
    M32(M32(S_REC) + 0x6c) = 0x16;
    M32(M32(S_REC) + 0x74) = 0x16;
    H_WIN_UPDATE(M32(S_REC));
    CHAIN_ADD(2, ADDR(frame));
    return CHAIN_ADD(0x14, ADDR(situation));
}

/* .text+0x1380 -- +0x08.  The window stays; message 0x37 closes it. */
static uint32_t FS98_STDCALL winmsg_deinit(void)
{
    uint32_t p, r;

    CHAIN_REMOVE(0x14, ADDR(situation));
    CHAIN_REMOVE(2, ADDR(frame));
    q_clear(M32(S_QUEUE));
    q_free(M32(S_QUEUE));
    r = H_FREE(S_QUEUE);
    for (p = S_TEXTS; p < S_TEXTS + 0x1c; p += 4)
        if (M32(p) != 0)
            r = H_FREE(p);
    return r;
}

/* .text+0x1620 -- +0x20: every message gone; the slots keep their texts.
 * Returns the head sentinel. */
static uint32_t FS98_STDCALL clear_all(void)
{
    return q_clear(M32(S_QUEUE));
}

/* .text+0x1450 -- +0x18: `text` in `slot`, for `time` frames.  Without
 * `append` the queue is emptied and the slot's text forgotten first, and a
 * plain message with no time is not shown at all.  Returns the last node
 * queued (the head sentinel if none), which is what message 0x50 names. */
static uint32_t FS98_STDCALL show(uint32_t text, uint32_t slot, uint32_t time,
                                  uint32_t append)
{
    uint32_t rec, flags;

    if (append == 0) {
        if (M32(S_TEXTS + 4 * slot) != 0)
            H_FREE(S_TEXTS + 4 * slot);
        clear_all();
        queue_after(PREV(TAIL(M32(S_QUEUE))), text, slot, time);
    } else {
        append_msg(text, slot, time);
    }
    rec = M32(S_REC);
    flags = M32(rec + R_FLAGS);
    if (!(flags & F_SHOWN)) {
        M32(rec + R_FLAGS) = flags | F_SHOWN;
        blank();
        H_WIN_UPDATE(M32(S_REC));
    }
    return PREV(TAIL(M32(S_QUEUE)));
}

/* .text+0x15b0 -- +0x1c: every message in `slot` gone, and its text.
 * Returns the queue. */
static uint32_t FS98_STDCALL clear_slot(uint32_t slot)
{
    uint32_t q = M32(S_QUEUE), node, next;

    node = NEXT(HEAD(q));
    if (M32(S_TEXTS + 4 * slot) != 0) {
        H_FREE(S_TEXTS + 4 * slot);
        q = M32(S_QUEUE);
    }
    while (node != TAIL(q)) {
        if (M32(M32(node + N_DATA) + W_SLOT) == slot) {
            next = NEXT(node);
            q_erase(q, node);
            q = M32(S_QUEUE);
            node = next;
        } else {
            node = NEXT(node);
        }
    }
    return q;
}

/* .text+0x1640 -- +0x24: 1 if anything in `slot` is queued */
static uint32_t FS98_STDCALL slot_queued(uint32_t slot)
{
    uint32_t q = M32(S_QUEUE), node = NEXT(HEAD(q)), tail = TAIL(q);

    if (node == tail)
        return 0;
    do {
        if (M32(M32(node + N_DATA) + W_SLOT) == slot)
            return 1;
        node = NEXT(node);
    } while (node != tail);
    return 0;
}

/* ---- the image -------------------------------------------------------------- */

struct winmsg_image winmsg_image = {{
#include "winmsg_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _winmsg_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _winmsg_image + 0x30");

static void winmsg_install(void)
{
    M32(D + WM_LINKAGE + 0x04) = ADDR(winmsg_init);
    M32(D + WM_LINKAGE + 0x08) = ADDR(winmsg_deinit);
    M32(D + WM_LINKAGE + 0x18) = ADDR(show);
    M32(D + WM_LINKAGE + 0x1c) = ADDR(clear_slot);
    M32(D + WM_LINKAGE + 0x20) = ADDR(clear_all);
    M32(D + WM_LINKAGE + 0x24) = ADDR(slot_queued);
}

/* .text+0x1000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        winmsg_install();
    }
    return TRUE;
}
