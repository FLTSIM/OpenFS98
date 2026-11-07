/* CSimsSlider (0x480 bytes, vtable 0x16d70): a CSimsButton drawn as a track (a left cap,
 * a middle repeated and a right cap, from a strip of three rows) with tick marks, and a
 * thumb (from a strip of three: normal, pressed, disabled) that the mouse and the arrow
 * keys move between positions.
 *
 * Beyond CSimsButton: its window +0xc0 and id +0xc4; dragging +0xcc; the remapped
 * rectangle +0xe0; the thumb's rectangle +0xf0 (its rest +0x100, where it was drawn
 * +0x110); the track's margin +0x120, the ticks' top +0x124, +0x128, bottom +0x12c and
 * count +0x130; the step between ticks +0x134, half of it +0x138, the travel +0x13c; the
 * thumb's offset while dragged +0x140; snapping off +0x144; needs painting +0x148; painted
 * +0x14c; the palette state given to the thumb +0x150; the position +0x154, the range's
 * start +0x158 and length +0x15c; the track's mask +0x160; the capturing window +0x164;
 * sections: the thumb's three frames +0x168, what is under it +0x1d8, the track +0x250,
 * the mask +0x2c0, the track strip +0x330, a thumb frame +0x3a0, the thumb strip +0x410;
 * the thumb's height +0x248 and width +0x24c. */
#include "simsui_internal.h"

#define VTABLE_SLIDER 0x16d70
#define SLIDER_SIZE 0x480
#define OBJECT_COUNT I32(fs98_simsui_image, 0x213cc)
#define BM_SETSTATE_ 0xf3
#define SLIDER_MOVED 0x209                   /* posted to the parent while dragging */

static const uint32_t sections[7] = {0x168, 0x1d8, 0x250, 0x2c0, 0x330, 0x3a0, 0x410};

/* 0x2047dad0 */
void *TC fs98_simsui_slider_construct(void *slider)
{
    int32_t at;
    fs98_simsui_button_construct(slider);
    for (at = 0; at < 7; ++at) fs98_simsui_dib_construct(AT(slider, sections[at]));
    D32(slider, 0x160) = 0;
    D32(slider, 0xcc) = 0;
    D32(slider, 0xb0) = 0;
    PTR(slider, 0) = IMAGE(VTABLE_SLIDER);
    D32(slider, 0x148) = 1;
    return slider;
}
/* 0x2047dc00 */
void TC fs98_simsui_slider_destroy(void *slider)
{
    int32_t at;
    PTR(slider, 0) = IMAGE(VTABLE_SLIDER);
    if (D32(slider, 0x160)) mfc_operator_delete(PTR(slider, 0x160));
    D32(slider, 0x160) = 0;
    for (at = 6; at >= 0; --at) fs98_simsui_dib_destroy(AT(slider, sections[at]));
    fs98_simsui_button_destroy(slider);
}
/* 0x2047dba0 */
void *TC fs98_simsui_slider_delete(void *slider, uint32_t flags)
{
    return fs98_simsui_delete_object(slider, flags, SLIDER_SIZE, fs98_simsui_slider_destroy, fs98_simsui_ui_delete);
}
/* 0x2047dce0 */
const void *TC fs98_simsui_slider_message_map(const void *slider) { (void)slider; return IMAGE(0x16c78); }
/* 0x2047dcf0, Init(track id, thumb id, background, CDIBPal *, margin, tick top, tick
 * height, positions, no snapping, key, HINSTANCE): placed by its text's spec
 * "name#l,t,r,b" (else where it is, remapped); the track built over the background with a
 * tick at each position, the thumb at the first. With no instance the module state's is
 * fetched and not used: the strips load from LoadDib's own default, which is the same. */
