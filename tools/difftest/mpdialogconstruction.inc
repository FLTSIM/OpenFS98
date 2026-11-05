/* Paired ordinary construction, DDX and destruction calls.  Hook only calls
 * from the current MP_DIALOG image; SIMSUI/MFC internals remain unmodified. */
struct fs98_layout_event { uint32_t kind, type, offset, value; };
struct fs98_layout_trace { uint32_t count; struct fs98_layout_event events[192]; };
struct fs98_layout_hook { const char *dll, *symbol; uint32_t ordinal; void *target, *replacement; uint8_t original[5], jump[5]; };
static struct fs98_layout_trace s_layout_trace[2][7];
static void *s_layout_object[2][7], *s_layout_exchange[2][7], *s_layout_descriptor[2][7];
static uint32_t s_layout_live[2][7], s_construction_only;
static const uint32_t s_layout_size[7] = {0x19f0,0x1cb0,0x1d5c,0x18fc,0x1af0,0x2120,0x1d80};
static const uint32_t s_layout_descriptor_offset[7] = {0x19ec,0x1cac,0x1d58,0x18f8,0x1ae8,0x211c,0x1d78};
static struct fs98_layout_hook s_layout_hooks[26];

static void fs98_layout_patch(uint32_t index, int hooked)
{
    struct fs98_layout_hook *hook = &s_layout_hooks[index];
    DWORD old;
    if (!VirtualProtect(hook->target, 5, PAGE_EXECUTE_READWRITE, &old)) fs98_fail("layout hook protection");
    memcpy(hook->target, hooked ? hook->jump : hook->original, 5);
    FlushInstructionCache(GetCurrentProcess(), hook->target, 5);
    if (!VirtualProtect(hook->target, 5, old, &old)) fs98_fail("layout hook restore protection");
}

static int fs98_layout_local(void *address)
{
    HMODULE owner = 0;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, address, &owner) && owner == s_current_module;
}

static int32_t fs98_layout_page(void *address)
{
    uintptr_t value = (uintptr_t)address;
    uint32_t page;
    for (page = 0; page < 7; ++page) {
        uintptr_t base = (uintptr_t)s_layout_object[s_side][page];
        if (s_layout_live[s_side][page] && value >= base && value - base < s_layout_size[page])
            return (int32_t)page;
    }
    return -1;
}

static void fs98_layout_record(uint32_t page, uint32_t kind, uint32_t type, void *object, uint32_t value)
{
    struct fs98_layout_trace *trace = &s_layout_trace[s_side][page];
    struct fs98_layout_event *event;
    if (trace->count >= 192) fs98_fail("layout trace overflow");
    event = &trace->events[trace->count++];
    event->kind = kind;
    event->type = type;
    event->offset = (uint32_t)((uintptr_t)object - (uintptr_t)s_layout_object[s_side][page]);
    event->value = value;
}

#define LAYOUT_CONTROL_HOOK(n) \
static void *TC fs98_layout_control_##n(void *object) { \
    int local = fs98_layout_local(__builtin_return_address(0)); \
    int32_t page = local ? fs98_layout_page(object) : -1; \
    void *result; \
    if (local && page < 0) fs98_fail("layout control outside dialog"); \
    if (page >= 0) fs98_layout_record((uint32_t)page, (n) < 10 ? 1 : 2, (n) % 10, object, 0); \
    fs98_layout_patch(n, 0); \
    result = ((void *(TC *)(void *))s_layout_hooks[n].target)(object); \
    fs98_layout_patch(n, 1); \
    return result; \
}
LAYOUT_CONTROL_HOOK(0) LAYOUT_CONTROL_HOOK(1) LAYOUT_CONTROL_HOOK(2)
LAYOUT_CONTROL_HOOK(3) LAYOUT_CONTROL_HOOK(4) LAYOUT_CONTROL_HOOK(5)
LAYOUT_CONTROL_HOOK(6) LAYOUT_CONTROL_HOOK(7) LAYOUT_CONTROL_HOOK(8)
LAYOUT_CONTROL_HOOK(9) LAYOUT_CONTROL_HOOK(10) LAYOUT_CONTROL_HOOK(11)
LAYOUT_CONTROL_HOOK(12) LAYOUT_CONTROL_HOOK(13) LAYOUT_CONTROL_HOOK(14)
LAYOUT_CONTROL_HOOK(15) LAYOUT_CONTROL_HOOK(16) LAYOUT_CONTROL_HOOK(17)
LAYOUT_CONTROL_HOOK(18) LAYOUT_CONTROL_HOOK(19)
#undef LAYOUT_CONTROL_HOOK

