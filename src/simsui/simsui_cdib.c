/* CDIB (0x10 bytes, vtable 0x15fe0, runtime class 0x200b8, DYNCREATE): the DIB class of
 * MFC's animation samples, linked in but exported by nothing; its runtime class still makes
 * it creatable by name. The BITMAPINFO +4 (malloc'd, 256 colours of room), the bits +8,
 * whether it owns them +0xc.
 *
 * Its virtuals: Load(name) 0x14, Load(resource id, instance) 0x18, Load(CFile *) 0x1c,
 * Draw 0x20. A CFileException made on the stack in Load(name) has its own copy of the
 * class's vtable (0x16018). */
#include "simsui_internal.h"

#define VTABLE_CDIB 0x15fe0
#define VTABLE_FILE_EXCEPTION 0x16018
#define VTABLE_OBJECT 0x15018
#define RUNTIME_CDIB 0x200b8
#define CLASS_INIT 0x213d8
#define CDIB_SIZE 0x10

#define heap_alloc(size) CALL_C(malloc, void *, (uint32_t))(size)
#define heap_free(block) CALL_C(free, void, (void *))(block)

static void copy_bytes(void *to, const void *from, uint32_t count)
{
    uint8_t *out = to;
    const uint8_t *in = from;
    while (count--) *out++ = *in++;
}

/* 0x20476a40, CreateObject */
void *SC fs98_simsui_cdib_create_object(void)
{
    void *dib = mfc_operator_new(CDIB_SIZE);
    return dib ? fs98_simsui_cdib_construct(dib) : 0;
}
/* 0x20476aa0, _GetBaseClass: CObject's */
void *SC fs98_simsui_cdib_base_class(void) { return fs98_simsui_imports[SIMSUI_IMPORT_MFC_1871]; }
/* 0x20476ab0, GetRuntimeClass */
const void *TC fs98_simsui_cdib_runtime_class(const void *dib) { (void)dib; return IMAGE(RUNTIME_CDIB); }
/* 0x20476ac0 (to 0x20476ad0): the static AFX_CLASSINIT that registers the class */
void fs98_simsui_cdib_register(void)
{
    CALL_THIS(MFC_266, void *, (void *, void *))(IMAGE(CLASS_INIT), IMAGE(RUNTIME_CDIB));
}
/* 0x20476ae0: 16 by 16, grey */
void *TC fs98_simsui_cdib_construct(void *dib)
{
    PTR(dib, 0) = IMAGE(VTABLE_CDIB);
    D32(dib, 4) = 0;
    D32(dib, 8) = 0;
    D32(dib, 0xc) = 1;
    fs98_simsui_cdib_create(dib, 0x10, 0x10);
    return dib;
}
/* 0x20476b40, GetWidth; 0x20476b50, GetStorageWidth; 0x20476b60, GetHeight */
int32_t TC fs98_simsui_cdib_width(const void *dib) { return I32(PTR(dib, 4), 4); }
int32_t TC fs98_simsui_cdib_storage_width(const void *dib) { return (I32(PTR(dib, 4), 4) + 3) & ~3; }
int32_t TC fs98_simsui_cdib_height(const void *dib) { return I32(PTR(dib, 4), 8); }
/* 0x20476b90 */
void TC fs98_simsui_cdib_destroy(void *dib)
{
    PTR(dib, 0) = IMAGE(VTABLE_CDIB);
    if (PTR(dib, 4)) heap_free(PTR(dib, 4));
    if (D32(dib, 0xc) && PTR(dib, 8)) heap_free(PTR(dib, 8));
    PTR(dib, 0) = IMAGE(VTABLE_OBJECT);
}
/* 0x20476b70 */
void *TC fs98_simsui_cdib_delete(void *dib, uint32_t flags)
{
    fs98_simsui_cdib_destroy(dib);
    if (flags & 1) mfc_operator_delete(dib);
    return dib;
}
/* 0x20476c00, Create(width, height): an 8-bit DIB with a grey ramp, cleared; 0 when either
 * block cannot be had */
