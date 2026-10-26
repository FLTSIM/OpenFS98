/* Independent priority core fixture: retail code, controlled UI/owner calls. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_priority.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429818
struct priority_fixture {
    uint8_t before[32], object[0x1bb4], descriptors[2][0x40], rows[2][6][0x40];
    char titles[2][6][64];
    int list_count, list_selection;
    uint32_t callbacks[5], painted_reads;
    uint8_t after[32];
};
static struct priority_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static uint32_t s_scenario, s_operation, s_calls, s_length, s_throw_at, s_comparisons, s_throws, s_seh, s_rng;
static uint32_t s_string_live, s_string_reference;
static int s_side;
static char s_string[512], s_logs[2][32768];
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static uint32_t world_offset(const void *ptr) { return (uint32_t)((const uint8_t *)ptr - (const uint8_t *)&s_fixture); }
static uint32_t world_random(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static void world_log(const char *format, ...)
{
    va_list args; int length;
    va_start(args, format); length = vsnprintf(s_logs[s_side] + s_length, 32768 - s_length, format, args); va_end(args);
    if (length < 0 || (uint32_t)length >= 32768 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) RaiseException(THROW_CODE, 0, 0, NULL);
    if (s_calls == 2 && (s_scenario & 1)) PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[1];
}
static int world_get_text(void *control, char *text, int capacity)
{
    static const char *s_numbers[] = {"002", "0", "-1", "999", "  +4x", "2147483647", "-2147483648"};
    const char *value = s_numbers[s_scenario % 7];
    world_log("get %x capacity=%d [%s]\n", world_offset(control), capacity, value);
    snprintf(text, (size_t)capacity, "%s", value); return (int)strlen(text);
}
static int TC world_get_hook(void *control, char *text, int capacity) { return world_get_text(control, text, capacity); }
static int world_parse(const char *text) { world_log("parse [%s]\n", text); return atoi(text); }
static int world_format(char *text, const char *format, int value) { world_log("format [%s] %d\n", format, value); return sprintf(text, format, value); }
static void world_set_text(void *control, const char *text) { world_log("text %x [%s]\n", world_offset(control), text); }
static void TC world_set_hook(void *control, const char *text) { world_set_text(control, text); }
static int world_enable(void *control, int enabled) { world_log("enable %x %d\n", world_offset(control), enabled); return -1; }
static int TC world_enable_hook(void *control, int enabled) { return world_enable(control, enabled); }
static void world_focus_control(void *control) { world_log("focus.control %x\n", world_offset(control)); }
static void *TC world_focus_hook(void *control) { world_focus_control(control); return control; }
static int world_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{
    if (message == 0x180) world_log("send %x %x text=[%s]\n", window, message, (char *)second);
    else world_log("send %x %x %x %x\n", window, message, first, second);
    if (message == 0x184) s_fixture.list_count = 0;
    if (message == 0x180) return s_fixture.list_count++;
    if (message == 0x18b) return s_fixture.list_count;
    if (message == 0x188) return s_fixture.list_selection;
    if (message == 0x186) {
        s_fixture.list_selection = first < (uint32_t)s_fixture.list_count ? (int)first : -1;
        return s_fixture.list_selection;
    }
    return -1;
}
static int SC world_send_hook(uint32_t window, uint32_t message, uint32_t first, uint32_t second) { return world_send(window, message, first, second); }
static int world_error(void *object, void *control, void *status)
{
    world_log("error %x control=%x status=%x text=[%s] max=%u\n", world_offset(object), world_offset(control), U32(status, 0), (char *)PTR(status, 4), U32(status, 8));
    return s_scenario & 2 ? 1 : 0;
}
static int TC world_error_hook(void *object, void *control, void *status) { return world_error(object, control, status); }
static int TC world_virtual_focus(void *object, uint32_t control) { world_log("focus %x control=%x\n", world_offset(object), control); return -7; }
static int TC world_virtual_validate(void *object, uint32_t unused)
{
    world_log("virtual.validate %x %x\n", world_offset(object), unused); PTR(object, 0) = s_other_vtable;
    return s_scenario & 2 ? 0x1b08 : 0;
}
static int TC world_virtual_error(void *object, uint32_t control) { world_log("virtual.error %x %x\n", world_offset(object), control); return !!(s_scenario & 4); }
static void TC world_close(void *object) { world_log("close %x answer=%u\n", world_offset(object), U32(PTR(object, 0x10dc), 0)); }
static void TC world_other_close(void *object) { world_log("other.close\n"); world_close(object); }
static void *TC world_virtual_delete(void *object, uint32_t flags) { world_log("virtual.delete %x %x\n", world_offset(object), flags); return object; }
static uint32_t world_callback(void *descriptor, uint32_t offset)
{
    world_log("callback %x %x\n", world_offset(descriptor), offset);
    ++s_fixture.callbacks[offset / 4 - 1];
    if (offset == 4) U32(descriptor, 0x28) = s_scenario & 8 ? 0xb : 0;
    if (offset == 8) --U32(descriptor, 0x20);
    if (offset == 0xc) ++U32(descriptor, 0x20);
    if (offset == 0x10 && (int32_t)U32(descriptor, 0x1c) >= 2) {
        uint8_t row[0x40]; uint8_t *rows = PTR(descriptor, 0x24);
        memcpy(row, rows, 0x40); memcpy(rows, rows + 0x40, 0x40); memcpy(rows + 0x40, row, 0x40);
    }
    if (offset == 0x14) U32(descriptor, 0x20) = U32(descriptor, 0x2c) - 1;
    return 0xffffffff;
}
#define PRIORITY_CALLBACK(name, offset) static uint32_t SC name(void *descriptor) { return world_callback(descriptor, offset); }
PRIORITY_CALLBACK(world_check_callback, 4)
PRIORITY_CALLBACK(world_up_callback, 8)
PRIORITY_CALLBACK(world_down_callback, 0xc)
PRIORITY_CALLBACK(world_sort_callback, 0x10)
PRIORITY_CALLBACK(world_move_callback, 0x14)
static const struct fs98_world_services s_events = {
    world_get_text, world_set_text, world_parse, world_format, world_send,
    NULL, world_enable, world_error, world_focus_control
};
static int world_init_base(void *object) { world_log("base.init %x\n", world_offset(object)); return -1; }
static int TC world_init_base_hook(void *object) { return world_init_base(object); }
static int TC world_init_skin(void *object, uint32_t bitmap, uint32_t module, uint32_t flags)
{ world_log("skin %x bitmap=%x module=%x flags=%x\n", world_offset(object), bitmap, module, flags); return s_scenario % 5 ? 1 : 0; }
static void world_init_string_construct(void *string)
{ world_log("string.construct\n"); PTR(string, 0) = s_string; s_string[0] = 0; ++s_string_live;
    /* Retail's CString local is immediately followed by its 100-byte scratch
     * buffer. Paint only that owned local, including first-invalid-layer runs. */
    memset((uint8_t *)string + 4, 0x5a, 100);
    strcpy((char *)string + 4, "Painted uninitialized layer"); }
