/* ANALYSIS's two graphs: the landing (0x14050002) and the manoeuvre (0x14050003).
 * Each sizes a control (its window kept at +0x10dc) to its remapped frame and lets
 * ANALYSIS draw into it through the block's control (+8): 0 when set up, 1 to draw, 2
 * when OK or Cancel closes the dialog. The blocks are in options_internal.h.
 *
 * Landing (template 0x38a, class at 0x22397900, 0x1724 bytes): the labels 0x10ec, 0x11a4,
 * 0x125c, 0x1314 (the caption), 0x13cc and 0x1484 (the touchdown), title art 0x153c, OK
 * 0x1624, the graph 0x11a8. +0x10e0 is set until the caption has been made, +0x10e4 the
 * block, +0x10e8 the last operation, +0x1720 the answer state.
 *
 * Manoeuvre (template 0x38b, class at 0x2239ab90, 0x1444 bytes): title art 0x10e8, OK
 * 0x11d0, the labels 0x12cc (the scale) and 0x1384, the graph 0x11af. +0x10e0 the block,
 * +0x10e4 the last operation, +0x143c the answer state, +0x1440 set once the scale's
 * label has been made. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define LANDING(object) PTR(object, 0x10e4)
#define MANOEUVRE(object) PTR(object, 0x10e0)

static const char s_bad_block[] = "Bad m_pliveInfo\npassed to dialog by core code";

static uint32_t control(void *block, uint32_t operation, uint32_t window)
{
    return ((fs98_options_graph_fn)PTR(block, 8))(operation, block, window);
}
static int32_t skin(void *object)
{
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
        object, 0x14f, fs98_options_resource_module(), 0xf);
}
static void ok_button(void *object, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_options_resource_module(), 1, 1, empty);
}
/* the graph control sized to its remapped frame, its window kept */
static void fit_graph(void *object, int32_t id)
{
    struct fs98_options_rect rect;
    void *graph = SERVICES->dialog_item(object, id);
    D32(object, 0x10dc) = D32(graph, 0x20);
    SERVICES->remap_rect(&rect, graph);
    SERVICES->move_window(graph, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
}
/* a block that is missing, or MSVC's debug fill, is reported to the user */
static uint32_t has_block(void *block)
{
    if (block != 0 && block != (void *)(uintptr_t)0xcdcdcdcd) return 1;
    SERVICES->message_box(s_bad_block, 0, 0);
    return 0;
}

/* ---- the landing graph -------------------------------------------------------------------- */

/* 0x22397b70, WM_USER+100: draw */
int32_t TC fs98_options_landing_draw(void *object, uint32_t w_param, uint32_t l_param)
{
    void *block = LANDING(object);
    uint32_t window;
    (void)w_param; (void)l_param;
    D32(object, 0x10e8) = 1;
    window = D32(object, 0x10dc);
    control(block, 1, window);
    return 0;
}
/* 0x22397ba0, OnOK, and 0x22397bf0, OnCancel */
void TC fs98_options_landing_accept(void *object)
{
    D32(object, 0x1720) = 1;
    D32(LANDING(object), 0) = 1;
    {
        uint32_t window = D32(object, 0x10dc);
        void *block = LANDING(object);
        D32(object, 0x10e8) = 2;
        control(block, 2, window);
    }
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
void TC fs98_options_landing_cancel(void *object)
{
    D32(object, 0x1720) = 2;
    D32(LANDING(object), 0) = 0;
    {
        uint32_t window = D32(object, 0x10dc);
        void *block = LANDING(object);
        D32(object, 0x10e8) = 2;
        control(block, 2, window);
    }
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
/* 0x22398010 */
void TC fs98_options_landing_set_up(void *object)
{
    void *block = LANDING(object);
    D32(object, 0x10e8) = 0;
    control(block, 0, D32(object, 0x10dc));
}
/* 0x22398030 */
uint32_t TC fs98_options_landing_has_descriptor(void *object) { return has_block(LANDING(object)); }
/* 0x22397c40, OnInitDialog */
int32_t TC fs98_options_landing_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object)) return 0;
    ok_button(object, 0x1624);
    SERVICES->init_art(AT(object, 0x153c), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_text(AT(object, 0x125c), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x1314), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x13cc), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x1484), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x11a4), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x10ec), AT(object, 0x220));
    if (!fs98_options_landing_has_descriptor(object)) return 0;
    fit_graph(object, 0x11a8);
    fs98_options_landing_set_up(object);
    return 1;
}
struct scale_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *unused, *caption;
    volatile uint32_t live;
};
static void scale_cleanup(struct fs98_options_unwind *frame)
{
    struct scale_unwind *state = (void *)frame;
    if (state->live > 1) { state->live = 1; SERVICES->string_destroy(state->caption); }
    if (state->live > 0) { state->live = 0; SERVICES->string_destroy(state->unused); }
}
/* 0x22397d90: the scale's two ends and its zero, right-aligned in white beside the graph,
 * a seventh and five sixteenths of its height apart; the first time, the caption (the
 * block's, two spaces and string 0xf750) and the touchdown label. The CString made first
 * is used only as the string's buffer. */
