/* What the stubs do beyond logging and answering from the script: imports whose effect the
 * routines depend on (the string functions do the real thing), what a call's arguments point
 * to, and the fixture's shapes. Included by hostrt_dispatch's file. */

enum { COM_DDRAW, COM_SURFACE, COM_D3D2, COM_PALETTE, COM_KINDS };
/* a scenario shaped for one routine's deep path: the 8-bit display with Direct3D and overlays */
static int s_overlay_focus;
static uint32_t com_new(uint32_t kind);

/* an import whose slot holds data rather than code: none in FLTSIM98.EXE so far */
static void *data_import(uint32_t index) { (void)index; return 0; }

static int in_data(uint32_t address) { return address >= BASE + DATA_RVA && address < BASE + DATA_RVA + DATA_SIZE; }
/* memory a stub may read or write for a caller: the fixture, the operation's stack, .data */
static int writable(uint32_t address, uint32_t size)
{
    if (size == 0) return 1;
    if (in_fixture(address) && in_fixture(address + size - 1)) return 1;
    if (in_stack(address) && in_stack(address + size - 1)) return 1;
    return in_data(address) && in_data(address + size - 1);
}
/* readable text: also retail's own strings */
static int readable_text(uint32_t address)
{
    uint32_t at;
    for (at = 0; at < 0x400; ++at) {
        if (!writable(address + at, 1) && !(address + at >= BASE + 0x16000 && address + at < BASE + DATA_RVA + DATA_SIZE))
            return 0;
        if (!*(const char *)(uintptr_t)(address + at)) return 1;
    }
    return 0;
}
static void log_text(uint32_t address)
{
    if (readable_text(address)) put(" \"%.80s\"", (const char *)(uintptr_t)address);
    else put(" ?");
}
/* the strings an import is given */
static void log_contents(uint32_t index, const uint32_t *arguments)
{
    switch (index) {
    case HOST_RT_IMPORT_lstrcpyA: case HOST_RT_IMPORT_lstrcatA:
        log_text(arguments[1]); break;
    case HOST_RT_IMPORT_lstrcmpiA: case HOST_RT_IMPORT_lstrcmpA:
        log_text(arguments[0]); log_text(arguments[1]); break;
    case HOST_RT_IMPORT_lstrlenA:
        log_text(arguments[0]); break;
    case HOST_RT_IMPORT_wsprintfA:
        log_text(arguments[1]); break;
    case HOST_RT_IMPORT_GetPrivateProfileStringA: case HOST_RT_IMPORT_GetPrivateProfileIntA:
        log_text(arguments[0]); log_text(arguments[1]); break;
    case HOST_RT_IMPORT_atoi: case HOST_RT_IMPORT_LoadLibraryA: case HOST_RT_IMPORT__strdup: case HOST_RT_IMPORT__strupr:
    case HOST_RT_IMPORT_FindFirstFileA:
        log_text(arguments[0]); break;
    case HOST_RT_IMPORT_GetProcAddress:
        log_text(arguments[1]); break;
    default: break;
    }
}

