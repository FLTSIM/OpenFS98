#ifndef FS98_MDL_NORMAL_H
#define FS98_MDL_NORMAL_H

#include <stdint.h>
#include "fs98/host.h"

enum fs98_mdl_kind {
    FS98_MDL_BOEING, FS98_MDL_BELL, FS98_MDL_C182RG, FS98_MDL_C182,
    FS98_MDL_EXTRA, FS98_MDL_LEAR, FS98_MDL_SCHWEIZER, FS98_MDL_SOPWITH
};

/* image is addressed by retail RVA, including mutable program data. The
 * module wrapper and generated data supply the relocated header/Linkage. */
void fs98_mdl_normal_update(enum fs98_mdl_kind kind, uint8_t *state,
                           const uint8_t *global, uint8_t *image,
                           fs98_host_memmove_fn copy);
#endif
