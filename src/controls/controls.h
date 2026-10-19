/* CONTROLS.DLL -- what controls.c (the shell and the hand-written assembly)
 * and controls_input.c (the compiled C: profile, joysticks, DirectInput)
 * share.  See controls.c for the module. */
#ifndef OPENFS98_CONTROLS_H
#define OPENFS98_CONTROLS_H

#include <stdint.h>

#include "fs98/host.h"

#define CT_IMAGE_SIZE 0xc020
#define CT_IMPORTS    0x000
#define CT_LINKAGE    0x078
#define CT_RDATA_SIZE 0x2b8
#define CT_COMMANDS   383

typedef volatile uint8_t  ct_v8;
typedef volatile uint16_t __attribute__((may_alias)) ct_v16;
typedef volatile uint32_t __attribute__((may_alias)) ct_v32;
typedef volatile double   __attribute__((may_alias)) ct_vd;

struct controls_image {
    uint8_t bytes[CT_IMAGE_SIZE];
};
extern struct controls_image controls_image;
extern uint8_t controls_rdata[CT_RDATA_SIZE];

/* a command: what retail keeps at .text+0x15f9 */
struct controls_command {
    void (*handler)(void);
    uint32_t kind;          /* a mask of the key tables it may be bound in */
    uint32_t command;
    uint32_t sid;           /* its display name's string id (ENGLISH.FLL), 0 if unlisted */
    const char *name;
};
extern struct controls_command controls_commands[CT_COMMANDS];
extern struct controls_command *const controls_table;     /* .text+0x15f1 */
extern const uint32_t controls_count;                      /* .text+0x15f5 */

#define D       ((uint32_t)(uintptr_t)controls_image.bytes)
#define RDATA   ((uint32_t)(uintptr_t)controls_rdata)
#define M8(a)   (*(ct_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(ct_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(ct_v32 *)(uintptr_t)(uint32_t)(a))
#define MD(a)   (*(ct_vd  *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

#define IMPORT(i)  M32(D + CT_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define ACONTAIN   IMPORT(2)
#define ADVDRV     IMPORT(3)
#define ATC        IMPORT(4)
#define CHAIN      IMPORT(5)
#define DEMO       IMPORT(6)
#define HG2D       IMPORT(7)
#define MAIN       IMPORT(8)
#define NUMERIC    IMPORT(9)
#define PANELS     IMPORT(10)
#define SOUND      IMPORT(11)
#define UTIL       IMPORT(12)
#define WINDOW     IMPORT(13)

#define SVC(m, off) M32((m) + (off))

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f6)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f7)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))

/* ---- the hand-written half's registers -------------------------------------
 *
 * The assembly passes values in registers between its own routines and
 * leaves them in registers its callers read, in both directions: DEMO's
 * +0x24 reads +0x11a0 of the edi it is called with, and DEMO reads the ecx
 * CONTROLS +0x44 leaves.  So that half runs on a model of the registers,
 * every import call it makes goes through controls_xcall (which loads the
 * model into the machine's registers and takes back what the callee left),
 * and every entry into it goes through a stub that does the reverse. */
struct controls_regs {
    uint32_t eax, ecx, edx, ebx, esi, edi;
};
extern struct controls_regs controls_regs;

#define eax controls_regs.eax
#define ecx controls_regs.ecx
#define edx controls_regs.edx
#define ebx controls_regs.ebx
#define esi controls_regs.esi
#define edi controls_regs.edi

#define AX ((uint16_t)eax)
#define BX ((uint16_t)ebx)
#define CX ((uint16_t)ecx)
#define DX ((uint16_t)edx)
#define AL ((uint8_t)eax)
#define AH ((uint8_t)(eax >> 8))
#define BL ((uint8_t)ebx)
#define BH ((uint8_t)(ebx >> 8))
#define CL ((uint8_t)ecx)
#define CH ((uint8_t)(ecx >> 8))
#define DL ((uint8_t)edx)
#define DH ((uint8_t)(edx >> 8))
#define SET_LO(r, v) ((r) = ((r) & 0xffff0000u) | (uint16_t)(v))
#define SET_B0(r, v) ((r) = ((r) & 0xffffff00u) | (uint8_t)(v))
#define SET_B1(r, v) ((r) = ((r) & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_AX(v) SET_LO(eax, v)
#define SET_BX(v) SET_LO(ebx, v)
#define SET_CX(v) SET_LO(ecx, v)
#define SET_DX(v) SET_LO(edx, v)
#define SET_SI(v) SET_LO(esi, v)
#define SET_DI(v) SET_LO(edi, v)
#define SET_AL(v) SET_B0(eax, v)
#define SET_AH(v) SET_B1(eax, v)
#define SET_BL(v) SET_B0(ebx, v)
#define SET_BH(v) SET_B1(ebx, v)
#define SET_CL(v) SET_B0(ecx, v)
#define SET_CH(v) SET_B1(ecx, v)
#define SET_DL(v) SET_B0(edx, v)
#define SET_DH(v) SET_B1(edx, v)

/* call `fn` (stdcall or cdecl) with n dword arguments, from the model's
 * registers; the model gets back eax, ecx, edx, ebx, esi and edi */
uint32_t controls_xcall(uint32_t fn, uint32_t n, const uint32_t *args);
#define XARGS(...) ((const uint32_t[]){ __VA_ARGS__ })
#define XCALL(fn, ...) controls_xcall((fn), sizeof(XARGS(__VA_ARGS__)) / 4, \
                                      XARGS(__VA_ARGS__))
#define XCALL0(fn) controls_xcall((fn), 0, 0)

/* .text+0x6f93 -- Linkage +0x40, in the hand-written half */
void controls_event(uint32_t which, uint32_t value);

#endif
