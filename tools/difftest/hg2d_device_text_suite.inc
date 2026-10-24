/* The GDI boundary is deterministic; no window or live graphics device needed. */
static uint32_t device_text_entry[2], device_text_get_result, device_text_length, device_text_begin_result;
static SIZE device_text_extent;
static const char *device_text_pointer = "HG2D text";
static int WINAPI device_text_strlen(const char *text)
{
    CHECK(text == device_text_pointer); event(0xd201);
    for (uint32_t index = 0; index < 9; index++) event((uint8_t)text[index]);
    return (int)device_text_length;
}
static BOOL WINAPI device_text_measure(HDC dc, const char *text, int length, SIZE *out)
{
    CHECK(ADDRESS(dc) == 0x12345678 && text == device_text_pointer);
    event(0xd202); event((uint32_t)length); *out = device_text_extent; return 0;
}
static COLORREF WINAPI device_text_colour(HDC dc, COLORREF colour)
{
    CHECK(ADDRESS(dc) == 0x12345678); event(0xd203); event(colour); return 0xffffffff;
}
static int WINAPI device_text_background(HDC dc, int mode)
{
    CHECK(ADDRESS(dc) == 0x12345678); event(0xd204); event((uint32_t)mode); return 0;
}
static BOOL WINAPI device_text_output(HDC dc, int x, int y, const char *text, int length)
{
    CHECK(ADDRESS(dc) == 0x12345678 && text == device_text_pointer);
    event(0xd205); event((uint32_t)x); event((uint32_t)y); event((uint32_t)length); return 0;
}
static uint32_t STDCALL device_text_get_dc(uint32_t instance, uint32_t *out)
{
    event(0xd206); event(device_tag(instance)); *out = 0x12345678; return device_text_get_result;
}
static uint32_t STDCALL device_text_release_dc(uint32_t instance, uint32_t dc)
{
    event(0xd207); event(device_tag(instance)); event(dc); return 0x8876eeee;
}
static uint32_t STDCALL device_text_end_scene(uint32_t instance)
{
    event(0xd208); event(device_tag(instance)); return 0x8876aaaa;
}
static uint32_t STDCALL device_text_begin_scene(uint32_t instance)
{
    event(0xd209); event(device_tag(instance)); return device_text_begin_result;
}
static void device_text_imports(HMODULE module)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    uint32_t found = 0;
    for (; descriptor->Name; descriptor++) {
        IMAGE_THUNK_DATA *lookup = (IMAGE_THUNK_DATA *)(base + descriptor->OriginalFirstThunk);
        IMAGE_THUNK_DATA *address = (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        for (; lookup->u1.AddressOfData; lookup++, address++) {
            uint32_t replacement;
            const char *name;
            if (IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) continue;
            name = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + lookup->u1.AddressOfData))->Name;
            replacement = !strcmp(name, "lstrlenA") ? ADDRESS(device_text_strlen) :
                !strcmp(name, "GetTextExtentPointA") ? ADDRESS(device_text_measure) :
                !strcmp(name, "SetTextColor") ? ADDRESS(device_text_colour) :
                !strcmp(name, "SetBkMode") ? ADDRESS(device_text_background) :
                !strcmp(name, "TextOutA") ? ADDRESS(device_text_output) : 0;
            if (replacement) {
                DWORD previous;
                CHECK(VirtualProtect(address, sizeof *address, PAGE_READWRITE, &previous));
                address->u1.Function = replacement;
                CHECK(VirtualProtect(address, sizeof *address, previous, &previous)); found++;
            }
        }
    }
    CHECK(found == 5);
}
static void run_device_text(void)
{
    static const uint32_t values[] = {0,1,2,3,0xffffffff,0x80000000,0x7fffffff,0xffff8000};
    uint32_t variant, index, result;
    draw_reset();
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    memset(device_vtables[3], 0, sizeof device_vtables[3]); device_objects[3][0] = ADDRESS(device_vtables[3]);
    device_vtables[1][10] = ADDRESS(device_text_begin_scene); device_vtables[1][11] = ADDRESS(device_text_end_scene);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
    device_vtables[3][17] = ADDRESS(device_text_get_dc); device_vtables[3][26] = ADDRESS(device_text_release_dc);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]); WORD(images[side], 0x28940) = ADDRESS(device_objects[3]);
    device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    for (variant = 0; variant < 128; variant++) {
        const char *text = variant % 8 == 1 ? NULL : device_text_pointer;
        uint32_t arguments[6] = {0xdeadbeef,values[(scenario + variant) % 8],values[(scenario + variant * 3) % 8],0x87654321,
            variant % 8 == 0 ? 0 : ADDRESS(&text),values[(scenario + variant / 8) % 8]};
        device_text_length = variant % 8 == 2 ? 0 : 9;
        device_text_get_result = variant & 16 ? 0x8876dddd : 0;
        device_text_begin_result = random_word();
        device_text_extent.cx = (LONG)values[(scenario + variant / 2) % 8];
        device_text_extent.cy = (LONG)values[(scenario + variant / 3) % 8];
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 8 ? 96 : 0));
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860); WORD(images[side], 0x5ee8) = 100;
        WORD(images[side], 0x8850) = 1; WORD(images[side], 0x8840) = 0; WORD(images[side], 0x885c) = 0;
        WORD(images[side], 0x5f24) = variant & 32 ? 0x100 : 0;
        WORD(global, 0x2ec) = (variant & 64 ? 0x6900 : 0xf000) | (variant & 255);
        touched = NULL; check_carry = 0;
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        result = invoke_draw(device_text_entry[side], arguments, 6, 0xaabbccdd); draw_active = 0;
        event(draw_fault_code);
        /* Compare EAX and callee-saved registers. Volatile ECX/EDX can
         * contain temporary stack addresses at this compiled API boundary. */
        for (index = 0; index < 7; index++) if (index != 0 && index != 2 && index != 3) event(draw_registers[index]);
        compare(result); CHECK(!draw_fault_code);
        CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    }
}
