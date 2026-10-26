/* hostdiff -- differential test of the HOST reimplementation (module id 1,
 * FLTSIM98.EXE) against the real host, both in one process.
 *
 * The host is a reloc-stripped executable that must sit at 0x400000, which wine
 * keeps committed and will not hand out or free.  So we do not load it: we take
 * the region it already owns, make it executable, and copy the file's sections
 * into place (see host.md).  Its service table then stands at 0x417ba0 with its
 * code pointers (absolute 0x4xxxxx) valid, and we can call any pure service.
 *
 * Our side is HOST_ours.dll, a normal relocatable DLL that exports its own
 * service table (HostTable) and its .data image (host_image).  Our image is a
 * byte-for-byte copy of the host's .data, so both sides start identical: after
 * a call the whole .data compares equal unless our logic wrote something the
 * host did not.  A returned (or written-back) pointer into either side's .data
 * is normalised to an offset before comparing.
 *
 * The harness itself is linked at 0x13000000 so it does not occupy 0x400000.
 *
 * Usage: hostdiff [N]        (from bin/difftest, after `make hostdiff`)
 */
#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define REAL_BASE   0x400000u
#define DATA_VA     0x417000u
#define DATA_SIZE   0x55ccu
#define DATA_END    (DATA_VA + DATA_SIZE)
#define TABLE_VA    0x417ba0u

#define P(x) ((uint32_t)(uintptr_t)(x))

static uint32_t norm(uint32_t v);

typedef uint32_t (__stdcall *fn0)(void);
typedef uint32_t (__stdcall *fn1)(uint32_t);
typedef uint32_t (__stdcall *fn2)(uint32_t, uint32_t);

static uint32_t *real_tbl;
static void **our_tbl;
static uint8_t *our_data;               /* base of HOST_ours.dll's host_image */
static uint32_t rng = 0x1234567u;
static int differ;
static int trace;                       /* HOSTDIFF_TRACE: print each profile and file operation */
static int skip_files;                  /* HOSTDIFF_SKIP_FILES: leave out the profile and file checks */

/* xorshift32: unlike an LCG, its low bits are as well-mixed as its high ones,
 * so decisions like rnd() & 1 and rnd() % n get full-period coverage. */
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* An import the harness could not bind.  Nothing tested should reach one. */
static void __stdcall unbound(void)
{
    printf("hostdiff: the real host called an unbound import\n");
    exit(3);
}

/* Both sides' realloc: always a new block, the old one poisoned before it is
 * freed, so a pointer kept across a move reads garbage rather than the
 * still-intact old contents. */
/* Both sides' malloc: the block filled with one pattern, so a field the host
 * never initialises reads the same on both sides. */
static void *__cdecl patterned_malloc(size_t size)
{
    void *p = malloc(size);
    if (p) memset(p, 0xa5, size);
    return p;
}

/* Both sides' free, counted, so a block a service should free and does not
 * shows. */
static uint32_t nfrees;
/* Blocks a free leaves in place: the in-memory sounds the sound test hands
 * the host, which frees them and goes on using them -- retail reads and
 * writes a freed sound's data, so it must stay the harness's, not the heap's. */
static uint8_t pinned_pool[2][64][256];
static void __cdecl counted_free(void *p)
{
    if (p) nfrees++;
    if ((uint8_t *)p >= &pinned_pool[0][0][0] && (uint8_t *)p < &pinned_pool[0][0][0] + sizeof pinned_pool) return;
    free(p);
}

static void *__cdecl moving_realloc(void *old, size_t size)
{
    size_t n;
    void *p;
    if (!old) return patterned_malloc(size);
    p = patterned_malloc(size);
    if (!p) return 0;
    n = _msize(old);
    memcpy(p, old, n < size ? n : size);
    memset(old, 0xdd, n);
    {   /* freed a while later, so the next blocks do not land on it: a pointer kept across the move keeps reading 0xdd */
        static void *quarantine[64];
        static uint32_t qn;
        free(quarantine[qn % 64]);
        quarantine[qn++ % 64] = old;
    }
    return p;
}

/* GetCommandLineA, both sides: a command line the harness makes up, with a
 * byte before it (the host's option finder reads one). */
static char fake_cl[512];
static char *__stdcall fake_GetCommandLineA(void) { return fake_cl + 1; }

/* Point one import of a loaded image at `fn`. */
static void hook_import(uint8_t *b, const char *dll, const char *name, void *fn)
{
    uint32_t pe = *(uint32_t *)(b + 0x3c);
    uint32_t *d = (uint32_t *)(b + *(uint32_t *)(b + pe + 0x80));
    for (; d[3] != 0; d += 5) {
        uint32_t *nm = (uint32_t *)(b + (d[0] ? d[0] : d[4]));
        uint32_t *iat = (uint32_t *)(b + d[4]);
        DWORD old;
        if (_stricmp((const char *)(b + d[3]), dll)) continue;
        for (; *nm != 0; nm++, iat++)
            if (!(*nm & 0x80000000u) && !strcmp((const char *)(b + *nm + 2), name)) {
                VirtualProtect(iat, 4, PAGE_READWRITE, &old);
                *iat = (uint32_t)(uintptr_t)fn;
                VirtualProtect(iat, 4, old, &old);
            }
    }
}

/* The window system and the clock, both sides: each call is logged (kind,
 * arguments, a string argument's text) into one log the harness clears
 * before each side's call and compares after; the results are made up and
 * the same for both. */
struct logged { uint32_t kind, a, b, c, d; char text[64]; };
static struct logged calls[32];
static uint32_t ncalls, fake_ticks, fake_tick_step;
static SYSTEMTIME fake_time;

static struct logged *log_call(uint32_t kind, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    struct logged *l = &calls[ncalls < 32 ? ncalls : 31];
    ncalls++;
    l->kind = kind; l->a = norm(a); l->b = norm(b); l->c = norm(c); l->d = norm(d); l->text[0] = 0;   /* .data pointers as offsets */
    return l;
}

static LRESULT WINAPI fake_SendMessageA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if ((m == 0x404 || m == 0x405) && l) {
        /* a TOOLINFO on the caller's stack: its fields, and the rectangle
         * its uId points at (each side's own heap block), not the pointers */
        const uint32_t *ti = (const uint32_t *)(uintptr_t)l, *r = (const uint32_t *)(uintptr_t)ti[3];
        struct logged *c = log_call(1, P(h), m, w, ti[2]);
        if (m == 0x404) memcpy(c->text, ti + 4, 24);
        if (r) memcpy(c->text + 24, r, 28);
        return 1;
    }
    log_call(1, P(h), m, w, (uint32_t)l);
    return 0x5e0d0000 + m;
}
static BOOL WINAPI fake_PostMessageA(HWND h, UINT m, WPARAM w, LPARAM l)
{ log_call(2, P(h), m, w, (uint32_t)l); return TRUE; }
static BOOL WINAPI fake_SetWindowTextA(HWND h, LPCSTR s)
{ struct logged *c = log_call(3, P(h), 0, 0, 0); if (s) { strncpy(c->text, s, 63); c->text[63] = 0; } return 7; }
static DWORD WINAPI fake_timeGetTime(void) { log_call(4, 0, 0, 0, 0); fake_ticks += fake_tick_step; return fake_ticks - fake_tick_step; }
static void WINAPI fake_GetLocalTime(SYSTEMTIME *t) { log_call(5, 0, 0, 0, 0); *t = fake_time; }
static int WINAPI fake_MessageBoxA(HWND h, LPCSTR text, LPCSTR cap, UINT f)
{ (void)h; (void)cap; (void)f; printf("hostdiff: a fatal message box: %.200s\n", text ? text : ""); exit(4); }

/* The joysticks and the registry, modelled: up to six WINMM ids, some
 * present, with caps and a registry key of their own; positions that change
 * after a scripted number of polls; and the keys the host reads the OEM name
 * through.  joy_reset() before each side's call replays the same script. */
static struct {
    uint32_t num, present[6], change_at[6], polls[6];
    JOYCAPSA caps[6];
    JOYINFOEX before[6], after[6];
} joy;
static const char *const joy_keys[] = { "Msjstick.drv<0000>", "Gameport<0001>", "Odd" };
static const char *const joy_oems[] = { "VID_045E&PID_0008", "Gravis", "Nobody" };
static const char *const joy_names[] = { "CH Flightstick", "Gravis GamePad", "", "Microsoft SideWinder 3D Pro", "Mystery" };
static uint32_t joy_reg_oem[6], joy_reg_name[3], joy_reg_has[6];

static void joy_reset(void) { memset(joy.polls, 0, sizeof joy.polls); }

static UINT WINAPI fake_joyGetNumDevs(void) { log_call(6, 0, 0, 0, 0); return joy.num; }
static MMRESULT WINAPI fake_joyGetPosEx(UINT id, LPJOYINFOEX ji)
{
    log_call(7, id, ji ? ji->dwFlags : 0, 0, 0);
    if (id >= 6 || !joy.present[id]) return JOYERR_UNPLUGGED;
    {
        DWORD size = ji->dwSize, flags = ji->dwFlags;
        *ji = joy.polls[id]++ >= joy.change_at[id] ? joy.after[id] : joy.before[id];
        ji->dwSize = size; ji->dwFlags = flags;
    }
    return 0;
}
static MMRESULT WINAPI fake_joyGetDevCapsA(UINT_PTR id, LPJOYCAPSA c, UINT size)
{
    log_call(8, (uint32_t)id, size, 0, 0);
    if (id >= 6 || !joy.present[id]) return JOYERR_PARMS;
    memcpy(c, &joy.caps[id], size < sizeof *c ? size : sizeof *c);
    return 0;
}
static void WINAPI fake_Sleep(DWORD ms) { log_call(9, ms, 0, 0, 0); }

/* Registry keys are handles 0x1000 + n: n < 3 a CurrentJoystickSettings key
 * (of joy_keys[n]), 0x10 + n an OEM key (of joy_oems[n]). */
static LONG WINAPI fake_RegOpenKeyExA(HKEY root, LPCSTR path, DWORD opt, REGSAM sam, PHKEY out)
{
    static char want[300];
    uint32_t n;
    struct logged *c = log_call(10, P(root), opt, sam, 0);
    strncpy(c->text, path ? path : "", 63);
    if (trace) printf("RegOpenKeyExA(%p, \"%s\")\n", (void *)root, path ? path : "(null)");
    for (n = 0; n < 3; n++) {
        sprintf(want, "System\\CurrentControlSet\\Control\\MediaResources\\Joystick\\%s\\CurrentJoystickSettings", joy_keys[n]);
        if (path && !strcmp(path, want) && n != 2) { *out = (HKEY)(uintptr_t)(0x1000 + n); return 0; }
        sprintf(want, "System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\Joystick\\OEM\\%s", joy_oems[n]);
        if (path && !strcmp(path, want) && n != 2) { *out = (HKEY)(uintptr_t)(0x1010 + n); return 0; }
    }
    return ERROR_FILE_NOT_FOUND;
}
static LONG WINAPI fake_RegQueryValueExA(HKEY h, LPCSTR name, LPDWORD r, LPDWORD type, LPBYTE data, LPDWORD size)
{
    uint32_t k = (uint32_t)(uintptr_t)h, id;
    const char *v = 0;
    struct logged *c = log_call(11, k, P(r), P(type), size ? *size : 0);
    strncpy(c->text, name ? name : "", 63);
    if (k >= 0x1000 && k < 0x1003 && name && sscanf(name, "Joystick%uOEMName", &id) == 1 && id >= 1 && id <= 6 && joy_reg_has[id - 1])
        v = joy_oems[joy_reg_oem[id - 1]];
    if (k >= 0x1010 && k < 0x1013 && name && !strcmp(name, "OEMName"))
        v = joy_names[joy_reg_name[k - 0x1010]];
    if (!v) return ERROR_FILE_NOT_FOUND;
    if (size && *size > strlen(v)) { strcpy((char *)data, v); *size = (DWORD)strlen(v) + 1; }
    return 0;
}
static LONG WINAPI fake_RegCloseKey(HKEY h) { log_call(12, P(h), 0, 0, 0); return 0; }

/* The mouse, disk space and temporary names, likewise. */
static uint32_t temp_seq, heap_n, heap_bad;
static DWORD WINAPI fake_GetProcessHeaps(DWORD n, PHANDLE h)
{ DWORD i; log_call(23, n, 0, 0, 0); for (i = 0; i < heap_n && i < n; i++) h[i] = (HANDLE)(uintptr_t)(0x100 + i); return heap_n; }
static BOOL WINAPI fake_HeapValidate(HANDLE h, DWORD f, LPCVOID p)
{ log_call(24, P(h), f, P(p), 0); return !((heap_bad >> (P(h) & 31)) & 1); }
static DWORD disk_spc, disk_bps, disk_free;
static BOOL WINAPI fake_GetCursorPos(POINT *p) { log_call(13, 0, 0, 0, 0); p->x = 123; p->y = 45; return TRUE; }
static BOOL WINAPI fake_GetClipCursor(RECT *r) { log_call(14, 0, 0, 0, 0); r->left = 1; r->top = 2; r->right = 640; r->bottom = 480; return TRUE; }
static int WINAPI fake_ShowCursor(BOOL show) { log_call(15, show, 0, 0, 0); return show ? 0 : -1; }
static BOOL WINAPI fake_SetCursorPos(int x, int y) { log_call(16, x, y, 0, 0); return TRUE; }
static HWND WINAPI fake_SetCapture(HWND h) { log_call(17, P(h), 0, 0, 0); return 0; }
static HWND WINAPI fake_GetCapture(void) { log_call(18, 0, 0, 0, 0); return (HWND)0x1234; }
static BOOL WINAPI fake_ClipCursor(const RECT *r)
{ log_call(19, r ? r->left : 0, r ? r->top : 0, r ? r->right : 0, r ? r->bottom : 0); return TRUE; }
static BOOL WINAPI fake_ReleaseCapture(void) { log_call(20, 0, 0, 0, 0); return TRUE; }
static BOOL WINAPI fake_GetDiskFreeSpaceA(LPCSTR root, LPDWORD spc, LPDWORD bps, LPDWORD freec, LPDWORD total)
{
    struct logged *c = log_call(21, 0, 0, 0, 0);
    strncpy(c->text, root ? root : "(null)", 63);
    if (disk_spc == 0) return FALSE;
    *spc = disk_spc; *bps = disk_bps; *freec = disk_free; *total = disk_free * 2;
    return TRUE;
}
static UINT WINAPI fake_GetTempFileNameA(LPCSTR path, LPCSTR prefix, UINT unique, LPSTR out)
{
    struct logged *c = log_call(22, unique, prefix ? (uint8_t)prefix[0] : 0xffff, 0, 0);
    strncpy(c->text, path ? path : "(null)", 63);
    if (temp_seq % 5 == 4) { temp_seq++; return 0; }
    sprintf(out, "%s\\%s%X.TMP", path, prefix, ++temp_seq);
    return temp_seq;
}

/* Menus: HMENUs are numbers from a counter the harness restarts for each
 * side; every menu call is logged with its arguments (and an item's text). */
static uint32_t menu_seq, is_window;
static HMENU WINAPI fake_CreateMenu(void) { log_call(25, menu_seq, 0, 0, 0); return (HMENU)(uintptr_t)(0x7000 + menu_seq++); }
static BOOL WINAPI fake_AppendMenuA(HMENU m, UINT f, UINT_PTR id, LPCSTR text)
{
    struct logged *c = log_call(26, P(m), f, (uint32_t)id, 0);
    if (text && !(f & MF_SEPARATOR)) { strncpy(c->text, text, 63); c->text[63] = 0; }
    return TRUE;
}
static BOOL WINAPI fake_DestroyMenu(HMENU m) { log_call(27, P(m), 0, 0, 0); return TRUE; }
static BOOL WINAPI fake_SetMenu(HWND h, HMENU m) { log_call(28, P(h), P(m), 0, 0); return TRUE; }
static BOOL WINAPI fake_DrawMenuBar(HWND h) { log_call(29, P(h), 0, 0, 0); return TRUE; }
static uint32_t dlg_alive[16], dlg_busy[16];     /* dialog windows 0x9300+k: IsWindow calls left alive, GWLP_USERDATA calls left 0 */
static BOOL WINAPI fake_IsWindow(HWND h)
{
    log_call(30, P(h), 0, 0, 0);
    if ((P(h) & ~0xfu) == 0x9300) { uint32_t k = P(h) & 0xf; if (dlg_alive[k]) { dlg_alive[k]--; return TRUE; } return FALSE; }
    return is_window;
}
static BOOL WINAPI fake_EnableMenuItem(HMENU m, UINT i, UINT f) { log_call(31, P(m), i, f, 0); return 0; }
static DWORD WINAPI fake_CheckMenuItem(HMENU m, UINT i, UINT f) { log_call(32, P(m), i, f, 0); return 0; }

/* Modules, faked: MA..MD with a Linkage each (id, init, deinit, a dialog
 * table on some) and an ImportTable (MA imports MB, MB imports MC, MD
 * imports MA and MB), and two language modules; LoadLibraryA matches the
 * base name.  The blocks are restored before each side's call, since the
 * host writes into them. */
static uint32_t fake_link[4][8], fake_imp[4][8];
static const uint32_t fake_dialogs[] = { 0x51, 0, 4, 0x52, 0, 0, 0, 0, 0 };
/* DirectPlay, by script: whether dplayx loads, which of its exports are
 * there, which calls fail, and what the lobby hands over. */
static struct { uint32_t no_lib, no_create, no_lobby, create_fail, qi_fail, lobby_fail, conn_size, flat; } dp_script;
static void *dp_create_ptr, *dp_lobby_ptr;
static const char *const fake_names[] = { "MA.DLL", "MB.DLL", "MC.DLL", "MD.DLL", "LANG1.FLL", "LANG2.FLL" };

static void WINAPI fake_init0(void) { log_call(40, 0, 0, 0, 0); }
static void WINAPI fake_init1(void) { log_call(40, 1, 0, 0, 0); }
static void WINAPI fake_init2(void) { log_call(40, 2, 0, 0, 0); }
static void WINAPI fake_init3(void) { log_call(40, 3, 0, 0, 0); }
static void WINAPI fake_fini0(void) { log_call(41, 0, 0, 0, 0); }
static void WINAPI fake_fini1(void) { log_call(41, 1, 0, 0, 0); }
static void WINAPI fake_fini2(void) { log_call(41, 2, 0, 0, 0); }
static void WINAPI fake_fini3(void) { log_call(41, 3, 0, 0, 0); }

static void modules_reset(void)
{
    static void (WINAPI *const inits[4])(void) = { fake_init0, fake_init1, fake_init2, fake_init3 };
    static void (WINAPI *const finis[4])(void) = { fake_fini0, fake_fini1, fake_fini2, fake_fini3 };
    uint32_t i;
    memset(fake_link, 0, sizeof fake_link); memset(fake_imp, 0, sizeof fake_imp);
    for (i = 0; i < 4; i++) {
        fake_link[i][0] = 0x40 + i;
        fake_link[i][1] = i == 2 ? 0 : P(inits[i]);
        fake_link[i][2] = i == 1 ? 0 : P(finis[i]);
        fake_link[i][5] = i & 1 ? P(fake_dialogs) : 0;
    }
    fake_imp[0][0] = 0x41;
    fake_imp[1][0] = 0x42;
    fake_imp[3][0] = 0x40; fake_imp[3][2] = 0x41;
}

static HMODULE WINAPI fake_LoadLibraryA(LPCSTR name)
{
    const char *base = name ? strrchr(name, '\\') : 0;
    uint32_t i;
    struct logged *c = log_call(42, 0, 0, 0, 0);
    base = base ? base + 1 : name ? name : "";
    strncpy(c->text, base, 63);
    for (i = 0; i < 6; i++)
        if (!_stricmp(base, fake_names[i])) return (HMODULE)(uintptr_t)(0x6000 + 0x10 * i);
    if (!_stricmp(base, "dplayx") && !dp_script.no_lib) return (HMODULE)0x6100;
    return 0;
}
static void *fake_DirectSoundCreate_ptr;
static FARPROC WINAPI fake_GetProcAddress(HMODULE h, LPCSTR name)
{
    uint32_t i = (P(h) - 0x6000) / 0x10;
    struct logged *c = log_call(43, P(h), 0, 0, 0);
    if (P(name) > 0xffff) strncpy(c->text, name, 63);
    if (P(h) == 0x6100 && P(name) > 0xffff) {
        if (!strcmp(name, "DirectPlayCreate")) return dp_script.no_create ? 0 : (FARPROC)(void *)dp_create_ptr;
        if (!strcmp(name, "DirectPlayLobbyCreateA")) return dp_script.no_lobby ? 0 : (FARPROC)(void *)dp_lobby_ptr;
        if (!strcmp(name, "DirectPlayEnumerateA")) return (FARPROC)(uintptr_t)0x6e6e0000;
        return 0;
    }
    if (P(name) > 0xffff && !strcmp(name, "DirectSoundCreate")) return (FARPROC)(void *)fake_DirectSoundCreate_ptr;
    if (P(h) < 0x6000 || i >= 4 || P(name) <= 0xffff) return 0;
    if (!strcmp(name, "Linkage")) return (FARPROC)(void *)fake_link[i];
    if (!strcmp(name, "ImportTable")) return fake_imp[i][0] ? (FARPROC)(void *)fake_imp[i] : 0;
    return 0;
}
static uint32_t fl_fail;
static BOOL WINAPI fake_FreeLibrary(HMODULE h) { log_call(44, P(h), 0, 0, 0); return !fl_fail; }

/* The cursor, and the host's purge hook (0x418834), as the harness's own. */
static HCURSOR WINAPI fake_LoadCursorA(HINSTANCE i, LPCSTR id) { log_call(45, P(i), P(id), 0, 0); return (HCURSOR)(uintptr_t)(0x9000 + (P(id) & 0xfff)); }
static HCURSOR WINAPI fake_SetCursor(HCURSOR c) { log_call(46, P(c), 0, 0, 0); return 0; }
static uint32_t __stdcall fake_purge(uint32_t code, uint32_t arg) { log_call(47, code, arg, 0, 0); return 0x77; }

/* DirectSound, faked: one IDirectSound and a pool of buffers, each call
 * logged; buffers are handed out from a counter restarted for each side, so
 * both get the same pointers.  Some calls fail, by a script also restarted
 * per side.  Lock gives the buffer's own memory; Unlock logs a checksum of
 * what was written. */
#define NBUF 24
struct fake_com { void **vtbl; uint32_t idx, status, mem_n; uint8_t mem[0x4000]; };
static struct fake_com fake_ds, fake_bufs[NBUF];
static uint32_t buf_next, com_calls, com_fail_mask[4], com_err_shift;
static const uint32_t com_errs[] = { 0x80070057u, 0x8878001eu, 0x88780046u, 0x88780032u, 0x88780096u, 0x80004005u };

static uint32_t com_result(void)
{
    uint32_t k = com_calls++;
    if (k < 128 && (com_fail_mask[k / 32] >> (k % 32)) & 1) return com_errs[(k + com_err_shift) % 6];
    return 0;
}
static uint32_t com_idx(void *o) { return o == &fake_ds ? 0xd5 : ((struct fake_com *)o)->idx; }

static uint32_t WINAPI c_qi(void *o, void *a, void *b) { log_call(60, com_idx(o), 0, 0, 0); (void)a; (void)b; return 0x80004002u; }
static uint32_t WINAPI c_addref(void *o) { log_call(61, com_idx(o), 0, 0, 0); return 1; }
static uint32_t WINAPI c_release(void *o) { log_call(62, com_idx(o), 0, 0, 0); return 0; }
static uint32_t WINAPI c_two(void *o, uint32_t a) { log_call(63, com_idx(o), a, 0, 0); return com_result(); }
static uint32_t WINAPI c_three(void *o, uint32_t a, uint32_t b) { log_call(64, com_idx(o), a, b, 0); return com_result(); }
static uint32_t WINAPI c_four(void *o, uint32_t a, uint32_t b, uint32_t c) { log_call(65, com_idx(o), a, b, c); return com_result(); }
static uint32_t WINAPI c_one(void *o) { log_call(66, com_idx(o), 0, 0, 0); return com_result(); }

static struct fake_com *new_buffer(void)
{
    struct fake_com *b = &fake_bufs[buf_next++ % NBUF];
    b->status = 0; b->mem_n = 0;
    return b;
}
/* IDirectSound */
static uint32_t WINAPI ds_create(void *o, const uint32_t *desc, struct fake_com **out, void *u)
{
    uint32_t r;
    struct logged *c = log_call(70, com_idx(o), desc ? desc[1] : 0, desc ? desc[2] : 0, 0);
    if (desc && desc[4]) memcpy(c->text, (void *)(uintptr_t)desc[4], 0x12);
    (void)u;
    r = com_result();
    *out = r ? 0 : new_buffer();
    return r;
}
static uint32_t WINAPI ds_dup(void *o, struct fake_com *orig, struct fake_com **out)
{
    uint32_t r = com_result();
    log_call(71, com_idx(o), orig ? com_idx(orig) : 0xffff, 0, 0);
    *out = r ? 0 : new_buffer();
    return r;
}
/* IDirectSoundBuffer */
static uint32_t WINAPI b_getcaps(struct fake_com *o, uint32_t *caps)
{ log_call(72, o->idx, 0, 0, 0); caps[1] = o->idx & 1 ? 4 : 8; return com_result(); }
static uint32_t WINAPI b_status(struct fake_com *o, uint32_t *st) { log_call(73, o->idx, 0, 0, 0); *st = o->status; return com_result(); }
static uint32_t WINAPI b_lock(struct fake_com *o, uint32_t off, uint32_t n, void **p1, uint32_t *n1, void **p2, uint32_t *n2, uint32_t f)
{
    uint32_t r = com_result();
    log_call(74, o->idx, off, n, f);
    if (r) return r;
    if (n > sizeof o->mem) n = sizeof o->mem;
    memset(o->mem, 0xcc, sizeof o->mem);
    *p1 = o->mem; *n1 = n; if (p2) *p2 = 0; if (n2) *n2 = 0;
    o->mem_n = n;
    return 0;
}
static uint32_t WINAPI b_unlock(struct fake_com *o, void *p1, uint32_t n1, void *p2, uint32_t n2)
{
    uint32_t i, sum = 0;
    for (i = 0; i < o->mem_n; i++) sum = sum * 31 + o->mem[i];
    log_call(75, o->idx, p1 == o->mem, n1, sum);
    (void)p2; (void)n2;
    return com_result();
}
static void *ds_vtbl[11], *buf_vtbl[21];

static void com_init(void)
{
    uint32_t i;
    for (i = 0; i < 11; i++) ds_vtbl[i] = (void *)c_two;
    ds_vtbl[0] = (void *)c_qi; ds_vtbl[1] = (void *)c_addref; ds_vtbl[2] = (void *)c_release;
    ds_vtbl[3] = (void *)ds_create; ds_vtbl[5] = (void *)ds_dup; ds_vtbl[6] = (void *)c_three;
    ds_vtbl[7] = (void *)c_one;
    for (i = 0; i < 21; i++) buf_vtbl[i] = (void *)c_two;
    buf_vtbl[0] = (void *)c_qi; buf_vtbl[1] = (void *)c_addref; buf_vtbl[2] = (void *)c_release;
    buf_vtbl[3] = (void *)b_getcaps; buf_vtbl[4] = (void *)c_three; buf_vtbl[5] = (void *)c_four;
    buf_vtbl[9] = (void *)b_status; buf_vtbl[10] = (void *)c_three; buf_vtbl[11] = (void *)b_lock;
    buf_vtbl[12] = (void *)c_four; buf_vtbl[18] = (void *)c_one; buf_vtbl[19] = (void *)b_unlock;
    buf_vtbl[20] = (void *)c_one;
    fake_ds.vtbl = ds_vtbl;
    for (i = 0; i < NBUF; i++) { fake_bufs[i].vtbl = buf_vtbl; fake_bufs[i].idx = i; }
}

static uint32_t WINAPI fake_DirectSoundCreate(void *guid, struct fake_com **out, void *u)
{
    uint32_t r = com_result();
    log_call(76, P(guid), 0, 0, 0); (void)u;
    *out = r ? 0 : &fake_ds;
    return r;
}

/* MSACM32: a stream that "converts" by a fixed pattern, sizes four times. */
static MMRESULT WINAPI fake_acmStreamOpen(LPHACMSTREAM has, HACMDRIVER d, LPWAVEFORMATEX src, LPWAVEFORMATEX dst,
                                         LPWAVEFILTER f, DWORD_PTR cb, DWORD_PTR inst, DWORD fl)
{
    uint32_t r = com_result() ? 0x200 : 0;
    log_call(80, src ? src->wFormatTag : 0, dst ? dst->nBlockAlign : 0, dst ? dst->nAvgBytesPerSec : 0, fl);
    (void)d; (void)f; (void)cb; (void)inst;
    *has = r ? 0 : (HACMSTREAM)0xacacu;
    return r;
}
static MMRESULT WINAPI fake_acmStreamSize(HACMSTREAM h, DWORD in, LPDWORD out, DWORD fl)
{ uint32_t r = com_result() ? 0x200 : 0; log_call(81, P(h), in, fl, 0); if (!r) *out = in * 4; return r; }
static MMRESULT WINAPI fake_acmStreamPrepareHeader(HACMSTREAM h, LPACMSTREAMHEADER hd, DWORD fl)
{ uint32_t r = com_result() ? 0x200 : 0; log_call(82, P(h), hd->cbSrcLength, hd->cbDstLength, fl); return r; }
static MMRESULT WINAPI fake_acmStreamConvert(HACMSTREAM h, LPACMSTREAMHEADER hd, DWORD fl)
{
    uint32_t r = com_result() ? 0x200 : 0, i;
    log_call(83, P(h), hd->cbSrcLength, hd->cbDstLength, fl);
    if (!r) for (i = 0; i < hd->cbDstLength; i++) hd->pbDst[i] = hd->pbSrc[i / 4] ^ (uint8_t)i;
    return r;
}
static MMRESULT WINAPI fake_acmStreamUnprepareHeader(HACMSTREAM h, LPACMSTREAMHEADER hd, DWORD fl)
{ log_call(84, P(h), hd->cbSrcLength, fl, 0); return 0; }
static MMRESULT WINAPI fake_acmStreamClose(HACMSTREAM h, DWORD fl) { log_call(85, P(h), fl, 0, 0); return 0; }

/* GDI and window calls for the view windows, and a DirectDraw surface whose
 * Lock gives fixed memory (failing, and Restore failing, by the script). */
static uint8_t fake_bits[0x400], fake_ddbits[0x400];
static uint32_t gp_main_mask, surf_fail;
static int WINAPI fake_GetObjectA(HGDIOBJ h, int n, void *out)
{
    log_call(90, P(h), n, 0, 0);
    if (n >= 0x18) { memset(out, 0, 0x18); ((uint32_t *)out)[2] = P(h) & 0xf; }   /* bmHeight from the handle */
    return n;
}
static BOOL WINAPI fake_DeleteObject(HGDIOBJ h) { log_call(91, P(h), 0, 0, 0); return TRUE; }
static BOOL WINAPI fake_DeleteDC(HDC h) { log_call(92, P(h), 0, 0, 0); return TRUE; }
static BOOL WINAPI fake_DestroyWindow(HWND h) { log_call(93, P(h), 0, 0, 0); return TRUE; }
static HDWP WINAPI fake_BeginDeferWindowPos(int n) { log_call(94, n, 0, 0, 0); return (HDWP)0xdef0; }
static HDWP WINAPI fake_DeferWindowPos(HDWP d, HWND h, HWND after, int x, int y, int cx, int cy, UINT f)
{ log_call(95, P(d), P(h), P(after), f); (void)x; (void)y; (void)cx; (void)cy; return (HDWP)(uintptr_t)(P(d) + 1); }
static BOOL WINAPI fake_EndDeferWindowPos(HDWP d) { log_call(96, P(d), 0, 0, 0); return TRUE; }
static HWND WINAPI fake_GetParent(HWND h)
{ log_call(97, P(h), 0, 0, 0); return (gp_main_mask >> (P(h) & 31)) & 1 ? (HWND)(uintptr_t)*(uint32_t *)(DATA_VA + 0x180c) : (HWND)0x4444; }

struct fake_surface { void **vtbl; };
static struct fake_surface fake_surf;
static void *surf_vtbl[34];
static uint32_t WINAPI s_lock(void *o, void *rect, uint32_t *desc, uint32_t flags, void *ev)
{
    uint32_t r = surf_fail & 1;
    log_call(98, P(rect), desc ? desc[0] : 0, flags, P(ev)); (void)o;
    surf_fail >>= 1;
    if (r) return 0x887601c2u;
    desc[9] = P(fake_ddbits); desc[4] = 0x40;
    return 0;
}
static uint32_t WINAPI s_restore(void *o) { uint32_t r = surf_fail & 1; surf_fail >>= 1; log_call(99, 0, 0, 0, 0); (void)o; return r ? 0x80004005u : 0; }
static uint32_t WINAPI s_unlock(void *o, void *rect) { log_call(100, P(rect), 0, 0, 0); (void)o; return 0; }
static uint32_t WINAPI s_other(void *o) { log_call(101, 0, 0, 0, 0); (void)o; return 0; }
static uint32_t WINAPI s_getdc(void *o, uint32_t *dc) { log_call(140, *dc, 0, 0, 0); *dc = 0xdd00; (void)o; return 0; }
static uint32_t WINAPI s_releasedc(void *o, uint32_t dc) { log_call(141, dc, 0, 0, 0); (void)o; return 0; }

/* Text: fonts, colours and DrawTextA (which, measuring, moves the
 * rectangle's corner), all logged; font handles counted per side. */
static uint32_t font_seq;
static HFONT WINAPI fake_CreateFontA(int h, int w, int e, int o, int wt, DWORD it, DWORD ul, DWORD so, DWORD cs,
                                     DWORD op, DWORD cp, DWORD q, DWORD pf, LPCSTR face)
{
    struct logged *c = log_call(142, (uint32_t)h, (uint32_t)w, (uint32_t)e, (uint32_t)o);
    if (face) { strncpy(c->text, face, 63); c->text[63] = 0; }
    log_call(143, (uint32_t)wt, it | ul << 8 | so << 16, cs | op << 8 | cp << 16 | q << 24, pf);
    return (HFONT)(uintptr_t)(0xf000 + font_seq++);
}
static HGDIOBJ WINAPI fake_GetStockObject(int i) { log_call(144, (uint32_t)i, 0, 0, 0); return (HGDIOBJ)(uintptr_t)(0x5700 + i); }
static HGDIOBJ WINAPI fake_SelectObject(HDC dc, HGDIOBJ h) { log_call(145, P(dc), P(h), 0, 0); return (HGDIOBJ)(uintptr_t)((P(dc) ^ P(h)) & 0xfffff); }
static int WINAPI fake_DrawTextA(HDC dc, LPCSTR t, int n, LPRECT r, UINT f)
{
    struct logged *c = log_call(146, P(dc), (uint32_t)n, f, 0);
    if (t) { strncpy(c->text, t, 63); c->text[63] = 0; }
    log_call(147, (uint32_t)r->left, (uint32_t)r->top, (uint32_t)r->right, (uint32_t)r->bottom);
    if (f & DT_CALCRECT) { r->right = r->left + (LONG)strlen(t) * 6; r->bottom = r->top + 13; }
    return 13;
}
static COLORREF WINAPI fake_SetTextColor(HDC dc, COLORREF c) { log_call(148, P(dc), c, 0, 0); return 0; }
static COLORREF WINAPI fake_SetBkColor(HDC dc, COLORREF c) { log_call(149, P(dc), c, 0, 0); return 0; }
static int WINAPI fake_SetBkMode(HDC dc, int m) { log_call(150, P(dc), (uint32_t)m, 0, 0); return 0; }
static uint32_t fake_acp;
static UINT WINAPI fake_GetACP(void) { log_call(155, 0, 0, 0, 0); return fake_acp; }
static BOOL WINAPI fake_GetTextExtentPoint32A(HDC dc, LPCSTR t, int n, LPSIZE sz)
{
    struct logged *c = log_call(156, P(dc), (uint32_t)n, 0, 0);
    if (t) { strncpy(c->text, t, 63); c->text[63] = 0; }
    sz->cx = n * 7 + (t && n ? (uint8_t)t[0] % 5 : 0); sz->cy = 9 + n % 7;
    return TRUE;
}
static BOOL WINAPI fake_TextOutA(HDC dc, int x, int y, LPCSTR t, int n)
{
    struct logged *c = log_call(157, P(dc), (uint32_t)x, (uint32_t)y, (uint32_t)n);
    if (t) { strncpy(c->text, t, 63); c->text[63] = 0; }
    return TRUE;
}
static BOOL WINAPI fake_GdiFlush(void) { log_call(151, 0, 0, 0, 0); return TRUE; }
static int WINAPI fake_IntersectClipRect(HDC dc, int l, int t, int r, int b)
{ log_call(152, P(dc), (uint32_t)l, (uint32_t)t, (uint32_t)r); log_call(153, (uint32_t)b, 0, 0, 0); return 2; }
static BOOL WINAPI fake_SetViewportOrgEx(HDC dc, int x, int y, LPPOINT p) { log_call(154, P(dc), (uint32_t)x, (uint32_t)y, P(p)); return TRUE; }

