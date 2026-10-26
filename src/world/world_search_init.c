/* World 0x223fdec0: drive-search control setup, with no owned temporaries. */
#include "world_search.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
static const struct fs98_world_search_init_services *s_services;
void fs98_world_search_set_init_services(const struct fs98_world_search_init_services *services)
{ s_services = services; }
int fs98_world_search_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    const struct fs98_world_services *events = s_services->events;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    static const uint32_t s_buttons[] = {0x1d78, 0x1c7c, 0x1ab8, 0x19bc, 0x18c0};
    uint32_t index;
    void **vtable;
    void *font;
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    ui->art(AT(object, 0x15d8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    ui->button(AT(object, 0x136c), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    for (index = 0; index < sizeof(s_buttons) / sizeof(s_buttons[0]); ++index)
        ui->button(AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), 0x137,
            AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    ui->text(AT(object, 0x1778), AT(object, 0x220));
    ui->combo(AT(object, 0x1878), AT(object, 0x3e0), AT(object, 0x1778));
    events->send(U32(object, 0x1898), 0x14e, 0, 0);
    ui->text(AT(object, 0x1468), AT(object, 0x370));
    ui->combo(AT(object, 0x1830), AT(object, 0x3e0), AT(object, 0x1468));
    events->send(U32(object, 0x1850), 0x14e, 0, 0);
    ui->text(AT(object, 0x10e0), AT(object, 0x220));
    ui->text(AT(object, 0x1520), AT(object, 0x220));
    /* CSimsCheckButton::Init at SIMSUI 0x20473c20, vtable +0xc4:
     * thiscall(font, bitmap, dib, palette, module), callee ret 0x14. */
    vtable = PTR(object, 0x1bb4);
    ((int (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(
        AT(object, 0x1bb4), AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_world_get_resource_module());
    ui->text(AT(object, 0x16c0), AT(object, 0x220));
    s_services->group(AT(object, 0x1198), AT(object, 0x220));
    ui->text(AT(object, 0x124c), AT(object, 0x220));
    font = ui->base_font(2);
    ui->edit(AT(object, 0x1304), font, 0xffffff, 0, 0xff0000, AT(object, 0x124c), 0);
    events->send(U32(object, 0x1324), 0xc5, U32(PTR(object, 0x10dc), 0x3c) - 1, 0);
    events->enable(AT(object, 0x124c), 0);
    events->enable(AT(object, 0x1304), 0);
    ui->title(AT(object, 0x1520), PTR(PTR(object, 0x10dc), 0x1c));
    fs98_world_search_populate(object);
    return 1;
}
