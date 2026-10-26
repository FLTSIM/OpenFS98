/* World.DLL shared add/edit performance-booster events. Retail x86 offsets;
 * descriptor and row loads deliberately follow retail's callback boundaries. */
#include "world_booster.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_booster_services *s_services;
void fs98_world_booster_set_services(const struct fs98_world_booster_services *services)
{ s_services = services; }
static void *booster_row(void *object)
{ return PTR(PTR(object, 0x10dc), 0xc); }
static void booster_check(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, 4))(descriptor);
}
/* 0x223f6dd0 */
void fs98_world_booster_collect(void *object)
{
    const struct fs98_world_services *ui = s_services->ui;
    char text[32];
    int value;
    ui->get_text(AT(object, 0x1b28), PTR(booster_row(object), 0x18), 0x104);
    ui->get_text(AT(object, 0x1bf8), PTR(booster_row(object), 4), 0x104);
    ui->get_text(AT(object, 0x1b90), text, 0x1f);
    value = ui->parse_int(text);
    U32(booster_row(object), 0x5c) = (uint32_t)value;
    value = ui->send(U32(object, 0x1c80), 0x147, 0, 0);
    U32(booster_row(object), 0x7c) = value == 1;
    U32(booster_row(object), 0x54) = U32(object, 0x1d64) != 0;
}
/* 0x223f6ea0: the BSTINFO recommended size is an unsigned WORD, not the entire slot. */
void fs98_world_booster_populate(void *object)
{
    const struct fs98_world_services *ui = s_services->ui;
    char original[32], text[32];
    ui->format_int(original, "%5d", *(uint16_t *)AT(booster_row(object), 0x74));
    ui->format_int(text, "%5d", (int)U32(booster_row(object), 0x5c));
    s_services->title(AT(object, 0x1a70), PTR(booster_row(object), 0x2c));
    s_services->title(AT(object, 0x1258), PTR(booster_row(object), 0x40));
    ui->set_text(AT(object, 0x1b28), PTR(booster_row(object), 0x18));
    s_services->title(AT(object, 0x1790), original);
    ui->set_text(AT(object, 0x1b90), text);
    ui->send(U32(object, 0x1c80), 0x14e, U32(booster_row(object), 0x7c) == 1, 0);
    ui->check(AT(object, 0x1ca8), U32(booster_row(object), 0x54) == 1);
    ui->set_text(AT(object, 0x1bf8), PTR(booster_row(object), 4));
}
/* 0x223f6740: an error service returning zero permits acceptance immediately,
 * without checking later statuses. Only nonzero results focus and keep open. */
void fs98_world_booster_accept(void *object)
{
    const struct fs98_world_services *ui = s_services->ui;
    void *row;
    uint32_t status, control = 0;
    char text[32];
    void *descriptor;
    void **vtable;
    fs98_world_booster_collect(object);
    booster_check(object);
    descriptor = PTR(object, 0x10dc);
    row = PTR(descriptor, 0xc);
    status = U32(row, 0);
    if (status) control = 0x1bf8;
    else if ((status = U32(row, 0x14))) control = 0x1b28;
    else if ((status = U32(row, 0x78))) control = 0x1c60;
    else if ((status = U32(row, 0x50))) control = 0x1ca8;
    else if (U32(row, 0x58)) {
        int value;
        row = PTR(descriptor, 0xc);
        U32(row, 0x60) = U32(row, 0x58);
        ui->get_text(AT(object, 0x1b90), text, 0x20);
        PTR(booster_row(object), 0x64) = text;
        value = ui->parse_int(PTR(booster_row(object), 0x70));
        U32(booster_row(object), 0x68) = (uint32_t)value;
        if (ui->error(object, AT(object, 0x1b90), AT(booster_row(object), 0x60))) {
            ui->focus_control(AT(object, 0x1b90));
            return;
        }
    }
    if (control && ui->error(object, AT(object, control), (void *)status)) {
        ui->focus_control(AT(object, control));
        return;
    }
    descriptor = PTR(object, 0x10dc);
    vtable = PTR(object, 0);
    U32(descriptor, 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223f6fd0: path notification uses capacity 0x80, unlike collection. The
 * enable decision is taken once; later label/layer loads are fresh. */
void fs98_world_booster_path_changed(void *object)
{
    static const uint32_t s_controls[] = {0x1b28, 0x16d8, 0x1c60, 0x1568, 0x1ca8, 0x1900, 0x1b90};
    const struct fs98_world_services *ui = s_services->ui;
    char text[0x104];
    uint32_t index;
    int enabled;
    ui->get_text(AT(object, 0x1bf8), PTR(booster_row(object), 4), 0x80);
    booster_check(object);
    enabled = U32(booster_row(object), 0) == 0;
    for (index = 0; index < sizeof(s_controls) / sizeof(s_controls[0]); ++index)
        ui->enable(AT(object, s_controls[index]), enabled);
    s_services->title(AT(object, 0x1258), PTR(booster_row(object), 0x40));
    s_services->title(AT(object, 0x1a70), PTR(booster_row(object), 0x2c));
    ui->format_int(text, "%5d", *(uint16_t *)AT(booster_row(object), 0x74));
    s_services->title(AT(object, 0x1790), text);
    ui->format_int(text, "%5d", (int)U32(booster_row(object), 0x5c));
    ui->set_text(AT(object, 0x1b90), text);
}
/* 0x223f7160: only the cache-size status belongs to virtual validation. */
int fs98_world_booster_validate(void *object, uint32_t unused)
{
    (void)unused;
    fs98_world_booster_collect(object);
    booster_check(object);
    return U32(booster_row(object), 0x58) ? 0x1ae9 : 0;
}
/* 0x223f71a0: the temporary text pointer is left at row +0x64 after return. */
int fs98_world_booster_error(void *object, uint32_t unused)
{
    const struct fs98_world_services *ui = s_services->ui;
    char text[32];
    void *row = booster_row(object);
    uint32_t control = 0;
    void **vtable;
    (void)unused;
    if (U32(row, 0x58)) {
        int value;
        U32(row, 0x60) = U32(row, 0x58);
        ui->get_text(AT(object, 0x1b90), text, 0x20);
        PTR(booster_row(object), 0x64) = text;
        value = ui->parse_int(PTR(booster_row(object), 0x70));
        U32(booster_row(object), 0x68) = (uint32_t)value;
        if (ui->error(object, AT(object, 0x1b90), AT(booster_row(object), 0x60))) control = 0x1ae9;
    }
    vtable = PTR(object, 0);
    return ((int (TC *)(void *, uint32_t))vtable[0xe0 / 4])(object, control);
}