static void surface_init(void)
{
    uint32_t i;
    for (i = 0; i < 34; i++) surf_vtbl[i] = (void *)s_other;
    surf_vtbl[0x44 / 4] = (void *)s_getdc; surf_vtbl[0x68 / 4] = (void *)s_releasedc;
    surf_vtbl[0x64 / 4] = (void *)s_lock; surf_vtbl[0x6c / 4] = (void *)s_restore; surf_vtbl[0x80 / 4] = (void *)s_unlock;
    fake_surf.vtbl = surf_vtbl;
}

static uint32_t cw_seq, cw_fail;
static HWND WINAPI fake_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h,
                                        HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    struct logged *c = log_call(102, ex, style, (uint32_t)x, P(parent));
    uint32_t fail = cw_fail & 1;
    if (P(cls) > 0xffff) strncpy(c->text, cls, 63);
    log_call(103, (uint32_t)y, (uint32_t)w, (uint32_t)h, P(param));
    log_call(104, P(menu), P(inst), P(name), 0);
    cw_fail >>= 1;
    return fail ? 0 : (HWND)(uintptr_t)(0x7700 + cw_seq++);
}
static BOOL WINAPI fake_ShowWindow(HWND h, int n) { log_call(105, P(h), n, 0, 0); return TRUE; }
static BOOL WINAPI fake_UpdateWindow(HWND h) { log_call(106, P(h), 0, 0, 0); return TRUE; }
static uint32_t wl_seed;
static LONG WINAPI fake_GetWindowLongA(HWND h, int i)
{
    log_call(107, P(h), (uint32_t)i, 0, 0);
    if (i == GWLP_USERDATA && (P(h) & ~0xfu) == 0x9300) { uint32_t k = P(h) & 0xf; if (dlg_busy[k]) { dlg_busy[k]--; return 0; } return 7; }
    return (LONG)(wl_seed * (uint32_t)(i + 40));
}
static HMENU WINAPI fake_GetMenu(HWND h) { log_call(108, P(h), 0, 0, 0); return (HMENU)(uintptr_t)(wl_seed & 0x10 ? 0x3e00 : 0); }
static BOOL WINAPI fake_AdjustWindowRectEx(LPRECT r, DWORD style, BOOL menu, DWORD ex)
{
    log_call(109, (uint32_t)r->left | (uint32_t)r->top << 16, (uint32_t)r->right | (uint32_t)r->bottom << 16, style, ex);
    log_call(110, (uint32_t)menu, 0, 0, 0);
    r->left -= (LONG)(style & 7); r->top -= (LONG)(ex & 0x1f) + (menu ? 19 : 0);
    r->right += (LONG)(style >> 28); r->bottom += (LONG)(ex >> 27);
    return TRUE;
}
static BOOL WINAPI fake_SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT f)
{ log_call(111, P(h), P(after), (uint32_t)x | (uint32_t)y << 16, f); log_call(112, (uint32_t)cx, (uint32_t)cy, 0, 0); return TRUE; }

static HRESULT WINAPI fake_DirectDrawCreate(GUID *g, void **out, void *unk);
static BOOL WINAPI fake_PeekMessageA(MSG *m, HWND h, UINT a, UINT b, UINT f);
static BOOL WINAPI fake_IsDialogMessageA(HWND h, MSG *m);
static int WINAPI fake_TranslateAcceleratorA(HWND h, HACCEL a, MSG *m);
static BOOL WINAPI fake_TranslateMessage(const MSG *m);
static LRESULT WINAPI fake_DispatchMessageA(const MSG *m);
static BOOL WINAPI fake_IsWindowEnabled(HWND h);
static BOOL WINAPI fake_IsWindowVisible(HWND h);
static HACCEL WINAPI fake_LoadAcceleratorsA(HINSTANCE i, LPCSTR n);
static BOOL WINAPI fake_IsZoomed(HWND h);
static BOOL WINAPI fake_GetWindowRect(HWND h, RECT *r);
static LONG WINAPI fake_SetWindowLongA(HWND h, int i, LONG v);
static int WINAPI fake_GetSystemMetrics(int i);
static HWND WINAPI fake_GetLastActivePopup(HWND h);
static UINT WINAPI fake_SetErrorMode(UINT m);
static BOOL WINAPI fake_EnumChildWindows(HWND h, WNDENUMPROC cb, LPARAM l);
static BOOL WINAPI fake_EnumThreadWindows(DWORD t, WNDENUMPROC cb, LPARAM l);
static int WINAPI fake_GetClassNameA(HWND h, LPSTR buf, int n);
static LONG WINAPI fake_ChangeDisplaySettingsA(DEVMODEA *m, DWORD f);
static HRESULT WINAPI fake_DirectDrawEnumerateA(LPDDENUMCALLBACKA cb, void *ctx);
static HDC WINAPI fake_GetDC(HWND h);
static int WINAPI fake_ReleaseDC(HWND h, HDC dc);
static int WINAPI fake_GetDeviceCaps(HDC dc, int i);
static BOOL WINAPI fake_AnimatePalette(HPALETTE h, UINT a, UINT n, const PALETTEENTRY *e);
static UINT WINAPI fake_SetPaletteEntries(HPALETTE h, UINT a, UINT n, const PALETTEENTRY *e);
static HPALETTE WINAPI fake_SelectPalette(HDC dc, HPALETTE h, BOOL bg);
static UINT WINAPI fake_RealizePalette(HDC dc);
static BOOL WINAPI fake_RedrawWindow(HWND h, const RECT *r, HRGN g, UINT f);

static void hook_window_system(uint8_t *b)
{
    hook_import(b, "USER32.dll", "CreateWindowExA", (void *)fake_CreateWindowExA);
    hook_import(b, "USER32.dll", "PeekMessageA", (void *)fake_PeekMessageA);
    hook_import(b, "USER32.dll", "IsDialogMessageA", (void *)fake_IsDialogMessageA);
    hook_import(b, "USER32.dll", "TranslateAcceleratorA", (void *)fake_TranslateAcceleratorA);
    hook_import(b, "USER32.dll", "TranslateMessage", (void *)fake_TranslateMessage);
    hook_import(b, "USER32.dll", "DispatchMessageA", (void *)fake_DispatchMessageA);
    hook_import(b, "USER32.dll", "IsWindowEnabled", (void *)fake_IsWindowEnabled);
    hook_import(b, "USER32.dll", "IsWindowVisible", (void *)fake_IsWindowVisible);
    hook_import(b, "USER32.dll", "LoadAcceleratorsA", (void *)fake_LoadAcceleratorsA);
    hook_import(b, "USER32.dll", "IsZoomed", (void *)fake_IsZoomed);
    hook_import(b, "USER32.dll", "GetWindowRect", (void *)fake_GetWindowRect);
    hook_import(b, "USER32.dll", "SetWindowLongA", (void *)fake_SetWindowLongA);
    hook_import(b, "USER32.dll", "GetSystemMetrics", (void *)fake_GetSystemMetrics);
    hook_import(b, "USER32.dll", "GetLastActivePopup", (void *)fake_GetLastActivePopup);
    hook_import(b, "KERNEL32.dll", "SetErrorMode", (void *)fake_SetErrorMode);
    hook_import(b, "USER32.dll", "EnumChildWindows", (void *)fake_EnumChildWindows);
    hook_import(b, "USER32.dll", "EnumThreadWindows", (void *)fake_EnumThreadWindows);
    hook_import(b, "USER32.dll", "GetClassNameA", (void *)fake_GetClassNameA);
    hook_import(b, "USER32.dll", "ChangeDisplaySettingsA", (void *)fake_ChangeDisplaySettingsA);
    hook_import(b, "DDRAW.dll", "DirectDrawCreate", (void *)fake_DirectDrawCreate);
    hook_import(b, "DDRAW.dll", "DirectDrawEnumerateA", (void *)fake_DirectDrawEnumerateA);
    hook_import(b, "GDI32.dll", "CreateFontA", (void *)fake_CreateFontA);
    hook_import(b, "GDI32.dll", "GetStockObject", (void *)fake_GetStockObject);
    hook_import(b, "GDI32.dll", "SelectObject", (void *)fake_SelectObject);
    hook_import(b, "USER32.dll", "DrawTextA", (void *)fake_DrawTextA);
    hook_import(b, "GDI32.dll", "SetTextColor", (void *)fake_SetTextColor);
    hook_import(b, "GDI32.dll", "SetBkColor", (void *)fake_SetBkColor);
    hook_import(b, "GDI32.dll", "SetBkMode", (void *)fake_SetBkMode);
    hook_import(b, "GDI32.dll", "GdiFlush", (void *)fake_GdiFlush);
    hook_import(b, "KERNEL32.dll", "GetACP", (void *)fake_GetACP);
    hook_import(b, "GDI32.dll", "GetTextExtentPoint32A", (void *)fake_GetTextExtentPoint32A);
    hook_import(b, "GDI32.dll", "TextOutA", (void *)fake_TextOutA);
    hook_import(b, "GDI32.dll", "IntersectClipRect", (void *)fake_IntersectClipRect);
    hook_import(b, "GDI32.dll", "SetViewportOrgEx", (void *)fake_SetViewportOrgEx);
    hook_import(b, "USER32.dll", "GetDC", (void *)fake_GetDC);
    hook_import(b, "USER32.dll", "ReleaseDC", (void *)fake_ReleaseDC);
    hook_import(b, "GDI32.dll", "GetDeviceCaps", (void *)fake_GetDeviceCaps);
    hook_import(b, "GDI32.dll", "AnimatePalette", (void *)fake_AnimatePalette);
    hook_import(b, "GDI32.dll", "SetPaletteEntries", (void *)fake_SetPaletteEntries);
    hook_import(b, "GDI32.dll", "SelectPalette", (void *)fake_SelectPalette);
    hook_import(b, "GDI32.dll", "RealizePalette", (void *)fake_RealizePalette);
    hook_import(b, "USER32.dll", "RedrawWindow", (void *)fake_RedrawWindow);
    hook_import(b, "USER32.dll", "ShowWindow", (void *)fake_ShowWindow);
    hook_import(b, "USER32.dll", "UpdateWindow", (void *)fake_UpdateWindow);
    hook_import(b, "USER32.dll", "GetWindowLongA", (void *)fake_GetWindowLongA);
    hook_import(b, "USER32.dll", "GetMenu", (void *)fake_GetMenu);
    hook_import(b, "USER32.dll", "AdjustWindowRectEx", (void *)fake_AdjustWindowRectEx);
    hook_import(b, "USER32.dll", "SetWindowPos", (void *)fake_SetWindowPos);
    hook_import(b, "GDI32.dll", "GetObjectA", (void *)fake_GetObjectA);
    hook_import(b, "GDI32.dll", "DeleteObject", (void *)fake_DeleteObject);
    hook_import(b, "GDI32.dll", "DeleteDC", (void *)fake_DeleteDC);
    hook_import(b, "USER32.dll", "DestroyWindow", (void *)fake_DestroyWindow);
    hook_import(b, "USER32.dll", "BeginDeferWindowPos", (void *)fake_BeginDeferWindowPos);
    hook_import(b, "USER32.dll", "DeferWindowPos", (void *)fake_DeferWindowPos);
    hook_import(b, "USER32.dll", "EndDeferWindowPos", (void *)fake_EndDeferWindowPos);
    hook_import(b, "USER32.dll", "GetParent", (void *)fake_GetParent);
    fake_DirectSoundCreate_ptr = (void *)fake_DirectSoundCreate;
    hook_import(b, "KERNEL32.dll", "LoadLibraryA", (void *)fake_LoadLibraryA);
    hook_import(b, "KERNEL32.dll", "GetProcAddress", (void *)fake_GetProcAddress);
    hook_import(b, "KERNEL32.dll", "FreeLibrary", (void *)fake_FreeLibrary);
    hook_import(b, "USER32.dll", "LoadCursorA", (void *)fake_LoadCursorA);
    hook_import(b, "USER32.dll", "SetCursor", (void *)fake_SetCursor);
    hook_import(b, "MSACM32.dll", "acmStreamOpen", (void *)fake_acmStreamOpen);
    hook_import(b, "MSACM32.dll", "acmStreamSize", (void *)fake_acmStreamSize);
    hook_import(b, "MSACM32.dll", "acmStreamPrepareHeader", (void *)fake_acmStreamPrepareHeader);
    hook_import(b, "MSACM32.dll", "acmStreamConvert", (void *)fake_acmStreamConvert);
    hook_import(b, "MSACM32.dll", "acmStreamUnprepareHeader", (void *)fake_acmStreamUnprepareHeader);
    hook_import(b, "MSACM32.dll", "acmStreamClose", (void *)fake_acmStreamClose);
    hook_import(b, "USER32.dll", "CreateMenu", (void *)fake_CreateMenu);
    hook_import(b, "USER32.dll", "AppendMenuA", (void *)fake_AppendMenuA);
    hook_import(b, "USER32.dll", "DestroyMenu", (void *)fake_DestroyMenu);
    hook_import(b, "USER32.dll", "SetMenu", (void *)fake_SetMenu);
    hook_import(b, "USER32.dll", "DrawMenuBar", (void *)fake_DrawMenuBar);
    hook_import(b, "USER32.dll", "IsWindow", (void *)fake_IsWindow);
    hook_import(b, "USER32.dll", "EnableMenuItem", (void *)fake_EnableMenuItem);
    hook_import(b, "USER32.dll", "CheckMenuItem", (void *)fake_CheckMenuItem);
    hook_import(b, "USER32.dll", "GetCursorPos", (void *)fake_GetCursorPos);
    hook_import(b, "USER32.dll", "GetClipCursor", (void *)fake_GetClipCursor);
    hook_import(b, "USER32.dll", "ShowCursor", (void *)fake_ShowCursor);
    hook_import(b, "USER32.dll", "SetCursorPos", (void *)fake_SetCursorPos);
    hook_import(b, "USER32.dll", "SetCapture", (void *)fake_SetCapture);
    hook_import(b, "USER32.dll", "GetCapture", (void *)fake_GetCapture);
    hook_import(b, "USER32.dll", "ClipCursor", (void *)fake_ClipCursor);
    hook_import(b, "USER32.dll", "ReleaseCapture", (void *)fake_ReleaseCapture);
    hook_import(b, "KERNEL32.dll", "GetDiskFreeSpaceA", (void *)fake_GetDiskFreeSpaceA);
    hook_import(b, "KERNEL32.dll", "GetTempFileNameA", (void *)fake_GetTempFileNameA);
    hook_import(b, "KERNEL32.dll", "GetProcessHeaps", (void *)fake_GetProcessHeaps);
    hook_import(b, "KERNEL32.dll", "HeapValidate", (void *)fake_HeapValidate);
    hook_import(b, "WINMM.dll", "joyGetNumDevs", (void *)fake_joyGetNumDevs);
    hook_import(b, "WINMM.dll", "joyGetPosEx", (void *)fake_joyGetPosEx);
    hook_import(b, "WINMM.dll", "joyGetDevCapsA", (void *)fake_joyGetDevCapsA);
    hook_import(b, "KERNEL32.dll", "Sleep", (void *)fake_Sleep);
    hook_import(b, "ADVAPI32.dll", "RegOpenKeyExA", (void *)fake_RegOpenKeyExA);
    hook_import(b, "ADVAPI32.dll", "RegQueryValueExA", (void *)fake_RegQueryValueExA);
    hook_import(b, "ADVAPI32.dll", "RegCloseKey", (void *)fake_RegCloseKey);
    hook_import(b, "USER32.dll", "SendMessageA", (void *)fake_SendMessageA);
    hook_import(b, "USER32.dll", "PostMessageA", (void *)fake_PostMessageA);
    hook_import(b, "USER32.dll", "SetWindowTextA", (void *)fake_SetWindowTextA);
    hook_import(b, "USER32.dll", "MessageBoxA", (void *)fake_MessageBoxA);
    hook_import(b, "WINMM.dll", "timeGetTime", (void *)fake_timeGetTime);
    hook_import(b, "KERNEL32.dll", "GetLocalTime", (void *)fake_GetLocalTime);
}

/* Bind the real host's import table as the loader would have, so the services
 * that reach MSVCRT (malloc, realloc, free, memmove) or the window system run.
 * Both sides then share one CRT heap.  A DLL or entry wine lacks (MFC42's
 * ordinals) is bound to a trap instead. */
static void bind_real(void)
{
    uint8_t *b = (uint8_t *)REAL_BASE;
    uint32_t pe = *(uint32_t *)(b + 0x3c);
    uint32_t *d = (uint32_t *)(b + *(uint32_t *)(b + pe + 0x80));

    for (; d[3] != 0; d += 5) {
        HMODULE m = LoadLibraryA((const char *)(b + d[3]));
        uint32_t *name = (uint32_t *)(b + (d[0] ? d[0] : d[4]));
        uint32_t *iat = (uint32_t *)(b + d[4]);
        for (; *name != 0; name++, iat++) {
            FARPROC f = 0;
            if (m)
                f = (*name & 0x80000000u) ? GetProcAddress(m, (LPCSTR)(uintptr_t)(*name & 0xffff))
                                          : GetProcAddress(m, (const char *)(b + *name + 2));
            *iat = f ? (uint32_t)(uintptr_t)f : (uint32_t)(uintptr_t)unbound;
        }
    }
    hook_import(b, "MSVCRT.dll", "realloc", (void *)moving_realloc);
    hook_import(b, "MSVCRT.dll", "malloc", (void *)patterned_malloc);
    hook_import(b, "MSVCRT.dll", "free", (void *)counted_free);
    hook_import(b, "KERNEL32.dll", "GetCommandLineA", (void *)fake_GetCommandLineA);
    hook_window_system(b);
}

/* Map the real host in place: make its region executable and copy the file. */
static void map_real(void)
{
    static uint8_t f[0x30000];
    FILE *fp = fopen("FLTSIM98.EXE", "rb");
    uint32_t pe, nsec, i, soi, soh;
    DWORD old;

    if (!fp) { printf("cannot open FLTSIM98.EXE\n"); exit(2); }
    fread(f, 1, sizeof f, fp);
    fclose(fp);
    pe = *(uint32_t *)(f + 0x3c);
    soi = *(uint32_t *)(f + pe + 0x50);
    soh = *(uint32_t *)(f + pe + 0x54);
    nsec = *(uint16_t *)(f + pe + 6);
    if (!VirtualProtect((void *)REAL_BASE, soi, PAGE_EXECUTE_READWRITE, &old)) {
        printf("cannot make 0x400000 executable (%lu)\n", GetLastError());
        exit(2);
    }
    memcpy((void *)REAL_BASE, f, soh);
    for (i = 0; i < nsec; i++) {
        uint8_t *s = f + pe + 24 + *(uint16_t *)(f + pe + 20) + i * 40;
        uint32_t va = *(uint32_t *)(s + 12), raw = *(uint32_t *)(s + 20), rsz = *(uint32_t *)(s + 16);
        memcpy((uint8_t *)REAL_BASE + va, f + raw, rsz);
    }
    bind_real();
    real_tbl = (uint32_t *)TABLE_VA;
    if (real_tbl[0] != 1) { printf("real host table id = %#x, not 1\n", real_tbl[0]); exit(2); }
}

/* A fault ends the run at once, with its address, rather than in wine's
 * debugger. */
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("hostdiff: fault %#lx at %p (reading %#lx)\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress,
           e->ExceptionRecord->NumberParameters > 1 ? (unsigned long)e->ExceptionRecord->ExceptionInformation[1] : 0ul);
    printf("  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n",
           e->ContextRecord->Eax, e->ContextRecord->Ebx, e->ContextRecord->Ecx, e->ContextRecord->Edx,
           e->ContextRecord->Esi, e->ContextRecord->Edi, e->ContextRecord->Ebp, e->ContextRecord->Esp);
    if (trace) {                        /* the code addresses on the stack, for a backtrace by hand */
        uint32_t *sp = (uint32_t *)(uintptr_t)e->ContextRecord->Esp, i;
        printf("  stack:");
        for (i = 0; i < 400; i++)
            if ((sp[i] >= 0x401000 && sp[i] < 0x416000) || (sp[i] >= 0x13001000 && sp[i] < 0x13100000) ||
                (sp[i] >= 0x41001000 && sp[i] < 0x41100000))
                printf(" %08x", sp[i]);
        printf("\n");
    }
    ExitProcess(5);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void load_ours(void)
{
    HMODULE m = LoadLibraryA("HOST_ours.dll");
    if (!m) { printf("cannot load HOST_ours.dll (%lu)\n", GetLastError()); exit(2); }
    our_tbl = (void **)GetProcAddress(m, "HostTable");
    our_data = (uint8_t *)GetProcAddress(m, "host_image");
    if (!our_tbl || !our_data) { printf("HOST_ours.dll: missing exports\n"); exit(2); }
    hook_import((uint8_t *)m, "msvcrt.dll", "realloc", (void *)moving_realloc);
    hook_import((uint8_t *)m, "msvcrt.dll", "malloc", (void *)patterned_malloc);
    hook_import((uint8_t *)m, "msvcrt.dll", "free", (void *)counted_free);
    hook_import((uint8_t *)m, "KERNEL32.dll", "GetCommandLineA", (void *)fake_GetCommandLineA);
    hook_window_system((uint8_t *)m);
    if ((uintptr_t)our_tbl[0] != 1) { printf("our table id = %p, not 1\n", our_tbl[0]); exit(2); }
}

static uint32_t view_rec_base[2], view_rec_size;     /* each side's view records (check_views) */
static uint32_t pair_base[2], pair_size;             /* each side's copy of one heap block (check_dialogs) */

/* Normalise a .data pointer (either side) to a tagged offset; likewise a
 * pointer into either side's view records. */
static uint32_t norm(uint32_t v)
{
    if (view_rec_size && v >= view_rec_base[0] && v < view_rec_base[0] + view_rec_size)
        return 0xee000000u | (v - view_rec_base[0]);
    if (view_rec_size && v >= view_rec_base[1] && v < view_rec_base[1] + view_rec_size)
        return 0xee000000u | (v - view_rec_base[1]);
    if (pair_size && v - pair_base[0] < pair_size)
        return 0xeb000000u | (v - pair_base[0]);
    if (pair_size && v - pair_base[1] < pair_size)
        return 0xeb000000u | (v - pair_base[1]);
    if (v >= DATA_VA && v < DATA_END)
        return 0xda000000u | (v - DATA_VA);
    if (v >= (uint32_t)(uintptr_t)our_data && v < (uint32_t)(uintptr_t)our_data + DATA_SIZE)
        return 0xda000000u | (v - (uint32_t)(uintptr_t)our_data);
    return v;
}

static void *real_fn(uint32_t off) { return (void *)real_tbl[off / 4]; }
static void *our_fn(uint32_t off)
{
    void *p = our_tbl[off / 4];
    if (!p) { printf("  +%#05x OURS MISSING\n", off); differ++; }
    return p;
}

/* Seed .data with distinct random values, identically on both sides, so a
 * service that reads the wrong offset of an otherwise-zero (or coincidentally
 * equal) global is caught.  The service table at 0xba0..0xec0 is left alone --
 * the real host dispatches through it -- and the string constant at 0x41bca0
 * (offset 0x4ca0) is given a short NUL-terminated value so the strcpy from it
 * stays bounded. */
#define TABLE_OFF   0xba0u
#define TABLE_LAST  0xec0u
#define STR_OFF     0x4ca0u            /* 0x41bca0 */
#define QUEUE_OFF   0x1380u            /* 0x418380, the removal queue */
#define QUEUE_END   0x139cu
#define EXT_OFF     0x0f20u            /* 0x417f20: the 37 built-in extension names */
#define EXT_END     0x1100u            /* ... their strings, the colon, separator and UNC prefix, the bytes per pixel */
#define CFG_OFF     0x00a0u            /* 0x4170a0: ".CFG", the last of them */
#define EMPTY_OFF   0x17f8u            /* 0x4187f8: the host's empty string (bss) */
#define REG_N_OFF   0x18acu            /* 0x4188ac: extensions registered at run time */
#define REG_P_OFF   0x18b0u            /* 0x4188b0: their _strdup'd names (heap) */
#define MODS_OFF    0x0b30u            /* 0x417b30: the module records, 0x14 bytes each */
#define STRMODS_OFF 0x12e0u            /* 0x4182e0: the modules strings are looked up in */
#define FMT_OFF     0x126cu            /* 0x41826c: "%s%2.2d" */
#define HINST_OFF   0x182cu            /* 0x41882c: the host's instance */
#define INAMES_OFF  0x3240u            /* 0x41a240: input names loaded; their lengths */
#define ACCEL_OFF   0x1278u            /* 0x418278: accelerator tables, a host list; id counter after */
#define BUTTON_OFF  0x1108u            /* 0x418108: "Button " (0x32 bytes) */
#define AXIS_OFF    0x1140u            /* 0x418140: "Axis " */

/* The run-time extension registry: a few names, the same on both sides, each
 * side's array and strings its own heap blocks (the host grows them with
 * realloc and _strdup). */
static const char *const reg_names[] = { ".XYZ", ".Q", ".ABCD", ".BGL2", ".", ".ZZ" };

static void seed_registry(void)
{
    uint32_t n = rnd() % 4, i;
    char **ra = 0, **oa = 0;
    if (n) {
        ra = malloc(n * 4); oa = malloc(n * 4);
        for (i = 0; i < n; i++) {
            const char *nm = reg_names[rnd() % 6];
            ra[i] = _strdup(nm); oa[i] = _strdup(nm);
        }
    }
    *(uint32_t *)(DATA_VA + REG_N_OFF) = n;
    *(uint32_t *)(our_data + REG_N_OFF) = n;
    *(uint32_t *)(DATA_VA + REG_P_OFF) = (uint32_t)(uintptr_t)ra;
    *(uint32_t *)(our_data + REG_P_OFF) = (uint32_t)(uintptr_t)oa;
    *(uint32_t *)(DATA_VA + EMPTY_OFF) = 0;
    *(uint32_t *)(our_data + EMPTY_OFF) = 0;
}

/* .data the seeding leaves alone: what the real host dispatches through or
 * treats as constant, and the state the harness sets up itself. */
static const struct { uint32_t lo, hi; } keep[] = {
    { TABLE_OFF, TABLE_LAST + 4 },      /* the service table */
    { QUEUE_OFF, QUEUE_END },           /* the removal queue (a host list) */
    { EXT_OFF, EXT_END },               /* the extension names and "\\\\" */
    { CFG_OFF, CFG_OFF + 8 },
    { EMPTY_OFF, EMPTY_OFF + 4 },
    { REG_N_OFF, REG_P_OFF + 4 },       /* the extension registry */
    { MODS_OFF, MODS_OFF + 0x1c },      /* the module records (a host list) */
    { STRMODS_OFF, STRMODS_OFF + 0x1c },/* the string modules (a host list) */
    { ACCEL_OFF, ACCEL_OFF + 0x1c },    /* the accelerator tables (a host list) */
    { FMT_OFF, FMT_OFF + 8 },           /* "%s%2.2d" */
    { HINST_OFF, HINST_OFF + 4 },       /* the host instance strings fall back on */
    { INAMES_OFF, INAMES_OFF + 0xc },   /* the input-name flag and lengths */
    { 0x5120, 0x5224 },                 /* FLTSIM98.CFG's path (seed_profile) */
    { 0x18a0, 0x18a4 },                 /* 0x4188a0: the empty double-NUL a section is cleared with */
    { 0x0a24, 0x0a28 },                 /* 0x417a24: "%d" */
    { 0x1304, 0x1308 },                 /* 0x418304: "..." */
    { 0x1298, 0x12c4 },                 /* 0x418298: the menu root and its terminator; 0x4182c0 */
    { 0x10dc, 0x10e0 },                 /* 0x4180dc: "a.x" */
    { 0x130c, 0x132c },                 /* 0x41830c: "DirectSoundCreate", "DSOUND.DLL" */
    { 0x1374, 0x137c },                 /* 0x418374: ".wav" */
    { 0x0b60, 0x0b88 },                 /* 0x417b60: "Linkage", "FS6API ...", "ImportTable" */
    { 0x12fc, 0x1304 },                 /* 0x4182fc: ".fll" */
    { 0x18b8, 0x3240 },                 /* the joystick table, its flag and count (seed_joysticks) */
    { 0x1174, 0x1278 },                 /* 0x418174..: the joystick registry names and formats */
    { 0x0ed0, 0x0ee0 },                 /* 0x417ed0: ", ", ",", "%f" */
    { 0x0168, 0x0188 },                 /* 0x417168: "FS98CHILD", "FS98FLOAT" (window classes) */
    { 0x13b8, 0x1458 },                 /* 0x4183b8..: " ", "%s (%s)", "display", the Direct3D suffixes; DirectPlay's names */
    { 0x1458, 0x15b8 },                 /* 0x418458..: the display option table, its key names, modes and formats */
    { 0x3508, 0x3518 },                 /* 0x41a508: the GUID DirectPlayCreate is given */
    { 0x4ec0, 0x4ec4 }, { 0x4c80, 0x4c88 }, { 0x4b64, 0x4b6c }, { 0x4fe8, 0x4fec },
    { 0x4da4, 0x4da8 }, { 0x4dac, 0x4db0 }, { 0x5360, 0x5364 }, { 0x5104, 0x5108 },
                                        /* the key words' pointers (seed_keynames) */
};

static int kept(uint32_t i)
{
    uint32_t k;
    for (k = 0; k < sizeof keep / sizeof keep[0]; k++)
        if (i + 4 > keep[k].lo && i < keep[k].hi)
            return 1;
    return 0;
}

static void seed_strings(void);
static void seed_accels(void);
static void seed_keynames(void);

/* Modules with string tables, as data files: the language DLL (ENGLISH.FLL,
 * which has the input names 0x8012/0x8013), two more, and the real host's
 * own image (string 9). */
static uint32_t strmod[4];

static void load_strmods(void)
{
    static const char *const f[] = { "ENGLISH.FLL", "SIMSUI.DLL", "DIALOG.DLL" };
    uint32_t i;
    for (i = 0; i < 3; i++) {
        strmod[i] = (uint32_t)(uintptr_t)LoadLibraryExA(f[i], 0, LOAD_LIBRARY_AS_DATAFILE);
        if (!strmod[i]) { printf("cannot load %s as data (%lu)\n", f[i], GetLastError()); exit(2); }
    }
    strmod[3] = REAL_BASE;
}

/* A host list header {count, cap, grow, size, 0, 0, data} on both sides, the
 * data a heap copy each of `bytes`. */
static void seed_list(uint32_t off, uint32_t n, uint32_t size, const void *bytes)
{
    uint32_t h[7] = { n, n, 0x20, size, 0, 0, 0 };
    uint8_t *r = 0, *o = 0;
    if (n) {
        r = malloc(n * size); o = malloc(n * size);
        memcpy(r, bytes, n * size); memcpy(o, bytes, n * size);
    }
    h[6] = (uint32_t)(uintptr_t)r; memcpy((void *)(DATA_VA + off), h, sizeof h);
    h[6] = (uint32_t)(uintptr_t)o; memcpy(our_data + off, h, sizeof h);
}

static const char *const prefixes[] = { "Button ", "Axis ", "Btn", "", "A" };

/* The string modules, the module records, the host instance, and the input
 * names -- either still to load (flag 0) or loaded as some short prefixes. */
static void seed_strings(void)
{
    uint32_t mods[4], recs[5 * 5], n, m, i, v;
    n = rnd() % 5;
    for (i = 0; i < n; i++) mods[i] = strmod[rnd() % 4];
    if (n == 4 && rnd() & 1) n = 3;
    seed_list(STRMODS_OFF, n, 4, mods);
    m = rnd() % 6;
    for (i = 0; i < m; i++) {
        recs[i * 5 + 0] = rnd();
        recs[i * 5 + 1] = rnd();
        recs[i * 5 + 2] = strmod[rnd() % 4];
        recs[i * 5 + 3] = 1 + rnd() % 8;
        recs[i * 5 + 4] = rnd() % 3 ? 1 + rnd() % 3 : 0;
    }
    seed_list(MODS_OFF, m, 0x14, recs);
    /* the host instance: the real host's image, or (as if the host's own
     * resources were the language DLL's) ENGLISH.FLL, which has accelerators */
    *(uint32_t *)(DATA_VA + HINST_OFF) = *(uint32_t *)(our_data + HINST_OFF) = rnd() % 4 ? REAL_BASE : strmod[0];
    v = rnd() & 1;
    *(uint32_t *)(DATA_VA + INAMES_OFF) = *(uint32_t *)(our_data + INAMES_OFF) = v;
    if (v) {
        const char *b = prefixes[rnd() % 5], *a = prefixes[rnd() % 5];
        strcpy((char *)(DATA_VA + BUTTON_OFF), b); strcpy((char *)(our_data + BUTTON_OFF), b);
        strcpy((char *)(DATA_VA + AXIS_OFF), a); strcpy((char *)(our_data + AXIS_OFF), a);
        v = rnd() % 8;
        *(uint32_t *)(DATA_VA + INAMES_OFF + 4) = *(uint32_t *)(our_data + INAMES_OFF + 4) = v;
        v = rnd() % 8;
        *(uint32_t *)(DATA_VA + INAMES_OFF + 8) = *(uint32_t *)(our_data + INAMES_OFF + 8) = v;
    }
}

static void seed_data(void)
{
    uint32_t i, len;
    for (i = 0; i + 4 <= DATA_SIZE; i += 4) {
        uint32_t v;
        if (kept(i))
            continue;
        v = rnd();
        *(uint32_t *)(DATA_VA + i) = v;
        *(uint32_t *)(our_data + i) = v;
    }
    len = rnd() % 60;
    for (i = 0; i < len; i++) {
        uint8_t c = (uint8_t)(0x21 + rnd() % 0x5e);
        *(uint8_t *)(DATA_VA + STR_OFF + i) = c;
        *(uint8_t *)(our_data + STR_OFF + i) = c;
    }
    *(uint8_t *)(DATA_VA + STR_OFF + len) = 0;
    *(uint8_t *)(our_data + STR_OFF + len) = 0;
    /* The queue is the host's own list and stays one: emptied each iteration,
     * its capacity and data (a heap block on each side) kept. */
    *(uint32_t *)(DATA_VA + QUEUE_OFF) = 0;
    *(uint32_t *)(our_data + QUEUE_OFF) = 0;
    seed_registry();
    seed_strings();
    seed_accels();
    seed_keynames();
}

/* After a call, the two .data images must be identical; report the first word
 * that is not. */