int32_t TC fs98_simsui_cdib_create(void *dib, int32_t width, int32_t height)
{
    uint8_t *info, *colour;
    uint32_t size, at;
    if (PTR(dib, 4)) heap_free(PTR(dib, 4));
    if (D32(dib, 0xc) && PTR(dib, 8)) heap_free(PTR(dib, 8));
    info = heap_alloc(0x428);
    PTR(dib, 4) = info;
    if (!info) return 0;
    size = (uint32_t)((width + 3) & ~3) * (uint32_t)height;
    PTR(dib, 8) = heap_alloc(size);
    if (!PTR(dib, 8)) {
        heap_free(PTR(dib, 4));
        D32(dib, 4) = 0;
        return 0;
    }
    info = PTR(dib, 4);
    D32(dib, 0xc) = 1;
    D32(info, 0) = 0x28;
    I32(info, 4) = width;
    I32(info, 8) = height;
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    for (at = 0x10; at < 0x28; at += 4) D32(info, at) = 0;
    colour = (uint8_t *)PTR(dib, 4) + 0x28;
    for (at = 0; at < 0x100; ++at, colour += 4) {
        colour[2] = (uint8_t)at;
        colour[1] = (uint8_t)at;
        colour[0] = (uint8_t)at;
        colour[3] = 0;
    }
    {
        uint8_t *bits = PTR(dib, 8);
        while (size--) *bits++ = 0;
    }
    return 1;
}
/* 0x20476d60, IsWinDIB(header): not an OS/2 core header */
int32_t SC fs98_simsui_cdib_is_win(const void *header) { return D32(header, 0) != 0xc; }
/* 0x20476d00, NumDIBColorEntries(info): 2, 16 or 256 for 1, 4 or 8 bits (else none), or
 * what a Windows header says it uses */
int32_t SC fs98_simsui_cdib_colour_entries(const void *info)
{
    uint32_t bits = fs98_simsui_cdib_is_win(info) ? D16(info, 0xe) : D16(info, 0xa);
    int32_t count;
    switch (bits) {
    case 1: count = 2; break;
    case 4: count = 0x10; break;
    case 8: count = 0x100; break;
    default: count = 0; break;
    }
    if (fs98_simsui_cdib_is_win(info) && D32(info, 0x20)) count = I32(info, 0x20);
    return count;
}
/* 0x20476d70, Load(CFile *): a .BMP read where the file stands (a Windows or OS/2 header,
 * its colours, the bits from the file header's offset); the old blocks given up only once
 * the new ones are whole */
int32_t TC fs98_simsui_cdib_load_file(void *dib, void *file)
{
    uint8_t file_header[0xe], header[0x28], core[0xc], triple[3];
    uint8_t *info = 0, *bits = 0;
    int32_t is_core = 0, entries;
    uint32_t start, table, size, at;
    int32_t (TC *read)(void *, void *, uint32_t) = (int32_t (TC *)(void *, void *, uint32_t))PTR(PTR(file, 0), 0x3c);
    start = VIRTUAL(file, 0x14, uint32_t, (void *))(file);
    if (read(file, file_header, 0xe) != 0xe || D16(file_header, 0) != 0x4d42) goto fail;
    if (read(file, header, 0x28) != 0x28) goto fail;
    if (D32(header, 0) != 0x28) {
        if (D32(header, 0) != 0xc) goto fail;
        VIRTUAL(file, 0x30, int32_t, (void *, int32_t, uint32_t))(file, (int32_t)(start + 0xe), 0);
        is_core = 1;
        if (read(file, core, 0xc) != 0xc) goto fail;
        D32(header, 4) = D16(core, 4);
        D32(header, 0) = 0x28;
        D32(header, 8) = D16(core, 6);
        D16(header, 0xc) = D16(core, 8);
        D16(header, 0xe) = D16(core, 0xa);
        for (at = 0x10; at < 0x28; at += 4) D32(header, at) = 0;
    }
    entries = fs98_simsui_cdib_colour_entries(header);
    table = (uint32_t)entries * 4;
    size = D32(file_header, 2) - D32(file_header, 0xa);
    info = heap_alloc(0x428);
    if (!info) goto fail;
    copy_bytes(info, header, 0x28);
    if (!is_core) {
        if ((uint32_t)read(file, info + 0x28, table) != table) goto fail;
    } else {
        uint8_t *colour = info + 0x28;
        for (at = 0; (int32_t)at < entries; ++at, colour += 4) {
            if (read(file, triple, 3) != 3) goto fail;
            colour[0] = triple[0];
            colour[1] = triple[1];
            colour[2] = triple[2];
            colour[3] = 0;
        }
    }
    bits = heap_alloc(size);
    if (!bits) goto fail;
    VIRTUAL(file, 0x30, int32_t, (void *, int32_t, uint32_t))(file, (int32_t)(D32(file_header, 0xa) + start), 0);
    if ((uint32_t)read(file, bits, size) != size) goto fail;
    if (PTR(dib, 4)) heap_free(PTR(dib, 4));
    PTR(dib, 4) = info;
    if (D32(dib, 0xc) && PTR(dib, 8)) heap_free(PTR(dib, 8));
    PTR(dib, 8) = bits;
    D32(dib, 0xc) = 1;
    return 1;
fail:
    if (info) heap_free(info);
    if (bits) heap_free(bits);
    return 0;
}
/* 0x20476fc0, Load(resource id, instance): a bitmap resource copied (Windows headers only).
 * The row is measured from the bit count, a 1-bit row as width >> 7 (rounded up) where >> 3
 * was meant: RETAIL BUG (kept): SUI-12, docs/retail-bugs.md */
