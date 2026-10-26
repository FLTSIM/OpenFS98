/* Drive-search initialization against mapped retail and guarded UI fixtures. */
#define main world_search_event_main
#include "worldsearchdiff.c"
#undef main
static void *s_check_vtable[60], *s_other_check_vtable[60];
static uint32_t s_resource_module, s_resource_reference;
static void world_init_tick(void)
{
    if (s_scenario & 32) s_resource_module ^= 0x31415927;
    if (s_side) fs98_world_set_resource_module(s_resource_module);
    else U32(s_retail, 0x2495c) = s_resource_module;
}
static int world_init_base(void *object)
{ world_log("base.init %x\n", world_offset(object)); world_init_tick(); return -1; }
static int TC world_init_base_hook(void *object) { return world_init_base(object); }
static int TC world_init_skin(void *object, uint32_t bitmap, uint32_t module, uint32_t flags)
{ world_log("skin %x bitmap=%x module=%x flags=%x\n", world_offset(object), bitmap, module, flags); world_init_tick(); return s_scenario % 5 ? 1 : 0; }
static int TC world_init_other_skin(void *object, uint32_t bitmap, uint32_t module, uint32_t flags)
{ world_log("other.skin %x bitmap=%x module=%x flags=%x\n", world_offset(object), bitmap, module, flags); world_init_tick(); return s_scenario % 5 ? 1 : 0; }
static int world_init_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ world_log("art %x %x %x %x %x\n", world_offset(control), bitmap, world_offset(dib), world_offset(palette), flags); world_init_tick(); return 1; }
static int TC world_init_art_hook(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return world_init_art(control, bitmap, dib, palette, flags); }
static int world_init_button(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{
    world_log("button %x font=%x/%x bitmap=%x dib=%x palette=%x flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        world_offset(control), world_offset(font), world_offset(alternate), bitmap, world_offset(dib), world_offset(palette), flags,
        (uint32_t)color, module, x, y, rect.left, rect.top, rect.right, rect.bottom); world_init_tick(); return 1;
}
static int TC world_init_button_hook(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{ return world_init_button(control, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void world_init_text(void *control, void *font)
{ world_log("init.text %x %x\n", world_offset(control), world_offset(font)); world_init_tick(); }
static void TC world_init_text_hook(void *control, void *font) { world_init_text(control, font); }
static void world_init_combo(void *control, void *font, void *label) { world_log("init.combo %x %x %x\n", world_offset(control), world_offset(font), world_offset(label)); world_init_tick(); }
static void TC world_init_combo_hook(void *control, void *font, void *label) { world_init_combo(control, font, label); }
static void *SC world_init_font(int font) { world_log("font %d\n", font); world_init_tick(); return s_fixture.after; }
static void *world_init_font_service(int font) { return world_init_font(font); }
static void world_init_edit(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags)
{ world_log("init.edit %x font=%x colors=%x/%x/%x label=%x flags=%x\n", world_offset(control), world_offset(font), foreground, background, error, world_offset(label), flags); world_init_tick(); }
static void TC world_init_edit_hook(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags)
{ world_init_edit(control, font, foreground, background, error, label, flags); }
static int TC world_init_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{ world_log("init.check %x font=%x bitmap=%x dib=%x palette=%x module=%x\n", world_offset(control), world_offset(font), bitmap, world_offset(dib), world_offset(palette), module); world_init_tick(); return 1; }

static void world_group(void *control, void *font)
{ world_log("init.group %x %x\n", world_offset(control), world_offset(font)); world_init_tick(); }
static void TC world_group_hook(void *control, void *font) { world_group(control, font); }
static int TC world_init_other_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{ world_log("other.check\n"); return world_init_check(control, font, bitmap, dib, palette, module); }
static const struct fs98_world_init_services s_init_services = {
    world_init_base, NULL, NULL, NULL, NULL, NULL, world_init_art, world_init_button,
    world_init_text, world_init_combo, world_init_font_service, world_init_edit, world_title
};
static const struct fs98_world_search_init_services s_search_init_services = {
    &s_init_services, &s_services, world_group
};
static uint32_t world_init_count(const char *needle)
{
    uint32_t count = 0;
    const char *cursor = s_logs[0];
    while ((cursor = strstr(cursor, needle)) != NULL) { ++count; cursor += strlen(needle); }
    return count;
}
static void world_search_init_run(uint32_t throw_at)
{
    int result;
    s_fixture = s_initial;
    s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    s_resource_module = 0x27182818;
    world_init_tick();
    if (__builtin_setjmp(s_jump) == 0) {
        result = s_side ? fs98_world_search_init(s_fixture.object) :
            ((int (TC *)(void *))((uint8_t *)s_retail + 0xdec0))(s_fixture.object);
        if (!s_side && !s_throw_at) {
            /* No explicit stores to +0x84 or the first text-control header
             * at +0x10e0/+0x10e4 under the synthetic setup boundary. */
            if (result != (s_scenario % 5 != 0) ||
                U32(s_fixture.object, 0x84) != U32(s_initial.object, 0x84) ||
                U32(s_fixture.object, 0x10e0) != U32(s_initial.object, 0x10e0) ||
                U32(s_fixture.object, 0x10e4) != U32(s_initial.object, 0x10e4) ||
                (!result && s_calls != 2) ||
                (result && (world_init_count("button ") != 6 || world_init_count("init.combo ") != 2 ||
                    world_init_count("init.text ") != 6 || world_init_count("init.check ") != 1 ||
                    world_init_count("init.group ") != 1 || world_init_count("init.edit ") != 1))) {
                fprintf(stderr, "search init fixture witness failed scenario=%u\n%s", s_scenario, s_logs[0]); exit(2);
            }
            ++s_self_checks;
        }
        world_log("return %d\n", result);
    } else world_log("caught\n");
    if (memcmp(s_fixture.before, s_initial.before, 32) || memcmp(s_fixture.after, s_initial.after, 32)) exit(2);
}
static int world_search_init_compare(uint32_t throw_at)
{
    s_side = 0; world_search_init_run(throw_at); s_reference = s_fixture; s_resource_reference = s_resource_module;
    s_side = 1; world_search_init_run(throw_at); ++s_comparisons;
    if (s_resource_module == s_resource_reference && !memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "SEARCH INIT DIFFER scenario=%u throw=%u\nRETAIL:\n%sOURS:\n%s", s_scenario, throw_at, s_logs[0], s_logs[1]); return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000;
    uint32_t index, limit;
    int interrupt = argc > 2 && !strcmp(argv[2], "interrupt");
    static const uint32_t s_capacities[] = {0, 1, 128, 0xffffffff, 0x80000000};
    if (!count) return 2;
    s_rng = 0x4a971b63;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
    world_patch(0x255bc, world_hook_set); world_patch(0x255c8, world_hook_enable);
    world_patch(0x2577c, world_hook_check); world_patch(0x258b0, world_hook_send);
    world_patch(0x257c0, world_init_base_hook); world_patch(0x257c4, world_init_art_hook);
    world_patch(0x257c8, world_init_button_hook); world_patch(0x257dc, world_init_text_hook);
    world_patch(0x25790, world_init_combo_hook); world_patch(0x257d0, world_init_font);
    world_patch(0x25740, world_init_edit_hook); world_patch(0x257e0, world_hook_title);
    world_patch(0x25730, world_group_hook);
    s_vtable[0xe4 / 4] = world_init_skin;
    s_other_vtable[0xe4 / 4] = world_init_other_skin;
    s_check_vtable[0xc4 / 4] = world_init_check;
    s_other_check_vtable[0xc4 / 4] = world_init_other_check;
    fs98_world_search_set_services(&s_search_services);
    fs98_world_search_set_init_services(&s_search_init_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        world_initialize();
        for (index = 0; index < 2; ++index) {
            PTR(s_fixture.descriptors[index], 0x1c) = index ? "Search second title" : "Search first title";
            U32(s_fixture.descriptors[index], 0x3c) = s_capacities[(s_scenario + index) % 5];
        }
        PTR(s_fixture.object, 0x1bb4) = s_scenario & 64 ? s_other_check_vtable : s_check_vtable;
        s_initial = s_fixture;
        if (world_search_init_compare(0)) return 1;
        limit = s_calls - 1;
        if (interrupt) for (index = 1; index <= limit; ++index)
            if (world_search_init_compare(index)) return 1;
    }
    printf("World search initializer: %u scenarios, %u comparisons, %u interrupted calls, %u self-checks, 0 differ; seed=4a971b63\n", count, s_comparisons, s_throws, s_self_checks);
    FreeLibrary(s_retail); return 0;
}
