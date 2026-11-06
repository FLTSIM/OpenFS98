/* Real x86 search/unwind across retail MSVC and replacement cleanup scopes.
 * Throw before a dependency constructor; only previously completed subobjects
 * belong to the enclosing class's cleanup. */
static void *s_exception_jump[5], *s_exception_previous;
static uint32_t s_exception_caught;
static void *s_exception_target, *s_exception_object;

static EXCEPTION_DISPOSITION __cdecl fs98_exception_catch(
    EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if ((record->ExceptionCode == MPD_CONSTRUCTOR_THROW || record->ExceptionCode == 0xe06d7363u) &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        s_exception_unwinding = 1;
        RtlUnwind(frame, 0, record, 0);
        s_exception_unwinding = 0;
        ++s_exception_caught;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_exception_previous) : "memory");
        __builtin_longjmp(s_exception_jump, 1);
    }
    return ExceptionContinueSearch;
}

static __attribute__((noinline)) void fs98_exception_invoke(struct fs98_test_linkage *linkage)
{
    struct { void *previous, *handler; } catcher;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_exception_previous));
    if (!__builtin_setjmp(s_exception_jump)) {
        catcher.previous = s_exception_previous;
        catcher.handler = fs98_exception_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        if (s_destructor_exceptions)
            ((void *(TC *)(void *, uint32_t))s_exception_target)(s_exception_object, 1);
        else linkage->dialogs[s_exception_page < 5 ? 0 : s_exception_page - 4].create(
            (uint32_t)(uintptr_t)s_main_window, s_layout_descriptor[s_side][s_exception_page]);
        fs98_fail("constructor exception not raised");
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_exception_previous) : "memory");
}

static uint32_t fs98_run_constructor_exceptions(HMODULE *modules, struct fs98_test_linkage **linkages)
{
    static const uint32_t controls[7] = {11,14,18,10,12,20,19};
    uint32_t page, position, side, index, differences = 0;
    for (page = 0; page < 7; ++page) for (position = 1; position <= controls[page] + 1; ++position) {
        struct fs98_layout_trace traces[2];
        uint32_t live[2];
        memset(s_layout_trace, 0, sizeof(s_layout_trace));
        memset(s_layout_live, 0, sizeof(s_layout_live));
        memset(s_layout_object, 0, sizeof(s_layout_object));
        s_exception_page = page;
        s_exception_at = position;
        for (side = 0; side < 2; ++side) {
            s_side = side;
            s_current_module = modules[side];
            s_exception_calls = s_exception_caught = 0;
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
            fs98_layout_install();
            fs98_exception_invoke(linkages[side]);
            traces[side] = s_layout_trace[side][page];
            live[side] = s_layout_live[side][page];
            if (s_exception_caught != 1 || s_exception_calls != position)
                fs98_fail("constructor exception catch count");
            if (!side) {
                uint32_t ctors = 0, dtors = 0, frees = 0, event;
                for (event = 0; event < traces[side].count; ++event) {
                    ctors += traces[side].events[event].kind == 1;
                    dtors += traces[side].events[event].kind == 2;
                    frees += traces[side].events[event].kind == 11;
                }
                if (ctors != position || dtors != position - 1 || frees != 1 || live[side]) {
                    fprintf(stderr, "EXCEPTION ORACLE page=%lu position=%lu ctors=%lu dtors=%lu frees=%lu live=%lu\n",
                        (unsigned long)page, (unsigned long)position, (unsigned long)ctors,
                        (unsigned long)dtors, (unsigned long)frees, (unsigned long)live[side]);
                    for (event = 0; event < traces[side].count; ++event) {
                        const struct fs98_layout_event *entry = &traces[side].events[event];
                        fprintf(stderr, "EXCEPTION EVENT %lu/%lu/%lx/%lx\n", (unsigned long)entry->kind,
                            (unsigned long)entry->type, (unsigned long)entry->offset, (unsigned long)entry->value);
                    }
                    fs98_fail("retail constructor exception cleanup");
                }
            }
            /* Preserve the failing-before witness without leaking partially
             * built replacement fixtures. This work is outside the snapshot. */
            for (index = 0; index < 7; ++index) {
                void *object = s_layout_object[side][index];
                if (!object || !s_layout_live[side][index]) continue;
                if (index == page) {
                    uint32_t event = traces[side].count, attempted = 0;
                    while (event) {
                        const struct fs98_layout_event *entry = &traces[side].events[--event];
                        uint32_t slot, done, destroyed = 0;
                        if (entry->kind != 1 || !attempted++) continue;
                        for (done = 0; done < traces[side].count; ++done) {
                            const struct fs98_layout_event *dtor = &traces[side].events[done];
                            destroyed |= dtor->kind == 2 && dtor->type == entry->type && dtor->offset == entry->offset;
                        }
                        if (destroyed) continue;
                        slot = entry->type == 10 ? 21 : entry->type + 10;
                        fs98_layout_patch(slot, 0);
                        ((void (TC *)(void *))s_layout_hooks[slot].target)((uint8_t *)object + entry->offset);
                        fs98_layout_patch(slot, 1);
                    }
                    fs98_layout_patch(25, 0);
                    ((void (*)(void *))s_layout_hooks[25].target)(object);
                    fs98_layout_patch(25, 1);
                    s_layout_live[side][index] = 0;
                } else if (!DestroyWindow((HWND)D32(object, 0x20))) fs98_fail("exception survivor cleanup");
            }
            fs98_layout_uninstall();
            s_flow_owner_active[side] = s_details_owner_active[side] = s_details_active = 0;
            fs98_pump();
        }
        if (live[0] != live[1] || memcmp(&traces[0], &traces[1], sizeof(traces[0]))) {
            ++differences;
            fprintf(stderr, "MP_DIALOG CONSTRUCTOR EXCEPTION DIFFER page=%lu position=%lu events=%lu/%lu live=%lu/%lu\n",
                (unsigned long)page, (unsigned long)position, (unsigned long)traces[0].count,
                (unsigned long)traces[1].count, (unsigned long)live[0], (unsigned long)live[1]);
        }
    }
    return differences;
}


