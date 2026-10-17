/* DEMO.DLL -- module id 7: flight videos and instant replay.
 *
 * A video (.VID, extension id 0x1d, in VIDEOS\) is a stream of tagged
 * records.  Recording, CHAIN list 2 samples GLOBAL every tick: the position
 * block as a frame record at one of four rates, and every 18 ticks the
 * controls that changed (flaps, gear, lights, the trim wheels...).  CONTROLS
 * reports the pilot's commands through +0x24.  Playing back, +0x18 is called
 * each frame and interpolates GLOBAL's position block between two frame
 * records, replaying the commands between them through CONTROLS +0x44.  A
 * record can carry a whole situation (MAIN +0x4c loads it) and a message
 * (WINMSG), or a sound cue.  Lessons and ADVDRV play videos too (+0x28), and
 * play recordings from memory (+0x20).
 *
 * Instant replay is a separate ring of 51 position snapshots, 0x20 bytes
 * each, taken every frame on CHAIN list 3 and played back through the same
 * reader from memory.
 *
 * Retail is two programs: hand-written assembly from .text+0x20 to +0x353e,
 * with ATC's stack-alignment wrapper around every call, and MSVC C from
 * +0x35e0 -- the four dialogs' callbacks.  The assembly passes values in
 * registers and returns status in the carry flag, so it is transliterated
 * here with six register variables and a carry flag, as fe.c does, and the
 * comments carry retail's addresses.  The whole 0x1165-byte .data image is
 * kept at retail's offsets: the reader routines are reached through a table
 * of pointers in it (+0x610..+0x634) that playback sets for a file or for
 * memory, and the record handlers through three more (+0x63c, +0x6b4,
 * +0x710).  docs/modules/demo.md walks through the format.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define DEMO_IMAGE_SIZE 0x1168          /* .data's 0x1165, rounded */
#define DEMO_IMPORTS    0x000
#define DEMO_LINKAGE    0x050

typedef volatile uint8_t  demo_v8;
typedef volatile uint16_t __attribute__((may_alias)) demo_v16;
typedef volatile uint32_t __attribute__((may_alias)) demo_v32;

struct demo_image {
    uint8_t bytes[DEMO_IMAGE_SIZE];
};

extern struct demo_image demo_image;

#define D       ((uint32_t)(uintptr_t)demo_image.bytes)
#define M8(a)   (*(demo_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(demo_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(demo_v32 *)(uintptr_t)(uint32_t)(a))

#define IMPORT(i)  M32(D + DEMO_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define CHAIN      IMPORT(2)
#define CONTROLS   IMPORT(3)
#define MAIN       IMPORT(4)
#define NUMERIC    IMPORT(5)
#define SOUND      IMPORT(6)
#define UTIL       IMPORT(7)
#define WINMSG     IMPORT(8)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f9)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *fplay)(uint32_t, uint32_t, double, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_POST       HOST_(0x03c, f2)    /* (command, parameter) */
#define H_DIALOG     HOST_(0x10c, f2)    /* (id, param) */
#define H_GET_ATTR   HOST_(0x134, f3)    /* (&attr, path, ext) */
#define H_CLOSE      HOST_(0x13c, f1)    /* (file) */
#define H_CREATE     HOST_(0x148, f3)    /* (&file, path, ext) */
#define H_DELETE     HOST_(0x14c, f2)    /* (path, ext) */
#define H_FREE_SPACE HOST_(0x150, f2)    /* (path, &u64) */
#define H_EXISTS     HOST_(0x158, f2)    /* (path, ext) */
#define H_MOVE       HOST_(0x16c, f4)    /* (to, toext, from, fromext) */
#define H_PATH_LEN   HOST_(0x170, f3)    /* (path, ext, &length) */
#define H_TEMP       HOST_(0x174, f2)    /* (out, dir) */
#define H_BAD_NAME   HOST_(0x178, f1)    /* (name): nonzero if not a file name */
#define H_OPEN       HOST_(0x17c, f4)    /* (&file, path, ext, mode) */
#define H_JOIN       HOST_(0x184, f4)    /* (out, drive, dir, name) */
#define H_SPLIT      HOST_(0x188, f5)    /* (path, drive, dir, name, &ext) */
#define H_TELL       HOST_(0x190, f2)    /* (file, &pos) */
#define H_SEEK       HOST_(0x194, f3)    /* (file, offset, whence) */
#define H_READ       HOST_(0x19c, f4)    /* (file, buf, n, &got) */
#define H_WRITE      HOST_(0x1b0, f4)    /* (file, buf, n, &wrote) */
#define H_FREE       HOST_(0x214, f1)    /* (&p) */
#define H_MEMMOVE    HOST_(0x21c, f3)    /* (dst, src, n) */
#define H_LOADSTRING HOST_(0x2a0, f3)    /* (id, buf, size) */
#define H_STRCPY     HOST_(0x2a8, f2)
#define H_STRCMP     HOST_(0x2bc, f2)
#define H_STRICMP    HOST_(0x2c0, f2)
#define H_VOICE_LOAD HOST_(0x2d4, f3)    /* (src, type, &h) */
#define H_VOICE_FREE HOST_(0x2dc, f1)    /* (&h) */
#define H_VOICE_PLAY HOST_(0x2e0, fplay) /* (h, loop, freq, done) */
#define H_VOICE_VOL  HOST_(0x2ec, f2)    /* (h, volume) */

#define CHAIN_ADD    CALL(CHAIN, 0x18, f2)   /* (list, callback) */
#define CHAIN_REMOVE CALL(CHAIN, 0x1c, f2)
#define CHAIN_SEND   CALL(CHAIN, 0x20, f3)   /* (list, a, b) */
#define CONTROLS_DO  CALL(CONTROLS, 0x44, f2) /* (command, value) */
#define MAIN_LOAD    CALL(MAIN, 0x48, f1)    /* (memory) a situation */
#define MAIN_LOAD_F  CALL(MAIN, 0x4c, f2)    /* (file, length) */
#define NUM_FORMAT   CALL(NUMERIC, 0x18, f3) /* (value, format, out) */
#define SOUND_SET    CALL(SOUND, 0x24, f2)   /* (what, value) */
#define SOUND_VOLUME CALL(SOUND, 0x30, f1)   /* (which) */
#define UTIL_LIST    CALL(UTIL, 0x98, f9)    /* the catalogue */
#define UTIL_ALERT   CALL(UTIL, 0xa4, f8)
#define WINMSG_SHOW  CALL(WINMSG, 0x18, f4)  /* (text, slot, time, mode) */

#define GLB8(o)  M8(G + (o))
#define GLB16(o) M16(G + (o))
#define GLB32(o) M32(G + (o))

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_REC_NAME   (D + 0x090)    /* the recording's file, 0x104 */
#define S_PLAY_NAME  (D + 0x194)    /* the video playing, 0x104 */
#define S_PLAY_FILE  (D + 0x298)
#define S_REC_FILE   (D + 0x29c)
#define S_CLOCK      (D + 0x2a0)    /* milliseconds, from GLOBAL +0x2aa */
#define S_SPEED_SET  (D + 0x2a4)    /* the video gave the airspeed */
#define S_VS_SET     (D + 0x2a8)    /* and the vertical speed */
#define S_FRAMES     (D + 0x2ac)    /* word: frame records written */
#define S_SAMPLE     (D + 0x2b0)    /* ticks to the next frame record */
#define S_LONG       (D + 0x2b4)    /* the keys are 48-bit positions */
#define S_DIALOG     (D + 0x2b8)    /* show the "video finished" dialog */
#define S_MEM_END    (D + 0x2bc)
#define S_BUF_VALID  (D + 0x2c0)
#define S_BUF        (D + 0x2c4)    /* 0x200: written or read */
#define S_BUF_POS    (D + 0x4c4)    /* read: in the buffer, -1 empty */
#define S_FILE_POS   (D + 0x4c8)    /* read: in the file */
#define S_BUF_FILL   (D + 0x4cc)    /* write */
#define S_WRITE_ERR  (D + 0x4d0)
#define S_STOP       (D + 0x4d4)
#define S_ASK        (D + 0x4d8)
#define S_LIST_C     (D + 0x4dc)    /* on CHAIN list 0xc */
#define S_RESYNC     (D + 0x4e0)
#define S_WRITABLE   (D + 0x4e4)
#define S_MSG_NEW    (D + 0x4e8)
#define S_MSG        (D + 0x4f8)    /* 0x38 */
#define S_MODE       (D + 0x54a)    /* +0x74c while playing a file */
#define S_KEY        (D + 0x550)    /* time, x, y, z, three angle words */
#define S_KEY_OLD    (D + 0x568)
#define S_LKEY       (D + 0x580)    /* time, three 48-bit, three dwords */
#define S_LKEY_OLD   (D + 0x5a8)
#define S_OUT        (D + 0x5d0)
#define S_PAUSED_AT  (D + 0x5e8)
#define S_PAUSED     (D + 0x5ec)    /* byte: 0xff none, 1 a pause to take out */
#define S_RING_I     (D + 0x5f0)
#define S_RING_SUM   (D + 0x5f4)
#define S_RING       (D + 0x5f8)    /* five speeds */
#define S_READ_OPEN  (D + 0x610)    /* the reader: */
#define S_READ_CLOSE (D + 0x614)
#define S_READ_REWIND (D + 0x618)
#define S_READ_BYTE  (D + 0x61c)
#define S_READ_PEEK  (D + 0x620)
#define S_READ_WORD  (D + 0x624)
#define S_READ_DWORD (D + 0x628)
#define S_READ_SKIP1 (D + 0x62c)
#define S_READ_SKIP  (D + 0x630)
#define S_READ_SYNC  (D + 0x634)
#define S_SEQ_SEEN   (D + 0x638)
#define S_SEQ        (D + 0x639)
#define S_KEEP_OLD   (D + 0x63a)    /* byte: the next key replaces the old too */
#define S_PENDING    (D + 0x63b)    /* byte: a key the frame tag implied */
#define S_HANDLERS   (D + 0x63c)    /* tags 0x0..0x18f, by tag >> 4 */
#define S_WORD_HANDLERS (D + 0x6b4) /* tags 0x2001..0x2017 */
#define S_EVENTS     (D + 0x710)    /* +0x24's twenty */
#define S_RATE_TAG   (D + 0x7c0)
#define S_RATE_TICKS (D + 0x7c5)
#define S_DELTAS     (D + 0x7d9)
#define S_LOW_DISK   (D + 0x7eb)
#define S_DISK_OK    (D + 0x7ef)
#define S_STARTING   (D + 0x7f3)
#define S_CUE        (D + 0x7f7)    /* "%%s:", a message that is a sound */
#define S_SLOW       (D + 0x7fb)    /* ticks to the next control sample */
#define S_REPLAY     (D + 0x800)    /* the ring, 51 x 0x20 */
#define S_REPLAY_END (D + 0xe60)
#define S_REPLAY_CB  (D + 0xe60)    /* the same address: a flag, too */
#define S_REPLAY_STOP (D + 0xe64)
#define S_REPLAY_EXIT (D + 0xe68)
#define S_REPLAY_N   (D + 0xe6c)

/* ---- registers --------------------------------------------------------------- */

static uint32_t eax, ebx, ecx, edx, esi, edi;
static int cf;

#define AX       ((uint16_t)eax)
#define AL       ((uint8_t)eax)
#define SET_AX(v) (eax = (eax & 0xffff0000u) | (uint16_t)(v))
#define SET_AL(v) (eax = (eax & 0xffffff00u) | (uint8_t)(v))
#define SET_AH(v) (eax = (eax & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define AH       ((uint8_t)(eax >> 8))
#define BX       ((uint16_t)ebx)
#define SET_BX(v) (ebx = (ebx & 0xffff0000u) | (uint16_t)(v))
#define CX       ((uint16_t)ecx)
#define SET_CX(v) (ecx = (ecx & 0xffff0000u) | (uint16_t)(v))
#define DX       ((uint16_t)edx)
#define SET_DX(v) (edx = (edx & 0xffff0000u) | (uint16_t)(v))

#define SAVE_REGS    uint32_t saved_ebx = ebx, saved_esi = esi, saved_edi = edi
#define RESTORE_REGS (ebx = saved_ebx, esi = saved_esi, edi = saved_edi)

/* A routine in one of the image's tables. */
typedef void (*demo_fn)(void);
#define VIA(slot) ((demo_fn)(uintptr_t)M32(slot))()

/* rep movsb / movsw with esi and edi as retail leaves them */
static void movs(uint32_t n, uint32_t size)
{
    while (n--) {
        if (size == 2)
            M16(edi) = M16(esi);
        else
            M8(edi) = M8(esi);
        esi += size;
        edi += size;
    }
}

static void alert(uint32_t tag, uint32_t id);
static void record_flush(void);
static void disk_check(void);
static void record_end(void);
static void chain_c_on(void);
static void chain_c_off(void);
static void stop_and_ask(void);
static void stop_playing(void);
static void play_finish(void);
static void play_review_end(void);
static void FS98_STDCALL play_end(uint32_t again);
static void play_start(void);
static void readers_memory(void);
static void readers_file(void);
static void parse_frame(void);
static void parse_records(void);
static void replay_tick(void);
static void sound_cue(uint32_t name);
static void dialog_finished(void);
static void dialog_ask(void);
static void dialog_stopped(void);
static void FS98_STDCALL ring_record(void);
static void ring_reset(void);
static uint32_t FS98_STDCALL play(uint32_t name, uint32_t dialog);
static uint32_t FS98_STDCALL interpolate(uint32_t out);
static uint32_t FS98_STDCALL demo_message(uint32_t text);
static void FS98_STDCALL file_insert(uint32_t file, uint32_t pos,
                                     uint32_t buf, uint32_t n);
static uint32_t playing_this(uint32_t path);
static void fault_div(void);

/* ---- writing: .text+0x1113..+0x121d ------------------------------------------- */

/* .text+0x1113 -- one byte (al). */
static void put8(void)
{
    uint32_t keep = ebx;

    ebx = M32(S_BUF_FILL);
    M8(ebx + S_BUF) = AL;
    ebx++;
    M32(S_BUF_FILL) = ebx;
    if (ebx >= 0x200)
        record_flush();
    ebx = keep;
}

/* .text+0x113e -- a word, low byte first; leaves al the high byte. */
static void put16(void)
{
    put8();
    SET_AL(AH);
    put8();
}

/* .text+0x1136 -- a tag: a word, high byte first. */
static void put_tag(void)
{
    uint8_t lo = AL;

    SET_AL(AH);
    SET_AH(lo);
    put16();
}

/* .text+0x114b */
static void put32(void)
{
    put16();
    eax >>= 16;
    put16();
}

/* .text+0x1159 -- cx bytes from esi. */
static void put_bytes(void)
{
    do {
        SET_AL(M8(esi));
        esi++;
        put8();
        SET_CX(CX - 1);
    } while (CX != 0);
}

