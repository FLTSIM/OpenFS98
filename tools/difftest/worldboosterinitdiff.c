/* Booster initialization against retail, including shared static font state. */
#define main world_booster_event_main
#include "worldboosterdiff.c"
#undef main
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
static void world_init_text(void *control, void *font) { world_log("init.text %x %x\n", world_offset(control), font == (s_side ? (void *)fs98_world_booster_get_font_state()->font : (uint8_t *)s_retail + 0x249f8) ? 0xf000 : world_offset(font)); }
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

static uint32_t s_font_live, s_font_reference, s_font_registered, s_registered_reference;
static void (*s_cleanup)(void);
static void world_font_construct(void *font, void *base_font)
{
    world_log("font.construct base=%x\n", world_offset(base_font));
    U32(font, 0) = 0xf00d1234; ++s_font_live;
}
static void *TC world_font_construct_hook(void *font, void *base_font)
{ world_font_construct(font, base_font); return font; }
static void world_font_destroy(void *font)
{ world_log("font.destroy marker=%x\n", U32(font, 0)); U32(font, 0) = 0xd00d1234; --s_font_live; }
static void TC world_font_destroy_hook(void *font) { world_font_destroy(font); }
static int world_font_at_exit(void (*callback)(void))
{ world_log("font.at_exit\n"); s_cleanup = callback; ++s_font_registered; return 0; }
static void *__cdecl world_on_exit_hook(void (*callback)(void))
{ world_font_at_exit(callback); return callback; }
static void world_group(void *control, void *font)
{ world_log("init.group %x %x\n", world_offset(control), world_offset(font)); }
static void TC world_group_hook(void *control, void *font) { world_group(control, font); }
static int world_booster_enable(void *control, int enabled)
{
    int result = world_enable(control, enabled);
    if (control == (void *)(s_fixture.object + 0x1b28) && (s_scenario & 8)) U32(s_fixture.object, 0x10e8) ^= 1;
    return result;
}
static int TC world_booster_enable_hook(void *control, int enabled) { return world_booster_enable(control, enabled); }
static struct fs98_world_services s_booster_events;
static const struct fs98_world_init_services s_init_services = {
    world_init_base, world_init_string_construct, world_init_buffer, world_init_release, world_init_string_destroy,
    world_init_load_service, world_init_art, world_init_button, world_init_text, world_init_combo,
    world_init_font_service, world_init_edit, world_title
};
static const struct fs98_world_booster_init_services s_booster_init_services = {
    &s_init_services, &s_booster_events, world_font_construct, world_font_destroy, world_font_at_exit, world_group
};
static struct fs98_world_booster_font_state s_font_initial, s_font_result;
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
static LONG WINAPI world_init_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr,"BOOSTER INIT FAULT scenario=%u side=%d throw=%u code=%lx pc=%p\n",s_scenario,s_side,s_throw_at,
        exception->ExceptionRecord->ExceptionCode,exception->ExceptionRecord->ExceptionAddress); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static __attribute__((noinline)) void world_init_run(uint32_t throw_at)
{
    struct {void *previous, *handler;} catcher;
    int result;
    void *state = s_side ? (void *)fs98_world_booster_get_font_state() : (uint8_t *)s_retail + 0x249f0;
    s_fixture = s_initial; memcpy(state, &s_font_initial, sizeof(s_font_initial));
    s_string_live = s_font_live = s_font_registered = 0; s_cleanup = NULL; s_string[0] = 0;
    s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_init_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_init_seh; catcher.handler = world_init_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        result = s_side ? fs98_world_booster_init(s_fixture.object) :
            ((int (TC *)(void *))((uint8_t *)s_retail + 0x68b0))(s_fixture.object);
        if (!s_side && !s_throw_at) {
            if (result != (s_scenario % 5 != 0) || s_string_live ||
                (result && (PTR(s_fixture.object,0x10e0) != s_fixture.object+0x1e6c ||
                            PTR(s_fixture.object,0x10e4) != s_fixture.object+0x1d70))) {
                fprintf(stderr,"booster init fixture witness failed\n");exit(2);
            }
            ++s_self_checks;
        }
        world_log("return %d\n", result);
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_init_seh) : "memory");
    /* Exercise the registered destructor separately from initializer unwinding.
     * A seeded initialized font has no newly registered callback in this run. */
    s_throw_at = 0;
    if (s_cleanup) s_cleanup();
    if (memcmp(s_fixture.before,s_initial.before,32) || memcmp(s_fixture.after,s_initial.after,32)) exit(2);
}
static int world_init_compare(uint32_t throw_at)
{
    s_side = 0; world_init_run(throw_at); s_reference = s_fixture;
    memcpy(&s_font_result,(uint8_t *)s_retail+0x249f0,sizeof(s_font_result));
    s_string_reference=s_string_live; s_font_reference=s_font_live; s_registered_reference=s_font_registered;
    s_side = 1; world_init_run(throw_at); ++s_comparisons;
    if (s_string_reference==s_string_live && s_font_reference==s_font_live && s_registered_reference==s_font_registered &&
        !memcmp(&s_font_result,fs98_world_booster_get_font_state(),sizeof(s_font_result)) &&
        !memcmp(&s_reference,&s_fixture,sizeof(s_fixture)) && !strcmp(s_logs[0],s_logs[1])) return 0;
    fprintf(stderr,"BOOSTER INIT DIFFER scenario=%u throw=%u string.live=%u/%u font.live=%u/%u registered=%u/%u\nRETAIL:\n%sOURS:\n%s",
        s_scenario,throw_at,s_string_reference,s_string_live,s_font_reference,s_font_live,s_registered_reference,s_font_registered,s_logs[0],s_logs[1]);return 1;
}
int main(int argc, char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):2000,index,limit,byte;
    int unwind=argc>2&&!strcmp(argv[2],"unwind");
    s_rng=0x4a971b63;SetErrorMode(SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(world_init_fault);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);
    if(!s_retail||GetProcAddress(s_retail,"Linkage")!=(FARPROC)((uint8_t *)s_retail+0x24120))return 2;
    world_patch(0x255bc,world_hook_set);world_patch(0x255c8,world_booster_enable_hook);world_patch(0x2577c,world_hook_check);
    world_patch(0x258b0,world_hook_send);world_patch(0x256b8,world_hook_format);
    world_patch(0x257c0,world_init_base_hook);world_patch(0x255b4,world_init_string_construct_hook);
    world_patch(0x25588,world_init_buffer_hook);world_patch(0x2558c,world_init_release_hook);world_patch(0x255c0,world_init_string_destroy_hook);
    world_patch(0x25890,world_init_load);world_patch(0x257c4,world_init_art_hook);world_patch(0x257c8,world_init_button_hook);
    world_patch(0x257dc,world_init_text_hook);world_patch(0x25790,world_init_combo_hook);world_patch(0x257d0,world_init_font);
    world_patch(0x25740,world_init_edit_hook);world_patch(0x257e0,world_hook_title);
    world_patch(0x2578c,world_font_construct_hook);world_patch(0x2579c,world_font_destroy_hook);world_patch(0x25730,world_group_hook);
    world_patch(0x256a8,world_on_exit_hook);U32(s_retail,0x24b7c)=0xffffffff;
    world_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));
    s_vtable[0xe4/4]=world_init_skin;s_other_vtable[0xe4/4]=world_init_skin;s_check_vtable[0xc4/4]=world_init_check;
    s_booster_events=s_services;s_booster_events.enable=world_booster_enable;
    fs98_world_booster_set_services(&s_booster_services);fs98_world_booster_set_init_services(&s_booster_init_services);
    U32(s_retail,0x2495c)=0x27182818;fs98_world_set_resource_module(0x27182818);
    for(s_scenario=0;s_scenario<count;++s_scenario){
        world_initialize();s_operation=6;
        for(index=0;index<2;++index){
            if(s_scenario&4)s_fixture.paths[index][0]=0;
            PTR(s_fixture.descriptors[index],8)="Booster fixture title";
            U32(s_fixture.rows[index],8)=288;U32(s_fixture.rows[index],0x1c)=288;
        }
        PTR(s_fixture.object,0x1ca8)=s_check_vtable;s_initial=s_fixture;
        for(byte=0;byte<sizeof(s_font_initial);++byte)((uint8_t *)&s_font_initial)[byte]=(uint8_t)world_random();
        s_font_initial.flags=(s_font_initial.flags&~1u)|((s_scenario>>4)&1);
        if(world_init_compare(0))return 1;
        limit=s_calls-(s_font_registered?2:1);
        if(unwind)for(index=1;index<=limit;++index)if(world_init_compare(index))return 1;
    }
    printf("World booster initializer: %u scenarios, %u comparisons, %u real unwinds, %u self-checks, 0 differ\n",count,s_comparisons,s_throws,s_self_checks);return 0;
}
