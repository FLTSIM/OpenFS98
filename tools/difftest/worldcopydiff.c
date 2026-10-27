/* Independent scenery-copy fixtures. Only imported controls and the
 * caller-owned validation/close callbacks are patched on the retail side. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_copy.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x1a74], descriptors[2][0x54], rows[2][5][0xc8], after[32];
};
static struct world_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][8192];
static uint32_t s_rng, s_scenario, s_operation, s_calls, s_length;
static uint32_t s_throw_at, s_comparisons, s_throws, s_self_checks;
static int s_side, s_selection;

static uint32_t world_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t world_offset(const void *pointer)
{ return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void world_log(const char *format, ...)
{
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_length, 8192 - s_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) RaiseException(THROW_CODE, 0, 0, NULL);
    /* Change the descriptor and vtable at every possible external boundary.
     * Distinct descriptor payloads expose caching across control calls. */
    if ((s_operation!=7 || s_scenario%3==2) && (s_scenario & 1) && s_calls == 1 + (s_scenario / 8) % 4)
        PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[1];
    if (s_scenario & 2) PTR(s_fixture.object, 0) = s_other_vtable;
}
static int world_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{
    if (message == 0x143 || message == 0x151)
        world_log("send %x %x %x ptr=%x\n", window, message, first, world_offset((void *)second));
    else world_log("send %x %x %x %x\n", window, message, first, second);
    return message == 0x147 && window == 0x87654321 && (s_scenario & 4) ? s_selection ^ 1 : s_selection;
}
static int SC world_hook_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return world_send(window, message, first, second); }
static int world_check_control(void *control, int checked)
{
    world_log("check %x %x\n", world_offset(control), (uint32_t)checked);
    U32(control, 0xbc) = (uint32_t)checked;
    return 0;
}
static int TC world_hook_check(void *control, int checked)
{ return world_check_control(control, checked); }
static void TC world_close(void *object)
{
    world_log("close answer=%x mode=%x\n", U32(PTR(object, 0x10dc), 0), U32(object, 0x1a70));
}
static void TC world_other_close(void *object)
{ world_log("other.close\n"); world_close(object); }

