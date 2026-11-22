/* BKROPEN.GAU's relocated .data slots, in retail's relocation order.  GAUGE_REBIND(off, rva): the slot at +off
 * (from the start of .data) holds the image base plus rva, an RVA in .data, .rdata or .text; attach stores the address
 * of the same place in this image (src/gauges/gauge.h, with the hand-written gauge's routines for the code ones). */
GAUGE_REBIND(0x24, 0x3000)
GAUGE_REBIND(0x28, 0x30c0)
GAUGE_REBIND(0x2c, 0x1110)
GAUGE_REBIND(0x30, 0x1030)
GAUGE_REBIND(0x34, 0x1080)
GAUGE_REBIND(0x38, 0x1090)
GAUGE_REBIND(0x3c, 0x10a0)
GAUGE_REBIND(0x40, 0x10c0)
GAUGE_REBIND(0x44, 0x10d0)
GAUGE_REBIND(0x90, 0x3020)
GAUGE_REBIND(0xc0, 0x3070)
GAUGE_REBIND(0x118, 0x1020)
GAUGE_REBIND(0x18c, 0x1100)
GAUGE_REBIND(0x190, 0x1100)
GAUGE_REBIND(0x1a0, 0x3020)