static void TC world_init_string_construct_hook(void *string) { world_init_string_construct(string); }
static char *world_init_buffer(void *string, int minimum)
{ (void)string; world_log("string.buffer %d\n", minimum); return s_string; }
static void world_init_release(void *string, int length)
{ (void)string; world_log("string.release %d\n", length); }
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

static void world_string_assign(void *string, const char *text)
{ world_log("string.assign [%s]\n", text); snprintf((char *)PTR(string, 0), 512, "%s", text); }
static void *TC world_assign_hook(void *string, const char *text) { world_string_assign(string, text); return string; }
static void world_string_append(void *string, const char *text)
{ world_log("string.append [%s]\n", text); if (!strcmp(text, "Painted uninitialized layer")) ++s_fixture.painted_reads; if (strlen(PTR(string, 0)) + strlen(text) >= 512) exit(2); strcat(PTR(string, 0), text); }
static void *TC world_append_hook(void *string, const char *text) { world_string_append(string, text); return string; }
static int world_tabs(void *font, int count, const int *tabs)
{ world_log("tabs %x count=%d values=%d/%d\n", world_offset(font), count, tabs[0], tabs[1]); return -1; }
static int TC world_tabs_hook(void *font, int count, const int *tabs) { return world_tabs(font, count, tabs); }
static const struct fs98_world_init_services s_init_services = {
    world_init_base, world_init_string_construct, world_init_buffer, world_init_release, world_init_string_destroy,
    world_init_load_service, world_init_art, world_init_button, world_init_text, world_init_combo, world_init_font_service, world_init_edit, world_set_text
};
static const struct fs98_world_priority_services s_priority_services = {
    world_string_assign, world_string_append, world_tabs, world_init_combo
};
static void world_patch(uint32_t rva, void *value)
{
    DWORD old; void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = value; if (!VirtualProtect(slot, 4, old, &old)) exit(2);
}
static const uint32_t s_entries[] = {0xcba0,0xd030,0xce90,0xcfd0,0xce70,0xce50,0xce30,0xd390,0xd2f0,0xcf50,0xe4e0,0x3d30};
static void world_initialize(void)
{
    uint32_t byte, kind, row;
    int count = (int)(s_scenario % 8) - 1;
    for (byte = 0; byte < sizeof(s_fixture); ++byte) ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    PTR(s_fixture.object, 0) = s_vtable; PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[0];
    for (kind = 0; kind < 2; ++kind) {
        void *descriptor = s_fixture.descriptors[kind];
        PTR(descriptor, 4) = world_check_callback; PTR(descriptor, 8) = world_up_callback;
        PTR(descriptor, 0xc) = world_down_callback; PTR(descriptor, 0x10) = world_sort_callback; PTR(descriptor, 0x14) = world_move_callback;
        PTR(descriptor, 0x18) = "Priority fixture"; U32(descriptor, 0x1c) = (uint32_t)count;
        U32(descriptor, 0x20) = s_scenario % 5 == 0 ? 0xffffffff : s_scenario % 5 == 1 ? 0 : (uint32_t)(count - 1);
        PTR(descriptor, 0x24) = s_fixture.rows[kind]; U32(descriptor, 0x28) = s_scenario & 16 ? 0xb : 0;
        PTR(descriptor, 0x34) = NULL;
        for (row = 0; row < 6; ++row) {
            snprintf(s_fixture.titles[kind][row], 64, "Fixture %u row %u", kind, row);
            PTR(s_fixture.rows[kind][row], 0x18) = s_fixture.titles[kind][row];
            U32(s_fixture.rows[kind][row], 0x2c) = (row % 5 + 1) * 10;
            /* Preserve an invalid later layer's reuse of the formatted index.
             * Sort can swap row 0/1; the first-row case is independently painted. */
            if ((row >= 2 && (s_scenario & 32)) || (row == 0 && (s_scenario & 64))) U32(s_fixture.rows[kind][row], 0x2c) = 11;
            U32(s_fixture.rows[kind][row], 0x34) = (s_scenario + row) & 1;
        }
    }
    memset(s_fixture.callbacks, 0, sizeof(s_fixture.callbacks)); s_fixture.painted_reads = 0;
    s_fixture.list_count = count > 0 ? count : 0;
    s_fixture.list_selection = (int)U32(s_fixture.descriptors[0], 0x20);
    s_initial = s_fixture;
}
static void world_invoke(void)
{
    void *object = s_operation == 11 && (s_scenario & 1) ? NULL : s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    int result = 0;
    if (!s_side) {
        if (s_operation == 7 || s_operation == 8) result = ((int (TC *)(void *, uint32_t))entry)(object, s_scenario);
        else if (s_operation == 0) result = ((int (TC *)(void *))entry)(object);
        else ((void (TC *)(void *))entry)(object);
    } else switch (s_operation) {
        case 0: result = fs98_world_priority_init(object); break;
        case 1: fs98_world_priority_populate(object); break;
        case 2: fs98_world_priority_selection(object); break;
        case 3: fs98_world_priority_collect(object); break;
        case 4: fs98_world_priority_action(object, 8); break;
        case 5: fs98_world_priority_action(object, 0xc); break;
        case 6: fs98_world_priority_action(object, 0x10); break;
        case 7: result = fs98_world_priority_validate(object, s_scenario); break;
        case 8: result = fs98_world_priority_error(object, s_scenario); break;
        case 9: fs98_world_priority_accept(object); break;
        case 10: fs98_world_priority_cancel(object); break;
        case 11: fs98_world_priority_post_destroy(object); break;
    }
    if (!s_side && !s_throw_at) {
        if ((s_operation == 1 && (s_scenario & 64) && (int32_t)U32(s_initial.descriptors[0], 0x1c) > 0 && s_fixture.painted_reads != 1) ||
            (s_operation == 0 && result != (s_scenario % 5 != 0)) ||
            (s_operation == 7 && (result != ((s_scenario & 8) ? 0x1b08 : 0) ||
                s_fixture.callbacks[0] != 1 || s_fixture.callbacks[4] != ((s_scenario & 8) ? 0u : 1u))) ||
            (s_operation == 8 && result != -7) || s_string_live ||
            (s_operation == 10 && U32(PTR(s_fixture.object, 0x10dc), 0) != 0)) {
            fprintf(stderr, "priority fixture witness failed scenario=%u op=%u\n", s_scenario, s_operation); exit(2);
        }
    }
    world_log("return %x\n", (uint32_t)result);
}
static void world_land(void) { __asm__ volatile("movl %0, %%fs:0" : : "r"(s_seh) : "memory"); __builtin_longjmp(s_jump, 1); }
static EXCEPTION_DISPOSITION __cdecl world_catch(EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == THROW_CODE && !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, NULL, record, NULL); s_throw_at = 0; world_log("caught\n"); if (!s_side) ++s_throws; world_land();
    }
    return ExceptionContinueSearch;
}
static LONG WINAPI world_fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "PRIORITY FAULT side=%d scenario=%u op=%u pc=%p code=%lx\n", s_side, s_scenario, s_operation,
        exception->ExceptionRecord->ExceptionAddress, exception->ExceptionRecord->ExceptionCode); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static __attribute__((noinline)) void world_run(uint32_t throw_at)
{
    struct {void *previous, *handler;} catcher;
    uint32_t index;
    s_fixture = s_initial; s_string_live = 0; s_string[0] = 0;
    s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_seh; catcher.handler = world_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory"); world_invoke();
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_seh) : "memory");
    for (index = 0; index < 2; ++index)
        if (PTR(s_fixture.descriptors[index], 0x34)) PTR(s_fixture.descriptors[index], 0x34) = (void *)0x1234;
    if (memcmp(s_fixture.before, s_initial.before, 32) || memcmp(s_fixture.after, s_initial.after, 32)) { fprintf(stderr,"priority guard changed\n"); exit(2); }
}
static int world_compare(uint32_t throw_at)
{
    uint32_t index;
    s_side = 0; world_run(throw_at); s_reference = s_fixture; s_string_reference = s_string_live;
    s_side = 1; world_run(throw_at); ++s_comparisons;
    if (s_string_reference == s_string_live && !memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr,"PRIORITY DIFFER scenario=%u op=%u throw=%u string.live=%u/%u\n", s_scenario, s_operation, throw_at, s_string_reference, s_string_live);
    for (index=0;index<sizeof(s_fixture);++index) if (((uint8_t *)&s_reference)[index]!=((uint8_t *)&s_fixture)[index]) {fprintf(stderr,"byte=%x retail=%x ours=%x\n",index,((uint8_t *)&s_reference)[index],((uint8_t *)&s_fixture)[index]);break;}
    fprintf(stderr,"RETAIL:\n%sOURS:\n%s",s_logs[0],s_logs[1]); return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1],NULL,0) : 2000, position, limit;
    int unwind = argc > 2 && !strcmp(argv[2],"unwind");
    s_rng = 0x4a971b63; SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fault);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);
    if(!s_retail || GetProcAddress(s_retail,"Linkage")!=(FARPROC)((uint8_t *)s_retail+0x24120))return 2;
    world_patch(0x25598, world_get_hook); world_patch(0x255bc, world_set_hook); world_patch(0x255c8, world_enable_hook);
    world_patch(0x25590, world_focus_hook); world_patch(0x258b0, world_send_hook); world_patch(0x25778, world_error_hook);
    world_patch(0x256b8, world_format); world_patch(0x256c0, world_parse);
    world_patch(0x257c0, world_init_base_hook); world_patch(0x255b4, world_init_string_construct_hook); world_patch(0x255c0, world_init_string_destroy_hook);
    world_patch(0x25578, world_assign_hook); world_patch(0x25580, world_append_hook); world_patch(0x25890, world_init_load);
    world_patch(0x257c4, world_init_art_hook); world_patch(0x257c8, world_init_button_hook); world_patch(0x257dc, world_init_text_hook);
    world_patch(0x257d0, world_init_font); world_patch(0x25740, world_init_edit_hook); world_patch(0x257e0, world_set_hook);
    world_patch(0x25714, world_tabs_hook); world_patch(0x25718, world_init_combo_hook);
    world_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));
    s_vtable[1]=world_virtual_delete; s_vtable[0xd8/4]=world_virtual_validate; s_vtable[0xdc/4]=world_virtual_error;
    s_vtable[0xe0/4]=world_virtual_focus; s_vtable[0xe4/4]=world_init_skin; s_vtable[0xec/4]=world_close;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));s_other_vtable[0xec/4]=world_other_close;
    fs98_world_set_services(&s_events);fs98_world_set_init_services(&s_init_services);fs98_world_priority_set_services(&s_priority_services);
    U32(s_retail,0x2495c)=0x27182818;fs98_world_set_resource_module(0x27182818);
    for(s_scenario=0;s_scenario<count;++s_scenario)for(s_operation=0;s_operation<12;++s_operation){
        world_initialize();if(world_compare(0))return 1;limit=s_calls-1;
        if(unwind)for(position=1;position<=limit;++position)if(world_compare(position))return 1;
    }
    printf("World priority core: %u scenarios, 12 operations, %u comparisons, %u real unwinds, 0 differ\n",count,s_comparisons,s_throws);return 0;
}