static uint32_t fs98_run_destructor_exceptions(HMODULE *modules, struct fs98_test_linkage **linkages)
{
    static const uint32_t controls[7] = {11,14,18,10,12,20,19};
    uint32_t page, position, side, index, differences = 0;
    for (page = 0; page < 7; ++page) for (position = 1; position <= controls[page] + 1; ++position) {
        struct fs98_layout_trace traces[2];
        memset(s_layout_trace, 0, sizeof(s_layout_trace));
        memset(s_layout_live, 0, sizeof(s_layout_live));
        memset(s_layout_object, 0, sizeof(s_layout_object));
        s_exception_page = page;
        for (side = 0; side < 2; ++side) {
            void *object, *post;
            void **slot;
            uint32_t event, attempted = 0, skipped = 0;
            s_side = side;
            s_current_module = modules[side];
            s_exception_at = 0;
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
            fs98_layout_install();
            linkages[side]->dialogs[page < 5 ? 0 : page - 4].create(
                (uint32_t)(uintptr_t)s_main_window, s_layout_descriptor[side][page]);
            object = s_layout_object[side][page];
            s_exception_object = object;
            s_exception_target = PTR(PTR(object, 0), 4);
            if (!fs98_layout_local(s_exception_target)) fs98_fail("destructor exception target ownership");
            slot = (void **)((uint8_t *)PTR(object, 0) + 0xac);
            s_retained_post_calls = 0;
            fs98_patch_slot(slot, (void *)fs98_retained_post_destroy, &post);
            if (!DestroyWindow((HWND)D32(object, 0x20))) fs98_fail("destructor exception HWND detach");
            fs98_restore_slot(slot, post);
            if (s_retained_post_calls != 1) fs98_fail("destructor exception PostNcDestroy detach");
            memset(&s_layout_trace[side][page], 0, sizeof(s_layout_trace[side][page]));
            s_exception_at = position;
            s_exception_calls = s_exception_caught = 0;
            fs98_exception_invoke(linkages[side]);
            traces[side] = s_layout_trace[side][page];
            s_exception_at = 0;
            if (s_exception_caught != 1 || !s_layout_live[side][page]) fs98_fail("destructor exception catch/lifetime");
            for (event = 0; event < traces[side].count; ++event) {
                const struct fs98_layout_event *entry = &traces[side].events[event];
                if (!side && entry->kind != 2) fs98_fail("retail destructor exception unexpected event");
                if (entry->kind == 2 && ++attempted == position) {
                    uint32_t target = entry->type == 10 ? 21 : entry->type + 10;
                    fs98_layout_patch(target, 0);
                    ((void (TC *)(void *))s_layout_hooks[target].target)((uint8_t *)object + entry->offset);
                    fs98_layout_patch(target, 1);
                    ++skipped;
                }
            }
            if (!side && (attempted != controls[page] + 1 || s_exception_calls != controls[page] + 1))
                fs98_fail("retail destructor exception cleanup completeness");
            if (skipped != 1) fs98_fail("destructor exception skipped dependency cleanup");
            fs98_layout_patch(25, 0);
            ((void (*)(void *))s_layout_hooks[25].target)(object);
            fs98_layout_patch(25, 1);
            s_layout_live[side][page] = 0;
            for (index = 0; index < 7; ++index)
                if (s_layout_live[side][index] && !DestroyWindow((HWND)D32(s_layout_object[side][index], 0x20)))
                    fs98_fail("destructor exception survivor cleanup");
            fs98_layout_uninstall();
            s_flow_owner_active[side] = s_details_owner_active[side] = s_details_active = 0;
            fs98_pump();
        }
        if (memcmp(&traces[0], &traces[1], sizeof(traces[0]))) {
            ++differences;
            fprintf(stderr, "MP_DIALOG DESTRUCTOR EXCEPTION DIFFER page=%lu position=%lu events=%lu/%lu\n",
                (unsigned long)page, (unsigned long)position,
                (unsigned long)traces[0].count, (unsigned long)traces[1].count);
        }
    }
    return differences;
}
