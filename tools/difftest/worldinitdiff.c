/* Reuse World's guarded event fixture, add the initializer's import boundary. */
#define main world_event_main
#include "worlddiff.c"
#undef main
#include "../../src/world/world_init.h"
#include "../../src/world/world_unwind.h"
static uint32_t s_init_seh, s_string_live, s_string_reference;
static char s_string[256];
static void *s_check_vtable[60];
static int world_init_base(void *object) { world_log("base.init %x\n", world_offset(object)); return -1; }
static int TC world_init_base_hook(void *object) { return world_init_base(object); }
static int TC world_init_skin(void *object, uint32_t bitmap, uint32_t module, uint32_t flags)
{ world_log("skin %x bitmap=%x module=%x flags=%x\n", world_offset(object), bitmap, module, flags); return s_scenario % 5 ? 1 : 0; }
static void world_init_string_construct(void *string)
{ world_log("string.construct\n"); PTR(string, 0) = s_string; s_string[0] = 0; ++s_string_live; }
static void TC world_init_string_construct_hook(void *string) { world_init_string_construct(string); }
static char *world_init_buffer(void *string, int minimum)
{ (void)string; world_log("string.buffer %d\n", minimum); return s_string; }
static char *TC world_init_buffer_hook(void *string, int minimum) { return world_init_buffer(string, minimum); }
static void world_init_release(void *string, int length)
{ (void)string; world_log("string.release %d\n", length); }
static void TC world_init_release_hook(void *string, int length) { world_init_release(string, length); }
static void world_init_string_destroy(void *string)
{ (void)string; world_log("string.destroy\n"); --s_string_live; }
static void TC world_init_string_destroy_hook(void *string) { world_init_string_destroy(string); }
static int SC world_init_load(uint32_t module, uint32_t id, char *buffer, int capacity)
{ world_log("load module=%x id=%x capacity=%d\n", module, id, capacity); strcpy(buffer, "Fixture empty-path label"); return 24; }
static int world_init_load_service(uint32_t module, uint32_t id, char *buffer, int capacity) { return world_init_load(module, id, buffer, capacity); }
static int world_init_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ world_log("art %x %x %x %x %x\n", world_offset(control), bitmap, world_offset(dib), world_offset(palette), flags); return 1; }
static int TC world_init_art_hook(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return world_init_art(control, bitmap, dib, palette, flags); }
static int world_init_button(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{
    world_log("button %x font=%x/%x bitmap=%x dib=%x palette=%x flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        world_offset(control), world_offset(font), world_offset(alternate), bitmap, world_offset(dib), world_offset(palette), flags,
        (uint32_t)color, module, x, y, rect.left, rect.top, rect.right, rect.bottom); return 1;
}
static int TC world_init_button_hook(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{ return world_init_button(control, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void world_init_text(void *control, void *font) { world_log("init.text %x %x\n", world_offset(control), world_offset(font)); }
static void TC world_init_text_hook(void *control, void *font) { world_init_text(control, font); }
static void world_init_combo(void *control, void *font, void *label) { world_log("init.combo %x %x %x\n", world_offset(control), world_offset(font), world_offset(label)); }
static void TC world_init_combo_hook(void *control, void *font, void *label) { world_init_combo(control, font, label); }
static void *SC world_init_font(int font) { world_log("font %d\n", font); return s_fixture.after; }
static void *world_init_font_service(int font) { return world_init_font(font); }
static void world_init_edit(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags)
{ world_log("init.edit %x font=%x colors=%x/%x/%x label=%x flags=%x\n", world_offset(control), world_offset(font), foreground, background, error, world_offset(label), flags); }
static void TC world_init_edit_hook(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags)
{ world_init_edit(control, font, foreground, background, error, label, flags); }
static int TC world_init_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{ world_log("init.check %x font=%x bitmap=%x dib=%x palette=%x module=%x\n", world_offset(control), world_offset(font), bitmap, world_offset(dib), world_offset(palette), module); return 1; }
static int world_init_enable(void *control, int enabled)
{
    int result = world_enable(control, enabled);
    if ((uint8_t *)control == s_fixture.object + 0x11a0 && (s_scenario & 8))
        U32(s_fixture.object, 0x10e0) ^= 1;
    return result;
}
static int TC world_init_enable_hook(void *control, int enabled) { return world_init_enable(control, enabled); }
static struct fs98_world_services s_init_event_services;
static const struct fs98_world_init_services s_init_services = {
    world_init_base, world_init_string_construct, world_init_buffer, world_init_release, world_init_string_destroy,
    world_init_load_service, world_init_art, world_init_button, world_init_text, world_init_combo, world_init_font_service, world_init_edit, world_set_text
};
static void world_init_land(void)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_seh) : "memory"); __builtin_longjmp(s_jump, 1); }
static EXCEPTION_DISPOSITION __cdecl world_init_catch(EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == THROW_CODE && !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, NULL, record, NULL); s_throw_at = 0; world_log("caught\n");
        if (!s_side) ++s_throws;
        world_init_land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) void world_init_run(uint32_t throw_at)
{
    struct { void *previous, *handler; } catcher;
    int result;
    s_fixture = s_initial; s_string_live = 0; s_string[0] = 0;
    s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_init_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_init_seh; catcher.handler = world_init_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        result = s_side ? fs98_world_area_init(s_fixture.object) :
            ((int (TC *)(void *))((uint8_t *)s_retail + 0xbef0))(s_fixture.object);
        world_log("return %d\n", result);
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_seh) : "memory");
}
static int world_init_compare(uint32_t throw_at)
{
    s_side = 0; world_init_run(throw_at); s_reference = s_fixture; s_string_reference = s_string_live;
    s_side = 1; world_init_run(throw_at); ++s_comparisons;
    if (s_string_reference == s_string_live && !memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "WORLD INIT DIFFER scenario=%u throw=%u live=%u/%u\nRETAIL:\n%sOURS:\n%s", s_scenario, throw_at, s_string_reference, s_string_live, s_logs[0], s_logs[1]); return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000, index, limit;
    int unwind = argc > 2 && !strcmp(argv[2], "unwind");
    s_rng = 0x4a971b63;
    SetErrorMode(SEM_NOGPFAULTERRORBOX); s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
    world_patch(0x255bc, world_hook_set); world_patch(0x255c8, world_init_enable_hook); world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send); world_patch(0x256b8, world_hook_format);
    world_patch(0x257c0, world_init_base_hook); world_patch(0x255b4, world_init_string_construct_hook);
    world_patch(0x25588, world_init_buffer_hook); world_patch(0x2558c, world_init_release_hook); world_patch(0x255c0, world_init_string_destroy_hook);
    world_patch(0x25890, world_init_load); world_patch(0x257c4, world_init_art_hook); world_patch(0x257c8, world_init_button_hook);
    world_patch(0x257dc, world_init_text_hook); world_patch(0x25790, world_init_combo_hook); world_patch(0x257d0, world_init_font);
    world_patch(0x25740, world_init_edit_hook); world_patch(0x257e0, world_hook_set);
    world_patch(0x256bc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    s_vtable[0xe4 / 4] = world_init_skin; s_check_vtable[0xc4 / 4] = world_init_check;
    s_init_event_services = s_services; s_init_event_services.enable = world_init_enable;
    fs98_world_set_services(&s_init_event_services); fs98_world_set_init_services(&s_init_services);
    U32(s_retail, 0x2495c) = 0x27182818; fs98_world_set_resource_module(0x27182818);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        world_initialize(); s_operation = 7;
        for (index = 0; index < 2; ++index) {
            if (s_scenario & 4) s_fixture.paths[index][0] = 0;
            PTR(s_fixture.descriptors[index], 8) = "Fixture title";
            U32(s_fixture.rows[index], 8) = 160; U32(s_fixture.rows[index], 0x1c) = 160;
        }
        U32(s_fixture.object, 0x10e0) = 1;
        PTR(s_fixture.object, 0x1a50) = s_check_vtable;
        s_initial = s_fixture;
        if (world_init_compare(0)) return 1;
        limit = s_calls - 1;
        if (unwind) for (index = 1; index <= limit; ++index) if (world_init_compare(index)) return 1;
    }
    printf("World initializer: %u scenarios, %u comparisons, %u real unwinds, 0 differ\n", count, s_comparisons, s_throws);
    return 0;
}