static void check_state(const char *name)
{
    uint32_t i;
    for (i = 0; i < DATA_SIZE; i += 4) {
        uint32_t rv;
        uint32_t ov;
        if (i == QUEUE_OFF + 0x18 || i == REG_P_OFF || i == MODS_OFF + 0x18 || i == STRMODS_OFF + 0x18 ||
            i == ACCEL_OFF + 0x18 || i == 0x12a4 || i == 0x0ee0 + 0x18 || i == 0x33c0 || i == 0x33dc ||
            i == 0x0f00 + 0x18 || i == 0x4a28 ||   /* the modeless dialogs' list, the dither table */
            (i >= 0x5120 && i < 0x5224))   /* the CFG paths differ in a letter */
            /* each side's own heap block */
            continue;
        rv = norm(*(uint32_t *)(DATA_VA + i));     /* real .data (mapped at 0x417000) */
        ov = norm(*(uint32_t *)(our_data + i));    /* our image */
        if (rv != ov) {
            printf("  %-16s STATE DIFFER at .data+%#x: real=%#010x ours=%#010x\n",
                   name, i, rv, ov);
            differ++;
            return;
        }
    }
    {   /* the two host lists the harness seeds: contents through each side's data */
        static const uint32_t lists[] = { MODS_OFF, STRMODS_OFF };
        for (i = 0; i < 2; i++) {
            uint32_t n = *(uint32_t *)(DATA_VA + lists[i]), sz = *(uint32_t *)(DATA_VA + lists[i] + 0xc);
            uint8_t *rd = *(uint8_t **)(DATA_VA + lists[i] + 0x18), *od = *(uint8_t **)(our_data + lists[i] + 0x18);
            uint32_t j, bad = n && n < 64 && sz <= 0x14 && (!rd || !od);
            for (j = 0; !bad && n < 64 && sz <= 0x14 && j < n; j++)       /* a module record's name (+0) is each side's own copy */
                bad = memcmp(rd + j * sz + (i == 0 ? 4 : 0), od + j * sz + (i == 0 ? 4 : 0), sz - (i == 0 ? 4 : 0)) != 0;
            if (bad) {
                printf("  %-16s LIST .data+%#x CONTENTS DIFFER\n", name, lists[i]);
                differ++;
                return;
            }
        }
    }
    {   /* the registry's names, through each side's own array */
        uint32_t n = *(uint32_t *)(DATA_VA + REG_N_OFF);
        char **ra = *(char ***)(DATA_VA + REG_P_OFF), **oa = *(char ***)(our_data + REG_P_OFF);
        for (i = 0; i < n && i < 64; i++)
            if (strcmp(ra[i], oa[i])) {
                printf("  %-16s REGISTRY DIFFER at %u: real=\"%s\" ours=\"%s\"\n", name, i, ra[i], oa[i]);
                differ++;
                return;
            }
    }
}

/* ---- the accessors (no arguments, no writes) --------------------------------- */

struct slot { uint32_t off; const char *name; int ret_ptr; };

static const struct slot accessors[] = {
    { 0x1c, "const-one",     0 },
    { 0x20, "global-41719c", 0 },
    { 0x34, "global-418850", 0 },
    { 0x58, "past-table",    1 },
    { 0x5c, "struct-41c240", 1 },
    { 0x60, "struct-41bb80", 1 },
    { 0x154, "zero0",        0 },
    { 0x2cc, "flag-41a3cc",  0 },
};

static void check_accessor(const struct slot *s)
{
    fn0 r, o;
    uint32_t rv, ov;
    r = (fn0)real_fn(s->off);
    o = (fn0)our_fn(s->off);
    if (!o) return;
    rv = r(); ov = o();
    if (s->ret_ptr) { rv = norm(rv); ov = norm(ov); }
    if (rv != ov) {
        printf("  +%#05x %-16s DIFFER  real=%#010x ours=%#010x\n", s->off, s->name, rv, ov);
        differ++;
    }
}

/* ---- the state writers ------------------------------------------------------- */

/* +0x18: copies six fields of its argument struct into host globals. */
static void check_018(void)
{
    static uint8_t buf[0x100];
    fn1 r = (fn1)real_fn(0x18), o = (fn1)our_fn(0x18);
    uint32_t i, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof buf; i += 4) *(uint32_t *)(buf + i) = rnd();
    rv = r((uint32_t)(uintptr_t)buf);
    ov = o((uint32_t)(uintptr_t)buf);
    if (rv != ov) { printf("  +0x018 arg-copy RET DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    check_state("+0x018 arg-copy");
}

/* +0x2c: sets a host flag to 1. */
static void check_02c(void)
{
    fn0 r = (fn0)real_fn(0x2c), o = (fn0)our_fn(0x2c);
    if (!o) return;
    r(); o();
    check_state("+0x02c set-flag");
}

/* +0x54: writes two host globals back through two out-pointers. */
static void check_054(void)
{
    fn2 r = (fn2)real_fn(0x54), o = (fn2)our_fn(0x54);
    uint32_t r1 = 0, r2 = 0, o1 = 0, o2 = 0;
    if (!o) return;
    r((uint32_t)(uintptr_t)&r1, (uint32_t)(uintptr_t)&r2);
    o((uint32_t)(uintptr_t)&o1, (uint32_t)(uintptr_t)&o2);
    if (norm(r1) != norm(o1) || norm(r2) != norm(o2)) {
        printf("  +0x054 read-pair DIFFER real=(%#x,%#x) ours=(%#x,%#x)\n", r1, r2, o1, o2);
        differ++;
    }
}

/* ---- the C-runtime strings --------------------------------------------------- */

/* A random NUL-terminated string of up to max-1 printable bytes. */
static void rnd_str(char *b, uint32_t max)
{
    uint32_t len = rnd() % max, i;
    for (i = 0; i < len; i++) b[i] = (char)(0x21 + rnd() % 0x5e);
    b[len] = '\0';
}

/* +0x2b8 strlen: a null pointer, the empty string, and random strings. */
static void check_2b8(void)
{
    static char s[128];
    fn1 r = (fn1)real_fn(0x2b8), o = (fn1)our_fn(0x2b8);
    uint32_t rv, ov;
    if (!o) return;
    if (r(0) != o(0)) { printf("  +0x2b8 strlen(NULL) DIFFER\n"); differ++; }
    rnd_str(s, sizeof s);
    rv = r((uint32_t)(uintptr_t)s); ov = o((uint32_t)(uintptr_t)s);
    if (rv != ov) { printf("  +0x2b8 strlen DIFFER real=%u ours=%u\n", rv, ov); differ++; }
}

/* +0x2a8 strcpy and +0x2b0 strcat: each side copies into its own buffer, filled
 * past the string with 0xaa so an over-copy shows; compare the buffers. */
static void check_2a8(void)
{
    static char src[96], rb[192], ob[192];
    fn2 r = (fn2)real_fn(0x2a8), o = (fn2)our_fn(0x2a8);
    if (!o) return;
    rnd_str(src, sizeof src);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb, (uint32_t)(uintptr_t)src);
    o((uint32_t)(uintptr_t)ob, (uint32_t)(uintptr_t)src);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x2a8 strcpy DIFFER\n"); differ++; }
}

static void check_2b0(void)
{
    static char src[96], rb[192], ob[192];
    fn2 r = (fn2)real_fn(0x2b0), o = (fn2)our_fn(0x2b0);
    uint32_t pre;
    if (!o) return;
    rnd_str(src, sizeof src);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rnd_str(rb, 64);                         /* a prefix to append onto */
    pre = (uint32_t)strlen(rb);
    memcpy(ob, rb, pre + 1);
    memset(rb + pre + 1, 0xaa, sizeof rb - pre - 1);
    memset(ob + pre + 1, 0xaa, sizeof ob - pre - 1);
    r((uint32_t)(uintptr_t)rb, (uint32_t)(uintptr_t)src);
    o((uint32_t)(uintptr_t)ob, (uint32_t)(uintptr_t)src);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x2b0 strcat DIFFER\n"); differ++; }
}

/* ---- the block and string primitives ---------------------------------------- */

/* +0x288: stores its argument in a host global; +0x304: clears the dword its
 * argument points at (the pointer is harness memory, so only the effect is
 * compared, not the returned pointer). */
static void check_288(void)
{
    fn1 r = (fn1)real_fn(0x288), o = (fn1)our_fn(0x288);
    uint32_t v;
    if (!o) return;
    v = rnd();
    r(v); o(v);
    check_state("+0x288 set-417194");
}

static void check_304(void)
{
    fn1 r = (fn1)real_fn(0x304), o = (fn1)our_fn(0x304);
    uint32_t rc = rnd(), oc = rc;
    if (!o) return;
    r((uint32_t)(uintptr_t)&rc); o((uint32_t)(uintptr_t)&oc);
    if (rc != oc) { printf("  +0x304 clear DIFFER real=%#x ours=%#x\n", rc, oc); differ++; }
}

/* +0x130: copies a fixed host string into the caller's buffer. */
static void check_130(void)
{
    static char rb[128], ob[128];
    fn1 r = (fn1)real_fn(0x130), o = (fn1)our_fn(0x130);
    if (!o) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb); o((uint32_t)(uintptr_t)ob);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x130 strcpy-const DIFFER\n"); differ++; }
}

/* +0x168: the UNC-prefix predicate over crafted inputs. */
static void check_168(void)
{
    static const char *t[] = { "\\\\server", "\\x", "x\\\\", "\\", "", "ab" };
    fn1 r = (fn1)real_fn(0x168), o = (fn1)our_fn(0x168);
    uint32_t i;
    if (!o) return;
    if (r(0) != o(0)) { printf("  +0x168 UNC(NULL) DIFFER\n"); differ++; }
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (r((uint32_t)(uintptr_t)t[i]) != o((uint32_t)(uintptr_t)t[i]))
            { printf("  +0x168 UNC(\"%s\") DIFFER\n", t[i]); differ++; }
}

/* +0x218 memset, +0x228 memcmp. */
static void check_218(void)
{
    static uint8_t rb[256], ob[256];
    {
        uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t) = (void *)real_fn(0x218);
        uint32_t (__stdcall *o)(uint32_t, uint32_t, uint32_t) = (void *)our_fn(0x218);
        uint32_t val = rnd() & 0xff, cnt = rnd() % sizeof rb;
        if (!o) return;
        memset(rb, 0x11, sizeof rb); memset(ob, 0x11, sizeof ob);
        r((uint32_t)(uintptr_t)rb, val, cnt); o((uint32_t)(uintptr_t)ob, val, cnt);
        if (memcmp(rb, ob, sizeof rb)) { printf("  +0x218 memset DIFFER\n"); differ++; }
    }
}