int32_t TC fs98_simsui_slider_init(void *slider, int32_t track_id, int32_t thumb_id, void *background, void *pal,
                                   int32_t margin, int32_t tick_top, int32_t tick_height, int32_t positions,
                                   int32_t no_snap, int32_t key, uint32_t instance)
{
    uint8_t dc[0x10], *info;
    uint32_t text, name, steps;
    int32_t left, top, piece, count, rest, at, ticks[4], point[2];
    void *old;
    D32(slider, 0xc4) = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(slider);
    D32(slider, 0xc0) = slider ? D32(slider, 0x20) : 0;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(slider, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_rect(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, (int32_t *)AT(slider, 0xe0));
    else {
        fs98_simsui_remap_rect((int32_t *)AT(slider, 0xe0), slider);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    left = I32(slider, 0xe0);
    top = I32(slider, 0xe4);
    I32(slider, 0x144) = no_snap;
    I32(slider, 0xb8) = I32(slider, 0xe8) - I32(slider, 0xe0);
    I32(slider, 0x130) = positions;
    I32(slider, 0x15c) = positions - 1;
    I32(slider, 0x124) = tick_top;
    I32(slider, 0x12c) = tick_height + tick_top;
    FS98_SIMSUI_BARRIER();
    steps = *(volatile uint32_t *)&(volatile uint32_t){(uint32_t)(I32(slider, 0xb8) - margin * 2) / (uint32_t)(positions - 1)};
    FS98_SIMSUI_BARRIER();
    I32(slider, 0x120) = margin;
    I32(slider, 0x128) = margin;
    D32(slider, 0xf4) = 0;
    D32(slider, 0xfc) = 0x13;
    D32(slider, 0x148) = 1;
    D32(slider, 0x158) = 0;
    D32(slider, 0x154) = 0;
    D32(slider, 0x134) = steps;
    I32(slider, 0x13c) = (int32_t)steps * (positions - 1);
    I32(slider, 0xf0) = margin - 6;
    I32(slider, 0x138) = (int32_t)steps / 2;
    I32(slider, 0xf8) = margin - 6 + 12;
    for (at = 0; at < 4; ++at) D32(slider, 0x100 + at * 4) = D32(slider, 0xf0 + at * 4);
    for (at = 0; at < 4; ++at) D32(slider, 0x110 + at * 4) = D32(slider, 0xf0 + at * 4);
    if (!instance) CALL_STD(MFC_1168, void *, (void))();
    PTR(slider, 0xb0) = pal;
    fs98_simsui_dib_load(AT(slider, 0x330), track_id, instance, pal, 0);
    I32(slider, 0xb4) = I32(slider, 0x33c) / 3;
    if (I32(slider, 0xb4) < 0) I32(slider, 0xb4) = -I32(slider, 0xb4);
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = I32(slider, 0xb8);
    I32(info, 8) = -I32(slider, 0xb4);
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(slider, 0xb0), 8 + at * 4);
    I32(info, 4) = I32(slider, 0xb8);
    I32(info, 8) = -I32(slider, 0xb4);
    fs98_simsui_dib_create(AT(slider, 0x250), info, PTR(slider, 0xb0), 0);
    fs98_simsui_dib_create(AT(slider, 0x2c0), info, PTR(slider, 0xb0), 0);
    I32(info, 4) = I32(slider, 0xb8);
    I32(info, 8) = -I32(slider, 0xb4);
    fs98_simsui_dib_create(AT(slider, 0x40), info, PTR(slider, 0xb0), 0);
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    {
        void *bitmap = AT(slider, 0x9c);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    fs98_simsui_dib_copy(background, AT(slider, 0x40), 0, 0, I32(slider, 0xb8), I32(slider, 0xb4), left, top);
    piece = I32(slider, 0x338);
    count = *(volatile int32_t *)&(volatile int32_t){(I32(slider, 0xb8) - piece * 2) / piece};
    rest = *(volatile int32_t *)&(volatile int32_t){(I32(slider, 0xb8) - piece * 2) % piece};
    fs98_simsui_dib_fill(AT(slider, 0x250), 0);
    fs98_simsui_dib_copy(AT(slider, 0x330), AT(slider, 0x250), 0, 0, piece, I32(slider, 0xb4), 0, 0);
    if (count > 0) {
        int32_t x = piece, remaining = count;
        do {
            fs98_simsui_dib_copy(AT(slider, 0x330), AT(slider, 0x250), x, 0, piece, I32(slider, 0xb4), 0, I32(slider, 0xb4));
            x += piece;
        } while (--remaining);
    }
    fs98_simsui_dib_copy(AT(slider, 0x330), AT(slider, 0x250), (count + 1) * piece, 0, rest, I32(slider, 0xb4), 0,
                         I32(slider, 0xb4));
    fs98_simsui_dib_copy(AT(slider, 0x330), AT(slider, 0x250), I32(slider, 0xb8) - piece, 0, piece, I32(slider, 0xb4), 0,
                         I32(slider, 0xb4) * 2);
    fs98_simsui_dib_sprite(AT(slider, 0x250), AT(slider, 0x40), 0, 0, I32(slider, 0xb8), I32(slider, 0xb4), 0, 0, key);
    ticks[0] = I32(slider, 0x120);
    ticks[1] = I32(slider, 0x124);
    ticks[2] = I32(slider, 0x128);
    ticks[3] = I32(slider, 0x12c);
    for (at = 0; at < I32(slider, 0x130); ++at) {
        CALL_THIS(MFC_4297, void *, (void *, void *, int32_t, int32_t))(dc, point, ticks[0], ticks[1]);
        CALL_THIS(MFC_4133, int32_t, (void *, int32_t, int32_t))(dc, ticks[2], ticks[3]);
        CALL_STD(OffsetRect, int32_t, (void *, int32_t, int32_t))(ticks, I32(slider, 0x134), 0);
    }
    fs98_simsui_dib_fill(AT(slider, 0x2c0), 0xff);
    fs98_simsui_dib_mask(AT(slider, 0x250), AT(slider, 0x2c0), 0, 0, I32(slider, 0xb8), I32(slider, 0xb4), 0, 0, 0, -1, 0);
    {
        uint32_t bytes = ((D32(slider, 0x2c8) + 3) & 0xfffffffc) * D32(slider, 0x2cc);
        PTR(slider, 0x160) = mfc_operator_new(bytes);
        fs98_simsui_copy(PTR(slider, 0x160), PTR(slider, 0x324), bytes);
    }
    fs98_simsui_dib_load(AT(slider, 0x410), thumb_id, instance, PTR(slider, 0xb0), 0);
    I32(slider, 0x248) = I32(slider, 0x41c) / 3;
    I32(slider, 0x24c) = I32(slider, 0x418);
    if (I32(slider, 0x248) < 0) I32(slider, 0x248) = -I32(slider, 0x248);
    I32(slider, 0x11c) = I32(slider, 0x248);
    I32(slider, 0x118) = I32(slider, 0x24c) + I32(slider, 0x110);
    I32(info, 4) = I32(slider, 0x24c);
    I32(info, 8) = -I32(slider, 0x248);
    fs98_simsui_dib_create(AT(slider, 0x3a0), info, PTR(slider, 0xb0), 0);
    I32(info, 4) = I32(slider, 0x24c);
    I32(info, 8) = -(I32(slider, 0x248) * 3);
    fs98_simsui_dib_create(AT(slider, 0x168), info, PTR(slider, 0xb0), 0);
    I32(info, 4) = I32(slider, 0x24c);
    I32(info, 8) = -I32(slider, 0x248);
    fs98_simsui_dib_create(AT(slider, 0x1d8), info, PTR(slider, 0xb0), 0);
    fs98_simsui_dib_copy_rect(AT(slider, 0x40), AT(slider, 0x1d8), (const int32_t *)AT(slider, 0x110));
    mfc_operator_delete(info);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
    D32(slider, 0x148) = 1;
    D32(slider, 0x14c) = 0;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        slider, left, top, I32(slider, 0xb8), I32(slider, 0xb4), 1);
    CALL_THIS(MFC_640, void, (void *))(dc);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
}
/* 0x2047e3b0, OnLButtonDown: dragging; on a part of it, the focus and the mouse taken and
 * the part posted to itself (0x5f7) */
void TC fs98_simsui_slider_button_down(void *slider, uint32_t keys, int32_t x, int32_t y)
{
    uint32_t hit;
    (void)keys; (void)x; (void)y;
    D32(slider, 0xcc) = 1;
    hit = fs98_simsui_slider_hit_test(slider, -1, -1);
    if (!hit) return;
    CALL_THIS(MFC_5981, void *, (void *))(slider);
    CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(SetCapture, uint32_t, (uint32_t))(D32(slider, 0x20)));
    D32(slider, 0x164) = D32(slider, 0x20);
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(slider, 0x20), 0x5f7, hit, 0);
}
/* 0x2047e410, OnLButtonUp: released, the command posted, the thumb snapped */
void TC fs98_simsui_slider_button_up(void *slider, uint32_t keys, int32_t x, int32_t y)
{
    void *parent;
    (void)keys; (void)x; (void)y;
    if (!OBJECT_COUNT) return;
    CALL_STD(ReleaseCapture, int32_t, (void))();
    D32(slider, 0xcc) = 0;
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(slider, 0x20), BM_SETSTATE_, 0, 0);
    parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(slider, 0x20)));
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111, D16(slider, 0xc4),
                                                                             D32(slider, 0xc0));
    fs98_simsui_slider_set_thumb(slider, -1);
}
/* the parent told the slider moved */
static void tell_moved(void *slider)
{
    void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(slider, 0x20)));
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), SLIDER_MOVED,
                                                                             D32(slider, 0xc4), D32(slider, 0xc4));
}
/* 0x2047e480, OnMouseMove: dragging over a part, the position follows the mouse (to a
 * tick when the range is the ticks', snapped unless +0x144) and the parent is told.
 * RETAIL BUG (kept): SUI-9 (docs/retail-bugs.md): past the end the offset is clamped to the
 * margin plus the travel, not the travel. */