/* .text+0x1166 -- cx copies of al. */
static void put_fill(void)
{
    do {
        put8();
        SET_CX(CX - 1);
    } while (CX != 0);
}

/* .text+0x1170 -- tag ax, one more if bx is set. */
static void put_flag(void)
{
    if (BX != 0)
        SET_AX(AX + 1);
    put_tag();
}

/* Host and UTIL calls through a thunk that takes back the edx the callee
 * left: the flush below keeps only eax and ecx. */
uint32_t demo_callee_edx;
uint32_t FS98_STDCALL demo_call4_edx(uint32_t fn, uint32_t a, uint32_t b,
                                     uint32_t c, uint32_t d);
__asm__(".text\n\t.p2align 4\n"
        ".globl _demo_call4_edx@20\n"
        "_demo_call4_edx@20:\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "call *20(%esp)\n\t"
        "movl %edx, _demo_callee_edx\n\t"
        "movl %ecx, _demo_callee_ecx4\n\t"
        "ret $20\n");
uint32_t demo_callee_ecx4;

/* .text+0x117e -- write the buffer out.  A short write stops the recording,
 * saying so once.  Only eax and ecx are kept: edx comes back as the host's
 * write (or the alert) left it, and an event value written after a flush
 * that splits its record is that, not the value (see demo.md). */
static void record_flush(void)
{
    uint32_t wrote, keep_eax = eax, keep_ecx = ecx;

    if (M32(S_REC_FILE) != 0 && M32(S_BUF_FILL) != 0) {
        disk_check();
        demo_call4_edx(M32(HOST + 0x1b0), M32(S_REC_FILE), S_BUF, M32(S_BUF_FILL),
                       (uint32_t)(uintptr_t)&wrote);
        edx = demo_callee_edx;
        ecx = M32(S_BUF_FILL);
        M32(S_BUF_FILL) = 0;
        if (ecx != wrote && M32(S_WRITE_ERR) == 0) {
            alert(2, 0xf413);
            edx = demo_callee_edx;
            M32(S_STOP) = 1;
            M32(S_WRITE_ERR) = 1;
        }
    }
    eax = keep_eax;
    ecx = keep_ecx;
}

/* .text+0x121d -- under 4K free: say so once ("disk full", or while starting
 * "not enough room to record").  Retail ignores the free-space call's
 * result, and its outputs are uninitialised if it fails; here they start
 * at 0. */
static void disk_check(void)
{
    uint32_t space[2] = { 0, 0 };

    M32(S_DISK_OK) = 1;
    H_FREE_SPACE(0, (uint32_t)(uintptr_t)space);
    if (space[1] == 0 && space[0] <= 0x1000 && M32(S_LOW_DISK) == 0) {
        M32(S_LOW_DISK) = 1;
        M32(S_DISK_OK) = 0;
        if (M32(S_STARTING) == 0)
            alert(2, 0xf414);
        else
            alert(1, 0xf41a);
    }
}

/* ---- recording: .text+0x272..+0x1014 ------------------------------------------- */

/* .text+0x2ac5 -- an alert with string `id`. */
uint32_t FS98_STDCALL demo_call8_edx(uint32_t fn, uint32_t a, uint32_t b,
                                     uint32_t c, uint32_t d, uint32_t e,
                                     uint32_t f, uint32_t g, uint32_t h);
__asm__(".text\n\t.p2align 4\n"
        ".globl _demo_call8_edx@36\n"
        "_demo_call8_edx@36:\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "pushl 36(%esp)\n\t"
        "call *36(%esp)\n\t"
        "movl %edx, _demo_callee_edx\n\t"
        "ret $36\n");

static void alert(uint32_t tag, uint32_t id)
{
    char text[0x100];

    H_LOADSTRING(id, (uint32_t)(uintptr_t)text, 0x100);
    demo_call8_edx(M32(UTIL + 0xa4), 1, tag, (uint32_t)(uintptr_t)text, 0, 0, 0, 0, 0);
}

/* .text+0x272 -- start recording to a temporary file in PILOTS (string
 * 0x9002): the header, then every frame from here. */
static void record_start(void)
{
    char dir[0x100];
    SAVE_REGS;

    M32(S_DIALOG) = 0;
    M32(S_WRITE_ERR) = 0;
    stop_playing();
    if (M8(S_REC_NAME) != 0)
        goto out;
    M32(S_STARTING) = 1;
    disk_check();
    M32(S_STARTING) = 0;
    M32(S_LOW_DISK) = 0;
    if (M32(S_DISK_OK) == 0)
        goto out;
    H_LOADSTRING(0x9002, (uint32_t)(uintptr_t)dir, 0x100);
    eax = H_TEMP(S_REC_NAME, (uint32_t)(uintptr_t)dir);
    if (eax != 0) {
        M8(S_REC_NAME) = 0;
        alert(1, 0xf411);
        goto out;
    }
    eax = H_CREATE(S_REC_FILE, S_REC_NAME, 0x1d);
    if (eax != 0) {
        M8(S_REC_NAME) = 0;
        alert(1, 0xf412);
        goto out;
    }
    SET_AX(0x4001); put_tag();
    SET_AX(0x21); put16();
    SET_AL(0); SET_CX(0x1f); put_fill();
    SET_AX(0x4002); put_tag();
    SET_AX(0x102); put16();
    SET_AL(0); SET_CX(0x100); put_fill();
    SET_AX(0x2014); put_tag();
    SET_AX(0x600); put16();
    edi = G;
    GLB32(0x760) = 1;
    GLB16(0x18) = 5;
    GLB32(0x264) = 0;
    M16(S_FRAMES) = 0;
    SET_AX(0xffff);
    M16(D + 0x530) = AX;
    M16(D + 0x532) = AX;
    M16(D + 0x534) = AX;
    M16(D + 0x53c) = AX;
    M16(D + 0x53e) = AX;
    M16(D + 0x540) = AX;
    M32(D + 0x546) = 0;
    M16(D + 0x542) = AX;
    M16(D + 0x536) = AX;
    M16(D + 0x538) = AX;
    M16(D + 0x53a) = AX;
    M16(D + 0x544) = AX;
    M32(D + 0x4ec) = 0x7fffffffu;
    M32(D + 0x4f0) = 0x80000000u;
    M16(D + 0x4f4) = 0x8000;
    M16(D + 0x4f6) = 0x8000;
    chain_c_on();
    if (M16(edi + 0x830) == 2) {
        M16(edi + 0x830) = 1;
        alert(2, 0xf410);
    }
out:
    RESTORE_REGS;
}

/* .text+0xa75 -- write cx-1 bytes of esi (to its NUL) at offset eax, the
 * rest of the field zeroed, and put the file position back.  .text+0xab3 is
 * the second half. */
static void patch_field(void)
{
    uint8_t field[0x104];
    uint32_t keep_eax = eax, keep_ecx = ecx, i, pos, off;

    for (i = 0; i < 0x104; i++)
        field[i] = 0;
    ecx = keep_ecx - 1;
    edi = (uint32_t)(uintptr_t)field;
    for (;;) {
        SET_AL(M8(esi));
        esi++;
        M8(edi) = AL;
        edi++;
        if (AL == 0)
            break;
        if (--ecx == 0)
            break;
    }
    esi = (uint32_t)(uintptr_t)field;
    ecx = keep_ecx;
    eax = keep_eax;
    off = eax;
    record_flush();
    H_TELL(M32(S_REC_FILE), (uint32_t)(uintptr_t)&pos);
    H_SEEK(M32(S_REC_FILE), off, 0);
    put_bytes();
    record_flush();
    H_SEEK(M32(S_REC_FILE), pos, 0);
}

/* .text+0xa55, +0xa65 -- the title (0x1f) and description (0x100). */
static void patch_title(void)
{
    eax = 4;
    ecx = 0x1f;
    patch_field();
}

static void patch_description(void)
{
    eax = 0x27;
    ecx = 0x100;
    patch_field();
}

/* .text+0x4be -- keep the recording as `name` in PILOTS, with a title and
 * description. */
static void FS98_STDCALL record_save(uint32_t name, uint32_t title,
                                     uint32_t description)
{
    char dir[0x104], path[0x104];
    SAVE_REGS;

    if (M8(S_REC_NAME) != 0) {
        H_OPEN(S_REC_FILE, S_REC_NAME, 0x1d, 2);
        esi = title;
        patch_title();
        esi = description;
        patch_description();
        record_end();
        H_LOADSTRING(0x9002, (uint32_t)(uintptr_t)dir, 0x100);
        H_JOIN((uint32_t)(uintptr_t)path, 0, (uint32_t)(uintptr_t)dir, name);
        if (H_EXISTS((uint32_t)(uintptr_t)path, 0x1d) == 0)
            H_DELETE((uint32_t)(uintptr_t)path, 0x1d);
        eax = H_MOVE((uint32_t)(uintptr_t)path, 0x1d, S_REC_NAME, 0x1d);
        if (eax == 0)
            M8(S_REC_NAME) = 0;
    }
    edi = G;
    GLB16(0x18) = 0;
    RESTORE_REGS;
}

/* .text+0x642 -- throw the recording away. */
static void record_discard(void)
{
    SAVE_REGS;

    if (M8(S_REC_NAME) != 0) {
        record_end();
        H_DELETE(S_REC_NAME, 0x1d);
        M8(S_REC_NAME) = 0;
    }
    edi = G;
    GLB16(0x18) = 0;
    RESTORE_REGS;
}

/* .text+0x696 -- go on recording, at the end of the file. */
static void record_resume(void)
{
    SAVE_REGS;

    M32(S_WRITE_ERR) = 0;
    edi = G;
    if (GLB32(0x760) == 0 && M8(S_REC_NAME) != 0) {
        H_OPEN(S_REC_FILE, S_REC_NAME, 0x1d, 2);
        disk_check();
        H_SEEK(M32(S_REC_FILE), 0, 2);
        M32(edi + 0x760) = 1;
        M32(S_BUF_FILL) = 0;
        chain_c_on();
    }
    RESTORE_REGS;
}

/* .text+0x9e8 -- open `esi` to write; the host's answer, or -1 for no name. */
static void open_named(void)
{
    H_STRCPY(S_REC_NAME, esi);
    eax = 0xffffffffu;
    if (M8(S_REC_NAME) == 0)
        return;
    eax = H_OPEN(S_REC_FILE, S_REC_NAME, 0x1d, 2);
}

/* .text+0x984, +0x9b6 -- set a saved video's title or description. */
static uint32_t FS98_STDCALL set_title(uint32_t name, uint32_t title)
{
    SAVE_REGS;

    esi = name;
    open_named();
    if (eax == 0) {
        esi = title;
        patch_title();
        record_end();
        M8(S_REC_NAME) = 0;
        eax = 0;
    }
    RESTORE_REGS;
    return eax;
}

static uint32_t FS98_STDCALL set_description(uint32_t name, uint32_t text)
{
    SAVE_REGS;

    esi = name;
    open_named();
    if (eax == 0) {
        esi = text;
        patch_description();
        record_end();
        M8(S_REC_NAME) = 0;
        eax = 0;
    }
    RESTORE_REGS;
    return eax;
}

/* .text+0x8fc, +0x941 -- the video playing, closed and reopened round the
 * catalogue. */
static void play_file_close(void)
{
    if (M32(S_PLAY_FILE) != 0) {
        VIA(S_READ_SYNC);
        H_CLOSE(M32(S_PLAY_FILE));
        M32(S_PLAY_FILE) = 0;
    }
}

static void play_file_reopen(void)
{
    if (M8(S_PLAY_NAME) != 0) {
        H_OPEN(S_PLAY_FILE, S_PLAY_NAME, 0x1d, 0);
        VIA(S_READ_SYNC);
    }
}

/* .text+0x747 -- a message, up to 0x38 bytes with its NUL: 2 if longer.
 * Recording, it is written (3 with no file).  Playing a writable video, it
 * is inserted into the file where playback is (4 if the video is read-only,
 * 3 with nothing open).  Otherwise 3. */
static uint32_t FS98_STDCALL demo_message(uint32_t text)
{
    uint8_t local[0x40];
    uint32_t length, pos, i;
    SAVE_REGS;

    edi = text;
    for (ecx = 0x38; ecx != 0;) {
        ecx--;
        if (M8(edi++) == 0)
            goto found;
    }
    eax = 2;
    goto out;
found:
    if (ecx == 0x38) {      /* never: the scan always counts one */
        eax = 1;
        goto out;
    }
    length = 0x38 - ecx;
    edi = G;
    if (GLB32(0x760) != 0) {
        if (M8(S_REC_NAME) == 0) {
            eax = 3;
            goto out;
        }
        SET_AL(0x84);
        put8();
        esi = text;
        ecx = length;
        put_bytes();
        esi = text;
        edi = S_MSG;
        ecx = length;
        movs(length, 1);
        ecx = 0;
        M32(S_MSG_NEW) = 1;
        eax = 0;
        goto out;
    }
    if (GLB32(0x758) == 0) {
        eax = 3;
        goto out;
    }
    if (M32(S_WRITABLE) == 0) {
        eax = 4;
        goto out;
    }
    if (M32(S_PLAY_FILE) == 0) {
        eax = 3;
        goto out;
    }
    VIA(S_READ_SYNC);
    local[0x3f - 0x3f] = 0x84;
    for (i = 0; i < length; i++)
        local[1 + i] = M8(text + i);
    length++;
    H_TELL(M32(S_PLAY_FILE), (uint32_t)(uintptr_t)&pos);
    H_CLOSE(M32(S_PLAY_FILE));
    eax = H_OPEN(S_PLAY_FILE, S_PLAY_NAME, 0x1d, 1);
    if (eax == 0)
        file_insert(M32(S_PLAY_FILE), pos, (uint32_t)(uintptr_t)local, length);
    H_CLOSE(M32(S_PLAY_FILE));
    play_file_reopen();
out:
    RESTORE_REGS;
    return eax;
}

/* ---- the CHAIN callbacks -------------------------------------------------------- */

/* .text+0xb60 -- list 1, every tick: the clock, instant replay, the message
 * a video carries (or the sound it names), the dialogs asked for. */
static void FS98_STDCALL demo_tick(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    (void)a;
    (void)b;
    edi = G;
    eax = (uint32_t)M16(edi + 0x2aa) * 1000u >> 15;
    M32(S_CLOCK) += eax;
    replay_tick();
    if (M32(S_MSG_NEW) != 0) {
        M32(S_MSG_NEW) = 0;
        if (M32(D + 0x60c) == 0) {
            if (M32(S_MSG) == M32(S_CUE))
                sound_cue(D + 0x4fc);
            else
                WINMSG_SHOW(S_MSG, 5, 0x90, 0);
        }
    }
    if (M32(S_ASK) != 0) {
        M32(S_ASK) = 0;
        dialog_ask();
    }
    if (M32(S_STOP) != 0 && M16(S_FRAMES) >= 2) {
        M32(S_STOP) = 0;
        stop_and_ask();
    }
    RESTORE_REGS;
}