/* a callback the code is given, called from a stub: the stub's own pops kept across it */
typedef int32_t (SC *callback2)(uint32_t, uint32_t);
typedef int32_t (SC *callback4)(uint32_t, uint32_t, uint32_t, uint32_t);
static int32_t call_back2(uint32_t code, uint32_t a, uint32_t b)
{
    uint32_t pop = s_pop_now;
    int32_t result = ((callback2)(uintptr_t)code)(a, b);
    s_pop_now = pop;
    put(" back %x", (uint32_t)result);
    return result;
}
static int32_t call_back4(uint32_t code, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t pop = s_pop_now;
    int32_t result = ((callback4)(uintptr_t)code)(a, b, c, d);
    s_pop_now = pop;
    put(" back %x", (uint32_t)result);
    return result;
}
static int semantics(uint32_t index, uint32_t ecx, uint32_t *arguments, uint64_t *result)
{
    (void)ecx;
    switch (index) {
    case HOST_RT_IMPORT_DirectDrawCreate: {     /* (guid, out, outer) */
        uint32_t value = script(), object = value % 4 ? com_new(COM_DDRAW) : 0;
        if (object && writable(arguments[1], 4)) memcpy((void *)(uintptr_t)arguments[1], &object, 4);
        *result = object ? 0 : 0x887600e1u;
        return 1;
    }
    case HOST_RT_IMPORT_DirectDrawEnumerateA: { /* (callback, context): a few devices */
        static const char *const descriptions[] = {"Primary Display Driver", "Voodoo Graphics", "Secondary"};
        static const char *const drivers[] = {"display", "\\\\.\\DISPLAY2", "vdddraw", "DISPLAY"};
        uint32_t value = script(), count = value % 5, at;
        uint8_t *scratch = s_fixture.blocks[29];
        for (at = 0; at < count; ++at) {
            uint32_t pick = value >> (at * 3 + 4);
            strcpy((char *)scratch + 0x40, descriptions[pick % 3]);
            strcpy((char *)scratch + 0x100, drivers[(pick >> 2) % 4]);
            put(" device %u", at);
            if (!call_back4(arguments[0], at == 0 ? 0 : fixture_address(scratch + at * 16), fixture_address(scratch + 0x40),
                            fixture_address(scratch + 0x100), arguments[1]))
                break;
        }
        *result = 0;
        return 1;
    }
    case HOST_RT_IMPORT_EnumChildWindows: case HOST_RT_IMPORT_EnumThreadWindows: {
        uint32_t value = script(), count = value % 4, at;
        for (at = 0; at < count; ++at)
            if (!call_back2(arguments[1], (index == HOST_RT_IMPORT_EnumChildWindows ? 0x5000 : 0x6000) + at, arguments[2]))
                break;
        *result = 1;
        return 1;
    }
    case HOST_RT_IMPORT_GetClassNameA: {        /* (window, buffer, size) */
        const char *name = (script() ^ arguments[0]) % 3 ? "FS98FLOAT" : "Static";
        if (!writable(arguments[1], 16) || arguments[2] < 16) return 0;
        strcpy((char *)(uintptr_t)arguments[1], name);
        *result = strlen(name);
        return 1;
    }
    case HOST_RT_IMPORT_GetPrivateProfileStringA: {   /* (section, key, default, out, size, file) */
        static const char *const values[] = {"", "Voodoo Graphics", "Primary Display Driver (Direct3D)",
            "800x600x16", "640x480", "1024x768x16x3", "x", "512x384x8", "0x0x0", "\\\\.\\DISPLAY2 (Direct3D RGB)",
            "640x480x16", "abc", "640x400x16"};
        uint32_t value = script(), size = arguments[4];
        const char *text = value % 4 == 0 && readable_text(arguments[2]) ? (const char *)(uintptr_t)arguments[2]
                                                                         : values[(value >> 2) % 13];
        size_t length;
        /* opening DirectDraw: the second device, half the time (its kind 4 takes the 640x480 path) */
        if (!strcmp(CURRENT.name, "open_directdraw") && readable_text(arguments[1]) &&
            !strcmp((const char *)(uintptr_t)arguments[1], "Device") && (value >> 6) % 2)
            text = "Voodoo Graphics";
        length = strlen(text);
        if (size == 0 || !writable(arguments[3], size)) return 0;
        if (length >= size) length = size - 1;
        memcpy((char *)(uintptr_t)arguments[3], text, length);
        ((char *)(uintptr_t)arguments[3])[length] = 0;
        put(" now \"%.80s\"", (char *)(uintptr_t)arguments[3]);
        *result = (uint32_t)length;
        return 1;
    }
    case HOST_RT_IMPORT_LoadLibraryA: {         /* a module, by which of four, or none */
        uint32_t value = script();
        *result = value % 5 ? 0x10000000u + (value >> 4) % 4 * 0x10000 : 0;
        return 1;
    }
    case HOST_RT_IMPORT_GetProcAddress: {       /* a module's Linkage or ImportTable (blocks 15, 19), or none */
        uint32_t value = script(), which = (arguments[0] >> 16) & 3;
        if (!readable_text(arguments[1])) return 0;
        if (!strcmp((char *)(uintptr_t)arguments[1], "Linkage"))
            *result = value % 8 ? fixture_address(s_fixture.blocks[15] + which * 0x80) : 0;
        else if (!strcmp((char *)(uintptr_t)arguments[1], "ImportTable"))
            *result = value % 3 ? fixture_address(s_fixture.blocks[19] + which * 0x80) : 0;
        else return 0;
        return 1;
    }
    case HOST_RT_IMPORT__strdup: {
        uint8_t *copy;
        if (!readable_text(arguments[0])) return 0;
        copy = heap_allocate((uint32_t)strlen((char *)(uintptr_t)arguments[0]) + 1);
        if (copy) strcpy((char *)copy, (char *)(uintptr_t)arguments[0]);
        put(" =>"); describe(fixture_address(copy)); put("\n   ");
        *result = fixture_address(copy);
        return 1;
    }
    case HOST_RT_IMPORT__strupr:
        if (!readable_text(arguments[0]) || !writable(arguments[0], (uint32_t)strlen((char *)(uintptr_t)arguments[0]) + 1)) return 0;
        _strupr((char *)(uintptr_t)arguments[0]);
        *result = arguments[0];
        return 1;
    case HOST_RT_IMPORT_memmove:
        if (!writable(arguments[0], arguments[2]) || !readable(arguments[1], arguments[2]) || arguments[2] > 0x10000) return 0;
        memmove((void *)(uintptr_t)arguments[0], (void *)(uintptr_t)arguments[1], arguments[2]);
        *result = arguments[0];
        return 1;
    case HOST_RT_IMPORT_FindFirstFileA: case HOST_RT_IMPORT_FindNextFileA: {   /* a file or directory, or the end */
        static const char *const names[] = {"A.DLL", "b.dll", "SUB", "C.DLL"};
        uint32_t value = script(), found = arguments[1], attributes = value % 4 == 2 ? 0x10 : 0x20;
        if (!writable(found, 0x140)) return 0;
        if (value % 4 == 3 || (index == HOST_RT_IMPORT_FindFirstFileA && value % 8 == 7)) {
            *result = index == HOST_RT_IMPORT_FindFirstFileA ? 0xffffffffu : 0;
            return 1;
        }
        memcpy((void *)(uintptr_t)found, &attributes, 4);
        strcpy((char *)(uintptr_t)found + 0x2c, names[(value >> 3) % 4]);
        *result = index == HOST_RT_IMPORT_FindFirstFileA ? 0x7700 : 1;
        return 1;
    }
    case HOST_RT_IMPORT_GetPrivateProfileSectionA: {    /* (section, buffer, size, file): lines, or full */
        static const char contents[3][40] = {"MODULE=a.dll\0x=1\0module=B.DLL\0", "\0", "Module=c.dll\0MODULE\0"};
        static const uint32_t lengths[3] = {30, 0, 20};
        uint32_t value = script(), pick = (value >> 2) % 3;
        if (!writable(arguments[1], arguments[2]) || arguments[2] < 40) return 0;
        memcpy((void *)(uintptr_t)arguments[1], contents[pick], 40);
        *result = value % 3 == 0 && arguments[2] < 0xc00 ? arguments[2] - 2 : lengths[pick];
        return 1;
    }
    case HOST_RT_IMPORT_GetCursorPos: {         /* (point): somewhere on a 640x480 screen, or off it */
        uint32_t value = script();
        int32_t point[2] = {(int32_t)(value % 700) - 30, (int32_t)((value >> 10) % 540) - 30};
        if (value % 3 == 0) {   /* where it was drawn last */
            point[0] = *(int32_t *)(uintptr_t)(BASE + 0x1a970) + *(int32_t *)(uintptr_t)(BASE + 0x1a52c);
            point[1] = *(int32_t *)(uintptr_t)(BASE + 0x1a974) + *(int32_t *)(uintptr_t)(BASE + 0x1a530);
        }
        if (!writable(arguments[0], 8)) return 0;
        memcpy((void *)(uintptr_t)arguments[0], point, 8);
        put(" now %d,%d", point[0], point[1]);
        *result = 1;
        return 1;
    }
    case HOST_RT_IMPORT_IntersectRect:
        if (!writable(arguments[0], 16) || !writable(arguments[1], 16) || !writable(arguments[2], 16)) return 0;
        *result = (uint32_t)IntersectRect((RECT *)(uintptr_t)arguments[0], (RECT *)(uintptr_t)arguments[1],
                                          (RECT *)(uintptr_t)arguments[2]);
        return 1;
    case HOST_RT_IMPORT_GetRegionData: {        /* (region, size, data): a few small rectangles */
        uint32_t value = script(), count = value % 6, at, header[8] = {0x20, 1, 0, 0, 0, 0, 0, 0};
        if (!arguments[2] || arguments[1] < 0x20 + count * 16 || !writable(arguments[2], 0x20 + count * 16)) return 0;
        header[2] = count; header[3] = count * 16;
        memcpy((void *)(uintptr_t)arguments[2], header, 0x20);
        for (at = 0; at < count; ++at) {
            int32_t rect[4];
            value ^= value << 13; value ^= value >> 17; value ^= value << 5;
            rect[0] = (int32_t)(value % 70) - 10; rect[1] = (int32_t)((value >> 8) % 70) - 10;
            rect[2] = rect[0] + (int32_t)((value >> 16) % 50); rect[3] = rect[1] + (int32_t)((value >> 24) % 50);
            memcpy((void *)(uintptr_t)(arguments[2] + 0x20 + at * 16), rect, 16);
        }
        *result = 0x20 + count * 16;
        return 1;
    }
    case HOST_RT_IMPORT_GetWindow: {            /* (window, command): another window, or none */
        uint32_t value = script();
        *result = value % 4 == 0 ? 0 : 0x4100 + (value >> 4) % 8;
        return 1;
    }
    case HOST_RT_IMPORT_GetWindowLongA:         /* (window, index): a floating window's data for GWL_USERDATA */
        if (arguments[1] != (uint32_t)-21) return 0;
        {
            uint32_t value = script();
            *result = value % 5 == 0 ? 0 : fixture_address(s_fixture.blocks[value % 2 ? 5 : 13]);
        }
        return 1;
    case HOST_RT_IMPORT_UnionRect:              /* (out, a, b): the real one */
        if (!writable(arguments[0], 16) || !writable(arguments[1], 16) || !writable(arguments[2], 16)) return 0;
        *result = (uint32_t)UnionRect((RECT *)(uintptr_t)arguments[0], (RECT *)(uintptr_t)arguments[1],
                                      (RECT *)(uintptr_t)arguments[2]);
        return 1;
    case HOST_RT_IMPORT_atoi:
        if (!readable_text(arguments[0])) return 0;
        *result = (uint32_t)atoi((char *)(uintptr_t)arguments[0]);
        return 1;
    case HOST_RT_IMPORT__strcmpi:
        if (!readable_text(arguments[0]) || !readable_text(arguments[1])) return 0;
        *result = (uint32_t)_stricmp((char *)(uintptr_t)arguments[0], (char *)(uintptr_t)arguments[1]);
        return 1;
    case HOST_RT_IMPORT_lstrcpyA: case HOST_RT_IMPORT_lstrcatA: {
        uint32_t to = arguments[0], from = arguments[1];
        size_t have = index == HOST_RT_IMPORT_lstrcatA && readable_text(to) ? strlen((char *)(uintptr_t)to) : 0;
        if (!readable_text(from) || (index == HOST_RT_IMPORT_lstrcatA && !readable_text(to)) ||
            !writable(to, (uint32_t)(have + strlen((char *)(uintptr_t)from) + 1))) {
            *result = 0;            /* the real one catches the fault and returns null */
            return 1;
        }
        memmove((char *)(uintptr_t)to + have, (char *)(uintptr_t)from, strlen((char *)(uintptr_t)from) + 1);
        put(" now \"%.80s\"", (char *)(uintptr_t)to);
        *result = to;
        return 1;
    }
    case HOST_RT_IMPORT_lstrcmpiA: case HOST_RT_IMPORT_lstrcmpA:
        if (!readable_text(arguments[0]) || !readable_text(arguments[1])) return 0;
        *result = (uint32_t)(index == HOST_RT_IMPORT_lstrcmpiA
            ? lstrcmpiA((char *)(uintptr_t)arguments[0], (char *)(uintptr_t)arguments[1])
            : lstrcmpA((char *)(uintptr_t)arguments[0], (char *)(uintptr_t)arguments[1]));
        return 1;
    case HOST_RT_IMPORT_lstrlenA:
        *result = readable_text(arguments[0]) ? strlen((char *)(uintptr_t)arguments[0]) : 0;
        return 1;
    case HOST_RT_IMPORT_wsprintfA: {
        char out[1100];
        int length;
        if (!readable_text(arguments[1])) return 0;
        length = wvsprintfA(out, (char *)(uintptr_t)arguments[1], (va_list)&arguments[2]);
        if (length < 0 || !writable(arguments[0], (uint32_t)length + 1)) return 0;
        memcpy((char *)(uintptr_t)arguments[0], out, (size_t)length + 1);
        put(" now \"%.80s\"", out);
        *result = (uint32_t)length;
        return 1;
    }
    default:
        return 0;
    }
}