FS98_OPTIONS_NOINLINE void TC fs98_options_landing_scale(void *object, void *dc)
{
    struct scale_unwind state;
    struct fs98_options_string unused, caption;
    struct fs98_options_rect rect;
    char high[0x10], low[0x10];
    int32_t step, y;
    void **vtable;
    void *graph;
    state.unused = &unused; state.caption = &caption; state.live = 0;
    fs98_options_unwind_enter(&state.frame, scale_cleanup);
    SERVICES->string_construct(&unused);
    state.live = 1;
    SERVICES->format_int(high, "%4d", SERVICES->to_int(PTR(LANDING(object), 0x14)));
    SERVICES->format_int(low, "%4d", SERVICES->to_int(PTR(LANDING(object), 0x18)));
    if (D32(object, 0x10e0) != 0) {
        void *block;
        SERVICES->string_construct(&caption);
        state.live = 2;
        block = LANDING(object);
        D32(object, 0x10e0) = 0;
        SERVICES->string_assign(&caption, PTR(block, 0x1c));
        SERVICES->string_append(&caption, "  ");
        {
            char *buffer = SERVICES->get_buffer(&unused, 0x100);
            SERVICES->load_string_a(D32(object, 0x80), 0xf750, buffer, 0x100);
        }
        SERVICES->release_buffer(&unused, -1);
        SERVICES->string_append_string(&caption, &unused);
        SERVICES->set_sims_text(AT(object, 0x1314), caption.text);
        SERVICES->set_sims_text(AT(object, 0x1484), PTR(LANDING(object), 0x10));
        state.live = 1;
        SERVICES->string_destroy(&caption);
    }
    graph = SERVICES->dialog_item(object, 0x11a8);
    SERVICES->get_window_rect(D32(graph, 0x20), &rect);
    SERVICES->screen_to_client(D32(object, 0x20), &rect.left);
    SERVICES->screen_to_client(D32(object, 0x20), &rect.right);
    step = (rect.bottom - rect.top) / 16;
    y = rect.bottom - step * 7 - 2;
    SERVICES->dc_mode(dc, 1);
    vtable = PTR(dc, 0);
    ((uint32_t (TC *)(void *, uint32_t))vtable[0x38 / 4])(dc, 0xffffff);
    SERVICES->dc_align(dc, 2);
    {
        int32_t (TC *text_out)(void *, int32_t, int32_t, const char *, int32_t) =
            (int32_t (TC *)(void *, int32_t, int32_t, const char *, int32_t))vtable[0x64 / 4];
        text_out(dc, rect.left - 0x10, y, high, SERVICES->text_length(high));
        y -= step * 5 + 2;
        text_out(dc, rect.left - 0x10, y, low, SERVICES->text_length(low));
        text_out(dc, rect.left - 0x10, rect.bottom - (step * 2 + 2), "0", 1);
    }
    scale_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}
struct paint_unwind {
    struct fs98_options_unwind frame;
    void *dc;
    volatile uint32_t live;
};
static void paint_cleanup(struct fs98_options_unwind *frame)
{
    struct paint_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->paint_destroy(state->dc); }
}
/* 0x22398060, WM_PAINT: with a landing to draw, the scale, then ANALYSIS draws */
FS98_OPTIONS_NOINLINE void TC fs98_options_landing_paint(void *object)
{
    struct paint_unwind state;
    uint8_t dc[0x54];
    state.dc = dc; state.live = 0;
    fs98_options_unwind_enter(&state.frame, paint_cleanup);
    SERVICES->paint_construct(dc, object);
    state.live = 1;
    if (D32(LANDING(object), 0xc) != 0) {
        uint32_t window;
        void *block;
        fs98_options_landing_scale(object, dc);
        window = D32(object, 0x10dc);
        block = LANDING(object);
        D32(object, 0x10e8) = 1;
        control(block, 1, window);
    }
    paint_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}

