#ifndef FS98_WORLD_EXCEPTIONS_H
#define FS98_WORLD_EXCEPTIONS_H
#include "world_unwind.h"
#include <stdint.h>
/* MSVC x86 ThrowInfo/CatchableType ABI. The area factories catch CException*,
 * including derived pointers whose catchable-type list names that base. */
static int world_exception_pointer(EXCEPTION_RECORD *record, void **exception_out)
{
    uint32_t *throw_info, *types;
    uint32_t index;
    if (record->ExceptionCode != 0xe06d7363 || record->NumberParameters < 3 ||
        record->ExceptionInformation[0] != 0x19930520)
        return 0;
    throw_info = (uint32_t *)record->ExceptionInformation[2];
    types = (uint32_t *)throw_info[3];
    for (index = 0; index < types[0]; ++index) {
        uint32_t *type = (uint32_t *)types[index + 1];
        const char *name = (const char *)type[1] + 8;
        const char *expected_text = ".PAVCException@@";
        uint8_t *exception_bytes;
        while (*name && *name == *expected_text) { ++name; ++expected_text; }
        if (*name || *expected_text) continue;
        if (!(type[0] & 1) || type[5] != 4) continue;
        exception_bytes = *(uint8_t **)record->ExceptionInformation[1];
        if (exception_bytes) {
            if ((int)type[3] >= 0) {
                uint8_t *virtual_base_table = *(uint8_t **)(exception_bytes + type[3]);
                exception_bytes += type[3] + *(int *)(virtual_base_table + type[4]);
            }
            exception_bytes += (int)type[2];
        }
        *exception_out = exception_bytes;
        return 1;
    }
    return 0;
}
#endif
