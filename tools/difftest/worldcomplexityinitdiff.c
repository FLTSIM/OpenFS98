/* Scenery complexity initialization and auxiliary CNotify scalar deletion.
 * Reuse the independent guarded event fixture, not any retail local code. */
#define main world_complexity_event_main
#include "worldcomplexitydiff.c"
#undef main
static uint32_t s_saved_seh, s_init_operation, s_flags;
static void *s_control_vtable[51];
static int world_base_init(void *object)
{ world_log("base.init %x\n", world_offset(object)); return -1; }
static int TC world_base_init_hook(void *object) { return world_base_init(object); }
static uint32_t world_resources(void) { return 0xbad98765; }
static int TC world_skin(void *object, uint32_t bitmap, uint32_t module, uint32_t flags)
{
    world_log("skin %x %x %x %x\n", world_offset(object), bitmap, module, flags);
    return s_scenario % 5 != 0;
}
static int world_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    world_log("art %x %x %x %x %x\n", world_offset(control), bitmap, world_offset(dib), world_offset(palette), flags);
    return -1;
}
static int TC world_art_hook(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return world_art(control, bitmap, dib, palette, flags); }
static int world_button(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{
    world_log("button %x fonts=%x/%x bitmap=%x dib=%x palette=%x flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        world_offset(control), world_offset(font), world_offset(alternate), bitmap, world_offset(dib), world_offset(palette),
        flags, (uint32_t)color, module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    return -1;
}
static int TC world_button_hook(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette,
    uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{ return world_button(control, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect); }
static int TC world_init_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{
    world_log("init.check %x font=%x bitmap=%x dib=%x palette=%x module=%x\n",
        world_offset(control), world_offset(font), bitmap, world_offset(dib), world_offset(palette), module);
    return -1;
}
static void world_text(void *control, void *font)
{ world_log("text %x font=%x\n", world_offset(control), world_offset(font)); }
static void TC world_text_hook(void *control, void *font) { world_text(control, font); }
static void world_combo(void *control, void *font, void *label)
{ world_log("combo %x font=%x label=%x\n", world_offset(control), world_offset(font), world_offset(label)); }
static void TC world_combo_hook(void *control, void *font, void *label) { world_combo(control, font, label); }
static void *world_allocate(uint32_t bytes)
{
    world_log("allocate %x\n", bytes);
    return s_scenario % 7 ? s_fixture.notify : NULL;
}
static void *TC world_notify_construct(void *object, uint32_t message, uint32_t flags)
{
    world_log("notify.construct %x message=%x flags=%x\n", world_offset(object), message, flags);
    U32(object, 0) = 0x1234; U32(object, 4) = message; U32(object, 8) = flags;
    return object;
}
static void SC world_notify_free(void *object)
{ world_log("notify.free %x\n", world_offset(object)); U32(object, 12) = 0xf4; }
static void TC world_notify_destroy(void *object)
{ world_log("notify.destroy %x\n", world_offset(object)); U32(object, 12) = 0xbc; }
static void world_notify_delete(void *object)
{ world_log("notify.delete %x\n", world_offset(object)); U32(object, 16) = 0x800; }
static void *world_resolve(uint32_t slot)
{
    switch (slot) {
        case 0x2584c: return world_allocate;
        case 0x257cc: return world_notify_construct;
        case 0x257f4: return world_notify_free;
        case 0x257bc: return world_notify_destroy;
        case 0x25800: return world_notify_delete;
        case 0x258b0: return world_hook_send;
        default: exit(2);
    }
}
static const struct fs98_world_init_services s_init_services = {
    .base_init = world_base_init, .art = world_art, .button = world_button,
    .text = world_text, .combo = world_combo
};
static struct fs98_world_complexity_init_services s_complexity_init_services = {
    &s_init_services, world_resolve, world_resources, NULL
};
static void world_land(void)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory"); __builtin_longjmp(s_jump, 1); }
static EXCEPTION_DISPOSITION __cdecl world_catch(EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == THROW_CODE && !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, NULL, record, NULL); s_throw_at = 0; world_log("caught\n");
        if (!s_side) ++s_throws;
        world_land();
    }
    return ExceptionContinueSearch;
}
static LONG WINAPI world_fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u throw=%u\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
        s_side, s_scenario, s_init_operation, s_throw_at);
    ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static __attribute__((noinline)) void world_init_run(uint32_t throw_at)
{
    struct {void *previous, *handler;} catcher;
    int result;
    s_fixture = s_initial; s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = world_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        if (!s_init_operation) result = s_side ? fs98_world_complexity_init(s_fixture.object) :
            ((int (TC *)(void *))((uint8_t *)s_retail + 0xafd0))(s_fixture.object);
        else {
            void *returned = s_side ? fs98_world_complexity_notify_delete(s_fixture.notify, s_flags) :
                ((void *(TC *)(void *, uint32_t))((uint8_t *)s_retail + 0x2700))(s_fixture.notify, s_flags);
            result = (int)world_offset(returned);
        }
        if (!s_side && !s_throw_at) {
            int bad;
            if (!s_init_operation) {
                int populated = strstr(s_logs[0], "allocate ") != NULL;
                bad = (result != populated) || ((s_scenario % 5 == 0) && s_calls != 2);
                if (populated) bad |= PTR(s_fixture.object, 0x10dc) != (s_scenario % 7 ? s_fixture.notify : NULL);
            } else bad = s_calls != 1u + (s_flags & 1) || U32(s_fixture.notify, 12) != 0xbc;
            if (bad) { fprintf(stderr, "init witness failed case=%u op=%u\n%s",s_scenario,s_init_operation,s_logs[0]); exit(2); }
            ++s_self_checks;
        }
        world_log("return %x\n", (uint32_t)result);
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    if (memcmp(s_fixture.before,s_initial.before,32) || memcmp(s_fixture.after,s_initial.after,32)) exit(2);
}
static int world_init_compare(uint32_t throw_at)
{
    s_side = 0; world_init_run(throw_at); s_reference = s_fixture;
    s_side = 1; world_init_run(throw_at); ++s_comparisons;
    if (!memcmp(&s_reference,&s_fixture,sizeof(s_fixture)) && !strcmp(s_logs[0],s_logs[1])) return 0;
    fprintf(stderr,"DIFFER scenario=%u operation=%u throw=%u\nRETAIL:\n%sOURS:\n%s",s_scenario,s_init_operation,throw_at,s_logs[0],s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1],NULL,0) : 2000, index, limit;
    int unwind = argc > 2 && !strcmp(argv[2],"unwind");
    if (!count || (argc > 2 && strcmp(argv[2],"normal") && !unwind)) return 2;
    s_rng = 0x4a971b63; s_operation = 0;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fatal);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail,"Linkage") != (FARPROC)((uint8_t *)s_retail+0x24120)) return 2;
    world_patch(0x2577c,world_hook_check); world_patch(0x258b0,world_hook_send);
    world_patch(0x257c0,world_base_init_hook); world_patch(0x257c4,world_art_hook);
    world_patch(0x257c8,world_button_hook); world_patch(0x257dc,world_text_hook); world_patch(0x25790,world_combo_hook);
    world_patch(0x2584c,world_allocate); world_patch(0x257cc,world_notify_construct);
    world_patch(0x257f4,world_notify_free); world_patch(0x257bc,world_notify_destroy); world_patch(0x25800,world_notify_delete);
    world_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));
    U32(s_retail,0x2495c)=world_resources();
    s_vtable[0xe4/4]=world_skin; memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));
    s_control_vtable[0xc4/4]=world_init_check;
    s_complexity_init_services.notify_vtable=(uint8_t *)s_retail+0x1e1e8;
    fs98_world_complexity_set_services(&s_services);
    fs98_world_complexity_set_init_services(&s_complexity_init_services);
    for (s_scenario=0;s_scenario<count;++s_scenario) for(s_init_operation=0;s_init_operation<2;++s_init_operation) {
        world_initialize(); s_flags=world_random();
        PTR(s_fixture.object,0x10dc)=NULL;
        if (s_scenario%3==0) PTR(s_fixture.object,0x10e0)=NULL;
        PTR(s_fixture.object,0x14c4)=PTR(s_fixture.object,0x158c)=PTR(s_fixture.object,0x1654)=s_control_vtable;
        s_initial=s_fixture;
        if(world_init_compare(0)) return 1;
        limit=s_calls-1;
        if(unwind) for(index=1;index<=limit;++index) if(world_init_compare(index)) return 1;
    }
    printf("World complexity init: %u scenarios, %u comparisons, 0 differ; seed=4a971b63 unwinds=%u witnesses=%u\n",count,s_comparisons,s_throws,s_self_checks);
    FreeLibrary(s_retail); return 0;
}
