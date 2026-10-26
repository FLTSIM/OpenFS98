/* World.DLL 0x223fc2c0..0x223fc811: shared add/edit scenery-area events.
 * Offsets describe the retail x86 ABI; services form the differential boundary.
 * Do not cache descriptor/vtable loads across callbacks unless retail does. */
#include "world_internal.h"
static const struct fs98_world_services *s_pServices;
typedef void (FS98_WORLD_THISCALL *world_close_fn)(void *pObject);
typedef int (FS98_WORLD_THISCALL *world_validate_fn)(void *pObject, uint32 uiValue);
typedef uint32 (FS98_WORLD_STDCALL *world_check_fn)(void *pDescriptor);
void fs98_world_set_services(const struct fs98_world_services *pServices)
{
    s_pServices = pServices;
}
static void world_check(void *pObject)
{
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    ((world_check_fn)WORLD_PTR(pDescriptor, 4))(pDescriptor);
}
/* 0x223fc360 */
void fs98_world_area_collect(void *pObject)
{
    char acPosition[0x20];
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    void *pRow = WORLD_PTR(pDescriptor, 0xc);
    int iSelection;
    s_pServices->pfnGetText((uint8 *)pObject + 0x19a0, acPosition, 0x1f);
    iSelection = s_pServices->pfnParseInt(acPosition);
    WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0x14) = (uint32)iSelection;
    s_pServices->pfnGetText((uint8 *)pObject + 0x1938, WORLD_PTR(pRow, 4), 0x80);
    WORLD_U32(pRow, 0x34) = WORLD_U32(pObject, 0x1b0c);
    iSelection = s_pServices->pfnSend(WORLD_U32(pObject, 0x1a28), 0x147, 0, 0);
    if ((uint32)iSelection <= 4)
        WORLD_U32(pRow, 0x2c) = (uint32)(iSelection + 1) * 10;
    s_pServices->pfnGetText((uint8 *)pObject + 0x1258, WORLD_PTR(pRow, 0x18), 0x80);
}
/* 0x223fc440 */
void fs98_world_area_populate(void *pObject)
{
    char acPosition[0x20];
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    void *pRow = WORLD_PTR(pDescriptor, 0xc);
    uint32 uiLayer;
    s_pServices->pfnFormatInt(acPosition, "%04d", (int)WORLD_U32(pDescriptor, 0x14));
    uiLayer = WORLD_U32(pRow, 0x2c);
    if (uiLayer >= 10 && uiLayer <= 50 && uiLayer % 10 == 0)
        s_pServices->pfnSend(WORLD_U32(pObject, 0x1a28), 0x14e, uiLayer / 10 - 1, 0);
    s_pServices->pfnCheck((uint8 *)pObject + 0x1a50, (int)WORLD_U32(pRow, 0x34));
    s_pServices->pfnSetText((uint8 *)pObject + 0x19a0, acPosition);
    s_pServices->pfnSetText((uint8 *)pObject + 0x1938, WORLD_PTR(pRow, 4));
    s_pServices->pfnSetText((uint8 *)pObject + 0x1258, WORLD_PTR(pRow, 0x18));
    s_pServices->pfnSend(WORLD_U32(pObject, 0x1958), 0xcf, WORLD_U32(pRow, 0x3c), 0);
    s_pServices->pfnSend(WORLD_U32(pObject, 0x1278), 0xcf, WORLD_U32(pRow, 0x3c), 0);
    s_pServices->pfnEnable((uint8 *)pObject + 0x1a50, WORLD_U32(pRow, 0x3c) == 0);
    s_pServices->pfnEnable((uint8 *)pObject + 0x1a08, WORLD_U32(pRow, 0x3c) == 0);
    s_pServices->pfnEnable((uint8 *)pObject + 0x1710, WORLD_U32(pRow, 0x3c) == 0);
}
/* 0x223fc2c0 */
void fs98_world_area_cancel(void *pObject)
{
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    void **ppVtable = WORLD_PTR(pObject, 0);
    WORLD_U32(pDescriptor, 0) = 0;
    ((world_close_fn)ppVtable[0xec / 4])(pObject);
}
/* 0x223fc2e0: keep the vtable used before validate, including its error/close. */
void fs98_world_area_accept(void *pObject)
{
    void *pDescriptor, *pRow;
    void **ppVtable;
    int iError;
    fs98_world_area_collect(pObject);
    world_check(pObject);
    pDescriptor = WORLD_PTR(pObject, 0x10e4);
    pRow = WORLD_PTR(pDescriptor, 0xc);
    if (WORLD_U32(pRow, 0) != 0) {
        s_pServices->pfnError(pObject, (uint8 *)pObject + 0x1938, (void *)WORLD_U32(pRow, 0));
        s_pServices->pfnFocusControl((uint8 *)pObject + 0x1258);
        return;
    }
    ppVtable = WORLD_PTR(pObject, 0);
    iError = ((world_validate_fn)ppVtable[0xd8 / 4])(pObject, 0);
    if (iError && ((world_validate_fn)ppVtable[0xdc / 4])(pObject, (uint32)iError))
        return;
    WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 1;
    ((world_close_fn)ppVtable[0xec / 4])(pObject);
}
/* 0x223fc610: title error wins over position error; path checked by OK. */
int fs98_world_area_validate(void *pObject, uint32 uiUnused)
{
    void *pDescriptor, *pRow;
    (void)uiUnused;
    fs98_world_area_collect(pObject);
    world_check(pObject);
    pDescriptor = WORLD_PTR(pObject, 0x10e4);
    pRow = WORLD_PTR(pDescriptor, 0xc);
    if (WORLD_U32(pRow, 0x14))
        return 0x1b07;
    return WORLD_U32(pDescriptor, 0x10) ? 0x1aef : 0;
}
/* 0x223fc660: uiUnused is ignored even when zero. The pointer at +0x1c
 * deliberately outlives this stack buffer in retail; only consumed in Error. */
