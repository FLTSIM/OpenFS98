/* Reuse World's guarded event fixture, add the initializer's import boundary. */
#define main world_event_main
#include "worlddiff.c"
#undef main
#include "../../src/world/world_init.h"
#include "../../src/world/world_unwind.h"
static uint32 s_uiInitSeh, s_uiStringLive, s_uiStringReference;
static char s_acString[256];
static void *s_apCheckVtable[60];
static int world_init_base(void *pObject) { world_log("base.init %x\n", world_offset(pObject)); return -1; }
static int TC world_init_base_hook(void *pObject) { return world_init_base(pObject); }
static int TC world_init_skin(void *pObject, uint32 uiBitmap, uint32 uiModule, uint32 uiFlags)
{ world_log("skin %x bitmap=%x module=%x flags=%x\n", world_offset(pObject), uiBitmap, uiModule, uiFlags); return s_uiScenario % 5 ? 1 : 0; }
static void world_init_string_construct(void *pString)
{ world_log("string.construct\n"); PTR(pString, 0) = s_acString; s_acString[0] = 0; ++s_uiStringLive; }
static void TC world_init_string_construct_hook(void *pString) { world_init_string_construct(pString); }
static char *world_init_buffer(void *pString, int iMinimum)
{ (void)pString; world_log("string.buffer %d\n", iMinimum); return s_acString; }
static char *TC world_init_buffer_hook(void *pString, int iMinimum) { return world_init_buffer(pString, iMinimum); }
static void world_init_release(void *pString, int iLength)
{ (void)pString; world_log("string.release %d\n", iLength); }
static void TC world_init_release_hook(void *pString, int iLength) { world_init_release(pString, iLength); }
static void world_init_string_destroy(void *pString)
{ (void)pString; world_log("string.destroy\n"); --s_uiStringLive; }
static void TC world_init_string_destroy_hook(void *pString) { world_init_string_destroy(pString); }
static int SC world_init_load(uint32 uiModule, uint32 uiId, char *pcBuffer, int iCapacity)
{ world_log("load module=%x id=%x capacity=%d\n", uiModule, uiId, iCapacity); strcpy(pcBuffer, "Fixture empty-path label"); return 24; }
static int world_init_load_service(uint32 uiModule, uint32 uiId, char *pcBuffer, int iCapacity) { return world_init_load(uiModule, uiId, pcBuffer, iCapacity); }
static int world_init_art(void *pControl, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiFlags)
{ world_log("art %x %x %x %x %x\n", world_offset(pControl), uiBitmap, world_offset(pDib), world_offset(pPalette), uiFlags); return 1; }
static int TC world_init_art_hook(void *pControl, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiFlags)
{ return world_init_art(pControl, uiBitmap, pDib, pPalette, uiFlags); }
static int world_init_button(void *pControl, void *pFont, void *pAlternate, uint32 uiBitmap, void *pDib, void *pPalette,
    uint32 uiFlags, int iColor, uint32 uiModule, int iX, int iY, struct fs98_world_rect stRect)
{
    world_log("button %x font=%x/%x bitmap=%x dib=%x palette=%x flags=%x color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        world_offset(pControl), world_offset(pFont), world_offset(pAlternate), uiBitmap, world_offset(pDib), world_offset(pPalette), uiFlags,
        (uint32)iColor, uiModule, iX, iY, stRect.iLeft, stRect.iTop, stRect.iRight, stRect.iBottom); return 1;
}
static int TC world_init_button_hook(void *pControl, void *pFont, void *pAlternate, uint32 uiBitmap, void *pDib, void *pPalette,
    uint32 uiFlags, int iColor, uint32 uiModule, int iX, int iY, struct fs98_world_rect stRect)
{ return world_init_button(pControl, pFont, pAlternate, uiBitmap, pDib, pPalette, uiFlags, iColor, uiModule, iX, iY, stRect); }
static void world_init_text(void *pControl, void *pFont) { world_log("init.text %x %x\n", world_offset(pControl), world_offset(pFont)); }
static void TC world_init_text_hook(void *pControl, void *pFont) { world_init_text(pControl, pFont); }
static void world_init_combo(void *pControl, void *pFont, void *pLabel) { world_log("init.combo %x %x %x\n", world_offset(pControl), world_offset(pFont), world_offset(pLabel)); }
static void TC world_init_combo_hook(void *pControl, void *pFont, void *pLabel) { world_init_combo(pControl, pFont, pLabel); }
static void *SC world_init_font(int iFont) { world_log("font %d\n", iFont); return s_stFixture.abyAfter; }
static void *world_init_font_service(int iFont) { return world_init_font(iFont); }
static void world_init_edit(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground, uint32 uiError, void *pLabel, uint32 uiFlags)
{ world_log("init.edit %x font=%x colors=%x/%x/%x label=%x flags=%x\n", world_offset(pControl), world_offset(pFont), uiForeground, uiBackground, uiError, world_offset(pLabel), uiFlags); }
static void TC world_init_edit_hook(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground, uint32 uiError, void *pLabel, uint32 uiFlags)
{ world_init_edit(pControl, pFont, uiForeground, uiBackground, uiError, pLabel, uiFlags); }
static int TC world_init_check(void *pControl, void *pFont, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiModule)
{ world_log("init.check %x font=%x bitmap=%x dib=%x palette=%x module=%x\n", world_offset(pControl), world_offset(pFont), uiBitmap, world_offset(pDib), world_offset(pPalette), uiModule); return 1; }
static int world_init_enable(void *pControl, int iEnabled)
{
    int iResult = world_enable(pControl, iEnabled);
    if ((uint8 *)pControl == s_stFixture.abyObject + 0x11a0 && (s_uiScenario & 8))
        U32(s_stFixture.abyObject, 0x10e0) ^= 1;
    return iResult;
}
static int TC world_init_enable_hook(void *pControl, int iEnabled) { return world_init_enable(pControl, iEnabled); }
static struct fs98_world_services s_stInitEventServices;
static const struct fs98_world_init_services s_stInitServices = {
    world_init_base, world_init_string_construct, world_init_buffer, world_init_release, world_init_string_destroy,
    world_init_load_service, world_init_art, world_init_button, world_init_text, world_init_combo, world_init_font_service, world_init_edit, world_set_text
};
static void world_init_land(void)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiInitSeh) : "memory"); __builtin_longjmp(s_apJump, 1); }
static EXCEPTION_DISPOSITION __cdecl world_init_catch(EXCEPTION_RECORD *pRecord, void *pFrame, CONTEXT *pContext, void *pDispatcher)
{
    (void)pContext; (void)pDispatcher;
    if (pRecord->ExceptionCode == THROW_CODE && !(pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(pFrame, NULL, pRecord, NULL); s_uiThrowAt = 0; world_log("caught\n");
        if (!s_iSide) ++s_uiThrows;
        world_init_land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) void world_init_run(uint32 uiThrowAt)
{
    struct { void *pPrevious, *pHandler; } stCatch;
    int iResult;
    s_stFixture = s_stInitial; s_uiStringLive = 0; s_acString[0] = 0;
    s_uiCalls = s_uiLength = 0; s_aacLogs[s_iSide][0] = 0; s_uiThrowAt = uiThrowAt;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_uiInitSeh));
    if (__builtin_setjmp(s_apJump) == 0) {
        stCatch.pPrevious = (void *)s_uiInitSeh; stCatch.pHandler = world_init_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&stCatch) : "memory");
        iResult = s_iSide ? fs98_world_area_init(s_stFixture.abyObject) :
            ((int (TC *)(void *))((uint8 *)s_hRetail + 0xbef0))(s_stFixture.abyObject);
        world_log("return %d\n", iResult);
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiInitSeh) : "memory");
}
static int world_init_compare(uint32 uiThrowAt)
{
    s_iSide = 0; world_init_run(uiThrowAt); s_stReference = s_stFixture; s_uiStringReference = s_uiStringLive;
    s_iSide = 1; world_init_run(uiThrowAt); ++s_uiComparisons;
    if (s_uiStringReference == s_uiStringLive && !memcmp(&s_stReference, &s_stFixture, sizeof(s_stFixture)) && !strcmp(s_aacLogs[0], s_aacLogs[1])) return 0;
    fprintf(stderr, "WORLD INIT DIFFER scenario=%u throw=%u live=%u/%u\nRETAIL:\n%sOURS:\n%s", s_uiScenario, uiThrowAt, s_uiStringReference, s_uiStringLive, s_aacLogs[0], s_aacLogs[1]); return 1;
}
int main(int iArgc, char **ppcArgv)
{
    uint32 uiCount = iArgc > 1 ? strtoul(ppcArgv[1], NULL, 0) : 2000, uiIndex, uiLimit;
    int iUnwind = iArgc > 2 && !strcmp(ppcArgv[2], "unwind");
    s_uiRng = 0x4a971b63;
    SetErrorMode(SEM_NOGPFAULTERRORBOX); s_hRetail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_hRetail || GetProcAddress(s_hRetail, "Linkage") != (FARPROC)((uint8 *)s_hRetail + 0x24120)) return 2;
    world_patch(0x255bc, world_hook_set); world_patch(0x255c8, world_init_enable_hook); world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send); world_patch(0x256b8, world_hook_format);
    world_patch(0x257c0, world_init_base_hook); world_patch(0x255b4, world_init_string_construct_hook);
    world_patch(0x25588, world_init_buffer_hook); world_patch(0x2558c, world_init_release_hook); world_patch(0x255c0, world_init_string_destroy_hook);
    world_patch(0x25890, world_init_load); world_patch(0x257c4, world_init_art_hook); world_patch(0x257c8, world_init_button_hook);
    world_patch(0x257dc, world_init_text_hook); world_patch(0x25790, world_init_combo_hook); world_patch(0x257d0, world_init_font);
    world_patch(0x25740, world_init_edit_hook); world_patch(0x257e0, world_hook_set);
    world_patch(0x256bc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    s_apVtable[0xe4 / 4] = world_init_skin; s_apCheckVtable[0xc4 / 4] = world_init_check;
    s_stInitEventServices = s_stServices; s_stInitEventServices.pfnEnable = world_init_enable;
    fs98_world_set_services(&s_stInitEventServices); fs98_world_set_init_services(&s_stInitServices);
    U32(s_hRetail, 0x2495c) = 0x27182818; fs98_world_set_resource_module(0x27182818);
    for (s_uiScenario = 0; s_uiScenario < uiCount; ++s_uiScenario) {
        world_initialize(); s_uiOperation = 7;
        for (uiIndex = 0; uiIndex < 2; ++uiIndex) {
            if (s_uiScenario & 4) s_stFixture.aacPaths[uiIndex][0] = 0;
            PTR(s_stFixture.aabyDescriptors[uiIndex], 8) = "Fixture title";
            U32(s_stFixture.aabyRows[uiIndex], 8) = 160; U32(s_stFixture.aabyRows[uiIndex], 0x1c) = 160;
        }
        U32(s_stFixture.abyObject, 0x10e0) = 1;
        PTR(s_stFixture.abyObject, 0x1a50) = s_apCheckVtable;
        s_stInitial = s_stFixture;
        if (world_init_compare(0)) return 1;
        uiLimit = s_uiCalls - 1;
        if (iUnwind) for (uiIndex = 1; uiIndex <= uiLimit; ++uiIndex) if (world_init_compare(uiIndex)) return 1;
    }
    printf("World initializer: %u scenarios, %u comparisons, %u real unwinds, 0 differ\n", uiCount, s_uiComparisons, s_uiThrows);
    return 0;
}
