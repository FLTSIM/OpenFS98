#ifndef FS98_WORLD_SEARCH_H
#define FS98_WORLD_SEARCH_H
#include "world_internal.h"
/* Drive-search UI boundary. Owner callbacks are stdcall(descriptor), with
 * ignored returns. This class embeds its area row at descriptor +0x20. */
struct fs98_world_search_services {
    const struct fs98_world_services *ui;
    void (*title)(void *control, const char *text);
    int (*post)(uint32_t window, uint32_t message, uint32_t first, uint32_t second);
};
void fs98_world_search_set_services(const struct fs98_world_search_services *services);
void fs98_world_search_collect(void *object);
void fs98_world_search_populate(void *object);
void fs98_world_search_add(void *object);
void fs98_world_search_add_all(void *object);
void fs98_world_search_next(void *object);
void fs98_world_search_next_dir(void *object);
void fs98_world_search_scan(void *object);
void fs98_world_search_drive(void *object);
void fs98_world_search_layer(void *object);
void fs98_world_search_cancel(void *object);
int fs98_world_search_validate(void *object);
#endif