int fs98_world_area_error(void *pObject, uint32 uiUnused)
{
    char acPosition[0x40];
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    void *pRow = WORLD_PTR(pDescriptor, 0xc);
    void **ppVtable;
    int iControl = 0;
    (void)uiUnused;
    if (WORLD_U32(pRow, 0x14)) {
        if (s_pServices->pfnError(pObject, (uint8 *)pObject + 0x1258, (void *)WORLD_U32(pRow, 0x14)))
            iControl = 0x1b07;
    } else if (WORLD_U32(pDescriptor, 0x10)) {
        s_pServices->pfnGetText((uint8 *)pObject + 0x19a0, acPosition, 0x40);
        pDescriptor = WORLD_PTR(pObject, 0x10e4);
        WORLD_U32(pDescriptor, 0x18) = WORLD_U32(pDescriptor, 0x10);
        WORLD_PTR(WORLD_PTR(pObject, 0x10e4), 0x1c) = acPosition;
        pDescriptor = WORLD_PTR(pObject, 0x10e4);
        WORLD_U32(pDescriptor, 0x20) = WORLD_U32(pDescriptor, 0x2c);
        pDescriptor = WORLD_PTR(pObject, 0x10e4);
        if (s_pServices->pfnError(pObject, (uint8 *)pObject + 0x19a0, (uint8 *)pDescriptor + 0x18))
            iControl = 0x1aef;
    }
    ppVtable = WORLD_PTR(pObject, 0);
    return ((world_validate_fn)ppVtable[0xe0 / 4])(pObject, (uint32)iControl);
}
/* 0x223fc720: no title/position collection here. */
void fs98_world_area_path_changed(void *pObject)
{
    static const uint32 s_auiControls[] = {0x11a0, 0x1258, 0x1710, 0x1a08, 0x1a50, 0x17c8, 0x19a0};
    void *pDescriptor = WORLD_PTR(pObject, 0x10e4);
    void *pRow = WORLD_PTR(pDescriptor, 0xc);
    uint32 uiIndex;
    int iEnabled;
    s_pServices->pfnGetText((uint8 *)pObject + 0x1938, WORLD_PTR(pRow, 4), 0x80);
    world_check(pObject);
    pDescriptor = WORLD_PTR(pObject, 0x10e4);
    pRow = WORLD_PTR(pDescriptor, 0xc);
    iEnabled = WORLD_U32(pRow, 0) == 0;
    for (uiIndex = 0; uiIndex < sizeof(s_auiControls) / sizeof(s_auiControls[0]); ++uiIndex)
        s_pServices->pfnEnable((uint8 *)pObject + s_auiControls[uiIndex], iEnabled);
}

const struct fs98_world_services *fs98_world_get_services(void) { return s_pServices; }