/* memory a model may read: writable memory or retail's image */
static int readable(uint32_t address, uint32_t size)
{
    return writable(address, size) || (address >= BASE && address + size <= BASE + s_image_size && address + size >= address);
}
/* the growable array (host.c's +0x2f8 and its neighbours), on a header that looks like one */
static int list_sane(uint32_t list)
{
    uint32_t *header = (uint32_t *)(uintptr_t)list;
    if (!writable(list, 0x1c)) return 0;
    return header[0] <= 64 && header[1] <= 128 && header[2] <= 0x40 && header[3] <= 0x100 &&
           (header[6] == 0 || writable(header[6], header[0] * header[3]));
}
static int service_semantics(uint32_t offset, uint32_t *arguments, uint64_t *result)
{
    switch (offset) {
    case 0x20c: {   /* (flags, size, out, zero): *out = an allocation, zeroed if asked; 0 */
        uint8_t *p;
        if (!writable(arguments[2], 4) || arguments[1] > 0x4000) return 0;
        p = heap_allocate(arguments[1]);
        memcpy((void *)(uintptr_t)arguments[2], &p, 4);
        if (p && arguments[3]) memset(p, 0, arguments[1]);
        put(" now"); describe(fixture_address(p));
        *result = 0;
        return 1;
    }
    case 0x214:     /* (pp): freed, nulled; 0 */
        if (!writable(arguments[0], 4)) return 0;
        memset((void *)(uintptr_t)arguments[0], 0, 4);
        *result = 0;
        return 1;
    case 0x2b8:     /* strlen, 0 for null */
        if (!arguments[0]) { *result = 0; return 1; }
        if (!readable_text(arguments[0])) return 0;
        *result = strlen((char *)(uintptr_t)arguments[0]);
        return 1;
    case 0x2f8: {   /* (list): a zeroed element appended */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0], element;
        if (!list_sane(arguments[0])) return 0;
        if (header[0] == header[1]) {
            uint8_t *data;
            header[1] += header[2];
            data = heap_allocate(header[1] * header[3]);
            if (!data) return 0;
            if (header[6]) memcpy(data, (void *)(uintptr_t)header[6], header[0] * header[3]);
            header[6] = fixture_address(data);
        }
        element = header[6] + header[0] * header[3];
        memset((void *)(uintptr_t)element, 0, header[3]);
        ++header[0];
        put(" =>"); describe(element); put("\n");
        *result = element;
        put("   ");
        return 1;
    }
    case 0x2fc: {   /* (list): its data freed */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0];
        if (!writable(arguments[0], 0x1c)) return 0;
        if (!header[6]) { *result = arguments[0] + 0x18; return 1; }
        header[6] = 0;
        *result = 0;
        return 1;
    }
    case 0x308: {   /* (list, context, fn): fn(element, context) in order until one is not 0 */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0], at, back = 0;
        if (!list_sane(arguments[0])) return 0;
        ++header[4];
        put("\n");
        for (at = 0; at < header[0] && at < 64; ++at) {
            back = (uint32_t)call_back2(arguments[2], header[6] + at * header[3], arguments[1]);
            put("\n");
            if (back) break;
        }
        --header[4];
        *result = back;
        return 1;
    }
    case 0x310: {   /* (list, offset, key, length, indirect): the first element matching, or 0 */
        uint32_t *header = (uint32_t *)(uintptr_t)arguments[0], at, size = arguments[3];
        if (!list_sane(arguments[0]) || size > 0x400) return 0;
        for (at = 0; at < header[0]; ++at) {
            uint32_t a = header[6] + at * header[3] + arguments[1], b = arguments[2];
            if (arguments[4]) {
                if (!readable(a, 4) || !readable(b, 4)) return 0;
                a = *(uint32_t *)(uintptr_t)a; b = *(uint32_t *)(uintptr_t)b;
            }
            if (size && (!readable(a, size) || !readable(b, size))) return 0;
            if (!memcmp((void *)(uintptr_t)a, (void *)(uintptr_t)b, size)) {
                *result = header[6] + at * header[3];
                return 1;
            }
        }
        *result = 0;
        return 1;
    }
    case 0x184: {   /* (out, drive, directory, name): directory\\name */
        char path[0x300] = "";
        if (!writable(arguments[0], 0x104)) return 0;
        if (arguments[2] && readable_text(arguments[2])) strcpy(path, (char *)(uintptr_t)arguments[2]);
        if (path[0] && path[strlen(path) - 1] != '\\') strcat(path, "\\");
        if (arguments[3] && readable_text(arguments[3])) strcat(path, (char *)(uintptr_t)arguments[3]);
        path[0x103] = 0;
        strcpy((char *)(uintptr_t)arguments[0], path);
        put(" now \"%.80s\"", path);
        *result = 0;
        return 1;
    }
    case 0x2a0: {   /* (id, out, size): a string from the resources */
        static const char *const texts[] = {"C:\\FS98\\MODULES", "", "C:\\FS98\\GAUGES\\"};
        const char *text = texts[script() % 3];
        if (!writable(arguments[1], arguments[2] ? arguments[2] : 1) || arguments[2] < 32) return 0;
        strcpy((char *)(uintptr_t)arguments[1], text);
        *result = strlen(text);
        return 1;
    }
    default:
        return 0;
    }
}

