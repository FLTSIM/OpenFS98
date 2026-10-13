/* Field offsets into GLOBAL's shared state block.
 *
 * GLOBAL (module id 2) publishes no services.  Its Linkage pointer is the base
 * of a 4776 byte block that every other module reads and writes by byte
 * offset, so recovering what those offsets mean is the shared, cumulative task
 * of this project: each module reversed names a few more of them.
 *
 * Only offsets actually established from a disassembly belong here.  The
 * module each one was learned from is named, so a wrong guess can be traced
 * back and re-argued.
 */
#ifndef FS98_GLOBAL_H
#define FS98_GLOBAL_H

#include <stdint.h>

/* Accessors.  Offsets stay as raw numbers on purpose: an offset seen in a
 * disassembly listing should be greppable against this header verbatim. */
#define FS98_G_U8(g, off)   (*(uint8_t  *)((unsigned char *)(g) + (off)))
#define FS98_G_S16(g, off)  (*(int16_t  *)((unsigned char *)(g) + (off)))
#define FS98_G_U16(g, off)  (*(uint16_t *)((unsigned char *)(g) + (off)))
#define FS98_G_S32(g, off)  (*(int32_t  *)((unsigned char *)(g) + (off)))
#define FS98_G_PTR(g, off)  (*(void *   *)((unsigned char *)(g) + (off)))

/* Aircraft position is a 48 bit fixed-point quantity stored as a 16 bit low
 * half followed by a 32 bit high half, and updated with add/adc pairs.  The
 * high half of altitude is metres: SLEW clamps it to 30480, which is 100000
 * feet exactly. */
enum {
    /* --- from SLEW.DLL --- */
    FS98_G_GROUND_ALT     = 0x0020,  /* s32, terrain height under the aircraft */
    FS98_G_TIME_SCALE     = 0x02aa,  /* s16, per-tick rate scalar; 0x1fff default */
    FS98_G_GROUNDSPEED_A  = 0x02b8,  /* s32, written together with ..._B      */
    FS98_G_GROUNDSPEED_B  = 0x02bc,  /* s32                                   */
    FS98_G_VERT_SPEED     = 0x02c8,  /* s32, clamped to 0x7fff                */
    FS98_G_SLEW_INACTIVE  = 0x0365,  /* u8, nonzero means do not slew         */
    FS98_G_MODE           = 0x0390,  /* s32, gates the fast-slew check        */

    FS98_G_POS_BASE       = 0x0560,  /* passed by address to UTIL slot 0x44   */
    FS98_G_LAT_LO         = 0x0562,  /* u16 \_ 48 bit latitude                */
    FS98_G_LAT_HI         = 0x0564,  /* s32 /                                 */
    FS98_G_LON_LO         = 0x056a,  /* u16 \_ 48 bit longitude               */
    FS98_G_LON_HI         = 0x056c,  /* s32 /                                 */
    FS98_G_ALT_LO         = 0x0572,  /* u16 \_ 48 bit altitude, high is metres*/
    FS98_G_ALT_HI         = 0x0574,  /* s32 /                                 */

    FS98_G_PITCH          = 0x0578,  /* s32, wraps at +/-0x40000000           */
    FS98_G_BANK           = 0x057c,  /* s32                                   */
    FS98_G_HEADING        = 0x0580,  /* s32; 0x582 is its high word           */
    FS98_G_HEADING_HI     = 0x0582,  /* s16, heading as a 16 bit angle        */

    FS98_G_ATT_RESET      = 0x05de,  /* s16; only the LOW BYTE asks to recentre*/
    FS98_G_RATE_BANK      = 0x05e4,  /* s16, zero centred                     */
    FS98_G_RATE_HEADING   = 0x05e6,  /* s16, zero centred                     */
    FS98_G_RATE_VERT      = 0x05e8,  /* s16, centred on 0x4000                */
    FS98_G_ATT_ZERO       = 0x05e0,  /* s32, xchg'd to 0; nonzero zeroes attitude */
    FS98_G_RATE_FWD       = 0x05ea,  /* s16; SLEW uses only its high byte     */
    FS98_G_RATE_SIDE      = 0x05ec,  /* s16; SLEW uses only its high byte     */
    FS98_G_RATE_PITCH     = 0x05ee,  /* s16, centred on 0x4000                */
    FS98_G_SPEED_OUT      = 0x05f8,  /* s32, speed magnitude SLEW publishes   */

    FS98_G_AIRCRAFT       = 0x11a0,  /* pointer; +0x10 and +0x26 gate fast slew */
    FS98_G_ALT_OFFSET     = 0x120c   /* s32, added to ground alt for the floor */
};

#endif /* FS98_GLOBAL_H */
