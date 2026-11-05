/* Independent control/descriptor combinations beyond normal owner scripts. */
static uint32_t s_boundary[2][7][128][16];
static void fs98_boundary_probe(void *object, uint32_t side, uint32_t page)
{
    static const uint32_t descriptor_offsets[] = {0x19ec,0x1cac,0x1d58,0x18f8,0x1ae8,0x211c,0x1d78};
    static const uint32_t words[] = {0,1,2,0x80000000,0xffffffff};
    uint8_t *block = PTR(object, descriptor_offsets[page]);
    uint8_t saved[0xbc];
    void (TC *update)(void *) = PTR(PTR(object, 0), 0xf0);
    void (TC *capture)(void *) = PTR(PTR(object, 0), 0xf4);
    uint32_t i, j;
    HWND window = (HWND)D32(object, 0x20);
    if (page != 0 && page != 1 && page != 3 && page != 5) return;
    memcpy(saved, block, page == 5 ? sizeof(struct fs98_mpdialog_owner_block) : sizeof(saved));
    for (i = 0; i < (page == 5 ? 128u : 5u); ++i) {
        uint32_t *out = s_boundary[side][page][i];
        if (page == 0) {
            D32(block, 0x1c) = words[i];
            update(object);
            out[0] = D32(object, 0x1824);
            D32(object, 0x1824) = words[i];
            capture(object);
            out[1] = D32(block, 0x1c);
        } else if (page == 1) {
            D32(block, 0x24) = i < 3 ? i : words[i];
            update(object);
            out[0] = D32(block, 0x24);
            out[1] = IsWindowEnabled(GetDlgItem(window, 0x419));
            out[2] = IsWindowEnabled(GetDlgItem(window, 1));
            out[3] = IsWindowEnabled(GetDlgItem(window, 0x3e8));
            capture(object);
            out[4] = D32(block, 0x24);
        } else if (page == 3) {
            D32(block, 0x24) = i < 3 ? i : words[i];
            update(object);
            out[0] = IsWindowEnabled(GetDlgItem(window, 0x433));
            out[1] = IsWindowEnabled(GetDlgItem(window, 0x3e8));
            out[2] = GetWindowLongA(GetDlgItem(window, 0x433), GWL_STYLE) & WS_VISIBLE;
            out[3] = GetWindowLongA(GetDlgItem(window, 0x3e8), GWL_STYLE) & WS_VISIBLE;
        } else {
            static const uint32_t fields[] = {0x50,0x2c,0x38,0x24,0x40,0x48,0x30};
            static const uint32_t checks[] = {0x2024,0x1c3c,0x1d04,0x1dcc,0x1e94,0x1f5c};
            for (j = 0; j < 7; ++j) D32(block, fields[j]) = i & (1u << j) ? 2 : 0;
            update(object);
            for (j = 0; j < 6; ++j) out[j] = D32(object, checks[j]);
            out[6] = IsWindowEnabled(GetDlgItem(window, 0x415));
            for (j = 0; j < 6; ++j) D32(object, checks[j]) = words[(i+j)%5];
            capture(object);
            for (j = 0; j < 6; ++j) out[7+j] = D32(block, fields[j]);
        }
    }
    memcpy(block, saved, page == 5 ? sizeof(struct fs98_mpdialog_owner_block) : sizeof(saved));
    update(object);
}