/* .data a scenario may give values of its own where retail's are zero: not the service table,
 * nor the COM objects (none is none: a random one would run the fixture as code) */
static int data_may_vary(uint32_t rva)
{
    if (rva == 0x18830) return 0;       /* the frame callback: shape_data's */
    if ((rva >= 0x17ae8 && rva < 0x17b04) || (rva >= 0x17b30 && rva < 0x17b4c) || (rva >= 0x17ee0 && rva < 0x17f1c) ||
        (rva >= 0x18380 && rva < 0x1839c) || (rva >= 0x18800 && rva < 0x18830) || rva == 0x1bb28 || rva == 0x1889c)
        return 0;                       /* the lists, the host's import table, the order: shape_modules's */
    if (rva >= 0x1a4dc && rva < 0x1a500 && rva != 0x1a4ec && rva != 0x1a4f0 && rva != 0x1a4f4) return 0;
    return rva < 0x17ba0 || rva >= 0x17ba0 + 0x320;
}

/* ---- COM: DirectDraw's and Direct3D's objects --------------------------------------------
 * A fake object is a fixture block of COM_OBJECTS: its vtable one of these, each slot a stub
 * popping what the method pops (this included). Methods past an interface's own pop 4. */
static const char *const com_names[COM_KINDS] = {"DirectDraw", "Surface", "Direct3D2", "Palette"};
static const uint8_t com_pops[COM_KINDS][36] = {
    /* IDirectDraw: QI AddRef Release Compact CreateClipper CreatePalette CreateSurface
     * DuplicateSurface EnumDisplayModes EnumSurfaces FlipToGDISurface GetCaps GetDisplayMode
     * GetFourCCCodes GetGDISurface GetMonitorFrequency GetScanLine GetVerticalBlankStatus
     * Initialize RestoreDisplayMode SetCooperativeLevel SetDisplayMode WaitForVerticalBlank */
    {12, 4, 4, 4, 16, 20, 16, 12, 20, 20, 4, 12, 8, 12, 8, 8, 8, 8, 8, 4, 12, 16, 12},
    /* IDirectDrawSurface: QI AddRef Release AddAttachedSurface AddOverlayDirtyRect Blt BltBatch
     * BltFast DeleteAttachedSurface EnumAttachedSurfaces EnumOverlayZOrders Flip
     * GetAttachedSurface GetBltStatus GetCaps GetClipper GetColorKey GetDC GetFlipStatus
     * GetOverlayPosition GetPalette GetPixelFormat GetSurfaceDesc Initialize IsLost Lock
     * ReleaseDC Restore SetClipper SetColorKey SetOverlayPosition SetPalette Unlock
     * UpdateOverlay UpdateOverlayDisplay UpdateOverlayZOrder */
    {12, 4, 4, 8, 8, 24, 16, 24, 12, 12, 16, 12, 12, 8, 8, 8, 12, 8, 8, 12, 8, 8, 8, 12, 4, 20, 8, 4,
     8, 12, 12, 8, 8, 24, 8, 12},
    /* IDirect3D2: QI AddRef Release EnumDevices CreateLight CreateMaterial CreateViewport
     * FindDevice CreateDevice */
    {12, 4, 4, 12, 12, 12, 12, 12, 16},
    /* IDirectDrawPalette: QI AddRef Release GetCaps GetEntries Initialize SetEntries */
    {12, 4, 4, 8, 20, 16, 20},
};
static void **s_com_vtables[COM_KINDS];
static uint32_t s_com_first;
static uint32_t com_pop(uint32_t kind, uint32_t slot)
{
    uint32_t pop = slot < 36 ? com_pops[kind][slot] : 0;
    return pop ? pop : 4;
}
/* a new object of a kind, from the fixture's pool, or 0 when it is used up */
static uint32_t com_new(uint32_t kind)
{
    uint8_t *object;
    if (s_com_used >= COM_OBJECTS - 1) return 0;     /* the last: ARG_SURFACE's own surface */
    object = s_fixture.com[s_com_used++];
    memcpy(object, &s_com_vtables[kind], 4);
    return fixture_address(object);
}
/* the script's words, for a structure a method fills */
static void com_fill(uint32_t address, uint32_t size, uint32_t from)
{
    uint32_t at, value = script();
    for (at = from; at + 4 <= size; at += 4) {
        value ^= value << 13; value ^= value >> 17; value ^= value << 5;
        if (writable(address + at, 4)) {
            uint32_t word = value % 3 == 0 ? value : value % 3 == 1 ? (value >> 8) & 0xff : 0;
            memcpy((void *)(uintptr_t)(address + at), &word, 4);
        }
    }
}
/* a surface description's pixel format: 16-bit 555 now and then, as the code tests for */
static void com_pixel_format(uint32_t desc)
{
    static const uint32_t formats[3][4] = {{0x10, 0x7c00, 0x3e0, 0x1f}, {0x10, 0xf800, 0x7e0, 0x1f}, {8, 0, 0, 0}};
    uint32_t value = script() >> 4;
    if (s_overlay_focus && value % 4 && writable(desc + 0x54, 16)) { memcpy((void *)(uintptr_t)(desc + 0x54), formats[2], 16); return; }
    if (value % 2 && writable(desc + 0x54, 16)) memcpy((void *)(uintptr_t)(desc + 0x54), formats[(value >> 1) % 3], 16);
}
/* what a COM method's arguments point to: the rectangles, the surface description, the caps */
static void com_log_words(uint32_t address, uint32_t count)
{
    uint32_t at;
    if (!address) { put(" {null}"); return; }
    if (!writable(address, count * 4)) { put(" {?}"); return; }
    put(" {");
    for (at = 0; at < count; ++at) put(at ? " %x" : "%x", ((uint32_t *)(uintptr_t)address)[at]);
    put("}");
}
static void com_log_contents(uint32_t kind, uint32_t slot, const uint32_t *arguments)
{
    switch (kind * 64 + slot) {
    case COM_DDRAW * 64 + 6: com_log_words(arguments[1], 0x1b); break;             /* CreateSurface(desc) */
    case COM_SURFACE * 64 + 5:                  /* Blt(rect, from, rect, flags, fx): fx's fill colour */
        com_log_words(arguments[1], 4);
        com_log_words(arguments[3], 4);
        if (arguments[5]) com_log_words(arguments[5] + 0x50, 1);
        break;
    case COM_SURFACE * 64 + 12: com_log_words(arguments[1], 1); break;             /* GetAttachedSurface(caps) */
    case COM_SURFACE * 64 + 33: com_log_words(arguments[1], 4); com_log_words(arguments[3], 4); break;  /* UpdateOverlay */
    default: break;
    }
}
/* whether a return address is in 0x413910, retail's or ours (to its ret 4, found at setup) */
static uint32_t s_frame_rect_end;
static int from_frame_rect(uint32_t back)
{
    uint32_t ours = (uint32_t)(uintptr_t)fs98_host_rt_frame_rect;
    if (!s_frame_rect_end) {
        const uint8_t *code = (const uint8_t *)(uintptr_t)ours;
        uint32_t at;
        for (at = 0; at < 0x200 && !s_frame_rect_end; ++at)
            if (code[at] == 0xc2 && code[at + 1] == 4 && code[at + 2] == 0) s_frame_rect_end = ours + at + 3;
        if (!s_frame_rect_end) GIVE_UP();
    }
    return (back >= BASE + 0x13910 && back < BASE + 0x1395d) || (back >= ours && back < s_frame_rect_end);
}
typedef int32_t (SC *enum_devices_callback)(void *, char *, char *, void *, void *, void *);
static int com_semantics(uint32_t kind, uint32_t slot, uint32_t *arguments, uint64_t *result)
{
    uint32_t value = script();
    if (slot == 0) {                    /* QueryInterface(iid, out): an IDirect3D2, or none */
        uint32_t object = value % 3 ? com_new(COM_D3D2) : 0;
        if (writable(arguments[2], 4)) memcpy((void *)(uintptr_t)arguments[2], &object, 4);
        *result = object ? 0 : 0x80004002u;
        return 1;
    }
    switch (kind * 64 + slot) {
    case COM_DDRAW * 64 + 5:            /* CreatePalette(flags, entries, out, outer) */
    case COM_DDRAW * 64 + 6: {          /* CreateSurface(desc, out, outer) */
        uint32_t out = slot == 5 ? arguments[3] : arguments[2];
        uint32_t object = value % 4 ? com_new(slot == 5 ? COM_PALETTE : COM_SURFACE) : 0;
        if (writable(out, 4)) memcpy((void *)(uintptr_t)out, &object, 4);
        *result = object ? 0 : 0x887600e1u;
        return 1;
    }
    case COM_DDRAW * 64 + 11: {         /* GetCaps(driver, hel): their words after dwSize */
        /* now and then a device the code looks for: 3D, blits, overlays, and little memory */
        static const uint32_t flags[4] = {0x841, 0x41, 0x8000841, 0x2000041};
        if (arguments[1]) com_fill(arguments[1], 0x16c, 4);
        if (arguments[2]) com_fill(arguments[2], 0x16c, 4);
        if (arguments[1] && (value % 3 || s_overlay_focus) && writable(arguments[1], 0x8c)) {
            uint32_t *caps = (uint32_t *)(uintptr_t)arguments[1];
            caps[1] = flags[s_overlay_focus && value % 2 ? 0 : (value >> 3) % 4];
            if (s_overlay_focus) caps[0x10] = 0;
            if ((value >> 9) % 4 == 0) caps[0x10] = 0x25800 - ((value >> 11) & 1);      /* the memory bounds, exactly */
            if ((value >> 12) % 4 == 0) caps[0xf] = 0x14b000 - ((value >> 14) & 1);
            if ((value >> 15) % 3 == 0) caps[0x22] = 1000 + ((value >> 17) & 1);
            if (value & 0x40) caps[0x22] = (value >> 8) % 2000;
        }
        *result = value % 5 ? 0 : 0x80004005u;
        return 1;
    }
    case COM_SURFACE * 64 + 22:         /* GetSurfaceDesc(desc) */
        com_fill(arguments[1], 0x6c, 4);
        com_pixel_format(arguments[1]);
        *result = value % 5 ? 0 : 0x80004005u;
        return 1;
    case COM_SURFACE * 64 + 25:         /* Lock(rect, desc, flags, event): memory in the fixture */
        com_fill(arguments[2], 0x6c, 4);
        if (writable(arguments[2], 0x28)) {
            uint32_t memory = fixture_address(s_fixture.blocks[8]), pitch = (value >> 4) % 6 == 0 ? (value >> 8) % 300 : 16 + (value >> 8) % 64;
            memcpy((void *)(uintptr_t)(arguments[2] + 0x24), &memory, 4);
            memcpy((void *)(uintptr_t)(arguments[2] + 0x10), &pitch, 4);
        }
        *result = value % 3 ? 0 : value % 2 ? 0x887601c2u : 0x8876021cu;
        return 1;
    case COM_DDRAW * 64 + 20:           /* SetCooperativeLevel, SetDisplayMode; a surface's Restore */
    case COM_DDRAW * 64 + 21:
    case COM_SURFACE * 64 + 27:
        *result = value % 4 ? 0 : 0x80004005u;
        return 1;
    case COM_SURFACE * 64 + 17:         /* GetDC(out): a DC, and now and then a failure */
        /* a failed GetDC leaves the out as it was -- but called from 0x413910 (or ours) it writes
         * 0: there the out is a slot retail never sets, holding its caller's ecx
         * (docs/open-differences.md) */
        if (!writable(arguments[1], 4)) return 0;
        if (value % 5 || from_frame_rect(arguments[-1])) {
            uint32_t dc = value % 5 ? 0x7000 + value % 16 : 0;
            memcpy((void *)(uintptr_t)arguments[1], &dc, 4);
        }
        *result = value % 5 ? 0 : 0x887601c2u;
        return 1;
    case COM_SURFACE * 64 + 5:          /* Blt, Flip: done, lost, or another failure */
    case COM_SURFACE * 64 + 11:
        *result = value % 3 == 0 ? 0 : value % 3 == 1 ? 0x887601c2u : 0x80004005u;
        return 1;
    case COM_SURFACE * 64 + 12: {       /* GetAttachedSurface(caps, out) */
        uint32_t object = value % 4 ? com_new(COM_SURFACE) : 0;
        if (writable(arguments[2], 4)) memcpy((void *)(uintptr_t)arguments[2], &object, 4);
        *result = object ? 0 : 0x887600ffu;
        return 1;
    }
    case COM_D3D2 * 64 + 3: {           /* EnumDevices(callback, context): a few devices */
        uint32_t count = value % 4, at;
        uint8_t *scratch = s_fixture.blocks[30];
        for (at = 0; at < count; ++at) {
            uint32_t guid = fixture_address(scratch), flags = script() >> (at + 3);
            int32_t more;
            com_fill(fixture_address(scratch + 0x40), 0xcc, 0);
            com_fill(fixture_address(scratch + 0x140), 0xcc, 0);
            memcpy(scratch + 0x44, &flags, 4);
            flags >>= 5; memcpy(scratch + 0x144, &flags, 4);
            strcpy((char *)scratch + 0x220, at % 2 ? "Direct3D HAL" : "RGB Emulation");
            strcpy((char *)scratch + 0x260, "dev");
            put(" device %u", at);
            more = ((enum_devices_callback)(uintptr_t)arguments[1])(at == 2 ? 0 : (void *)(uintptr_t)guid,
                (char *)scratch + 0x220, (char *)scratch + 0x260, scratch + 0x40, (flags & 1) ? scratch + 0x140 : 0,
                (void *)(uintptr_t)arguments[2]);
            put(" -> %x", (uint32_t)more);
            if (!more) break;
        }
        *result = value % 7 ? 0 : 0x80004005u;
        return 1;
    }
    default:
        return 0;
    }
}
static void com_setup(void)
{
    uint32_t kind, slot;
    for (kind = 0; kind < COM_KINDS; ++kind) {
        s_com_vtables[kind] = calloc(64, sizeof(void *));
        for (slot = 0; slot < 64; ++slot) s_com_vtables[kind][slot] = make_stub(COM_BASE + kind * 64 + slot);
    }
}

