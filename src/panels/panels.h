/* PANELS.DLL -- what the parts of panels.c share.  See panels.c. */
#ifndef OPENFS98_PANELS_H
#define OPENFS98_PANELS_H

#include <stdint.h>

#include "fs98/host.h"

#define PN_DATA_VA    0x1000f000u   /* retail .data */
#define PN_IMAGE_SIZE 0x16580       /* .data's virtual size */
#define PN_LINKAGE    0x050

struct panels_image;          /* panels.c, with the typed layout */
struct panels_rdata;
extern struct panels_image panels_image;
extern const struct panels_rdata panels_rdata;
#define FS98_DATA_IMAGE panels_image
#define FS98_RDATA_IMAGE panels_rdata
#define FS98_DATA_VA PN_DATA_VA
#include "fs98/arena.h"

/* retail's .data and .rdata addresses, as ours */
#define D8(va)  M8(DA(va))
#define D16(va) M16(DA(va))
#define D32(va) M32(DA(va))
#define PN_RD(va)  MD(RDATA + ((uint32_t)(va) - 0x1000e000u))     /* an .rdata double */
#define PN_RF(va)  MF(RDATA + ((uint32_t)(va) - 0x1000e000u))     /* an .rdata float */

#define IMPORT(i)  M32(D + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define GLOBAL     IMPORT(1)
#define ACONTAIN   IMPORT(2)
#define CHAIN      IMPORT(3)
#define HG2D       IMPORT(4)
#define GAUGES     IMPORT(5)        /* never read */
#define MAIN       IMPORT(6)
#define SOUND      IMPORT(7)
#define WINDOW     IMPORT(8)

#define G8(o)  M8(GLOBAL + (o))
#define G16(o) M16(GLOBAL + (o))
#define G32(o) M32(GLOBAL + (o))

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