static void check_228(void)
{
    static uint8_t a[64], b[64];
    uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t) = (void *)real_fn(0x228);
    uint32_t (__stdcall *o)(uint32_t, uint32_t, uint32_t) = (void *)our_fn(0x228);
    uint32_t i, n = 1 + rnd() % sizeof a, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof a; i++) { a[i] = (uint8_t)rnd(); b[i] = a[i]; }
    if (rnd() & 1) b[rnd() % n] ^= (uint8_t)(1 + rnd());    /* sometimes differ */
    rv = r((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, n);
    ov = o((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, n);
    if (rv != ov) { printf("  +0x228 memcmp DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
}

/* +0x2bc strcmp. */
static void check_2bc(void)
{
    static char a[64], b[64];
    fn2 r = (fn2)real_fn(0x2bc), o = (fn2)our_fn(0x2bc);
    uint32_t rv, ov;
    if (!o) return;
    rnd_str(a, sizeof a);
    if (rnd() & 1) memcpy(b, a, sizeof b); else rnd_str(b, sizeof b);  /* often equal */
    rv = r((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
    ov = o((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
    if (rv != ov) { printf("  +0x2bc strcmp DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
}

/* +0x28c: compacts a 256-entry host table into a 0x300 buffer. */
static void check_28c(void)
{
    static uint8_t rb[0x300], ob[0x300];
    fn1 r = (fn1)real_fn(0x28c), o = (fn1)our_fn(0x28c);
    if (!o) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb); o((uint32_t)(uintptr_t)ob);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x28c compact DIFFER\n"); differ++; }
}

/* The zero stubs, by argument count. */
static void check_stubs(void)
{
    fn1 s12c = (fn1)our_fn(0x12c);
    fn2 s108 = (fn2)our_fn(0x108), s220 = (fn2)our_fn(0x220), s224 = (fn2)our_fn(0x224);
    if (s12c && ((fn1)real_fn(0x12c))(rnd()) != s12c(rnd())) { printf("  +0x12c DIFFER\n"); differ++; }
    if (s108 && ((fn2)real_fn(0x108))(1, 2) != s108(1, 2)) { printf("  +0x108 DIFFER\n"); differ++; }
    if (s220 && ((fn2)real_fn(0x220))(1, 2) != s220(1, 2)) { printf("  +0x220 DIFFER\n"); differ++; }
    if (s224 && ((fn2)real_fn(0x224))(1, 2) != s224(1, 2)) { printf("  +0x224 DIFFER\n"); differ++; }
    if (our_fn(0x314)) { ((fn2)real_fn(0x314))(rnd(), rnd()); ((fn2)our_fn(0x314))(rnd(), rnd()); }   /* balanced, or the stack shows it */
}

/* ---- the allocator ----------------------------------------------------------- */

typedef uint32_t (__stdcall *fn3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (__stdcall *fn4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (__stdcall *fn5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);


/* +0x20c alloc, +0x210 realloc, +0x214 free.  The pointers differ by side, so
 * what is compared is the return, null-or-not, and the bytes the service
 * defines: zeroed when asked, the old contents kept across a realloc.  Only
 * flags 4 is asked to fail (a size no heap grants); any other failure is
 * fatal by design. */
static void check_alloc(void)
{
    fn4 ra = (fn4)real_fn(0x20c), oa = (fn4)our_fn(0x20c);
    fn3 rr = (fn3)real_fn(0x210), or_ = (fn3)our_fn(0x210);
    fn1 rf = (fn1)real_fn(0x214), of = (fn1)our_fn(0x214);
    uint8_t *rp = (uint8_t *)1, *op = (uint8_t *)1;
    uint32_t size = 1 + rnd() % 300, zero = rnd() & 1, nsize, i, rv, ov;
    if (!oa || !or_ || !of) return;

    rv = ra(rnd() & 1 ? 4 : 1, size, P(&rp), zero);
    ov = oa(rnd() & 1 ? 4 : 1, size, P(&op), zero);
    if (rv != ov || !rp != !op) { printf("  +0x20c alloc DIFFER real=%#x ours=%#x\n", rv, ov); differ++; return; }
    if (zero)
        for (i = 0; i < size; i++)
            if (rp[i] != op[i]) { printf("  +0x20c zero-fill DIFFER at %u\n", i); differ++; break; }
    for (i = 0; i < size; i++) rp[i] = op[i] = (uint8_t)rnd();

    nsize = 1 + rnd() % 600;
    rv = rr(rnd() & 1 ? 4 : 1, nsize, P(&rp));
    ov = or_(rnd() & 1 ? 4 : 1, nsize, P(&op));
    if (rv != ov || !rp != !op) { printf("  +0x210 realloc DIFFER real=%#x ours=%#x\n", rv, ov); differ++; return; }
    if (memcmp(rp, op, size < nsize ? size : nsize)) { printf("  +0x210 realloc contents DIFFER\n"); differ++; }

    {   /* flags 4 failing: -1, the old pointer put back */
        uint8_t *rk = rp, *ok = op;
        rv = rr(4, 0xfffffff0u, P(&rp));
        ov = or_(4, 0xfffffff0u, P(&op));
        if (rv != ov || (rp == rk) != (op == ok)) { printf("  +0x210 failing DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    }
    rv = rf(P(&rp)); ov = of(P(&op));
    if (rv != ov || rp != 0 || op != 0) { printf("  +0x214 free DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    rv = rf(P(&rp)); ov = of(P(&op));                   /* and of null */
    if (rv != ov) { printf("  +0x214 free(null) DIFFER\n"); differ++; }

    {   /* flags 4 failing on a fresh allocation */
        void *rq = (void *)1, *oq = (void *)1;
        rv = ra(4, 0xfffffff0u, P(&rq), 1);
        ov = oa(4, 0xfffffff0u, P(&oq), 1);
        if (rv != ov || rq != oq) { printf("  +0x20c failing DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    }
}

/* +0x21c memmove, overlapping either way. */
static void check_21c(void)
{
    static uint8_t rb[256], ob[256];
    fn3 r = (fn3)real_fn(0x21c), o = (fn3)our_fn(0x21c);
    uint32_t i, n = rnd() % 128, s = rnd() % 128, d = rnd() % 128, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof rb; i++) rb[i] = ob[i] = (uint8_t)rnd();
    rv = r(P(rb + d), P(rb + s), n);
    ov = o(P(ob + d), P(ob + s), n);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) { printf("  +0x21c memmove DIFFER\n"); differ++; }
}

/* ---- the growable array ------------------------------------------------------ */

/* struct fs98_host_list.  One per side, built identically; the services see
 * only their own.  Elements are compared by contents and returned element
 * pointers as offsets into their own side's data. */
struct hlist { int32_t count, cap, grow, size, iter, pend; uint8_t *data; };
static struct hlist rl, ol;
/* A bystander pair, permanently mid-walk, so removals from it are only ever
 * queued: the queue then holds entries for two lists, and a walk's end must
 * apply only its own. */
static struct hlist rby, oby;

#define REAL_QUEUE  ((struct hlist *)(DATA_VA + QUEUE_OFF))
#define OUR_QUEUE   ((struct hlist *)(our_data + QUEUE_OFF))

static char pool[64];                   /* what an indirect list's elements point into */

/* A fresh pair.  `indirect` makes 8-byte elements whose first or second dword
 * points into the pool, so +0x310 can follow it. */
static void list_init(int indirect, uint32_t ptr_off)
{
    int32_t i;
    free(rl.data); free(ol.data);
    free(rby.data); free(oby.data);
    memset(&rby, 0, sizeof rby);
    rby.size = 4; rby.grow = 4; rby.count = rby.cap = 30; rby.iter = 1;
    oby = rby;
    rby.data = calloc(30, 4); oby.data = calloc(30, 4);
    /* Sometimes the queue grows one entry at a time, so that a flush which
     * queues again (a nested walk's end) moves it under its own feet. */
    REAL_QUEUE->grow = OUR_QUEUE->grow = 0x20;
    if (rnd() % 4 == 0) {           /* the queue is empty here (seed_data) */
        free(REAL_QUEUE->data); free(OUR_QUEUE->data);
        REAL_QUEUE->data = OUR_QUEUE->data = 0;
        REAL_QUEUE->cap = OUR_QUEUE->cap = 0;
        REAL_QUEUE->grow = OUR_QUEUE->grow = 1;
    }
    memset(&rl, 0, sizeof rl);
    rl.size = indirect ? 8 : 1 + rnd() % 24;
    rl.grow = 1 + rnd() % 6;
    rl.count = rnd() % 12;
    rl.cap = rl.count + (rnd() % 3 ? rnd() % 4 : 0);
    rl.iter = (rnd() % 8 == 0);
    ol = rl;
    if (rl.cap) {
        rl.data = malloc(rl.cap * rl.size);
        ol.data = malloc(rl.cap * rl.size);
        for (i = 0; i < rl.cap * rl.size; i++) rl.data[i] = ol.data[i] = (uint8_t)rnd();
        if (indirect)
            for (i = 0; i < rl.cap; i++) {
                uint32_t p = P(pool + rnd() % 8);
                memcpy(rl.data + i * 8 + ptr_off, &p, 4);
                memcpy(ol.data + i * 8 + ptr_off, &p, 4);
            }
    }
}

/* Removals queued against list `l` on its side's queue. */
static int32_t queued(const struct hlist *q, const struct hlist *l)
{
    int32_t i, n = 0;
    for (i = 0; i < q->count; i++)
        if (*(uint32_t *)(q->data + i * 8) == P(l)) n++;
    return n;
}

/* A removal is allowed only while the list keeps more elements than removals
 * queued against it: retail drops the count once per queued removal whatever
 * its index, and a negative count makes the next append write below the data
 * -- a caller's error, not something to test. */
static int may_remove(const struct hlist *q, const struct hlist *l, int now)
{
    return l->count > queued(q, l) + (now ? 1 : 0);
}

static void list_compare(const char *name)
{
    int32_t i;
    if (rl.count != ol.count || rl.cap != ol.cap || rl.grow != ol.grow || rl.size != ol.size ||
        rl.iter != ol.iter || rl.pend != ol.pend || !rl.data != !ol.data) {
        printf("  %-16s LIST DIFFER real={%d,%d,%d,%d,%d,%d,%s} ours={%d,%d,%d,%d,%d,%d,%s}\n", name,
               rl.count, rl.cap, rl.grow, rl.size, rl.iter, rl.pend, rl.data ? "p" : "0",
               ol.count, ol.cap, ol.grow, ol.size, ol.iter, ol.pend, ol.data ? "p" : "0");
        differ++;
        return;
    }
    if (rby.count != oby.count || rby.pend != oby.pend) {
        printf("  %-16s BYSTANDER DIFFER real=%d,%d ours=%d,%d\n", name, rby.count, rby.pend, oby.count, oby.pend);
        differ++;
        return;
    }
    if (rl.count > 0 && rl.data && memcmp(rl.data, ol.data, rl.count * rl.size)) {
        printf("  %-16s LIST CONTENTS DIFFER\n", name); differ++; return;
    }
    {   /* the queue: entries name each side's own list */
        struct hlist *rq = REAL_QUEUE, *oq = OUR_QUEUE;
        if (rq->count != oq->count || rq->cap != oq->cap) {
            printf("  %-16s QUEUE DIFFER real=%d/%d ours=%d/%d\n", name, rq->count, rq->cap, oq->count, oq->cap);
            differ++; return;
        }
        for (i = 0; i < rq->count; i++) {
            uint32_t *re = (uint32_t *)(rq->data + i * 8), *oe = (uint32_t *)(oq->data + i * 8);
            uint32_t rt = re[0] == P(&rl) ? 1 : re[0] == P(&rby) ? 2 : 0;
            uint32_t ot = oe[0] == P(&ol) ? 1 : oe[0] == P(&oby) ? 2 : 0;
            if (rt != ot || re[1] != oe[1]) {
                printf("  %-16s QUEUE ENTRY %d DIFFER\n", name, i); differ++; return;
            }
        }
    }
    check_state(name);
}

/* Offset of a returned element pointer from its list's data, -1 for null. */
static int32_t elem_off(uint32_t p, const struct hlist *l)
{
    return p ? (int32_t)(p - P(l->data)) : -1;
}

/* A walk's callback.  Each side gets its own context, and both follow one
 * script drawn before either walks, so they make the same decisions as long
 * as the lists agree; what each visit saw is logged and compared. */
#define VISITS 64
struct visit { uint32_t r, act, stop; };
static struct visit script[VISITS];

struct walkctx {
    struct hlist *l;
    struct hlist *by;
    struct hlist *q;
    int real;
    int depth;
    int n;
    int32_t log[VISITS * 2];
};

static uint32_t __stdcall walk_cb(uint32_t elem, uint32_t c)
{
    struct walkctx *w = (struct walkctx *)(uintptr_t)c;
    const struct visit *v;
    if (w->n >= VISITS) return 0x7777;
    v = &script[w->n];
    w->log[w->n * 2] = (int32_t)(elem - P(w->l->data));
    w->log[w->n * 2 + 1] = w->l->count;
    w->n++;
    switch (v->act) {
    case 1:     /* remove an element, queued since we are walking */
        if (may_remove(w->q, w->l, 0)) {
            fn2 f = (fn2)(w->real ? real_fn(0x300) : our_fn(0x300));
            f(P(w->l), P(w->l->data) + (v->r % w->l->count) * w->l->size + (v->r >> 16) % w->l->size);
        }
        break;
    case 2:     /* append */
        if (w->l->count < 40) {
            fn1 f = (fn1)(w->real ? real_fn(0x2f8) : our_fn(0x2f8));
            f(P(w->l));
        }
        break;
    case 4:     /* remove from the bystander: queued */
        if (may_remove(w->q, w->by, 0)) {
            fn2 f = (fn2)(w->real ? real_fn(0x300) : our_fn(0x300));
            f(P(w->by), P(w->by->data) + (v->r % w->by->count) * 4);
        }
        break;
    case 3:     /* a nested walk, once */
        if (w->depth == 0) {
            fn3 f = (fn3)(w->real ? real_fn(v->r & 1 ? 0x308 : 0x30c) : our_fn(v->r & 1 ? 0x308 : 0x30c));
            w->depth++;
            f(P(w->l), c, P(walk_cb));
            w->depth--;
        }
        break;
    }
    return v->stop;
}

static void list_op(void)
{
    uint32_t op = rnd() % 8, rv, ov;
    switch (op) {
    case 0: case 1: {                                   /* +0x2f8 append */
        fn1 r = (fn1)real_fn(0x2f8), o = (fn1)our_fn(0x2f8);
        int32_t i;
        if (!o || rl.count >= 40) return;
        rv = r(P(&rl)); ov = o(P(&ol));
        if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
            printf("  +0x2f8 append RET DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
            differ++;
        }
        list_compare("+0x2f8 append");
        if (rl.iter == 0 && rv && ov)                   /* give it contents, both alike */
            for (i = 0; i < rl.size; i++)
                ((uint8_t *)(uintptr_t)rv)[i] = ((uint8_t *)(uintptr_t)ov)[i] = (uint8_t)rnd();
        break;
    }
    case 2: {                                           /* +0x300 remove */
        fn2 r = (fn2)real_fn(0x300), o = (fn2)our_fn(0x300);
        uint32_t i, sub;
        if (!o || !may_remove(REAL_QUEUE, &rl, !rl.iter) || !may_remove(OUR_QUEUE, &ol, !ol.iter)) return;
        i = rnd() % (rl.count + 1);                     /* the end itself: a count drop, no move */
        sub = rnd() % rl.size;
        rv = r(P(&rl), P(rl.data) + i * rl.size + sub);
        ov = o(P(&ol), P(ol.data) + i * ol.size + sub);
        if ((rv > 0xffff ? 1 : rv) != (ov > 0xffff ? 1 : ov)) {   /* a queue entry, or the size, or 0 */
            printf("  +0x300 remove RET DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
        }
        list_compare("+0x300 remove");
        break;
    }
    case 3: case 4: {                                   /* +0x308 / +0x30c walk */
        uint32_t s = op == 3 ? 0x308 : 0x30c, i;
        fn3 r = (fn3)real_fn(s), o = (fn3)our_fn(s);
        static struct walkctx rw, ow;
        if (!o) return;
        uint32_t dense = rnd() % 4 == 0;    /* mostly removals and nesting: several queued at once */
        for (i = 0; i < VISITS; i++) {
            script[i].r = rnd();
            script[i].act = rnd() % 7;                  /* 1 remove, 2 append, 3 nest, 4 bystander */
            if (script[i].act == 3 && rnd() % 3 && !dense) script[i].act = 0;
            if (dense && rnd() % 2) script[i].act = rnd() % 3 ? 1 : 3;
            script[i].stop = rnd() % 12 == 0 ? rnd() | 1 : 0;
        }
        memset(&rw, 0, sizeof rw); memset(&ow, 0, sizeof ow);
        rw.l = &rl; rw.by = &rby; rw.q = REAL_QUEUE; rw.real = 1;
        ow.l = &ol; ow.by = &oby; ow.q = OUR_QUEUE;
        rv = r(P(&rl), P(&rw), P(walk_cb));
        ov = o(P(&ol), P(&ow), P(walk_cb));
        if (rv != ov || rw.n != ow.n || memcmp(rw.log, ow.log, sizeof rw.log)) {
            printf("  +%#05x walk DIFFER ret real=%#x ours=%#x, visits %d/%d\n", s, rv, ov, rw.n, ow.n);
            differ++;
        }
        list_compare(op == 3 ? "+0x308 walk" : "+0x30c walk back");
        break;
    }
    case 5: case 6: {                                   /* +0x310 find */
        fn5 r = (fn5)real_fn(0x310), o = (fn5)our_fn(0x310);
        static uint8_t key[32];
        uint32_t off = rnd() % rl.size, len = rnd() % (rl.size - off + 1), i;
        if (!o) return;
        if (rl.count > 0 && rnd() % 4)                  /* usually a key some element holds */
            memcpy(key, rl.data + (rnd() % rl.count) * rl.size + off, len);
        else
            for (i = 0; i < len; i++) key[i] = (uint8_t)rnd();
        if (rnd() % 8 == 0) len = 0;                    /* matches at once */
        rv = r(P(&rl), off, P(key), len, 0);
        ov = o(P(&ol), off, P(key), len, 0);
        if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
            printf("  +0x310 find DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
            differ++;
        }
        break;
    }
    case 7:                                             /* +0x304 clears the count */
        if (rl.iter == 0 && rnd() % 4 == 0) {
            ((fn1)real_fn(0x304))(P(&rl)); ((fn1)our_fn(0x304))(P(&ol));
            list_compare("+0x304 clear");
        }
        break;
    }
}

/* +0x310 with `indirect`: elements hold pointers into the pool, the key is a
 * pointer to a pointer into it. */
static void check_find_indirect(void)
{
    fn5 r = (fn5)real_fn(0x310), o = (fn5)our_fn(0x310);
    uint32_t off = (rnd() & 1) * 4, len = rnd() % 9, rv, ov, i;
    char *k;
    if (!o) return;
    for (i = 0; i < sizeof pool - 1; i++) pool[i] = (char)('a' + rnd() % 3);
    list_init(1, off);
    k = pool + rnd() % 8;
    rv = r(P(&rl), off, P(&k), len, 1);
    ov = o(P(&ol), off, P(&k), len, 1);
    if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
        printf("  +0x310 find-indirect DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
        differ++;
    }
}

/* A run of operations on one pair, ended by +0x2fc on each side. */
static void check_lists(void)
{
    fn1 r = (fn1)real_fn(0x2fc), o = (fn1)our_fn(0x2fc);
    uint32_t n = 4 + rnd() % 16, i, rv, ov;
    list_init(0, 0);
    for (i = 0; i < n && !differ; i++)
        list_op();
    if (!o) return;
    rv = r(P(&rl)); ov = o(P(&ol));
    if ((rv == P(&rl.data) ? 1 : rv) != (ov == P(&ol.data) ? 1 : ov)) {
        printf("  +0x2fc free DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
    }
    list_compare("+0x2fc free");
    rv = r(P(&rl)); ov = o(P(&ol));                     /* and with nothing to free */
    if ((rv == P(&rl.data) ? 1 : rv) != (ov == P(&ol.data) ? 1 : ov)) {
        printf("  +0x2fc free(null) DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
    }
}

/* ---- the command line -------------------------------------------------------- */

static const char *const opt_names[] = { "a", "ab", "fly", "mode", "x", "" };
static const char *const opt_words[] = { "fs98", "12", "-3.5", "1e3", "abc", "c:\\fs98", "  ", "", "0x10", "7" };

/* A random command line of options in both forms, values and stray words,
 * with irregular spacing; and the byte before it, a space or not. */
static void make_cmdline(void)
{
    char *p = fake_cl + 1;
    uint32_t n = rnd() % 7, i;
    fake_cl[0] = rnd() & 1 ? ' ' : 'q';
    p += sprintf(p, "%s", rnd() % 4 ? "fltsim98.exe" : "");
    for (i = 0; i < n; i++) {
        uint32_t k = rnd() % 5;
        p += sprintf(p, "%s", rnd() % 4 ? " " : rnd() & 1 ? "   " : "");
        if (k < 3)
            p += sprintf(p, "%c%s%s%s", rnd() & 1 ? '/' : '-', opt_names[rnd() % 6],
                         rnd() % 3 ? ":" : rnd() & 1 ? " " : "", opt_words[rnd() % 10]);
        else
            p += sprintf(p, "%s", opt_words[rnd() % 10]);
    }
    if (rnd() % 4 == 0) p += sprintf(p, "  ");
}

/* +0x98 into a guarded buffer (the byte before it included -- an empty value
 * trims from there), +0x9c, +0xa0 and +0xa4, for one name. */
static void check_cmdline(void)
{
    static uint8_t rb[80], ob[80];
    const char *nm = opt_names[rnd() % 6];
    uint32_t len = 1 + rnd() % 40, rv, ov, ri, oi;
    double rd, od;
    fn3 r98 = (fn3)real_fn(0x98), o98 = (fn3)our_fn(0x98);
    fn2 r9c = (fn2)real_fn(0x9c), o9c = (fn2)our_fn(0x9c);
    fn2 ra0 = (fn2)real_fn(0xa0), oa0 = (fn2)our_fn(0xa0);
    fn1 ra4 = (fn1)real_fn(0xa4), oa4 = (fn1)our_fn(0xa4);
    if (!o98 || !o9c || !oa0 || !oa4) return;
    make_cmdline();
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rb[7] = ob[7] = rnd() & 1 ? ' ' : 'z';
    rv = r98(P(nm), P(rb + 8), len);
    ov = o98(P(nm), P(ob + 8), len);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x098 option(\"%s\", %u) DIFFER real=%d ours=%d  cl=[%s]\n", nm, len, (int)rv, (int)ov, fake_cl + 1);
        differ++;
    }
    ri = oi = rnd();
    rv = r9c(P(nm), P(&ri)); ov = o9c(P(nm), P(&oi));
    if (rv != ov || ri != oi) { printf("  +0x09c option-int DIFFER  cl=[%s]\n", fake_cl + 1); differ++; }
    rd = od = (double)rnd();
    rv = ra0(P(nm), P(&rd)); ov = oa0(P(nm), P(&od));
    if (rv != ov || memcmp(&rd, &od, 8)) { printf("  +0x0a0 option-double DIFFER  cl=[%s]\n", fake_cl + 1); differ++; }
    rv = ra4(P(nm)); ov = oa4(P(nm));
    if (rv != ov) { printf("  +0x0a4 option-given DIFFER real=%u ours=%u  cl=[%s]\n", rv, ov, fake_cl + 1); differ++; }
}

/* ---- file extensions --------------------------------------------------------- */

static const char *const ext_samples[] = {
    ".bgl", ".BGL", ".Wav", ".xyz", ".q", ".abcd", ".bgl2", ".", ".zz", ".cfg", ".SC1", ".r81", ".new", ""
};

/* +0x1ac: each side upper-cases its own copy. */
static void check_1ac(void)
{
    static char rs[16], os[16];
    fn2 r = (fn2)real_fn(0x1ac), o = (fn2)our_fn(0x1ac);
    uint32_t ri = rnd(), oi = ri, rv, ov;
    if (!o) return;
    strcpy(rs, ext_samples[rnd() % 14]); strcpy(os, rs);
    rv = r(P(rs), P(&ri)); ov = o(P(os), P(&oi));
    if (rv != ov || ri != oi || strcmp(rs, os)) {
        printf("  +0x1ac ext(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", rs, (int)rv, ri, (int)ov, oi);
        differ++;
    }
    check_state("+0x1ac ext");
}

static const char *const path_dirs[] = { "", "c:", "C:\\", "\\\\srv", "\\\\srv\\share", "dir\\", "a\\b\\", "\\", "\\\\" };
static const char *const path_names[] = { "", "name", "x", "a.b", "scenery", "..", "n m" };

/* +0x188 over composed paths, with each part's buffer and the id sometimes
 * null. */
static void check_188(void)
{
    static char path[160], rb[3][160], ob[3][160];
    fn5 r = (fn5)real_fn(0x188), o = (fn5)our_fn(0x188);
    uint32_t ri = rnd(), oi = ri, rv, ov, k, mask = rnd();
    if (!o) return;
    sprintf(path, "%s%s%s%s", path_dirs[rnd() % 9], rnd() & 1 ? "sub\\" : "",
            path_names[rnd() % 7], ext_samples[rnd() % 14]);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
#define PART(b, k) ((mask >> (k)) & 1 ? P((b)[k]) : 0)
    k = rnd() % 32 ? P(path) : 0;
    rv = r(k, PART(rb, 0), PART(rb, 1), PART(rb, 2), mask & 8 ? P(&ri) : 0);
    ov = o(k, PART(ob, 0), PART(ob, 1), PART(ob, 2), mask & 8 ? P(&oi) : 0);
    if (rv != ov || ri != oi || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x188 split(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", path, (int)rv, ri, (int)ov, oi);
        differ++;
    }
    check_state("+0x188 split");
}

/* ---- the C-runtime wrappers -------------------------------------------------- */

/* A stdcall comparator on the first dword. */
static uint32_t __stdcall cmp_first(uint32_t a, uint32_t b)
{
    int32_t x = *(int32_t *)(uintptr_t)a, y = *(int32_t *)(uintptr_t)b;
    return (uint32_t)(x < y ? -1 : x > y);
}

/* +0x22c qsort (ties included: both sides run one MSVCRT qsort) and +0x234
 * bsearch over the sorted result. */
static void check_sort(void)
{
    static int32_t ra[3 * 40], oa[3 * 40];
    fn4 rq = (fn4)real_fn(0x22c), oq = (fn4)our_fn(0x22c);
    fn5 rb = (fn5)real_fn(0x234), ob = (fn5)our_fn(0x234);
    uint32_t n = rnd() % 40, w = rnd() & 1 ? 4 : 12, i, rv, ov;
    int32_t key;
    if (!oq || !ob) return;
    for (i = 0; i < 3 * 40; i++) ra[i] = oa[i] = (int32_t)(rnd() % 16) - 4;
    rq(P(ra), n, w, P(cmp_first)); oq(P(oa), n, w, P(cmp_first));
    if (memcmp(ra, oa, sizeof ra)) { printf("  +0x22c qsort DIFFER\n"); differ++; return; }
    check_state("+0x22c qsort");
    key = (int32_t)(rnd() % 18) - 5;
    rv = rb(P(&key), P(ra), n, w, P(cmp_first));
    ov = ob(P(&key), P(oa), n, w, P(cmp_first));
    if ((rv ? (int32_t)(rv - P(ra)) : -1) != (ov ? (int32_t)(ov - P(oa)) : -1)) {
        printf("  +0x234 bsearch DIFFER\n"); differ++;
    }
}

/* +0x230 swap, +0x2ac strncpy, +0x2b4 strdup, +0x2c0 _strcmpi, +0x2c8 strip. */
static void check_crt(void)
{
    static uint8_t ra[128], oa[128];
    static char rs[96], os[96], src[64];
    uint32_t rv, ov, i, n;
    fn3 r230 = (fn3)real_fn(0x230), o230 = (fn3)our_fn(0x230);
    fn3 r2ac = (fn3)real_fn(0x2ac), o2ac = (fn3)our_fn(0x2ac);
    fn1 r2b4 = (fn1)real_fn(0x2b4), o2b4 = (fn1)our_fn(0x2b4);
    fn2 r2c0 = (fn2)real_fn(0x2c0), o2c0 = (fn2)our_fn(0x2c0);
    fn2 r2c8 = (fn2)real_fn(0x2c8), o2c8 = (fn2)our_fn(0x2c8);
    if (!o230 || !o2ac || !o2b4 || !o2c0 || !o2c8) return;

    for (i = 0; i < sizeof ra; i++) ra[i] = oa[i] = (uint8_t)rnd();
    n = rnd() % 64;
    rv = r230(P(ra), P(ra + 64), n); ov = o230(P(oa), P(oa + 64), n);
    if (rv != ov || memcmp(ra, oa, sizeof ra)) { printf("  +0x230 swap DIFFER\n"); differ++; }

    rnd_str(src, rnd() & 1 ? 16 : sizeof src);
    memset(rs, 0xaa, sizeof rs); memset(os, 0xaa, sizeof os);
    n = rnd() % 80;
    rv = r2ac(P(rs), P(src), n); ov = o2ac(P(os), P(src), n);
    if (rv - P(rs) != ov - P(os) || memcmp(rs, os, sizeof rs)) { printf("  +0x2ac strncpy DIFFER\n"); differ++; }

    rv = r2b4(rnd() % 8 ? P(src) : 0);
    ov = o2b4(rv ? P(src) : 0);
    if (!rv != !ov || (rv && strcmp((char *)(uintptr_t)rv, (char *)(uintptr_t)ov))) {
        printf("  +0x2b4 strdup DIFFER\n"); differ++;
    }
    free((void *)(uintptr_t)rv); free((void *)(uintptr_t)ov);

    rnd_str(rs, 24);
    if (rnd() & 1) { strcpy(os, rs); for (i = 0; os[i]; i++) if (rnd() & 1) os[i] ^= 0x20 * ((os[i] | 0x20) >= 'a' && (os[i] | 0x20) <= 'z'); }
    else rnd_str(os, 24);
    rv = r2c0(P(rs), P(os)); ov = o2c0(P(rs), P(os));
    if (rv != ov) { printf("  +0x2c0 strcmpi DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }

    for (i = 0, n = rnd() % 60; i < n; i++) rs[i] = "ab\\ c"[rnd() % 5];
    rs[n] = 0; strcpy(os, rs);
    memset(rs + n + 1, 0x55, sizeof rs - n - 1); memset(os + n + 1, 0x55, sizeof os - n - 1);
    i = "ab\\ cz"[rnd() % 6];
    rv = r2c8(P(rs), i); ov = o2c8(P(os), i);
    if (rv - P(rs) != ov - P(os) || memcmp(rs, os, sizeof rs)) { printf("  +0x2c8 strip DIFFER\n"); differ++; }
}

/* ---- string resources -------------------------------------------------------- */

/* Ids worth asking for: the input names, strings only ENGLISH.FLL, SIMSUI or
 * DIALOG has, the host's own 9, and ones nobody has. */
static const uint32_t str_ids[] = { 9, 0x8012, 0x8013, 0x2201, 0x73, 0xb000, 0xb001, 0xf000, 0xf001,
                                    0xf100, 0xf800, 0xf8d0, 1, 0x5c, 0x7fff };

/* +0x2a0 into a guarded buffer of random length, +0x2a4. */
static void check_strings(void)
{
    static char rb[0x180], ob[0x180];
    fn3 r = (fn3)real_fn(0x2a0), o = (fn3)our_fn(0x2a0);
    fn2 r4 = (fn2)real_fn(0x2a4), o4 = (fn2)our_fn(0x2a4);
    uint32_t id = str_ids[rnd() % 15], len = rnd() % 4 ? 0x100 : rnd() % 12, rv, ov;
    char *rp = (char *)1, *op = (char *)1;
    if (!o || !o4) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r(id, P(rb), len); ov = o(id, P(ob), len);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x2a0 string(%#x, %u) DIFFER real=%u ours=%u\n", id, len, rv, ov); differ++;
    }
    check_state("+0x2a0 string");
    rv = r4(id, P(&rp)); ov = o4(id, P(&op));
    if (rv != ov || !rp != !op || (rp && strcmp(rp, op))) {
        printf("  +0x2a4 string-dup(%#x) DIFFER real=%u ours=%u\n", id, rv, ov); differ++;
    }
    free(rp); free(op);
    check_state("+0x2a4 string-dup");
}

/* +0x298 (a module's HMODULE onto the string list) and +0x29c (one off it). */
static void check_strmods(void)
{
    fn2 r8 = (fn2)real_fn(0x298), o8 = (fn2)our_fn(0x298);
    fn1 rc = (fn1)real_fn(0x29c), oc = (fn1)our_fn(0x29c);
    uint32_t id = rnd() % 10, ro = rnd(), oo = ro, rv, ov, h;
    if (!o8 || !oc) return;
    if (*(uint32_t *)(DATA_VA + STRMODS_OFF) < 12) {
        rv = r8(id, P(&ro)); ov = o8(id, P(&oo));
        if (rv != ov || ro != oo) { printf("  +0x298 add-strings(%u) DIFFER real=%d/%#x ours=%d/%#x\n", id, (int)rv, ro, (int)ov, oo); differ++; }
        check_state("+0x298 add-strings");
    }
    h = rnd() % 5 ? strmod[rnd() % 4] : rnd();
    rv = rc(h); ov = oc(h);
    if (rv != ov) { printf("  +0x29c drop-strings DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
    check_state("+0x29c drop-strings");
}

/* +0x1d0 (an input's name) and +0x1d4 (a name's input). */
static void check_inputs(void)
{
    static char rb[96], ob[96], name[48];
    fn3 r0 = (fn3)real_fn(0x1d0), o0 = (fn3)our_fn(0x1d0);
    fn2 r4 = (fn2)real_fn(0x1d4), o4 = (fn2)our_fn(0x1d4);
    int32_t code = (int32_t)(rnd() % 0x58) - 4;
    uint32_t rv, ov, ro, oo;
    if (!o0 || !o4) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r0((uint32_t)code, P(rb), rnd()); ov = o0((uint32_t)code, P(ob), rnd());
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x1d0 input-name(%d) DIFFER real=%#x ours=%#x [%s] [%s]\n", code, rv, ov, rb, ob); differ++;
    }
    check_state("+0x1d0 input-name");
    if (rnd() & 1)
        strcpy(name, rb);                       /* a name it made */
    else
        sprintf(name, "%s%s%d", prefixes[rnd() % 5], rnd() & 1 ? prefixes[rnd() % 5] : "",
                (int)(rnd() % 0x50) - 3);
    ro = oo = rnd();
    rv = r4(P(name), P(&ro)); ov = o4(P(name), P(&oo));
    if (rv != ov || ro != oo) {
        printf("  +0x1d4 input-code(\"%s\") DIFFER real=%#x/%d ours=%#x/%d\n", name, rv, (int)ro, ov, (int)oo); differ++;
    }
    check_state("+0x1d4 input-code");
}

/* ---- names and paths --------------------------------------------------------- */

/* +0x178 and +0x18c over strings drawn from a set that has every refused
 * character, a space, high bytes, and sometimes nothing. */
static void check_names(void)
{
    static const char cs[] = "aZ09._-\"&*/:<>?\\|! }~#{\x80\xe9\xff";
    static char s[24];
    fn1 r8 = (fn1)real_fn(0x178), o8 = (fn1)our_fn(0x178);
    fn1 rc = (fn1)real_fn(0x18c), oc = (fn1)our_fn(0x18c);
    uint32_t n = rnd() % 8, i, p;
    if (!o8 || !oc) return;
    for (i = 0; i < n; i++)
        s[i] = rnd() % 3 ? cs[rnd() % 7] : cs[rnd() % (sizeof cs - 1)];
    s[n] = 0;
    p = rnd() % 32 ? P(s) : 0;
    if (r8(p) != o8(p)) { printf("  +0x178 file-name(\"%s\") DIFFER\n", s); differ++; }
    if (rc(p) != oc(p)) { printf("  +0x18c path-name(\"%s\") DIFFER\n", s); differ++; }
}

/* +0x184 over parts with and without separators, sometimes long enough not
 * to fit. */
static void check_184(void)
{
    static const char *const drives[] = { "", "c", "C:", "c:\\", "\\\\srv", "ab", "\\" };
    static const char *const parts[] = { "", "fs98", "\\fs98", "fs98\\", "a\\b\\", "\\", "x.bgl", "\\\\" };
    static char dir[200], name[300], rb[0x120], ob[0x120];
    fn4 r = (fn4)real_fn(0x184), o = (fn4)our_fn(0x184);
    uint32_t rv, ov, k;
    const char *d;
    if (!o) return;
    d = drives[rnd() % 7];
    strcpy(dir, parts[rnd() % 8]);
    strcpy(name, parts[rnd() % 8]);
    if (rnd() % 4 == 0) { k = strlen(dir); memset(dir + k, 'd', 60 + rnd() % 130); dir[k + 60 + rnd() % 130] = 0; dir[199] = 0; }
    if (rnd() % 4 == 0) { k = strlen(name); memset(name + k, 'n', 40 + rnd() % 150); name[k + 40 + rnd() % 150] = 0; name[199] = 0; }
    if (rnd() % 8 == 0) {           /* a result right at the limit: dir plus name */
        k = 0x102 + rnd() % 5;
        strcpy(dir, "d"); memset(name, 'n', k - 2); name[k - 2] = 0;
        d = "";
    }
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    k = rnd();
    rv = r(k % 32 ? P(rb) : 0, k & 32 ? P(d) : 0, k & 64 ? P(dir) : 0, k & 128 ? P(name) : 0);
    ov = o(k % 32 ? P(ob) : 0, k & 32 ? P(d) : 0, k & 64 ? P(dir) : 0, k & 128 ? P(name) : 0);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x184 join(\"%s\",\"%.20s\",\"%.20s\") DIFFER real=%d ours=%d\n", d, dir, name, (int)rv, (int)ov);
        differ++;
    }
}

/* ---- image buffers ----------------------------------------------------------- */

/* Compare two images: the header but its pointers, the pixels, and the row
 * table as offsets into the pixels. */
static int img_differ(const uint32_t *r, const uint32_t *o)
{
    int32_t i;
    if (!r != !o) return 1;
    if (!r) return 0;
    if (r[0] != o[0] || r[1] != o[1] || r[2] != o[2] || r[3] != o[3] || r[5] != o[5]) return 1;
    if (!r[6] != !o[6] || !r[4] != !o[4]) return 1;
    if (r[6] && memcmp((void *)(uintptr_t)r[6], (void *)(uintptr_t)o[6], r[5])) return 1;
    for (i = 0; r[4] && i < (int32_t)r[2]; i++)
        if (((uint32_t *)(uintptr_t)r[4])[i] - r[6] != ((uint32_t *)(uintptr_t)o[4])[i] - o[6]) return 1;
    return 0;
}

/* +0x1b4 create (every format, and some that are not), scribble, +0x1bc
 * resize, +0x1b8 free (and of null). */
static void check_images(void)
{
    fn4 rmk = (fn4)real_fn(0x1b4), omk = (fn4)our_fn(0x1b4);
    fn3 rrs = (fn3)real_fn(0x1bc), ors = (fn3)our_fn(0x1bc);
    fn1 rfr = (fn1)real_fn(0x1b8), ofr = (fn1)our_fn(0x1b8);
    uint32_t *ri = (uint32_t *)1, *oi = (uint32_t *)1, rv, ov, w, h, f, i;
    if (!omk || !ors || !ofr) return;
    f = (uint32_t)((int32_t)(rnd() % 9) - 1);
    w = rnd() % 41; h = rnd() % 21;
    rv = rmk(P(&ri), w, h, f); ov = omk(P(&oi), w, h, f);
    if (rv != ov || (rv == 0 && img_differ(ri, oi)) || (rv && ri != oi)) {
        printf("  +0x1b4 image(%u,%u,%d) DIFFER real=%d ours=%d\n", w, h, (int)f, (int)rv, (int)ov); differ++; return;
    }
    if (rv) return;
    for (i = 0; i < ri[5]; i++)
        ((uint8_t *)(uintptr_t)ri[6])[i] = ((uint8_t *)(uintptr_t)oi[6])[i] = (uint8_t)rnd();
    w = rnd() % 41; h = rnd() % 21;
    rv = rrs(P(ri), w, h); ov = ors(P(oi), w, h);
    if (rv != ov || img_differ(ri, oi)) { printf("  +0x1bc resize(%u,%u) DIFFER\n", w, h); differ++; }
    rv = rfr(P(ri)); ov = ofr(P(oi));
    if (rv != ov) { printf("  +0x1b8 free DIFFER\n"); differ++; }
    if (rfr(0) != ofr(0)) { printf("  +0x1b8 free(null) DIFFER\n"); differ++; }
}

/* ---- accelerator tables ------------------------------------------------------ */

/* A few entries for tables to hold, so that removals find matches. */
static const uint32_t accel_pool[6][3] = {
    { 0x9c40, 8, 0x10041 }, { 0x9c41, 9, 0x10042 }, { 0x9c40, 8, 0x10042 },
    { 7, 0, 0x10070 }, { 0x9c40, 2, 0x10041 }, { 1, 1, 1 },
};

/* The table list: up to four tables of up to six pool entries each. */
static void seed_accels(void)
{
    uint32_t recs[4 * 4], m = rnd() % 5, i, j, n;
    uint8_t *ra[4], *oa[4];
    for (i = 0; i < m; i++) {
        n = rnd() % 7;
        recs[i * 4 + 0] = rnd() & 1;
        recs[i * 4 + 1] = 1 + rnd() % 12;
        recs[i * 4 + 3] = n;
        ra[i] = oa[i] = 0;
        if (n || rnd() & 1) {
            ra[i] = malloc(n * 12 + 1); oa[i] = malloc(n * 12 + 1);
            for (j = 0; j < n; j++) {
                const uint32_t *e = accel_pool[rnd() % 6];
                memcpy(ra[i] + 12 * j, e, 12); memcpy(oa[i] + 12 * j, e, 12);
            }
        }
        recs[i * 4 + 2] = (uint32_t)(uintptr_t)ra[i];
    }
    seed_list(ACCEL_OFF, m, 0x10, recs);
    for (i = 0; i < m; i++)
        (*(uint32_t **)(our_data + ACCEL_OFF + 0x18))[i * 4 + 2] = (uint32_t)(uintptr_t)oa[i];
    if (rnd() % 8 == 0)     /* the id counter about to wrap, or at 0 (the next table's id) */
        *(uint32_t *)(DATA_VA + ACCEL_OFF + 0x1c) = *(uint32_t *)(our_data + ACCEL_OFF + 0x1c) =
            rnd() & 1 ? 0xffffffffu : 0;
}

/* The tables, field by field, entries through each side's own pointer. */
static int accels_differ(void)
{
    uint32_t n = *(uint32_t *)(DATA_VA + ACCEL_OFF), i;
    uint32_t *r = *(uint32_t **)(DATA_VA + ACCEL_OFF + 0x18), *o = *(uint32_t **)(our_data + ACCEL_OFF + 0x18);
    if (n != *(uint32_t *)(our_data + ACCEL_OFF)) return 1;
    for (i = 0; i < n; i++) {
        uint32_t *rt = r + 4 * i, *ot = o + 4 * i;
        if (rt[0] != ot[0] || rt[1] != ot[1] || rt[3] != ot[3] || !rt[2] != !ot[2]) return 1;
        if (rt[2] && rt[3] < 4096 && memcmp((void *)(uintptr_t)rt[2], (void *)(uintptr_t)ot[2], rt[3] * 12)) return 1;
    }
    return 0;
}

/* An id worth asking about: one a table has, 0, or one nobody has. */
static uint32_t accel_id(void)
{
    uint32_t n = *(uint32_t *)(DATA_VA + ACCEL_OFF);
    uint32_t *r = *(uint32_t **)(DATA_VA + ACCEL_OFF + 0x18);
    if (n && rnd() % 4) return r[4 * (rnd() % n) + 1];
    return rnd() % 3 ? rnd() % 20 : 0;
}

static void check_accels(void)
{
    static uint32_t rbuf[512 * 3], obuf[512 * 3], ents[8 * 3];
    uint32_t ops = 1 + rnd() % 6, k;
    for (k = 0; k < ops && *(uint32_t *)(DATA_VA + ACCEL_OFF) < 12; k++) {
        uint32_t op = rnd() % 10, s = 0x1d8 + 4 * op, rv = 0, ov = 0, ro, oo, id = accel_id(), i, n;
        void *of = our_fn(s);
        void *rf = real_fn(s);
        if (!of) return;
        ro = oo = rnd();
        switch (op) {
        case 0:                         /* +0x1d8 new */
            rv = ((fn1)rf)(P(&ro)); ov = ((fn1)of)(P(&oo));
            break;
        case 1:                         /* +0x1dc destroy (through a pointer it zeroes) */
            ro = oo = id;
            rv = ((fn1)rf)(P(&ro)); ov = ((fn1)of)(P(&oo));
            break;
        case 2:                         /* +0x1e0 by index */
            i = rnd() % 6;
            rv = ((fn2)rf)(P(&ro), i); ov = ((fn2)of)(P(&oo), i);
            break;
        case 3:                         /* +0x1e4 from a resource */
            i = rnd() % 4 ? 103 + rnd() % 3 : rnd() % 300;
            rv = ((fn2)rf)(P(&ro), i); ov = ((fn2)of)(P(&oo), i);
            break;
        case 4: case 5: case 6:         /* +0x1e8 enable, +0x1ec disable, +0x1f0 clear */
            rv = ((fn1)rf)(id); ov = ((fn1)of)(id);
            break;
        case 7: case 8:                 /* +0x1f4 add, +0x1f8 remove */
            n = rnd() % 5;
            for (i = 0; i < n; i++) memcpy(ents + 3 * i, accel_pool[rnd() % 6], 12);
            rv = ((fn3)rf)(id, P(ents), n); ov = ((fn3)of)(id, P(ents), n);
            break;
        case 9: {                       /* +0x1fc read */
            uint32_t (__stdcall *r6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = (void *)rf;
            uint32_t (__stdcall *o6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = (void *)of;
            uint32_t start = rnd() % 8, mode = rnd() % 4 ? 2 : rnd() % 4, b = rnd() % 4;
            memset(rbuf, 0xaa, sizeof rbuf); memset(obuf, 0xaa, sizeof obuf);
            rv = r6(id, rnd(), start, mode, b ? P(rbuf) : 0, P(&ro));
            ov = o6(id, rnd(), start, mode, b ? P(obuf) : 0, P(&oo));
            if (memcmp(rbuf, obuf, sizeof rbuf)) {
                for (i = 0; i < 512 * 3 && rbuf[i] == obuf[i]; i++) ;
                printf("  +0x1fc read(id %u, start %u, mode %u) DIFFER at dword %u: real=%#x ours=%#x, n %u/%u\n",
                       id, start, mode, i, rbuf[i], obuf[i], ro, oo);
                differ++;
            }
            break;
        }
        }
        if (rv != ov || ro != oo) {
            printf("  +%#05x accel op DIFFER real=%d/%#x ours=%d/%#x\n", s, (int)rv, ro, (int)ov, oo); differ++;
        }
        if (accels_differ()) { printf("  +%#05x accel TABLES DIFFER\n", s); differ++; }
        check_state("accelerators");
        if (differ) return;
    }
}

/* ---- key names --------------------------------------------------------------- */

/* The host's key words, as its init loads them from ENGLISH.FLL (0x9520 on),
 * and the slots it keeps their copies in; the harness points both sides'
 * slots at one set of strings (pointers outside .data compare as they are). */
static const uint32_t kn_slot[10] = { 0x41bec0, 0x41bc80, 0x41bb64, 0x41bfe8, 0x41bb68,
                                      0x41bdac, 0x41bda4, 0x41c360, 0x41bc84, 0x41c104 };
static const char *const kn_text[10] = { "CTRL + ", "SHIFT + ", "ALT + ", "NumLock", "Num Enter",
                                         "Num /", "Break", "Menu", "Right Windows", "Left Windows" };

static void seed_keynames(void)
{
    uint32_t i;
    for (i = 0; i < 10; i++) {
        const char *t = kn_text[i];
        if (i >= 3 && rnd() % 16 == 0) t = kn_text[rnd() % 10];  /* now and then a key word out of place */
        if (i == 5 && rnd() % 8 == 0) t = kn_text[4];             /* Num / as Num Enter: which is tried first shows */
        *(uint32_t *)(DATA_VA + kn_slot[i] - DATA_VA) = (uint32_t)(uintptr_t)t;
        *(uint32_t *)(our_data + kn_slot[i] - DATA_VA) = (uint32_t)(uintptr_t)t;
    }
}

/* +0x200 over key codes (every virtual key, any modifiers) into a guarded
 * buffer of random length; +0x204 over the names it made and some others. */
static void check_keynames(void)
{
    static char rb[96], ob[96], name[96];
    static const char *const odd[] = { "", "ctrl + a", "CTRL + ", "Num Enterprise", "F1", "Space", "Num Enter", "CTRL + SHIFT + ALT + Break" };
    fn3 r0 = (fn3)real_fn(0x200), o0 = (fn3)our_fn(0x200);
    fn2 r4 = (fn2)real_fn(0x204), o4 = (fn2)our_fn(0x204);
    uint32_t key = (rnd() % 8) << 8 | rnd() % 0x100, len = rnd() % 4 ? 64 : 23 + rnd() % 20;
    /* (len must leave room past the modifier words, up to 21 bytes: retail
     * passes len - used - 1 on unchecked, and a negative count runs strncpy's
     * zero fill off the end of memory) */
    uint32_t rv, ov, ro, oo;
    if (!o0 || !o4) return;
    if (rnd() % 4 == 0) key = (key & 0x700) | (uint32_t)"\x00\x03\x13\x5b\x5c\x5d\x6f\x87\x90"[rnd() % 9];
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r0(key, P(rb), len); ov = o0(key, P(ob), len);
    if (rv > 0x10000) rv -= P(rb);
    if (ov > 0x10000) ov -= P(ob);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x200 key-name(%#x) DIFFER real=%#x ours=%#x [%s] [%s]\n", key, rv, ov, rb, ob); differ++;
    }
    if (rnd() % 4) { memcpy(name, rb, sizeof name); name[sizeof name - 1] = 0; }
    else strcpy(name, odd[rnd() % 8]);
    ro = oo = rnd();
    rv = r4(P(name), P(&ro)); ov = o4(P(name), P(&oo));
    if (rv != ov || ro != oo) {
        printf("  +0x204 key-code(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", name, (int)rv, ro, (int)ov, oo); differ++;
    }
}

/* ---- profiles ---------------------------------------------------------------- */

/* Each side has its own copy of a made-up profile, in hostfs\r and hostfs\o
 * (paths of one length, differing in that letter), under a new name each
 * iteration so wine's profile cache never serves a stale copy.  The CFG path
 * at 0x41c120 points each side at its own; the path+extension services are
 * given each side's own path. */
#define CFGPATH_OFF 0x5120u            /* 0x41c120: FLTSIM98.CFG's full path */

static char fs_base[MAX_PATH];         /* ...\hostfs\ */
static char prof_r[MAX_PATH], prof_o[MAX_PATH];   /* without the extension */
static uint32_t prof_seq;

static void fs_init(void)
{
    GetCurrentDirectoryA(sizeof fs_base - 16, fs_base);
    strcat(fs_base, "\\hostfs\\");
    CreateDirectoryA(fs_base, 0);
    sprintf(prof_r, "%sr", fs_base); CreateDirectoryA(prof_r, 0);
    sprintf(prof_o, "%so", fs_base); CreateDirectoryA(prof_o, 0);
}

static const char *const prof_sections[] = { "Main", "Lists", "Empty", "Nope", "main" };
static const char *const prof_keys[] = { "a", "b", "list", "dbl", "s", "missing", "A" };
static const char *const prof_values[] = {
    "12", "-5", "0x1f", "077", "1, 2,3", "4,,5", "1.5", "2.25, -3e2,7", " 9 ", "", "abc", "3.14159",
    "100000000000", "1e5, 2e-3", "-0", "7,8,9,10,11,12,13", "1 ,2", "5 , 6.5 ,7"
};

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static int files_differ(const char *a, const char *b)
{
    static char ba[8192], bb[8192];
    FILE *f;
    size_t na = 0, nb = 0;
    if ((f = fopen(a, "rb"))) { na = fread(ba, 1, sizeof ba, f); fclose(f); }
    if ((f = fopen(b, "rb"))) { nb = fread(bb, 1, sizeof bb, f); fclose(f); }
    return na != nb || memcmp(ba, bb, na);
}

/* A fresh pair of profiles; the CFG path on each side. */
static void seed_profile(void)
{
    static char text[2048], rcfg[MAX_PATH], ocfg[MAX_PATH];
    uint32_t s, k, n;
    char *p = text;
    if (prof_seq) {                     /* the last pair */
        sprintf(rcfg, "%sr\\P%07u.CFG", fs_base, prof_seq); DeleteFileA(rcfg);
        sprintf(ocfg, "%so\\P%07u.CFG", fs_base, prof_seq); DeleteFileA(ocfg);
    }
    prof_seq++;
    for (s = 0; s < 3; s++) {
        if (rnd() % 4 == 0) continue;
        p += sprintf(p, "[%s]\r\n", prof_sections[s]);
        n = s == 2 ? 0 : 1 + rnd() % 5;
        for (k = 0; k < n; k++)
            p += sprintf(p, "%s=%s\r\n", prof_keys[rnd() % 6], prof_values[rnd() % 18]);
    }
    sprintf(prof_r, "%sr\\P%07u", fs_base, prof_seq);
    sprintf(prof_o, "%so\\P%07u", fs_base, prof_seq);
    sprintf(rcfg, "%s.CFG", prof_r); sprintf(ocfg, "%s.CFG", prof_o);
    write_file(rcfg, text); write_file(ocfg, text);
    strcpy((char *)(DATA_VA + CFGPATH_OFF), rcfg);
    strcpy((char *)(our_data + CFGPATH_OFF), ocfg);
}

/* One random profile service on each side; outputs and both files compared. */
static void check_profiles(void)
{
    static char rbuf[256], obuf[256], rcfg[MAX_PATH], ocfg[MAX_PATH];
    static uint8_t rout[64], oout[64], vals[64];
    uint32_t ops = 1 + rnd() % 3, k;
    sprintf(rcfg, "%s.CFG", prof_r); sprintf(ocfg, "%s.CFG", prof_o);
    for (k = 0; k < ops; k++) {
        uint32_t s = 0xa8 + 4 * (rnd() % 0x18), i, n = rnd() % 7, rv = 0, ov = 0, ext;
        uint32_t sec = P(prof_sections[rnd() % 5]), key = P(prof_keys[rnd() % 7]);
        void *rf = real_fn(s), *of = our_fn(s);
        double d = (double)(int32_t)(rnd() % 2000000 - 1000000) / (1 + rnd() % 64);
        const char *rp = prof_r, *op = prof_o;
        if (!of) return;
        if (trace) printf("prof +%#x n=%u\n", s, n);
        ext = rnd() % 8 ? 0x24 : (uint32_t[]){ 999, 0xfffffffeu, 0x25, 0x26 }[rnd() % 4];  /* registered ones too */
        if (ext == 0xfffffffeu) { rp = rcfg; op = ocfg; }
        for (i = 0; i < sizeof vals; i++) vals[i] = (uint8_t)rnd();
        for (i = 0; i < 8; i++) ((double *)vals)[i] = (double)(int32_t)(rnd() % 20000 - 10000) / 8;
        ((uint32_t *)vals)[0] = rnd();                  /* an int's default, or a flag left alone */
        memcpy(rout, vals, sizeof rout); memcpy(oout, vals, sizeof oout);
        memset(rbuf, 0, sizeof rbuf); memset(obuf, 0, sizeof obuf);
        if (rnd() & 1) { strcpy(rbuf, "dflt"); strcpy(obuf, "dflt"); }
        switch (s) {
        case 0xa8: case 0xb8: rv = ((fn3)rf)(sec, key, P(rout)); ov = ((fn3)of)(sec, key, P(oout)); break;
        case 0xac: rv = ((fn3)rf)(sec, key, vals[0] * 1000u - 7); ov = ((fn3)of)(sec, key, vals[0] * 1000u - 7); break;
        case 0xb0: case 0xc0: rv = ((fn4)rf)(sec, key, P(rout), n); ov = ((fn4)of)(sec, key, P(oout), n); break;
        case 0xb4: case 0xc4: rv = ((fn4)rf)(sec, key, P(vals), n); ov = ((fn4)of)(sec, key, P(vals), n); break;
        case 0xbc: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, double) = rf, (__stdcall *o)(uint32_t, uint32_t, double) = of;
            rv = r(sec, key, d); ov = o(sec, key, d);
            break;
        }
        case 0xc8: i = 1 + rnd() % 64; rv = ((fn4)rf)(sec, key, P(rbuf), i); ov = ((fn4)of)(sec, key, P(obuf), i); break;
        case 0xcc: i = rnd() % 18; rv = ((fn3)rf)(sec, key, P(prof_values[i])); ov = ((fn3)of)(sec, key, P(prof_values[i])); break;
        case 0xd0: case 0xe0: rv = ((fn5)rf)(sec, key, P(rout), P(rp), ext); ov = ((fn5)of)(sec, key, P(oout), P(op), ext); break;
        case 0xd4: rv = ((fn5)rf)(sec, key, vals[1] * 77u, P(rp), ext); ov = ((fn5)of)(sec, key, vals[1] * 77u, P(op), ext); break;
        case 0xd8: case 0xe8: case 0xdc: case 0xec: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = of;
            int put = s == 0xdc || s == 0xec;
            rv = r(sec, key, put ? P(vals) : P(rout), n, P(rp), ext);
            ov = o(sec, key, put ? P(vals) : P(oout), n, P(op), ext);
            break;
        }
        case 0xe4: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, double, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, double, uint32_t, uint32_t) = of;
            rv = r(sec, key, d, P(rp), ext); ov = o(sec, key, d, P(op), ext);
            break;
        }
        case 0xf0: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = of;
            i = 1 + rnd() % 64;
            rv = r(sec, key, P(rbuf), i, P(rp), ext); ov = o(sec, key, P(obuf), i, P(op), ext);
            break;
        }
        case 0xf4: i = rnd() % 18; rv = ((fn5)rf)(sec, key, P(prof_values[i]), P(rp), ext); ov = ((fn5)of)(sec, key, P(prof_values[i]), P(op), ext); break;
        case 0xf8: rv = ((fn1)rf)(sec); ov = ((fn1)of)(sec); break;
        case 0xfc: rv = ((fn3)rf)(sec, P(rp), ext); ov = ((fn3)of)(sec, P(op), ext); break;
        case 0x100: rv = ((fn2)rf)(sec, P(rout)); ov = ((fn2)of)(sec, P(oout)); break;
        case 0x104: rv = ((fn4)rf)(sec, P(rp), ext, P(rout)); ov = ((fn4)of)(sec, P(op), ext, P(oout)); break;
        }
        if (rv != ov || memcmp(rout, oout, sizeof rout) || memcmp(rbuf, obuf, sizeof rbuf)) {
            printf("  +%#05x profile DIFFER real=%d ours=%d [%s] [%s]\n", s, (int)rv, (int)ov, rbuf, obuf); differ++;
        }
        if (files_differ(rcfg, ocfg)) { printf("  +%#05x profile FILES DIFFER\n", s); differ++; }
        check_state("profiles");
        if (differ) return;
    }
}

/* ---- files ------------------------------------------------------------------- */

/* Each side works in its own directory (hostfs\r\, hostfs\o\) on the same
 * small set of files, names relative to the harness's directory; up to three
 * files open at a time each.  After every operation the results, the file
 * objects, and the two directories' contents are compared. */
static const char *const fs_names[] = { "Fa", "Fb", "Fc", "Dd" };
static const uint32_t fs_exts[] = { 18, 30, 0xfffffffeu, 18, 30, 999, 0x25 };   /* .BIN .TMP none, bad, registered */
static uint32_t rfh[3], ofh[3];

/* The file a name and extension make, on a side ('r' or 'o'). */
static void fs_path(char *out, char side, const char *name, uint32_t ext)
{
    static const char *const e[] = { [18] = ".BIN", [30] = ".TMP" };
    sprintf(out, "hostfs\\%c\\%s%s", side, name, ext < 31 && e[ext] ? e[ext] : "");
}

/* Remove everything in both sides' directories. */
static void fs_clean(void)
{
    static char p[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint32_t s;
    for (s = 0; s < 2; s++) {
        sprintf(p, "hostfs\\%c\\*", "ro"[s]);
        h = FindFirstFileA(p, &d);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, "..")) continue;
            sprintf(p, "hostfs\\%c\\%s", "ro"[s], d.cFileName);
            SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(p);
            RemoveDirectoryA(p);
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
}

/* A fresh pair of directories: some of the files, with the same contents. */
static void seed_files(void)
{
    static char rp[MAX_PATH], op[MAX_PATH], data[200];
    uint32_t n, x, len, i;
    fs_clean();
    for (n = 0; n < 3; n++)
        for (x = 0; x < 3; x++) {
            if (rnd() % 3) continue;
            len = rnd() % sizeof data;
            for (i = 0; i < len; i++) data[i] = (char)rnd();
            fs_path(rp, 'r', fs_names[n], fs_exts[x]); fs_path(op, 'o', fs_names[n], fs_exts[x]);
            {
                FILE *f = fopen(rp, "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
                f = fopen(op, "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
            }
            if (rnd() % 4 == 0) {           /* some read-only */
                SetFileAttributesA(rp, FILE_ATTRIBUTE_READONLY); SetFileAttributesA(op, FILE_ATTRIBUTE_READONLY);
            }
        }
    {   /* names a search can tell apart: one longer, one of an unknown extension */
        static const char *const extra[] = { "Fab.BIN", "Fc.QQQ" };
        for (n = 0; n < 2; n++) {
            FILE *f;
            if (rnd() % 3) continue;
            sprintf(rp, "hostfs\\r\\%s", extra[n]); sprintf(op, "hostfs\\o\\%s", extra[n]);
            f = fopen(rp, "wb"); if (f) { fputs(extra[n], f); fclose(f); }
            f = fopen(op, "wb"); if (f) { fputs(extra[n], f); fclose(f); }
        }
    }
    {   /* a file both sides always have, for the fatal wrappers */
        FILE *f;
        len = 1 + rnd() % 40;
        for (i = 0; i < len; i++) data[i] = (char)rnd();
        f = fopen("hostfs\\r\\Ok.BIN", "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
        f = fopen("hostfs\\o\\Ok.BIN", "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
    }
    memset(rfh, 0, sizeof rfh); memset(ofh, 0, sizeof ofh);
}

/* The two directories alike: every name in each present in the other, with
 * the same attributes and contents. */
static int dirs_differ(void)
{
    static char rp[MAX_PATH], op[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint32_t s, bad = 0;
    for (s = 0; s < 2 && !bad; s++) {
        sprintf(rp, "hostfs\\%c\\*", "ro"[s]);
        h = FindFirstFileA(rp, &d);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            uint32_t ra, oa;
            if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, "..")) continue;
            sprintf(rp, "hostfs\\r\\%s", d.cFileName); sprintf(op, "hostfs\\o\\%s", d.cFileName);
            ra = GetFileAttributesA(rp); oa = GetFileAttributesA(op);
            if (ra != oa) bad = 1;
            else if (!(ra & FILE_ATTRIBUTE_DIRECTORY) && files_differ(rp, op)) bad = 1;
        } while (!bad && FindNextFileA(h, &d));
        FindClose(h);
    }
    return bad;
}

/* A file object on each side alike: mode, extension, and path but for the
 * side's letter. */
static int fobj_differ(uint32_t r, uint32_t o)
{
    const uint32_t *rf = (const uint32_t *)(uintptr_t)r, *of = (const uint32_t *)(uintptr_t)o;
    const char *a, *b;
    if (!r != !o) return 1;
    if (!r) return 0;
    if (rf[2] != of[2] || rf[3] != of[3]) return 1;
    a = (const char *)(uintptr_t)rf[1]; b = (const char *)(uintptr_t)of[1];
    if (strlen(a) != strlen(b) || strncmp(a, b, 7) || strcmp(a + 8, b + 8)) return 1;
    return 0;
}

static void check_files(void)
{
    static uint8_t rbuf[128], obuf[128], data[128];
    static char rp[MAX_PATH], op[MAX_PATH], rq[MAX_PATH], oq[MAX_PATH];
    uint32_t ops = 1 + rnd() % 8, k, i;
    for (k = 0; k < ops; k++) {
        uint32_t op_ = rnd() % 16, slot = rnd() % 3, rv = 0, ov = 0, ro, oo, n, x, x2;
        uint32_t rf = rfh[slot], of_ = ofh[slot];
        const char *nm = fs_names[rnd() % 4], *nm2 = fs_names[rnd() % 3];
        static const uint32_t slots[16] = { 0x17c, 0x17c, 0x148, 0x13c, 0x19c, 0x19c, 0x1b0, 0x194,
                                            0x190, 0x1a4, 0x1a8, 0x134, 0x138, 0x158, 0x14c, 0x144 };
        uint32_t s = slots[op_];
        void *R = real_fn(s), *O = our_fn(s);
        if (!O) return;
        if (trace) printf("file +%#x slot %u\n", s, slot);
        x = fs_exts[rnd() % 7]; x2 = fs_exts[rnd() % 5];
        sprintf(rp, "hostfs\\r\\%s", nm); sprintf(op, "hostfs\\o\\%s", nm);
        sprintf(rq, "hostfs\\r\\%s", nm2); sprintf(oq, "hostfs\\o\\%s", nm2);
        if (x == 0xfffffffeu && rnd() & 1) { strcat(rp, ".BIN"); strcat(op, ".BIN"); }
        ro = oo = rnd();
        if (rnd() % 16 == 0) rf = of_ = 0;              /* a null file now and then */
        memset(rbuf, 0x5a, sizeof rbuf); memset(obuf, 0x5a, sizeof obuf);
        switch (s) {
        case 0x17c: case 0x148:
            if (rfh[slot] || ofh[slot]) continue;
            n = rnd() % 5;                              /* modes 0..3, and 4 */
            if (s == 0x17c) { rv = ((fn4)R)(P(&rfh[slot]), P(rp), x, n); ov = ((fn4)O)(P(&ofh[slot]), P(op), x, n); }
            else { rv = ((fn3)R)(P(&rfh[slot]), P(rp), x); ov = ((fn3)O)(P(&ofh[slot]), P(op), x); }
            if (rv) rfh[slot] = 0;
            if (ov) ofh[slot] = 0;
            if (fobj_differ(rfh[slot], ofh[slot])) { printf("  +%#05x open OBJECT DIFFER\n", s); differ++; }
            break;
        case 0x13c:
            rv = ((fn1)R)(rf); ov = ((fn1)O)(of_);
            if (rf) rfh[slot] = 0;
            if (of_) ofh[slot] = 0;
            break;
        case 0x19c: case 0x1b0:
            n = rnd() % 100;
            for (i = 0; i < n; i++) data[i] = (uint8_t)rnd();
            if (s == 0x1b0) { memcpy(rbuf, data, n); memcpy(obuf, data, n); }
            i = rnd() & 1;
            rv = ((fn4)R)(rf, P(rbuf), n, i ? P(&ro) : 0); ov = ((fn4)O)(of_, P(obuf), n, i ? P(&oo) : 0);
            break;
        case 0x194: {
            uint32_t size = 0;
            if (rf) size = GetFileSize((HANDLE)(uintptr_t)*(uint32_t *)(uintptr_t)rf, 0);
            n = rnd() % (size + 4); i = rnd() % 3;
            rv = ((fn3)R)(rf, n, i); ov = ((fn3)O)(of_, n, i);
            break;
        }
        case 0x190: case 0x1a4:
            rv = ((fn2)R)(rf, P(&ro)); ov = ((fn2)O)(of_, P(&oo));
            break;
        case 0x1a8:
            n = rnd() % 120;
            rv = ((fn2)R)(rf, n); ov = ((fn2)O)(of_, n);
            break;
        case 0x134:
            rv = ((fn3)R)(P(&ro), P(rp), x); ov = ((fn3)O)(P(&oo), P(op), x);
            break;
        case 0x138:
            n = rnd() % 0x20;
            rv = ((fn3)R)(n, P(rp), x); ov = ((fn3)O)(n, P(op), x);
            break;
        case 0x158: case 0x14c:
            if (s == 0x14c && rnd() % 4 == 0) {         /* +0x11c, a directory, instead */
                s = 0x11c; R = real_fn(s); O = our_fn(s);
                if (!O) return;
                sprintf(rp, "hostfs\\r\\Dd"); sprintf(op, "hostfs\\o\\Dd");
                rv = ((fn1)R)(P(rp)); ov = ((fn1)O)(P(op));
                break;
            }
            rv = ((fn2)R)(P(rp), x); ov = ((fn2)O)(P(op), x);
            break;
        case 0x144:
            if (rnd() & 1) { s = 0x16c; R = real_fn(s); O = our_fn(s); if (!O) return; }
            rv = ((fn4)R)(P(rq), x2, P(rp), x); ov = ((fn4)O)(P(oq), x2, P(op), x);
            break;
        }
        if (rv != ov || ro != oo || memcmp(rbuf, obuf, sizeof rbuf)) {
            printf("  +%#05x file op DIFFER real=%d/%#x ours=%d/%#x (%s, %#x)\n", s, (int)rv, ro, (int)ov, oo, nm, x); differ++;
        }
        if (dirs_differ()) { printf("  +%#05x file op DIRS DIFFER (%s, %#x)\n", s, nm, x); differ++; }
        check_state("files");
        if (differ) break;
    }
    for (k = 0; k < 3; k++) {                           /* close what is open, each side its own */
        if (rfh[k]) ((fn1)real_fn(0x13c))(rfh[k]);
        if (ofh[k] && our_fn(0x13c)) ((fn1)our_fn(0x13c))(ofh[k]);
        rfh[k] = ofh[k] = 0;
    }
}

/* +0x164 / +0x160 / +0x15c: a search of each side's directory, walked to the
 * end (or a few entries), entries compared as they come. */
static void check_find(void)
{
    static const char *const pats[] = { "*", "F*", "Fa", "D*", "?b", 0 };
    static const uint32_t exts[] = { 0xffffffffu, 0xfffffffeu, 18, 30, 999 };
    static uint8_t re[0x110], oe[0x110];
    fn4 r64 = (fn4)real_fn(0x164), o64 = (fn4)our_fn(0x164);
    fn2 r60 = (fn2)real_fn(0x160), o60 = (fn2)our_fn(0x160);
    fn1 r5c = (fn1)real_fn(0x15c), o5c = (fn1)our_fn(0x15c);
    const char *pat = pats[rnd() % 6];
    uint32_t ext = exts[rnd() % 5], rf = 0, of_ = 0, rv, ov, k;
    int slash = rnd() & 1;
    if (!o64 || !o60 || !o5c) return;
    rv = r64(P(&rf), P(slash ? "hostfs\\r\\" : "hostfs\\r"), P(pat), ext);
    ov = o64(P(&of_), P(slash ? "hostfs\\o\\" : "hostfs\\o"), P(pat), ext);
    if (trace) printf("find %s %#x -> %d/%d\n", pat ? pat : "(null)", ext, (int)rv, (int)ov);
    if (rv != ov || !rf != !of_) { printf("  +0x164 find-first(%s, %#x) DIFFER real=%d ours=%d\n", pat ? pat : "(null)", ext, (int)rv, (int)ov); differ++; }
    if (rv || ov) return;
    for (k = 0; k < 12; k++) {
        uint32_t fill = rnd();
        memset(re, (int)fill, sizeof re); memset(oe, (int)fill, sizeof oe);
        rv = r60(rf, P(re)); ov = o60(of_, P(oe));
        if (rv != ov || memcmp(re, oe, sizeof re)) {
            printf("  +0x160 find-next DIFFER real=%d [%s] ours=%d [%s]\n", (int)rv, re, (int)ov, oe); differ++; break;
        }
        if (rv) break;
    }
    rv = r5c(P(&rf)); ov = o5c(P(&of_));
    if (rv != ov || rf || of_) { printf("  +0x15c find-close DIFFER\n"); differ++; }
    check_state("find");
}

/* ---- messages, the clock, the fatal file wrappers ---------------------------- */

/* Run one service on each side under the call log; compare return, log and
 * state.  `call` gets the side (0 real, 1 ours). */
static void logged_pair(const char *name, uint32_t (*call)(int side))
{
    static struct logged saved[32];
    uint32_t rv, ov, rn;
    ncalls = 0; memset(calls, 0, sizeof calls);
    rv = call(0);
    rn = ncalls; memcpy(saved, calls, sizeof saved);
    ncalls = 0; memset(calls, 0, sizeof calls);
    ov = call(1);
    if (rv != ov || rn != ncalls || memcmp(saved, calls, sizeof saved)) {
        uint32_t k;
        printf("  %-16s DIFFER real=%#x ours=%#x, calls %u/%u\n", name, rv, ov, rn, ncalls); differ++;
        for (k = 0; k < 32 && k < rn; k++)
            if (memcmp(&saved[k], &calls[k], sizeof calls[k])) {
                printf("    call %u: real {%u %#x %#x %#x %#x \"%.20s\"} ours {%u %#x %#x %#x %#x \"%.20s\"}\n", k,
                       saved[k].kind, saved[k].a, saved[k].b, saved[k].c, saved[k].d, saved[k].text,
                       calls[k].kind, calls[k].a, calls[k].b, calls[k].c, calls[k].d, calls[k].text);
                break;
            }
    }
    check_state(name);
}

static uint32_t pw_args[4];
static uint8_t pw_block[0x40];
static uint32_t pw_rec[0x30];
static uint16_t pw_rout[8], pw_oout[8];

#define SIDE(s, off) ((s) ? our_fn(off) : real_fn(off))
static uint32_t do_024(int s) { return ((fn0)SIDE(s, 0x24))(); }
static uint32_t do_028(int s) { return ((fn0)SIDE(s, 0x28))(); }
static uint32_t do_038(int s) { return ((fn0)SIDE(s, 0x38))(); }
static uint32_t do_03c(int s) { return ((fn2)SIDE(s, 0x3c))(pw_args[0], pw_args[1]); }
static uint32_t do_040(int s) { return ((fn2)SIDE(s, 0x40))(pw_args[0], pw_args[1]); }
static uint32_t do_044(int s) { return ((fn1)SIDE(s, 0x44))(P(s ? pw_oout : pw_rout)); }
static uint32_t do_07c(int s) { return pw_args[2] ? ((fn1)SIDE(s, 0x7c))(P(pw_rec)) & 0 : (((fn1)SIDE(s, 0x7c))(0), 0); }
static uint32_t do_080(int s) { return ((fn1)SIDE(s, 0x80))(P(pw_rec)); }
static uint32_t do_084(int s) { return ((fn1)SIDE(s, 0x84))(pw_args[2] ? P(pw_rec) : 0); }

static void check_messages(void)
{
    static const char *const titles[] = { "Cockpit", "", "Map View 1", "Spot" };
    static uint32_t (*const ops[])(int) = { do_024, do_028, do_038, do_03c, do_040, do_044, do_07c, do_080, do_084 };
    static const char *const names[] = { "+0x024 quit", "+0x028 quit-post", "+0x038 ticks", "+0x03c post-cmd",
                                         "+0x040 send-cmd", "+0x044 local-time", "+0x07c title", "+0x080 titles",
                                         "+0x084 notify" };
    uint32_t k = rnd() % 9;
    if (!our_fn(0x24) || !our_fn(0x7c)) return;
    pw_args[0] = rnd(); pw_args[1] = rnd(); pw_args[2] = rnd() % 8 != 0;
    fake_ticks = rnd();
    fake_time.wYear = 1990 + rnd() % 30; fake_time.wMonth = 1 + rnd() % 12; fake_time.wDayOfWeek = rnd() % 7;
    fake_time.wDay = 1 + rnd() % 28; fake_time.wHour = rnd() % 24; fake_time.wMinute = rnd() % 60;
    fake_time.wSecond = rnd() % 60; fake_time.wMilliseconds = rnd() % 1000;
    memset(pw_rout, 0x77, sizeof pw_rout); memset(pw_oout, 0x77, sizeof pw_oout);
    *(uint32_t *)(pw_block + 0x24) = rnd(); *(uint32_t *)(pw_block + 0x28) = rnd();
    pw_rec[0] = P(titles[rnd() % 4]); pw_rec[2] = rnd(); pw_rec[0xa0 / 4] = P(pw_block);
    logged_pair(names[k], ops[k]);
    if (k == 5 && memcmp(pw_rout, pw_oout, sizeof pw_rout)) { printf("  +0x044 local-time OUT DIFFER\n"); differ++; }
}

/* The fatal wrappers on their non-fatal paths only: a file both sides have
 * (Ok.BIN), opened, read, sought within it, closed.  A failure would exit
 * through the hooked message box. */
static void check_fatal_files(void)
{
    static uint8_t rb[64], ob[64];
    fn4 ro = (fn4)real_fn(0x180), oo = (fn4)our_fn(0x180);
    fn4 rr = (fn4)real_fn(0x1a0), or_ = (fn4)our_fn(0x1a0);
    fn3 rs = (fn3)real_fn(0x198), os = (fn3)our_fn(0x198);
    fn1 rc = (fn1)real_fn(0x140), oc = (fn1)our_fn(0x140);
    uint32_t rf = 0, of_ = 0, rg = 0, og = 0, rv, ov, n = rnd() % 64, pos = rnd() % 33, mode = rnd() % 2, i;
    if (!oo || !or_ || !os || !oc) return;
    rv = ro(P(&rf), P("hostfs\\r\\Ok"), 18, mode); ov = oo(P(&of_), P("hostfs\\o\\Ok"), 18, mode);
    if (rv != ov || fobj_differ(rf, of_)) { printf("  +0x180 open-or-die DIFFER\n"); differ++; return; }
    pos %= GetFileSize((HANDLE)(uintptr_t)*(uint32_t *)(uintptr_t)rf, 0) + 1;   /* within it: past it is fatal */
    i = rnd() % 3;                       /* any method: within the size none can fail */
    rv = rs(rf, pos, i); ov = os(of_, pos, i);
    if (rv != ov) { printf("  +0x198 seek-or-die DIFFER\n"); differ++; }
    memset(rb, 0x33, sizeof rb); memset(ob, 0x33, sizeof ob);
    rv = rr(rf, P(rb), n, P(&rg)); ov = or_(of_, P(ob), n, P(&og));
    if (rv != ov || rg != og || memcmp(rb, ob, sizeof rb)) { printf("  +0x1a0 read-or-die DIFFER\n"); differ++; }
    rv = rc(rf); ov = oc(of_);
    if (rv != ov) { printf("  +0x140 close-or-die DIFFER\n"); differ++; }
    check_state("fatal wrappers");
}

/* +0x088: rectangles into a view window's dirty list.  Each side gets its own
 * window record and host block (the list at +0x54 a host list of 16-byte
 * rectangles), started alike; rectangles are added and the lists compared. */
static void check_dirty(void)
{
    static uint32_t rrec[0x30], orec[0x30];
    static uint8_t rblk[0x90], oblk[0x90];
    fn2 r = (fn2)real_fn(0x88), o = (fn2)our_fn(0x88);
    int32_t rect[4];
    uint32_t n = rnd() % 4, k, i, *rl, *ol, rv, ov;
    if (!o) return;
    memset(rblk, 0, sizeof rblk); memset(oblk, 0, sizeof oblk);
    rrec[0xa0 / 4] = P(rblk); orec[0xa0 / 4] = P(oblk);
    rl = (uint32_t *)(rblk + 0x54); ol = (uint32_t *)(oblk + 0x54);
    rl[0] = ol[0] = n; rl[1] = ol[1] = n; rl[2] = ol[2] = 4; rl[3] = ol[3] = 0x10;
    rl[6] = n ? P(malloc(n * 16)) : 0; ol[6] = n ? P(malloc(n * 16)) : 0;
    for (i = 0; i < n * 4; i++) {
        int32_t v = (int32_t)(rnd() % 400) - 50;
        ((int32_t *)(uintptr_t)rl[6])[i] = ((int32_t *)(uintptr_t)ol[6])[i] = v;
    }
    for (k = 0; k < 1 + rnd() % 5; k++) {
        rect[0] = (int32_t)(rnd() % 400) - 50; rect[1] = (int32_t)(rnd() % 400) - 50;
        rect[2] = rect[0] + (int32_t)(rnd() % 120) - 10; rect[3] = rect[1] + (int32_t)(rnd() % 120) - 10;
        i = rnd() % 16 == 0;        /* now and then no record */
        rv = r(i ? 0 : P(rrec), P(rect)); ov = o(i ? 0 : P(orec), P(rect));
        if ((rv ? rv - rl[6] + 1 : 0) != (ov ? ov - ol[6] + 1 : 0) || rl[0] != ol[0] ||
            (rl[0] && memcmp((void *)(uintptr_t)rl[6], (void *)(uintptr_t)ol[6], rl[0] * 16))) {
            printf("  +0x088 dirty-rect DIFFER (count %u/%u)\n", rl[0], ol[0]); differ++; break;
        }
    }
    free((void *)(uintptr_t)rl[6]); free((void *)(uintptr_t)ol[6]);
    check_state("+0x088 dirty-rect");
}

/* ---- joysticks --------------------------------------------------------------- */

#define JOYTAB_OFF  0x18b8u            /* 0x4188b8: 16 entries of 0x198 */
#define JOYFLAG_OFF 0x3238u            /* 0x41a238: enumerated; 0x41a23c: count */

/* A made-up set of joysticks and registry, and the host's table emptied. */
static void seed_joysticks(void)
{
    uint32_t i;
    memset(&joy, 0, sizeof joy);
    joy.num = rnd() % 7;
    for (i = 0; i < 6; i++) {
        uint8_t *c = (uint8_t *)&joy.caps[i];
        uint32_t k;
        joy.present[i] = rnd() % 3 != 0;
        for (k = 0; k < sizeof joy.caps[i]; k++) c[k] = (uint8_t)rnd();
        joy.caps[i].wCaps = rnd() & 0x1f;
        joy.caps[i].wNumAxes = rnd() % 7;
        joy.caps[i].wMaxButtons = rnd() % 33;
        strcpy(joy.caps[i].szRegKey, joy_keys[rnd() % 3]);
        for (k = 0; k < 2; k++) {
            JOYINFOEX *j = k ? &joy.after[i] : &joy.before[i];
            j->dwXpos = rnd() % 0x10000; j->dwYpos = rnd() % 0x10000; j->dwZpos = rnd() % 0x10000;
            j->dwRpos = rnd() % 0x10000; j->dwUpos = rnd() % 0x10000; j->dwVpos = rnd() % 0x10000;
            j->dwButtons = rnd() % 4 ? 0 : rnd();
            j->dwPOV = rnd() % 3 ? 0xffff : rnd() % 0x9000;
        }
        if (rnd() & 1) joy.after[i] = joy.before[i];                  /* sometimes nothing moves */
        if (rnd() & 1) joy.after[i].dwButtons ^= 1u << (rnd() % 32);
        joy.change_at[i] = 1 + rnd() % 6;
        joy_reg_has[i] = rnd() % 4 != 0;
        joy_reg_oem[i] = rnd() % 3;
    }
    for (i = 0; i < 3; i++) joy_reg_name[i] = rnd() % 5;
    memset((void *)(DATA_VA + JOYTAB_OFF), 0, 0x1980); memset(our_data + JOYTAB_OFF, 0, 0x1980);
    *(uint32_t *)(DATA_VA + JOYFLAG_OFF) = *(uint32_t *)(our_data + JOYFLAG_OFF) = 0;
    *(uint32_t *)(DATA_VA + JOYFLAG_OFF + 4) = *(uint32_t *)(our_data + JOYFLAG_OFF + 4) = 0;
    i = rnd() % 4 == 0;                                                 /* the slow probe */
    *(uint32_t *)(DATA_VA + 0x1100) = *(uint32_t *)(our_data + 0x1100) = i;
    i = rnd() % 4 ? 0x7fffffff : rnd() % 8;
    *(uint32_t *)(DATA_VA + 0x1104) = *(uint32_t *)(our_data + 0x1104) = i;
    i = rnd() % 8 != 0;                                                 /* 0x418858: joysticks on */
    *(uint32_t *)(DATA_VA + 0x1858) = *(uint32_t *)(our_data + 0x1858) = i;
}

static uint32_t jr_idx, jr_mode;
static uint8_t jr_rst[0x150], jr_ost[0x150];
static uint32_t jr_rout[0x20], jr_oout[0x20];

static uint32_t do_1c0(int s) { joy_reset(); return ((fn0)SIDE(s, 0x1c0))(); }
static uint32_t do_1c4(int s) { joy_reset(); return ((fn2)SIDE(s, 0x1c4))(jr_idx, P(s ? jr_ost : jr_rst)); }
static uint32_t do_1c8(int s)
{
    uint32_t r;
    joy_reset();
    r = ((fn2)SIDE(s, 0x1c8))(jr_idx, P(s ? jr_oout : jr_rout));
    if (trace) printf("describe %u -> %d [%.40s] id %#x\n", jr_idx, (int)r, (char *)(s ? jr_oout : jr_rout) + 0x10, (s ? jr_oout : jr_rout)[0]);
    return r;
}
static uint32_t do_1cc(int s)
{
    uint32_t out = P(s ? jr_oout : jr_rout), r;
    joy_reset(); fake_ticks = 1000;
    r = ((fn3)SIDE(s, 0x1cc))(jr_idx, jr_mode, out);
    return r == out ? 0xc0de0001u : r;        /* on a timeout it returns its out pointer */
}

static void check_joysticks(void)
{
    uint32_t k, ops = 1 + rnd() % 6;
    if (!our_fn(0x1c0) || !our_fn(0x1cc)) return;
    seed_joysticks();
    fake_tick_step = 700;
    for (k = 0; k < ops && !differ; k++) {
        uint32_t op = k == 0 ? 0 : rnd() % 4, i;
        uint32_t (*const fns[])(int) = { do_1c0, do_1c4, do_1c8, do_1cc };
        static const char *const names[] = { "+0x1c0 joy-enum", "+0x1c4 joy-read", "+0x1c8 joy-describe", "+0x1cc joy-wait" };
        i = *(uint32_t *)(DATA_VA + JOYFLAG_OFF + 4);        /* mostly a joystick the table has */
        jr_idx = i && rnd() % 4 ? rnd() % i : rnd() % 5; jr_mode = rnd() % 4;
        for (i = 0; i < sizeof jr_rst; i++) jr_rst[i] = jr_ost[i] = (uint8_t)rnd();
        for (i = 0; i < 0x20; i++) jr_rout[i] = jr_oout[i] = rnd();
        logged_pair(names[op], fns[op]);
        if (memcmp(jr_rst, jr_ost, sizeof jr_rst) || memcmp(jr_rout, jr_oout, sizeof jr_rout)) {
            printf("  %-16s OUTPUT DIFFER\n", names[op]); differ++;
        }
    }
    fake_tick_step = 0;
}

/* +0x2c4: a string shortened to fit, into a guarded buffer. */
static void check_abbrev(void)
{
    static char src[64], rb[80], ob[80];
    fn3 r = (fn3)real_fn(0x2c4), o = (fn3)our_fn(0x2c4);
    uint32_t len = rnd() % 48, rv, ov;
    if (!o) return;
    rnd_str(src, rnd() & 1 ? 12 : sizeof src);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r(P(rb + 8), P(src), len); ov = o(P(ob + 8), P(src), len);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x2c4 abbrev(\"%s\", %u) DIFFER real=%d [%s] ours=%d [%s]\n", src, len, (int)rv, rb + 8, (int)ov, ob + 8); differ++;
    }
}

/* +0x274 / +0x278 the mouse, +0x238 the heaps, +0x150 disk space, +0x174 a
 * temporary name -- each under the call log. */
static char tn_rout[300], tn_oout[300], tn_dir[64];
static uint32_t df_rout[2], df_oout[2], ms_arg, tn_seq0;
static uint32_t do_274(int s) { return ((fn1)SIDE(s, 0x274))(ms_arg); }
static uint32_t do_278(int s) { return ((fn0)SIDE(s, 0x278))(); }
static uint32_t do_238(int s) { return ((fn0)SIDE(s, 0x238))(); }
static uint32_t do_150(int s) { return ((fn2)SIDE(s, 0x150))(tn_dir[0] ? P(tn_dir) : 0, P(s ? df_oout : df_rout)); }
static uint32_t do_174(int s) { temp_seq = tn_seq0; return ((fn2)SIDE(s, 0x174))(P(s ? tn_oout : tn_rout), tn_dir[0] == 'n' ? 0 : P(tn_dir)); }

static void check_misc(void)
{
    static const char *const dirs[] = { "C:\\fs98\\tmp", "hostfs\\r", "C:\\", "", "n", "D:\\a\\b.c" };
    static uint32_t (*const ops[])(int) = { do_274, do_278, do_238, do_150, do_174 };
    static const char *const names[] = { "+0x274 capture", "+0x278 release", "+0x238 heaps", "+0x150 disk-free", "+0x174 temp-name" };
    uint32_t k = rnd() % 5;
    if (!our_fn(0x274) || !our_fn(0x174)) return;
    heap_n = rnd() % 6; heap_bad = rnd() % 3 ? 0 : rnd();
    ms_arg = rnd();
    *(uint32_t *)(DATA_VA + 0x181c) = *(uint32_t *)(our_data + 0x181c) = rnd() & 1;   /* 0x41881c: captured */
    strcpy(tn_dir, dirs[rnd() % 6]);
    disk_spc = rnd() % 4 ? 1 + rnd() % 64 : 0; disk_bps = 512 << (rnd() % 4); disk_free = rnd();
    if (rnd() % 4 == 0) { disk_spc = rnd(); disk_bps = rnd(); }      /* a product past 32 bits */
    tn_seq0 = rnd() % 0x1000;
    memset(tn_rout, 0x61, sizeof tn_rout); memset(tn_oout, 0x61, sizeof tn_oout);
    df_rout[0] = df_oout[0] = rnd(); df_rout[1] = df_oout[1] = rnd();
    logged_pair(names[k], ops[k]);
    if (memcmp(tn_rout, tn_oout, sizeof tn_rout) || memcmp(df_rout, df_oout, sizeof df_rout)) {
        printf("  %-16s OUTPUT DIFFER [%.60s] [%.60s]\n", names[k], tn_rout, tn_oout); differ++;
    }
}

/* +0x208: keypad folding, both sides on the same three words. */
static void check_keypad(void)
{
    static uint32_t w[3];
    fn4 r = (fn4)real_fn(0x208), o = (fn4)our_fn(0x208);
    uint32_t k = rnd() % 4 ? 0x5c + rnd() % 0x18 : rnd() % 0x100, f = rnd(), x = rnd() & 1, rv, ov, rw[3];
    if (!o) return;
    if (rnd() % 4 == 0) k = 0x0d;
    w[0] = k; w[1] = f; w[2] = x;
    rv = r(P(&w[0]), rnd(), P(&w[1]), P(&w[2]));
    memcpy(rw, w, sizeof rw);
    w[0] = k; w[1] = f; w[2] = x;
    ov = o(P(&w[0]), rnd(), P(&w[1]), P(&w[2]));
    if (rv != ov || memcmp(rw, w, sizeof rw)) { printf("  +0x208 keypad(%#x) DIFFER\n", k); differ++; }
}

/* +0x170: the length of a path made absolute (the harness's directory is the
 * same for both sides). */
static void check_fullpath(void)
{
    static const char *const paths[] = { "hostfs\\r\\Fa", "x", "..\\a\\b", "C:\\abs\\f", "\\root\\y", "", ".\\z.q", "D:rel" };
    static const uint32_t exts[] = { 18, 0xfffffffeu, 999, 0x24, 0x25 };
    fn3 r = (fn3)real_fn(0x170), o = (fn3)our_fn(0x170);
    const char *p = paths[rnd() % 8];
    uint32_t x = exts[rnd() % 5], rl = rnd(), ol = rl, rv, ov;
    if (!o) return;
    rv = r(P(p), x, P(&rl)); ov = o(P(p), x, P(&ol));
    if (rv != ov || rl != ol) { printf("  +0x170 full-length(\"%s\", %#x) DIFFER real=%d/%d ours=%d/%d\n", p, x, (int)rv, (int)rl, (int)ov, (int)ol); differ++; }
    check_state("+0x170 full-length");
}

/* ---- menus ------------------------------------------------------------------- */

#define MENU_OFF    0x1298u            /* 0x418298: the root item; its items at 0x4182a4 */

/* Definition arrays the harness builds (the host copies them): items of
 * { parent, id, text, flags, sub, hmenu }, ids unique and fresh. */
static uint8_t menu_pool[4096];
static uint32_t menu_used, menu_next_id;
static const char *const menu_texts[] = { "&File", "Exit", "Views", "&Map View", "Help", "" };

static uint32_t menu_text(void)
{
    static const uint32_t ids[] = { 0x9520, 0x9526, 9, 0x2201, 0x7777 };
    return rnd() % 2 ? ids[rnd() % 5] : P(menu_texts[rnd() % 6]);
}

/* An array of up to n items under `parent`, submenus `depth` deep. */
static uint32_t menu_def(uint32_t parent, uint32_t n, int depth)
{
    uint32_t a = P(menu_pool + menu_used), i, k = rnd() % (n + 1);
    if (menu_used + (k + 1) * 0x14 > sizeof menu_pool) k = 0;
    menu_used += (k + 1) * 0x14;
    for (i = 0; i < k; i++) {
        uint32_t *it = (uint32_t *)(uintptr_t)(a + i * 0x14);
        uint32_t id = rnd() % 8 == 0 ? 1 : menu_next_id++;
        it[0] = parent | id << 16;
        it[1] = menu_text();
        it[2] = rnd() % 4;
        it[3] = 0; it[4] = rnd();
        if (depth > 0 && id != 1 && rnd() % 3 == 0) it[3] = menu_def(id, 4, depth - 1);
    }
    *(uint32_t *)(uintptr_t)(a + k * 0x14) = 0xffff0000u;   /* the terminator */
    return a;
}

/* The two trees alike: ids, parents, texts, flags, HMENUs, the same shape. */
static int menus_differ(uint32_t r, uint32_t o, int depth)
{
    if (!r != !o) return 1;
    if (!r || depth > 8) return 0;
    for (;; r += 0x14, o += 0x14) {
        const uint32_t *ri = (const uint32_t *)(uintptr_t)r, *oi = (const uint32_t *)(uintptr_t)o;
        if (ri[0] != oi[0]) return 1;
        if ((ri[0] >> 16) == 0xffff) return 0;
        if (ri[1] != oi[1] || ri[2] != oi[2] || ri[4] != oi[4]) return 1;
        if (menus_differ(ri[3], oi[3], depth + 1)) return 1;
    }
}

/* Every id in the real side's tree. */
static uint32_t menu_ids[128], menu_parents[128], menu_nids, menu_subs[128], menu_nsubs;
static void menu_collect(uint32_t items, int depth)
{
    if (!items || depth > 8) return;
    for (;; items += 0x14) {
        const uint32_t *it = (const uint32_t *)(uintptr_t)items;
        if ((it[0] >> 16) == 0xffff) return;
        if (menu_nids < 128) { menu_parents[menu_nids] = it[0] & 0xffff; menu_ids[menu_nids++] = it[0] >> 16; }
        if (it[3] && menu_nsubs < 128) menu_subs[menu_nsubs++] = it[0] >> 16;
        menu_collect(it[3], depth + 1);
    }
}

static uint32_t mn_args[4], mn_rout, mn_oout, mn_slot;
static uint32_t do_menu(int s)
{
    if (trace) printf("menu slot %u side %d args %#x %#x\n", mn_slot, s, mn_args[0], mn_args[1]);
    menu_seq = 0x10 * mn_slot;
    switch (mn_slot) {
    case 0: return ((fn1)SIDE(s, 0x23c))(mn_args[0]);
    case 1: return ((fn0)SIDE(s, 0x240))();
    case 2: return ((fn4)SIDE(s, 0x244))(mn_args[0], mn_args[1], mn_args[2], mn_args[3]);
    case 3: return ((fn2)SIDE(s, 0x248))(mn_args[0], mn_args[1]);
    case 4: return ((fn4)SIDE(s, 0x24c))(mn_args[0], mn_args[1], mn_args[2], mn_args[3]);
    case 5: return ((fn2)SIDE(s, 0x250))(mn_args[0], mn_args[1]);
    case 6: return ((fn2)SIDE(s, 0x254))(mn_args[0], mn_args[1]);
    case 7: return ((fn2)SIDE(s, 0x258))(mn_args[0], P(s ? &mn_oout : &mn_rout));
    case 8: return ((fn1)SIDE(s, 0x25c))(mn_args[0]);
    case 9: return ((fn2)SIDE(s, 0x260))(mn_args[0], mn_args[1]);
    case 10: return ((fn2)SIDE(s, 0x264))(mn_args[0], mn_args[1]);
    default: return ((fn1)SIDE(s, 0x268))(mn_args[0]);
    }
}

static void check_menus(void)
{
    static const char *const names[] = { "+0x23c menu-set", "+0x240 menu-drop", "+0x244 menu-append", "+0x248 menu-append-n",
        "+0x24c menu-insert", "+0x250 menu-insert-n", "+0x254 menu-state", "+0x258 menu-flags", "+0x25c menu-remove",
        "+0x260 menu-renumber", "+0x264 menu-retext", "+0x268 menu-exists" };
    uint32_t ops = 2 + rnd() % 8, k, z = 0, i;
    if (!our_fn(0x23c) || !our_fn(0x268)) return;
    menu_used = 0; menu_next_id = 2;
    {   /* the root: the bar (id 0, no items yet) and its terminator */
        static const uint32_t root[6] = { 0, 0, 0, 0, 0, 0xffff0000u };
        memcpy((void *)(DATA_VA + MENU_OFF), root, sizeof root); memcpy(our_data + MENU_OFF, root, sizeof root);
    }
    *(uint32_t *)(DATA_VA + 0x1810) = *(uint32_t *)(our_data + 0x1810) = 0;           /* 0x418810: none attached */
    *(uint32_t *)(DATA_VA + 0x12c0) = *(uint32_t *)(our_data + 0x12c0) = rnd() & 1;   /* 0x4182c0: redraw */
    is_window = rnd() & 1;
    for (k = 0; k < ops && !differ; k++) {
        uint32_t id;
        menu_nids = 0; menu_nsubs = 0;
        menu_collect(*(uint32_t *)(DATA_VA + MENU_OFF + 0xc), 0);
        i = menu_nids ? rnd() % menu_nids : 0;
        id = menu_nids && rnd() % 6 ? menu_ids[i] : (rnd() % 3 ? 0 : 999);
        mn_slot = k == 0 ? 0 : k == ops - 1 ? 1 : rnd() % 12 == 0 ? 0 : 2 + rnd() % 10;   /* a second set is refused */
        if ((mn_slot == 4 || mn_slot == 5 || mn_slot == 6 || mn_slot == 8 || mn_slot == 9) && id == 0)
            id = 999;       /* the bar's "parent" is itself, and retail then runs off its array --
                               and renumbering the bar makes any later lookup of that number find it */
        if (mn_slot == 2 || mn_slot == 3) {
            /* appending to an item with no submenu crashes retail (a null
             * array counted): only items that have one, or the bar (0) */
            if (*(uint32_t *)(DATA_VA + MENU_OFF + 0xc) == 0) continue;
            id = menu_nsubs && rnd() % 3 ? menu_subs[rnd() % menu_nsubs] : 0;
        }
        mn_args[0] = id; mn_args[1] = menu_next_id++; mn_args[2] = menu_text(); mn_args[3] = rnd() % 4;
        if (mn_slot == 0) mn_args[0] = menu_def(0, 6, 2);
        if (mn_slot == 3) mn_args[1] = menu_def(id, 3, 1);
        if (mn_slot == 5) {     /* items for id's parent's array: the parent of id's first occurrence, as retail finds it */
            uint32_t j, par = 0;
            for (j = 0; j < menu_nids; j++) if (menu_ids[j] == id) { par = menu_parents[j]; break; }
            mn_args[1] = menu_def(par, 3, 1);
        }
        if (mn_slot == 6) mn_args[1] = rnd() % 4;
        if (mn_slot == 10) mn_args[1] = menu_text();
        mn_rout = mn_oout = rnd();
        logged_pair(names[mn_slot], do_menu);
        if (mn_rout != mn_oout) { printf("  %-16s OUT DIFFER\n", names[mn_slot]); differ++; }
        if (menus_differ(*(uint32_t *)(DATA_VA + MENU_OFF + 0xc), *(uint32_t *)(our_data + MENU_OFF + 0xc), 0)) {
            printf("  %-16s TREES DIFFER\n", names[mn_slot]); differ++;
        }
        (void)z;
    }
}

/* ---- modules ----------------------------------------------------------------- */

#define DIALOGS_OFF 0x0ee0u            /* 0x417ee0: the dialog registry, a host list */

/* The module records alike: names (each side's own path, by its letter),
 * ids, handles, Linkages, counts. */
static int modules_differ(void)
{
    uint32_t n = *(uint32_t *)(DATA_VA + MODS_OFF), i;
    uint32_t *r = *(uint32_t **)(DATA_VA + MODS_OFF + 0x18), *o = *(uint32_t **)(our_data + MODS_OFF + 0x18);
    if (n != *(uint32_t *)(our_data + MODS_OFF)) return 1;
    for (i = 0; i < n; i++) {
        uint32_t *rr = r + 5 * i, *oo = o + 5 * i;
        const char *a = (const char *)(uintptr_t)rr[0], *b = (const char *)(uintptr_t)oo[0];
        if (rr[1] != oo[1] || rr[2] != oo[2] || rr[3] != oo[3] || rr[4] != oo[4]) return 1;
        if (!a != !b || (a && (strlen(a) != strlen(b) || strncmp(a, b, 7) || strcmp(a + 8, b + 8)))) return 1;
    }
    n = *(uint32_t *)(DATA_VA + DIALOGS_OFF);
    if (n != *(uint32_t *)(our_data + DIALOGS_OFF)) return 1;
    if (n && memcmp(*(void **)(DATA_VA + DIALOGS_OFF + 0x18), *(void **)(our_data + DIALOGS_OFF + 0x18), n * 0x10)) return 1;
    return 0;
}

static uint32_t md_name, md_link, md_rout[2], md_oout[2], md_slot;
static uint32_t do_module(int s)
{
    static char path[2][32];
    modules_reset();
    sprintf(path[s], "hostfs\\%c\\%s", s ? 'o' : 'r', md_name < 4 ? (const char *[]){ "MA", "MB", "MC", "MD" }[md_name] : "MZ");
    switch (md_slot) {
    case 0: return ((fn3)SIDE(s, 0x26c))(P(s ? md_oout : md_rout), P(path[s]), 21);
    case 1: (s ? md_oout : md_rout)[1] = md_link; return ((fn1)SIDE(s, 0x270))(P(&(s ? md_oout : md_rout)[1]));
    default: return ((fn2)SIDE(s, 0x294))(P(md_name & 1 ? "LANG1" : md_name & 2 ? "LANG2" : "NOPE"), P(s ? md_oout : md_rout));
    }
}

static void check_modules(void)
{
    static const char *const names[] = { "+0x26c module-load", "+0x270 module-free", "+0x294 language" };
    uint32_t k, ops = 1 + rnd() % 6, i;
    if (!our_fn(0x26c) || !our_fn(0x294)) return;
    seed_list(MODS_OFF, 0, 0x14, 0);
    seed_list(DIALOGS_OFF, 0, 0x10, 0);
    for (i = 0; i < 4; i++) {                       /* the module files the loads look for */
        static char p[MAX_PATH];
        if (rnd() % 6 == 0) continue;
        sprintf(p, "hostfs\\r\\M%c.DLL", 'A' + i); write_file(p, "x");
        sprintf(p, "hostfs\\o\\M%c.DLL", 'A' + i); write_file(p, "x");
    }
    for (k = 0; k < ops && !differ; k++) {
        md_slot = rnd() % 5 < 3 ? 0 : rnd() % 3 ? 1 : 2;
        md_name = rnd() % 5;
        md_link = rnd() % 4 ? P(fake_link[rnd() % 4]) : rnd();
        md_rout[0] = md_oout[0] = rnd();
        logged_pair(names[md_slot], do_module);
        if (md_rout[0] != md_oout[0] || md_rout[1] != md_oout[1]) { printf("  %-16s OUT DIFFER\n", names[md_slot]); differ++; }
        if (modules_differ()) { printf("  %-16s RECORDS DIFFER\n", names[md_slot]); differ++; }
    }
}

/* +0x030 and +0x050 (the busy cursor), the purge hook the harness's. */
static uint32_t bc_on;
static uint32_t do_030(int s) { return ((fn0)SIDE(s, 0x30))(); }
static uint32_t do_050(int s) { return ((fn1)SIDE(s, 0x50))(bc_on); }

static void check_busy(void)
{
    uint32_t hook = rnd() % 4 ? P(fake_purge) : 0;
    if (!our_fn(0x30) || !our_fn(0x50)) return;
    *(uint32_t *)(DATA_VA + 0x1834) = *(uint32_t *)(our_data + 0x1834) = hook;           /* 0x418834 */
    *(uint32_t *)(DATA_VA + 0x185c) = *(uint32_t *)(our_data + 0x185c) = rnd() % 3;      /* 0x41885c: busy */
    bc_on = rnd() % 3;
    if (rnd() & 1) logged_pair("+0x030 resume", do_030);
    else logged_pair("+0x050 busy", do_050);
}

/* ---- sound ------------------------------------------------------------------- */

#define SND_STATE_OFF 0x3258u          /* 0x41a258: the dummy voice .. 0x41a3d0 */

static int cur_side;                    /* which side the call log is recording */
static uint32_t snd_buf_next[2];

/* A voice's place on its side's list (1-based), 0 for null, 0xffff off it. */
static uint32_t voice_index(int side, uint32_t v)
{
    uint32_t e = *(uint32_t *)((side ? our_data : (uint8_t *)DATA_VA) + 0x33c0), n = 1;
    if (!v) return 0;
    if (v == (side ? P(our_data) + 0x3258 : DATA_VA + 0x3258)) return 0xd0d0;
    for (; e; e = *(uint32_t *)(uintptr_t)(e + 0x168), n++) if (e == v) return n;
    return 0xffff;
}
static void WINAPI voice_cb(uint32_t v) { log_call(77, voice_index(cur_side, v), 0, 0, 0); }

/* WAV fixtures, the same in both directories. */
static void put_wav(const char *name, uint16_t tag, uint16_t ch, uint32_t rate, uint16_t bits, uint32_t n, int extra, int truncate)
{
    static uint8_t w[2048];
    uint8_t *p = w;
    uint32_t i, fmtlen = tag == 1 ? 16 : 20;
    memcpy(p, "RIFF", 4); p += 8; memcpy(p, "WAVE", 4); p += 4;
    if (extra) { memcpy(p, "LIST", 4); *(uint32_t *)(p + 4) = 6; p += 8; memcpy(p, "abcdef", 6); p += 6; }
    memcpy(p, "fmt ", 4); *(uint32_t *)(p + 4) = fmtlen; p += 8;
    *(uint16_t *)p = tag; *(uint16_t *)(p + 2) = ch; *(uint32_t *)(p + 4) = rate;
    *(uint32_t *)(p + 8) = rate * ch * bits / 8; *(uint16_t *)(p + 12) = (uint16_t)(ch * bits / 8); *(uint16_t *)(p + 14) = bits;
    if (fmtlen == 20) *(uint32_t *)(p + 16) = 0x1234;
    p += fmtlen;
    memcpy(p, "data", 4); *(uint32_t *)(p + 4) = n; p += 8;
    for (i = 0; i < n; i++) *p++ = (uint8_t)(rnd());
    *(uint32_t *)(w + 4) = (uint32_t)(p - w - 8);
    {
        static char path[64];
        uint32_t s;
        for (s = 0; s < 2; s++) {
            FILE *f;
            sprintf(path, "hostfs\\%c\\%s.wav", "ro"[s], name);
            f = fopen(path, "wb");
            if (f) { fwrite(w, 1, (size_t)(p - w) - (truncate ? n / 2 : 0), f); fclose(f); }
        }
    }
}

/* Voices whose sound retail has freed under them (the real side's
 * pointers): a duplicate that fails to get a buffer frees the sound it shares
 * with its source.  Their bytes are heap leftovers, not compared. */
static uint32_t dangling[16], ndangling, pre_v[32], pre_d[32], npre;

static int is_dangling(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < ndangling; i++) if (dangling[i] == v) return 1;
    return 0;
}

/* The two voice lists alike. */
static int voices_differ(void)
{
    uint32_t r = *(uint32_t *)(DATA_VA + 0x33c0), o = *(uint32_t *)(our_data + 0x33c0);
    for (; r || o; r = *(uint32_t *)(uintptr_t)(r + 0x168), o = *(uint32_t *)(uintptr_t)(o + 0x168)) {
        const uint8_t *a = (const uint8_t *)(uintptr_t)r, *b = (const uint8_t *)(uintptr_t)o;
        if (!r || !o) { printf("    voice lists differ in length (real %s)\n", r ? "longer" : "shorter"); return 1; }
        if (memcmp(a, b, 4) || memcmp(a + 8, b + 8, 0x18) || memcmp(a + 0x24, b + 0x24, 0x40)) {
            uint32_t q;
            for (q = 0; q < 0x64; q += 4)
                if (q != 4 && q != 0x20 && *(uint32_t *)(a + q) != *(uint32_t *)(b + q))
                    printf("    voice +%#x: real %#x ours %#x (type %u)\n", q, *(uint32_t *)(a + q), *(uint32_t *)(b + q), *(uint32_t *)(a + 0x24));
            return 1;
        }
        if (voice_index(0, *(uint32_t *)(a + 0x20)) != voice_index(1, *(uint32_t *)(b + 0x20)) &&
            *(uint32_t *)(a + 0x20) != *(uint32_t *)(b + 0x20) && *(uint32_t *)(a + 0x24) >= 6) { printf("    voice source differs\n"); return 1; }
        if (!*(uint32_t *)(a + 4) != !*(uint32_t *)(b + 4)) { printf("    voice data null-ness differs (type %u)\n", *(uint32_t *)(a + 0x24)); return 1; }
        if (*(uint32_t *)(a + 4) && *(uint32_t *)(a + 0x24) < 6 && !is_dangling(r) && *(uint32_t *)(a + 8) < 0x10000 &&
            memcmp((void *)(uintptr_t)*(uint32_t *)(a + 4), (void *)(uintptr_t)*(uint32_t *)(b + 4), *(uint32_t *)(a + 8))) {
            uint32_t q; const uint8_t *x = (const uint8_t *)(uintptr_t)*(uint32_t *)(a + 4), *y = (const uint8_t *)(uintptr_t)*(uint32_t *)(b + 4);
            printf("    voice data differs (type %u, %u bytes)\n     real:", *(uint32_t *)(a + 0x24), *(uint32_t *)(a + 8));
            for (q = 0; q < 24; q++) printf(" %02x", x[q]);
            printf("\n     ours:");
            for (q = 0; q < 24; q++) printf(" %02x", y[q]);
            printf("\n");
            return 1; }
        {   /* the name: only file voices (2, 4, 5) set it; the others leave it unterminated */
            uint32_t t = *(uint32_t *)(a + 0x24);
            if ((t == 2 || t == 4 || t == 5) &&
                (strlen((const char *)a + 0x64) != strlen((const char *)b + 0x64) ||
                 strncmp((const char *)a + 0x64, (const char *)b + 0x64, 7) ||
                 strcmp((const char *)a + 0x64 + 8, (const char *)b + 0x64 + 8))) {
                printf("    voice name [%.40s] [%.40s]\n", a + 0x64, b + 0x64); return 1;
            }
        }
    }
    return 0;
}

static uint32_t sd_slot, sd_args[6], sd_voice[2][8], sd_nvoice, sd_out[2][12];
static uint8_t sd_mem_data[256];
static uint32_t sd_mem_hdr[2][3], sd_mem_src[2][2], sd_mem_len;
static const uint8_t sd_mem_fmt[0x12] = { 1, 0, 1, 0, 0x22, 0x56, 0, 0, 0x22, 0x56, 0, 0, 1, 0, 8, 0, 0, 0 };
static const char *const sd_names[] = { "S1", "S2", "S3", "S4", "S5", "S6" };

static uint32_t do_sound(int s)
{
    static char path[2][32];
    uint32_t v = sd_args[1] < sd_nvoice ? sd_voice[s][sd_args[1]] : sd_args[1];
    cur_side = s;
    if (trace && s == 0) printf("sound slot %u args %#x %#x %#x %#x nvoice %u\n", sd_slot, sd_args[0], sd_args[1], sd_args[2], sd_args[3], sd_nvoice);
    buf_next = snd_buf_next[s];
    com_calls = 0;
    sprintf(path[s], "hostfs\\%c\\%s", s ? 'o' : 'r', sd_names[sd_args[0] % 6]);
    switch (sd_slot) {
    case 0: {       /* +0x2d4 new voice */
        uint32_t type = sd_args[2], src = type == 1 || type == 3 ? P(sd_mem_src[s]) : type >= 6 ? v : P(path[s]), r;
        if (type == 1 || type == 3) {   /* a sound in memory: its data a heap block of each side's own (the host may free it) */
            static uint32_t next[2];
            uint8_t *d = pinned_pool[s][next[s]++ % 64];     /* the host frees it and goes on using it */
            memcpy(d, sd_mem_data, sizeof sd_mem_data);
            sd_mem_hdr[s][0] = P(d); sd_mem_hdr[s][1] = 0; sd_mem_hdr[s][2] = sd_mem_len;
            sd_mem_src[s][0] = P(sd_mem_hdr[s]); sd_mem_src[s][1] = P(sd_mem_fmt);
        }
        r = ((fn3)SIDE(s, 0x2d4))(src, type, P(&sd_out[s][0]));
        if (!r && sd_out[s][0] && sd_nvoice < 8) sd_voice[s][sd_nvoice] = sd_out[s][0];
        sd_out[s][0] = voice_index(s, sd_out[s][0]);
        snd_buf_next[s] = buf_next;
        return r;
    }
    case 1: { uint32_t r; sd_out[s][1] = v; r = ((fn1)SIDE(s, 0x2dc))(P(&sd_out[s][1])); snd_buf_next[s] = buf_next;
              sd_out[s][1] = sd_out[s][1] != 0; return r; }
    case 2: { uint32_t r = ((fn4)SIDE(s, 0x2d8))(sd_args[3] ? P(path[s]) : v == sd_voice[s][0] ? 0 : (sd_nvoice > 1 ? sd_voice[s][1] : 0),
                                                 sd_args[3] ? 2 : sd_args[4] % 3 == 0 ? 0 : 6, v, sd_args[5] & 1);
              snd_buf_next[s] = buf_next; return r; }
    case 3: { uint32_t (__stdcall *f)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = SIDE(s, 0x2e0);
              return f(v, sd_args[2] & 1, sd_args[3], sd_args[4], sd_args[5] & 1 ? P(voice_cb) : 0); }
    case 4: return ((fn3)SIDE(s, 0x2e4))(v, sd_args[3], sd_args[4]);
    case 5: return ((fn2)SIDE(s, 0x2ec))(v, sd_args[2] % 10001);
    case 6: return ((fn2)SIDE(s, 0x2f0))(v, sd_args[2] % 20001 - 10000);
    case 7: return ((fn1)SIDE(s, 0x2f4))(v);
    case 8:     /* (+0x2e8 trusts its voice: a real one, or null) */
        memset(sd_out[s], 0x5a, sizeof sd_out[s]);
        return ((fn2)SIDE(s, 0x2e8))(sd_args[2] & 1 && sd_args[1] < sd_nvoice ? v : 0, P(&sd_out[s][2]));
    default: { uint32_t r = ((fn1)SIDE(s, 0x2d0))(sd_args[2] & 1); snd_buf_next[s] = buf_next; return r; }
    }
}

static void check_sound(void)
{
    static const char *const names[] = { "+0x2d4 voice-new", "+0x2dc voice-free", "+0x2d8 voice-append", "+0x2e0 voice-play",
        "+0x2e4 voice-freq", "+0x2ec voice-volume", "+0x2f0 voice-pan", "+0x2f4 voice-stop", "+0x2e8 voice-state", "+0x2d0 sound-on" };
    uint32_t k, ops = 2 + rnd() % 10, i, ds;
    if (!our_fn(0x2d4) || !our_fn(0x2d0)) return;
    *(uint32_t *)(DATA_VA + 0x1834) = *(uint32_t *)(our_data + 0x1834) = P(fake_purge);  /* +0x2d0 reaches it */
    {   /* the sound state: off or on, DirectSound or not, no voices, the dummy's primary buffer */
        static uint8_t st[0x178];
        memset(st, 0, sizeof st);
        *(uint32_t *)st = 0x16c;
        ds = rnd() % 5 ? P(&fake_ds) : 0;
        *(uint32_t *)(st + 0x60) = ds ? P(&fake_bufs[NBUF - 1]) : 0;
        memcpy(st + 0xc, sd_mem_fmt, 0x12);
        *(uint32_t *)(st + 0x16c) = 0x5555;                      /* 0x41a3c4: the DSOUND module */
        *(uint32_t *)(st + 0x170) = ds;                          /* 0x41a3c8 */
        *(uint32_t *)(st + 0x174) = rnd() % 8 == 0;              /* 0x41a3cc: off */
        memcpy((void *)(DATA_VA + SND_STATE_OFF), st, sizeof st); memcpy(our_data + SND_STATE_OFF, st, sizeof st);
        i = (uint32_t[]){ 0, 0xffffffffu, 4, 0 }[rnd() % 4];
        *(uint32_t *)(DATA_VA + 0x1308) = *(uint32_t *)(our_data + 0x1308) = i;   /* 0x418308 */
        fake_bufs[NBUF - 1].status = rnd() % 5;
    }
    put_wav("S1", 1, 1, 11025, 8, 40 + rnd() % 60, 0, 0);
    put_wav("S2", 1, 2, 22050, 16, 64, rnd() & 1, 0);
    put_wav("S3", 2, 1, 22050, 4, 33, 1, 0);
    put_wav("S4", 1, 1, 8000, 8, 50, 0, 1);
    put_wav("S6", 0x11, 1, 11025, 4, 28, 0, 0);     /* IMA ADPCM: neither PCM nor tag 2 */
    write_file("hostfs\\r\\S5.wav", "not a wave"); write_file("hostfs\\o\\S5.wav", "not a wave");
    for (i = 0; i < sizeof sd_mem_data; i++) sd_mem_data[i] = (uint8_t)rnd();
    sd_mem_len = 32 + rnd() % 200;
    sd_nvoice = 0; snd_buf_next[0] = snd_buf_next[1] = 0; ndangling = 0;
    for (k = 0; k < ops && !differ; k++) {
        uint32_t b;
        sd_slot = k < 2 ? 0 : rnd() % 3 == 0 ? 0 : rnd() % 10;
        for (i = 0; i < 6; i++) sd_args[i] = rnd();
        sd_args[1] = sd_nvoice && rnd() % 6 ? rnd() % sd_nvoice : rnd() % 3 ? 0 : sd_args[1];
        if (sd_slot == 0) sd_args[2] = 1 + rnd() % 7;
        /* a type 7 copies its source's sound into its buffer, and a type 5
         * has freed its own once buffered: retail copies from null */
        if (sd_slot == 0 && sd_args[2] == 7 && sd_args[1] < sd_nvoice &&
            (!*(uint32_t *)(uintptr_t)(sd_voice[0][sd_args[1]] + 4) || is_dangling(sd_voice[0][sd_args[1]]))) sd_args[2] = 6;
        if (sd_slot == 3 || sd_slot == 4) {                       /* a multiplier: 0, 1, or something */
            double m = rnd() % 3 == 0 ? 0.0 : (double)(rnd() % 4000) / 1000.0;
            memcpy(&sd_args[3], &m, 8);
        }
        /* a voice with no buffer of its own would fault these with DirectSound present */
        /* a failed start can leave DirectSound with no primary buffer, which
         * +0x2e8 asked about the primary (a null voice) would call through */
        if (sd_slot == 8 && !(sd_args[2] & 1 && sd_args[1] < sd_nvoice) && *(uint32_t *)(DATA_VA + 0x33c8) &&
            !*(uint32_t *)(DATA_VA + 0x32b8)) continue;
        if ((sd_slot >= 3 && sd_slot <= 8) && sd_args[1] < sd_nvoice &&
            !*(uint32_t *)(uintptr_t)(sd_voice[0][sd_args[1]] + 0x60) && *(uint32_t *)(DATA_VA + 0x33c8)) continue;
        /* appending to a duplicate frees the sound it shares with its source,
         * which then reads freed memory (a retail use-after-free) */
        if (sd_slot == 2 && sd_args[1] < sd_nvoice && *(uint32_t *)(uintptr_t)(sd_voice[0][sd_args[1]] + 0x24) >= 6) continue;
        /* nor to or from a voice whose sound is gone (a buffered type 5): retail copies from null */
        if (sd_slot == 2 && sd_args[1] < sd_nvoice && (!*(uint32_t *)(uintptr_t)(sd_voice[0][sd_args[1]] + 4) ||
                                                    is_dangling(sd_voice[0][sd_args[1]]))) continue;
        if (sd_slot == 2 && !sd_args[3] && sd_nvoice > 1 && (!*(uint32_t *)(uintptr_t)(sd_voice[0][1] + 4) ||
                                                           is_dangling(sd_voice[0][1]))) continue;
        /* nor drop one: retail frees its sound a second time */
        if (sd_slot == 1 && sd_args[1] < sd_nvoice && is_dangling(sd_voice[0][sd_args[1]])) continue;
        /* nor rebuffer a dangling voice's sound (+0x2d0 turning on) */
        if (sd_slot == 9 && ndangling) continue;
        for (b = 0; b < 4; b++) com_fail_mask[b] = rnd() % 4 ? 0 : rnd();
        com_err_shift = rnd() % 6;          /* which error each failing call gets */
        for (b = 0; b < NBUF - 1; b++) fake_bufs[b].status = rnd() % 5;
        memset(sd_out, 0, sizeof sd_out);
        {   /* every real voice's sound before the call */
            uint32_t e = *(uint32_t *)(DATA_VA + 0x33c0);
            for (npre = 0; e && npre < 32; e = *(uint32_t *)(uintptr_t)(e + 0x168), npre++) {
                pre_v[npre] = e; pre_d[npre] = *(uint32_t *)(uintptr_t)(e + 4);
            }
        }
        logged_pair(names[sd_slot], do_sound);
        {   /* a sound some voice gave up (freed with it, or replaced) and another still points at */
            uint32_t j, e;
            for (j = 0; j < npre; j++) {
                uint32_t still = 0, kept = 0;
                for (e = *(uint32_t *)(DATA_VA + 0x33c0); e; e = *(uint32_t *)(uintptr_t)(e + 0x168)) {
                    if (e == pre_v[j]) { still = 1; kept = *(uint32_t *)(uintptr_t)(e + 4) == pre_d[j]; }
                }
                if (pre_d[j] && (!still || !kept))
                    for (e = *(uint32_t *)(DATA_VA + 0x33c0); e; e = *(uint32_t *)(uintptr_t)(e + 0x168))
                        if (e != pre_v[j] && *(uint32_t *)(uintptr_t)(e + 4) == pre_d[j] && !is_dangling(e) && ndangling < 16)
                            dangling[ndangling++] = e;
            }
        }
        if (sd_slot == 0 && sd_args[2] >= 6 && sd_args[1] < sd_nvoice && ndangling < 16) {
            if (sd_out[0][0] == 0) dangling[ndangling++] = sd_voice[0][sd_args[1]];   /* a failed duplicate */
        }
        if (sd_slot == 0 && sd_out[0][0] && sd_out[0][0] != 0xffff && sd_nvoice < 8 && sd_voice[0][sd_nvoice]) sd_nvoice++;
        if (sd_slot == 1) {                                      /* forget a freed voice on both sides */
            uint32_t j = sd_args[1];
            if (j < sd_nvoice && voice_index(0, sd_voice[0][j]) == 0xffff) {
                for (; j + 1 < sd_nvoice; j++) { sd_voice[0][j] = sd_voice[0][j + 1]; sd_voice[1][j] = sd_voice[1][j + 1]; }
                sd_nvoice--;
            }
        }
        if (memcmp(sd_out[0], sd_out[1], sizeof sd_out[0])) { printf("  %-16s OUT DIFFER\n", names[sd_slot]); differ++; }
        if (voices_differ()) { printf("  %-16s VOICES DIFFER\n", names[sd_slot]); differ++; }
    }
}

/* ---- view-window surfaces and order ------------------------------------------ */

#define NREC 4
static uint32_t vrec[2][NREC][0x2a], vhas[NREC], vsurf_fail0;

/* A side's group list, built alike: two groups (keys 1 and 2) of the records
 * whose +0x78 says so, some left out. */
static void view_groups(int side, uint32_t member)
{
    uint32_t *head = (uint32_t *)((side ? our_data : (uint8_t *)DATA_VA) + 0x33dc), key, i, *g, *n, **gl = (uint32_t **)head;
    *head = 0;
    for (key = 1; key <= 2; key++) {
        g = 0;
        for (i = 0; i < NREC; i++) {
            if (vrec[side][i][0x78 / 4] != key || !((member >> i) & 1)) continue;
            if (!g) { g = malloc(12); g[0] = key; g[1] = 0; g[2] = 0; *gl = g; gl = (uint32_t **)&g[2]; }
            n = malloc(8); n[0] = P(vrec[side][i]); n[1] = g[1]; g[1] = P(n);
        }
    }
}

/* The two group lists alike, records by index. */
static int view_groups_differ(void)
{
    uint32_t r = *(uint32_t *)(DATA_VA + 0x33dc), o = *(uint32_t *)(our_data + 0x33dc);
    for (; r || o; r = *(uint32_t *)(uintptr_t)(r + 8), o = *(uint32_t *)(uintptr_t)(o + 8)) {
        uint32_t rn, on;
        if (!r || !o || *(uint32_t *)(uintptr_t)r != *(uint32_t *)(uintptr_t)o) return 1;
        rn = *(uint32_t *)(uintptr_t)(r + 4); on = *(uint32_t *)(uintptr_t)(o + 4);
        for (; rn || on; rn = *(uint32_t *)(uintptr_t)(rn + 4), on = *(uint32_t *)(uintptr_t)(on + 4)) {
            if (!rn || !on) return 1;
            if ((*(uint32_t *)(uintptr_t)rn - P(vrec[0])) != (*(uint32_t *)(uintptr_t)on - P(vrec[1]))) return 1;
        }
    }
    return 0;
}

/* Records alike (their blocks by contents, lists' data pointers aside). */
static int view_recs_differ(void)
{
    uint32_t i, k;
    for (i = 0; i < NREC; i++) {
        uint32_t *r = vrec[0][i], *o = vrec[1][i];
        for (k = 0; k < 0x2a; k++) {
            if (k == 0x90 / 4 || k == 0xa0 / 4) continue;
            if (r[k] != o[k]) { printf("    record %u +%#x: real %#x ours %#x\n", i, k * 4, r[k], o[k]); return 1; }
        }
        if (!r[0xa0 / 4] != !o[0xa0 / 4] || !r[0x90 / 4] != !o[0x90 / 4]) return 1;
        if (r[0x90 / 4] && memcmp((void *)(uintptr_t)r[0x90 / 4], (void *)(uintptr_t)o[0x90 / 4], 16 * 4)) {
            printf("    record %u rows differ\n", i); return 1;
        }
        if (r[0xa0 / 4] && vhas[i]) {
            uint32_t *rb = (uint32_t *)(uintptr_t)r[0xa0 / 4], *ob = (uint32_t *)(uintptr_t)o[0xa0 / 4];
            for (k = 0; k < 0x90 / 4; k++) {
                if (k == (0x54 + 0x18) / 4 || k == (0x70 + 0x18) / 4) continue;
                if (rb[k] != ob[k]) { printf("    record %u block +%#x: real %#x ours %#x\n", i, k * 4, rb[k], ob[k]); return 1; }
            }
        }
    }
    return 0;
}

typedef uint32_t (__stdcall *fn7)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
static uint32_t vw_slot, vw_idx, vw_hwnd, vw_cwfail, vw_null, vw_out[2], vw_rect[2][4], vw_fg, vw_bg, vw_kind;
static char vw_text[24];
static uint32_t do_view(int s)
{
    uint32_t rec = P(vrec[s][vw_idx]);
    surf_fail = vsurf_fail0;
    cw_seq = 0; cw_fail = vw_cwfail;
    switch (vw_slot) {
    case 6: return ((fn1)SIDE(s, 0x64))(vw_null ? 0 : rec);
    case 7: return ((fn0)SIDE(s, 0x4c))();
    case 10:
        font_seq = 0;
        return ((fn5)SIDE(s, 0x8c))(vw_null ? 0 : rec, vw_kind, vw_fg, P(vw_text), rnd());
    case 9: {
        uint32_t r;
        font_seq = 0;
        r = ((fn7)SIDE(s, 0x318))(rec, P(vw_rect[s]), vw_fg, vw_bg, P(vw_text), vw_kind, rnd());
        if (memcmp(vw_rect[0], vw_rect[1], sizeof vw_rect[0]) && s) { printf("  +0x318 view-text RECT DIFFER\n"); differ++; }
        return r;
    }
    case 8: {
        uint32_t r = ((fn3)SIDE(s, 0x94))(P(&vw_out[s]), vw_idx, vw_hwnd), o = vw_out[s];
        vw_out[s] = o ? o - P(vrec[s]) + 1 : 0;     /* a record as its offset */
        return r;
    }
    case 0: return ((fn2)SIDE(s, 0x74))(vw_hwnd, rec);
    case 1: return ((fn1)SIDE(s, 0x78))(rec);
    case 2: return ((fn1)SIDE(s, 0x68))(rec);
    case 3: return ((fn1)SIDE(s, 0x6c))(rec);
    case 4: return ((fn1)SIDE(s, 0x70))(rec);
    default: return ((fn1)SIDE(s, 0x90))(vw_idx == 3 && rnd() == 0 ? 0 : rec);
    }
}

static void check_views(void)
{
    static const char *const names[] = { "+0x074 view-block", "+0x078 view-free", "+0x068 view-destroy",
                                         "+0x06c view-lock", "+0x070 view-unlock", "+0x090 view-front",
                                         "+0x064 view-create", "+0x04c main-size", "+0x094 view-nth",
                                         "+0x318 view-text", "+0x08c view-label" };
    uint32_t i, k, ops = 1 + rnd() % 8, member = rnd();
    if (!our_fn(0x6c) || !our_fn(0x90) || !our_fn(0x64) || !our_fn(0x4c)) return;
    view_rec_base[0] = P(vrec[0]); view_rec_base[1] = P(vrec[1]); view_rec_size = sizeof vrec[0];
    /* the records: each side its own, alike; a host block with a row table for most */
    for (i = 0; i < NREC; i++) {
        uint32_t w;
        for (w = 0; w < 0x2a; w++) vrec[0][i][w] = rnd();
        vrec[0][i][0x78 / 4] = 1 + rnd() % 2;
        vrec[0][i][0x7c / 4] = rnd() % 4 != 0;
        vrec[0][i][8 / 4] = (rnd() & 0xe001e000u) & ~0x2000u;     /* DD, 0x4000 kept, 0x8000 palette colours, 0x20000000 unclipped */
        if (rnd() % 3 == 0) vrec[0][i][8 / 4] |= 0x2000;
        vrec[0][i][0x9c / 4] = 0;
        memcpy(vrec[1][i], vrec[0][i], sizeof vrec[0][i]);
        vhas[i] = rnd() % 5 != 0;
        if (vhas[i]) {
            uint32_t *rb = malloc(0x90), *ob = malloc(0x90);
            for (w = 0; w < 0x90 / 4; w++) rb[w] = ob[w] = rnd();
            rb[0x1c / 4] = ob[0x1c / 4] = P(fake_bits) + (rnd() % 4) * 0x40;
            rb[0x20 / 4] = ob[0x20 / 4] = 0x20 + (rnd() % 3) * 0x10;
            rb[0x30 / 4] = ob[0x30 / 4] = 0x5000 + rnd() % 0x10;          /* the bitmap: its height in the low bits */
            rb[0x54 / 4] = ob[0x54 / 4] = 0; rb[(0x54 + 0x18) / 4] = ob[(0x54 + 0x18) / 4] = 0;
            rb[0x70 / 4] = ob[0x70 / 4] = 0; rb[(0x70 + 0x18) / 4] = ob[(0x70 + 0x18) / 4] = 0;
            vrec[0][i][0xa0 / 4] = P(rb); vrec[1][i][0xa0 / 4] = P(ob);
        } else {
            vrec[0][i][0xa0 / 4] = vrec[1][i][0xa0 / 4] = 0;
        }
        if (rnd() % 6) {
            uint32_t *rr = calloc(16, 4), *orr = calloc(16, 4);
            if (rnd() & 1) rr[0] = orr[0] = vhas[i] ? ((uint32_t *)(uintptr_t)vrec[0][i][0xa0 / 4])[0x1c / 4] : 0;
            vrec[0][i][0x90 / 4] = P(rr); vrec[1][i][0x90 / 4] = P(orr);
        } else {
            vrec[0][i][0x90 / 4] = vrec[1][i][0x90 / 4] = 0;
        }
    }
    view_groups(0, member); view_groups(1, member);
    *(uint32_t *)(DATA_VA + 0x34e0) = *(uint32_t *)(our_data + 0x34e0) = rnd() % 3 ? P(&fake_surf) : 0;   /* 0x41a4e0 */
    *(uint32_t *)(DATA_VA + 0x1858) = *(uint32_t *)(our_data + 0x1858) = rnd() % 4 != 0;              /* 0x418858 */
    *(uint32_t *)(DATA_VA + 0x3250) = *(uint32_t *)(our_data + 0x3250) = rnd() % 4 == 0;              /* 0x41a250 */
    *(uint32_t *)(DATA_VA + 0x13b4) = *(uint32_t *)(our_data + 0x13b4) = rnd() % 3;                   /* 0x4183b4 */
    *(uint32_t *)(DATA_VA + 0x33e8) = *(uint32_t *)(our_data + 0x33e8) = rnd() % 6 == 0;              /* 0x41a3e8 */
    gp_main_mask = rnd();
    for (k = 0; k < ops && !differ; k++) {
        vw_slot = rnd() % 11; vw_idx = rnd() % NREC; vw_hwnd = rnd(); vsurf_fail0 = rnd() % 3 ? 0 : rnd();
        vw_cwfail = rnd() % 4 ? 0 : rnd() & 3; wl_seed = rnd(); vw_null = rnd() % 16 == 0;
        if (vw_slot == 6) {     /* a new view: the record's group key, sometimes one with no group yet */
            vrec[0][vw_idx][0x78 / 4] = vrec[1][vw_idx][0x78 / 4] = rnd() % 4;
        }
        if (vw_slot == 8) {     /* a key (some with no group), the nth flagged record */
            vw_idx = rnd() % 4; vw_hwnd = rnd() % 4; vw_out[0] = vw_out[1] = rnd();
        }
        if (vw_slot == 9 || vw_slot == 10) {     /* text: a font kind or label place (a bad one now and then), colours or -1, a rectangle */
            uint32_t j;
            vw_kind = vw_slot == 10 ? (rnd() % 16 ? rnd() % 15 : 15 + rnd() % 3) : rnd() % 16 ? rnd() % 4 : 4 + rnd() % 3;
            fake_acp = rnd() % 4 ? 1252 : 0x3a4;
            *(uint32_t *)(DATA_VA + 0x33d4) = *(uint32_t *)(our_data + 0x33d4) = rnd() % 3 ? 0 : rnd();   /* 0x41a3d4 */
            *(uint32_t *)(DATA_VA + 0x33d8) = *(uint32_t *)(our_data + 0x33d8) = rnd() % 3 ? 0 : rnd();   /* 0x41a3d8 */
            vw_fg = rnd() % 4 ? rnd() : (uint32_t)-1; vw_bg = rnd() % 2 ? rnd() : (uint32_t)-1;
            for (j = 0; j < 4; j++) vw_rect[0][j] = vw_rect[1][j] = rnd() % 640;
            for (j = 0; j + 1 < sizeof vw_text; j++) vw_text[j] = "Ab c,\n&x"[rnd() % 9];
            vw_text[rnd() % sizeof vw_text] = 0; vw_text[sizeof vw_text - 1] = 0;
            *(uint32_t *)(DATA_VA + 0x33e0) = *(uint32_t *)(our_data + 0x33e0) = rnd() % 3 ? 0 : rnd();   /* 0x41a3e0 */
            *(uint32_t *)(DATA_VA + 0x33e4) = *(uint32_t *)(our_data + 0x33e4) = rnd() % 3 ? 0 : rnd();   /* 0x41a3e4 */
            *(uint32_t *)(DATA_VA + 0x34f4) = *(uint32_t *)(our_data + 0x34f4) = rnd() % 2;               /* 0x41a4f4 */
        }
        if (vw_slot == 7)
            *(uint32_t *)(DATA_VA + 0x1854) = *(uint32_t *)(our_data + 0x1854) = rnd() % 3 == 0;   /* 0x418854 */
        /* a record whose block is gone: only a new one is safe (retail reads the block unchecked) */
        if (!vhas[vw_idx] && vw_slot != 0 && vw_slot != 5 && (vw_slot < 6 || vw_slot == 9 || (vw_slot == 10 && !vw_null))) continue;
        if (vw_slot == 5) {     /* the restack reads every grouped frame's block */
            uint32_t j, bad = 0;
            for (j = 0; j < NREC; j++) if (!vhas[j]) bad = 1;
            if (bad) continue;
        }
        if ((vw_slot == 3 || vw_slot == 9 || vw_slot == 10) && !vrec[0][vw_idx][0x90 / 4] && vrec[0][vw_idx][0x7c / 4]) continue;   /* no row table (+0x318 relocks) */
        logged_pair(names[vw_slot], do_view);
        if (vw_slot == 8 && vw_out[0] != vw_out[1]) { printf("  +0x094 view-nth OUT DIFFER %#x %#x\n", vw_out[0], vw_out[1]); differ++; }
        if (vw_slot == 0 || (vw_slot == 6 && !vw_null && vrec[0][vw_idx][0xa0 / 4])) vhas[vw_idx] = 1;
        if (vw_slot == 6 && !vw_null && !vrec[0][vw_idx][0xa0 / 4]) vhas[vw_idx] = 0;
        if (vw_slot == 1 || vw_slot == 2) vhas[vw_idx] = 0;
        if (view_recs_differ()) { printf("  %-16s RECORDS DIFFER\n", names[vw_slot]); differ++; }
        if (view_groups_differ()) { printf("  %-16s GROUPS DIFFER\n", names[vw_slot]); differ++; }
    }
}

/* +0x128: the three words cleared through a block, and the library at
 * 0x41a51c freed (FreeLibrary failing by the script). */
static uint32_t lib_words[2][3], lib_block[2][3], lib_null;
static uint32_t do_128(int s)
{
    uint32_t i;
    for (i = 0; i < 3; i++) lib_block[s][i] = P(&lib_words[s][i]);
    return ((fn1)SIDE(s, 0x128))(lib_null ? 0 : P(lib_block[s]));
}
static void check_library_free(void)
{
    uint32_t i;
    if (!our_fn(0x128)) return;
    for (i = 0; i < 3; i++) lib_words[0][i] = lib_words[1][i] = rnd();
    lib_null = rnd() % 4 == 0;
    fl_fail = rnd() % 3 == 0;
    *(uint32_t *)(DATA_VA + 0x3518) = *(uint32_t *)(our_data + 0x3518) = rnd() % 3 ? rnd() : 0;   /* 0x41a518 */
    *(uint32_t *)(DATA_VA + 0x351c) = *(uint32_t *)(our_data + 0x351c) = rnd();                   /* 0x41a51c */
    logged_pair("+0x128 lib-free", do_128);
    fl_fail = 0;
    if (memcmp(lib_words[0], lib_words[1], sizeof lib_words[0])) { printf("  +0x128 lib-free WORDS DIFFER\n"); differ++; }
}

/* ---- the palette -------------------------------------------------------------- */

/* GDI's palette calls, logged; the DC's raster caps by the script.  A fake
 * DirectDraw object, primary surface (its format by the script) and palette,
 * which log their calls (a palette's entries as a checksum). */
static uint32_t pal_caps, pal_desc[0x1b];
static uint32_t pal_sum(const void *p, uint32_t n)
{
    const uint8_t *b = p; uint32_t h = 0, i;
    for (i = 0; i < n; i++) h = h * 31 + b[i];
    return h;
}
static HDC WINAPI fake_GetDC(HWND h) { log_call(120, P(h), 0, 0, 0); return (HDC)0xdc00; }
static int WINAPI fake_ReleaseDC(HWND h, HDC dc) { log_call(121, P(h), P(dc), 0, 0); return 0x51; }
static int WINAPI fake_GetDeviceCaps(HDC dc, int i) { log_call(122, P(dc), (uint32_t)i, 0, 0); return (int)(pal_caps * (uint32_t)(i + 3)) & (i == 8 || i == 10 ? 0xfff : i == 12 || i == 14 ? 0xf : -1); }
static BOOL WINAPI fake_AnimatePalette(HPALETTE h, UINT a, UINT n, const PALETTEENTRY *e)
{ log_call(123, P(h), a, n, P(e)); log_call(124, pal_sum(e, n * 4), 0, 0, 0); return TRUE; }
static UINT WINAPI fake_SetPaletteEntries(HPALETTE h, UINT a, UINT n, const PALETTEENTRY *e)
{ log_call(125, P(h), a, n, P(e)); log_call(124, pal_sum(e, n * 4), 0, 0, 0); return n; }
static HPALETTE WINAPI fake_SelectPalette(HDC dc, HPALETTE h, BOOL bg) { log_call(126, P(dc), P(h), (uint32_t)bg, 0); return 0; }
static UINT WINAPI fake_RealizePalette(HDC dc) { log_call(127, P(dc), 0, 0, 0); return 0; }
static BOOL WINAPI fake_RedrawWindow(HWND h, const RECT *r, HRGN g, UINT f) { log_call(128, P(h), P(r), P(g), f); return TRUE; }

static struct fake_surface pal_dd, pal_prim, pal_pal;
static void *pal_dd_vtbl[8], *pal_prim_vtbl[34], *pal_pal_vtbl[8];
static uint32_t WINAPI pd_create(void *o, uint32_t f, void *e, uint32_t *out, void *u)
{ log_call(129, f, P(e), norm(P(out)), P(u)); log_call(124, pal_sum(e, 0x400), 0, 0, 0); *out = P(&pal_pal); (void)o; return 0; }
static uint32_t WINAPI pp_set(void *o, uint32_t f, uint32_t a, uint32_t n, void *e)
{ log_call(130, f, a, n, P(e)); log_call(124, pal_sum(e, n * 4), 0, 0, 0); (void)o; return 0; }
static uint32_t WINAPI ps_desc(void *o, uint32_t *d)
{ log_call(131, d[0], 0, 0, 0); memcpy(d + 1, pal_desc + 1, sizeof pal_desc - 4); (void)o; return 0; }
static uint32_t WINAPI ps_setpal(void *o, void *p) { log_call(132, P(p) == P(&pal_pal), P(p), 0, 0); (void)o; return 0; }

static uint8_t pal_rgb[2][2][0x300];
static uint32_t pal_null;
static uint32_t do_290(int s) { return ((fn2)SIDE(s, 0x290))(pal_null ? 0 : P(pal_rgb[s][0]), P(pal_rgb[s][1])); }

static void check_palette(void)
{
    uint32_t i;
    static int hooked;
    if (!our_fn(0x290)) return;
    if (!hooked) {
        for (i = 0; i < 8; i++) pal_dd_vtbl[i] = pal_pal_vtbl[i] = (void *)s_other;
        for (i = 0; i < 34; i++) pal_prim_vtbl[i] = (void *)s_other;
        pal_dd_vtbl[0x14 / 4] = (void *)pd_create; pal_pal_vtbl[0x18 / 4] = (void *)pp_set;
        pal_prim_vtbl[0x58 / 4] = (void *)ps_desc; pal_prim_vtbl[0x7c / 4] = (void *)ps_setpal;
        pal_dd.vtbl = pal_dd_vtbl; pal_prim.vtbl = pal_prim_vtbl; pal_pal.vtbl = pal_pal_vtbl;
        hooked = 1;
    }
    for (i = 0; i < sizeof pal_rgb[0]; i++) ((uint8_t *)pal_rgb[0])[i] = ((uint8_t *)pal_rgb[1])[i] = (uint8_t)rnd();
    pal_null = rnd() % 8 == 0;
    pal_caps = rnd() % 3 ? rnd() | (rnd() & 0x100) : rnd() & ~0x100u;
    for (i = 0; i < 0x1b; i++) pal_desc[i] = rnd();
    pal_desc[0x54 / 4] = rnd() % 3 ? 8 : rnd() % 2 ? 16 : rnd() % 33;
    if (rnd() & 1) { pal_desc[0x58 / 4] = 0x7c00; pal_desc[0x5c / 4] = 0x3e0; pal_desc[0x60 / 4] = 0x1f; }
    if (rnd() & 1) { pal_desc[0x58 / 4] = 0x7c00 + (rnd() & 1); pal_desc[0x5c / 4] = 0x3e0 + (rnd() & 1); pal_desc[0x60 / 4] = 0x1f + (rnd() & 1); }
#define BOTH(off, v) (*(uint32_t *)(DATA_VA + (off)) = *(uint32_t *)(our_data + (off)) = (v))
    BOTH(0x34e4, rnd() % 4 ? P(&pal_dd) : 0);                                  /* 0x41a4e4 */
    BOTH(0x34f8, rnd() % 4 ? P(&pal_prim) : 0);                                /* 0x41a4f8 */
    BOTH(0x34e8, rnd() % 2 ? P(&pal_pal) : 0);                                 /* 0x41a4e8 */
    BOTH(0x34ec, rnd() % 4);                                                   /* 0x41a4ec */
    BOTH(0x1854, rnd() % 4 != 0);                                              /* 0x418854 */
    BOTH(0x3250, rnd() % 3 == 0);                                              /* 0x41a250 */
    BOTH(0x3254, rnd() % 3 == 0);                                              /* 0x41a254 */
#undef BOTH
    logged_pair("+0x290 palette", do_290);
    if (memcmp(pal_rgb[0], pal_rgb[1], sizeof pal_rgb[0])) { printf("  +0x290 palette INPUT CHANGED\n"); differ++; }
}

/* ---- DirectPlay ---------------------------------------------------------------- */

/* A fake DirectPlay object (QueryInterface by script, Release logged) and a
 * lobby whose GetConnectionSettings sizes and then fills a DPLCONNECTION:
 * connection, session description, player name and address, then the three
 * strings, the pointers into the caller's block. */
static void *dp_vtbl[8], *lobby_vtbl[16];
static struct fake_surface dp_obj, dp_obj2, lobby_obj;
static uint8_t dp_strings[3][40];
static uint32_t WINAPI dp_qi(void *o, const GUID *iid, uint32_t *out)
{
    log_call(160, P(o) == P(&dp_obj), pal_sum(iid, 16), 0, 0);
    if (dp_script.qi_fail) { *out = 0; return 0x80004002u; }
    *out = P(&dp_obj2);
    return 0;
}
static uint32_t WINAPI dp_release(void *o) { log_call(161, P(o) == P(&dp_obj) ? 1 : P(o) == P(&dp_obj2) ? 2 : 3, 0, 0, 0); return 0; }
static uint32_t WINAPI fake_DirectPlayCreate(const GUID *g, uint32_t *out, void *unk)
{
    log_call(162, pal_sum(g, 16), P(unk), 0, 0);
    if (dp_script.create_fail) { *out = 0; return 0x80004005u; }
    *out = P(&dp_obj);
    return 0;
}
static uint32_t WINAPI fake_DirectPlayLobbyCreateA(const GUID *g, uint32_t *out, void *unk, void *data, uint32_t n)
{
    log_call(163, P(g), P(unk), P(data), n);
    if (dp_script.lobby_fail) { *out = 0; return 0x80004005u; }
    *out = P(&lobby_obj);
    return 0;
}
static uint32_t WINAPI lobby_settings(void *o, uint32_t app, uint8_t *data, uint32_t *size)
{
    uint32_t i, k, at, len[3];
    log_call(164, app, data != 0, *size, 0); (void)o;
    if (!data) { *size = dp_script.conn_size; return dp_script.conn_size ? 0x8877001e : 0; }
    for (i = 0; i < 3; i++) len[i] = (uint32_t)strlen((char *)dp_strings[i]) + 1;
    for (i = 0; i < *size; i++) data[i] = (uint8_t)(i * 7 + dp_script.conn_size);
    k = 0x10 + dp_script.conn_size % 0x20;          /* the address's size */
    *(uint32_t *)(data + 0x24) = k;
    *(uint32_t *)(data + 8) = P(data + 0x28);
    *(uint32_t *)(data + 0xc) = P(data + 0x78);
    *(uint32_t *)(data + 0x20) = P(data + 0x88);
    at = 0x88 + k;
    *(uint32_t *)(data + 0x28 + 0x30) = P(data + at); memcpy(data + at, dp_strings[0], len[0]); at += len[0];
    *(uint32_t *)(data + 0x28 + 0x34) = P(data + 0x28);           /* the password: left as it is */
    *(uint32_t *)(data + 0x78 + 8) = P(data + at); memcpy(data + at, dp_strings[1], len[1]); at += len[1];
    *(uint32_t *)(data + 0x78 + 0xc) = P(data + at); memcpy(data + at, dp_strings[2], len[2]);
    return 0;
}

static uint32_t dp_rblk[2], dp_rsize[2], dp_ptrs[2][3][1], dp_block[2][3];
static uint32_t dp_frees[2];
static uint32_t do_120(int s)
{
    uint32_t r, n = nfrees;
    r = ((fn3)SIDE(s, 0x120))(P(&dp_rblk[s]), P(&dp_rsize[s]), dp_script.flat);
    dp_frees[s] = nfrees - n;
    return r;
}
static uint32_t do_124(int s)
{
    uint32_t i;
    for (i = 0; i < 3; i++) { dp_ptrs[s][i][0] = 0x5a5a5a5a; dp_block[s][i] = P(dp_ptrs[s][i]); }
    return ((fn1)SIDE(s, 0x124))(dp_script.flat ? P(dp_block[s]) : 0);
}

/* A returned block alike, its pointers as offsets from its start (the
 * flattened password pointer, into the lobby's freed copy, as null or not). */
static int dp_blocks_differ(void)
{
    uint8_t *r = (uint8_t *)(uintptr_t)dp_rblk[0], *o = (uint8_t *)(uintptr_t)dp_rblk[1];
    static const uint32_t ptrs[] = { 8, 0xc, 0x20, 0x28 + 0x30, 0x28 + 0x34, 0x78 + 8, 0x78 + 0xc };
    uint32_t i, k;
    if (dp_rsize[0] != dp_rsize[1] || !r != !o) return 1;
    if (!r) return 0;
    for (i = 0; i < dp_rsize[0]; i++) {
        for (k = 0; k < 7; k++) if (i >= ptrs[k] && i < ptrs[k] + 4) break;
        if (k == 7 && r[i] != o[i]) { printf("    byte %#x: real %#x ours %#x\n", i, r[i], o[i]); return 1; }
    }
    for (k = 0; k < 7; k++) {
        uint32_t rv = *(uint32_t *)(r + ptrs[k]), ov = *(uint32_t *)(o + ptrs[k]);
        if (k == 4 && dp_script.flat) { if (!rv != !ov) return 1; continue; }
        if (!dp_script.flat) { rv -= P(r); ov -= P(o); }
        if (rv != ov) { printf("    pointer +%#x: real %#x ours %#x\n", ptrs[k], rv, ov); return 1; }
    }
    return 0;
}

static void check_dplay(void)
{
    uint32_t i, k;
    static int init;
    if (!our_fn(0x120) || !our_fn(0x124)) return;
    if (!init) {
        for (i = 0; i < 8; i++) dp_vtbl[i] = (void *)s_other;
        for (i = 0; i < 16; i++) lobby_vtbl[i] = (void *)s_other;
        dp_vtbl[0] = (void *)dp_qi; dp_vtbl[2] = (void *)dp_release;
        lobby_vtbl[2] = (void *)dp_release; lobby_vtbl[0x20 / 4] = (void *)lobby_settings;
        dp_obj.vtbl = dp_obj2.vtbl = dp_vtbl; lobby_obj.vtbl = lobby_vtbl;
        dp_create_ptr = (void *)fake_DirectPlayCreate; dp_lobby_ptr = (void *)fake_DirectPlayLobbyCreateA;
        init = 1;
    }
    memset(&dp_script, 0, sizeof dp_script);
    dp_script.no_lib = rnd() % 6 == 0; dp_script.no_create = rnd() % 6 == 0; dp_script.no_lobby = rnd() % 6 == 0;
    dp_script.create_fail = rnd() % 6 == 0; dp_script.qi_fail = rnd() % 6 == 0; dp_script.lobby_fail = rnd() % 6 == 0;
    dp_script.conn_size = rnd() % 5 ? 0x140 + rnd() % 0x80 : 0;     /* room for the strings (0x88 + 0x2f + 3 * 31) */
    dp_script.flat = rnd() & 1;
    for (i = 0; i < 3; i++) {
        uint32_t n = rnd() % 30;
        for (k = 0; k < n; k++) dp_strings[i][k] = (uint8_t)('a' + rnd() % 26);
        dp_strings[i][n] = 0;
    }
    *(uint32_t *)(DATA_VA + 0x3518) = *(uint32_t *)(our_data + 0x3518) = rnd() % 2 ? rnd() : 0;                 /* 0x41a518 */
    *(uint32_t *)(DATA_VA + 0x351c) = *(uint32_t *)(our_data + 0x351c) = rnd() % 2 ? 0 : rnd() % 2 ? 0x6100 : rnd();   /* 0x41a51c */
    if (rnd() & 1) {
        dp_rblk[0] = dp_rblk[1] = dp_rsize[0] = dp_rsize[1] = 0x77;
        logged_pair("+0x120 dplay-lobby", do_120);
        if (dp_blocks_differ()) { printf("  +0x120 dplay-lobby BLOCKS DIFFER\n"); differ++; }
        if (dp_frees[0] != dp_frees[1]) { printf("  +0x120 dplay-lobby FREES DIFFER %u %u\n", dp_frees[0], dp_frees[1]); differ++; }
        free((void *)(uintptr_t)dp_rblk[0]); free((void *)(uintptr_t)dp_rblk[1]);
    } else {
        logged_pair("+0x124 dplay-load", do_124);
        if (memcmp(dp_ptrs[0], dp_ptrs[1], sizeof dp_ptrs[0])) { printf("  +0x124 dplay-load POINTERS DIFFER\n"); differ++; }
    }
}

/* ---- display settings (+0x31c) ------------------------------------------------ */

/* Scripted DirectDraw drivers (some failing to be made), each with Direct3D
 * devices (HAL, MMX, another; the HAL's flags by script), display modes, a
 * video memory and a current mode.  Every call is logged. */
#define NDRV 4
static const uint8_t guid_hal[16] = { 0xe0, 0x3d, 0xe6, 0x84, 0xaa, 0x46, 0xcf, 0x11, 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e };
static const uint8_t guid_mmx[16] = { 0xa1, 0x49, 0x19, 0x88, 0xf3, 0xd6, 0xd0, 0x11, 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29 };
static const uint8_t guid_rgb[16] = { 0x60, 0x5c, 0x66, 0xa4, 0x73, 0x26, 0xcf, 0x11, 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 };
static uint8_t drv_guid[NDRV][16];
static struct drv {
    int has_guid, create_fail, no_d3d, ndev, nmode, stop_at;
    char desc[40], name[24];
    const uint8_t *dev_guid[3];
    uint32_t hal[3][0x34], hel[3][0x34];
    uint32_t mode[8][3];
    uint32_t vidmem, cur[3];
} drv[NDRV];
static uint32_t ndrv;
struct fake_dd { void **vtbl; uint32_t idx; };
static struct fake_dd dd_obj[NDRV], d3d_obj[NDRV];
static void *dd_vtbl[32], *d3d_vtbl[8];

static HRESULT WINAPI fake_DirectDrawCreate(GUID *g, void **out, void *unk)
{
    uint32_t i;
    for (i = 0; i < ndrv; i++)
        if (drv[i].has_guid ? g != 0 && !memcmp(g, drv_guid[i], 16) : g == 0) break;
    log_call(170, i, P(unk), 0, 0);
    if (i == ndrv || drv[i].create_fail) { *out = 0; return 0x80004005; }
    *out = &dd_obj[i];
    return 0;
}
static HRESULT WINAPI fake_DirectDrawEnumerateA(LPDDENUMCALLBACKA cb, void *ctx)
{
    uint32_t i;
    log_call(171, norm(P(ctx)) & 0xff000000u, 0, 0, 0);
    for (i = 0; i < ndrv; i++) {
        BOOL r = cb(drv[i].has_guid ? (GUID *)drv_guid[i] : 0, drv[i].desc, drv[i].name, ctx);
        log_call(172, i, (uint32_t)r, 0, 0);
        if (!r) break;
    }
    return 0;
}
static HRESULT WINAPI dd_qi(struct fake_dd *o, const GUID *iid, void **out)
{
    log_call(173, o->idx, pal_sum(iid, 16), 0, 0);
    if (drv[o->idx].no_d3d) { *out = 0; return 0x80004002; }
    *out = &d3d_obj[o->idx];
    return 0;
}
static ULONG WINAPI dd_release(struct fake_dd *o) { log_call(174, o->idx, o->vtbl == d3d_vtbl, 0, 0); return 0; }
static HRESULT WINAPI dd_enummodes(struct fake_dd *o, DWORD f, void *desc, void *ctx, HRESULT (WINAPI *cb)(void *, void *))
{
    uint32_t i, d[0x1b];
    log_call(175, o->idx, f, P(desc), 0);
    for (i = 0; i < (uint32_t)drv[o->idx].nmode; i++) {
        HRESULT r;
        memset(d, 0, sizeof d);
        d[0] = 0x6c; d[2] = drv[o->idx].mode[i][1]; d[3] = drv[o->idx].mode[i][0]; d[0x54 / 4] = drv[o->idx].mode[i][2];
        r = cb(d, ctx);
        log_call(176, i, (uint32_t)r, 0, 0);
        if (!r) break;
    }
    return 0;
}
static HRESULT WINAPI dd_getcaps(struct fake_dd *o, uint32_t *caps, uint32_t *hel)
{
    log_call(177, o->idx, caps[0], P(hel), 0);
    memset(caps + 1, 0x11, 0x16c - 4); caps[0x3c / 4] = drv[o->idx].vidmem;
    return 0;
}
static HRESULT WINAPI dd_getmode(struct fake_dd *o, uint32_t *d)
{
    log_call(178, o->idx, d[0], d[1], 0);
    d[2] = drv[o->idx].cur[1]; d[3] = drv[o->idx].cur[0]; d[0x54 / 4] = drv[o->idx].cur[2];
    return 0;
}
static HRESULT WINAPI d3d_enumdev(struct fake_dd *o, HRESULT (WINAPI *cb)(GUID *, LPSTR, LPSTR, void *, void *, void *), void *ctx)
{
    uint32_t i;
    log_call(179, o->idx, 0, 0, 0);
    for (i = 0; i < (uint32_t)drv[o->idx].ndev; i++) {
        HRESULT r = cb((GUID *)drv[o->idx].dev_guid[i], "Direct3D device", "dev", drv[o->idx].hal[i], drv[o->idx].hel[i], ctx);
        log_call(180, i, (uint32_t)r, 0, 0);
        if (!r) break;
    }
    return 0;
}
static HRESULT WINAPI dd_other(struct fake_dd *o) { log_call(181, o->idx, 0, 0, 0); return 0; }

/* For +0x048: the rest of the DirectDraw object (palettes, surfaces, flip,
 * cooperative level, display mode -- failing by script), fake surfaces and
 * palettes (Restore failing by script), and the window calls the mode
 * switch makes. */
static struct { uint32_t coop_fail, mode_fail, surf_fail, restore_fail, zoomed, popup_self, cds, errmode, nchild, nthread; } fs;
struct fake_obj { void **vtbl; uint32_t id; };
static struct fake_obj fs_objs[16], fs_pre[6];
static uint32_t fs_nobj;
static void *fs_vtbl[34];
static ULONG WINAPI fo_release(struct fake_obj *o) { log_call(190, o->id, 0, 0, 0); return 0; }
static HRESULT WINAPI fo_restore(struct fake_obj *o) { uint32_t r = fs.restore_fail & 1; fs.restore_fail >>= 1; log_call(191, o->id, r, 0, 0); return r ? 0x887601c2 : 0; }
static HRESULT WINAPI fo_blt(struct fake_obj *o, RECT *r, void *src, RECT *sr, DWORD f, uint32_t *fx)
{ log_call(192, o->id, P(r), P(src), f); log_call(193, P(sr), fx ? fx[0] : 0, fx ? fx[0x50 / 4] : 0, 0); return 0; }
static HRESULT WINAPI fo_desc(struct fake_obj *o, uint32_t *d)
{ log_call(194, o->id, d[0], 0, 0); memcpy(d + 1, pal_desc + 1, sizeof pal_desc - 4); return 0; }
static HRESULT WINAPI fo_setpal(struct fake_obj *o, struct fake_obj *p) { log_call(195, o->id, p ? p->id : 0xff, 0, 0); return 0; }
static HRESULT WINAPI fo_setentries(struct fake_obj *o, DWORD f, DWORD a, DWORD n, void *e)
{ log_call(196, o->id, f, a, n); log_call(124, pal_sum(e, n * 4), 0, 0, 0); return 0; }
static struct fake_obj *fs_new(uint32_t kind)
{
    struct fake_obj *o = &fs_objs[fs_nobj < 16 ? fs_nobj : 15];
    o->vtbl = fs_vtbl; o->id = kind << 8 | fs_nobj++;
    return o;
}
static HRESULT WINAPI dd_createpal(struct fake_dd *o, DWORD f, void *e, void **out, void *u)
{ log_call(197, o->idx, f, P(u), 0); log_call(124, pal_sum(e, 0x400), 0, 0, 0); *out = fs_new(2); return 0; }
static HRESULT WINAPI dd_createsurf(struct fake_dd *o, uint32_t *d, void **out, void *u)
{
    uint32_t r = fs.surf_fail & 1;
    fs.surf_fail >>= 1;
    log_call(198, o->idx, d[1], d[0x68 / 4], d[0x14 / 4]); log_call(199, d[0], P(u), r, 0);
    *out = r ? 0 : fs_new(1);
    return r ? 0x80004005 : 0;
}
static HRESULT WINAPI dd_flipgdi(struct fake_dd *o) { log_call(200, o->idx, 0, 0, 0); return 0; }
static HRESULT WINAPI dd_coop(struct fake_dd *o, HWND h, DWORD f) { log_call(201, o->idx, P(h), f, 0); return fs.coop_fail ? 0x80004005 : 0; }
static HRESULT WINAPI dd_setmode(struct fake_dd *o, DWORD w, DWORD h, DWORD b)
{ uint32_t r = fs.mode_fail & 1; fs.mode_fail >>= 1; log_call(202, o->idx, w, h, b); return r ? 0x80004005 : 0; }

static BOOL WINAPI fake_IsZoomed(HWND h) { log_call(203, P(h), 0, 0, 0); return fs.zoomed; }
static BOOL WINAPI fake_GetWindowRect(HWND h, RECT *r)
{ log_call(204, P(h), 0, 0, 0); r->left = 10 + (int)(wl_seed & 0x3f); r->top = 20; r->right = 650 + (int)(wl_seed >> 26); r->bottom = 500; return TRUE; }
static LONG WINAPI fake_SetWindowLongA(HWND h, int i, LONG v) { log_call(205, P(h), (uint32_t)i, (uint32_t)v, 0); return 0; }
static int WINAPI fake_GetSystemMetrics(int i) { log_call(206, (uint32_t)i, 0, 0, 0); return 1000 + i * 7; }
static HWND WINAPI fake_GetLastActivePopup(HWND h) { log_call(207, P(h), 0, 0, 0); return fs.popup_self ? h : (HWND)0x4321; }
static UINT WINAPI fake_SetErrorMode(UINT m) { log_call(208, m, 0, 0, 0); return fs.errmode; }
static BOOL WINAPI fake_EnumChildWindows(HWND h, WNDENUMPROC cb, LPARAM l)
{
    uint32_t i;
    log_call(209, P(h), (uint32_t)l, 0, 0);
    for (i = 0; i < fs.nchild; i++) if (!cb((HWND)(uintptr_t)(0x9100 + i), l)) break;
    return TRUE;
}
static BOOL WINAPI fake_EnumThreadWindows(DWORD t, WNDENUMPROC cb, LPARAM l)
{
    uint32_t i;
    log_call(210, t == GetCurrentThreadId(), 0, 0, 0);
    for (i = 0; i < fs.nthread; i++) if (!cb((HWND)(uintptr_t)(0x9200 + i), l)) break;
    return TRUE;
}
static int WINAPI fake_GetClassNameA(HWND h, LPSTR buf, int n)
{
    log_call(211, P(h), (uint32_t)n, 0, 0);
    strncpy(buf, (const char *[]){ "FS98FLOAT", "fs98float", "FS98CHILD", "FS98FLOATX" }[P(h) % 4], (size_t)n);
    return (int)strlen(buf);
}
static LONG WINAPI fake_ChangeDisplaySettingsA(DEVMODEA *m, DWORD f)
{
    uint32_t r = fs.cds & 3;
    fs.cds >>= 2;
    if (m) log_call(212, m->dmSize, m->dmFields, m->dmPelsWidth, m->dmPelsHeight), log_call(213, m->dmBitsPerPel, f, 0, 0);
    else log_call(212, 0, f, 0, 0);
    return r == 3 ? -1 : (LONG)r;
}

static uint8_t disp_cfg[2][0xe8];
static uint32_t disp_list[2][80], disp_op;
static char disp_rcfg[MAX_PATH], disp_ocfg[MAX_PATH];
static uint32_t fs_script[10], fs_state[16];
static uint32_t do_31c(int s)
{
    if (trace) printf("display side %d\n", s);
    *(uint32_t *)(disp_cfg[s] + 0xe4) = P(disp_list[s]);
    if (disp_op >= 100) {           /* +0x048: the same script on each side */
        memcpy(&fs, fs_script, sizeof fs);
        fs_nobj = 0;
        return ((fn1)SIDE(s, 0x48))(disp_op - 100);
    }
    return ((fn3)SIDE(s, 0x31c))(disp_op, P(disp_cfg[s]), rnd());
}

static const char *const disp_keys[] = { "DirectDrawDisable", "Direct3D", "DirectDraw", "MMX", "Filter", "Dither",
                                         "MipMap", "PageFlip", "TextureSmall", "Texture8Bit", "VertexFog", "Fog" };
static const char *const disp_suffix[] = { "", " (Direct3D)", " (Direct3D MMX)", " (Direct3D RGB)", " (Direct3D RAMP)", "(Direct3D)" };

/* The fake DirectDraw, Direct3D, surface and palette vtables, made once. */
static void dd_fakes_init(void)
{
    static int init;
    uint32_t i;
    if (init) return;
    for (i = 0; i < 32; i++) dd_vtbl[i] = (void *)dd_other;
    for (i = 0; i < 8; i++) d3d_vtbl[i] = (void *)dd_other;
    dd_vtbl[0] = (void *)dd_qi; dd_vtbl[2] = (void *)dd_release; dd_vtbl[0x20 / 4] = (void *)dd_enummodes;
    dd_vtbl[0x2c / 4] = (void *)dd_getcaps; dd_vtbl[0x30 / 4] = (void *)dd_getmode;
    d3d_vtbl[2] = (void *)dd_release; d3d_vtbl[0xc / 4] = (void *)d3d_enumdev;
    dd_vtbl[0x14 / 4] = (void *)dd_createpal; dd_vtbl[0x18 / 4] = (void *)dd_createsurf; dd_vtbl[0x28 / 4] = (void *)dd_flipgdi;
    dd_vtbl[0x50 / 4] = (void *)dd_coop; dd_vtbl[0x54 / 4] = (void *)dd_setmode;
    for (i = 0; i < 34; i++) fs_vtbl[i] = (void *)dd_other;
    fs_vtbl[2] = (void *)fo_release; fs_vtbl[0x14 / 4] = (void *)fo_blt; fs_vtbl[0x18 / 4] = (void *)fo_setentries;
    fs_vtbl[0x58 / 4] = (void *)fo_desc; fs_vtbl[0x6c / 4] = (void *)fo_restore; fs_vtbl[0x7c / 4] = (void *)fo_setpal;
    for (i = 0; i < NDRV; i++) { dd_obj[i].vtbl = dd_vtbl; dd_obj[i].idx = i; d3d_obj[i].vtbl = d3d_vtbl; d3d_obj[i].idx = i; }
    init = 1;
}

static void check_display(void)
{
    static char text[2048];
    static uint32_t seq;
    uint32_t i, k;
    char *t = text;
    if (!our_fn(0x31c)) return;
    dd_fakes_init();
    /* the drivers */
    ndrv = 1 + rnd() % NDRV;
    for (i = 0; i < ndrv; i++) {
        struct drv *d = &drv[i];
        memset(d, 0, sizeof *d);
        d->has_guid = i > 0 || rnd() % 5 == 0;
        for (k = 0; k < 16; k++) drv_guid[i][k] = (uint8_t)(i * 16 + k);
        d->create_fail = rnd() % 6 == 0; d->no_d3d = rnd() % 5 == 0;
        sprintf(d->desc, "%s %u", (const char *[]){ "Primary Display", "3D Card", "Voodoo", "Display" }[rnd() % 4], i);
        sprintf(d->name, "%s", (const char *[]){ "display", "DISPLAY", "\\\\.\\DISPLAY2", "vdoo", "\\\\x" }[rnd() % 5]);
        d->ndev = rnd() % 4;
        for (k = 0; k < (uint32_t)d->ndev; k++) {
            uint32_t j;
            d->dev_guid[k] = (const uint8_t *[]){ guid_hal, guid_mmx, guid_rgb, 0 }[rnd() % 4];
            for (j = 0; j < 0x34; j++) { d->hal[k][j] = rnd() % 3 ? 0 : rnd(); d->hel[k][j] = rnd() % 3 ? 0 : rnd(); }
            if (rnd() & 1) d->hal[k][1] = 1 + rnd() % 8;
            if (rnd() & 1) { d->hel[k][1] = 1 + rnd() % 8; d->hel[k][2] |= rnd() & 2; }
            if (rnd() & 1) d->hal[k][0x84 / 4] |= 1;
        }
        d->nmode = rnd() % 9;
        for (k = 0; k < (uint32_t)d->nmode; k++) {
            d->mode[k][0] = (uint32_t[]){ 320, 512, 640, 800, 1024, 1280 }[rnd() % 6];
            d->mode[k][1] = (uint32_t[]){ 200, 240, 384, 480, 600, 768, 1024 }[rnd() % 7];
            d->mode[k][2] = (uint32_t[]){ 8, 16, 16, 24, 32 }[rnd() % 5];
        }
        d->vidmem = rnd() % 4 ? (rnd() & 1 ? 0x180000 : rnd() % 0x800000) : 0;   /* 1.5 MB: between 640x480's two 16-bit tests */
        d->cur[0] = 640 + rnd() % 400; d->cur[1] = 480 + rnd() % 300; d->cur[2] = (uint32_t[]){ 8, 8, 16, 24, 32 }[rnd() % 5];
    }
    /* the profile */
    seq++;
    if (rnd() % 6) {
        struct drv *d = &drv[rnd() % ndrv];
        t += sprintf(t, "[HARDWARE]\r\n");
        if (rnd() % 4) {
            k = rnd() % 6;
            if (k == 0) t += sprintf(t, "Device=%s%s\r\n", d->desc, disp_suffix[rnd() % 6]);
            else if (k == 1) t += sprintf(t, "Device=%s (%s)%s\r\n", d->desc, d->name, disp_suffix[rnd() % 6]);
            else if (k == 2) t += sprintf(t, "Device=%s%s\r\n", d->name, disp_suffix[rnd() % 6]);
            else if (k == 3) t += sprintf(t, "Device=%s\r\n", disp_suffix[1 + rnd() % 4] + 1);    /* a suffix alone */
            else if (k == 4) t += sprintf(t, "Device=%s with a description long enough to fill most of the buffer%s\r\n",
                                          d->desc, disp_suffix[rnd() % 6]);
            else t += sprintf(t, "Device=Nothing%s\r\n", disp_suffix[rnd() % 6]);
        }
        if (rnd() % 4)
            t += sprintf(t, "Mode=%s\r\n", (const char *[]){ "640x480x16", "800x600x8", "1024x768x16x75", "x", "640", "640x480",
                                                              "0x480x8", "320x200x8x", "1280x1024x32", "640000000000000000000000000000000000000000",
                                                              "640x48000000000000000000000000000000000000", "640x600x16",
                                                              "640x480x16000000000000000000000000000000" /* 39: fills the buffer */ }[rnd() % 13]);
        for (k = 0; k < 12; k++)
            if (rnd() % 3 == 0) t += sprintf(t, "%s=%d\r\n", disp_keys[k], (int)(rnd() % 3));
    }
    if (rnd() % 3 == 0) t += sprintf(t, "[OTHER]\r\nA=1\r\n");
    *t = 0;
    sprintf(disp_rcfg, "%sr\\D%07u.CFG", fs_base, seq); sprintf(disp_ocfg, "%so\\D%07u.CFG", fs_base, seq);
    write_file(disp_rcfg, text); write_file(disp_ocfg, text);
    strcpy((char *)(DATA_VA + CFGPATH_OFF), disp_rcfg); strcpy((char *)(our_data + CFGPATH_OFF), disp_ocfg);
    *(uint8_t *)(DATA_VA + 0x17f8) = *(uint8_t *)(our_data + 0x17f8) = 0;      /* 0x4187f8: the empty string */
    *(uint32_t *)(DATA_VA + 0x1834) = *(uint32_t *)(our_data + 0x1834) = rnd() % 4 ? P(fake_purge) : 0;   /* the busy cursor's hook */
    *(uint32_t *)(DATA_VA + 0x185c) = *(uint32_t *)(our_data + 0x185c) = rnd() % 3;
    /* the settings block: random, a device string from the drivers sometimes */
    for (i = 0; i < 0xe8; i++) disp_cfg[0][i] = (uint8_t)rnd();
    if (rnd() % 3) {
        struct drv *d = &drv[rnd() % ndrv];
        k = rnd() % 5;
        if (k == 0) sprintf((char *)disp_cfg[0], "%s%s", d->desc, disp_suffix[rnd() % 6]);
        else if (k == 1) sprintf((char *)disp_cfg[0], "%s (%s)%s", d->desc, d->name, disp_suffix[rnd() % 6]);
        else if (k == 2) sprintf((char *)disp_cfg[0], "%s%s", d->name, disp_suffix[rnd() % 6]);   /* found by name */
        else if (k == 3) strcpy((char *)disp_cfg[0], disp_suffix[1 + rnd() % 4] + 1);           /* a suffix alone */
        else disp_cfg[0][0] = 0;
    } else disp_cfg[0][0x7f] = 0;
    disp_cfg[0][0x80 + rnd() % 0x28] = 0; disp_cfg[0][0xa7] = 0; disp_cfg[0][0xa8 + rnd() % 0x28] = 0; disp_cfg[0][0xcf] = 0;
    if (rnd() & 1) disp_cfg[0][0x80] = 0;
    if (rnd() & 1) strcpy((char *)disp_cfg[0] + 0x80, disp_suffix[1 + rnd() % 5] + 1 + (rnd() & 0));
    *(uint32_t *)(disp_cfg[0] + 0xd0) = rnd() & 0x30003fff;
    *(uint32_t *)(disp_cfg[0] + 0xe0) = rnd() % 3 ? 0 : 0x100000 * (rnd() % 8);
    memcpy(disp_cfg[1], disp_cfg[0], 0xe8);
    for (i = 0; i < 80; i++) disp_list[0][i] = disp_list[1][i] = 0xeeee0000 + i;
    disp_op = rnd() % 16 ? 1 + rnd() % 6 : rnd() % 9;
    if (rnd() % 3 == 0 && !getenv("HOSTDIFF_NO_FS")) {           /* +0x048 instead, over a DirectDraw state */
        static const uint32_t gl[] = { 0x34e4, 0x34dc, 0x34f8, 0x34e0, 0x34fc, 0x34e8 };   /* 0x41a4e4 .. */
        uint8_t *sides[2] = { (uint8_t *)DATA_VA, our_data };
        uint32_t j, sd;
        disp_op = 100 + (rnd() & 1);
        for (j = 0; j < 10; j++) fs_script[j] = rnd();
        fs_script[0] = rnd() % 6 == 0; fs_script[1] = rnd() % 3 ? 0 : rnd(); fs_script[2] = rnd() % 4 ? 0 : rnd();
        fs_script[3] = rnd(); fs_script[4] = rnd() & 1; fs_script[5] = rnd() & 1; fs_script[6] = rnd();
        fs_script[8] = rnd() % 3; fs_script[9] = rnd() % 4;
        fs_nobj = 0;
        for (j = 0; j < 6; j++) {   /* objects already there: DirectDraw, Direct3D, surfaces, palette */
            fs_pre[j].vtbl = fs_vtbl; fs_pre[j].id = 0x300 | j;
            fs_state[j] = rnd() % 3 ? 0 : j == 0 ? P(&dd_obj[rnd() % ndrv]) : j == 1 ? P(&d3d_obj[0]) : P(&fs_pre[j]);
        }
        if (fs_state[0] == 0)       /* no DirectDraw, no surfaces (retail flips through it unchecked) */
            for (j = 1; j < 6; j++) fs_state[j] = 0;
        {
            uint32_t v[7];
            v[0] = rnd() % 8 == 0; v[1] = rnd() % 3; v[2] = rnd() % 4 != 0; v[3] = rnd() & 1; v[4] = rnd() % 8; v[5] = rnd() & 1;
            for (sd = 0; sd < 2; sd++) {
                for (j = 0; j < 6; j++) *(uint32_t *)(sides[sd] + gl[j]) = fs_state[j];
                *(uint32_t *)(sides[sd] + 0x1854) = disp_op == 100;       /* 0x418854: the opposite of the request, mostly */
                *(uint32_t *)(sides[sd] + 0x3500) = v[0];                 /* 0x41a500: being built */
                *(uint32_t *)(sides[sd] + 0x13b4) = v[1];                 /* 0x4183b4 */
                *(uint32_t *)(sides[sd] + 0x1858) = v[2];                 /* 0x418858 */
                *(uint32_t *)(sides[sd] + 0x34f4) = v[3];                 /* 0x41a4f4 */
                *(uint32_t *)(sides[sd] + 0x34ec) = v[4];                 /* 0x41a4ec */
                *(uint32_t *)(sides[sd] + 0x353c) = v[5];                 /* 0x41a53c */
                *(uint32_t *)(sides[sd] + 0x180c) = 0x5150;               /* the main window */
            }
        }
        if (rnd() % 5 == 0) *(uint32_t *)(sides[1] + 0x1854) = *(uint32_t *)(sides[0] + 0x1854) = disp_op == 101;
        is_window = rnd() % 5 != 0;
        pal_desc[0x54 / 4] = rnd() % 2 ? 8 : 16;
    }
    if (trace) printf("display op %u\n", disp_op);
    logged_pair(disp_op >= 100 ? "+0x048 full-screen" : "+0x31c display", do_31c);
    /* the blocks (the list cursor as an index), the lists' strings, the files */
    {
        uint32_t re = *(uint32_t *)(disp_cfg[0] + 0xe4), oe = *(uint32_t *)(disp_cfg[1] + 0xe4);
        if (re - P(disp_list[0]) < sizeof disp_list[0]) re -= P(disp_list[0]);          /* a cursor into the list: an index */
        if (oe - P(disp_list[1]) < sizeof disp_list[1]) oe -= P(disp_list[1]);
        if (re != oe || memcmp(disp_cfg[0], disp_cfg[1], 0xe4)) {
            printf("  +0x31c display(%u) BLOCKS DIFFER (list at %u/%u)\n", disp_op, re / 4, oe / 4); differ++;
            for (i = 0; i < 0xe4; i++) if (disp_cfg[0][i] != disp_cfg[1][i]) { printf("    +%#x: real %#x ours %#x\n", i, disp_cfg[0][i], disp_cfg[1][i]); break; }
        }
        for (i = 0; i < 80; i++) {
            uint32_t r = disp_list[0][i], o = disp_list[1][i];
            int rs = r >= 0x10000 && (r >> 16) != 0xeeee, os = o >= 0x10000 && (o >> 16) != 0xeeee;
            if (rs != os || (!rs && r != o) || (rs && strcmp((char *)(uintptr_t)r, (char *)(uintptr_t)o))) {
                printf("  +0x31c display(%u) LIST DIFFERS at %u: \"%s\" / \"%s\"\n", disp_op, i, rs ? (char *)(uintptr_t)r : "-", os ? (char *)(uintptr_t)o : "-");
                differ++; break;
            }
            if (rs) { free((void *)(uintptr_t)r); free((void *)(uintptr_t)o); }
        }
    }
    if (files_differ(disp_rcfg, disp_ocfg)) { printf("  +0x31c display(%u) FILES DIFFER\n", disp_op); differ++; }
    DeleteFileA(disp_rcfg); DeleteFileA(disp_ocfg);
}

/* ---- dialogs (+0x10c..+0x118) ------------------------------------------------ */

/* The registry and module records seeded alike; factories that log and make
 * scripted windows (0x9300+k, alive and busy for scripted numbers of checks,
 * so every loop ends); a scripted message queue; the modeless list and the
 * modal stack seeded per side (their record pointers into each side's
 * registry, compared as offsets). */
static uint32_t dlg_arr[5] = { 0x9301, 0x9302, 0x9303, 0, 0 }, dlg_arr_below[3] = { 0x9307, 0x9308, 0 };
static uint32_t dlg_make[3], dlg_peek, dlg_enabled, dlg_param[4];
static uint32_t WINAPI dlg_f0(uint32_t parent, uint32_t param) { log_call(220, 0, parent, P(param) == P(dlg_param), 0); return dlg_make[0] ? 0x9300 : 0; }
static uint32_t WINAPI dlg_f1(uint32_t parent, uint32_t param) { log_call(220, 1, parent, P(param) == P(dlg_param), 0); return dlg_make[1] ? P(dlg_arr) : P(&dlg_arr[3]); }
static uint32_t WINAPI dlg_f2(uint32_t parent, uint32_t param) { log_call(220, 2, parent, P(param) == P(dlg_param), 0); return dlg_make[2] ? 0x9304 : 0; }
static uint32_t dlg_table[3 * 6];
static BOOL WINAPI fake_PeekMessageA(MSG *m, HWND h, UINT a, UINT b, UINT f)
{
    log_call(221, P(h), a, b, f);
    if (!dlg_peek) return FALSE;
    dlg_peek--;
    memset(m, 0, sizeof *m);
    m->hwnd = (HWND)(uintptr_t)(0x9300 + dlg_peek % 9); m->message = 0x100 + dlg_peek * 7; m->wParam = dlg_peek;
    return TRUE;
}
static BOOL WINAPI fake_IsDialogMessageA(HWND h, MSG *m) { log_call(222, P(h), m->message, P(m->hwnd), 0); return (m->message + P(h)) % 3 == 0; }
static int WINAPI fake_TranslateAcceleratorA(HWND h, HACCEL a, MSG *m) { log_call(223, P(h), P(a), m->message, 0); return (m->message ^ P(h)) % 5 == 0; }
static BOOL WINAPI fake_TranslateMessage(const MSG *m) { log_call(224, m->message, 0, 0, 0); return TRUE; }
static LRESULT WINAPI fake_DispatchMessageA(const MSG *m) { log_call(225, m->message, P(m->hwnd), 0, 0); return 0; }
static BOOL WINAPI fake_IsWindowEnabled(HWND h) { log_call(226, P(h), 0, 0, 0); return dlg_enabled; }
static BOOL WINAPI fake_IsWindowVisible(HWND h) { log_call(227, P(h), 0, 0, 0); return (P(h) & 1) != 0; }
static HACCEL (WINAPI *real_LoadAcceleratorsA)(HINSTANCE, LPCSTR);
static HACCEL WINAPI fake_LoadAcceleratorsA(HINSTANCE i, LPCSTR n)
{
    if (!real_LoadAcceleratorsA) real_LoadAcceleratorsA = (HACCEL (WINAPI *)(HINSTANCE, LPCSTR))(void *)GetProcAddress(GetModuleHandleA("user32.dll"), "LoadAcceleratorsA");
    if (P(n) == 0x69) { log_call(228, P(i), 0x69, 0, 0); return (HACCEL)0xacc0; }
    return real_LoadAcceleratorsA(i, n);
}

static uint32_t dlg_slot, dlg_id, dlg_alive0[16], dlg_busy0[16], dlg_peek0, dlg_stack_n, dlg_nmodeless;
static uint32_t dlg_stack_rec[3], dlg_stack_win[3];
static const char *const dlg_modnames[] = { "MA.DLL", "MB.DLL", "MC.DLL", "MD.DLL" };
static uint32_t do_dialog(int s)
{
    uint32_t i;
    uint8_t *d = s ? our_data : (uint8_t *)DATA_VA;
    uint32_t reg = *(uint32_t *)(d + 0xee0 + 0x18);
    modules_reset();
    for (i = 0; i < 4; i++) fake_link[i][5] = P(dlg_table);
    memcpy(dlg_alive, dlg_alive0, sizeof dlg_alive); memcpy(dlg_busy, dlg_busy0, sizeof dlg_busy); dlg_peek = dlg_peek0;
    for (i = 0; i < dlg_stack_n; i++) {      /* the modal stack below: records into this side's registry */
        *(uint32_t *)(d + 0x4a60 + 8 * i) = reg + 0x10 * dlg_stack_rec[i];
        *(uint32_t *)(d + 0x4a64 + 8 * i) = dlg_stack_win[i];
    }
    switch (dlg_slot) {
    case 0: return ((fn2)SIDE(s, 0x10c))(dlg_id, P(dlg_param));
    case 1: return ((fn2)SIDE(s, 0x110))(dlg_id, P(dlg_param));
    case 2: return ((fn2)SIDE(s, 0x118))(dlg_id, P(dlg_param));
    default: return ((fn0)SIDE(s, 0x114))();
    }
}

static void check_dialogs(void)
{
    static const uint32_t flags[6] = { 0, 2, 4, 8, 0xc, 6 };
    static const char *const names[] = { "+0x10c dialog", "+0x110 dialog", "+0x118 dialog", "+0x114 dialog-tick" };
    uint32_t mods[4 * 5], regs[6 * 4], i, nreg, s;
    if (!our_fn(0x10c) || !our_fn(0x114) || getenv("HOSTDIFF_NO_DIALOGS")) return;
    dd_fakes_init();
    /* the dialog table every fake module has: f0 single, f1 an array, f2 single */
    for (i = 0; i < 6; i++) {
        dlg_table[3 * i] = 0x51 + i;
        static void *const facts[6] = { (void *)dlg_f0, (void *)dlg_f1, (void *)dlg_f2, (void *)dlg_f0, (void *)dlg_f2, (void *)dlg_f2 };
        dlg_table[3 * i + 1] = P(facts[i]);
        dlg_table[3 * i + 2] = flags[i];
    }
    dlg_table[3 * 5] = 0;
    /* module records: some loaded already */
    for (i = 0; i < 4; i++) {
        uint32_t loaded = rnd() % 3 == 0;
        mods[5 * i] = P(dlg_modnames[i]); mods[5 * i + 1] = 0x40 + i;
        mods[5 * i + 2] = loaded ? 0x6000 + 0x10 * i : 0; mods[5 * i + 3] = loaded ? P(fake_link[i]) : 0; mods[5 * i + 4] = loaded ? 1 + rnd() % 2 : 0;
    }
    modules_reset();
    seed_list(MODS_OFF, 4, 0x14, mods);
    /* the registry: { id, module, index, flags }, the flag-2 one only on the array factory */
    nreg = 6;
    for (i = 0; i < nreg; i++) {
        uint32_t idx = i < 5 ? i : rnd() % 5;
        regs[4 * i] = 0x51 + i; regs[4 * i + 1] = 0x40 + rnd() % 4; regs[4 * i + 2] = idx;
        regs[4 * i + 3] = i == 1 ? 2 | (rnd() % 3 == 0 ? 8 : 0) : (uint32_t[]){ 0, 4, 8, 0xc, 4 }[rnd() % 5];
        if (idx == 1) regs[4 * i + 3] |= 2;
        else regs[4 * i + 3] &= ~2u;
    }
    seed_list(DIALOGS_OFF, nreg, 0x10, regs);
    seed_list(QUEUE_OFF, 0, 8, 0);      /* the removal queue, empty (a closed modeless dialog is removed mid-walk) */
    pair_base[0] = *(uint32_t *)(DATA_VA + 0xee0 + 0x18); pair_base[1] = *(uint32_t *)(our_data + 0xee0 + 0x18); pair_size = nreg * 0x10;
    /* the windows' scripts, the queue */
    for (i = 0; i < 16; i++) { dlg_alive0[i] = rnd() % 4 ? rnd() % 12 : 0; dlg_busy0[i] = rnd() % 10; }
    dlg_peek0 = rnd() % 4 ? rnd() % 9 : 0;
    for (i = 0; i < 3; i++) dlg_make[i] = rnd() % 6 != 0;
    for (i = 0; i < 4; i++) dlg_param[i] = rnd();
    dlg_enabled = rnd() % 4 != 0;
    /* the modal stack below, the modeless list */
    dlg_stack_n = rnd() % 3;
    for (i = 0; i < dlg_stack_n; i++) {
        dlg_stack_rec[i] = rnd() % nreg;
        dlg_stack_win[i] = regs[4 * dlg_stack_rec[i] + 3] & 2 ? P(dlg_arr_below) : 0x9305 + i;
    }
    dlg_nmodeless = rnd() % 3;
    for (s = 0; s < 2; s++) {
        uint32_t h[7] = { dlg_nmodeless, dlg_nmodeless, 0x20, 8, 0, 0, 0 }, *e = malloc(8 * 8);
        uint8_t *d = s ? our_data : (uint8_t *)DATA_VA;
        for (i = 0; i < dlg_nmodeless; i++) { e[2 * i] = *(uint32_t *)(d + 0xee0 + 0x18) + 0x10 * (2 + i); e[2 * i + 1] = 0x9309 + i; }
        h[6] = P(e); memcpy(d + 0xf00, h, sizeof h);
        *(uint32_t *)(d + 0x18a8) = dlg_stack_n;                      /* 0x4188a8 */
        *(uint32_t *)(d + 0x4a28) = 0; *(uint32_t *)(d + 0x4608) = 0;  /* 0x41ba28, 0x41b608 */
        *(uint32_t *)(d + 0x180c) = 0x5150;
    }
    *(uint32_t *)(our_data + 0x1834) = *(uint32_t *)(DATA_VA + 0x1834) = rnd() % 4 ? P(fake_purge) : 0;   /* the event hook */
    *(uint32_t *)(DATA_VA + 0x34e4) = *(uint32_t *)(our_data + 0x34e4) =
        rnd() % 3 || !dd_obj[0].vtbl ? 0 : P(&dd_obj[0]);   /* 0x41a4e4 (the fake is made by the display check, which a file-less run skips) */
    *(uint32_t *)(DATA_VA + 0x1848) = *(uint32_t *)(our_data + 0x1848) = rnd() % 3 ? 0 : 3;               /* 0x418848 */
    pal_caps = rnd() % 3 ? 0x100 : rnd() & ~0x100u;
    dlg_slot = rnd() % 5; if (dlg_slot > 3) dlg_slot = 3;
    dlg_id = rnd() % 8 ? 0x51 + rnd() % nreg : 0x99;
    logged_pair(names[dlg_slot], do_dialog);
    /* the modeless lists, the dither tables, the module records */
    {
        uint32_t rn = *(uint32_t *)(DATA_VA + 0xf00), on = *(uint32_t *)(our_data + 0xf00);
        uint32_t *re = *(uint32_t **)(DATA_VA + 0xf18), *oe = *(uint32_t **)(our_data + 0xf18);
        uint8_t *rt = *(uint8_t **)(DATA_VA + 0x4a28), *ot = *(uint8_t **)(our_data + 0x4a28);
        if (rn != on) { printf("  %-16s MODELESS COUNT DIFFERS %u/%u\n", names[dlg_slot], rn, on); differ++; }
        else for (i = 0; i < rn; i++)
            if (norm(re[2 * i]) != norm(oe[2 * i]) || re[2 * i + 1] != oe[2 * i + 1]) { printf("  %-16s MODELESS DIFFER at %u\n", names[dlg_slot], i); differ++; break; }
        if (!rt != !ot || (rt && memcmp(rt, ot, 0x4000))) { printf("  %-16s DITHER TABLES DIFFER\n", names[dlg_slot]); differ++; }
        free(rt); free(ot);
        *(uint32_t *)(DATA_VA + 0x4a28) = *(uint32_t *)(our_data + 0x4a28) = 0;
        if (modules_differ()) { printf("  %-16s MODULES DIFFER\n", names[dlg_slot]); differ++; }
    }
    for (i = 0; i < 8; i++)     /* the modal stack's leftovers point into each side's registry */
        *(uint32_t *)(DATA_VA + 0x4a60 + 4 * i) = *(uint32_t *)(our_data + 0x4a60 + 4 * i) = 0;
    pair_size = 0;
}

/* ---- tooltip regions --------------------------------------------------------- */

static uint32_t tt_rec[2][0x2a], tt_block[2][0x24];
static uint32_t tt_tree[2][3][11 * 16];
static uint32_t tt_ntrees, tt_slot, tt_t, tt_x, tt_y;

/* A random tree into both sides' copies: nodes, groups (type 1 ... type 3),
 * ending at type 0; sizes that sometimes overflow their group. */
static uint32_t tt_gen(uint32_t *a, uint32_t n, uint32_t room, int depth)
{
    uint32_t used = 0, k = 1 + rnd() % 3, i;
    for (i = 0; i < k && used + 2 < room; i++) {
        uint32_t *nd = a + 11 * used, j;
        for (j = 0; j < 11; j++) nd[j] = 0;
        nd[1] = rnd() % 40; nd[2] = rnd() % 40; nd[3] = rnd() % 60; nd[4] = rnd() % 60;
        nd[6] = rnd() % 3 ? (0x100 + rnd() % 8) | (rnd() & 1 ? 0x30000 : 0) : 0;   /* a word, high bits besides */
        /* the top level is groups: retail's check reads each top-level node
         * as one, scanning for its closing 3 past the list's end otherwise */
        nd[0] = depth == 0 ? 1 : depth < 2 && rnd() % 3 == 0 ? 1 : 2;
        used++;
        if (nd[0] == 1) {
            used += tt_gen(a + 11 * used, n, room - used - 1, depth + 1);
            for (j = 0; j < 11; j++) a[11 * used + j] = 0;
            a[11 * used] = 3;
            used++;
        }
    }
    (void)n;
    return used;
}

static uint32_t do_tips(int s)
{
    uint32_t rec = P(tt_rec[s]), tree = P(tt_tree[s][tt_t]);
    if (trace) printf("tips slot %u side %d tree %u\n", tt_slot, s, tt_t);
    cw_seq = 0; cw_fail = 0;
    switch (tt_slot) {
    case 0: return ((fn4)SIDE(s, 0x27c))(rec, tree, tt_x, tt_y);
    case 1: return ((fn2)SIDE(s, 0x280))(rec, tree);
    default: return ((fn2)SIDE(s, 0x284))(rec, tree);
    }
}

static int tips_differ(void)
{
    uint32_t t, k;
    if (tt_block[0][0x2c / 4] != tt_block[1][0x2c / 4]) return 1;
    if (tt_block[0][0x70 / 4] != tt_block[1][0x70 / 4]) return 1;
    {   /* the registered trees' entries: { rec +0x38, +0x3c, bounds..., tree } */
        uint32_t n = tt_block[0][0x70 / 4], *a = (uint32_t *)(uintptr_t)tt_block[0][(0x70 + 0x18) / 4],
                 *b = (uint32_t *)(uintptr_t)tt_block[1][(0x70 + 0x18) / 4], j;
        for (j = 0; j < n && j < 8; j++) {
            if (memcmp(a + 5 * j, b + 5 * j, 16)) return 1;
            if (a[5 * j + 4] - P(tt_tree[0]) != b[5 * j + 4] - P(tt_tree[1])) return 1;
        }
    }
    for (t = 0; t < tt_ntrees; t++)
        for (k = 0; k < 11 * 16; k++) {
            uint32_t a = tt_tree[0][t][k], b = tt_tree[1][t][k];
            if (k % 11 == 10) {                   /* the rectangle: by contents */
                if (!a != !b) return 1;
                if (a && memcmp((void *)(uintptr_t)a, (void *)(uintptr_t)b, 0x1c)) return 1;
            } else if (a != b) return 1;
        }
    return 0;
}

static void check_tips(void)
{
    static const char *const names[] = { "+0x27c tips-add", "+0x280 tips-move", "+0x284 tips-drop" };
    uint32_t i, k, ops = 1 + rnd() % 6, reg[3] = { 0, 0, 0 };
    if (!our_fn(0x27c)) return;
    for (i = 0; i < 0x2a; i++) tt_rec[0][i] = rnd();
    for (i = 0; i < 0x24; i++) tt_block[0][i] = rnd();
    tt_block[0][0x2c / 4] = rnd() % 3 ? 0 : 0x7900;
    tt_block[0][0x44 / 4] = 0x8000 + rnd() % 0x10000; tt_block[0][0x48 / 4] = 0x8000 + rnd() % 0x10000;
    tt_block[0][0x70 / 4] = 0; tt_block[0][0x74 / 4] = 0; tt_block[0][0x78 / 4] = 0x20; tt_block[0][0x7c / 4] = 0x14;
    tt_block[0][0x80 / 4] = 0; tt_block[0][0x84 / 4] = 0; tt_block[0][0x88 / 4] = 0;
    memcpy(tt_block[1], tt_block[0], sizeof tt_block[0]);
    memcpy(tt_rec[1], tt_rec[0], sizeof tt_rec[0]);
    tt_rec[0][0xa0 / 4] = P(tt_block[0]); tt_rec[1][0xa0 / 4] = P(tt_block[1]);
    tt_ntrees = 1 + rnd() % 3;
    for (i = 0; i < tt_ntrees; i++) {
        uint32_t used = tt_gen(tt_tree[0][i], 0, 14, 0);
        memset(tt_tree[0][i] + 11 * used, 0, 11 * 4);             /* type 0: the end */
        memcpy(tt_tree[1][i], tt_tree[0][i], sizeof tt_tree[0][i]);
    }
    for (k = 0; k < ops && !differ; k++) {
        tt_t = rnd() % tt_ntrees;
        tt_slot = reg[tt_t] ? 1 + rnd() % 2 : 0;
        if (tt_slot == 0 && rnd() % 6 == 0 && k) tt_t = (tt_t + 1) % tt_ntrees;   /* now and then a second registration */
        if (tt_slot == 0 && reg[tt_t] && rnd() % 4) tt_slot = 2;   /* else a second registration: refused */
        tt_x = rnd() % 100; tt_y = rnd() % 100;
        logged_pair(names[tt_slot], do_tips);
        if (tt_slot == 0) reg[tt_t] = 1;
        if (tt_slot == 2) reg[tt_t] = 0;
        if (tips_differ()) { printf("  %-16s TREES DIFFER\n", names[tt_slot]); differ++; }
    }
    for (k = 0; k < tt_ntrees; k++)                               /* leave nothing registered */
        if (reg[k]) { tt_t = k; tt_slot = 2; logged_pair("+0x284 tips-drop", do_tips); }
}

static int heapchk;
#define HEAPCHK(name) do { if (heapchk && _heapchk() != _HEAPOK) { printf("hostdiff: heap damaged after %s\n", name); exit(6); } } while (0)

int main(int argc, char **argv)
{
    uint32_t n = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 1, k, i;

    setvbuf(stdout, 0, _IONBF, 0);     /* so a crash still shows what came before it */
    SetUnhandledExceptionFilter(on_fault);
    trace = getenv("HOSTDIFF_TRACE") != 0;
    heapchk = getenv("HOSTDIFF_HEAPCHK") != 0;
    skip_files = getenv("HOSTDIFF_SKIP_FILES") != 0;
    map_real();
    load_ours();
    load_strmods();
    fs_init();
    com_init();
    surface_init();
    printf("hostdiff: %u accessors + writers + strings + primitives + stubs + allocator + lists + command line + extensions + CRT + string resources + names + images + accelerators + key names + profiles + files + find + messages + joysticks + menus + modules + sound + views + palette + DirectPlay + display settings + dialogs + tooltips, %u iteration(s)\n",
           (unsigned)(sizeof accessors / sizeof accessors[0]), n);
    for (k = 0; k < n; k++) {
        if (trace) printf("iteration %u\n", k);
        seed_data();
        for (i = 0; i < sizeof accessors / sizeof accessors[0]; i++)
            check_accessor(&accessors[i]);
        HEAPCHK("check_accessor");
        check_018(); HEAPCHK("check_018()");
        check_02c(); HEAPCHK("check_02c()");
        check_054(); HEAPCHK("check_054()");
        check_2b8(); HEAPCHK("check_2b8()");
        check_2a8(); HEAPCHK("check_2a8()");
        check_2b0(); HEAPCHK("check_2b0()");
        check_288(); HEAPCHK("check_288()");
        check_304(); HEAPCHK("check_304()");
        check_130(); HEAPCHK("check_130()");
        check_168(); HEAPCHK("check_168()");
        check_218(); HEAPCHK("check_218()");
        check_228(); HEAPCHK("check_228()");
        check_2bc(); HEAPCHK("check_2bc()");
        check_28c(); HEAPCHK("check_28c()");
        check_stubs(); HEAPCHK("check_stubs()");
        check_alloc(); HEAPCHK("check_alloc()");
        check_21c(); HEAPCHK("check_21c()");
        check_lists(); HEAPCHK("check_lists()");
        check_find_indirect(); HEAPCHK("check_find_indirect()");
        check_cmdline(); HEAPCHK("check_cmdline()");
        check_1ac(); HEAPCHK("check_1ac()");
        check_188(); HEAPCHK("check_188()");
        check_sort(); HEAPCHK("check_sort()");
        check_crt(); HEAPCHK("check_crt()");
        check_strings(); HEAPCHK("check_strings()");
        check_strmods(); HEAPCHK("check_strmods()");
        check_inputs(); HEAPCHK("check_inputs()");
        check_names(); HEAPCHK("check_names()");
        check_184(); HEAPCHK("check_184()");
        check_images(); HEAPCHK("check_images()");
        check_accels(); HEAPCHK("check_accels()");
        check_keynames(); HEAPCHK("check_keynames()");
        if (!skip_files) {          /* the slow ones: real files */
            seed_profile();
            check_profiles(); HEAPCHK("check_profiles()");
            seed_files();
            check_files(); HEAPCHK("check_files()");
            check_find(); HEAPCHK("check_find()");
            check_fatal_files(); HEAPCHK("check_fatal_files()");
            check_display(); HEAPCHK("check_display()");
        }
        check_messages(); HEAPCHK("check_messages()");
        check_dirty(); HEAPCHK("check_dirty()");
        check_joysticks(); HEAPCHK("check_joysticks()");
        check_abbrev(); HEAPCHK("check_abbrev()");
        check_misc(); HEAPCHK("check_misc()");
        check_keypad(); HEAPCHK("check_keypad()");
        check_fullpath(); HEAPCHK("check_fullpath()");
        check_menus(); HEAPCHK("check_menus()");
        check_modules(); HEAPCHK("check_modules()");
        check_busy(); HEAPCHK("check_busy()");
        check_sound(); HEAPCHK("check_sound()");
        check_views(); HEAPCHK("check_views()");
        check_library_free(); HEAPCHK("check_library_free()");
        check_palette(); HEAPCHK("check_palette()");
        check_dplay(); HEAPCHK("check_dplay()");
        check_dialogs(); HEAPCHK("check_dialogs()");
        check_tips(); HEAPCHK("check_tips()");
    }
    printf("hostdiff: %d differ\n", differ);
    return differ ? 1 : 0;
}