/* the COM objects .data holds: each none or a fresh one of its kind */
static void shape_data(void)
{
    static const struct { uint32_t va, kind; } objects[] = {
        {0x41a4e4, COM_DDRAW}, {0x41a4dc, COM_D3D2}, {0x41a4e0, COM_SURFACE}, {0x41a4f8, COM_SURFACE},
        {0x41a4fc, COM_SURFACE}, {0x41a4e8, COM_PALETTE}};
    uint32_t at;
    s_com_used = 0;
    for (at = 0; at < sizeof objects / sizeof objects[0]; ++at) {
        uint32_t object = next_random() % 2 ? com_new(objects[at].kind) : 0;
        memcpy(s_fixture.data + objects[at].va - BASE - DATA_RVA, &object, 4);
    }
    {   /* the modes: the configured one (0x41a4c4..cc) and the one set (0x4183a8..b0), often 640x480 */
        static const uint32_t vas[] = {0x41a4c4, 0x41a4c8, 0x41a4cc, 0x4183a8, 0x4183ac, 0x4183b0};
        static const uint32_t usual[] = {0x280, 0x1e0, 0x10, 0x280, 0x1e0, 8};
        for (at = 0; at < 6; ++at) {
            uint32_t random = next_random(), value;
            if (random % 4 == 0) continue;
            value = random % 4 == 1 ? usual[at] : random % 4 == 2 ? (random >> 4) % 80 : usual[at] + (random >> 4) % 3 - 1;
            memcpy(s_fixture.data + vas[at] - BASE - DATA_RVA, &value, 4);
        }
    }
    for (at = 0; at < 2; ++at)          /* the exit table: an executable's -1, or a table */
        if (next_random() % 2) memset(s_fixture.data + 0x41c5c4 + at * 4 - BASE - DATA_RVA, 0xff, 4);
    s_callback0_stub_for_shaping = s_callback0_stub;
    shape_modules();
    {   /* the game's frame callback: the harness's, or none */
        uint32_t code = next_random() % 5 ? (uint32_t)(uintptr_t)s_callback0_stub : 0;
        memcpy(s_fixture.data + 0x418830 - BASE - DATA_RVA, &code, 4);
    }
    s_overlay_focus = !strcmp(CURRENT.name, "render_surface") && next_random() % 2;
    if (s_overlay_focus) {
        static const uint32_t present[] = {0x41a4e4, 0x41a4dc, 0x41a4f8};
        for (at = 0; at < 3; ++at) {
            uint32_t object;
            memcpy(&object, s_fixture.data + present[at] - BASE - DATA_RVA, 4);
            if (!object) object = com_new(at == 0 ? COM_DDRAW : at == 1 ? COM_D3D2 : COM_SURFACE);
            memcpy(s_fixture.data + present[at] - BASE - DATA_RVA, &object, 4);
        }
    }
    s_com_first = s_com_used;
    memcpy(s_fixture.com[COM_OBJECTS - 1], &s_com_vtables[COM_SURFACE], 4);
    {   /* the DirectDraw options and states: flags, or 0 and 1 */
        static const struct { uint32_t va, mask; } words[] = {
            {0x41a4c0, 0xffff}, {0x41a470, 1}, {0x418854, 1}, {0x418858, 1}, {0x41a4f4, 1}, {0x41a4ec, 7},
            {0x41a500, 1}, {0x4183b4, 1}, {0x41a4f0, 1}, {0x418858, 3}};
        for (at = 0; at < sizeof words / sizeof words[0]; ++at) {
            uint32_t random = next_random(), value = random % 3 == 0 ? 0 : (random >> 2) & words[at].mask;
            if (words[at].va == 0x41a4c0 && random % 16 >= 4) value &= ~1u;    /* DirectDraw off, now and then */
            if (words[at].va == 0x41a500 && random % 8) value = 0;              /* the opening under way, rarely */
            if (words[at].va == 0x41a4c0 && s_overlay_focus) value |= 0x202;
            memcpy(s_fixture.data + words[at].va - BASE - DATA_RVA, &value, 4);
        }
    }
}

