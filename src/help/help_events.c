/* Help's dialog handlers; offsets describe the retail PE32 object layouts.
 * The service boundary preserves call order and lets tests replace mutable
 * descriptors and vtables at the same points as external MFC/Win32 calls. */
#include "help_internal.h"
#include "help_tables.inc"
#include "help_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_HELP_THISCALL
static const struct fs98_help_services *s_services;
static uint32_t s_resource_module;

void fs98_help_set_resource_module(uint32_t module) { s_resource_module = module; }

void fs98_help_set_services(const struct fs98_help_services *services)
{ s_services = services; }

/* These handlers fetch the close slot before either state write. */
static void fs98_help_answer(void *object, uint32_t descriptor_offset, uint32_t state, uint32_t answer)
{
    void *descriptor = PTR(object, descriptor_offset);
    void *vtable = PTR(object, 0);
    if (state) D32(object, state) = answer ? 1 : 2;
    D32(descriptor, 0) = answer;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
void TC fs98_help_about_cancel(void *object) { fs98_help_answer(object, 0x10dc, 0x1978, 0); }
void TC fs98_help_about_accept(void *object) { fs98_help_answer(object, 0x10dc, 0x1978, 1); }
void TC fs98_help_credits_cancel(void *object) { fs98_help_answer(object, 0x1174, 0x1498, 0); }
void TC fs98_help_credits_accept(void *object) { fs98_help_answer(object, 0x1174, 0x1498, 1); }
void TC fs98_help_lessons_cancel(void *object) { fs98_help_answer(object, 0x10dc, 0x187c, 0); }
void TC fs98_help_completion_cancel(void *object) { fs98_help_answer(object, 0x10dc, 0, 0); }
void TC fs98_help_completion_accept(void *object) { fs98_help_answer(object, 0x10dc, 0, 1); }

void TC fs98_help_about_credits(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    ((fs98_help_about_fn)PTR(descriptor, 0x10))(descriptor);
}

#define HELP_CLASS(name, base) \
int32_t TC fs98_help_##name##_help(void *object, uint32_t unused, void *info) \
{ \
    (void)object; (void)unused; \
    return s_services->win_help(D32(info, 0xc), "fs98.hlp", 0xc, s_help_##name); \
} \
int32_t TC fs98_help_##name##_context(void *object, uint32_t window, uint32_t unused) \
{ \
    (void)object; (void)unused; \
    s_services->win_help(window, "fs98.hlp", 0xa, s_help_##name); \
    return 1; \
} \
void TC fs98_help_##name##_exchange(void *object, void *exchange) \
{ \
    uint32_t index; \
    if (base) s_services->base_exchange(object, exchange); \
    for (index = 0; index < sizeof(s_ddx_##name) / sizeof(s_ddx_##name[0]); ++index) \
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \
                                 AT(object, s_ddx_##name[index][0])); \
}
HELP_CLASS(about, 1)
HELP_CLASS(credits, 1)
HELP_CLASS(lessons, 1)
HELP_CLASS(graph, 0)
HELP_CLASS(completion, 0)
#undef HELP_CLASS

void TC fs98_help_disable(void *object) { s_services->enable_window(D32(object, 0x20), 0); }
void TC fs98_help_enable(void *object) { s_services->enable_window(D32(object, 0x20), 1); }
void TC fs98_help_empty(void *object) { (void)object; }
void TC fs98_help_empty_arg(void *object, uint32_t unused) { (void)object; (void)unused; }
void TC fs98_help_post_destroy(void *object)
{
    if (object) ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}

/* 0x22331ef0: clear each handle after its dependency returns. */
void TC fs98_help_credits_close(void *object)
{
    uint32_t handle = D32(object, 0x10e8);
    if (handle) s_services->kill_timer(D32(object, 0x20), handle);
    handle = D32(object, 0x10ec);
    D32(object, 0x10e8) = 0;
    if (handle) s_services->destroy_window(handle);
    handle = D32(object, 0x110c);
    D32(object, 0x10ec) = 0;
    if (handle) {
        s_services->free_library(handle);
        D32(object, 0x110c) = 0;
    }
    s_services->close(object);
}

/* 0x22331fe0: a failed ReadFile clears the byte count but STILL returns zero,
 * so RichEdit sees EOF rather than an EDITSTREAM callback error. */
uint32_t FS98_HELP_STDCALL fs98_help_credits_read(uint32_t cookie, void *buffer,
                                               int32_t count, uint32_t *read)
{
    if (!s_services->read_file(cookie, buffer, (uint32_t)count, read, 0)) *read = 0;
    return 0;
}

/* 0x22332010: the descriptor is deliberately replaced by embedded storage.
 * The synchronous EM_STREAMIN call owns the callback's file lifetime. */
void TC fs98_help_credits_load(void *object)
{
    uint8_t info[0x88];
    void *name = AT(object, 0x1124);
    int32_t file;
    PTR(object, 0x1174) = AT(object, 0x1110);
    s_services->copy_text(name, "fltcred.rtf");
    PTR(PTR(object, 0x1174), 0xc) = name;
    file = s_services->open_file(PTR(PTR(object, 0x1174), 0xc), info, 0);
    if (file == -1) {
        s_services->message_box("OpenFile failed", 0, 0);
        return;
    }
    D32(object, 0x10dc) = (uint32_t)file;
    D32(object, 0x10e0) = 0;
    PTR(object, 0x10e4) = fs98_help_credits_read;
    s_services->send_message(D32(object, 0x10ec), 0x435, 0, 0x7d00);
    s_services->send_message(D32(object, 0x10ec), 0x449, 2, (uint32_t)AT(object, 0x10dc));
    s_services->send_message(D32(object, 0x10ec), 0xcf, 1, 0);
    s_services->close_handle((uint32_t)file);
    D32(object, 0x10f0) = (uint32_t)s_services->send_message(D32(object, 0x10ec), 0xba, 0, 0) - 6;
    s_services->send_message(D32(object, 0x10ec), 0x443, 0, 0xffffff);
    fs98_help_credits_start_timer(object);
}

/* 0x22332130: timer ids 3..14, 500 ms, preserving every failed attempt. */
int32_t TC fs98_help_credits_start_timer(void *object)
{
    uint32_t timer;
    for (timer = 3; timer < 15; ++timer) {
        D32(object, 0x10e8) = s_services->set_timer(D32(object, 0x20), timer, 500, 0);
        if (D32(object, 0x10e8)) break;
    }
    if (!D32(object, 0x10e8)) s_services->message_box("SetTimer failed", 0, 0);
    return (int32_t)D32(object, 0x10e8);
}
void TC fs98_help_credits_timer(void *object, uint32_t timer)
{
    int32_t line;
    if (timer != D32(object, 0x10e8)) return;
    ++D32(object, 0x10f4);
    s_services->send_message(D32(object, 0x10ec), 0x115, 1, 0);
    line = s_services->send_message(D32(object, 0x10ec), 0xce, 0, 0);
    if (line > (int32_t)D32(object, 0x10f0)) {
        s_services->send_message(D32(object, 0x10ec), 0x115, 6, 0);
        D32(object, 0x10f4) = 0;
    }
}
void TC fs98_help_credits_toggle(void *object)
{
    if (D32(object, 0x1108)) {
        s_services->kill_timer(D32(object, 0x20), D32(object, 0x10e8));
        D32(object, 0x10e8) = 0;
    } else fs98_help_credits_start_timer(object);
    D32(object, 0x1108) ^= 1;
}

/* 0x22332790: the error helper at 0x2930 always returns zero, so its second
 * call is unreachable. Selection validity is tested before committing. */
void TC fs98_help_lessons_accept(void *object)
{
    if (s_services->send_message(D32(object, 0x151c), 0x188, 0, 0) == -1) {
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xd0))(object);
        return;
    }
    D32(object, 0x187c) = 1;
    fs98_help_lessons_sync(object);
    fs98_help_answer(object, 0x10dc, 0, 1);
}
void TC fs98_help_lessons_sync(void *object)
{
    int32_t selection = s_services->send_message(D32(object, 0x151c), 0x188, 0, 0);
    int32_t checked;
    uint32_t window;
    if (selection != -1) D32(PTR(object, 0x10dc), 0xc) = (uint32_t)selection;
    checked = s_services->get_window_long(D32(object, 0x17a8), -21);
    window = D32(object, 0x17a8);
    D32(PTR(object, 0x10dc), 0x14) = window == (uint32_t)checked;
}
int32_t TC fs98_help_lessons_no_error(void *object) { (void)object; return 0; }
int32_t TC fs98_help_lessons_validate(void *object) { return PTR(object, 0x10dc) != 0; }

void TC fs98_help_lessons_populate(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    uint32_t index;
    s_services->set_checked(AT(object, D32(descriptor, 0x14) ? 0x1788 : 0x1694), 1);
    s_services->send_message(D32(object, 0x151c), 0x184, 0, 0);
    index = 0;
    descriptor = PTR(object, 0x10dc);
    while (index < D32(descriptor, 8)) {
        s_services->send_message(D32(object, 0x151c), 0x180, 0,
                                (uint32_t)AT(PTR(descriptor, 0x10), index * (uint32_t)0xa08));
        ++index;
        descriptor = PTR(object, 0x10dc);
    }
    s_services->send_message(D32(object, 0x151c), 0x186, 0, 0);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0xc) < D32(descriptor, 8))
        s_services->send_message(D32(object, 0x151c), 0x186, D32(descriptor, 0xc), 0);
    fs98_help_lessons_select(object);
}

/* 0x22332950: the record is saved before the edit setter and host services,
 * so a replacement descriptor must not switch the chosen record mid-path. */
void TC fs98_help_lessons_select(void *object)
{
    char path[0x104], root[0x100], name[0x100];
    void *descriptor, *record;
    int32_t selection = s_services->send_message(D32(object, 0x151c), 0x188, 0, 0);
    int32_t enable;
    if (selection == -1) return;
    descriptor = PTR(object, 0x10dc);
    D32(descriptor, 0xc) = (uint32_t)selection;
    descriptor = PTR(object, 0x10dc);
    record = AT(PTR(descriptor, 0x10), D32(descriptor, 0xc) * (uint32_t)0xa08);
    s_services->set_text(AT(object, 0x162c), (char *)AT(record, 0x100));
    s_services->load_host_string(0x9007, root, 0x100);
    s_services->split_path((char *)AT(record, 0x900), 0, name, path, 0);
    s_services->join_path(root, 0, name, root);
    s_services->join_path(path, 0, root, path);
    enable = s_services->exists(path, 0x1d) == 0;
    s_services->enable_control(AT(object, 0x1788), enable);
    if (!enable) s_services->set_checked(AT(object, 0x1694), 1);
}
void TC fs98_help_lessons_double_click(void *object)
{
    fs98_help_lessons_select(object);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);
}

/* 0x22332c20/2de0: the graph callback's third argument is a saved HWND,
 * whereas the descriptor itself is reloaded immediately before the call. */
static void fs98_help_graph_call(void *object, uint32_t operation)
{
    void *descriptor = PTR(object, 0x10e0);
    uint32_t window;
    D32(object, 0x10e4) = operation;
    window = D32(object, 0x10dc);
    ((fs98_help_graph_fn)PTR(descriptor, 8))(operation, descriptor, window);
}
int32_t TC fs98_help_graph_tick(void *object, uint32_t w_param, uint32_t l_param)
{
    (void)w_param; (void)l_param;
    fs98_help_graph_call(object, 1);
    return 0;
}
void TC fs98_help_graph_notify(void *object) { fs98_help_graph_call(object, 0); }
void TC fs98_help_graph_accept(void *object)
{
    D32(object, 0x12cc) = 1;
    D32(PTR(object, 0x10e0), 0) = 1;
    fs98_help_graph_call(object, 2);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
int32_t TC fs98_help_graph_validate(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    if (descriptor && (uint32_t)descriptor != 0xcdcdcdcd) return 1;
    s_services->message_box("Bad m_pliveInfo\npassed to dialog by core code", 0, 0);
    return 0;
}

/* 0x22332ca0/2fa0: both ignore the base initialiser's return and each
 * control initialiser's return; only the virtual skin setup is an early exit. */
static int32_t fs98_help_skin(void *object)
{
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
        object, 0x14f, s_resource_module, 9);
}
static void fs98_help_button(void *object, uint32_t offset)
{
    struct fs98_help_rect rect = {0, 0, 0, 0};
    s_services->init_button(AT(object, offset), AT(object, 0x5a0), AT(object, 0x610),
                            0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                            s_resource_module, 1, 1, rect);
}
int32_t TC fs98_help_graph_init(void *object)
{
    void *control;
    struct fs98_help_rect rect;
    s_services->dialog_init(object);
    if (!fs98_help_skin(object)) return 0;
    fs98_help_button(object, 0x11d0);
    s_services->init_art(AT(object, 0x10e8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    if (!fs98_help_graph_validate(object)) return 0;
    control = s_services->get_control(object, 0xee2);
    s_services->remap_rect(&rect, control);
    s_services->move_control(control, rect.left, rect.top,
                             (int32_t)((uint32_t)rect.right - (uint32_t)rect.left),
                             (int32_t)((uint32_t)rect.bottom - (uint32_t)rect.top), 1);
    fs98_help_graph_notify(object);
    return 1;
}
int32_t TC fs98_help_completion_init(void *object)
{
    s_services->base_init(object);
    if (!fs98_help_skin(object)) return 0;
    fs98_help_button(object, 0x1198);
    fs98_help_button(object, 0x1294);
    s_services->init_text(AT(object, 0x10e0), AT(object, 0x220));
    return 1;
}

/* 0x22332570: font colors are changed only after the virtual skin succeeds.
 * The descriptor check is deliberately delayed until every control is ready. */
int32_t TC fs98_help_lessons_init(void *object)
{
    void *font;
    void *radio;
    s_services->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14b, s_resource_module, 7)) return 0;
    D32(object, 0x228) = 0;
    D32(object, 0x4c8) = 0;
    s_services->init_art(AT(object, 0x1544), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    fs98_help_button(object, 0x10e0);
    fs98_help_button(object, 0x11dc);
    s_services->init_group(AT(object, 0x1448), AT(object, 0x220));
    s_services->init_text(AT(object, 0x1390), AT(object, 0x370));
    s_services->init_list(AT(object, 0x14fc), AT(object, 0x450), AT(object, 0x1390));
    s_services->init_text(AT(object, 0x12d8), AT(object, 0x370));
    font = s_services->base_font(2);
    s_services->init_edit(AT(object, 0x162c), font, 0xffffff, 0, 0xff0000, AT(object, 0x12d8), 0);
    radio = AT(object, 0x1694);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(radio, 0), 0xc4))(
        radio, AT(object, 0x4c0), 0x134, AT(object, 0xff4), AT(object, 0x7e8), s_resource_module);
    radio = AT(object, 0x1788);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(radio, 0), 0xc4))(
        radio, AT(object, 0x4c0), 0x134, AT(object, 0xff4), AT(object, 0x7e8), s_resource_module);
    if (!fs98_help_lessons_validate(object)) return 0;
    s_services->send_message(D32(object, 0x164c), 0xc5, 0x3ff, 0);
    fs98_help_lessons_populate(object);
    return 1;
}

