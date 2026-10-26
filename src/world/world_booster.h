#ifndef FS98_WORLD_BOOSTER_H
#define FS98_WORLD_BOOSTER_H
#include "world_internal.h"
/* Shared event core for 0x13080007/8. Native registration is still pending. */
struct fs98_world_booster_services {
    const struct fs98_world_services *ui;
    void (*title)(void *control, const char *text);
};
void fs98_world_booster_set_services(const struct fs98_world_booster_services *services);
void fs98_world_booster_collect(void *object);
void fs98_world_booster_populate(void *object);
void fs98_world_booster_accept(void *object);
void fs98_world_booster_path_changed(void *object);
int fs98_world_booster_validate(void *object, uint32_t unused);
int fs98_world_booster_error(void *object, uint32_t unused);
#endif