void TC fs98_simsui_slider_move(void *slider, uint32_t keys, int32_t x, int32_t y)
{
    int32_t last;
    (void)keys;
    if (!D32(slider, 0xcc)) return;
    if (!fs98_simsui_slider_hit_test(slider, x, y)) return;
    I32(slider, 0x140) = x - I32(slider, 0x120);
    if (I32(slider, 0x140) < 0) I32(slider, 0x140) = 0;
    else if (I32(slider, 0x140) > I32(slider, 0x120) + I32(slider, 0x13c)) I32(slider, 0x140) = I32(slider, 0x120) + I32(slider, 0x13c);
    last = I32(slider, 0x130) - 1;
    if (I32(slider, 0x15c) != last) {
        int32_t offset = I32(slider, 0x140);
        int32_t position = *(volatile int32_t *)&(volatile int32_t){I32(slider, 0x15c) * offset * 10 / I32(slider, 0x13c)};
        position = (position + 5) / 10;
        if (position == I32(slider, 0x154)) return;
        I32(slider, 0x154) = position;
        fs98_simsui_slider_set_thumb(slider, offset);
    } else {
        int32_t offset = I32(slider, 0x140);
        int32_t tick = *(volatile int32_t *)&(volatile int32_t){offset / I32(slider, 0x134)};
        int32_t into = offset % I32(slider, 0x134);
        I32(slider, 0x154) = tick;
        if (into > I32(slider, 0x138) && tick < last) I32(slider, 0x154) = tick + 1;
        fs98_simsui_slider_set_thumb(slider, I32(slider, 0x144) ? -1 : offset);
    }
    tell_moved(slider);
}
/* 0x2047e5e0, OnMyKeyDown: Page Up/Down a step, Home/End the ends, the arrows one; the
 * command posted. Other keys go to the parent. */