static void *TC fs98_layout_dialog_ctor(void *object, int32_t template_id, void *parent)
{
    void *result;
    if (fs98_layout_local(__builtin_return_address(0))) {
        uint32_t page = (uint32_t)template_id - 0x65;
        if (page >= 7 || !s_layout_live[s_side][page] || s_layout_object[s_side][page] != object)
            fs98_fail("layout base constructor page");
        fs98_layout_record(page, 1, 10, object, parent != 0);
    }
    fs98_layout_patch(20, 0);
    result = ((void *(TC *)(void *, int32_t, void *))s_layout_hooks[20].target)(object, template_id, parent);
    fs98_layout_patch(20, 1);
    return result;
}

static void *TC fs98_layout_dialog_dtor(void *object)
{
    int32_t page = fs98_layout_local(__builtin_return_address(0)) ? fs98_layout_page(object) : -1;
    void *result;
    if (page >= 0) fs98_layout_record((uint32_t)page, 2, 10, object, 0);
    fs98_layout_patch(21, 0);
    result = ((void *(TC *)(void *))s_layout_hooks[21].target)(object);
    fs98_layout_patch(21, 1);
    return result;
}

static void TC fs98_layout_base_exchange(void *object, void *exchange)
{
    int32_t page = fs98_layout_local(__builtin_return_address(0)) ? fs98_layout_page(object) : -1;
    if (page >= 0) {
        uint32_t flags = (PTR(object, s_layout_descriptor_offset[page]) == s_layout_descriptor[s_side][page] ? 1u : 0u) |
                         (D32(object, 0x80) == (uint32_t)(uintptr_t)s_current_module ? 2u : 0u);
        s_layout_exchange[s_side][page] = exchange;
        fs98_layout_record((uint32_t)page, 3, 0, object, flags);
    }
    fs98_layout_patch(22, 0);
    ((void (TC *)(void *, void *))s_layout_hooks[22].target)(object, exchange);
    fs98_layout_patch(22, 1);
}

static void SC fs98_layout_ddx_control(void *exchange, uint32_t id, void *control)
{
    int32_t page = fs98_layout_local(__builtin_return_address(0)) ? fs98_layout_page(control) : -1;
    if (page >= 0) {
        if (exchange != s_layout_exchange[s_side][page]) fs98_fail("layout DDX exchange pointer");
        fs98_layout_record((uint32_t)page, 4, 0, control, id);
    }
    fs98_layout_patch(23, 0);
    ((void (SC *)(void *, uint32_t, void *))s_layout_hooks[23].target)(exchange, id, control);
    fs98_layout_patch(23, 1);
}

static void *fs98_layout_allocate(uint32_t size)
{
    int local = fs98_layout_local(__builtin_return_address(0));
    void *object;
    uint32_t page;
    fs98_layout_patch(24, 0);
    object = ((void *(*)(uint32_t))s_layout_hooks[24].target)(size);
    fs98_layout_patch(24, 1);
    if (local) {
        for (page = 0; page < 7 && s_layout_size[page] != size; ++page) {}
        if (page == 7 || !object || s_layout_live[s_side][page]) fs98_fail("layout allocation");
        s_layout_object[s_side][page] = object;
        s_layout_live[s_side][page] = 1;
        fs98_layout_record(page, 0, 0, object, size);
    }
    return object;
}

static void fs98_layout_free(void *object)
{
    int local = fs98_layout_local(__builtin_return_address(0));
    int32_t page = local ? fs98_layout_page(object) : -1;
    if (local && page < 0) fs98_fail("layout free object");
    if (page >= 0) fs98_layout_record((uint32_t)page, 5, 0, object, 1);
    fs98_layout_patch(25, 0);
    ((void (*)(void *))s_layout_hooks[25].target)(object);
    fs98_layout_patch(25, 1);
    if (page >= 0) s_layout_live[s_side][page] = 0;
}