/* .text+0xc4b -- lists 9 and 0xd: the sim paused; take the pause out of
 * playback's keys. */
static void unpause(void)
{
    if (M8(S_PAUSED) == 0xff)
        return;
    eax = M32(S_CLOCK) - M32(S_PAUSED_AT);
    M32(S_KEY_OLD) += eax;
    M32(S_PAUSED_AT) += eax;
    M32(S_KEY) += eax;
    M8(S_PAUSED) = 1;
}

static void FS98_STDCALL demo_paused(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    (void)a;
    (void)b;
    unpause();
    RESTORE_REGS;
}

/* .text+0x233 -- list 3, every frame: the instant replay ring. */
static void FS98_STDCALL demo_frame(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    (void)a;
    (void)b;
    ring_record();
    RESTORE_REGS;
}

/* .text+0xc79 -- one sample.  Frame records at the rate GLOBAL +0x75c
 * picks; the controls every 18 ticks, each only when it changed. */
static void sample(void)
{
    edi = G;
    if (--M32(S_SAMPLE) == 0) {
        ebx = M32(edi + 0x75c);
        eax = M32(ebx * 4 + S_RATE_TICKS);
        M32(S_SAMPLE) = eax;
        eax = M32(edi + 0x2b8);
        if (eax != M32(D + 0x4ec)) {
            SET_AX(0x4003); put_tag();
            SET_AX(6); put16();
            eax = M32(edi + 0x2b8);
            M32(D + 0x4ec) = eax;
            put32();
        }
        eax = M32(edi + 0x2c8);
        if (eax != M32(D + 0x4f0)) {
            M32(D + 0x4f0) = 0x80000000u;
            SET_AX(0x4004); put_tag();
            SET_AX(6); put16();
            eax = M32(edi + 0x2c8);
            M32(D + 0x4f0) = eax;
            put32();
        }
        SET_AX(M16(edi + 0x37c));
        if (AX != M16(D + 0x4f4)) {
            SET_AX(0x2015); put_tag();
            SET_AX(M16(edi + 0x37c));
            M16(D + 0x4f4) = AX;
            put16();
        }
        SET_AX((int16_t)(int8_t)M8(edi + 0x36e));
        if (AX != M16(D + 0x4f6)) {
            SET_AX(0x2016); put_tag();
            SET_AX((int16_t)(int8_t)M8(edi + 0x36e));
            M16(D + 0x4f6) = AX;
            put16();
        }
        ebx = M32(edi + 0x75c);
        SET_AL(M8(ebx + S_RATE_TAG)); put8();
        SET_AX(M16(edi + 0x562)); put16();
        eax = M32(edi + 0x564); put32();
        SET_AX(M16(edi + 0x56a)); put16();
        eax = M32(edi + 0x56c); put32();
        SET_AX(M16(edi + 0x572)); put16();
        eax = M32(edi + 0x574); put32();
        eax = M32(edi + 0x578); put32();
        eax = M32(edi + 0x57c); put32();
        eax = M32(edi + 0x580); put32();
        SET_AX(M16(S_FRAMES)); put8();
        M16(S_FRAMES)++;
    }
    if (--M32(S_SLOW) != 0)
        return;
    M32(S_SLOW) = 0x12;
    /* Each of these three is `mov bx,1` then `mov bx,0`: always 0. */
    SET_BX(0);
    if (BX != M16(D + 0x536)) {
        M16(D + 0x536) = BX;
        SET_AX(0xf0); put_flag();
    }
    SET_BX(0);
    if (BX != M16(D + 0x538)) {
        M16(D + 0x538) = BX;
        SET_AX(0x100); put_flag();
    }
    SET_BX(0);
    if (BX != M16(D + 0x53a)) {
        M16(D + 0x53a) = BX;
        SET_AX(0x110); put_flag();
    }
    if (M16(edi + 0x5dc) != 0)
        return;
    SET_BX(M16(edi + 0xbb2) & 0xffe0);
    if (BX != M16(D + 0x53e)) {
        M16(D + 0x53e) = BX;
        SET_AX(0x2001); put_tag();
        SET_AX(M16(edi + 0xbb2)); put16();
    }
    SET_BX(M16(edi + 0xbb6) & 0xffe0);
    if (BX != M16(D + 0x530)) {
        M16(D + 0x530) = BX;
        SET_AX(0x2002);
        if (M32(edi + 0x278) != 0) {
            SET_AX(0x2004);
            M16(D + 0x540) = BX;
        }
        put_tag();
        SET_AX(M16(edi + 0xbb6)); put16();
    }
    if (M32(edi + 0x278) == 0) {
        SET_BX(M16(edi + 0xbba) & 0xffe0);
        if (BX != M16(D + 0x540)) {
            M16(D + 0x540) = BX;
            SET_AX(0x2003); put_tag();
            SET_AX(M16(edi + 0xbba)); put16();
        }
    }
    SET_BX(M16(edi + 0xbc0) & 0xffe0);
    if (BX != M16(D + 0x53c)) {
        M16(D + 0x53c) = BX;
        SET_AX(0x200a); put_tag();
        SET_AX(M16(edi + 0xbc0)); put16();
    }
    SET_BX(M16(edi + 0x88c) & 0xffe0);
    if (BX != M16(D + 0x542)) {
        M16(D + 0x542) = BX;
        SET_AX(0x2005); put_tag();
        SET_AX(M16(edi + 0x88c)); put16();
    }
    ebx = (uint32_t)(int32_t)(int8_t)M8(edi + 0x36c);
    if (ebx != M32(D + 0x546)) {
        M32(D + 0x546) = ebx;
        SET_AX(0xc0); put_flag();
    }
    SET_BX(M16(edi + 0xc00));
    if (BX != M16(D + 0x532)) {
        M16(D + 0x532) = BX;
        SET_AX(0x2012); put_tag();
        SET_AX(M16(edi + 0xc00)); put16();
    }
    SET_BX(M16(edi + 0xbc8));
    if (BX != M16(D + 0x534)) {
        M16(D + 0x534) = BX;
        SET_AX(0x2013); put_tag();
        SET_AX(M16(edi + 0xbc8)); put16();
    }
    SET_BX(M8(edi + 0x366));
    if (BX != M16(D + 0x544)) {
        M16(D + 0x544) = BX;
        SET_AX(0x180); put_flag();
    }
}

/* .text+0x24b -- list 2, every tick: sample while recording. */
static void FS98_STDCALL demo_sample(uint32_t a, uint32_t b)
{
    SAVE_REGS;

    (void)a;
    (void)b;
    edi = G;
    if (M32(edi + 0x760) != 0)
        sample();
    RESTORE_REGS;
}

/* .text+0x1014 -- list 0xc, the sim's commands, while recording or playing.
 * Recording: 0x10033 asks for the dialog, 0x1002b and 0x10001 stop, 0x10007
 * and 0x10008 step the sample rate; taken if done.  Playing: 0x10001 stops;
 * five commands pass through, and the rest are swallowed. */
static void FS98_STDCALL demo_command(uint32_t command, uint32_t taken)
{
    SAVE_REGS;

    edi = G;
    if (M32(edi + 0x758) == 0) {
        if (M32(taken) != 0)
            goto out;
        switch (command) {
        case 0x10033:
            M32(S_ASK) = 1;
            break;
        case 0x1002b:
        case 0x10001:
            M32(S_STOP) = 1;
            break;
        case 0x10008:
            if (M32(edi + 0x75c) == 3)
                goto out;
            M32(edi + 0x75c)++;
            break;
        case 0x10007:
            if (M32(edi + 0x75c) == 0)
                goto out;
            M32(edi + 0x75c)--;
            break;
        default:
            goto out;
        }
        eax = 1;
    } else {
        switch (command) {
        case 0x10033:
            if (M32(S_WRITABLE) != 0)
                M32(S_ASK) = 1;
            eax = 1;
            break;
        case 0x10001:
            stop_playing();
            eax = 1;
            break;
        case 0x10010:
        case 0x10019:
        case 0x100c4:
        case 0x100c5:
        case 0x100c9:
            eax = 0;
            break;
        default:
            eax = 1;
            break;
        }
    }
    ebx = taken;
    M32(ebx) = eax;
out:
    RESTORE_REGS;
}

/* .text+0x1102 -- +0x34: stop recording at the next tick. */
static void FS98_STDCALL demo_stop(void)
{
    M32(S_STOP) = 1;
}

/* .text+0x12a9 -- stop recording: unhook, flush, close. */
static void record_end(void)
{
    edi = G;
    M32(edi + 0x760) = 0;
    chain_c_off();
    if (M32(S_REC_FILE) != 0) {
        record_flush();
        H_CLOSE(M32(S_REC_FILE));
        M32(S_REC_FILE) = 0;
    }
}

/* .text+0x1302, +0x1336 */
static void chain_c_on(void)
{
    CHAIN_ADD(0xc, (uint32_t)(uintptr_t)demo_command);
    M32(S_LIST_C) = 1;
}

static void chain_c_off(void)
{
    if (M32(S_LIST_C) != 0) {
        CHAIN_REMOVE(0xc, (uint32_t)(uintptr_t)demo_command);
        M32(S_LIST_C) = 0;
    }
}

/* .text+0x1373 -- stop recording and ask what to do with it. */
static void stop_and_ask(void)
{
    edi = G;
    M32(edi + 0x760) = 0;
    record_end();
    dialog_finished();
}

/* ---- playing: .text+0x138e..+0x1b08 ---------------------------------------------- */

/* .text+0x138e -- +0x28: play video `name`; `dialog` asks at the end.
 * Refused while GLOBAL +0x748 is set.  Carry and eax: a failed open. */
static uint32_t FS98_STDCALL play(uint32_t name, uint32_t dialog)
{
    uint32_t keep;
    SAVE_REGS;

    edi = G;
    keep = M32(edi + 0x750);
    M32(S_DIALOG) = 0;
    stop_playing();
    M32(edi + 0x750) = keep;
    M32(S_DIALOG) = dialog;
    H_STRCPY(S_PLAY_NAME, name);
    edi = G;
    if (M32(edi + 0x748) != 0) {
        alert(2, 0xf415);
        eax = 0xffffffffu;
        cf = 1;
        goto out;
    }
    record_end();
    readers_file();
    M32(S_MODE) = 1;
    VIA(S_READ_OPEN);
    if (cf)
        goto out;
    play_start();
    eax = 0;
    cf = 0;
out:
    RESTORE_REGS;
    return eax;
}

/* .text+0x143b -- read the first two frames. */
static void play_start(void)
{
    edi = G;
    M32(S_SPEED_SET) = 0;
    M32(S_VS_SET) = 0;
    M8(edi + 0x36c) = 0;
    M16(edi + 0xbc8) = 0;
    M32(edi + 0x758) = 1;
    M32(edi + 0x264) = 0;
    M16(edi + 0x18) = 5;
    M16(D + 0x532) = 0;
    M32(D + 0x546) = 0;
    chain_c_on();
    eax = M32(S_MODE);
    M32(edi + 0x74c) = eax;
    VIA(S_READ_REWIND);
    esi = M32(edi + 0x754);
    eax = M32(edi + 0x744);
    M32(S_MEM_END) = eax;
    M32(S_RESYNC) = 1;
    parse_frame();
    if (cf) {
        play_end(0);
        return;
    }
    M32(S_RESYNC) = 0;
    parse_frame();
    if (cf) {
        play_end(0);
        return;
    }
    edi = G;
    M32(edi + 0x754) = esi;
    eax = M32(S_CLOCK);
    M32(S_OUT) = eax;
    M8(S_PAUSED) = 0xff;
}

/* .text+0x150b -- play the recording just made, back to the dialog after. */
static void play_recording(void)
{
    SAVE_REGS;

    if (M8(S_REC_NAME) != 0) {
        play(S_REC_NAME, 1);
        edi = G;
        M32(edi + 0x758) = 2;
    }
    RESTORE_REGS;
}

/* .text+0x1537 -- +0x18, every frame while playing: GLOBAL's position block
 * between the keys, reading frames as the clock passes them.  At the end a
 * looping video (GLOBAL +0x750) starts again.  A frame whose sequence byte
 * does not follow the last tells CHAIN list 0x14 (message 0x35). */
static void FS98_STDCALL demo_play_frame(void)
{
    SAVE_REGS;

    for (;;) {
        edi = G;
        interpolate(edi + 0x55c);
        if (!cf) {
            edi = G;
            M32(edi + 0xd2c) = 0;
            goto out;
        }
        for (;;) {
            edi = G;
            esi = M32(edi + 0x754);
            eax = M32(edi + 0x744);
            M32(S_MEM_END) = eax;
            M32(S_RESYNC) = 0;
            parse_frame();
            if (!cf)
                break;
            play_end(M32(edi + 0x750));
            if (M32(edi + 0x750) == 0)
                goto out;
            VIA(S_READ_REWIND);
            M32(edi + 0x758) = 1;
            M32(edi + 0x74c) = 1;
        }
        edi = G;
        M32(edi + 0x754) = esi;
        SET_AL(M8(S_SEQ));
        SET_AL(AL - M8(S_SEQ_SEEN));
        if (AL != 1)
            CHAIN_SEND(0x14, 0x35, 0);
        SET_AL(M8(S_SEQ));
        M8(S_SEQ_SEEN) = AL;
    }
out:
    RESTORE_REGS;
}

/* .text+0x1611 -- the end of the video. */
static void FS98_STDCALL play_end(uint32_t again)
{
    H_POST(0x100ce, 0);
    edi = G;
    M32(edi + 0x74c) = 0;
    M8(edi + 0x36c) = 0;
    if (--M32(edi + 0x758) != 0)
        play_review_end();
    else if (again == 0)
        play_finish();
}

/* .text+0x1672 */
static void play_finish(void)
{
    VIA(S_READ_CLOSE);
    chain_c_off();
    edi = G;
    M32(edi + 0x73c) = 0;
    M16(edi + 0x18) = 0;
    if (M32(S_DIALOG) != 0) {
        M32(edi + 0x264) = 1;
        dialog_stopped();
    }
}

/* .text+0x16ac -- reviewing a recording: run to its end, put the aircraft
 * where it ends, and stop. */
static void play_review_end(void)
{
    do {
        edi = G;
        esi = M32(edi + 0x754);
        M32(S_RESYNC) = 0;
        parse_frame();
    } while (!cf);
    esi = S_LKEY;
    edi += 0x55c;           /* the edi parsing left: GLOBAL, as it happens */
    movs(0x28, 1);
    ecx = 0;
    edi = G;
    VIA(S_READ_CLOSE);
    chain_c_off();
    M32(edi + 0x758) = 0;
    M32(edi + 0x74c) = 0;
    M32(S_STOP) = 1;
}

/* .text+0x170b -- the readers for a recording in memory (esi to the end
 * at S_MEM_END), and .text+0x1766..+0x17db. */
