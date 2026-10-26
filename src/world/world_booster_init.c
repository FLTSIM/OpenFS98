/* World 0x223f68b0: booster editor setup, shared font and CString lifetime. */
#include "world_booster.h"
#include "world_unwind.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
static const struct fs98_world_booster_init_services *s_services;
static struct fs98_world_booster_font_state s_font_state;
void fs98_world_booster_set_init_services(const struct fs98_world_booster_init_services *services)
{ s_services = services; }
struct fs98_world_booster_font_state *fs98_world_booster_get_font_state(void) { return &s_font_state; }
/* 0x223f6d80, registered only after successful font construction. */
void fs98_world_booster_font_cleanup(void) { s_services->font_destroy(s_font_state.font); }
struct booster_string_unwind { struct fs98_world_unwind frame; void *string; volatile int live; };
static void booster_string_cleanup(struct fs98_world_unwind *frame)
{
    struct booster_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->ui->string_destroy(state->string); }
}
WORLD_NOINLINE int fs98_world_booster_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    const struct fs98_world_services *events = s_services->events;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    struct booster_string_unwind state;
    uint32_t string = 0, index;
    void *font, *row;
    void **vtable;
    static const uint32_t s_controls[] = {0x1b28, 0x16d8, 0x1c60, 0x1568, 0x1ca8, 0x1900, 0x1b90};
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    U32(object, 0x84) = 12;
    row = PTR(PTR(object, 0x10dc), 0xc);
    if (!*(char *)PTR(row, 4)) {
        state.string = &string; state.live = 0;
        world_unwind_enter(&state.frame, booster_string_cleanup);
        ui->string_construct(&string); state.live = 1;
        {
            char *buffer = ui->string_buffer(&string, 0x100);
            ui->load_string(U32(object, 0x80), 0xf8d4, buffer, 0x100);
        }
        ui->string_release(&string, -1);
        events->set_text(AT(object, 0x1480), (char *)string);
        U32(object, 0x10e8) = 1;
        state.live = 0; ui->string_destroy(&string);
        world_unwind_leave(&state.frame);
    } else U32(object, 0x10e8) = 0;
    if (!(s_font_state.flags & 1)) {
        s_font_state.flags |= 1;
        font = ui->base_font(8);
        s_services->font_construct(s_font_state.font, font);
        s_services->at_exit(fs98_world_booster_font_cleanup);
    }
    U32(s_font_state.font, 8) = U32(object, 0x228);
    U32(s_font_state.font, 12) = U32(object, 0x22c);
    U32(s_font_state.font, 0x50) = U32(object, 0x270);
    *(uint8_t *)AT(s_font_state.font, 0x58) |= 0x10;
    U32(s_font_state.font, 0x54) = U32(object, 0x274);
    ui->art(AT(object, 0x1480), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    ui->button(AT(object, 0x1d70), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    ui->button(AT(object, 0x1e6c), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    ui->text(AT(object, 0x19b8), AT(object, 0x370));
    font = ui->base_font(2);
    ui->edit(AT(object, 0x1bf8), font, 0xffffff, 0, 0xff0000, AT(object, 0x19b8), 0);
    ui->text(AT(object, 0x10ec), AT(object, 0x220));
    ui->text(AT(object, 0x13c8), AT(object, 0x220));
    ui->text(AT(object, 0x1900), AT(object, 0x220));
    font = ui->base_font(2);
    ui->edit(AT(object, 0x1b90), font, 0xffffff, 0, 0xff0000, AT(object, 0x1900), 0);
    ui->text(AT(object, 0x16d8), AT(object, 0x370));
    font = ui->base_font(2);
    ui->edit(AT(object, 0x1b28), font, 0xffffff, 0, 0xff0000, AT(object, 0x16d8), 0);
    s_services->group(AT(object, 0x11a4), AT(object, 0x220));
    ui->text(AT(object, 0x1258), s_font_state.font);
    ui->text(AT(object, 0x1568), AT(object, 0x370));
    ui->combo(AT(object, 0x1c60), AT(object, 0x3e0), AT(object, 0x1568));
    events->send(U32(object, 0x1c80), 0x14e, 0, 0);
    ui->text(AT(object, 0x1a70), AT(object, 0x220));
    ui->text(AT(object, 0x1848), AT(object, 0x220));
    ui->text(AT(object, 0x1310), AT(object, 0x220));
    ui->text(AT(object, 0x1790), AT(object, 0x220));
    ui->text(AT(object, 0x1620), AT(object, 0x220));
    ui->title(AT(object, 0x1620), PTR(PTR(object, 0x10dc), 8));
    vtable = PTR(object, 0x1ca8);
    ((int (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(
        AT(object, 0x1ca8), AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_world_get_resource_module());
    row = PTR(PTR(object, 0x10dc), 0xc);
    events->send(U32(object, 0x1b48), 0xc5, U32(row, 0x1c) - 1, 0);
    row = PTR(PTR(object, 0x10dc), 0xc);
    events->send(U32(object, 0x1c18), 0xc5, U32(row, 8) - 1, 0);
    events->send(U32(object, 0x1bb0), 0xc5, 7, 0);
    if (U32(object, 0x10e8)) {
        fs98_world_booster_populate(object);
        for (index = 0; index < 7; ++index) events->enable(AT(object, s_controls[index]), 0);
    } else {
        for (index = 0; index < 7; ++index) events->enable(AT(object, s_controls[index]), index < 5);
        fs98_world_booster_populate(object);
    }
    PTR(object, 0x10e0) = AT(object, 0x1e6c);
    PTR(object, 0x10e4) = AT(object, 0x1d70);
    return 1;
}
