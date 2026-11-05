/* Direct error-slot matrix: independent statuses, helper and downstream returns. */
static uint32_t s_details_error_matrix[2][72][7];
static uint32_t s_details_error_event[7], s_details_helper_value, s_details_downstream_value;
static int32_t TC fs98_details_error_helper(void *object, void *control, uint32_t status)
{
    ++s_details_error_event[1];
    s_details_error_event[2] = (uint8_t *)control - (uint8_t *)object;
    s_details_error_event[3] = status - (uint32_t)(uintptr_t)PTR(object, 0x1d78);
    return s_details_helper_value;
}
static int32_t TC fs98_details_error_downstream(void *object, int32_t field)
{
    (void)object;
    ++s_details_error_event[4];
    s_details_error_event[5] = field;
    return s_details_downstream_value;
}
static void fs98_details_error_matrix(HWND window, uint32_t side)
{
    void *(SC *from_handle)(HWND) = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
    void *object = from_handle(window), **original = PTR(object, 0), *vtable[64];
    uint8_t *block = PTR(object, 0x1d78);
    uint32_t saved_pilot = D32(block, 0x1c), saved_observer = D32(block, 0x30);
    uint32_t saved_dialog = D32(object, 0x8c), mask, helper, downstream, zero, index = 0;
    static const int32_t values[] = {0, 2, -7};
    int32_t (TC *error)(void *, int32_t) = original[0xdc / 4];
    memcpy(vtable, original, sizeof(vtable));
    vtable[0xe0 / 4] = fs98_details_error_downstream;
    PTR(object, 0) = vtable;
    s_host_checked_target = (void *)GetProcAddress(s_ui, "?DoErrorDialog@CSimsDialog@@IAEHPAVCWnd@@K@Z");
    memcpy(s_host_checked_original, s_host_checked_target, 5);
    s_host_checked_jump[0] = 0xe9;
    D32(s_host_checked_jump, 1) = (uint32_t)((uintptr_t)fs98_details_error_helper - (uintptr_t)s_host_checked_target - 5);
    fs98_host_patch_checked(s_host_checked_jump);
    for (mask = 0; mask < 4; ++mask)
    for (helper = 0; helper < 3; ++helper)
    for (downstream = 0; downstream < 3; ++downstream)
    for (zero = 0; zero < 2; ++zero) {
        memset(s_details_error_event, 0, sizeof(s_details_error_event));
        D32(block, 0x1c) = mask & 1;
        D32(block, 0x30) = mask & 2;
        D32(object, 0x8c) = 0x1234;
        s_details_helper_value = values[helper];
        s_details_downstream_value = values[downstream];
        s_details_error_event[0] = error(object, zero ? 0 : 0x555);
        s_details_error_event[6] = D32(object, 0x8c);
        memcpy(s_details_error_matrix[side][index++], s_details_error_event, sizeof(s_details_error_event));
    }
    fs98_host_patch_checked(s_host_checked_original);
    PTR(object, 0) = original;
    D32(block, 0x1c) = saved_pilot;
    D32(block, 0x30) = saved_observer;
    D32(object, 0x8c) = saved_dialog;
}

static uint32_t s_details_update_matrix[2][12][5];
static void fs98_details_update_matrix(HWND window, uint32_t side)
{
    void *(SC *from_handle)(HWND) = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
    void *object = from_handle(window);
    void (TC *update)(void *) = PTR(PTR(object, 0), 0xf8);
    uint8_t *block = PTR(object, 0x1d78), saved[0xd4], rows[4][16] = {{0}};
    uint32_t i, original_ready = D32(object, 0x1d7c);
    HWND list = GetDlgItem(window, 0x406);
    memcpy(saved, block, sizeof(saved));
    for (i = 0; i < 4; ++i) D32(rows[i], 12) = (uint32_t)(uintptr_t)"Player";
    for (i = 0; i < 12; ++i) {
        uint32_t *out = s_details_update_matrix[side][i];
        D32(block, 0x14) = i & 1;
        D32(block, 0x50) = (i / 2) % 3;
        D32(block, 0x4c) = (uint32_t)(uintptr_t)rows[1]; /* readable predecessor for retail's -1 lookup */
        D32(block, 0x58) = 0x12345678;
        D32(rows[1], 8) = i >= 6;
        SendMessageA(list, LB_RESETCONTENT, 0, 0);
        SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)"Previous");
        SendMessageA(list, LB_SETCURSEL, 0, 0);
        update(object);
        out[0] = D32(block, 0x58);
        out[1] = SendMessageA(list, LB_GETCURSEL, 0, 0);
        out[2] = SendMessageA(list, LB_GETCOUNT, 0, 0);
        out[3] = IsWindowEnabled(GetDlgItem(window, 0x3ef));
        out[4] = D32(object, 0x1d7c);
    }
    memcpy(block, saved, sizeof(saved));
    D32(object, 0x1d7c) = original_ready;
    update(object);
}

static uint32_t s_details_validate_matrix[2][16][4], s_details_capture_calls;
static int32_t s_details_callback_value;
static void TC fs98_details_capture_probe(void *object)
{ (void)object; ++s_details_capture_calls; }
static int32_t SC fs98_details_matrix_callback(void *block)
{ (void)block; return s_details_callback_value; }
static void fs98_details_validate_matrix(HWND window, uint32_t side)
{
    void *(SC *from_handle)(HWND) = (void *)GetProcAddress(s_mfc, (const char *)(uintptr_t)2864);
    void *object = from_handle(window), **original = PTR(object, 0), *vtable[64];
    uint8_t *block = PTR(object, 0x1d78);
    uint32_t saved_callback = D32(block, 4), saved_a = D32(block, 0x1c), saved_b = D32(block, 0x30), i;
    int32_t (TC *validate)(void *, uint32_t) = original[0xd8 / 4];
    memcpy(vtable, original, sizeof(vtable));
    vtable[0xfc / 4] = fs98_details_capture_probe;
    PTR(object, 0) = vtable;
    D32(block, 4) = (uint32_t)(uintptr_t)fs98_details_matrix_callback;
    for (i = 0; i < 16; ++i) {
        D32(block, 0x1c) = i & 1;
        D32(block, 0x30) = i & 2;
        s_details_callback_value = (i & 4) ? -7 : 0;
        ShowWindow(window, (i & 8) ? SW_HIDE : SW_SHOW);
        s_details_capture_calls = 0;
        s_details_validate_matrix[side][i][0] = validate(object, 0x555);
        s_details_validate_matrix[side][i][1] = s_details_capture_calls;
        s_details_validate_matrix[side][i][2] = D32(block, 0x1c);
        s_details_validate_matrix[side][i][3] = D32(block, 0x30);
    }
    PTR(object, 0) = original;
    D32(block, 4) = saved_callback;
    D32(block, 0x1c) = saved_a; D32(block, 0x30) = saved_b;
    ShowWindow(window, SW_SHOW);
}