int32_t TC fs98_simsui_cdib_load_resource(void *dib, uint32_t id, uint32_t instance)
{
    uint8_t header[0x28], *resource, *info = 0, *bits;
    int32_t row, table;
    uint32_t size = 0;
    resource = CALL_STD(LockResource, void *, (uint32_t))(CALL_STD(LoadResource, uint32_t, (uint32_t, uint32_t))(
        instance, CALL_STD(FindResourceA, uint32_t, (uint32_t, uint32_t, uint32_t))(instance, id & 0xffff, 2)));
    if (!resource) goto fail;
    copy_bytes(header, resource, 0x28);
    if (D32(header, 0) != 0x28) goto fail;
    table = fs98_simsui_cdib_colour_entries(header) * 4;
    row = I32(header, 4);
    switch (D16(header, 0xe)) {
    case 1: row = row & 7 ? (row >> 7) + 1 : row >> 7; break;
    case 4: row = row & 1 ? (row >> 1) + 1 : row >> 1; break;
    case 0x10: row = (int32_t)((uint32_t)row + (uint32_t)row); break;
    case 0x18: row = (int32_t)((uint32_t)row * 3); break;
    case 0x20: row = (int32_t)((uint32_t)row << 2); break;
    default: break;
    }
    size = (uint32_t)((row + 3) & ~3) * D32(header, 8);
    info = heap_alloc(0x428);
    if (!info) goto fail;
    copy_bytes(info, header, 0x28);
    copy_bytes(info + 0x28, resource + 0x28, (uint32_t)table);
    bits = heap_alloc(size);
    if (!bits) goto fail;
    copy_bytes(bits, resource + table + 0x28, size);
    if (PTR(dib, 4)) heap_free(PTR(dib, 4));
    PTR(dib, 4) = info;
    if (D32(dib, 0xc) && PTR(dib, 8)) heap_free(PTR(dib, 8));
    PTR(dib, 8) = bits;
    D32(dib, 0xc) = 1;
    return 1;
fail:
    if (info) heap_free(info);
    return 0;
}
/* 0x204771a0, Load(name): the file opened shared for reading, then Load(CFile *) */
int32_t TC fs98_simsui_cdib_load_name(void *dib, const char *name)
{
    uint8_t file[0x10], exception[0x14];
    int32_t result;
    CALL_THIS(MFC_354, void *, (void *))(file);
    CALL_THIS(MFC_350, void *, (void *))(exception);
    CALL_THIS(MFC_540, void *, (void *))(exception + 0x10);
    PTR(exception, 0) = IMAGE(VTABLE_FILE_EXCEPTION);
    D32(exception, 8) = 0;
    I32(exception, 0xc) = -1;
    CALL_THIS(MFC_860, void *, (void *, const char *))(exception + 0x10, 0);
    if (!CALL_THIS(MFC_5186, int32_t, (void *, const char *, uint32_t, void *))(file, name, 0x20, exception))
        result = 0;
    else
        result = VIRTUAL(dib, 0x1c, int32_t, (void *, void *))(dib, file);
    PTR(exception, 0) = IMAGE(VTABLE_FILE_EXCEPTION);
    CALL_THIS(MFC_800, void, (void *))(exception + 0x10);
    PTR(exception, 0) = IMAGE(VTABLE_OBJECT);
    CALL_THIS(MFC_665, void, (void *))(file);
    return result;
}
/* 0x204772c0, Draw(CDC *, x, y, width, height, source x, source y): StretchDIBits, one to one */
int32_t TC fs98_simsui_cdib_draw(void *dib, void *dc, int32_t x, int32_t y, int32_t width, int32_t height,
                                 int32_t source_x, int32_t source_y)
{
    return CALL_STD(StretchDIBits, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                                             int32_t, const void *, const void *, uint32_t, uint32_t))(
        dc ? D32(dc, 4) : 0, x, y, width, height, source_x, source_y, width, height, PTR(dib, 8), PTR(dib, 4), 0, 0xcc0020);
}
/* 0x20477310, NumColors */
int32_t TC fs98_simsui_cdib_colours(const void *dib) { return fs98_simsui_cdib_colour_entries(PTR(dib, 4)); }
/* 0x20477320, MapColorsToPalette(CDIBPal *): every pixel mapped to the palette's nearest
 * entry for its colour, and the colour table made the palette's (as far as both go), 256
 * used */