static int world_error(void *object, void *control, uint32_t message)
{
    world_log("error %x %x\n", world_offset(control), message);
    (void)object;
    return (int)0x80000001;
}
static int TC world_hook_error(void *object, void *control, uint32_t message)
{ return world_error(object, control, message); }
static int world_enable(void *control, int enable)
{ world_log("enable %x %x\n",world_offset(control),(uint32_t)enable); U32(control,8)=(uint32_t)enable; return -1; }
static int TC world_hook_enable(void *control, int enable) { return world_enable(control, enable); }
static int world_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ world_log("post %x %x %x %x\n", window, message, first, second); return -1; }
static int SC world_hook_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return world_post(window, message, first, second); }
static const struct fs98_world_copy_services s_services = {world_send, world_check_control, world_error, world_enable, world_post};
static void world_patch(uint32_t rva, void *value)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = value;
    if (!VirtualProtect(slot, 4, old, &old)) exit(2);
}
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
static const struct fs98_world_init_services s_init_services = {
    .base_init=world_base_init, .art=world_art, .button=world_button, .text=world_text, .combo=world_combo
};
static const struct fs98_world_copy_init_services s_copy_init_services={&s_init_services,world_resources};
static const uint32_t s_entries[] = {0x3e00, 0x3ea0, 0x3d40, 0x3d60, 0x40a0, 0x40c0, 0x4080, 0x3b20};
static const uint32_t s_controls[] = {0x15c0, 0x1688, 0x1750, 0x1818, 0x18e0, 0x19a8};
static const uint32_t s_fields[] = {0x28, 0x38, 0x50, 0x40, 0x30, 0x48};
static void *s_control_vtable[51];
static void world_initialize(void)
{
    static const uint32_t s_values[] = {0,1,2,0xffffffff,0x80000000,0x1234ffff,65536,65535};
    uint32_t byte, row, index, set;
    for(byte=0;byte<sizeof(s_fixture);++byte) ((uint8_t *)&s_fixture)[byte]=(uint8_t)world_random();
    PTR(s_fixture.object,0)=s_vtable;
    PTR(s_fixture.object,0x10dc)=s_fixture.descriptors[0];
    if ((s_operation==4||s_operation==7) && s_scenario%3==0) PTR(s_fixture.object,0x10dc)=NULL;
    if ((s_operation==4||s_operation==7) && s_scenario%3==1) U32(s_fixture.object,0x10dc)=0xcdcdcdcd;
    U32(s_fixture.object,0x14b0)=0x12345678; U32(s_fixture.object,0x1468)=0x87654321;
    for(index=0;index<6;++index) {
        PTR(s_fixture.object,s_controls[index])=s_control_vtable;
        U32(s_fixture.object,s_controls[index]+0xbc)=s_values[(s_scenario+index)%8];
    }
    for(set=0;set<2;++set)for(row=0;row<5;++row) PTR(s_fixture.rows[set][row],8)=s_fixture.rows[set][row]+16;
    for(row=0;row<2;++row) {
        PTR(s_fixture.descriptors[row],0x10)=s_fixture.rows[row];
        /* Changing descriptors can change the live loop bound, but cannot
         * escape the guarded five-row allocation. */
        U32(s_fixture.descriptors[row],0xc)=(s_scenario/8+row)%7-1;
        if ((int32_t)U32(s_fixture.descriptors[row],0xc)>5) exit(2);
        U32(s_fixture.descriptors[row],0x14)=s_scenario&8;
        U32(s_fixture.descriptors[row],0x18)=s_scenario&16;
        U32(s_fixture.descriptors[row],0x1c)=s_values[(s_scenario+row)%8];
        U32(s_fixture.descriptors[row],0x20)=s_values[(s_scenario/8+row)%8];
        for(index=0;index<6;++index) U32(s_fixture.descriptors[row],s_fields[index])=s_values[(s_scenario+index+row)%8];
    }
    s_selection=(int)s_values[(s_scenario/4)%8];
    s_initial=s_fixture;
}
static void world_witness(int result)
{
    uint32_t index; int bad=0;
    if (s_operation==4) bad=result!=(PTR(s_initial.object,0x10dc)!=NULL && U32(s_initial.object,0x10dc)!=0xcdcdcdcd);
    if (s_operation==5) bad=result!=((s_scenario&4)?0:0x1012);
    if (s_operation==6) bad=result!=1;
    if (s_operation==0 && !(s_scenario&1)) {
        for(index=0;index<6;++index) bad|=U32(s_fixture.descriptors[0],s_fields[index])!=U32(s_initial.object,s_controls[index]+0xbc);
        bad|=U32(s_fixture.descriptors[0],0x1c)!=(uint32_t)s_selection;
        bad|=U32(s_fixture.descriptors[0],0x20)!=(uint32_t)((s_scenario&4)?s_selection^1:s_selection);
    }
    if (s_operation==1 && !(s_scenario&1))
        for(index=0;index<6;++index) bad|=U32(s_fixture.object,s_controls[index]+0xbc)!=U32(s_initial.descriptors[0],s_fields[index]);
    if (s_operation==2) bad|=U32(s_fixture.object,0x1a70)!=2||U32(s_fixture.descriptors[0],0)!=0;
    if (s_operation==3) bad|=U32(s_fixture.object,0x1a70)!=1||
        (!!strstr(s_logs[0],"close answer=")!=!!(s_scenario&4));
    if (s_operation==7) {
        int populated=s_scenario%5!=0 && s_scenario%3==2;
        bad|=result!=populated;
        bad|=(s_scenario%5==0 && s_calls!=2);
        bad|=(populated && !strstr(s_logs[0],"check "));
    }
    if(bad){fprintf(stderr,"fixture witness failed case=%u op=%u\n%s",s_scenario,s_operation,s_logs[0]);exit(2);}
    ++s_self_checks;
}
static void world_invoke(void)
{
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+s_entries[s_operation]; int result=0;
    if(!s_side) {
        if(s_operation>=4)result=((int(TC *)(void *))entry)(object);
        else ((void(TC *)(void *))entry)(object);
    } else switch(s_operation) {
        case 0: fs98_world_copy_collect(object);break;
        case 1: fs98_world_copy_populate(object);break;
        case 2: fs98_world_copy_cancel(object);break;
        case 3: fs98_world_copy_accept(object);break;
        case 4: result=fs98_world_copy_has_descriptor(object);break;
        case 5: result=fs98_world_copy_check(object);break;
        case 6: result=fs98_world_copy_error(object);break;
        case 7: result=fs98_world_copy_init(object);break;
    }
    if(!s_side&&!s_throw_at)world_witness(result);
    world_log("return %x\n",(uint32_t)result);
}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* The seven routines have no local EH frames or cleanup. This is
         * interruption acceptance, not an MSVC class-lifetime unwind test. */
        __builtin_longjmp(s_jump, 1);
    }
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
        s_side, s_scenario, s_operation);
    ExitProcess(2);
}
static void world_run(uint32_t throw_at)
{
    s_fixture = s_initial;
    s_calls = s_length = 0;
    s_logs[s_side][0] = 0;
    s_throw_at = throw_at;
    if (__builtin_setjmp(s_jump) == 0) world_invoke();
    else world_log("caught\n");
    if (memcmp(s_fixture.before, s_initial.before, sizeof(s_fixture.before)) ||
        memcmp(s_fixture.after, s_initial.after, sizeof(s_fixture.after))) exit(2);
}
static int world_compare(uint32_t throw_at)
{
    uint32_t byte;
    s_side = 0; world_run(throw_at); s_reference = s_fixture;
    s_side = 1; world_run(throw_at); ++s_comparisons;
    if (!memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u\n", s_scenario, s_operation, throw_at);
    for (byte = 0; byte < sizeof(s_fixture); ++byte)
        if (((uint8_t *)&s_reference)[byte] != ((uint8_t *)&s_fixture)[byte]) {
            fprintf(stderr, "byte=%x retail=%x ours=%x\n", byte, ((uint8_t *)&s_reference)[byte], ((uint8_t *)&s_fixture)[byte]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000;
    uint32_t seed = argc > 2 ? strtoul(argv[2], NULL, 0) : 0x4a971b63;
    uint32_t position, limit;
    int interrupt = argc > 3 && !strcmp(argv[3], "interrupt");
    if (!count || !seed || (argc > 3 && strcmp(argv[3], "normal") && !interrupt)) return 2;
    s_rng = seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
    world_patch(0x25778, world_hook_error);
    world_patch(0x255c8, world_hook_enable);
    world_patch(0x258a0, world_hook_post);
    world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send);
    s_vtable[0xec / 4] = world_close;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = world_other_close;
    world_patch(0x257c0,world_base_init_hook);world_patch(0x257c4,world_art_hook);
    world_patch(0x257c8,world_button_hook);world_patch(0x257dc,world_text_hook);world_patch(0x25790,world_combo_hook);
    U32(s_retail,0x2495c)=world_resources();
    s_vtable[0xe4/4]=s_other_vtable[0xe4/4]=world_skin;
    s_control_vtable[0xc4/4]=world_init_check;
    fs98_world_copy_set_init_services(&s_copy_init_services);
    fs98_world_copy_set_services(&s_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 8; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1;
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World copy core: %u scenarios, 8 operations, %u comparisons, 0 differ; seed=%x interrupts=%u witnesses=%u\n",
        count, s_comparisons, seed, s_throws, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
