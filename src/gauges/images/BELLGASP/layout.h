/* BELLGASP.GAU's layout in retail: the .data and .rdata sections (RVA and virtual size), the image base, and
 * the offsets of the ImportTable and Linkage exports from the start of .data.  Read by src/gauges/gauge.h. */
#define GAUGE_DATA_RVA 0x3000u
#define GAUGE_DATA_SIZE 0x388u
#define GAUGE_RDATA_RVA 0x2000u
#define GAUGE_RDATA_SIZE 0x8bu
#define GAUGE_BASE 0x20e10000u
#define GAUGE_IMPORT_OFFSET 0x348
#define GAUGE_LINKAGE_OFFSET 0x358