/* the floating windows' data (blocks 5 and 13): flags, bitmap (6 and 14: its screen
 * rectangle at +0xc, the keyed flag at +0x40) and picture at +0x80 (bits in 20 and 24);
 * a picture to draw on (block 7 +0x200: bits in block 8); a small rectangle (block 10 +0x3c0) */
static void shape_picture(uint8_t *picture, uint8_t *bits)
{
    int32_t fields[4];
    uint32_t pointer = fixture_address(bits), random = next_random();
    fields[0] = (int32_t)(random % 48); fields[1] = (int32_t)((random >> 6) % 40);
    fields[2] = random % 7 == 0 ? (int32_t)((random >> 12) % 300) : (int32_t)(fields[0] + (random >> 12) % 20);
    memcpy(picture + 4, fields, 12);
    memcpy(picture + 0x18, &pointer, 4);
}
/* the modules (shape_data): up to four on the list (entries in block 12, names in 11), their
 * Linkages (block 15 + 0x80 each: id, init, deinit, level, registrations in block 9) and
 * ImportTables (block 19 + 0x80 each), the host's own imports (0x418800), and an order (block
 * 16) of some of the Linkages */
static void *s_callback0_stub_for_shaping;
static void shape_modules(void)
{
    static const char *const names[] = {"C:\\FS98\\MODULES\\A.DLL", "C:\\FS98\\MODULES\\b.dll", "a.dll", "C:\\FS98\\SIMSUI.DLL"};
    uint32_t count = next_random() % 5, at, header[7] = {0, 0, 0x20, 0x14, 0, 0, 0}, ordered = 0;
    uint8_t *data = s_fixture.data;
    #define DATA_AT(va) (data + (va) - BASE - DATA_RVA)
    for (at = 0; at < 4; ++at) {        /* the Linkages and ImportTables */
        uint32_t *linkage = (uint32_t *)(s_fixture.blocks[15] + at * 0x80), *table = (uint32_t *)(s_fixture.blocks[19] + at * 0x80);
        uint32_t random = next_random(), pairs = random % 4, pair;
        uint32_t *records = (uint32_t *)(s_fixture.blocks[9] + at * 0x40);
        memset(linkage, 0, 0x80);
        linkage[0] = random % 7 ? at + 1 : 2;
        linkage[1] = random & 0x10 ? (uint32_t)(uintptr_t)s_callback0_stub_for_shaping : 0;
        linkage[2] = random & 0x20 ? (uint32_t)(uintptr_t)s_callback0_stub_for_shaping : 0;
        linkage[4] = (random >> 6) % 4;
        linkage[5] = random & 0x100 ? fixture_address(records) : 0;
        memset(records, 0, 0x40);
        for (pair = 0; pair < (random >> 9) % 4; ++pair) { records[pair * 3] = 0x100 + pair; records[pair * 3 + 2] = random >> 12; }
        memset(table, 0, 0x80);
        for (pair = 0; pair < pairs; ++pair) table[pair * 2] = 1 + (random >> (12 + pair * 2)) % 5;
    }
    header[0] = header[1] = count;
    header[6] = count ? fixture_address(s_fixture.blocks[12]) : 0;
    for (at = 0; at < count; ++at) {
        uint32_t *entry = (uint32_t *)(s_fixture.blocks[12] + at * 0x14), random = next_random();
        char *name = (char *)s_fixture.blocks[11] + at * 0x40;
        strcpy(name, names[random % 4]);
        entry[0] = fixture_address(name);
        entry[1] = random & 4 ? at + 1 : 1 + (random >> 3) % 4;
        entry[2] = random & 0x20 ? 0x10000000u + at * 0x10000 : 0;
        entry[3] = random & 0x40 ? fixture_address(s_fixture.blocks[15] + at * 0x80) : 0;
        entry[4] = (random >> 7) % 3;
        if (entry[3] && random % 3 && ordered < 4)    /* in the order */
            memcpy(s_fixture.blocks[16] + ordered++ * 4, &entry[3], 4);
    }
    memset(s_fixture.blocks[16] + ordered * 4, 0, 4);
    memcpy(DATA_AT(0x417b30), header, 0x1c);
    {
        uint32_t order = fixture_address(s_fixture.blocks[16]);
        memcpy(DATA_AT(0x41bb28), &order, 4);
        memcpy(DATA_AT(0x41889c), &ordered, 4);
    }
    memset(DATA_AT(0x418800), 0, 0x30);  /* the host's own imports: none, or a few ids */
    for (at = 0; at < next_random() % 4; ++at) { uint32_t id = 1 + next_random() % 5; memcpy(DATA_AT(0x418800) + at * 8, &id, 4); }
    #undef DATA_AT
}
static void shape_fixture(void)
{
    static const uint8_t windows[2][3] = {{5, 6, 20}, {13, 14, 24}};
    uint32_t at;
    for (at = 0; at < 2; ++at) {
        uint8_t *data = s_fixture.blocks[windows[at][0]], *bitmap = s_fixture.blocks[windows[at][1]];
        uint32_t random = next_random(), flags = (random % 5 ? 0x80000000u : 0) | (random & 0x200) |
                                           (random % 3 == 0 ? 0x10000000u : 0) | (random % 7 == 1 ? 0x40000000u : 0), pointer;
        int32_t rect[4];
        memcpy(data + 8, &flags, 4);
        if (random % 6) memset(data + 0x7c, 0, 4);
        pointer = random % 7 ? fixture_address(bitmap) : 0;
        memcpy(data + 0xa0, &pointer, 4);
        rect[0] = (int32_t)((random >> 10) % 60) - 10; rect[1] = (int32_t)((random >> 16) % 60) - 10;
        rect[2] = rect[0] + (int32_t)((random >> 22) % 50); rect[3] = rect[1] + (int32_t)((random >> 26) % 50);
        memcpy(bitmap + 0xc, rect, 16);
        if (random & 0x400) memset(bitmap + 0x40, 0, 4);
        shape_picture(data + 0x80, s_fixture.blocks[windows[at][2]]);
    }
    shape_picture(s_fixture.blocks[7] + 0x200, s_fixture.blocks[8]);
    {
        uint32_t random = next_random();
        int32_t rect[4];
        rect[0] = (int32_t)(random % 50) - 8; rect[1] = (int32_t)((random >> 6) % 50) - 8;
        rect[2] = rect[0] + (int32_t)((random >> 12) % 40) - 4; rect[3] = rect[1] + (int32_t)((random >> 18) % 40) - 4;
        memcpy(s_fixture.blocks[10] + 0x3c0, rect, 16);
        rect[0] = (int32_t)((random >> 3) % 60) - 12; rect[1] = (int32_t)((random >> 9) % 60) - 12;
        memcpy(s_fixture.blocks[10] + 0x3e0, rect, 8);
        if (random & 0x80000000u) {     /* a window the rectangle's size, now and then: moved, not resized */
            int32_t now[4], size[2];
            memcpy(now, s_fixture.blocks[10] + 0x3c0, 16);
            size[0] = now[2] - now[0]; size[1] = now[3] - now[1];
            memcpy(s_fixture.blocks[(random >> 30) & 1 ? 5 : 13] + 0x60, size, 8);
        }
    }
}
