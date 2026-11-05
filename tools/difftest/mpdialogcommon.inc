/* Shared local virtuals, using each constructed class's actual vtable on a
 * sentinel-filled synthetic object. Only EnableWindow is intercepted, then forwarded
 * to USER32 on a private ordinary window, avoiding dialog callback side effects. */
static uint32_t s_common_results[2][7][26][8];
static uint32_t s_common_calls, s_common_window_matches, s_common_enabled;
static HWND s_common_window;
static BOOL (WINAPI *s_common_enable)(HWND, BOOL);

static BOOL WINAPI fs98_common_enable(HWND window, BOOL enabled)
{
    ++s_common_calls;
    s_common_window_matches += window == s_common_window;
    s_common_enabled = (uint32_t)enabled;
    return s_common_enable(window, enabled);
}

/* Explicit thiscall caller: verify stack cleanup and EBX/ESI/EDI preservation,
 * recovering ESP even for an incorrect ret/ret 4 mutation. EAX is unspecified
 * by these void methods and deliberately excluded from comparison. */
static uint32_t __attribute__((naked, noinline)) fs98_common_call(
    void *target __attribute__((unused)), void *object __attribute__((unused)),
    uint32_t argument __attribute__((unused)), uint32_t has_argument __attribute__((unused)))
{
    __asm__ volatile(
        "push %ebp\n\tmov %esp,%ebp\n\tpush %ebx\n\tpush %esi\n\tpush %edi\n\t"
        "mov $0x13579bdf,%ebx\n\tmov $0x2468ace0,%esi\n\tmov $0x789abcde,%edi\n\t"
        "mov 12(%ebp),%ecx\n\tcmpl $0,20(%ebp)\n\tje 1f\n\tpush 16(%ebp)\n"
        "1:\tcall *8(%ebp)\n\txor %eax,%eax\n\tlea -12(%ebp),%edx\n\t"
        "cmp %edx,%esp\n\tje 2f\n\tor $1,%eax\n"
        "2:\tcmp $0x13579bdf,%ebx\n\tje 3f\n\tor $2,%eax\n"
        "3:\tcmp $0x2468ace0,%esi\n\tje 4f\n\tor $4,%eax\n"
        "4:\tcmp $0x789abcde,%edi\n\tje 5f\n\tor $8,%eax\n"
        "5:\tlea -12(%ebp),%esp\n\tpop %edi\n\tpop %esi\n\tpop %ebx\n\tpop %ebp\n\tret\n\t");
}

static void fs98_common_probe(void *dialog, uint32_t side, uint32_t page)
{
    static const uint32_t slots[] = {0x08,0x0c,0x10,0x90,0x94};
    static const uint32_t arguments[] = {0,1,2,0x80000000,0xffffffff};
    uint32_t object[32], saved[32], slot, initial, argument, index = 0;
    void **enable_slot = fs98_find_import_slot(s_current_module, "USER32.dll", "EnableWindow");
    void *original;
    if (!enable_slot) fs98_fail("common EnableWindow import");
    s_common_window = CreateWindowExA(0, "STATIC", "common virtual fixture", WS_POPUP,
        0, 0, 64, 32, 0, 0, GetModuleHandleA(0), 0);
    if (!s_common_window) fs98_fail("common private window");
    memset(object, 0xa5, sizeof(object));
    PTR(object, 0) = PTR(dialog, 0);
    D32(object, 0x20) = (uint32_t)(uintptr_t)s_common_window;
    memcpy(saved, object, sizeof(saved));
    fs98_patch_slot(enable_slot, (void *)fs98_common_enable, &original);
    s_common_enable = (void *)original;
    for (slot = 0; slot < 5; ++slot) {
        void *target = PTR(PTR(dialog, 0), slots[slot]);
        HMODULE owner = 0;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, target, &owner) || owner != s_current_module)
            fs98_fail("common virtual ownership");
        for (initial = 0; initial < 2; ++initial) {
            for (argument = 0; argument < ((slot == 0 || slot == 2) ? 5u : 1u); ++argument) {
                uint32_t *out = s_common_results[side][page][index];
                uint32_t expected_calls = slot >= 3;
                memcpy(object, saved, sizeof(object));
                EnableWindow(s_common_window, initial);
                s_common_calls = s_common_window_matches = s_common_enabled = 0;
                out[0] = fs98_common_call(target, object, arguments[argument], slot == 0 || slot == 2);
                out[1] = s_common_calls;
                out[2] = s_common_window_matches;
                out[3] = s_common_enabled;
                out[4] = IsWindowEnabled(s_common_window) != 0;
                out[5] = (GetWindowLongA(s_common_window, GWL_STYLE) & WS_VISIBLE) != 0;
                out[6] = memcmp(saved, object, sizeof(saved)) != 0;
                out[7] = IsWindow(s_common_window) != 0;
                if (!side && (out[0] || out[1] != expected_calls || out[2] != expected_calls ||
                        out[3] != (slot == 4) || out[4] != (slot >= 3 ? slot == 4 : initial) ||
                        out[5] || out[6] || !out[7]))
                    fs98_fail("retail common virtual contract");
                ++index;
            }
        }
    }
    if (index != 26) fs98_fail("common virtual case count");
    fs98_restore_slot(enable_slot, original);
    if (!DestroyWindow(s_common_window)) fs98_fail("common private window cleanup");
    s_common_window = 0;
}

static uint32_t fs98_common_compare(uint32_t page)
{
    uint32_t index, field, differences = 0;
    for (index = 0; index < 26; ++index) {
        if (memcmp(s_common_results[0][page][index], s_common_results[1][page][index],
                sizeof(s_common_results[0][page][index]))) {
            ++differences;
            fprintf(stderr, "MP_DIALOG COMMON DIFFER page=%lu case=%lu\n",
                (unsigned long)page, (unsigned long)index);
            for (field = 0; field < 8; ++field)
                if (s_common_results[0][page][index][field] != s_common_results[1][page][index][field])
                    fprintf(stderr, "COMMON field=%lu retail=%lu replacement=%lu\n",
                        (unsigned long)field, (unsigned long)s_common_results[0][page][index][field],
                        (unsigned long)s_common_results[1][page][index][field]);
        }
    }
    fprintf(stderr, "COMMON page=%lu cases=26 differences=%lu\n",
        (unsigned long)page, (unsigned long)differences);
    return differences != 0;
}
