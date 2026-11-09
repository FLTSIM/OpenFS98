/* Helpers over the game's windows. */
#include "host_rt.h"

typedef int32_t (SC *window_callback)(uint32_t, uint32_t);
struct window_visit { window_callback callback; uint32_t parameter; };

/* 0x4078b0: EnumThreadWindows's callback: a floating window ("FS98FLOAT") is visited. */
int32_t SC fs98_host_rt_visit_floating(uint32_t window, struct window_visit *visit)
{
    char name[0x100];
    CALL_STD(GetClassNameA, int32_t, (uint32_t, char *, int32_t))(window, name, 0xff);
    if (CALL_C(_strcmpi, int32_t, (const char *, const char *))(name, STR(0x417178)) == 0)
        visit->callback(window, visit->parameter);
    return 1;
}

/* 0x407850: call back for every child of a window, then for the thread's floating windows. */
void SC fs98_host_rt_for_each_window(uint32_t window, window_callback callback, uint32_t parameter)
{
    struct window_visit visit;
    if (window == 0 || !CALL_STD(IsWindow, int32_t, (uint32_t))(window))
        return;
    CALL_STD(EnumChildWindows, int32_t, (uint32_t, window_callback, uint32_t))(window, callback, parameter);
    visit.callback = callback;
    visit.parameter = parameter;
    CALL_STD(EnumThreadWindows, int32_t, (uint32_t, void *, void *))(
        CALL_STD(GetCurrentThreadId, uint32_t, (void))(), (void *)fs98_host_rt_visit_floating, &visit);
}
