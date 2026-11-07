/* What the harness's stubs do beyond logging and answering from the script: included by
 * simsuidiff.c after its fixture and helpers.
 *
 * semantics() returns 1 when it has answered an import itself (in *result); the
 * dispatcher has already logged the call. Pointers given to an import are only written
 * through where the real import would, and only into the fixture or a local. */

/* imports that are data, not code: their slots hold an address */
static uint8_t s_data_imports[8][256];
static void *data_import(uint32_t index)
{
    switch (index) {
    case SIMSUI_IMPORT_MFC_4234: return s_data_imports[0];
    case SIMSUI_IMPORT_MFC_6379: return s_data_imports[1];
    case SIMSUI_IMPORT_MFC_1795: return s_data_imports[5];
    case SIMSUI_IMPORT_MFC_1816: return s_data_imports[6];
    case SIMSUI_IMPORT_MFC_1871: return s_data_imports[7];
    case SIMSUI_IMPORT_MFC_4275: return s_data_imports[0] + 128;
    case SIMSUI_IMPORT_MFC_4225: return s_data_imports[2];
    case SIMSUI_IMPORT_MFC_1829: return s_data_imports[3];
    case SIMSUI_IMPORT__adjust_fdiv: return s_data_imports[4];
    default: return 0;
    }
}
/* the argument bytes a CDC's virtual slot pops: MFC42's CDC vtable (0x5f49d88c), whose
 * 31 slots CPaintDC, CClientDC and CWindowDC keep */
static uint32_t dc_slot_pop(uint32_t slot)
{
    static const uint8_t pops[31] = {0, 4, 4, 0, 4, 4, 4, 0, 0, 0, 4, 4, 4, 4, 4, 4, 12, 12, 12, 20, 12, 20,
                                     4, 8, 4, 16, 28, 32, 16, 32, 16};
    return slot < 31 ? pops[slot] : 0;
}
/* the arguments a caller-cleaned import takes, where its callers do not show it */
static uint32_t import_arguments(uint32_t index)
{
    switch (index) {
    case SIMSUI_IMPORT_atoi: return 1;
    case SIMSUI_IMPORT__mbscmp: return 2;
    case SIMSUI_IMPORT_vsprintf: return 3;
    default: return s_imports[index].arguments;
    }
}
static int in_data(uint32_t address) { return address >= BASE + DATA_RVA && address < BASE + DATA_RVA + DATA_SIZE; }
static int writable(uint32_t address, uint32_t size)
{
    return (in_fixture(address) && in_fixture(address + size - 1)) || (in_stack(address) && in_stack(address + size - 1)) ||
           (in_data(address) && in_data(address + size - 1));
}
/* the contents of a table an import is handed, as a hash in the log: palettes and colour
 * tables are built on the stack, so only what the import is given shows what was built */