uint32_t TC fs98_simsui_slider_key_down(void *slider, uint32_t key, uint32_t lparam)
{
    void *parent;
    switch (key) {
    case 0x21: I32(slider, 0x154) -= I32(slider, 0x134); break;
    case 0x22: I32(slider, 0x154) += I32(slider, 0x134); break;
    case 0x23: I32(slider, 0x154) = I32(slider, 0x15c); break;
    case 0x24: I32(slider, 0x154) = 0; break;
    case 0x25: case 0x26: --I32(slider, 0x154); break;
    case 0x27: case 0x28: ++I32(slider, 0x154); break;
    default:
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
            CALL_STD(GetParent, uint32_t, (uint32_t))(D32(slider, 0x20)), 0x100, key, lparam);
        return 0;
    }
    fs98_simsui_slider_set_thumb(slider, -1);
    parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(slider, 0x20)));
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111, D16(slider, 0xc4),
                                                                             D32(slider, 0xc0));
    return 1;
}
/* 0x2047e6e0, GetScrollPos */
int32_t TC fs98_simsui_slider_position(const void *slider) { return I32(slider, 0x158) + I32(slider, 0x154); }
/* 0x2047e6f0, SetScrollPos */
int32_t TC fs98_simsui_slider_set_position(void *slider, int32_t position)
{
    I32(slider, 0x154) = position - I32(slider, 0x158);
    fs98_simsui_slider_set_thumb(slider, -1);
    return 1;
}
/* 0x2047e720, SetScrollRange(first, last): snapping on, the step rescaled to the range */
void TC fs98_simsui_slider_set_range(void *slider, int32_t first, int32_t last)
{
    int32_t length = last - first, step;
    I32(slider, 0x158) = first;
    I32(slider, 0x15c) = length;
    D32(slider, 0x144) = 0;
    step = *(volatile int32_t *)&(volatile int32_t){I32(slider, 0x134) * length * 10 / I32(slider, 0x13c)};
    I32(slider, 0x134) = (step + 5) / 10;
}
/* 0x2047e780, GetHitTest(x, y): the part under a point (the cursor's for x -1): the thumb
 * 6, the track before it 10 and after it 11, else 0 */