int32_t TC fs98_simsui_cdib_map_to_palette(void *dib, void *pal)
{
    uint8_t map[0x100], entries[0x400], *colour, *bits;
    int32_t count, used, at;
    uint32_t size;
    if (!pal) return 0;
    colour = (uint8_t *)PTR(dib, 4) + 0x28;
    count = fs98_simsui_cdib_colours(dib);
    for (at = 0; at < 0x100; ++at) map[at] = 0;
    for (at = 0; at < count; ++at, colour += 4)
        map[at] = (uint8_t)CALL_STD(GetNearestPaletteIndex, uint32_t, (uint32_t, uint32_t))(
            D32(pal, 4), (uint32_t)colour[0] << 16 | (uint32_t)colour[1] << 8 | colour[2]);
    bits = PTR(dib, 8);
    size = (uint32_t)((I32(PTR(dib, 4), 4) + 3) & ~3) * D32(PTR(dib, 4), 8);
    while (size--) { *bits = map[*bits]; ++bits; }
    used = fs98_simsui_pal_colour_count(pal);
    CALL_STD(GetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(pal, 4), 0, (uint32_t)used, entries);
    colour = (uint8_t *)PTR(dib, 4) + 0x28;
    for (at = 0; at < (used < count ? used : count); ++at, colour += 4) {
        colour[2] = entries[at * 4];
        colour[1] = entries[at * 4 + 1];
        colour[0] = entries[at * 4 + 2];
    }
    D32(PTR(dib, 4), 0x20) = 0x100;
    return 1;
}
/* 0x20477470: the stack CFileException's destructor; 0x20477450 its deleting destructor */
void TC fs98_simsui_file_exception_destroy(void *exception)
{
    PTR(exception, 0) = IMAGE(VTABLE_FILE_EXCEPTION);
    CALL_THIS(MFC_800, void, (void *))(AT(exception, 0x10));
    PTR(exception, 0) = IMAGE(VTABLE_OBJECT);
}
void *TC fs98_simsui_file_exception_delete(void *exception, uint32_t flags)
{
    fs98_simsui_file_exception_destroy(exception);
    if (flags & 1) mfc_operator_delete(exception);
    return exception;
}
/* 0x20477440: a CObject's destructor */
void TC fs98_simsui_object_destroy(void *object) { PTR(object, 0) = IMAGE(VTABLE_OBJECT); }
