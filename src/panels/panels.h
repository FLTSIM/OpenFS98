/* PANELS.DLL -- what the parts of panels.c share.  See panels.c. */
#ifndef OPENFS98_PANELS_H
#define OPENFS98_PANELS_H

#include <stdint.h>

#include "fs98/host.h"

#define PN_DATA_VA    0x1000f000u   /* retail .data */
#define PN_IMAGE_SIZE 0x16580       /* .data's virtual size */
#define PN_INIT_SIZE  0x1e00        /* .data's initialised part */
#define PN_LINKAGE    0x050
#define PN_RDATA_SIZE 0x1fb

typedef volatile uint8_t  pn_v8;
typedef volatile uint16_t __attribute__((may_alias)) pn_v16;
typedef volatile uint32_t __attribute__((may_alias)) pn_v32;
typedef volatile float    __attribute__((may_alias)) pn_vf;
typedef volatile double   __attribute__((may_alias)) pn_vd;

struct panels_image {
    uint8_t bytes[PN_IMAGE_SIZE];
} __attribute__((aligned(16)));
extern struct panels_image panels_image;
extern const uint8_t panels_rdata[PN_RDATA_SIZE];

#define D       ((uint32_t)(uintptr_t)panels_image.bytes)
#define RDATA   ((uint32_t)(uintptr_t)panels_rdata)
#define M8(a)   (*(pn_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(pn_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(pn_v32 *)(uintptr_t)(uint32_t)(a))
#define MF(a)   (*(pn_vf  *)(uintptr_t)(uint32_t)(a))
#define MD(a)   (*(pn_vd  *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))
#define P(x)    ((uint32_t)(uintptr_t)(x))

/* retail's .data and .rdata addresses, as ours */
#define DA(va)  (D + ((uint32_t)(va) - PN_DATA_VA))
#define D8(va)  M8(DA(va))
#define D16(va) M16(DA(va))
#define D32(va) M32(DA(va))
#define RD(va)  MD(RDATA + ((uint32_t)(va) - 0x1000e000u))     /* an .rdata double */
#define RF(va)  MF(RDATA + ((uint32_t)(va) - 0x1000e000u))     /* an .rdata float */

/* a record's fields */
#define R8(p, o)  M8((uint32_t)(p) + (o))
#define R16(p, o) M16((uint32_t)(p) + (o))
#define R32(p, o) M32((uint32_t)(p) + (o))
#define RFL(p, o) MF((uint32_t)(p) + (o))
#define RDB(p, o) MD((uint32_t)(p) + (o))

#define IMPORT(i)  M32(D + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define ACONTAIN   IMPORT(2)
#define CHAIN      IMPORT(3)
#define HG2D       IMPORT(4)
#define GAUGES     IMPORT(5)        /* never read */
#define MAIN       IMPORT(6)
#define SOUND      IMPORT(7)
#define WINDOW     IMPORT(8)

#define G8(o)  M8(G + (o))
#define G16(o) M16(G + (o))
#define G32(o) M32(G + (o))

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HC(off, fn)  CALL(HOST, off, fn)
#define CH(off, fn)  CALL(CHAIN, off, fn)
#define HG(off, fn)  CALL(HG2D, off, fn)
#define WN(off, fn)  CALL(WINDOW, off, fn)
#define MN(off, fn)  CALL(MAIN, off, fn)
#define SN(off, fn)  CALL(SOUND, off, fn)
#define AC(off, fn)  CALL(ACONTAIN, off, fn)

#endif /* OPENFS98_PANELS_H */