static void m_byte(void)  { SET_AL(M8(esi)); esi++; }
static void m_peek(void)
{
    if (esi >= M32(S_MEM_END)) {
        cf = 1;
        return;
    }
    SET_AL(M8(esi));
    cf = 0;
}
static void m_skip1(void) { esi++; }
static void m_skip(void)  { cf = esi + eax < esi; esi += eax; }
static void m_word(void)  { SET_AX(M16(esi)); cf = esi + 2 < esi; esi += 2; }
static void m_dword(void) { eax = M32(esi); cf = esi + 4 < esi; esi += 4; }
static void m_rewind(void) { M32(edi + 0x754) = 0; }
static void m_close(void)
{
    edi = G;
    if (M32(edi + 0x740) != 0) {
        H_FREE(edi + 0x740);
        M32(edi + 0x740) = 0;
    }
}
static void m_sync(void)  { }

static void readers_memory(void)
{
    M32(S_READ_CLOSE) = (uint32_t)(uintptr_t)m_close;
    M32(S_READ_REWIND) = (uint32_t)(uintptr_t)m_rewind;
    M32(S_READ_BYTE) = (uint32_t)(uintptr_t)m_byte;
    M32(S_READ_PEEK) = (uint32_t)(uintptr_t)m_peek;
    M32(S_READ_WORD) = (uint32_t)(uintptr_t)m_word;
    M32(S_READ_DWORD) = (uint32_t)(uintptr_t)m_dword;
    M32(S_READ_SKIP1) = (uint32_t)(uintptr_t)m_skip1;
    M32(S_READ_SKIP) = (uint32_t)(uintptr_t)m_skip;
    M32(S_READ_SYNC) = (uint32_t)(uintptr_t)m_sync;
}

/* .text+0x17dc..+0x1ac1 -- the readers for a file, through the 0x200-byte
 * buffer.  Past the end every byte reads 0x89, the end tag. */
/* The refill keeps only eax: ecx and edx come back as the host's read left
 * them, and the message reader, which counts in ecx across it, then counts
 * from that (see demo.md). */
static void f_refill(void)
{
    uint32_t got, keep = eax;

    demo_call4_edx(M32(HOST + 0x19c), M32(S_PLAY_FILE), S_BUF, 0x200,
                   (uint32_t)(uintptr_t)&got);
    ecx = demo_callee_ecx4;
    edx = demo_callee_edx;
    eax = got;
    M32(S_BUF_VALID) = eax;
    if (eax != 0x200) {
        do {
            M8(eax + S_BUF) = 0x89;
            eax++;
        } while (eax < 0x200);
    }
    eax = keep;
}

static void f_peek(void)
{
    uint32_t keep;

    while (M32(S_BUF_POS) >= 0x200) {
        f_refill();
        if (M32(S_BUF_POS) == 0xffffffffu)
            M32(S_BUF_POS) = 0;
        else
            M32(S_BUF_POS) -= 0x200;
    }
    keep = ebx;
    ebx = M32(S_BUF_POS);
    if (ebx < M32(S_BUF_VALID)) {
        SET_AL(M8(ebx + S_BUF));
        cf = 0;
    } else {
        SET_AL(0x89);
        cf = 1;
    }
    ebx = keep;
}

static void f_byte(void)
{
    f_peek();
    M32(S_BUF_POS)++;
    M32(S_FILE_POS)++;
}

static void f_skip1(void)
{
    M32(S_BUF_POS)++;
    M32(S_FILE_POS)++;
}

static void f_skip(void)
{
    M32(S_BUF_POS) += eax;
    cf = M32(S_FILE_POS) + eax < M32(S_FILE_POS);
    M32(S_FILE_POS) += eax;
}

static void f_word(void)
{
    uint8_t lo;

    f_byte();
    SET_AH(AL);
    f_byte();
    lo = AL;
    SET_AL(AH);
    SET_AH(lo);
}

static void f_dword(void)
{
    f_word();
    eax <<= 16;
    f_word();
    eax = (eax << 16) | (eax >> 16);
}

static void f_rewind(void)
{
    H_SEEK(M32(S_PLAY_FILE), 0, 0);
    M32(S_BUF_POS) = 0xffffffffu;
    M32(S_FILE_POS) = 0;
}

static void f_close(void)
{
    if (M32(S_PLAY_FILE) != 0) {
        H_CLOSE(M32(S_PLAY_FILE));
        M32(S_PLAY_FILE) = 0;
    }
    M8(S_PLAY_NAME) = 0;
    M32(S_BUF_POS) = 0xffffffffu;
    M32(S_FILE_POS) = 0;
}

/* .text+0x19db -- open S_PLAY_NAME; carry if it is not there.  A video
 * that is not read-only can take messages. */
static void f_open(void)
{
    uint32_t attributes;

    eax = H_EXISTS(S_PLAY_NAME, 0x1d);
    if (eax != 0)
        goto fail;
    eax = H_GET_ATTR((uint32_t)(uintptr_t)&attributes, S_PLAY_NAME, 0x1d);
    if (eax != 0)
        goto fail;
    M32(S_WRITABLE) = 0;
    if (!(attributes & 2))
        M32(S_WRITABLE) = 1;
    eax = H_OPEN(S_PLAY_FILE, S_PLAY_NAME, 0x1d, 0);
    if (eax != 0)
        goto fail;
    M32(S_BUF_POS) = 0xffffffffu;
    M32(S_FILE_POS) = 0;
    cf = 0;
    return;
fail:
    M8(S_PLAY_NAME) = 0;
    cf = 1;
}

/* .text+0x1ac1 -- after the file moved under the buffer. */
static void f_sync(void)
{
    if (M32(S_PLAY_FILE) != 0) {
        H_SEEK(M32(S_PLAY_FILE), M32(S_FILE_POS), 0);
        M32(S_BUF_POS) = 0xffffffffu;
    }
}

static void readers_file(void)
{
    M32(S_READ_OPEN) = (uint32_t)(uintptr_t)f_open;
    M32(S_READ_CLOSE) = (uint32_t)(uintptr_t)f_close;
    M32(S_READ_REWIND) = (uint32_t)(uintptr_t)f_rewind;
    M32(S_READ_BYTE) = (uint32_t)(uintptr_t)f_byte;
    M32(S_READ_PEEK) = (uint32_t)(uintptr_t)f_peek;
    M32(S_READ_WORD) = (uint32_t)(uintptr_t)f_word;
    M32(S_READ_DWORD) = (uint32_t)(uintptr_t)f_dword;
    M32(S_READ_SKIP1) = (uint32_t)(uintptr_t)f_skip1;
    M32(S_READ_SKIP) = (uint32_t)(uintptr_t)f_skip;
    M32(S_READ_SYNC) = (uint32_t)(uintptr_t)f_sync;
}

/* .text+0x1b08 -- stop playing (a review runs to its end first). */
static void stop_playing(void)
{
    SAVE_REGS;

    edi = G;
    M8(edi + 0x36c) = 0;
    M32(edi + 0x750) = 0;
    H_POST(0x100ce, 0);
    if (M32(edi + 0x758) == 2) {
        play_review_end();
    } else {
        M32(edi + 0x74c) = 0;
        M32(edi + 0x758) = 0;
        play_finish();
    }
    RESTORE_REGS;
}

/* ---- interpolation: .text+0x1b78, +0x2fc2 --------------------------------------- */

/* .text+0x2fc2 -- the 48-bit difference ax:edx times ecx (Q15) on the x87,
 * back as ax:edx. */
static void mul48(void)
{
    int32_t t = (int32_t)ecx;
    int64_t v;

    eax <<= 16;
    eax = (eax >> 16) | (edx << 16);
    edx = (uint32_t)((int32_t)edx >> 16);
    v = (int64_t)(((uint64_t)edx << 32) | eax);
    __asm__ ("fildll %1\n\tfimull %2\n\tfistpll %0"
             : "=m" (v) : "m" (v), "m" (t) : "st");
    eax = (uint32_t)v;
    edx = (uint32_t)((uint64_t)v >> 32);
    edx = (edx << 1) | (eax >> 31);
    eax >>= 15;
}

/* .text+0x1b78 -- +0x1c: the position between the two keys at the clock,
 * into `out` (GLOBAL +0x55c's layout).  Carry once the clock is past the
 * newer key.  Short keys give the angles only; long keys everything. */
static uint32_t FS98_STDCALL interpolate(uint32_t out)
{
    int64_t p;
    SAVE_REGS;

    esi = S_OUT;
    edi = out;
    eax = M32(S_CLOCK);
    if (M8(S_PAUSED) != 1)
        M32(S_PAUSED_AT) = eax;
    M8(S_PAUSED) = 0;
    ebx = M32(S_KEY_OLD);
    eax -= ebx;
    ebx = -ebx + M32(S_KEY);
    if (eax >= ebx) {
        cf = 1;
        goto out;
    }
    ecx = M32(S_CLOCK);
    M32(esi) = ecx;
    M32(edi) = ecx;
    do {
        eax += eax;
        ebx += ebx;
    } while ((int32_t)ebx >= 0);
    ebx >>= 15;
    eax = eax / ebx;
    ecx = eax;
#define LERP32(key, old, dst) \
    p = (int64_t)(int32_t)(M32(key) - M32(old)) * (int32_t)ecx; \
    eax = (uint32_t)((uint64_t)p >> 15) + M32(old); \
    M32(dst) = eax
#define LERP16(key, old, dst) \
    p = (int32_t)(int16_t)(uint16_t)(M16(key) - M16(old)) * (int16_t)CX; \
    SET_DX((uint16_t)((uint32_t)p >> 15) + M16(old)); \
    M16(dst) = DX
    if (M32(S_LONG) == 0) {
        LERP32(D + 0x554, D + 0x56c, esi + 4);
        LERP32(D + 0x558, D + 0x570, esi + 8);
        LERP32(D + 0x55c, D + 0x574, esi + 0xc);
        LERP16(D + 0x560, D + 0x578, esi + 0x10);
        LERP16(D + 0x562, D + 0x57a, esi + 0x12);
        LERP16(D + 0x564, D + 0x57c, esi + 0x14);
        SET_AX(M16(esi + 0x10));
        eax <<= 16;
        M32(edi + 0x1c) = eax;
        SET_AX(M16(esi + 0x12));
        eax <<= 16;
        M32(edi + 0x20) = eax;
        SET_AX(M16(esi + 0x14));
        eax <<= 16;
        M32(edi + 0x24) = eax;
    } else {
        static const uint16_t parts[3][2] = {
            { 0x586, 0x5ae }, { 0x58e, 0x5b6 }, { 0x596, 0x5be }
        };
        uint32_t k;

        for (k = 0; k < 3; k++) {
            uint16_t lo = M16(D + parts[k][0]), olo = M16(D + parts[k][1]);
            uint32_t hi = M32(D + parts[k][0] + 2), ohi = M32(D + parts[k][1] + 2);
            uint32_t borrow = lo < olo, carry;
            uint16_t dl;

            SET_AX(lo - olo);
            edx = hi - ohi - borrow;
            mul48();
            dl = AX;
            carry = (uint32_t)dl + olo > 0xffff;
            SET_AX(dl + olo);
            edx = edx + ohi + carry;
            M16(edi + 6 + 8 * k) = AX;
            M32(edi + 8 + 8 * k) = edx;
        }
        LERP32(D + 0x59c, D + 0x5c4, edi + 0x1c);
        LERP32(D + 0x5a0, D + 0x5c8, edi + 0x20);
        LERP32(D + 0x5a4, D + 0x5cc, edi + 0x24);
    }
#undef LERP32
#undef LERP16
    cf = 0;
out:
    RESTORE_REGS;
    return eax;
}

/* ---- parsing: .text+0x1da8..+0x2665 ---------------------------------------------- */

/* .text+0x1da8 -- +0x20: a frame of a recording in memory, from *cursor to
 * end; *done is 1 at its end. */
static void FS98_STDCALL parse_memory(uint32_t cursor, uint32_t end,
                                      uint32_t resync, uint32_t done)
{
    SAVE_REGS;

    readers_memory();
    M32(S_RESYNC) = resync;
    esi = M32(cursor);
    M32(S_MEM_END) = end;
    parse_frame();
    M32(cursor) = esi;
    M32(done) = 0;
    if (cf)
        M32(done) = 1;
    RESTORE_REGS;
}

static void resync_keys(void)
{
    if (M32(S_RESYNC) != 0) {
        eax = M32(S_CLOCK);
        M32(S_KEY_OLD) = eax;
        M32(S_LKEY_OLD) = eax;
        M32(S_KEY) = eax;
        M32(S_LKEY) = eax;
        M32(S_RESYNC) = 0;
    }
}

/* .text+0x1dea -- the next frame: the newer key becomes the older, the
 * records up to the frame tag are applied, and the tag's interval (scaled
 * by GLOBAL +0x634, the replay speed) is added to the key's time.  Carry at
 * the end.  A record that will not parse skips a second instead: the carry
 * is then the carry of that addition. */
static void parse_frame(void)
{
    uint32_t keep;

    VIA(S_READ_PEEK);
    if (cf)
        goto end;
    keep = esi;
    esi = S_KEY;
    edi = S_KEY_OLD;
    movs(0xc, 2);
    ecx = 0;
    if (M32(S_LONG) != 0) {
        esi = S_LKEY;
        edi = S_LKEY_OLD;
        movs(0x14, 2);
        ecx = 0;
        edi = G;
        esi = D + 0x5d4;
        edi = D + 0x554;
        movs(0xa, 2);
        ecx = 0;
    }
    esi = keep;
    parse_records();
    if (cf)
        goto skip;
    VIA(S_READ_PEEK);
    if (cf)
        goto skip;
    switch (AL) {
    case 0x9b: M8(S_PENDING) = 3; ecx = 250; goto step;
    case 0x9a: M8(S_PENDING) = 3; ecx = 500; goto step;
    case 0x98: M8(S_PENDING) = 3; goto second;
    case 0x94: M8(S_PENDING) = 2; goto second;
    case 0x90: M8(S_PENDING) = 1; goto second;
    case 0x80: VIA(S_READ_SKIP1); goto second;
    case 0x99: M8(S_PENDING) = 3; goto five;
    case 0x95: M8(S_PENDING) = 2; goto five;
    case 0x91: M8(S_PENDING) = 1; goto five;
    case 0x81: VIA(S_READ_SKIP1); goto five;
    case 0x88:
        ecx = 15000;
        VIA(S_READ_SKIP1);
        goto step;
    default:
        M32(S_DIALOG) = 0;
        stop_playing();
        alert(1, 0xf416);
        cf = 1;
        goto end;
    }
second:
    ecx = 1000;
    goto step;
five:
    ecx = 5000;
step:
    edi = G;
    {
        uint32_t speed = M32(edi + 0x634);
        uint64_t m;

        eax = 0x640000u / speed;
        m = (uint64_t)eax * ecx;
        eax = (uint32_t)(m >> 16);
        M32(S_KEY) += eax;
    }
    parse_records();
    if (cf)
        goto skip;
    keep = esi;
    {
        extern void derive_speeds(void);
        derive_speeds();
    }
    esi = keep;
    cf = 0;
end:
    {
        int saved = cf;
        resync_keys();
        cf = saved;
    }
    return;
skip:
    cf = M32(S_KEY) + 1000u < M32(S_KEY);
    M32(S_KEY) += 1000;
}