/* ---- the manoeuvre graph ------------------------------------------------------------------ */

struct label_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *label, *caption;
    volatile uint32_t live;
};
static void label_cleanup(struct fs98_options_unwind *frame)
{
    struct label_unwind *state = (void *)frame;
    if (state->live > 1) { state->live = 1; SERVICES->string_destroy(state->caption); }
    if (state->live > 0) { state->live = 0; SERVICES->string_destroy(state->label); }
}
/* 0x2239ad50, WM_USER+100: draw; the first time there is a scale, its label (the block's
 * text and string 0xf751 or 0xf750) */
FS98_OPTIONS_NOINLINE int32_t TC fs98_options_manoeuvre_draw(void *object, uint32_t w_param, uint32_t l_param)
{
    struct label_unwind state;
    struct fs98_options_string label, caption;
    (void)w_param; (void)l_param;
    state.label = &label; state.caption = &caption; state.live = 0;
    fs98_options_unwind_enter(&state.frame, label_cleanup);
    SERVICES->string_construct(&label);
    state.live = 1;
    SERVICES->string_construct(&caption);
    state.live = 2;
    {
        uint32_t window = D32(object, 0x10dc);
        void *block = MANOEUVRE(object);
        D32(object, 0x10e4) = 1;
        control(block, 1, window);
    }
    if (D32(object, 0x1440) == 0 && D32(MANOEUVRE(object), 0x10) != 0) {
        char *buffer;
        uint32_t id;
        D32(object, 0x1440) = 1;
        SERVICES->string_assign(&label, PTR(MANOEUVRE(object), 0x14));
        id = D32(MANOEUVRE(object), 0xc) != 0 ? 0xf751 : 0xf750;
        buffer = SERVICES->get_buffer(&caption, 0x100);
        SERVICES->load_string_a(D32(object, 0x80), id, buffer, 0x100);
        SERVICES->release_buffer(&caption, -1);
        SERVICES->string_append_string(&label, &caption);
        SERVICES->set_sims_text(AT(object, 0x12cc), label.text);
    }
    label_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
    return 0;
}
/* 0x2239ae90, OnOK, and 0x2239aee0, OnCancel */
void TC fs98_options_manoeuvre_accept(void *object)
{
    D32(object, 0x143c) = 1;
    D32(MANOEUVRE(object), 0) = 1;
    {
        uint32_t window = D32(object, 0x10dc);
        void *block = MANOEUVRE(object);
        D32(object, 0x10e4) = 2;
        control(block, 2, window);
    }
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
void TC fs98_options_manoeuvre_cancel(void *object)
{
    D32(object, 0x143c) = 2;
    D32(MANOEUVRE(object), 0) = 0;
    {
        uint32_t window = D32(object, 0x10dc);
        void *block = MANOEUVRE(object);
        D32(object, 0x10e4) = 2;
        control(block, 2, window);
    }
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
/* 0x2239b090 */
void TC fs98_options_manoeuvre_set_up(void *object)
{
    void *block = MANOEUVRE(object);
    D32(object, 0x10e4) = 0;
    control(block, 0, D32(object, 0x10dc));
}
/* 0x2239b0b0 */
uint32_t TC fs98_options_manoeuvre_has_descriptor(void *object) { return has_block(MANOEUVRE(object)); }
/* 0x2239af20, OnInitDialog: OK before the title art, unlike the landing's */
int32_t TC fs98_options_manoeuvre_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object)) return 0;
    ok_button(object, 0x11d0);
    SERVICES->init_art(AT(object, 0x10e8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_text(AT(object, 0x1384), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x12cc), AT(object, 0x220));
    if (!fs98_options_manoeuvre_has_descriptor(object)) return 0;
    fit_graph(object, 0x11af);
    fs98_options_manoeuvre_set_up(object);
    return 1;
}