static void fs98_layout_install(void)
{
    static const char *const ctor[] = {
        "??0CSims3PieceButton@@QAE@XZ", "??0CSimsText@@QAE@XZ",
        "??0CSimsGroupbox@@QAE@XZ", "??0CSimsCheckButton@@QAE@XZ",
        "??0CSimsRadioButton@@QAE@XZ", "??0CSimsEdit@@QAE@XZ",
        "??0CSimsListBox@@QAE@XZ", "??0CSimsPicture@@QAE@XZ",
        "??0CSimsTextArt@@QAE@XZ", 0
    };
    static const char *const dtor[] = {
        "??1CSims3PieceButton@@UAE@XZ", "??1CSimsText@@UAE@XZ",
        "??1CSimsGroupbox@@UAE@XZ", "??1CSimsCheckButton@@UAE@XZ",
        "??1CSimsRadioButton@@UAE@XZ", "??1CSimsEdit@@UAE@XZ",
        "??1CSimsListBox@@UAE@XZ", "??1CSimsPicture@@UAE@XZ",
        "??1CSimsTextArt@@UAE@XZ", 0
    };
    static void *const handlers[] = {
        fs98_layout_control_0,fs98_layout_control_1,fs98_layout_control_2,fs98_layout_control_3,
        fs98_layout_control_4,fs98_layout_control_5,fs98_layout_control_6,fs98_layout_control_7,
        fs98_layout_control_8,fs98_layout_control_9,fs98_layout_control_10,fs98_layout_control_11,
        fs98_layout_control_12,fs98_layout_control_13,fs98_layout_control_14,fs98_layout_control_15,
        fs98_layout_control_16,fs98_layout_control_17,fs98_layout_control_18,fs98_layout_control_19,
        fs98_layout_dialog_ctor,fs98_layout_dialog_dtor,fs98_layout_base_exchange,fs98_layout_ddx_control,
        fs98_layout_allocate,fs98_layout_free
    };
    uint32_t i, j;
    for (i = 0; i < 26; ++i) {
        struct fs98_layout_hook *hook = &s_layout_hooks[i];
        HMODULE module = (i == 9 || i == 19 || i == 23) ? s_mfc : s_ui;
        hook->dll = module == s_mfc ? "MFC42.DLL" : "SIMSUI.DLL";
        hook->symbol = i < 10 ? ctor[i] : i < 20 ? dtor[i - 10] :
            i == 20 ? "??0CSimsDialog@@QAE@HPAVCWnd@@@Z" :
            i == 21 ? "??1CSimsDialog@@UAE@XZ" :
            i == 22 ? "?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z" :
            i == 24 ? "?ui_new@@YAPAXI@Z" : i == 25 ? "?ui_delete@@YAXPAX@Z" : 0;
        hook->ordinal = i == 9 ? 567 : i == 19 ? 793 : i == 23 ? 2302 : 0;
        hook->target = (void *)GetProcAddress(module, hook->ordinal ?
            (const char *)(uintptr_t)hook->ordinal : hook->symbol);
        hook->replacement = handlers[i];
        if (!hook->target) fs98_fail("resolve layout hook target");
        for (j = 0; j < i; ++j)
            if (s_layout_hooks[j].target == hook->target) fs98_fail("overlapping layout hook targets");
        memcpy(hook->original, hook->target, 5);
        hook->jump[0] = 0xe9;
        D32(hook->jump, 1) = (uint32_t)((uintptr_t)hook->replacement - (uintptr_t)hook->target - 5);
        fs98_layout_patch(i, 1);
    }
}

static void fs98_layout_uninstall(void)
{
    uint32_t i;
    for (i = 0; i < 26; ++i) fs98_layout_patch(i, 0);
}

