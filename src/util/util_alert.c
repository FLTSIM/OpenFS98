/* UTIL.DLL's alert boxes: slots 0xa4, 0xa8 and 0xac, and their text builder.
 *
 * These were first read as a general dialog framework.  They are narrower than
 * that: the dialogs they open are ids 2 and 3 in the host's dialog registry,
 * and both belong to English\Alerts.DLL (module 0x80000009) -- 2 modal, 3
 * modeless.  So the trio is the game's message-box API:
 *
 *   +0xa4  run a modal alert and return what it answered
 *   +0xa8  open a modeless one, handing back an object
 *   +0xac  close a modeless one and free it
 *
 * The host side -- registry, modal loop, modeless list -- is described in
 * docs/modules/host.md under "The dialog engine".
 */

#include <stdint.h>

#include <fs98/host.h>

#include "util_imports.h"


#define H FS98_UTIL_HOST

/* The alert's handler when the caller supplies none: `xor eax,eax / ret 4`. */
static int32_t FS98_STDCALL fs98_util_alert_no_handler(void *alert)
{
    (void)alert;
    return 0;
}

/* ---- the text builder (.text+0x2480) --------------------------------------
 *
 * Joins up to five pieces into one heap block, separated by a space -- or by a
 * newline when `lines` is non-zero.  Each piece is the Win32 MAKEINTRESOURCE
 * idiom: below 0x10000 it is a string-resource id, loaded through the host
 * into a 256-byte buffer (so at most 254 characters survive); at or above it
 * is a `char *`.  A zero piece is skipped.
 *
 * The block is sized as it goes, and the order is exact: the separator is
 * appended BEFORE the block is grown for the next piece, so it lands in the
 * four bytes of slack the previous allocation left, and the new size is
 * measured with the separator already in it.
 *
 * The first piece is not optional.  If it is zero, retail never initialises
 * its block pointer and the next append writes through stack garbage; here the
 * pointer starts null, which faults as surely.  No caller passes zero.
 */
static const char *fs98_util_alert_piece(uint32_t piece, char *buf)
{
    if (piece < 0x10000) {
        FS98_HOST_LOADSTRING(H)(piece, buf, 0xff);
        return buf;
    }
    return (const char *)(uintptr_t)piece;
}

static char *fs98_util_alert_text(uint32_t p1, uint32_t p2, uint32_t p3,
                                  uint32_t p4, uint32_t p5, int32_t lines)
{
    char buf[0x100];
    char *block = 0;
    const uint32_t more[4] = { p2, p3, p4, p5 };
    const char *s;
    unsigned i;

    if (p1 != 0) {
        s = fs98_util_alert_piece(p1, buf);
        FS98_HOST_ALLOC(H)(1, FS98_HOST_STRLEN(H)(s) + 4, (void **)&block, 0);
        FS98_HOST_STRCPY(H)(block, s);
    }
    for (i = 0; i < 4; i++) {
        if (more[i] == 0)
            continue;
        FS98_HOST_STRCAT(H)(block, lines ? "\n" : " ");
        s = fs98_util_alert_piece(more[i], buf);
        FS98_HOST_REALLOC(H)(1, FS98_HOST_STRLEN(H)(s) + FS98_HOST_STRLEN(H)(block) + 4,
                             (void **)&block);
        FS98_HOST_STRCAT(H)(block, s);
    }
    return block;
}

/* ---- slot 0xa4: a modal alert ---------------------------------------------
 *
 * Builds a 24-byte descriptor on the stack and runs dialog 2 over it:
 *
 *   +0x00  0
 *   +0x04  handler, the do-nothing default
 *   +0x08  flags
 *   +0x0c  the answer -- written by the dialog, and NOT initialised first, so
 *          an alert that closes without answering returns stack garbage
 *   +0x10  tag
 *   +0x14  the text block
 *
 * The text block is freed afterwards.  With flag 8 set, an answer of exactly 8
 * also calls host +0x24, which closes the main window: the "exit" answer.
 */
int32_t FS98_STDCALL fs98_util_alert_modal(uint32_t flags, uint32_t tag,
                                           uint32_t p1, uint32_t p2, uint32_t p3,
                                           uint32_t p4, uint32_t p5, int32_t lines)
{
    struct {
        uint32_t zero;
        int32_t (FS98_STDCALL *handler)(void *);
        uint32_t flags;
        int32_t  answer;
        uint32_t tag;
        char    *text;
    } d;

    d.text    = fs98_util_alert_text(p1, p2, p3, p4, p5, lines);
    d.zero    = 0;
    d.handler = fs98_util_alert_no_handler;
    d.flags   = flags;
    d.tag     = tag;
    d.answer  = 0;              /* retail leaves this as whatever was there */

    FS98_HOST_DIALOG(H)(2, &d);
    FS98_HOST_FREE(H)((void **)&d.text);

    if ((flags & 8) && d.answer == 8)
        FS98_HOST_QUIT(H)();
    return d.answer;
}

/* ---- slot 0xa8: open a modeless alert --------------------------------------
 *
 * Allocates a 0x28-byte object through the host, stores it through `out`
 * BEFORE filling it in, and registers it as dialog 3 through +0x110:
 *
 *   +0x00  0
 *   +0x04  handler, the do-nothing default
 *   +0x08  tag
 *   +0x0c  1 -- alive; +0xac clears it to make the alert close itself
 *   +0x10  0
 *   +0x14  uninitialised, sixteen bytes of whatever malloc returned
 *   +0x24  the text block
 *
 * The descriptor layout is not the modal one: the tag moves from +0x10 to +0x08
 * and the text from +0x14 to +0x24.  Alerts.DLL's two create functions each
 * read their own.
 */
void FS98_STDCALL fs98_util_alert_open(void **out, uint32_t tag,
                                       uint32_t p1, uint32_t p2, uint32_t p3,
                                       uint32_t p4, uint32_t p5, int32_t lines)
{
    unsigned char *o;

    FS98_HOST_ALLOC(H)(1, 0x28, (void **)&o, 1);
    *out = o;
    *(uint32_t *)(o + 0x00) = 0;
    *(uint32_t *)(o + 0x0c) = 1;
    *(uint32_t *)(o + 0x10) = 0;
    *(void **)(o + 0x04)    = (void *)fs98_util_alert_no_handler;
    *(uint32_t *)(o + 0x08) = tag;
    *(char **)(o + 0x24)    = fs98_util_alert_text(p1, p2, p3, p4, p5, lines);
    FS98_HOST_DIALOG_REG(H)(3, o);
}

/* ---- slot 0xac: close a modeless alert -------------------------------------
 *
 * Clears the live flag, then runs the host's modeless-dialog tick (+0x114),
 * which sends every open modeless dialog WM_USER+100 -- synchronously -- so
 * the alert sees the flag down and destroys itself before its object is
 * freed.  Then the text, then the object, through `pp`, which the host nulls.
 */
void FS98_STDCALL fs98_util_alert_close(void **pp)
{
    unsigned char *o = *pp;

    *(uint32_t *)(o + 0x0c) = 0;
    FS98_HOST_DIALOG_TICK(H)();
    FS98_HOST_FREE(H)((void **)(o + 0x24));
    FS98_HOST_FREE(H)(pp);
}
