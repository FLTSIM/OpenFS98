/* UTIL.DLL -- module id 20, assembled from its reimplemented services.
 *
 * The services themselves live in util_math.c, util_files.c, util_exchange.c
 * and util_alert.c, each verified against retail one at a time (utildiff,
 * ofstest).  This file is only the module: the ImportTable the host binds,
 * and the Linkage table the host and every importing module call through.
 *
 * The table is laid out exactly as retail's -- 47 services in slots +0x04 to
 * +0xc8, with +0x0c, +0x10 and +0x14 null -- because the slot offsets ARE the
 * interface: importers call `[link + offset]` and nothing else.  +0x14 being
 * null matters too: it is where the host looks for a module's dialog table,
 * and UTIL publishes none.
 *
 * Built with FS98_UTIL_MODULE, so the services read their imports straight
 * out of ImportTable on every call, as retail does (see util_imports.h).
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

/* The order is retail's: host, GLOBAL, NUMERIC.  util_imports.h indexes it. */
struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,    0 },
    { FS98_MOD_GLOBAL,  0 },
    { FS98_MOD_NUMERIC, 0 },
    { 0, 0 }
};

typedef int32_t (FS98_STDCALL *fs98_util_entry_cb)(const char *, char *);

int32_t FS98_STDCALL fs98_util_sqrt32(uint32_t);
int32_t FS98_STDCALL fs98_util_sqrt64(const void *);
int32_t FS98_STDCALL fs98_util_hypot(int16_t, int16_t);
void FS98_STDCALL fs98_util_sincos(int16_t, int16_t *, int16_t *);
void FS98_STDCALL fs98_util_metres_to_longitude(const void *, const void *, void *);
int32_t FS98_STDCALL fs98_util_stub_50(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_54(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_58(int32_t, int32_t);
void FS98_STDCALL fs98_util_stub_5c(void);
int32_t FS98_STDCALL fs98_util_sqrt16(uint16_t);
uint32_t FS98_STDCALL fs98_util_rand(void);
uint32_t FS98_STDCALL fs98_util_rand32(void);
void FS98_STDCALL fs98_util_copy24(void *, const void *);
int32_t FS98_STDCALL fs98_util_double_to_int(double);
void FS98_STDCALL fs98_util_v48_add(void *, const void *, const void *);
void FS98_STDCALL fs98_util_v48_sub(void *, const void *, const void *);
int32_t FS98_STDCALL fs98_util_v48_equal(const void *, const void *);
void FS98_STDCALL fs98_util_shr48(void *, const void *, uint32_t);
int32_t FS98_STDCALL fs98_util_double_to_int64_low(double);
void FS98_STDCALL fs98_util_rotate2d(int32_t, int32_t, int16_t, int16_t, int32_t *, int32_t *);
void FS98_STDCALL fs98_util_div48_u16(const void *, uint16_t, void *);
void FS98_STDCALL fs98_util_metres32_to_longitude(const int32_t *, const void *, void *);
void FS98_STDCALL fs98_util_deinit(void);
void FS98_STDCALL fs98_util_mul48_s16(const void *, int16_t, void *);
void FS98_STDCALL fs98_util_unpack_position(const void *, void *);
int32_t FS98_STDCALL fs98_util_atan2(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_angle_from_sin(int16_t, int16_t);
void FS98_STDCALL fs98_util_fixed48_to_double(double *, const void *);
void FS98_STDCALL fs98_util_double_to_fixed48(const double *, void *);
double FS98_STDCALL fs98_util_frand(void);
const char * FS98_STDCALL fs98_util_longer_string(const char *, const char *);
void FS98_STDCALL fs98_util_init(void);
void FS98_STDCALL fs98_util_double_to_fixed48_signed(const double *, void *);
void FS98_STDCALL fs98_util_fixed48_to_double_signed(double *, const void *);
void FS98_STDCALL fs98_util_longitude_to_metres(const void *, const void *, void *);
void FS98_STDCALL fs98_util_range_and_bearing(const int32_t *, const int32_t *, int32_t, int32_t *, int16_t *);
void FS98_STDCALL fs98_util_rotate3d(const int16_t *, const int16_t *, int32_t *);
void FS98_STDCALL fs98_util_range_and_bearing48(const void *, const void *, int32_t, int32_t *, int16_t *, uint8_t *);
void FS98_STDCALL fs98_util_marshal(const void *, const void *, int32_t);
int32_t FS98_STDCALL fs98_util_exchange_put(void *, uint32_t, void *, int32_t);
void FS98_STDCALL fs98_util_exchange_get(void *, uint32_t, void *);
int32_t FS98_STDCALL fs98_util_list_tree(void **, int32_t *, const char *, const char *, int32_t);
int32_t FS98_STDCALL fs98_util_catalogue(uint32_t, uint32_t, int32_t, int32_t *, void **, fs98_util_entry_cb, fs98_util_entry_cb, fs98_util_entry_cb, int32_t);
int32_t FS98_STDCALL fs98_util_is_dir(const char *);
int32_t FS98_STDCALL fs98_util_alert_modal(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
void FS98_STDCALL fs98_util_alert_open(void **, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
void FS98_STDCALL fs98_util_alert_close(void **);

/* slot at byte offset `off` from the start of Linkage */
#define SLOT(off, fn) [(off) / 4 - 1] = (void *)(fn)

struct {
    uint32_t id;
    void    *slot[50];          /* +0x04 .. +0xc8 */
} Linkage = { FS98_MOD_UTIL, {
    SLOT(0x04, fs98_util_init),
    SLOT(0x08, fs98_util_deinit),
    /* +0x0c, +0x10 null; +0x14 null: no dialog table */
    SLOT(0x18, fs98_util_sqrt64),
    SLOT(0x1c, fs98_util_sqrt32),
    SLOT(0x20, fs98_util_sqrt16),
    SLOT(0x24, fs98_util_mul48_s16),
    SLOT(0x28, fs98_util_hypot),
    SLOT(0x2c, fs98_util_sincos),
    SLOT(0x30, fs98_util_div48_u16),
    SLOT(0x34, fs98_util_rotate2d),
    SLOT(0x38, fs98_util_angle_from_sin),
    SLOT(0x3c, fs98_util_atan2),
    SLOT(0x40, fs98_util_metres32_to_longitude),
    SLOT(0x44, fs98_util_metres_to_longitude),
    SLOT(0x48, fs98_util_longitude_to_metres),
    SLOT(0x4c, fs98_util_rotate3d),
    SLOT(0x50, fs98_util_stub_50),
    SLOT(0x54, fs98_util_stub_54),
    SLOT(0x58, fs98_util_stub_58),
    SLOT(0x5c, fs98_util_stub_5c),
    SLOT(0x60, fs98_util_unpack_position),
    SLOT(0x64, fs98_util_range_and_bearing48),
    SLOT(0x68, fs98_util_range_and_bearing),
    SLOT(0x6c, fs98_util_rand),
    SLOT(0x70, fs98_util_rand32),
    SLOT(0x74, fs98_util_v48_add),
    SLOT(0x78, fs98_util_v48_sub),
    SLOT(0x7c, fs98_util_copy24),
    SLOT(0x80, fs98_util_v48_equal),
    SLOT(0x84, fs98_util_shr48),
    SLOT(0x88, fs98_util_marshal),
    SLOT(0x8c, fs98_util_exchange_put),
    SLOT(0x90, fs98_util_exchange_get),
    SLOT(0x94, fs98_util_list_tree),
    SLOT(0x98, fs98_util_catalogue),
    SLOT(0x9c, fs98_util_longer_string),
    SLOT(0xa0, fs98_util_is_dir),
    SLOT(0xa4, fs98_util_alert_modal),
    SLOT(0xa8, fs98_util_alert_open),
    SLOT(0xac, fs98_util_alert_close),
    SLOT(0xb0, fs98_util_double_to_int),
    SLOT(0xb4, fs98_util_double_to_int64_low),
    SLOT(0xb8, fs98_util_frand),
    SLOT(0xbc, fs98_util_double_to_fixed48),
    SLOT(0xc0, fs98_util_fixed48_to_double),
    SLOT(0xc4, fs98_util_double_to_fixed48_signed),
    SLOT(0xc8, fs98_util_fixed48_to_double_signed),
} };

/* ---- FS98_UTIL_TRACE: which services does the game actually call? --------
 *
 * Built with -DFS98_UTIL_TRACE (`make util-trace`), every slot is replaced in
 * DllMain -- before the host reads Linkage -- by a two-instruction thunk:
 *
 *     incl  calls[i]
 *     jmp   *target[i]
 *
 * which leaves the stack and every register as the caller set them, so it is
 * transparent to any calling convention and needs no prototype.  A thread
 * rewrites the counts to C:\openfs98-util-trace.log every two seconds, so a
 * run that is killed rather than exited still leaves them behind.
 */
#ifdef FS98_UTIL_TRACE
#define NSLOT 50
volatile uint32_t fs98_util_calls[NSLOT];
void *fs98_util_target[NSLOT];

#define THUNK(i) __asm__(".text\n.p2align 2\n_fs98_util_thunk" #i ":\n" \
                         "\tincl _fs98_util_calls+" #i "*4\n" \
                         "\tjmp *_fs98_util_target+" #i "*4\n");
#define THUNK10(t) THUNK(t##0) THUNK(t##1) THUNK(t##2) THUNK(t##3) THUNK(t##4) \
                   THUNK(t##5) THUNK(t##6) THUNK(t##7) THUNK(t##8) THUNK(t##9)
THUNK(0) THUNK(1) THUNK(2) THUNK(3) THUNK(4) THUNK(5) THUNK(6) THUNK(7) THUNK(8) THUNK(9)
THUNK10(1) THUNK10(2) THUNK10(3) THUNK10(4)

extern char fs98_util_thunk0[];
/* each thunk is 6 + 6 bytes, padded to 4: 12 apart */
#define THUNK_ADDR(i) (fs98_util_thunk0 + 12 * (i))

static void trace_write(void)
{
    static const char hex[] = "0123456789abcdef";
    char line[48];
    DWORD wrote;
    unsigned i, used = 0;
    HANDLE f = CreateFileA("C:\\openfs98-util-trace.log", GENERIC_WRITE, FILE_SHARE_READ,
                           0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE)
        return;
    for (i = 0; i < NSLOT; i++) {
        uint32_t n = fs98_util_calls[i], off = (i + 1) * 4;
        char *p = line;
        int k;
        if (!fs98_util_target[i])
            continue;
        used += n != 0;
        *p++ = '+'; *p++ = '0'; *p++ = 'x';
        *p++ = hex[off >> 4 & 15]; *p++ = hex[off & 15];
        *p++ = ' ';
        for (k = 28; k >= 0; k -= 4) *p++ = hex[n >> k & 15];
        *p++ = '\r'; *p++ = '\n';
        WriteFile(f, line, (DWORD)(p - line), &wrote, 0);
    }
    (void)used;
    CloseHandle(f);
}

static DWORD WINAPI trace_thread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        Sleep(2000);
        trace_write();
    }
    return 0;
}
#endif

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
#ifdef FS98_UTIL_TRACE
        {
            unsigned i;
            for (i = 0; i < NSLOT; i++) {
                fs98_util_target[i] = Linkage.slot[i];
                if (Linkage.slot[i])
                    Linkage.slot[i] = THUNK_ADDR(i);
            }
            CreateThread(0, 0, trace_thread, 0, 0, 0);
        }
#endif
    }
#ifdef FS98_UTIL_TRACE
    if (reason == DLL_PROCESS_DETACH)
        trace_write();
#endif
    return TRUE;
}
