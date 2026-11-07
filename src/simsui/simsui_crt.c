/* The compiler's helpers SIMSUI carries. */
#include "simsui_internal.h"

/* 0x20483180, `eh vector destructor iterator': each of count elements, last first. While
 * it runs, an exception from a destructor ends the rest (0x20483230) and terminates on a
 * C++ one; nothing here throws, so that path is left to the runtime. */
void SC fs98_simsui_vector_destroy(void *array, uint32_t size, int32_t count, void (TC *destroy)(void *))
{
    uint8_t *element = (uint8_t *)array + size * (uint32_t)count;
    while (--count >= 0) {
        element -= size;
        destroy(element);
    }
}
/* 0x20483310, `eh vector constructor iterator': each of count elements, first first. Were a
 * constructor to throw, the ones made would be destroyed (0x20483388); none here throws. */
void SC fs98_simsui_vector_construct(void *array, uint32_t size, int32_t count, void *(TC *construct)(void *),
                                     void (TC *destroy)(void *))
{
    uint8_t *element = array;
    int32_t at;
    (void)destroy;
    for (at = 0; at < count; ++at) {
        construct(element);
        element += size;
    }
}
