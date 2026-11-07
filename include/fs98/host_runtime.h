/* Optional executable observation interface for the deterministic game driver.
 * A retail executable has no such export; tools retain its original IAT path.
 */
#ifndef FS98_HOST_RUNTIME_OBSERVER_H
#define FS98_HOST_RUNTIME_OBSERVER_H
#include <stdint.h>
struct fs98_host_runtime {
    uint32_t version;
    uint8_t *image;
    void *(*patch_import)(const char *name, void *replacement);
    void (*main_loop_pass)(void);
};
#endif