static void record_tag(void);
static void record_message(void);
static void record_skip_8a(void);
static void key_restore(void);
static void key_short(void);
static void key_delta(void);
static void key_capture(void);
static void key_long(void);

/* .text+0x1f8c -- apply records to the next frame tag (clear carry) or the
 * end (carry).  A key the last frame tag implied is read first. */
static void parse_records(void)
{
    uint8_t pending = M8(S_PENDING);

    if (pending != 0) {
        M8(S_PENDING) = 0;
        if (pending == 1) {
            key_short();
        } else if (pending == 2) {
            key_delta();
        } else if (pending == 3) {
            key_long();
        }
    }
    for (;;) {
        VIA(S_READ_PEEK);
        if (cf)
            return;
        if (AL <= 0x7f) {
            record_tag();
            continue;
        }
        switch (AL) {
        case 0x80:
        case 0x81:
        case 0x88:
            cf = 0;
            return;
        case 0x82: key_short(); continue;
        case 0x83: key_delta(); continue;
        case 0x86: key_long(); continue;
        case 0x84: record_message(); continue;
        case 0x89: cf = 1; return;
        case 0x8a:
            record_skip_8a();
            VIA(S_READ_SKIP1);
            continue;
        case 0x8b:
            M8(S_KEEP_OLD) = 1;
            VIA(S_READ_SKIP1);
            continue;
        case 0x8c:
            VIA(S_READ_SKIP1);
            continue;
        case 0x8d: key_restore(); continue;
        case 0x8e: key_capture(); continue;
        default:
            if (AL >= 0x90 && AL <= 0x9f) {
                cf = 0;
                return;
            }
            alert(1, 0xf417);
            cf = 1;
            return;
        }
    }
}

static void load_situation(void);

/* .text+0x2061 -- a tagged record.  Tags to 0x2000 are commands, their
 * value in the low nibble; to 0x4000 a command with a word; to 0x6000 a
 * word length and data (0x4003 the airspeed, 0x4004 the vertical speed);
 * above, a dword length (0x6001 a situation).  Commands only apply while
 * GLOBAL +0x74c is set.  Retail's range checks fall through into the next
 * case rather than out, and that is kept: a tag 0x190..0x2000 is read as a
 * word command with bx its tag >> 4, and so on down. */
static void record_tag(void)
{
    uint8_t hi;

    VIA(S_READ_BYTE);
    hi = AL;
    SET_AH(hi);
    VIA(S_READ_BYTE);
    eax = AX;
    edi = G;
    ebx = eax;
    if (AX <= 0x2000) {
        if (M32(edi + 0x74c) == 0) {
            cf = 0;
            return;
        }
        SET_AX(AX & 0xf);
        ebx >>= 4;
        if (ebx < 0x19) {
            uint32_t keep = esi;
            ecx = eax;
            VIA(ebx * 4 + S_HANDLERS);
            esi = keep;
            return;
        }
    }
    if (BX <= 0x4000) {
        VIA(S_READ_WORD);
        if (M32(edi + 0x74c) == 0) {
            cf = 0;
            return;
        }
        ebx -= 0x2001;
        if (ebx < 0x17) {
            uint32_t keep = esi;
            ecx = eax;
            VIA(ebx * 4 + S_WORD_HANDLERS);
            esi = keep;
            cf = 0;
            return;
        }
    }
    if (BX <= 0x6000) {
        VIA(S_READ_WORD);
        eax = AX;
        if (BX == 0x4003) {
            M32(S_SPEED_SET) = 1;
            VIA(S_READ_DWORD);
            M32(edi + 0x2b8) = eax;
            M32(edi + 0x2bc) = eax;
            return;
        }
        if (BX == 0x4004) {
            M32(S_VS_SET) = 1;
            VIA(S_READ_DWORD);
            M32(edi + 0x2c8) = eax;
            return;
        }
        eax -= 2;
        VIA(S_READ_SKIP);
        return;
    }
    VIA(S_READ_WORD);
    eax <<= 16;
    VIA(S_READ_WORD);
    eax = (eax << 16) | (eax >> 16);
    if (BX == 0x6001) {
        uint32_t length = eax;

        VIA(S_READ_SYNC);
        eax = length;
        load_situation();
        VIA(S_READ_SYNC);
        VIA(S_READ_PEEK);
        eax = length;
    }
    eax -= 4;
    VIA(S_READ_SKIP);
}

/* .text+0x2186 -- 0x84: a message, up to 0x37 bytes kept. */
static void record_message(void)
{
    uint32_t keep_ebx = ebx, keep_ecx = ecx;

    VIA(S_READ_SKIP1);
    ebx = 0;
    ecx = 0x37;
    do {
        VIA(S_READ_BYTE);
        if (ecx != 0) {
            M8(ebx + S_MSG) = AL;
            ebx++;
            ecx--;
        }
    } while (AL != 0);
    M8(ebx + S_MSG) = 0;
    ebx = keep_ebx;
    ecx = keep_ecx;
    M32(S_MSG_NEW) = 1;
}

/* .text+0x21bd -- 0x8a: the tag is peeked, not taken, so the length word
 * read has the tag for its low byte (and so is never 0xa). */
static void record_skip_8a(void)
{
    VIA(S_READ_PEEK);
    VIA(S_READ_WORD);
    if (AX == 0xa)
        SET_AX(0x206);
    SET_AX(AX - 2);
    eax = AX;
    VIA(S_READ_SKIP);
}

/* .text+0x21e1 -- 0x6001: a situation, through MAIN, with four GLOBAL
 * flags held at 1 while it loads. */
static void load_situation(void)
{
    uint32_t length = eax - 4;
    uint32_t r[6] = { eax, ebx, ecx, edx, esi, edi };
    uint32_t a, b, c, d;

    edi = G;
    a = M32(edi + 0x1244);
    b = M32(edi + 0x124c);
    c = M32(edi + 0x1250);
    d = M32(edi + 0x1248);
    M32(edi + 0x1244) = 1;
    M32(edi + 0x124c) = 1;
    M32(edi + 0x1250) = 1;
    M32(edi + 0x1248) = 1;
    if (M32(S_PLAY_FILE) != 0)
        MAIN_LOAD_F(M32(S_PLAY_FILE), length);
    else
        MAIN_LOAD(esi);
    M32(edi + 0x1248) = d;
    M32(edi + 0x1250) = c;
    M32(edi + 0x124c) = b;
    M32(edi + 0x1244) = a;
    eax = r[0]; ebx = r[1]; ecx = r[2]; edx = r[3]; esi = r[4]; edi = r[5];
}

/* After a key: 0x8b asked for it to replace the older key too. */
static void keep_old(uint32_t from, uint32_t to, uint32_t words)
{
    uint8_t b = M8(S_KEEP_OLD);

    M8(S_KEEP_OLD) = b >> 1;
    if (b & 1) {
        uint32_t keep = esi;
        esi = from;
        edi = to;
        movs(words, 2);
        ecx = 0;
        esi = keep;
    }
}

/* .text+0x22a8 -- 0x8d: the short key from the output. */
static void key_restore(void)
{
    uint32_t keep_esi, keep_edi;

    VIA(S_READ_SKIP1);
    keep_esi = esi;
    keep_edi = edi;
    edi = G;
    esi = D + 0x5d4;
    edi = D + 0x554;
    movs(0xa, 2);
    ecx = 0;
    M32(S_LONG) = 0;
    {
        uint8_t b = M8(S_KEEP_OLD);
        M8(S_KEEP_OLD) = b >> 1;
        if (b & 1) {
            esi = D + 0x554;
            edi = D + 0x56c;
            movs(0xa, 2);
            ecx = 0;
        }
    }
    edi = keep_edi;
    esi = keep_esi;
}

/* .text+0x22f3 -- 0x82, or a 0x90 frame: a short key in full. */
static void key_short(void)
{
    VIA(S_READ_SKIP1);
    VIA(S_READ_DWORD); M32(D + 0x554) = eax;
    VIA(S_READ_DWORD); M32(D + 0x558) = eax;
    VIA(S_READ_DWORD); M32(D + 0x55c) = eax;
    VIA(S_READ_WORD);  M16(D + 0x560) = AX;
    VIA(S_READ_WORD);  M16(D + 0x562) = AX;
    VIA(S_READ_WORD);  M16(D + 0x564) = AX;
    M32(S_LONG) = 0;
    keep_old(D + 0x554, D + 0x56c, 0xa);
}

/* .text+0x23d7 -- a delta byte: small ones times 16, larger ones through
 * the table at +0x7d9, sign kept. */
static void delta_decode(void)
{
    uint16_t sign;
    int16_t v;
    uint8_t cl, ch;

    v = (int16_t)(int8_t)AL;
    SET_AX(v);
    if ((int8_t)AL <= 0x10 && (int8_t)AL >= -0x10) {
        SET_AX(AX << 4);
    } else {
        sign = (int16_t)AX < 0 ? 0xffff : 0;
        SET_DX(sign);
        SET_AX((uint16_t)((AX ^ sign) - sign));
        ebx = AL;
        ebx >>= 4;
        SET_CX(M16(ebx + S_DELTAS));
        cl = (uint8_t)CX;
        ch = (uint8_t)(CX >> 8);
        ch = (uint8_t)(ch - cl);
        SET_CX((uint16_t)(cl | (ch << 8)));
        ebx <<= 4;
        SET_AL(AL - (uint8_t)ebx);
        SET_AL(AL << 3);
        SET_AX((int16_t)(int8_t)AL * (int16_t)(int8_t)ch);
        SET_AX(AX << 1);
        SET_AH(AH + cl);
        SET_AX((uint16_t)((AX ^ DX) - DX));
    }
    eax = (uint32_t)(int32_t)(int16_t)AX;
}

/* .text+0x2367 -- 0x83, or a 0x94 frame: a short key as deltas. */
static void key_delta(void)
{
    static const uint16_t dwords[3] = { 0x554, 0x558, 0x55c };
    static const uint16_t words[3] = { 0x560, 0x562, 0x564 };
    uint32_t i;

    VIA(S_READ_SKIP1);
    for (i = 0; i < 3; i++) {
        edi = D + dwords[i];
        VIA(S_READ_BYTE);
        delta_decode();
        M32(edi) += eax;
    }
    for (i = 0; i < 3; i++) {
        edi = D + words[i];
        VIA(S_READ_BYTE);
        delta_decode();
        M16(edi) += AX;
    }
    M32(S_LONG) = 0;
}

/* .text+0x2415 -- 0x8e: the long key from GLOBAL's position block. */
static void key_capture(void)
{
    uint32_t keep;

    VIA(S_READ_SKIP1);
    keep = esi;
    edi = G;
    esi = edi + 0x560;
    edi = D + 0x584;
    movs(0x12, 2);
    ecx = 0;
    M32(S_LONG) = 1;
    keep_old(D + 0x584, D + 0x5ac, 0x12);
    esi = keep;
}

/* .text+0x245e -- 0x86, or a 0x98 frame: a long key, and the sequence
 * byte. */
static void key_long(void)
{
    VIA(S_READ_SKIP1);
    VIA(S_READ_WORD);  M16(D + 0x586) = AX;
    VIA(S_READ_DWORD); M32(D + 0x588) = eax;
    VIA(S_READ_WORD);  M16(D + 0x58e) = AX;
    VIA(S_READ_DWORD); M32(D + 0x590) = eax;
    VIA(S_READ_WORD);  M16(D + 0x596) = AX;
    VIA(S_READ_DWORD); M32(D + 0x598) = eax;
    VIA(S_READ_DWORD); M32(D + 0x59c) = eax;
    VIA(S_READ_DWORD); M32(D + 0x5a0) = eax;
    VIA(S_READ_DWORD); M32(D + 0x5a4) = eax;
    M32(S_LONG) = 1;
    VIA(S_READ_BYTE);
    M8(S_SEQ) = AL;
    keep_old(D + 0x584, D + 0x5ac, 0x12);
}

/* .text+0x24fe -- after a frame, while applying: the airspeed from the
 * distance between the keys (unless the video gave it), averaged over five
 * frames and ignored above 0x5dc00, and the vertical speed and the pitch
 * rate.  The square root is three Newton steps from a quick estimate; a
 * zero interval divides by zero, as in retail. */
void derive_speeds(void)
{
    uint32_t dt, sum_lo, sum_hi, dz;
    uint64_t s;

    edi = G;
    if (M32(edi + 0x74c) == 0)
        return;
    {
        uint64_t m = (uint64_t)(M32(S_KEY) - M32(S_KEY_OLD)) * 0x418938u;
        dt = (uint32_t)(m >> 17);
    }
    eax = M32(D + 0x554) - M32(D + 0x56c);
    ecx = (int32_t)eax < 0 ? -eax : eax;
    eax = M32(D + 0x558) - M32(D + 0x570);
    dz = eax;
    ebx = (int32_t)eax < 0 ? -eax : eax;
    eax = M32(D + 0x55c) - M32(D + 0x574);
    eax = (int32_t)eax < 0 ? -eax : eax;
    if (M32(S_SPEED_SET) == 0) {
        uint32_t t;

        s = (uint64_t)eax * eax;
        s += (uint64_t)ebx * ebx;
        s += (uint64_t)ecx * ecx;
        sum_lo = (uint32_t)s;
        sum_hi = (uint32_t)(s >> 32);
        if (eax < ebx) { t = eax; eax = ebx; ebx = t; }
        if (eax < ecx) { t = eax; eax = ecx; ecx = t; }
        if (ebx < ecx) { t = ebx; ebx = ecx; ecx = t; }
        ebx >>= 1;
        ecx >>= 2;
        ebx += eax;
        ebx += ecx;
        if (ebx != 0) {
            int i;

            for (i = 0; i < 3; i++) {
                uint64_t q = (((uint64_t)sum_hi << 32) | sum_lo) / ebx;
                uint64_t a;

                if (q > 0xffffffffu) fault_div();
                eax = (uint32_t)q;
                a = (uint64_t)ebx + eax;
                ebx = (uint32_t)(a >> 1);
            }
        }
        eax = ebx;
        if (dt == 0) fault_div();
        {
            uint64_t n = (uint64_t)eax << 15, q;
            q = n / dt;
            if (q > 0xffffffffu) fault_div();
            eax = (uint32_t)q;
        }
        edi = G;
        if (eax <= 0x5dc00) {
            uint32_t old;

            ebx = M32(D + 0x5f0) + 4;
            if (ebx >= 0x14)
                ebx = 0;
            M32(D + 0x5f0) = ebx;
            ecx = M32(D + 0x5f4) + eax;
            old = M32(ebx + S_RING);
            M32(ebx + S_RING) = eax;
            eax = old;
            ecx -= eax;
            M32(D + 0x5f4) = ecx;
            eax = ecx / 5;
            M32(edi + 0x2b8) = eax;
            M32(edi + 0x2bc) = eax;
        }
    }
    if (M32(S_VS_SET) == 0) {
        int64_t n = (int64_t)(int32_t)dz << 16, q;

        if (dt == 0) fault_div();
        q = n / (int32_t)dt;
        if (q != (int32_t)q) fault_div();
        eax = (uint32_t)q;
        M32(edi + 0x2c8) = eax;
        SET_AX(M16(D + 0x564) - M16(D + 0x57c));
        n = (int64_t)(int32_t)(eax << 16);
        q = n / (int32_t)dt;
        if (q != (int32_t)q) fault_div();
        eax = (uint32_t)q;
        M16(edi + 0x37c) = AX;
    }
}