uint32_t TC fs98_simsui_slider_hit_test(void *slider, int32_t x, int32_t y)
{
    int32_t point[2];
    if (x == -1) {
        CALL_STD(GetCursorPos, int32_t, (int32_t *))(point);
        CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(slider, 0x20), point);
    } else {
        point[0] = x;
        point[1] = y;
    }
    if (CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))((const int32_t *)AT(slider, 0xf0), point[0], point[1]))
        return 6;
    if (point[0] < 0 || point[0] > I32(slider, 0xe8) - I32(slider, 0xe0)) return 0;
    if (point[0] < I32(slider, 0xf0)) return 0xa;
    return point[0] <= I32(slider, 0xf8) ? 6 : 0xb;
}
/* 0x2047e840, SetThumbPos(offset): the position clamped, the thumb moved to the offset (or,
 * for -1, to the position's), the window repainted */
void TC fs98_simsui_slider_set_thumb(void *slider, int32_t offset)
{
    int32_t last = I32(slider, 0x15c), at;
    if (I32(slider, 0x154) > last) I32(slider, 0x154) = last;
    if (I32(slider, 0x154) < 0) I32(slider, 0x154) = 0;
    D32(slider, 0x14c) = 1;
    for (at = 0; at < 4; ++at) D32(slider, 0xf0 + at * 4) = D32(slider, 0x100 + at * 4);
    if (offset == -1) {
        int32_t position = I32(slider, 0x154);
        if (!position) {
            CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(slider, 0x20), 0, 1);
            return;
        }
        if (last == I32(slider, 0x130) - 1) offset = I32(slider, 0x134) * position;
        else {
            int32_t travel = I32(slider, 0x13c);
            offset = *(volatile int32_t *)&(volatile int32_t){travel * position * 10 / last};
            offset = (offset + 5) / 10;
            if (offset < 0) offset = 0;
            if (offset > travel) offset = travel;
        }
    } else if (offset > I32(slider, 0x13c)) offset = I32(slider, 0x13c);
    I32(slider, 0xf0) += offset;
    I32(slider, 0xf8) += offset;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(slider, 0x20), 0, 1);
    CALL_STD(UpdateWindow, int32_t, (uint32_t))(D32(slider, 0x20));
}
/* 0x2047e970, DrawItem: the thumb, kept inside the track, drawn over what is under it in
 * its frame for the state (pressed or focused 1, disabled 2) */
