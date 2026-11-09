/* Startup: the static initialisers the C runtime runs before WinMain. */
#include "host_rt.h"

/* 0x413220 (a jump to 0x413230): four words from .rdata (0x416180) into 0x41a508. */
void fs98_host_rt_413220(void)
{
    G32(0x41a508) = G32(0x416180);
    G32(0x41a50c) = G32(0x416184);
    G32(0x41a510) = G32(0x416188);
    G32(0x41a514) = G32(0x41618c);
}

/* ---- the C runtime's exit list (VC5's onexit wrapper) -------------------------------------
 * 0x41c5c8/0x41c5c4 are the module's own exit table's end and start; -1 (as the startup sets
 * them in an executable) hands the function to MSVCRT's list instead. */

/* 0x414c70: _onexit, into the module's table or MSVCRT's. */
void *fs98_host_rt_onexit(void *fn)
{
    if (G32(0x41c5c8) == 0xffffffffu)
        return CALL_C(_onexit, void *, (void *))(fn);
    return CALL_C(__dllonexit, void *, (void *, void *, void *))(fn, VA(0x41c5c8), VA(0x41c5c4));
}

/* 0x414ca0: atexit: 0 when registered, else -1. */
int32_t fs98_host_rt_atexit(void *fn)
{
    return fs98_host_rt_onexit(fn) != 0 ? 0 : -1;
}

/* ---- MFC's module state and the globals' constructors --------------------------------------
 * The C runtime runs the three constructors 0x41504a, 0x401000 and 0x4010b0, in that order,
 * before WinMain (MFC's AfxWinMain, import #1576). */

/* 0x41500e: MFC's module state (import #1168) told whether this is a DLL and the MFC version
 * it was built for; an executable's multibyte code page set to the ANSI one (-3). 1. */
int32_t SC fs98_host_rt_afx_initialize(int32_t dll, uint32_t version)
{
    uint8_t *state = CALL_C(MFC_1168, uint8_t *, (void))();
    D8(state, 0x14) = (uint8_t)dll;
    D32(state, 0x1040) = version;
    if (dll == 0)
        CALL_C(_setmbcp, int32_t, (int32_t))(-3);
    return 1;
}

/* 0x41504a (through 0x415037): the global at 0x41a9a0, whose constructor initialises MFC's
 * module state for an executable built for MFC 4.21. */
void *fs98_host_rt_construct_afx(void)
{
    fs98_host_rt_afx_initialize(0, 0x421);
    return VA(0x41a9a0);
}

/* 0x401010: the global at 0x418798 constructed (import #567). */
uint32_t fs98_host_rt_construct_418798(void)
{
    return CALL_THIS(MFC_567, uint32_t, (void *))(VA(0x418798));
}

/* 0x401030: and destroyed (import #818). */
uint32_t fs98_host_rt_destroy_418798(void)
{
    return CALL_THIS(MFC_818, uint32_t, (void *))(VA(0x418798));
}

/* 0x401000: the global at 0x418798's constructor, and its destructor registered. */
int32_t fs98_host_rt_static_418798(void)
{
    fs98_host_rt_construct_418798();
    return fs98_host_rt_atexit((void *)fs98_host_rt_destroy_418798);
}

/* 0x401040: what import #4274 is (its import slot's value, read as data). */
void *fs98_host_rt_401040(void)
{
    return fs98_host_rt_imports[HOST_RT_IMPORT_MFC_4274];
}

/* 0x401050: the application's runtime class (0x416000). */
void *fs98_host_rt_application_class(void)
{
    return VA(0x416000);
}

/* 0x401060 (reached from 0x4010c0): the application constructed: CWinApp's constructor
 * (import #561, no name), then the application's vtable (0x416020). */
static void *TC construct_application(void *application)
{
    CALL_THIS(MFC_561, void *, (void *, const char *))(application, 0);
    PTR(application, 0) = VA(0x416020);
    return application;
}

/* 0x4010a0: the application destroyed (CWinApp's destructor, import #815). */
uint32_t TC fs98_host_rt_destroy_application(void *application)
{
    return CALL_THIS(MFC_815, uint32_t, (void *))(application);
}

/* 0x401080: its scalar deleting destructor: destroyed, and freed if flags bit 0. */
void *TC fs98_host_rt_delete_application(void *application, uint32_t flags)
{
    fs98_host_rt_destroy_application(application);
    if (flags & 1)
        CALL_C(operator_delete, void, (void *))(application);
    return application;
}

/* 0x4010c0: the application, the global at 0x4185c8, constructed. */
void *fs98_host_rt_construct_application(void)
{
    return construct_application(VA(0x4185c8));
}

/* 0x4010e0: and destroyed. */
uint32_t fs98_host_rt_destroy_the_application(void)
{
    return fs98_host_rt_destroy_application(VA(0x4185c8));
}

/* 0x4010b0: the application's constructor, and its destructor registered. */
int32_t fs98_host_rt_static_application(void)
{
    fs98_host_rt_construct_application();
    return fs98_host_rt_atexit((void *)fs98_host_rt_destroy_the_application);
}
