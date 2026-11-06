/* Dynamic-link module init: native allocation boundaries, real x86 unwind and
 * two deinit calls per case. The CD's MFC and resource DLL remain loaded. */
static uint8_t s_init_dynamic[0x40];
static void *fs98_init_allocate(uint32_t size)
{
    if (size != sizeof(s_init_dynamic)) fs98_fail("init allocation size");
    fs98_init_note('A');
    if (s_init_case == 2) RaiseException(MPD_CONSTRUCTOR_THROW, 0, 0, 0);
    return s_init_case == 1 ? 0 : s_init_dynamic;
}
static void *TC fs98_init_construct(void *object, void *extension, int32_t system)
{
    if (object != s_init_dynamic || !extension || system) fs98_fail("init dynamic constructor arguments");
    fs98_init_note('C');
    if (s_init_case == 3) RaiseException(MPD_CONSTRUCTOR_THROW, 0, 0, 0);
    return object;
}
static void fs98_init_free(void *object)
{
    if (object != s_init_dynamic) fs98_fail("init dynamic free pointer");
    fs98_init_note('D');
}
static __attribute__((noinline)) void fs98_init_invoke(struct fs98_test_linkage *linkage)
{
    struct { void *previous, *handler; } catcher;
    s_exception_caught = 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_exception_previous));
    if (!__builtin_setjmp(s_exception_jump)) {
        catcher.previous = s_exception_previous;
        catcher.handler = fs98_exception_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        linkage->init();
        fs98_init_note('N');
    } else fs98_init_note('X');
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_exception_previous) : "memory");
    linkage->deinit();
    linkage->deinit();
}
static uint32_t fs98_run_init_exceptions(HMODULE *modules, struct fs98_test_linkage **linkages)
{
    static const uint32_t ordinals[3] = {823,342,825};
    static const char *const expected[4] = {"RACNL","RANL","RAXL","RACDXL"};
    void *handlers[3] = {fs98_init_allocate,fs98_init_construct,fs98_init_free};
    uint32_t index, side, differences = 0;
    for (index = 0; index < 3; ++index) {
        struct fs98_layout_hook *hook = &s_layout_hooks[index];
        hook->target = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)ordinals[index]);
        if (!hook->target) fs98_fail("init hook target");
        memcpy(hook->original, hook->target, 5);
        hook->jump[0] = 0xe9;
        D32(hook->jump, 1) = (uint32_t)((uintptr_t)handlers[index] - (uintptr_t)hook->target - 5);
        fs98_layout_patch(index, 1);
    }
    for (s_init_case = 0; s_init_case < 4; ++s_init_case) {
        for (side = 0; side < 2; ++side) {
            s_side = side;
            s_current_module = modules[side];
            s_init_count = 0;
            s_host_init[side] = s_host_release[side] = 0;
            s_init_events[side][0] = 0;
            fs98_init_invoke(linkages[side]);
            if (s_host_init[side] != 1 || s_host_release[side] != 1 || s_exception_caught != (s_init_case >= 2))
                fs98_fail("init lifecycle counts");
        }
        if (strcmp(s_init_events[0], expected[s_init_case])) fs98_fail("retail module init sequence");
        if (strcmp(s_init_events[0], s_init_events[1])) {
            ++differences;
            fprintf(stderr, "MP_DIALOG MODULE INIT DIFFER case=%lu retail=%s replacement=%s\n",
                (unsigned long)s_init_case, s_init_events[0], s_init_events[1]);
        }
    }
    for (index = 0; index < 3; ++index) fs98_layout_patch(index, 0);
    return differences;
}