/* The #DE retail's div takes: a real one, which the compiler cannot drop as
 * it may a C division by zero. */
static void fault_div(void)
{
    __asm__ volatile("xorl %%ecx, %%ecx\n\tdivl %%ecx" : : : "eax", "ecx", "edx");
}

/* ---- the command records' handlers: .text+0x265d..+0x2927 ------------------------ */

/* .text+0x2902 -- CONTROLS +0x44, command eax with value ecx.  CONTROLS
 * leaves ecx as it likes, and the 0x40 handler sends its second command
 * with that: a thunk takes it back. */
uint32_t demo_callee_ecx;
uint32_t FS98_STDCALL demo_call2(uint32_t a, uint32_t b, uint32_t fn);
__asm__(".text\n\t.p2align 4\n"
        ".globl _demo_call2@12\n"
        "_demo_call2@12:\n\t"
        "pushl 8(%esp)\n\t"
        "pushl 8(%esp)\n\t"
        "call *20(%esp)\n\t"
        "movl %ecx, _demo_callee_ecx\n\t"
        "ret $12\n");

static void controls(void)
{
    eax = demo_call2(eax, ecx, M32(CONTROLS + 0x44));
    ecx = demo_callee_ecx;
}

#define TABLE_CMD(name, table) \
    static void name(void) { eax = M32(eax * 4 + D + (table)); controls(); }
#define FIXED_CMD(name, command) \
    static void name(void) { eax = (command); controls(); }

static void h_unknown(void)
{
    M32(S_DIALOG) = 0;
    stop_playing();
    alert(1, 0xf418);
}
TABLE_CMD(h_768, 0x768)
TABLE_CMD(h_770, 0x770)
TABLE_CMD(h_77c, 0x77c)
static void h_770_77c(void)
{
    uint32_t keep = eax;
    h_770();
    eax = keep;
    h_77c();
}
TABLE_CMD(h_788, 0x788)
static void h_6a0(void)
{
    ecx = M32(ecx * 4 + D + 0x6a0);
    eax = 0x1009b;
    controls();
}
FIXED_CMD(h_100ae, 0x100ae)
FIXED_CMD(h_1009d, 0x1009d)
FIXED_CMD(h_100af, 0x100af)
TABLE_CMD(h_79c, 0x79c)
static void h_280(void)
{
    eax = M32(eax * 4 + D + 0x760);
    M8(edi + 0x280) = AL;
    M32(edi + 0x28c) = eax;
}
static void h_36c(void)
{
    eax &= 1;
    eax = M32(eax * 4 + D + 0x760);
    M8(edi + 0x36c) = AL;
    ebx = AL;
    SOUND_SET(8, ebx);
}
FIXED_CMD(h_10034, 0x10034)
static void h_smoke(void)
{
    if (AL != 0)
        SET_AX(0x4e20);
    M16(edi + 0xc00) = AX;
    M16(edi + 0xbc8) = AX;
}
FIXED_CMD(h_100a7, 0x100a7)
FIXED_CMD(h_100a8, 0x100a8)
FIXED_CMD(h_100a9, 0x100a9)
static void h_7b0(void)
{
    eax = 0x100a6;
    ecx = M32(ecx * 4 + D + 0x7b0);
    controls();
}
FIXED_CMD(h_100a4, 0x100a4)
static void h_760(void)
{
    eax = 0x1009c;
    ecx = M32(ecx * 4 + D + 0x760);
    controls();
}
static void h_28c(void)
{
    eax = M32(eax * 4 + D + 0x760);
    M32(edi + 0x28c) = eax;
}
static void h_281(void)
{
    eax = M32(eax * 4 + D + 0x760);
    M8(edi + 0x281) = AL;
}
static void h_lights(void)
{
    SET_BX(AX & 1);
    if (BX != 0)
        SET_BX(0xffff);
    if (AL <= 1)
        M16(edi + 0x8b2) = BX;
    else if (AL <= 3)
        M16(edi + 0x94a) = BX;
    else if (AL <= 5)
        M16(edi + 0x9e2) = BX;
    else if (AL <= 7)
        M16(edi + 0xa7a) = BX;
}
static void h_none(void) { }

FIXED_CMD(w_1009e, 0x1009e)
FIXED_CMD(w_1009f, 0x1009f)
FIXED_CMD(w_100a0, 0x100a0)
static void w_both(void)
{
    uint32_t keep = ecx;
    w_1009f();
    ecx = keep;
    w_100a0();
}
FIXED_CMD(w_100a1, 0x100a1)
static void w_half(void)
{
    ecx >>= 1;
    eax = 0x100a2;
    controls();
}
FIXED_CMD(w_100a2, 0x100a2)
FIXED_CMD(w_100a5, 0x100a5)
FIXED_CMD(w_100b0, 0x100b0)
FIXED_CMD(w_100b1, 0x100b1)
FIXED_CMD(w_100aa, 0x100aa)
FIXED_CMD(w_100ab, 0x100ab)
FIXED_CMD(w_100ac, 0x100ac)
FIXED_CMD(w_100ad, 0x100ad)
FIXED_CMD(w_100b2, 0x100b2)
FIXED_CMD(w_100b3, 0x100b3)
FIXED_CMD(w_100b4, 0x100b4)
FIXED_CMD(w_100b5, 0x100b5)
static void w_c00(void) { M16(edi + 0xc00) = AX; }
static void w_bc8(void) { M16(edi + 0xbc8) = AX; }
static void w_none(void) { }
static void w_37c(void) { M16(edi + 0x37c) = AX; }
static void w_36e(void) { M8(edi + 0x36e) = AL; }

/* The events +0x24 records, tag and value; edx is the value. */
#define EV_WORD(name, tag) \
    static void name(void) { SET_AX(tag); put_tag(); SET_AX(DX); put16(); }
#define EV_BIT(name, tag) \
    static void name(void) { SET_AX((tag) + (DX & 1)); put_tag(); }
static void e_none(void) { }
static void e_none2(void) { }      /* a second ret of retail's own */
EV_WORD(e_200e, 0x200e)
EV_WORD(e_200b, 0x200b)
EV_WORD(e_2006, 0x2006)
EV_BIT(e_130, 0x130)
EV_BIT(e_b0, 0xb0)
EV_WORD(e_200c, 0x200c)
EV_WORD(e_200d, 0x200d)
EV_WORD(e_2010, 0x2010)
EV_WORD(e_2011, 0x2011)
/* These three test +0x11a0 of edi -- the caller's edi, which +0x24 never
 * sets: see demo_event_entry. */
static void e_view(void)
{
    ecx = M32(edi + 0x11a0);
    if (ecx != 0) {
        SET_AX(0x80 + DX);
        put_tag();
    }
}
EV_WORD(e_200f, 0x200f)
static void e_2007(void)
{
    ecx = M32(edi + 0x11a0);
    if (ecx != 0) {
        SET_AX(0x2007); put_tag();
        SET_AX(DX); put16();
    }
}
EV_BIT(e_150, 0x150)
EV_BIT(e_160, 0x160)
EV_BIT(e_170, 0x170)
static void e_mode(void)
{
    ecx = M32(edi + 0x11a0);
    if (ecx == 0)
        return;
    eax = 0x60;
    if (edx != 0) {
        eax = 0x64;
        if (edx != 1) {
            eax = 0x61;
            if (edx != 2) {
                eax = 0x62;
                if (edx != 3) {
                    eax = 0x63;
                    if (edx != 4)
                        return;
                }
            }
        }
    }
    put_tag();
}
EV_BIT(e_140, 0x140)
EV_WORD(e_2017, 0x2017)

/* .text+0x2927 -- +0x24: record event `which` with `value`, both words. */
void FS98_STDCALL demo_event(uint32_t which, uint32_t value);
void FS98_STDCALL demo_event(uint32_t which, uint32_t value)
{
    uint32_t keep_edx;
    SAVE_REGS;

    eax = (uint16_t)which;
    if (eax < 0x14) {
        keep_edx = edx;
        edx = (uint16_t)value;
        VIA(eax * 4 + S_EVENTS);
        edx = keep_edx;
    }
    RESTORE_REGS;
}

/* The entry: retail's +0x24 reads +0x11a0 of whatever edi its caller left,
 * so the caller's edi is taken in here. */
uint32_t demo_caller_edi;
void FS98_STDCALL demo_event_body(uint32_t which, uint32_t value);
void FS98_STDCALL demo_event_body(uint32_t which, uint32_t value)
{
    edi = demo_caller_edi;
    demo_event(which, value);
}
__asm__(".text\n\t.p2align 4\n"
        "_demo_event_entry:\n\t"
        "movl %edi, _demo_caller_edi\n\t"
        "jmp _demo_event_body@8\n");
extern void demo_event_entry(void);

/* ---- inserting into a video: .text+0x2b41, +0x2e64 ------------------------------- */

/* .text+0x2e64 -- copy n bytes from (src at src_off) to (dst at dst_off),
 * 0x200 at a time; 0 or the failing host call's answer through *result. */
static uint32_t FS98_STDCALL file_copy(uint32_t dst, uint32_t dst_off,
                                       uint32_t src, uint32_t src_off,
                                       uint32_t n, uint32_t result)
{
    uint8_t buf[0x200];
    uint32_t chunk, got, wrote;

    eax = n;
    if (n != 0) {
        eax = H_SEEK(dst, dst_off, 0);
        if (eax != 0)
            goto out;
        eax = H_SEEK(src, src_off, 0);
        if (eax != 0)
            goto out;
        do {
            chunk = n <= 0x200 ? n : 0x200;
            eax = H_READ(src, (uint32_t)(uintptr_t)buf, chunk,
                         (uint32_t)(uintptr_t)&got);
            if (eax != 0)
                goto out;
            eax = got;
            if (eax != chunk)
                goto out;
            eax = H_WRITE(dst, (uint32_t)(uintptr_t)buf, got,
                          (uint32_t)(uintptr_t)&wrote);
            if (eax != 0)
                goto out;
            eax = wrote;
            if (eax != chunk)
                goto out;
            n -= chunk;
        } while (n != 0);
    }
    eax = 0;
out:
    if (result != 0)
        M32(result) = eax;
    return eax;
}

/* .text+0x2b41 -- insert n bytes of buf at pos: the tail goes to a
 * temporary file and back after them.  Each step stops at the first host
 * call that fails, leaving things as they are. */
static void FS98_STDCALL file_insert(uint32_t file, uint32_t pos,
                                     uint32_t buf, uint32_t n)
{
    uint32_t keep, size, temp_file, result, wrote, tail, after;
    char dir[0x100], temp[0x104];
    SAVE_REGS;

    if (H_TELL(file, (uint32_t)(uintptr_t)&keep) != 0)
        goto out;
    if (H_SEEK(file, 0, 2) != 0)
        goto out;
    if (H_TELL(file, (uint32_t)(uintptr_t)&size) != 0)
        goto out;
    if (H_LOADSTRING(0x9002, (uint32_t)(uintptr_t)dir, 0x100) == 0)
        goto out;
    if (H_TEMP((uint32_t)(uintptr_t)temp, (uint32_t)(uintptr_t)dir) != 0)
        goto out;
    if (H_CREATE((uint32_t)(uintptr_t)&temp_file, (uint32_t)(uintptr_t)temp,
                 0x1d) != 0)
        goto out;
    if (file_copy(temp_file, 0, file, 0, size, (uint32_t)(uintptr_t)&result) != 0)
        goto out;
    if (H_SEEK(file, pos, 0) != 0)
        goto out;
    if (H_SEEK(temp_file, pos, 0) != 0)
        goto out;
    if (H_WRITE(file, buf, n, (uint32_t)(uintptr_t)&wrote) != 0)
        goto out;
    if (wrote != n)
        goto out;
    tail = size - pos;
    after = pos + n;
    if (file_copy(file, after, temp_file, pos, tail, (uint32_t)(uintptr_t)&result) != 0)
        goto out;
    H_CLOSE(temp_file);
    if (H_DELETE((uint32_t)(uintptr_t)temp, 0x1d) != 0)
        goto out;
    H_SEEK(file, keep, 0);
out:
    RESTORE_REGS;
}

/* ---- sound cues: .text+0x3000..+0x3070 ------------------------------------------- */

static void FS98_STDCALL cue_done(uint32_t h);

/* .text+0x3000 -- a message "%%s:name" plays name.WAV once, at SOUND's
 * volume 4; ignored while one is playing. */
static void sound_cue(uint32_t name)
{
    if (M32(D + 0xf10) != 0)
        return;
    if (H_VOICE_LOAD(name, 2, D + 0xf10) != 0)
        return;
    eax = SOUND_VOLUME(4);
    H_VOICE_VOL(M32(D + 0xf10), eax);
    H_VOICE_PLAY(M32(D + 0xf10), 0, 0.0, (uint32_t)(uintptr_t)cue_done);
}

static void FS98_STDCALL cue_done(uint32_t h)
{
    (void)h;
    H_VOICE_FREE(D + 0xf10);
}

/* ---- instant replay: .text+0x3090..+0x35d2 --------------------------------------- */

/* .text+0x3090 -- CHAIN list 0x14: message 0x21 stops a replay. */
static void FS98_STDCALL replay_message(uint32_t message, uint32_t b)
{
    (void)b;
    if (message == 0x21)
        M32(S_REPLAY_STOP) = 1;
}

static void FS98_STDCALL replay_command(uint32_t command, uint32_t taken);
static void FS98_STDCALL replay_step(uint32_t resync);

/* .text+0x30b0 -- every tick: play the ring from GLOBAL +0x620 while
 * GLOBAL +0x628 says so (2: once the sim is ready, +0x838).  At the newest
 * entry it loops (+0x614) or stops; 0x10001 (+0x68) puts the aircraft back
 * where the replay started. */