static void log_contents(uint32_t index, const uint32_t *arguments)
{
    uint32_t address = 0, size = 0, at, hash = 0x811c9dc5u;
    switch (index) {
    case SIMSUI_IMPORT_SetDIBColorTable:
    case SIMSUI_IMPORT_SetPaletteEntries:
        address = arguments[3]; size = arguments[2] <= 0x100 ? arguments[2] * 4 : 0; break;
    case SIMSUI_IMPORT_CreateDIBSection:   /* the header and colour count it is given */
        address = arguments[1]; size = 0x28; break;
    case SIMSUI_IMPORT_CreatePalette:
        address = arguments[0];
        if (writable(address, 4)) size = 4 + (uint32_t)((const uint16_t *)(uintptr_t)address)[1] * 4;
        if (size > 0x404) size = 0x404;
        break;
    default: return;
    }
    if (!size || !(writable(address, size) || (in_image(address) && in_image(address + size - 1)))) return;
    for (at = 0; at < size; ++at) hash = (hash ^ ((const uint8_t *)(uintptr_t)address)[at]) * 0x01000193u;
    put(" #%08x", hash);
}
/* the DIB sections CreateDIBSection made this run: their handles, sizes and bits */
#define SECTION_HANDLE 0x5d000000u
static struct { int32_t width, height, stride; uint16_t depth; uint32_t bits; } s_sections[32];
static uint32_t s_section_count;
static int section_index(uint32_t handle)
{
    return (handle & 0xffffff00u) == SECTION_HANDLE && (handle & 0xff) < s_section_count ? (int)(handle & 0xff) : -1;
}
/* a text the script chooses: a control spec, a font spec, or plain */
static const char *script_text(uint32_t value)
{
    static const char *const texts[] = {
        "Button#10,20,30,40", "Arial,-12,0,700", "Pic#5,6", "Name#1,2,3,4,5", "", "plain text",
        "Tab#3,4,9", "Courier New,14,2,400", "&Open#4,6,60", "Save && Quit", "Two\nlines#2,3,70", "E&xit\n"};
    return texts[value % 12];
}
static void fill_script(uint32_t address, uint32_t size, uint32_t value)
{
    uint32_t at;
    if (!writable(address, size)) return;
    for (at = 0; at < size; at += 4) {
        uint32_t word = (value * (at + 7) * 0x9e3779b1u) >> 23;     /* small, signed now and then */
        if (word & 0x100) word = 0u - (word & 0xff);
        memcpy((uint8_t *)(uintptr_t)address + at, &word, size - at < 4 ? size - at : 4);
    }
}
/* CStrings: the text pointer, with MFC's header before it (references, length, room) */
static uint32_t cstring_room(const char *text, uint32_t length, uint32_t room)
{
    uint8_t *block = heap_allocate(12 + room + 1);
    int32_t header[3];
    if (!block) { fprintf(stderr, "cstring_room %u %u used %u\n", length, room, s_heap_used); GIVE_UP(); }
    header[0] = 1; header[1] = (int32_t)length; header[2] = (int32_t)room;
    memcpy(block, header, 12);
    memcpy(block + 12, text, length);
    block[12 + length] = 0;
    return fixture_address(block + 12);
}
static uint32_t cstring_make(const char *text, uint32_t length)
{
    return cstring_room(text, length, length);
}
/* a CString this model made: in the heap, its header sane */
static int cstring_known(const char *text)
{
    uint32_t at = (uint32_t)(uintptr_t)text;
    const int32_t *header = (const int32_t *)(text - 12);
    return at >= fixture_address(s_fixture.heap) + 12 && at < fixture_address(s_fixture.heap) + HEAP_SIZE &&
           header[2] >= 0 && header[2] <= HEAP_SIZE && header[1] >= 0 && header[1] <= header[2];
}
static uint32_t cstring_length(const char *text)
{
    return cstring_known(text) ? (uint32_t)*(const int32_t *)(text - 8) : 0;
}
static int semantics(uint32_t index, uint32_t ecx, uint32_t *arguments, uint64_t *result)
{
    uint32_t value = script();
    (void)ecx;
    switch (index) {
    case SIMSUI_IMPORT_MFC_540:            /* CString() */
        if (writable(ecx, 4)) *(uint32_t *)(uintptr_t)ecx = cstring_make("", 0);
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_537:            /* CString(const char *) */
    case SIMSUI_IMPORT_MFC_860:            /* operator=(const char *) */
        if (writable(ecx, 4)) {
            const char *text = (const char *)(uintptr_t)arguments[0];
            *(uint32_t *)(uintptr_t)ecx = cstring_make(text, text ? (uint32_t)strlen(text) : 0);
        }
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_535:            /* CString(const CString &) */
    case SIMSUI_IMPORT_MFC_858:            /* operator=(const CString &) */
        if (writable(ecx, 4)) {
            const char *text = *(const char **)(uintptr_t)arguments[0];
            *(uint32_t *)(uintptr_t)ecx = cstring_known(text) ? cstring_make(text, cstring_length(text)) : cstring_make("", 0);
        }
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_3874: {         /* CWnd::GetWindowText(CString &): a spec or plain */
        static const char *const numbers[8] = {"", "12", "-3", "4.5", "-6.7", "1234567", "0", "-"};
        const char *text = CURRENT.rva == 0x92e0 ? numbers[(value >> 3) % 8] : script_text(value >> 3);   /* an edit's */
        if (writable(arguments[0], 4)) *(uint32_t *)(uintptr_t)arguments[0] = cstring_make(text, (uint32_t)strlen(text));
        *result = 0;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_925: {          /* operator+(char, const CString &), into the hidden result */
        const char *text = (const char *)(uintptr_t)*(uint32_t *)(uintptr_t)arguments[2];
        char joined[0x200];
        uint32_t length;
        if (!writable(arguments[2], 4) || !cstring_known(text)) text = "";
        length = cstring_length(text) < sizeof(joined) - 2 ? cstring_length(text) : (uint32_t)sizeof(joined) - 2;
        joined[0] = (char)arguments[1];
        memcpy(joined + 1, text, length);
        if (writable(arguments[0], 4)) *(uint32_t *)(uintptr_t)arguments[0] = cstring_make(joined, length + 1);
        *result = arguments[0];
        return 1;
    }
    case SIMSUI_IMPORT_MFC_2763: {         /* Find(char) */
        const char *text = *(const char **)(uintptr_t)ecx, *found;
        if (!cstring_known(text)) text = "";
        found = strchr(text, (char)arguments[0]);
        *result = found && arguments[0] & 0xff ? (uint32_t)(found - text) : 0xffffffffu;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_4278:           /* Mid(first, count), into the hidden result */
    case SIMSUI_IMPORT_MFC_4277: {         /* Mid(first) */
        const char *text = *(const char **)(uintptr_t)ecx;
        int32_t length, first = (int32_t)arguments[1];
        if (!cstring_known(text)) text = "";
        length = (int32_t)cstring_length(text);
        int32_t count = index == SIMSUI_IMPORT_MFC_4278 ? (int32_t)arguments[2] : length - first;
        if (first < 0) first = 0;
        if (count < 0) count = 0;
        if (first > length) first = length;
        if (first + count > length) count = length - first;
        if (writable(arguments[0], 4)) *(uint32_t *)(uintptr_t)arguments[0] = cstring_make(text + first, (uint32_t)count);
        *result = arguments[0];
        return 1;
    }
    case SIMSUI_IMPORT_MFC_4129:           /* Left(count), into the hidden result */
    case SIMSUI_IMPORT_MFC_5710: {         /* Right(count) */
        const char *text = *(const char **)(uintptr_t)ecx;
        int32_t length, count = (int32_t)arguments[1];
        if (!cstring_known(text)) text = "";
        length = (int32_t)cstring_length(text);
        if (count < 0) count = 0;
        if (count > length) count = length;
        if (writable(arguments[0], 4))
            *(uint32_t *)(uintptr_t)arguments[0] =
                cstring_make(index == SIMSUI_IMPORT_MFC_4129 ? text : text + length - count, (uint32_t)count);
        *result = arguments[0];
        return 1;
    }
    case SIMSUI_IMPORT_MFC_2784: {         /* Find(const char *) */
        const char *text = *(const char **)(uintptr_t)ecx, *found;
        const char *wanted = (const char *)(uintptr_t)arguments[0];
        if (!cstring_known(text)) text = "";
        found = wanted && (writable(arguments[0], 1) || in_image(arguments[0])) ? strstr(text, wanted) : 0;
        *result = found ? (uint32_t)(found - text) : 0xffffffffu;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_939:            /* operator+=(const CString &) */
    case SIMSUI_IMPORT_MFC_941: {          /* operator+=(const char *) */
        const char *text = *(const char **)(uintptr_t)ecx, *more;
        char joined[0x400];
        uint32_t length, extra;
        if (!cstring_known(text)) text = "";
        if (index == SIMSUI_IMPORT_MFC_939) {
            more = writable(arguments[0], 4) ? *(const char **)(uintptr_t)arguments[0] : "";
            if (!cstring_known(more)) more = "";
        } else {
            more = (const char *)(uintptr_t)arguments[0];
            if (!more || !(writable(arguments[0], 1) || in_image(arguments[0]))) more = "";
        }
        length = cstring_length(text) < 0x1ff ? cstring_length(text) : 0x1ff;
        extra = (uint32_t)strnlen(more, 0x1ff);
        memcpy(joined, text, length);
        memcpy(joined + length, more, extra);
        if (writable(ecx, 4)) *(uint32_t *)(uintptr_t)ecx = cstring_make(joined, length + extra);
        *result = ecx;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_2915: {         /* GetBuffer(n): its text, with room for n */
        char *text = *(char **)(uintptr_t)ecx;
        int32_t room = (int32_t)arguments[0];
        int known = cstring_known(text);
        if (!known || (room > 0 && *(int32_t *)(text - 4) < room)) {
            uint32_t length = known ? cstring_length(text) : 0;
            text = (char *)(uintptr_t)cstring_room(known ? text : "", length,
                                                  room > (int32_t)length ? (uint32_t)room : length);
            if (writable(ecx, 4)) *(uint32_t *)(uintptr_t)ecx = (uint32_t)(uintptr_t)text;
        }
        *result = (uint32_t)(uintptr_t)text;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_5572: {         /* ReleaseBuffer(n) */
        char *text = *(char **)(uintptr_t)ecx;
        int32_t length = (int32_t)arguments[0];
        if (cstring_known(text)) {
            if (length == -1) length = (int32_t)strnlen(text, (size_t)*(int32_t *)(text - 4));
            *(int32_t *)(text - 8) = length;
            text[length] = 0;
        }
        *result = 0;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_800:            /* ~CString */
        *result = 0;
        return 1;
    case SIMSUI_IMPORT_MFC_323:            /* CDC(): no DCs yet */
        if (writable(ecx, 16)) {
            void **vtable = s_fake_vtables[DC_VTABLE];
            memcpy((void *)(uintptr_t)ecx, &vtable, 4);
            memset((uint8_t *)(uintptr_t)ecx + 4, 0, 12);
        }
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_470:            /* CPaintDC(CWnd *) */
    case SIMSUI_IMPORT_MFC_289:            /* CClientDC(CWnd *): a DC object, attached to a handle */
        if (writable(ecx, 16)) {
            void **vtable = s_fake_vtables[DC_VTABLE];
            uint32_t handle = 0x7dc00000u | (value & 0xffff);
            memcpy((void *)(uintptr_t)ecx, &vtable, 4);
            memcpy((uint8_t *)(uintptr_t)ecx + 4, &handle, 4);
            memcpy((uint8_t *)(uintptr_t)ecx + 8, &handle, 4);
            memset((uint8_t *)(uintptr_t)ecx + 12, 0, 4);
        }
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_1640:           /* CDC::Attach(hdc) */
        if (writable(ecx, 12)) {
            memcpy((uint8_t *)(uintptr_t)ecx + 4, arguments, 4);
            memcpy((uint8_t *)(uintptr_t)ecx + 8, arguments, 4);
        }
        *result = arguments[0] != 0;
        return 1;
    case SIMSUI_IMPORT_MFC_283:            /* CBrush(COLORREF): a GDI object */
        if (writable(ecx, 8)) {
            void **vtable = s_fake_vtables[GENERIC_VTABLE];
            uint32_t handle = (value >> 4) & 0xfff;
            memcpy((void *)(uintptr_t)ecx, &vtable, 4);
            memcpy((uint8_t *)(uintptr_t)ecx + 4, &handle, 4);
        }
        *result = ecx;
        return 1;
    case SIMSUI_IMPORT_MFC_2859:           /* CDC::FromHandle: a CDC (its fake vtable a DC's) for a handle */
        *result = arguments[0] ? fixture_address(s_fixture.blocks[(value % (BLOCKS / 4)) * 4 + 2]) : 0;
        return 1;
    case SIMSUI_IMPORT_MFC_2864:           /* CWnd::FromHandle: a window object for a handle */
        *result = arguments[0] ? fixture_address(s_fixture.blocks[(value % (BLOCKS / 4)) * 4]) : 0;
        return 1;
    case SIMSUI_IMPORT_GetWindowRect:
    case SIMSUI_IMPORT_GetClientRect:
        if (value % 3 == 0 && writable(arguments[1], 16)) {     /* now and then a rectangle of boundaries */
            static const int32_t edges[8] = {0, 5, 40, 300, 640, 10000, 10001, -3};
            int32_t rect[4], at;
            for (at = 0; at < 4; ++at) rect[at] = edges[(value >> (3 + at * 3)) % 8];
            memcpy((void *)(uintptr_t)arguments[1], rect, 16);
        } else if (writable(arguments[1], 16)) {     /* else a rectangle a screen could hold */
            int32_t rect[4], at;
            for (at = 0; at < 4; ++at) rect[at] = (int32_t)((value >> (at * 7)) % 720) - 20;
            memcpy((void *)(uintptr_t)arguments[1], rect, 16);
        }
        *result = 1;
        return 1;
    case SIMSUI_IMPORT_GetDeviceCaps:      /* the screen's size at the usual modes and their edges */
        if (arguments[1] == 8 || arguments[1] == 0xa) {
            static const int32_t widths[6] = {640, 800, 1024, 639, 641, 320};
            static const int32_t heights[6] = {480, 600, 768, 479, 481, 200};
            *result = (uint32_t)(arguments[1] == 8 ? widths : heights)[value % 6];
            return 1;
        }
        return 0;
    case SIMSUI_IMPORT_ScreenToClient:
    case SIMSUI_IMPORT_ClientToScreen:
        if (writable(arguments[1], 8)) {
            int32_t *point = (int32_t *)(uintptr_t)arguments[1];
            int32_t shift = (int32_t)(value % 64) - 32;
            point[0] += index == SIMSUI_IMPORT_ScreenToClient ? -shift : shift;
            point[1] += index == SIMSUI_IMPORT_ScreenToClient ? -shift / 2 : shift / 2;
        }
        *result = 1;
        return 1;
    case SIMSUI_IMPORT_SetRect:
        if (writable(arguments[0], 16)) memcpy((void *)(uintptr_t)arguments[0], arguments + 1, 16);
        *result = 1;
        return 1;
    case SIMSUI_IMPORT_UnionRect:
        if (writable(arguments[0], 16) && writable(arguments[1], 16) && writable(arguments[2], 16)) {
            int32_t a[4], b[4], *out = (int32_t *)(uintptr_t)arguments[0];
            memcpy(a, (void *)(uintptr_t)arguments[1], 16);
            memcpy(b, (void *)(uintptr_t)arguments[2], 16);
            if (a[0] >= a[2] || a[1] >= a[3]) memcpy(a, b, 16);
            else if (b[0] < b[2] && b[1] < b[3]) {
                if (b[0] < a[0]) a[0] = b[0];
                if (b[1] < a[1]) a[1] = b[1];
                if (b[2] > a[2]) a[2] = b[2];
                if (b[3] > a[3]) a[3] = b[3];
            }
            memcpy(out, a, 16);
            *result = 1;
        } else *result = 0;
        return 1;
    case SIMSUI_IMPORT_LockResource:
        *result = value % 7 ? fixture_address(s_fixture.resource) : 0;
        return 1;
    case SIMSUI_IMPORT_lstrcpyA: {         /* the real copy, between memory both sides own */
        const char *from = (const char *)(uintptr_t)arguments[1];
        uint32_t length;
        if (!writable(arguments[1], 1)) { *result = 0; return 1; }
        length = (uint32_t)strnlen(from, 0x400);
        if (length < 0x400 && writable(arguments[0], length + 1)) memmove((void *)(uintptr_t)arguments[0], from, length + 1);
        *result = arguments[0];
        return 1;
    }
    case SIMSUI_IMPORT_vsprintf: {         /* the real formatting, of the formats the harness gives */
        char out[0x400];
        int length;
        if (!writable(arguments[1], 1) || strnlen((const char *)(uintptr_t)arguments[1], 0x100) >= 0x100) { *result = 0; return 1; }
        length = vsnprintf(out, sizeof(out), (const char *)(uintptr_t)arguments[1], (va_list)(uintptr_t)arguments[2]);
        if (length >= 0 && length < (int)sizeof(out) && writable(arguments[0], (uint32_t)length + 1))
            memcpy((void *)(uintptr_t)arguments[0], out, (size_t)length + 1);
        put(" \"%s\"", length >= 0 && length < (int)sizeof(out) ? out : "?");
        *result = (uint32_t)length;
        return 1;
    }
    case SIMSUI_IMPORT_CopyRect:
        if (writable(arguments[0], 16) && writable(arguments[1], 16))
            memcpy((void *)(uintptr_t)arguments[0], (void *)(uintptr_t)arguments[1], 16);
        *result = 1;
        return 1;
    case SIMSUI_IMPORT_CreateDIBSection: { /* (dc, info, usage, &bits, section, offset): bits in the heap */
        const uint8_t *info = (const uint8_t *)(uintptr_t)arguments[1];
        int32_t width, height, stride;
        uint16_t depth;
        uint8_t *bits = 0;
        *result = 0;
        if (writable(arguments[1], 0x10) && value % 9 && s_section_count < 32) {
            memcpy(&width, info + 4, 4); memcpy(&height, info + 8, 4); memcpy(&depth, info + 0xe, 2);
            stride = (int32_t)((((uint32_t)width * depth + 31) / 32) * 4);
            if (width > 0 && width <= 0x400 && height && height > -0x400 && height < 0x400 &&
                (depth == 1 || depth == 4 || depth == 8 || depth == 16 || depth == 24 || depth == 32) &&
                stride * (height < 0 ? -height : height) <= 0x3000)
                bits = heap_allocate((uint32_t)(stride * (height < 0 ? -height : height)));
        }
        if (bits) {
            s_sections[s_section_count].width = width;
            s_sections[s_section_count].height = height < 0 ? -height : height;
            s_sections[s_section_count].stride = stride;
            s_sections[s_section_count].depth = depth;
            s_sections[s_section_count].bits = fixture_address(bits);
            *result = SECTION_HANDLE | s_section_count++;
        }
        if (writable(arguments[3], 4)) *(uint32_t *)(uintptr_t)arguments[3] = bits ? fixture_address(bits) : 0;
        return 1;
    }
    case SIMSUI_IMPORT_GetObjectA:         /* (object, size, buffer) */
        if (arguments[1] <= 0x200) fill_script(arguments[2], arguments[1], value);
        if (section_index(arguments[0]) >= 0 && arguments[1] >= 0x18 && writable(arguments[2], 0x18)) {
            int at = section_index(arguments[0]);    /* a section's BITMAP, as it is */
            uint32_t bitmap[6] = {0, (uint32_t)s_sections[at].width, (uint32_t)s_sections[at].height,
                                  (uint32_t)s_sections[at].stride, 1u | (uint32_t)s_sections[at].depth << 16,
                                  s_sections[at].bits};
            memcpy((void *)(uintptr_t)arguments[2], bitmap, 0x18);
            *result = arguments[1];
            return 1;
        }
        if ((arguments[1] == 0x18 || arguments[1] == 0x54) && writable(arguments[2], 0x18)) {
            int32_t *bitmap = (int32_t *)(uintptr_t)arguments[2];  /* another object's BITMAP: a small one */
            bitmap[0] = 0;
            bitmap[1] = (int32_t)((value >> 3) % 44);
            bitmap[2] = (int32_t)((value >> 9) % 44);
            bitmap[3] = (bitmap[1] + 3) & ~3;
        }
        *result = value % 5 ? arguments[1] : 0;
        return 1;
    case SIMSUI_IMPORT_GetPaletteEntries:  /* (palette, first, count, entries) */
    case SIMSUI_IMPORT_GetDIBColorTable:
        if (arguments[2] <= 0x100) fill_script(arguments[3], arguments[2] * 4, value);
        *result = value % 5 ? arguments[2] : 0;
        return 1;
    case SIMSUI_IMPORT_GetTextExtentPointA:    /* (dc, text, length, size): a small size */
        if (writable(arguments[3], 8)) {
            int32_t size[2] = {(int32_t)(value % 4 ? value % 200 : 94 + (value >> 2) % 3), (int32_t)((value >> 8) % 30)};
            memcpy((void *)(uintptr_t)arguments[3], size, 8);
        }
        *result = value % 7 != 0;
        return 1;
    case SIMSUI_IMPORT_GetWindowLongA:     /* a style with WS_GROUP and WS_TABSTOP now and then; user data a window */
        if ((int32_t)arguments[1] == -16) { *result = 0x50000000u | (value & 3) << 16 | ((value >> 8) & 0xff); return 1; }
        if ((int32_t)arguments[1] == -21) { *result = 0x5000 + ((value >> 2) % 4) * 0x10; return 1; }
        return 0;
    case SIMSUI_IMPORT_GetNextDlgGroupItem:    /* a window of the group */
        *result = 0x5000 + ((value >> 2) % 4) * 0x10;
        return 1;
    case SIMSUI_IMPORT_SendMessageA:       /* EM_GETSEL writes the selection; EM_CHARFROMPOS 0 often */
        if (arguments[1] == 0xd7) { *result = value % 2 ? 0 : value >> 16; return 1; }
        if (arguments[1] == 0xb0) {
            uint32_t start = value % 3 ? 0 : value % 6, end = start + ((value >> 4) % 3 ? 0 : (value >> 6) % 4);
            if (writable(arguments[2], 4)) *(uint32_t *)(uintptr_t)arguments[2] = start;
            if (writable(arguments[3], 4)) *(uint32_t *)(uintptr_t)arguments[3] = end;
            *result = start | end << 16;
            return 1;
        }
        return 0;
    case SIMSUI_IMPORT_GetCursorPos:
    case SIMSUI_IMPORT_GetCaretPos:
        fill_script(arguments[0], 8, value);
        *result = 1;
        return 1;
    case SIMSUI_IMPORT_GetWindowThreadProcessId:
        fill_script(arguments[1], 4, value);
        *result = value & 0xfff;
        return 1;
    case SIMSUI_IMPORT_LoadStringA:
    case SIMSUI_IMPORT_FormatMessageA: {
        uint32_t buffer = arguments[index == SIMSUI_IMPORT_LoadStringA ? 2 : 4];
        uint32_t room = arguments[index == SIMSUI_IMPORT_LoadStringA ? 3 : 5];
        const char *text = script_text(value >> 3);
        uint32_t length;
        /* SUI-1: a font spec that fails leaves AddFont reading its locals unset, which within
         * DllMain hold what earlier calls left on each side's own stack: DllMain is given
         * fonts that load */
        if (index == SIMSUI_IMPORT_LoadStringA && CURRENT.rva == 0x1950 && arguments[1] != 0xf84e)
            text = (value >> 3) % 2 ? "Arial,-12,0,700" : "Courier New,14,2,400";
        else if (value % 6 == 0) { *result = 0; return 1; }
        else if (value % 6 == 1 && index == SIMSUI_IMPORT_FormatMessageA) {     /* a length past the room */
            if (writable(arguments[4], 2)) memcpy((void *)(uintptr_t)arguments[4], "!", 2);
            *result = 0x200 + (value >> 4) % 2;
            return 1;
        }
        length = (uint32_t)strlen(text);
        if (room && writable(buffer, length + 1 < room ? length + 1 : room)) {
            uint32_t copy = length < room ? length : room - 1;
            memcpy((void *)(uintptr_t)buffer, text, copy);
            ((char *)(uintptr_t)buffer)[copy] = 0;
            *result = copy;
        } else *result = 0;
        return 1;
    }
    case SIMSUI_IMPORT_MFC_823:            /* operator new */
    case SIMSUI_IMPORT_malloc:
        *result = value % 8 == 0 ? 0 : fixture_address(heap_allocate(arguments[0]));
        return 1;
    case SIMSUI_IMPORT_CharNextA: {        /* single-byte text: the next byte, or the end */
        const char *text = (const char *)(uintptr_t)arguments[0];
        *result = (uint32_t)(uintptr_t)(*text ? text + 1 : text);
        return 1;
    }
    case SIMSUI_IMPORT_CharPrevA: {
        const char *start = (const char *)(uintptr_t)arguments[0], *at = (const char *)(uintptr_t)arguments[1];
        *result = (uint32_t)(uintptr_t)(at > start ? at - 1 : start);
        return 1;
    }
    case SIMSUI_IMPORT_atoi:
        *result = (uint32_t)atoi((const char *)(uintptr_t)arguments[0]);
        return 1;
    case SIMSUI_IMPORT_MFC_825:            /* operator delete */
    case SIMSUI_IMPORT_free:
        *result = 0;
        return 1;
    default:
        return 0;
    }
}

/* HOST's services SIMSUI calls through its import link (stdcall), and the bytes each pops */
static uint32_t host_pop(uint32_t offset)
{
    switch (offset) {
    case 0x20c: return 16;      /* allocate(pool, size, &block, flag) */
    case 0x214: return 4;       /* free(&block) */
    case 0x10c: return 8;       /* run dialog(id, block) */
    case 0x200: return 12;      /* key name(code, buffer, room) */
    case 0x208: return 16;      /* fold the keypad(&key, &unused, &flags, &extended) */
    case 0x1cc: return 12;      /* wait for a joystick control(joystick, sense, &control) */
    default: return 0;
    }
}
static int host_semantics(uint32_t offset, uint32_t *arguments, uint64_t *result)
{
    uint32_t value = script();
    switch (offset) {
    case 0x20c:
        if (writable(arguments[2], 4))
            *(uint32_t *)(uintptr_t)arguments[2] = value % 8 == 0 ? 0 : fixture_address(heap_allocate(arguments[1]));
        *result = 0;
        return 1;
    case 0x214:
        *result = 0;
        return 1;
    case 0x10c:                            /* a message box: its answer into the block */
        if (writable(arguments[1], 0x18)) {
            uint32_t *box = (uint32_t *)(uintptr_t)arguments[1];
            put(" box %x %x %x %x", box[0], box[1], box[2], box[4]); describe(box[5]);
            if (writable(box[5], 1)) put(" \"%.64s\"", (const char *)(uintptr_t)box[5]);
            { static const uint32_t answers[6] = {0, 1, 2, 0x40, 3, 7}; box[3] = answers[value % 6]; }
        }
        *result = 0;
        return 1;
    case 0x200:                            /* a name made from the code, as long as the room allows */
        if (arguments[2] && writable(arguments[1], arguments[2] < 0x40 ? arguments[2] : 0x40))
            snprintf((char *)(uintptr_t)arguments[1], arguments[2] < 0x40 ? arguments[2] : 0x40, "K%x+%u", arguments[0], value % 7);
        *result = value % 2;
        return 1;
    case 0x208:                            /* the host's own rule (host.c host_208) */
        if (writable(arguments[0], 4) && writable(arguments[2], 4) && writable(arguments[3], 4)) {
            static const uint8_t pad[10] = {0x2d, 0x23, 0x28, 0x22, 0x25, 0x0c, 0x27, 0x24, 0x26, 0x21};
            uint32_t *key = (uint32_t *)(uintptr_t)arguments[0];
            put(" key %x flags %x extended %x", *key, *(uint32_t *)(uintptr_t)arguments[2], *(uint32_t *)(uintptr_t)arguments[3]);
            if (*key >= 0x60 && *key <= 0x69) *key = pad[*key - 0x60];
            else if (*key == 0x6e) *key = 0x2e;
            else {
                if (*key == 0x0d && *(uint32_t *)(uintptr_t)arguments[3]) *key = 0x87;
                *result = arguments[0];
                return 1;
            }
            *(uint32_t *)(uintptr_t)arguments[2] |= 1;
        }
        *result = arguments[2];
        return 1;
    case 0x1cc:                            /* a control moved, or none in time */
        if (writable(arguments[2], 4)) *(uint32_t *)(uintptr_t)arguments[2] = value % 3 ? 1 + value % 0x50 : 0;
        *result = value;
        return 1;
    default:
        return 0;
    }
}