static uint32_t fs98_run_construction_suite(HMODULE *modules, struct fs98_test_linkage **linkages)
{
    uint32_t side, page, i, differences = 0;
    memset(s_layout_trace, 0, sizeof(s_layout_trace));
    memset(s_layout_live, 0, sizeof(s_layout_live));
    memset(s_layout_object, 0, sizeof(s_layout_object));
    memset(s_layout_exchange, 0, sizeof(s_layout_exchange));
    for (side = 0; side < 2; ++side) {
        HWND *windows, window;
        s_side = side;
        s_current_module = modules[side];
        fs98_flow_reset_host_focus();
        fs98_flow_setup(side, 1);
        for (page = 0; page < 5; ++page) s_layout_descriptor[side][page] = s_flow_blocks[side];
        fs98_layout_install();
        windows = (HWND *)(uintptr_t)linkages[side]->dialogs[0].create(
            (uint32_t)(uintptr_t)s_main_window, s_flow_blocks[side]);
        if (!windows || windows[5]) fs98_fail("layout flow factory");
        for (page = 0; page < 5; ++page) {
            if (!IsWindow(windows[page]) || !s_layout_live[side][page] ||
                (HWND)D32(s_layout_object[side][page], 0x20) != windows[page])
                fs98_fail("layout flow object and HWND");
        }
        for (page = 0; page < 5; ++page) fs98_boundary_probe(s_layout_object[side][page], side, page);
        for (page = 0; page < 5; ++page)
            if (!DestroyWindow(windows[page])) fs98_fail("layout flow teardown");
        s_flow_owner_active[side] = 0;
        fs98_setup_block(0);
        s_layout_descriptor[side][5] = &s_blocks[side];
        window = (HWND)(uintptr_t)linkages[side]->dialogs[1].create(
            (uint32_t)(uintptr_t)s_main_window, &s_blocks[side]);
        if (!IsWindow(window) || !s_layout_live[side][5]) fs98_fail("layout Settings factory");
        fs98_boundary_probe(s_layout_object[side][5], side, 5);
        if (!DestroyWindow(window)) fs98_fail("layout Settings teardown");
        s_details_active = 1;
        fs98_setup_details(side, 0);
        s_layout_descriptor[side][6] = s_details_blocks[side];
        window = (HWND)(uintptr_t)linkages[side]->dialogs[2].create(
            (uint32_t)(uintptr_t)s_main_window, s_details_blocks[side]);
        if (!IsWindow(window) || !s_layout_live[side][6]) fs98_fail("layout Details factory");
        if (!DestroyWindow(window)) fs98_fail("layout Details teardown");
        s_details_owner_active[side] = 0;
        s_details_active = 0;
        fs98_layout_uninstall();
        for (page = 0; page < 7; ++page)
            if (s_layout_live[side][page]) fs98_fail("layout object survived destruction");
        fs98_pump();
    }
    for (page = 0; page < 7; ++page) {
        struct fs98_layout_trace *a = &s_layout_trace[0][page], *b = &s_layout_trace[1][page];
        static const uint32_t expected_controls[7] = {11,14,18,10,12,20,19};
        static const uint32_t expected_ddx[7] = {11,14,18,10,12,15,19};
        uint32_t base = 0, exchange = 0, controls = 0, destroyed = 0, constructed = 0, control_dtors = 0, allocations = 0, frees = 0;
        for (i = 0; i < a->count; ++i) {
            const struct fs98_layout_event *e = &a->events[i];
            if (e->kind == 0) ++allocations;
            if (e->kind == 5) ++frees;
            if (e->kind == 1 && e->type == 10) { ++base; if (e->value) fs98_fail("retail base parent"); }
            if (e->kind == 2 && e->type == 10) ++destroyed;
            if (e->kind == 1 && e->type != 10) ++constructed;
            if (e->kind == 2 && e->type != 10) ++control_dtors;
            if (e->kind == 3) { ++exchange; if (e->value != 3) fs98_fail("retail descriptor or instance attachment"); }
            if (e->kind == 4) ++controls;
        }
        if (base != 1 || destroyed != 1 || allocations != 1 || frees != 1 || exchange != 1 || controls != expected_ddx[page] ||
            !a->count || a->events[0].kind != 0 || a->events[a->count - 1].kind != 5 ||
            constructed != expected_controls[page] || control_dtors != expected_controls[page])
            fs98_fail("retail construction trace completeness");
        if (memcmp(s_boundary[0][page], s_boundary[1][page], sizeof(s_boundary[0][page]))) {
            ++differences;
            fprintf(stderr, "MP_DIALOG BOUNDARY DIFFER page=%lu\n", (unsigned long)page);
        }
        if (a->count != b->count || memcmp(a->events, b->events, sizeof(a->events))) {
            ++differences;
            fprintf(stderr, "MP_DIALOG CONSTRUCTION DIFFER page=%lu retail_count=%lu replacement_count=%lu\n",
                (unsigned long)page, (unsigned long)a->count, (unsigned long)b->count);
            for (i = 0; i < a->count || i < b->count; ++i) {
                struct fs98_layout_event *x = &a->events[i], *y = &b->events[i];
                if (i >= a->count || i >= b->count || memcmp(x, y, sizeof(*x))) {
                    fprintf(stderr, "LAYOUT DIFFER event=%lu retail=%lu/%lu/%lx/%lx replacement=%lu/%lu/%lx/%lx\n",
                        (unsigned long)i, (unsigned long)x->kind, (unsigned long)x->type,
                        (unsigned long)x->offset, (unsigned long)x->value,
                        (unsigned long)y->kind, (unsigned long)y->type,
                        (unsigned long)y->offset, (unsigned long)y->value);
                }
            }
        }
        fprintf(stderr, "LAYOUT page=%lu constructor_base=%lu ddx_base=%lu ddx_controls=%lu destructor_base=%lu events=%lu\n",
            (unsigned long)page, (unsigned long)base, (unsigned long)exchange,
            (unsigned long)controls, (unsigned long)destroyed, (unsigned long)a->count);
    }
    return differences;
}