static void replay_tick(void)
{
top:
    edi = G;
    if (M32(edi + 0x628) == 0)
        goto cleanup;
    if (M32(edi + 0x620) == 0) {
        M32(S_SPEED_SET) = 0;
        M32(S_VS_SET) = 0;
        M32(S_REPLAY_STOP) = 0;
        if (M32(S_REPLAY_CB) == 0) {
            CHAIN_ADD(0xc, (uint32_t)(uintptr_t)replay_command);
            M32(S_REPLAY_CB) = 1;
        }
        M32(edi + 0x74c) = 1;
        eax = M32(edi + 0x62c);
        M32(edi + 0x61c) = eax;
        eax = eax * 0xffffffe0u + M32(edi + 0x624);
        if (eax < S_REPLAY)
            eax += 0x660;
        M32(edi + 0x620) = eax;
        M32(edi + 0x618) = eax;
        esi = eax;
start:
        replay_step(1);
        eax = M32(S_CLOCK);
        M32(S_OUT) = eax;
        M32(edi + 0x620) = esi;
    }
    if (M32(edi + 0x628) == 2) {
        if (M8(edi + 0x838) != 0)
            return;
        M32(edi + 0x628) = 1;
        M32(S_REPLAY_STOP) = 0;
    }
    if (M32(S_REPLAY_EXIT) != 0) {
        M32(S_REPLAY_EXIT) = 0;
        M32(edi + 0x264) = 1;
        ebx = M32(edi + 0x624) - 0x20;
        if (ebx < S_REPLAY)
            ebx += 0x660;
        M16(edi + 0x562) = M16(ebx + 1);
        M32(edi + 0x564) = M32(ebx + 3);
        M16(edi + 0x56a) = M16(ebx + 7);
        M32(edi + 0x56c) = M32(ebx + 9);
        M16(edi + 0x572) = M16(ebx + 0xd);
        M32(edi + 0x574) = M32(ebx + 0xf);
        M32(edi + 0x578) = M32(ebx + 0x13);
        M32(edi + 0x57c) = M32(ebx + 0x17);
        M32(edi + 0x580) = M32(ebx + 0x1b);
        goto stop;
    }
    if (M32(S_REPLAY_STOP) != 0)
        goto stop;
    M32(edi + 0xd2c) = 0;
    interpolate(edi + 0x55c);
    if (!cf)
        return;
    if (M32(edi + 0x62c) != 0)
        M32(edi + 0x62c)--;
    esi = M32(edi + 0x620);
    if (esi != M32(edi + 0x624) && M8(esi) != 0x89) {
        replay_step(0);
        M32(edi + 0x620) = esi;
        goto top;
    }
    if (M32(edi + 0x614) != 0) {
        M32(edi + 0x62c) = M32(edi + 0x61c);
        esi = M32(edi + 0x618);
        goto start;
    }
    H_POST(0x1403, 0);
    M32(edi + 0x264) = 1;
stop:
    M32(S_REPLAY_STOP) = 0;
    M32(edi + 0x628) = 0;
    M32(edi + 0x62c) = 0;
    M32(edi + 0x614) = 0;
    M32(edi + 0x74c) = 0;
    M32(edi + 0x620) = 0;
cleanup:
    if (M32(S_REPLAY_CB) != 0) {
        CHAIN_REMOVE(0xc, (uint32_t)(uintptr_t)replay_command);
        M32(S_REPLAY_CB) = 0;
    }
    if (M16(edi + 0x18) == 8)
        M16(edi + 0x18) = 0;
}

/* .text+0x3375 -- one ring entry through +0x20, the smoke flag first. */
static void FS98_STDCALL replay_step(uint32_t resync)
{
    uint32_t cursor = esi, done;

    SET_AL(M8(esi + 0x1f));
    if (AL != M8(edi + 0x36c)) {
        M8(edi + 0x36c) = AL;
        ebx = AL;
        SOUND_SET(8, ebx);
    }
    parse_memory((uint32_t)(uintptr_t)&cursor, S_REPLAY_END, resync,
                 (uint32_t)(uintptr_t)&done);
    esi = cursor;
    if (esi >= S_REPLAY_END)
        esi = S_REPLAY;
}

/* .text+0x33e0 -- list 0xc while replaying: 0x10001 leaves it. */
static void FS98_STDCALL replay_command(uint32_t command, uint32_t taken)
{
    SAVE_REGS;

    edi = G;
    if (M32(edi + 0x628) != 0 && M32(taken) == 0 && command == 0x10001) {
        M32(S_REPLAY_EXIT) = 1;
        M32(taken) = 1;
    }
    RESTORE_REGS;
}

/* .text+0x341d -- every frame, unless recording, replaying or in the
 * middle of something (+0x268, +0x365, +0x260 under 2): the position into
 * the ring, an end tag after it. */
static void FS98_STDCALL ring_record(void)
{
    edi = G;
    if (M32(edi + 0x760) != 0 || M32(edi + 0x268) != 0 ||
        M32(edi + 0x628) != 0 || M8(edi + 0x365) != 0 ||
        M32(edi + 0x260) < 2)
        return;
    ebx = M32(edi + 0x624);
    eax = S_REPLAY_END - 0x20;
    if (ebx > eax)
        ebx = S_REPLAY;
    M32(S_REPLAY_N)++;
    M8(ebx) = 0x98;
    SET_AL(M8(edi + 0x36c));
    M8(ebx + 0x1f) = AL;
    M16(ebx + 1) = M16(edi + 0x562);
    M32(ebx + 3) = M32(edi + 0x564);
    M16(ebx + 7) = M16(edi + 0x56a);
    M32(ebx + 9) = M32(edi + 0x56c);
    M16(ebx + 0xd) = M16(edi + 0x572);
    M32(ebx + 0xf) = M32(edi + 0x574);
    M32(ebx + 0x13) = M32(edi + 0x578);
    M32(ebx + 0x17) = M32(edi + 0x57c);
    M32(ebx + 0x1b) = M32(edi + 0x580);
    ebx += 0x20;
    M32(edi + 0x624) = ebx;
    if (ebx >= S_REPLAY_END)
        ebx = S_REPLAY;
    M8(ebx) = 0x89;
    eax = M32(S_REPLAY_N);
    if (eax >= 0x33)
        eax = 0x32;
    M32(edi + 0x630) = eax;
}

/* .text+0x3513 */
static void ring_reset(void)
{
    uint32_t i;

    edi = G;
    M32(edi + 0x624) = S_REPLAY;
    M32(S_REPLAY_N) = 0;
    for (i = 0; i < 0x330; i++)
        M16(S_REPLAY + 2 * i) = 0xffff;
    edi = S_REPLAY + 0x660;
    eax = (eax & 0xffff0000u) | 0xffff;
    ecx = 0;
}

/* .text+0x353f -- +0x3c: the ring entry `index` back (negative) as a
 * position (`out`, 0x18 bytes), its altitude less GLOBAL +0x20's ground.
 * -1 if there is none. */
static uint32_t FS98_STDCALL replay_position(int32_t index, uint32_t out)
{
    uint32_t p, ground_lo, ground_hi;
    uint16_t lo;
    SAVE_REGS;

    edi = G;
    eax = 0xffffffffu;
    if (index >= 0)
        goto out;
    if ((uint32_t)-index >= M32(S_REPLAY_N))
        goto out;
    p = (uint32_t)(index * 0x20) + M32(edi + 0x624);
    if ((int32_t)p < (int32_t)S_REPLAY) {
        p += 0x660;
        if ((int32_t)p < (int32_t)S_REPLAY)
            goto out;
    }
    esi = out;
    M32(esi + 4) = M32(p + 3);
    M16(esi + 2) = M16(p + 1);
    M32(esi + 0xc) = M32(p + 9);
    M16(esi + 0xa) = M16(p + 7);
    edx = M32(p + 0xf);
    lo = M16(p + 0xd);
    ecx = M32(edi + 0x20);
    ground_lo = (ecx << 24) >> 16;
    ground_hi = ecx >> 8;
    {
        uint32_t borrow = lo < (uint16_t)ground_lo;
        lo = (uint16_t)(lo - ground_lo);
        edx = edx - ground_hi - borrow;
    }
    M32(esi + 0x14) = edx;
    M16(esi + 0x12) = lo;
    eax = 0;
out:
    RESTORE_REGS;
    return eax;
}

/* ---- the dialogs' C half: .text+0x35e0.. ------------------------------------------ */

static uint32_t FS98_STDCALL replay_check(uint32_t block);

/* .text+0x3650 -- fill the instant replay dialog's block at +0xe80. */
static uint32_t replay_prepare(void)
{
    uint32_t g = G;

    if (M32(g + 0x758) == 1 || M32(g + 0x760) == 1 ||
        M16(g + 0x18) == 2 || M16(g + 0x18) == 7) {
        M32(D + 0xe88) = 1;
    } else {
        M32(D + 0xe88) = 0;
        if (M8(g + 0x1a) & 2)
            M32(D + 0xe88) = 1;
    }
    if (M32(g + 0x628) != 0) {
        uint32_t keep_esi = esi, keep_edi = edi, keep_ebx = ebx;
        replay_message(0x21, 0);
        replay_tick();
        esi = keep_esi;
        edi = keep_edi;
        ebx = keep_ebx;
        g = G;
    }
    M32(D + 0xeac) = 0;
    M32(D + 0xeb0) = M32(g + 0x634);
    M32(D + 0xecc) = M32(g + 0x614);
    M32(D + 0xec8) = 0;
    M32(D + 0xe94) = M32(g + 0x62c) != 0 ? M32(g + 0x62c) : M32(g + 0x630);
    M32(D + 0xe8c) = M32(g + 0x630);
    M32(D + 0xe90) = 0;
    NUM_FORMAT(D + 0xe70, 0x1001b, D + 0xf18);
    NUM_FORMAT(D + 0xe74, 0x1001b, D + 0xf38);
    NUM_FORMAT(D + 0xe78, 0x1001b, D + 0xf48);
    return 0;
}

/* .text+0x3800 -- start the replay the dialog set up. */
static uint32_t replay_begin(void)
{
    uint32_t g = G;

    M32(g + 0x634) = M32(D + 0xeb0);
    M32(g + 0x614) = M32(D + 0xecc);
    M32(g + 0x628) = 1;
    M32(g + 0x62c) = M32(D + 0xe94);
    M32(g + 0x264) = 0;
    return 0;
}

/* .text+0x35e0 -- +0x30: the instant replay dialog (0x1403); nothing to
 * replay says so (0xf550). */
static uint32_t FS98_STDCALL replay_dialog(void)
{
    if (M32(G + 0x630) == 0) {
        UTIL_ALERT(1, 1, 0xf550, 0, 0, 0, 0, 0);
        return 0;
    }
    eax = replay_prepare();
    if (eax != 0)
        return eax;
    eax = H_DIALOG(0x1403, D + 0xe80);
    if (eax != 0)
        return eax;
    if (M32(D + 0xe80) != 1)
        return eax;
    if (M32(D + 0xe88) != 0)
        return eax;         /* the dialog's 0 */
    return replay_begin();
}

/* .text+0x3750 -- the replay dialog's check: the length (+0x14) within
 * +0xe70..GLOBAL +0x630, the speed (+0x30) within +0xe74..+0xe78, the loop
 * flag (+0x4c) 0 or 1.  Nonzero if anything was out. */
static uint32_t FS98_STDCALL replay_check(uint32_t block)
{
    uint32_t bad = 0;

    M32(block + 0x10) = 0;
    M32(block + 0x2c) = 0;
    M32(block + 0x48) = 0;
    NUM_FORMAT(G + 0x630, 0x1001b, D + 0xf28);
    eax = M32(block + 0x14);
    if (eax < M32(D + 0xe70)) {
        M32(block + 0x10) = 5;
        bad = 1;
    }
    if (eax > M32(G + 0x630)) {
        M32(block + 0x10) = 6;
        bad = 1;
    }
    eax = M32(block + 0x30);
    if (eax < M32(D + 0xe74)) {
        M32(block + 0x2c) = 5;
        bad = 1;
    }
    if (eax > M32(D + 0xe78)) {
        M32(block + 0x2c) = 6;
        bad = 1;
    }
    eax = M32(block + 0x4c);
    if (eax != 1 && eax != 0) {
        M32(block + 0x48) = 0x1005;
        bad = 1;
    }
    return bad;
}

static uint32_t FS98_STDCALL list_check(uint32_t block);
static uint32_t FS98_STDCALL list_delete(uint32_t block);
static uint32_t FS98_STDCALL list_edit(uint32_t block, uint32_t item);
static uint32_t FS98_STDCALL catalogue(uint32_t count, uint32_t data);

/* The video list's 0xa08-byte records: +0 the name shown, +0x100 the title,
 * +0x500 the description, +0x900 the path, +0xa04 writable. */
#define ITEM(data, i) ((data) + 0xa08u * (i))

/* .text+0x3860 -- +0x2c: the video list (0x1404).  Record starts a new
 * recording; play plays the one picked (unless it has gone: 0xf419). */
static uint32_t FS98_STDCALL video_dialog(void)
{
    /* The whole frame is the block: the catalogue's count and list land in
     * its last two fields. */
    struct {
        uint32_t result, check, remove, edit;
        uint32_t play, record, one, x1c, rate, loop, pick, count, data;
    } block;

    if (catalogue((uint32_t)(uintptr_t)&block.count,
                  (uint32_t)(uintptr_t)&block.data) == 0)
        return 0;
    block.check = (uint32_t)(uintptr_t)list_check;
    block.pick = 0;
    block.result = 0;
    block.remove = (uint32_t)(uintptr_t)list_delete;
    block.edit = (uint32_t)(uintptr_t)list_edit;
    block.play = 0;
    block.record = 0;
    block.loop = 0;
    block.x1c = 0;
    block.one = 1;
    block.rate = M32(G + 0x75c);
    H_DIALOG(0x1404, (uint32_t)(uintptr_t)&block);
    M32(G + 0x75c) = block.rate;
    if (block.record != 0) {
        M32(D + 0xee8) = 0;
        M32(D + 0xefc) = 0;
        M8(M32(D + 0xeec)) = 0;
        M8(M32(D + 0xf00)) = 0;
        record_start();
    } else if (block.play != 0) {
        M32(G + 0x750) = block.loop;
        if (H_EXISTS(ITEM(block.data, block.pick) + 0x900, 0x1d) != 0)
            UTIL_ALERT(2, 1, 0xf419, 0, 0, 0, 0, 0);
        else
            play(ITEM(block.data, block.pick) + 0x900, 1);
    }
    H_FREE((uint32_t)(uintptr_t)&block.data);
    return 0;
}

static uint32_t FS98_STDCALL read_title(uint32_t path, uint32_t out);
static uint32_t FS98_STDCALL read_description(uint32_t path, uint32_t out);

/* .text+0x39b0 -- VIDEOS (0x9007) then PILOTS (0x9002), sorted; the video
 * playing is closed round it. */
