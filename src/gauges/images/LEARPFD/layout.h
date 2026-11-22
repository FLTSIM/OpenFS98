/* LEARPFD.GAU's layout in retail: the .data and .rdata sections (RVA and virtual size), the image base, and
 * the offsets of the ImportTable and Linkage exports from the start of .data.  Read by src/gauges/gauge.h. */
#define GAUGE_DATA_RVA 0x3000u
#define GAUGE_DATA_SIZE 0x2b34u
#define GAUGE_RDATA_RVA 0x2000u
#define GAUGE_RDATA_SIZE 0x110u
#define GAUGE_BASE 0x219f0000u
#define GAUGE_IMPORT_OFFSET 0x2ae8
#define GAUGE_LINKAGE_OFFSET 0x2af8
