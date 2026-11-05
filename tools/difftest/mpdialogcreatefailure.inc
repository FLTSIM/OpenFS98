/* Veto native child creation while leaving MFC Create and its cleanup intact.
 * Mutations quarantine module-local ui_delete requests until factory return;
 * a separate immediate-free diagnostic forwards those requests immediately. */
static HWND s_failure_window, s_failure_children[128];
static uint32_t s_failure_page, s_failure_at, s_failure_count, s_failure_vetoes;
static uint32_t s_failure_prior_live, s_failure_parent_live;
static void **s_failure_vtable;
static void *s_failure_post, *s_failure_delete;

static void TC fs98_failure_post_destroy(void *object)
{
    fs98_layout_record(s_failure_page, 8, 0, object, 0);
    ((void (TC *)(void *))s_failure_post)(object);
}

static void *TC fs98_failure_delete_object(void *object, uint32_t flags)
{
    void *result;
    fs98_layout_record(s_failure_page, 9, 0, object, flags);
    result = ((void *(TC *)(void *, uint32_t))s_failure_delete)(object, flags);
    fs98_layout_record(s_failure_page, 10, 0, object, result == object);
    if (s_create_failure_native_free)
        fprintf(stderr, "CREATE_FAILURE DESTRUCTOR_RETURN side=%lu page=%lu object=%p result=%p\n",
            (unsigned long)s_side, (unsigned long)s_failure_page, object, result);
    return result;
}

static LRESULT CALLBACK fs98_failure_cbt(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_CREATEWND && s_layout_live[s_side][s_failure_page] && !s_failure_vetoes) {
        const CBT_CREATEWNDA *data = (const void *)lparam;
        const CREATESTRUCTA *create = data->lpcs;
        if (!(create->style & WS_CHILD) && !s_failure_window) {
            void *object = s_layout_object[s_side][s_failure_page];
            s_failure_window = (HWND)wparam;
            s_failure_vtable = PTR(object, 0);
            if (!fs98_layout_local(s_failure_vtable[1]) || !fs98_layout_local(s_failure_vtable[0xac / 4]))
                fs98_fail("failure virtual ownership");
            fs98_patch_slot(&s_failure_vtable[1], (void *)fs98_failure_delete_object, &s_failure_delete);
            fs98_patch_slot(&s_failure_vtable[0xac / 4], (void *)fs98_failure_post_destroy, &s_failure_post);
        } else if (create->hwndParent == s_failure_window) {
            uint32_t index = s_failure_count++;
            if (index >= 128) fs98_fail("failure child overflow");
            s_failure_children[index] = (HWND)wparam;
            fs98_layout_record(s_failure_page, 7, 0,
                s_layout_object[s_side][s_failure_page], (uint32_t)(uintptr_t)create->hMenu);
            if (index == s_failure_at) {
                uint32_t prior;
                s_failure_parent_live = IsWindow(s_failure_window) != 0;
                for (prior = 0; prior < index; ++prior)
                    s_failure_prior_live += IsWindow(s_failure_children[prior]) != 0;
                ++s_failure_vetoes;
                return 1;
            }
        }
    }
    return CallNextHookEx(0, code, wparam, lparam);
}

static uint32_t fs98_failure_child_count(HMODULE module, uint32_t page)
{
    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(0x65 + page), RT_DIALOG);
    const uint16_t *data = resource ? LockResource(LoadResource(module, resource)) : 0;
    uint32_t count;
    if (!data) fs98_fail("failure dialog resource");
    count = data[1] == 0xffff ? data[8] : data[4];
    if (count < 4 || count >= 128) fs98_fail("failure resource child count");
    return count;
}