static uint32_t FS98_STDCALL catalogue(uint32_t count, uint32_t data)
{
    uint32_t r;

    play_file_close();
    r = UTIL_LIST(0x9007, 0x9002, 0x1d, count, data,
                  (uint32_t)(uintptr_t)read_title,
                  (uint32_t)(uintptr_t)read_description, 0, 1);
    play_file_reopen();
    return r;
}

/* .text+0x3a00 -- the list's check: the rate 0..3, the pick in the list. */
static uint32_t FS98_STDCALL list_check(uint32_t block)
{
    uint32_t r = 0;
    int32_t rate = (int32_t)M32(block + 0x20);

    if (rate < 0 || rate > 3)
        r = 0xffffffffu;
    if (M32(block + 0x28) >= M32(block + 0x2c))
        r = 0xffffffffu;
    return r;
}

/* .text+0x3a30 -- delete the picked video, after asking (0xf4e0); not the
 * one playing (0xf4e1).  Answer bit 0x40 is yes. */
static uint32_t FS98_STDCALL list_delete(uint32_t block)
{
    uint32_t i = M32(block + 0x28), n = M32(block + 0x2c), data = M32(block + 0x30);

    if (i >= n || M32(ITEM(data, i) + 0xa04) == 0)
        return 0xffffffffu;
    if (playing_this(ITEM(data, i) + 0x900)) {
        UTIL_ALERT(2, 1, 0xf4e1, 0, 0, 0, 0, 1);
        return 0xffffffffu;
    }
    if (!(UTIL_ALERT(0xc0, 3, 0xf4e0, 0, 0, 0, 0, 1) & 0x40))
        return 0xffffffffu;
    if (H_DELETE(ITEM(data, i) + 0x900, 0x1d) != 0)
        return 0xffffffffu;
    n = M32(block + 0x2c);
    i = M32(block + 0x28);
    if (i < n - 1)
        H_MEMMOVE(ITEM(data, i), ITEM(data, i + 1), (n - i - 1) * 0xa08u);
    M32(block + 0x28) = 0;
    M32(block + 0x2c) = n - 1;
    return 0;
}

/* .text+0x4180 -- is `path` the video playing? */
static uint32_t playing_this(uint32_t path)
{
    return H_STRCMP(path, G + 0x638) == 0;
}

/* .text+0x3b50 -- rename the picked video to item's name (+0) and set its
 * title (+0x100 of item); with no name, only the description (+0x100 on).
 * 0x100d for a bad name or one too long, 0x1004 if it is taken. */
static uint32_t FS98_STDCALL list_edit(uint32_t block, uint32_t item)
{
    char drive[0x100], dir[0x100], name[0x100], path[0x104];
    uint32_t attributes, length, i = M32(block + 0x28), data = M32(block + 0x30);
    uint32_t entry = ITEM(data, i);

    if (i >= M32(block + 0x2c) || M32(entry + 0xa04) == 0)
        return 0xffffffffu;
    H_GET_ATTR((uint32_t)(uintptr_t)&attributes, item + 0x900, 0x1d);
    if (attributes & 2)
        return 0xffffffffu;
    if (M8(item) != 0) {
        if (H_BAD_NAME(item) != 0)
            return 0x100d;
        H_SPLIT(item + 0x900, (uint32_t)(uintptr_t)drive,
                (uint32_t)(uintptr_t)dir, (uint32_t)(uintptr_t)name, 0);
        if (H_JOIN((uint32_t)(uintptr_t)path, (uint32_t)(uintptr_t)drive,
                   (uint32_t)(uintptr_t)dir, item) != 0)
            return 0x100d;
        if (H_PATH_LEN((uint32_t)(uintptr_t)path, 0x1d,
                       (uint32_t)(uintptr_t)&length) != 0)
            return 0x100d;
        if (length >= 0x104)
            return 0x100d;
        if (H_EXISTS((uint32_t)(uintptr_t)path, 0x1d) == 0 &&
            H_STRICMP((uint32_t)(uintptr_t)path, item + 0x900) != 0)
            return 0x1004;
        if (H_MOVE((uint32_t)(uintptr_t)path, 0x1d, entry + 0x900, 0x1d) != 0)
            return 0xffffffffu;
        H_STRCPY(entry + 0x900, (uint32_t)(uintptr_t)path);
        H_STRCPY(entry, item);
        set_title((uint32_t)(uintptr_t)path, item);
        return 0;
    }
    item += 0x100;
    if (M8(item) != 0) {
        set_description(item + 0x800, item);
        H_STRCPY(entry + 0x100, item);
    }
    return 0;
}

/* .text+0x3db0 -- the catalogue's title: header offset 4, if the file
 * starts with tag 0x4001's word 0x140 (it is read low byte first).  1 if the
 * file opened. */
static uint32_t FS98_STDCALL read_title(uint32_t path, uint32_t out)
{
    uint32_t file;
    uint16_t word = 0;

    M8(out) = 0;
    if (H_OPEN((uint32_t)(uintptr_t)&file, path, 0x1d, 0) != 0)
        return 0;
    H_SEEK(file, 0, 0);
    H_READ(file, (uint32_t)(uintptr_t)&word, 2, 0);
    if (word == 0x140) {
        H_SEEK(file, 4, 0);
        if (H_READ(file, out, 0x1f, 0) == 0)
            M8(out + 0x1f) = 0;
    }
    H_CLOSE(file);
    return 1;
}

/* .text+0x3e70 -- the description: 0x100 bytes at 0x27, after tag 0x4002
 * at 0x23. */
static uint32_t FS98_STDCALL read_description(uint32_t path, uint32_t out)
{
    uint32_t file;
    uint16_t word = 0;

    M8(out) = 0;
    if (H_OPEN((uint32_t)(uintptr_t)&file, path, 0x1d, 0) != 0)
        return 0;
    H_SEEK(file, 0x23, 0);
    H_READ(file, (uint32_t)(uintptr_t)&word, 2, 0);
    if (word == 0x240) {
        H_SEEK(file, 0x27, 0);
        H_READ(file, out, 0x100, 0);
    }
    H_CLOSE(file);
    return 1;
}

static uint32_t FS98_STDCALL finished_check(uint32_t block);

/* .text+0x3f20 -- the recording finished (0x14040001): review it, discard
 * it, save it under the name and title the dialog left at +0xeec/+0xf00,
 * or go on. */
static void dialog_finished(void)
{
    M32(D + 0xed0) = 0;
    M32(D + 0xed8) = 0;
    M32(D + 0xedc) = 0;
    M32(D + 0xee0) = 0;
    M32(D + 0xee4) = 0;
    M32(D + 0xed4) = (uint32_t)(uintptr_t)finished_check;
    H_DIALOG(0x14040001, D + 0xed0);
    if (M32(D + 0xee0) != 0) {
        play_recording();
        return;
    }
    if (M32(D + 0xee4) != 0) {
        record_discard();
        return;
    }
    if (M32(D + 0xed8) != 0) {
        record_save(M32(D + 0xeec), M32(D + 0xeec), M32(D + 0xf00));
        return;
    }
    if (M32(D + 0xedc) == 1 || M32(D + 0xed0) == 0)
        record_resume();
}

/* .text+0x3fc0 -- the finished dialog's check of the name at +0x1c: a file
 * name, short enough, and not taken (0x100d, 0x1004). */
static uint32_t FS98_STDCALL finished_check(uint32_t block)
{
    char dir[0x104], path[0x104];
    uint32_t length, r = 0, i;
    uint8_t fill = M8(D + 0x1164);

    for (i = 0; i < 0x104; i++)
        dir[i] = path[i] = 0;
    dir[0] = path[0] = (char)fill;
    M32(block + 0x18) = 0;
    M32(block + 0x2c) = 0;
    if (H_LOADSTRING(0x9002, (uint32_t)(uintptr_t)dir, 0x104) == 0)
        return 0xffffffffu;
    M32(block + 0x18) = 0;
    if (M8(M32(block + 0x1c)) == 0) {
        r = 0xffffffffu;
        M32(block + 0x18) = 0x100d;
    }
    if (H_BAD_NAME(M32(block + 0x1c)) != 0) {
        r = 0xffffffffu;
        M32(block + 0x18) = 0x100d;
    }
    if (H_JOIN((uint32_t)(uintptr_t)path, 0, (uint32_t)(uintptr_t)dir,
               M32(block + 0x1c)) != 0) {
        M32(block + 0x18) = 0x100d;
        return 0xffffffffu;
    }
    if (H_PATH_LEN((uint32_t)(uintptr_t)path, 0x1d, (uint32_t)(uintptr_t)&length) != 0 ||
        length >= 0x104) {
        r = 0xffffffffu;
        M32(block + 0x18) = 0x100d;
    }
    if (r == 0 && H_EXISTS((uint32_t)(uintptr_t)path, 0x1d) == 0) {
        r = 0xffffffffu;
        M32(block + 0x18) = 0x1004;
    }
    return r;
}

/* .text+0x4100 -- a video played from the list finished (0x14040003). */
static void dialog_stopped(void)
{
    H_DIALOG(0x14040003, 0);
}

static uint32_t FS98_STDCALL ask_check(uint32_t block);

/* .text+0x4120 -- while recording, a message to put in (0x14040002). */
static void dialog_ask(void)
{
    struct {
        uint32_t ok, check;
        uint8_t text[0x38];
    } block;

    block.ok = 0;
    block.check = (uint32_t)(uintptr_t)ask_check;
    block.text[0] = 0;
    H_DIALOG(0x14040002, (uint32_t)(uintptr_t)&block);
    if (block.ok != 0)
        demo_message((uint32_t)(uintptr_t)block.text);
}

/* .text+0x4170 */
static uint32_t FS98_STDCALL ask_check(uint32_t block)
{
    (void)block;
    return 0;
}

/* ---- init and deinit: .text+0x20, +0x122 ------------------------------------------ */

static void demo_init(void)
{
    SAVE_REGS;

    CHAIN_ADD(1, (uint32_t)(uintptr_t)demo_tick);
    CHAIN_ADD(0xd, (uint32_t)(uintptr_t)demo_paused);
    CHAIN_ADD(9, (uint32_t)(uintptr_t)demo_paused);
    CHAIN_ADD(0x14, (uint32_t)(uintptr_t)replay_message);
    CHAIN_ADD(3, (uint32_t)(uintptr_t)demo_frame);
    CHAIN_ADD(2, (uint32_t)(uintptr_t)demo_sample);
    ring_reset();
    RESTORE_REGS;
}

static void demo_deinit(void)
{
    SAVE_REGS;

    record_discard();
    M32(S_DIALOG) = 0;
    stop_playing();
    CHAIN_REMOVE(1, (uint32_t)(uintptr_t)demo_tick);
    CHAIN_REMOVE(0xd, (uint32_t)(uintptr_t)demo_paused);
    CHAIN_REMOVE(9, (uint32_t)(uintptr_t)demo_paused);
    CHAIN_REMOVE(0x14, (uint32_t)(uintptr_t)replay_message);
    CHAIN_REMOVE(3, (uint32_t)(uintptr_t)demo_frame);
    CHAIN_REMOVE(2, (uint32_t)(uintptr_t)demo_sample);
    RESTORE_REGS;
}

/* ---- the image ------------------------------------------------------------------------ */

struct demo_image demo_image = {{
#include "demo_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _demo_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _demo_image + 0x50");

static void demo_install(void)
{
    static void (*const handlers[0x19])(void) = {
        h_unknown, h_768, h_770, h_77c, h_770_77c, h_788, h_6a0, h_100ae,
        h_1009d, h_100af, h_79c, h_280, h_36c, h_10034, h_smoke, h_100a7,
        h_100a8, h_100a9, h_7b0, h_100a4, h_760, h_28c, h_281, h_lights,
        h_none
    };
    static void (*const word_handlers[0x17])(void) = {
        w_1009e, w_1009f, w_100a0, w_both, w_100a1, w_half, w_100a5, w_100b0,
        w_100b1, w_100aa, w_100ab, w_100ac, w_100ad, w_100b2, w_100b3,
        w_100b4, w_100b5, w_c00, w_bc8, w_none, w_37c, w_36e, w_100a2
    };
    static void (*const events[0x14])(void) = {
        e_none, e_200e, e_200b, e_2006, e_130, e_b0, e_200c, e_200d, e_2010,
        e_2011, e_view, e_200f, e_2007, e_150, e_160, e_170, e_none2, e_mode,
        e_140, e_2017
    };
    uint32_t i;

    for (i = 0; i < 0x19; i++)
        M32(S_HANDLERS + 4 * i) = (uint32_t)(uintptr_t)handlers[i];
    for (i = 0; i < 0x17; i++)
        M32(S_WORD_HANDLERS + 4 * i) = (uint32_t)(uintptr_t)word_handlers[i];
    for (i = 0; i < 0x14; i++)
        M32(S_EVENTS + 4 * i) = (uint32_t)(uintptr_t)events[i];
    /* As retail's image starts: the memory readers, and the file open. */
    readers_memory();
    M32(S_READ_OPEN) = (uint32_t)(uintptr_t)f_open;
    M32(D + 0xe84) = (uint32_t)(uintptr_t)replay_check;
    M32(D + 0xea4) = D + 0xf18;
    M32(D + 0xea8) = D + 0xf28;
    M32(D + 0xec0) = D + 0xf38;
    M32(D + 0xec4) = D + 0xf48;
    M32(D + 0xed4) = (uint32_t)(uintptr_t)finished_check;
    M32(D + 0xeec) = D + 0xf58;
    M32(D + 0xf00) = D + 0x1060;
    M32(D + DEMO_LINKAGE + 0x04) = (uint32_t)(uintptr_t)demo_init;
    M32(D + DEMO_LINKAGE + 0x08) = (uint32_t)(uintptr_t)demo_deinit;
    M32(D + DEMO_LINKAGE + 0x18) = (uint32_t)(uintptr_t)demo_play_frame;
    M32(D + DEMO_LINKAGE + 0x1c) = (uint32_t)(uintptr_t)interpolate;
    M32(D + DEMO_LINKAGE + 0x20) = (uint32_t)(uintptr_t)parse_memory;
    M32(D + DEMO_LINKAGE + 0x24) = (uint32_t)(uintptr_t)demo_event_entry;
    M32(D + DEMO_LINKAGE + 0x28) = (uint32_t)(uintptr_t)play;
    M32(D + DEMO_LINKAGE + 0x2c) = (uint32_t)(uintptr_t)video_dialog;
    M32(D + DEMO_LINKAGE + 0x30) = (uint32_t)(uintptr_t)replay_dialog;
    M32(D + DEMO_LINKAGE + 0x34) = (uint32_t)(uintptr_t)demo_stop;
    M32(D + DEMO_LINKAGE + 0x38) = (uint32_t)(uintptr_t)catalogue;
    M32(D + DEMO_LINKAGE + 0x3c) = (uint32_t)(uintptr_t)replay_position;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        demo_install();
    }
    return TRUE;
}