void TC fs98_simsui_slider_draw_item(void *slider, const uint8_t *item)
{
    uint8_t dc[0x10], memory[0x10];
    uint32_t state;
    int32_t row = 0;
    void *old_pal, *old, *pal;
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    state = D32(item, 0x10);
    if (!PTR(slider, 0xb0)) {
        CALL_THIS(MFC_640, void, (void *))(memory);
        CALL_THIS(MFC_640, void, (void *))(dc);
        return;
    }
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, D32(item, 0x18));
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
            CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        void *bitmap;
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(slider, 0xb0), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        I32(slider, 0x110) = I32(slider, 0xf0);
        I32(slider, 0x118) = I32(slider, 0x24c) + I32(slider, 0xf0);
        if (I32(slider, 0x118) > I32(slider, 0x48)) {
            int32_t over = I32(slider, 0x118) - I32(slider, 0x48);
            I32(slider, 0x110) = I32(slider, 0xf0) - over;
            I32(slider, 0x118) -= over;
        }
        fs98_simsui_dib_copy(AT(slider, 0x40), AT(slider, 0x1d8), 0, 0, I32(slider, 0x24c), I32(slider, 0x248),
                             I32(slider, 0x110), 0);
        if (state & 0x11) row = I32(slider, 0x248);
        if (state & 4) row = I32(slider, 0x248) * 2;
        bitmap = AT(slider, 0x1c4);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        pal = PTR(slider, 0xb0);
        if (D32(slider, 0x150) != D32(pal, 0x808)) {
            uint32_t active_process = 0, foreground_process = 0, active, foreground;
            void *table;
            active = CALL_STD(GetActiveWindow, uint32_t, (void))();
            foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
            if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
            if (foreground)
                CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
            if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
                table = AT(pal, 0x408);
            else table = AT(pal, 8);
            CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
            D32(slider, 0x150) = D32(PTR(slider, 0xb0), 0x808);
        }
        fs98_simsui_dib_copy(AT(slider, 0x40), AT(slider, 0x168), 0, 0, I32(slider, 0x24c), I32(slider, 0x248),
                             I32(slider, 0x110), 0);
        fs98_simsui_dib_fill(AT(slider, 0x3a0), 0);
        fs98_simsui_dib_copy(AT(slider, 0x410), AT(slider, 0x3a0), 0, 0, I32(slider, 0x24c), I32(slider, 0x248), 0, row);
        fs98_simsui_dib_sprite(AT(slider, 0x3a0), AT(slider, 0x168), 0, 0, I32(slider, 0x24c), I32(slider, 0x248), 0, 0, -1);
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), I32(slider, 0x110), 0, I32(slider, 0x24c), I32(slider, 0x248), D32(memory, 4), 0, 0, 0xcc0020);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_2450, uint32_t, (void *))(dc);
    D32(slider, 0x14c) = 0;
    CALL_STD(ValidateRect, int32_t, (uint32_t, const void *))(D32(slider, 0x20), 0);
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_640, void, (void *))(dc);
}
/* 0x2047ece0, OnEraseBkgnd: the track copied in through the palette */
int32_t TC fs98_simsui_slider_erase(void *slider, void *dc)
{
    uint8_t memory[0x10];
    void *old_pal, *old, *bitmap, *pal, *table;
    uint32_t active_process = 0, foreground_process = 0, active, foreground;
    CALL_THIS(MFC_323, void *, (void *))(memory);
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
            CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(dc ? D32(dc, 4) : 0))) {
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(slider, 0xb0), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        bitmap = AT(slider, 0x9c);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        pal = PTR(slider, 0xb0);
        active = CALL_STD(GetActiveWindow, uint32_t, (void))();
        foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
        if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
        if (foreground) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
        if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
            table = AT(pal, 0x408);
        else table = AT(pal, 8);
        CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), 0, 0, I32(slider, 0xb8), I32(slider, 0xb4), D32(memory, 4), 0, 0, 0xcc0020);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_640, void, (void *))(memory);
    return 1;
}
/* 0x2047ee90, OnGetDlgCode: arrows and a button */
uint32_t TC fs98_simsui_slider_dialog_code(void *slider) { (void)slider; return 0x2001; }
/* 0x2047eea0, OnNcPaint: unless just painted, repainted */
void TC fs98_simsui_slider_nc_paint(void *slider)
{
    if (D32(slider, 0x14c) == 1) return;
    D32(slider, 0x148) = 1;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(slider, 0x20), 0, 1);
}
/* 0x2047eed0, OnKillFocus */
void TC fs98_simsui_slider_kill_focus(void *slider, void *next)
{
    (void)next;
    CALL_THIS(MFC_2379, uint32_t, (void *))(slider);
    D32(slider, 0xcc) = 0;
}
/* 0x2047eef0, OnSliderClicked (0x5f7, the part): the thumb pressed, or the track stepped
 * by one. RETAIL BUG (kept): SUI-10 (docs/retail-bugs.md): stepping past the end puts the
 * position at the travel less 6 (pixels, not a position) and the thumb is not moved. */
uint32_t TC fs98_simsui_slider_clicked(void *slider, uint32_t part, uint32_t lparam)
{
    (void)lparam;
    if (!D32(slider, 0xcc)) return 0;
    switch (part) {
    case 6:
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(slider, 0x20), BM_SETSTATE_, 1, 0);
        return 0;
    case 0xa:
        if (--I32(slider, 0x154) < 0) {
            I32(slider, 0x154) = 0;
            return 0;
        }
        break;
    case 0xb:
        if (++I32(slider, 0x154) >= I32(slider, 0x13c)) {
            I32(slider, 0x154) = I32(slider, 0x13c) - 6;
            return 0;
        }
        break;
    default:
        return 0;
    }
    fs98_simsui_slider_set_thumb(slider, -1);
    return 0;
}