static void fs98_failure_retail_contract(const struct fs98_layout_trace *trace, uint32_t page)
{
    static const uint32_t controls[7] = {11,14,18,10,12,20,19};
    uint32_t index, next = trace->count, counts[11] = {0};
    for (index = 0; index < trace->count; ++index) {
        const struct fs98_layout_event *event = &trace->events[index];
        if (event->kind > 10) fs98_fail("failure event kind");
        ++counts[event->kind];
        if (event->kind == 1) {
            const struct fs98_layout_event *dtor;
            do { if (!next) fs98_fail("failure retail reverse destruction missing"); --next; }
            while (trace->events[next].kind != 2);
            dtor = &trace->events[next];
            if (event->type != dtor->type || event->offset != dtor->offset)
                fs98_fail("failure retail reverse destruction order");
        }
        if ((event->kind == 9 || event->kind == 10) && event->value != 1)
            fs98_fail("failure retail deleting flags or return");
    }
    if (counts[0] != 1 || counts[1] != controls[page] + 1 || counts[2] != controls[page] + 1 ||
        counts[3] || counts[4] || counts[5] != 1 || counts[7] != s_failure_at + 1 ||
        counts[8] != 1 || counts[9] != 1 || counts[10] != 1)
        fs98_fail("failure retail cleanup completeness");
    /* Native child creation precedes PostNcDestroy, which dispatches the
     * flag-1 destructor, then reverse destruction and the one free request. */
    next = 0;
    for (index = 0; index < trace->count; ++index) {
        uint32_t kind = trace->events[index].kind;
        if (kind == 7 && next) fs98_fail("failure retail native cleanup order");
        if (kind == 8) { if (next) fs98_fail("failure retail post order"); next = 1; }
        if (kind == 9) { if (next != 1) fs98_fail("failure retail delete order"); next = 2; }
        if (kind == 2 && next != 2) fs98_fail("failure retail destructor order");
        if (kind == 5) { if (next != 2) fs98_fail("failure retail free order"); next = 3; }
        if (kind == 10) { if (next != 3) fs98_fail("failure retail return order"); next = 4; }
    }
    if (next != 4) fs98_fail("failure retail final cleanup state");
}

