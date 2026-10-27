/* 0x223f3b20: copy-weather skin and thirteen controls. No CNotify. */
#include "world_copy.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
static const struct fs98_world_copy_init_services *s_services;
void fs98_world_copy_set_init_services(const struct fs98_world_copy_init_services *services)
{ s_services = services; }
int fs98_world_copy_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    void **vtable;
    uint32_t index;
    static const uint32_t s_checks[] = {0x15c0, 0x1688, 0x1750, 0x1818, 0x18e0, 0x19a8};
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, s_services->resources(), 15)) return 0;
    ui->art(AT(object, 0x14d8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    ui->button(AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), 0, 0, rect);
    ui->button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), 0, 0, rect);
    ui->text(AT(object, 0x1390), AT(object, 0x370));
    ui->combo(AT(object, 0x1490), AT(object, 0x3e0), AT(object, 0x1390));
    ui->text(AT(object, 0x12d8), AT(object, 0x370));
    ui->combo(AT(object, 0x1448), AT(object, 0x3e0), AT(object, 0x12d8));
    for (index = 0; index < 6; ++index) {
        void *control = AT(object, s_checks[index]);
        vtable = PTR(control, 0);
        ((int (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(
            control, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), s_services->resources());
    }
    if (!fs98_world_copy_has_descriptor(object)) return 0;
    fs98_world_copy_populate(object);
    return 1;
}
