#ifndef FS98_WORLD_EXCEPTIONS_H
#define FS98_WORLD_EXCEPTIONS_H
#include "world_unwind.h"
#include "types.h"
/* MSVC x86 ThrowInfo/CatchableType ABI. The area factories catch CException*,
 * including derived pointers whose catchable-type list names that base. */
static int world_exception_pointer(EXCEPTION_RECORD *pRecord, void **ppException)
{
    uint32 *puiThrowInfo, *puiTypes;
    uint32 uiIndex;
    if (pRecord->ExceptionCode != 0xe06d7363 || pRecord->NumberParameters < 3 ||
        pRecord->ExceptionInformation[0] != 0x19930520)
        return 0;
    puiThrowInfo = (uint32 *)pRecord->ExceptionInformation[2];
    puiTypes = (uint32 *)puiThrowInfo[3];
    for (uiIndex = 0; uiIndex < puiTypes[0]; ++uiIndex) {
        uint32 *puiType = (uint32 *)puiTypes[uiIndex + 1];
        const char *pcName = (const char *)puiType[1] + 8;
        const char *pcExpected = ".PAVCException@@";
        uint8 *pbyException;
        while (*pcName && *pcName == *pcExpected) { ++pcName; ++pcExpected; }
        if (*pcName || *pcExpected) continue;
        if (!(puiType[0] & 1) || puiType[5] != 4) continue;
        pbyException = *(uint8 **)pRecord->ExceptionInformation[1];
        if (pbyException) {
            if ((int)puiType[3] >= 0) {
                uint8 *pbyVtable = *(uint8 **)(pbyException + puiType[3]);
                pbyException += puiType[3] + *(int *)(pbyVtable + puiType[4]);
            }
            pbyException += (int)puiType[2];
        }
        *ppException = pbyException;
        return 1;
    }
    return 0;
}
#endif