static uint32_t fs98_run_create_failure(HMODULE *modules, struct fs98_test_linkage **linkages)
{
    uint32_t page, stage, side, index, differences = 0;
    for (page = 0; page < 7; ++page) {
        uint32_t count = fs98_failure_child_count(modules[0], page);
        if (fs98_failure_child_count(modules[1], page) != count) fs98_fail("failure paired resource count");
        for (stage = 0; stage < 3; ++stage) {
            uint32_t outcomes[2][9] = {{0}};
            struct fs98_layout_trace failed_trace[2];
            s_failure_page = page;
            s_failure_at = stage == 0 ? 0 : stage == 1 ? count / 2 : count - 1;
            memset(s_layout_trace, 0, sizeof(s_layout_trace));
            memset(s_layout_live, 0, sizeof(s_layout_live));
            memset(s_layout_object, 0, sizeof(s_layout_object));
            memset(s_layout_exchange, 0, sizeof(s_layout_exchange));
            for (side = 0; side < 2; ++side) {
                HHOOK hook;
                uint32_t result, live_children = 0, mask = 0;
                uint32_t *out = outcomes[side];
                HWND windows[7] = {0};
                s_side = side;
                s_current_module = modules[side];
                fs98_flow_reset_host_focus();
                if (page < 5) {
                    fs98_flow_setup(side, 1);
                    D32(s_flow_blocks[side], 0x1c) = 1;
                    for (index = 0; index < 5; ++index) s_layout_descriptor[side][index] = s_flow_blocks[side];
                } else if (page == 5) {
                    fs98_setup_block(0);
                    s_layout_descriptor[side][5] = &s_blocks[side];
                } else {
                    s_details_active = 1;
                    fs98_setup_details(side, 0);
                    s_layout_descriptor[side][6] = s_details_blocks[side];
                }
                s_failure_window = 0;
                s_failure_vtable = 0;
                s_failure_count = s_failure_vetoes = 0;
                s_failure_prior_live = s_failure_parent_live = 0;
                memset(s_failure_children, 0, sizeof(s_failure_children));
                fs98_layout_install();
                hook = SetWindowsHookExA(WH_CBT, fs98_failure_cbt, 0, GetCurrentThreadId());
                if (!hook) fs98_fail("failure CBT hook");
                result = linkages[side]->dialogs[page < 5 ? 0 : page - 4].create(
                    (uint32_t)(uintptr_t)s_main_window, s_layout_descriptor[side][page]);
                if (!UnhookWindowsHookEx(hook)) fs98_fail("failure CBT unhook");
                if (!s_failure_vtable || s_failure_vetoes != 1) fs98_fail("native failure veto precondition");
                fs98_restore_slot(&s_failure_vtable[1], s_failure_delete);
                fs98_restore_slot(&s_failure_vtable[0xac / 4], s_failure_post);
                if (page < 5) {
                    HWND *array = (void *)(uintptr_t)result;
                    if (!array || array[5]) fs98_fail("failure flow array");
                    memcpy(windows, array, 5 * sizeof(HWND));
                } else windows[page] = (HWND)(uintptr_t)result;
                for (index = 0; index < 7; ++index)
                    if (IsWindow(windows[index])) mask |= 1u << index;
                for (index = 0; index < s_failure_count; ++index)
                    live_children += IsWindow(s_failure_children[index]) != 0;
                out[0] = s_failure_count;
                out[1] = s_failure_vetoes;
                out[2] = IsWindow(s_failure_window) != 0;
                out[3] = live_children;
                out[4] = s_layout_live[side][page];
                out[5] = mask;
                out[6] = windows[page] != 0;
                out[7] = s_failure_prior_live;
                out[8] = s_failure_parent_live;
                fprintf(stderr, "CREATE_FAILURE side=%lu page=%lu stage=%lu veto_index=%lu children=%lu vetoes=%lu parent_live=%lu children_live=%lu object_live=%lu surviving_mask=%lx failed_hwnd=%lu prior_live=%lu parent_before=%lu\n",
                    (unsigned long)side, (unsigned long)page, (unsigned long)stage, (unsigned long)s_failure_at,
                    (unsigned long)out[0], (unsigned long)out[1], (unsigned long)out[2], (unsigned long)out[3],
                    (unsigned long)out[4], (unsigned long)out[5], (unsigned long)out[6],
                    (unsigned long)out[7], (unsigned long)out[8]);
                if (!side) {
                    if (out[0] != s_failure_at + 1 || out[7] != s_failure_at || out[8] != 1 || out[2] || out[3] || out[4] || out[6] ||
                        mask != (page < 5 ? (0x1fu ^ (1u << page)) : 0))
                        fs98_fail("retail native failure outcome");
                    fs98_failure_retail_contract(&s_layout_trace[side][page], page);
                }
                failed_trace[side] = s_layout_trace[side][page];
                /* Snapshot of the failed object's trace is unaffected by
                 * destroying the other pages, including entries beyond a hole. */
                for (index = 0; index < 7; ++index)
                    if (IsWindow(windows[index]) && !DestroyWindow(windows[index])) fs98_fail("failure survivor cleanup");
                for (index = 0; index < 7; ++index) {
                    void *object = s_layout_object[side][index];
                    if (!object) continue;
                    uint32_t event, destruction_started = 0;
                    const struct fs98_layout_trace *trace = &s_layout_trace[side][index];
                    for (event = 0; event < trace->count; ++event)
                        destruction_started |= trace->events[event].kind == 2 || trace->events[event].kind == 9;
                    /* A no-free/wrong-flags mutant can finish destruction yet
                     * leave the allocation live. Never destroy it twice. */
                    if (s_layout_live[side][index] && !destruction_started) {
                        void *target = PTR(PTR(object, 0), 4);
                        if (!fs98_layout_local(target)) fs98_fail("failure retained cleanup target");
                        ((void *(TC *)(void *, uint32_t))target)(object, 1);
                    }
                    if (!s_create_failure_native_free) {
                        fs98_layout_patch(25, 0);
                        ((void (*)(void *))s_layout_hooks[25].target)(object);
                        fs98_layout_patch(25, 1);
                    }
                    s_layout_live[side][index] = 0;
                }
                fs98_layout_uninstall();
                s_flow_owner_active[side] = 0;
                s_details_owner_active[side] = 0;
                s_details_active = 0;
                fs98_pump();
            }
            if (memcmp(outcomes[0], outcomes[1], sizeof(outcomes[0])) ||
                memcmp(&failed_trace[0], &failed_trace[1], sizeof(failed_trace[0]))) {
                const struct fs98_layout_trace *a = &failed_trace[0], *b = &failed_trace[1];
                ++differences;
                fprintf(stderr, "MP_DIALOG CREATE FAILURE DIFFER page=%lu stage=%lu retail_events=%lu replacement_events=%lu\n",
                    (unsigned long)page, (unsigned long)stage, (unsigned long)a->count, (unsigned long)b->count);
                for (index = 0; index < a->count || index < b->count; ++index) {
                    const struct fs98_layout_event *x = &a->events[index], *y = &b->events[index];
                    if (memcmp(x, y, sizeof(*x)))
                        fprintf(stderr, "FAILURE_EVENT DIFFER index=%lu retail=%lu/%lu/%lx/%lx replacement=%lu/%lu/%lx/%lx\n",
                            (unsigned long)index, (unsigned long)x->kind, (unsigned long)x->type,
                            (unsigned long)x->offset, (unsigned long)x->value,
                            (unsigned long)y->kind, (unsigned long)y->type,
                            (unsigned long)y->offset, (unsigned long)y->value);
                }
            }
        }
    }
    return differences;
}
