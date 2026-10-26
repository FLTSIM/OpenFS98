/* World 0x13080002 core. Retail preferred VAs identify recovered routines.
 * No factories or native registration until the remaining class is accepted. */
#include "world_search.h"
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define AT(ptr, offset) ((uint8_t *)(ptr) + (offset))
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_search_services *s_services;
void fs98_world_search_set_services(const struct fs98_world_search_services *services)
{ s_services = services; }
static void search_callback(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, offset))(descriptor);
}
/* Shared prefix at 0x223fe210/270/2d0/320/370/3c0. Both descriptor
 * loads stay fresh; reading text can invoke owner/UI code. */
static void search_text(void *object)
{
    s_services->ui->get_text(AT(object, 0x1304), PTR(PTR(object, 0x10dc), 0x38), 0x80);
    s_services->ui->get_text(AT(object, 0x16c0), PTR(PTR(object, 0x10dc), 0x24), 0x80);
}
/* 0x223fe2d0: preserve the raw checkbox state, including noncanonical bits. */
void fs98_world_search_collect(void *object)
{
    search_text(object);
    U32(PTR(object, 0x10dc), 0x54) = U32(object, 0x1c70);
}
/* 0x223fe560 */
void fs98_world_search_populate(void *object)
{
    const struct fs98_world_services *ui = s_services->ui;
    uint32_t layer;
    int32_t index = 0;
    int enabled;
    void *descriptor;
    s_services->title(AT(object, 0x16c0), PTR(PTR(object, 0x10dc), 0x24));
    ui->set_text(AT(object, 0x1304), PTR(PTR(object, 0x10dc), 0x38));
    layer = U32(PTR(object, 0x10dc), 0x4c);
    if (layer >= 10 && layer <= 50 && layer % 10 == 0)
        ui->send(U32(object, 0x1850), 0x14e, layer / 10 - 1, 0);
    ui->check(AT(object, 0x1bb4), (int)U32(PTR(object, 0x10dc), 0x54));
    ui->send(U32(object, 0x1898), 0x14b, 0, 0);
    while (index < (int32_t)U32(PTR(object, 0x10dc), 0x68)) {
        descriptor = PTR(object, 0x10dc);
        ui->send(U32(object, 0x1898), 0x143, 0,
            (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x70), (uint32_t)index * 4));
        ++index;
    }
    ui->send(U32(object, 0x1898), 0x14e, U32(PTR(object, 0x10dc), 0x6c), 0);
    ui->enable(AT(object, 0x1c7c), (int)U32(PTR(object, 0x10dc), 0x74));
    ui->enable(AT(object, 0x1d78), (int)U32(PTR(object, 0x10dc), 0x78));
    ui->enable(AT(object, 0x1ab8), (int)U32(PTR(object, 0x10dc), 0x7c));
    ui->enable(AT(object, 0x19bc), (int)U32(PTR(object, 0x10dc), 0x80));
    descriptor = PTR(object, 0x10dc);
    enabled = U32(descriptor, 0x78) != 0 && U32(descriptor, 0x74) != 0;
    ui->enable(AT(object, 0x124c), enabled);
    ui->enable(AT(object, 0x1304), enabled);
}
/* 0x223fe760. The call to 0x223f99f0 is a bare ret, omitted. */
int fs98_world_search_validate(void *object)
{
    void *descriptor;
    uint32_t status, control;
    search_callback(object, 4);
    descriptor = PTR(object, 0x10dc);
    status = U32(descriptor, 0x20); control = 0x16c0;
    if (!status) { status = U32(descriptor, 0x34); control = 0x1304; }
    if (!status) { status = U32(descriptor, 0x48); control = 0x1830; }
    if (!status) return 1;
    s_services->ui->error(object, AT(object, control), (void *)(uintptr_t)status);
    return 0;
}
/* 0x223fe270 / 0x223fe210; validation's result is ignored. */
void fs98_world_search_add(void *object)
{
    search_text(object);
    search_callback(object, 8);
    fs98_world_search_validate(object);
    fs98_world_search_populate(object);
}
void fs98_world_search_add_all(void *object)
{
    search_text(object);
    search_callback(object, 0xc);
    fs98_world_search_validate(object);
    fs98_world_search_populate(object);
}
/* 0x223fe320 / 0x223fe370 */
void fs98_world_search_next(void *object)
{
    search_text(object);
    search_callback(object, 0x14);
    fs98_world_search_populate(object);
}
void fs98_world_search_next_dir(void *object)
{
    search_text(object);
    search_callback(object, 0x18);
    fs98_world_search_populate(object);
}
/* 0x223fe3c0. Search posts the edit's current HWND after population. */
void fs98_world_search_scan(void *object)
{
    search_text(object);
    search_callback(object, 0x10);
    fs98_world_search_populate(object);
    s_services->post(U32(object, 0x20), 0x5fa, U32(object, 0x1324), 0);
}
/* 0x223fe430: reload descriptor after CB_GETCURSEL. */
void fs98_world_search_drive(void *object)
{
    int selected = s_services->ui->send(U32(object, 0x1898), 0x147, 0, 0);
    U32(PTR(object, 0x10dc), 0x6c) = (uint32_t)selected;
}
/* 0x223fe470; 0x223fe460 jumps here. The row is cached before SendMessage. */
void fs98_world_search_layer(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    uint32_t selected = (uint32_t)s_services->ui->send(U32(object, 0x1850), 0x147, 0, 0);
    if (selected <= 4) U32(descriptor, 0x4c) = (selected + 1) * 10;
}
/* 0x223fe4e0 and 0x223fe500 share observable behavior; the latter calls
 * the bare-ret 0x223f99f0 between the answer store and vtable load. */
void fs98_world_search_cancel(void *object)
{
    void *vtable;
    U32(PTR(object, 0x10dc), 0) = 0;
    vtable = PTR(object, 0);
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