/* 0x22331d50: replace the template placeholder with a native RichEdit window.
 * LoadLibrary/CreateWindow failures are not checked before the stream load. */
int32_t TC fs98_help_credits_init(void *object)
{
    struct fs98_help_rect rect;
    uint32_t instance, window;
    void *placeholder;
    s_services->base_init(object);
    if (!fs98_help_skin(object)) return 0;
    s_services->init_art(AT(object, 0x13b0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    fs98_help_button(object, 0x12b4);
    fs98_help_button(object, 0x1178);
    s_services->copy_rect(&rect, (void *)AT(object, 0x10f8));
    placeholder = AT(object, 0x1274);
    s_services->remap_rect(&rect, placeholder);
    *(struct fs98_help_rect *)AT(object, 0x10f8) = rect;
    ((int32_t (TC *)(void *))PTR(PTR(placeholder, 0), 0x60))(placeholder);
    s_services->common_controls();
    D32(object, 0x110c) = s_services->load_library("RICHED32.DLL");
    instance = D32(s_services->module_state(), 8);
    window = s_services->create_window(0x200, "RICHEDIT", "", 0x503000c4,
        (int32_t)D32(object, 0x10f8), (int32_t)D32(object, 0x10fc),
        (int32_t)(D32(object, 0x1100) - D32(object, 0x10f8)),
        (int32_t)(D32(object, 0x1104) - D32(object, 0x10fc)),
        D32(object, 0x20), 0xede, instance, 0);
    D32(object, 0x10ec) = window;
    fs98_help_credits_load(object);
    return 1;
}

struct fs98_help_about_unwind {
    struct fs98_help_unwind frame;
    void *string;
    volatile uint32_t live;
};
static void fs98_help_about_cleanup(struct fs98_help_unwind *frame)
{
    struct fs98_help_about_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        s_services->string_destroy(state->string);
    }
}
/* 0x22331950: one unused CString still has observable construction/cleanup.
 * The successful RegOpen handle is never closed in retail. The first value
 * is Name (99 bytes), the second ProductID (49 bytes), despite the screen's
 * field order. The published descriptor pointer expires on return. */
FS98_HELP_NOINLINE void fs98_help_about_populate_body(void *object, uint8_t *scratch)
{
    struct fs98_help_about_unwind state;
    uint32_t *capacity = (void *)(scratch + 0xc), *key_out = (void *)(scratch + 0x10);
    char *version = (char *)scratch + 0x18, *build = (char *)scratch + 0x2c;
    char *product_id = (char *)scratch + 0x40, *prefix = (char *)scratch + 0x74;
    char *name = (char *)scratch + 0xa8, *copyright = (char *)scratch + 0x10c;
    state.string = scratch + 0x14; state.live = 0;
    fs98_help_unwind_enter(&state.frame, fs98_help_about_cleanup);
    *capacity = 0x63;
    s_services->string_construct(state.string);
    state.live = 1;
    if (s_services->reg_open(0x80000002,
            "Software\\Microsoft\\Microsoft Games\\Flight Simulator\\6.1", 0, 0x20019, key_out)) {
        s_services->copy_text(name, "Registry Error");
        s_services->copy_text(product_id, "Registry Error");
    } else {
        *capacity = 0x63;
        if (s_services->reg_query(*key_out, "Name", 0, 0, name, capacity))
            s_services->copy_text(name, "Failed Registry");
        *capacity = 0x31;
        if (s_services->reg_query(*key_out, "ProductID", 0, 0, product_id, capacity))
            s_services->copy_text(product_id, "Failed Registry");
    }
    PTR(PTR(object, 0x10dc), 8) = name;
    s_services->set_sims_text(AT(object, 0x1528), name);
    s_services->set_sims_text(AT(object, 0x15e0), product_id);
    s_services->load_string(D32(object, 0x80), 0xf70b, prefix, 0x1d);
    s_services->load_string(D32(object, 0x80), 0xf6fd, build, 0x13);
    s_services->format_version(version, "%d.%02d      %s%d", 6, 10, build, 0x1a2);
    s_services->append_text(prefix, version);
    s_services->set_sims_text(AT(object, 0x1198), prefix);
    s_services->load_string(D32(object, 0x80), 0xf70c, copyright, 0x1ff);
    s_services->set_sims_text(AT(object, 0x10e0), copyright);
    fs98_help_about_cleanup(&state.frame);
    fs98_help_unwind_leave(&state.frame);
}

static struct fs98_help_about_state s_default_about_state;
static struct fs98_help_about_state *s_about_state = &s_default_about_state;
void fs98_help_set_about_state(struct fs98_help_about_state *state) { s_about_state = state; }
void fs98_help_about_font_destroy(void) { s_services->font_destroy(s_about_state->font); }
/* 0x22331710: the initialized bit is committed before the base-font and
 * font-constructor calls. A throw does not clear it or schedule a destructor. */
int32_t fs98_help_about_init_body(void *object, uint8_t *scratch)
{
    s_services->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x13a, s_resource_module, 1)) return 0;
    if (!(s_about_state->initialized & 1)) {
        void *base_font;
        s_about_state->initialized |= 1;
        base_font = s_services->base_font(11);
        s_services->font_construct(s_about_state->font, base_font);
        s_services->register_exit(fs98_help_about_font_destroy);
    }
    D32(s_about_state->font, 8) = 0xffffff;
    D32(s_about_state->font, 0xc) = 0;
    D32(s_about_state->font, 0x50) = 0;
    D32(s_about_state->font, 0x54) = 1;
    s_services->init_art(AT(object, 0x1890), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    fs98_help_button(object, 0x1698);
    fs98_help_button(object, 0x1794);
    s_services->init_text(AT(object, 0x10e0), s_about_state->font);
    s_services->init_text(AT(object, 0x1198), AT(object, 0x220));
    s_services->init_text(AT(object, 0x1250), AT(object, 0x220));
    s_services->init_text(AT(object, 0x1308), AT(object, 0x220));
    s_services->init_group(AT(object, 0x13c0), AT(object, 0x220));
    s_services->init_group(AT(object, 0x1474), AT(object, 0x220));
    s_services->init_text(AT(object, 0x1528), AT(object, 0x220));
    s_services->init_text(AT(object, 0x15e0), AT(object, 0x220));
    fs98_help_about_populate_body(object, scratch);
    return 1;
}
