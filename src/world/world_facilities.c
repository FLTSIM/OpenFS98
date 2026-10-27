/* Facilities directory: browser transitions, output and window events.
 * Imports are a typed ABI boundary; no retail code is used by this module. */
#include "world_facilities.h"
#include "world_unwind.h"
#include "world_facilities_tables.inc"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)s_services->resolve(slot))
static const struct fs98_world_facilities_services *s_services;
static uint32_t s_font[28], s_font_live, s_font_constructed, s_cursor_live, s_paint_result;
void fs98_world_facilities_set_services(const struct fs98_world_facilities_services *services) { s_services = services; }
static void call0(uint32_t slot, void *object) { API(slot, void (TC *)(void *))(object); }
static int call1(uint32_t slot, void *object, uint32_t value) { return API(slot, int (TC *)(void *, uint32_t))(object, value); }
static uint32_t output_send(void *object, uint32_t message, uint32_t first, uint32_t second)
{ return API(0x258b0, uint32_t (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(U32(PTR(object, 0x1428), 0x20), message, first, second); }
static void cursor(uint32_t module, uint32_t id)
{ uint32_t value = API(0x25884, uint32_t (SC *)(uint32_t, uint32_t))(module, id); API(0x25888, uint32_t (SC *)(uint32_t))(value); }
static void show(void *object, int value) { call1(0x25500, object, (uint32_t)value); }
static void enable(void *object, int value) { call1(0x255c8, object, (uint32_t)value); }
static void invalidate(void *object) { API(0x25894, int (SC *)(uint32_t, void *, int))(U32(object, 0x1b64), 0, 1); }
static void string_load(void *string, uint32_t id) { call1(0x25548, string, id); }
static void string_assign(void *string, const char *text) { API(0x25578, void *(TC *)(void *, const char *))(string, text); }
static void string_append(void *string, const char *text) { API(0x25580, void *(TC *)(void *, const char *))(string, text); }
static void string_copy(void *string, void *other) { API(0x2555c, void *(TC *)(void *, void *))(string, other); }
static void string_add(void *string, void *other) { API(0x25560, void *(TC *)(void *, void *))(string, other); }
struct facilities_life {
    struct fs98_world_unwind frame;
    struct { void *object; uint32_t slot; } items[164];
    volatile uint32_t count;
};
static void life_cleanup(struct fs98_world_unwind *frame)
{
    struct facilities_life *life = (void *)frame;
    while (life->count) {
        uint32_t index = --life->count;
        call0(life->items[index].slot, life->items[index].object);
    }
}
static void life_begin(struct facilities_life *life) { life->count = 0; world_unwind_enter(&life->frame, life_cleanup); }
static void life_push(struct facilities_life *life, void *object, uint32_t slot)
{ life->items[life->count].object = object; life->items[life->count].slot = slot; ++life->count; }
static void life_construct(struct facilities_life *life, void *object, uint32_t ctor, uint32_t dtor)
{ call0(ctor, object); life_push(life, object, dtor); }
static void life_pop(struct facilities_life *life)
{ uint32_t index = --life->count; call0(life->items[index].slot, life->items[index].object); }
static void life_end(struct facilities_life *life) { life_cleanup(&life->frame); world_unwind_leave(&life->frame); }
static void string_construct(struct facilities_life *life, void *string) { life_construct(life, string, 0x255b4, 0x255c0); }
static void output_format(void *object, uint32_t offset, int paragraph)
{ API(paragraph ? 0x25550 : 0x25554, int (TC *)(void *, void *))(PTR(object, 0x1428), AT(object, offset)); }
static void output(void *object, uint32_t string) { output_send(object, 0xc2, 0, string); }
static void image(void *object, uint32_t id)
{ API(0x25858, void (TC *)(void *, uint32_t, void *, void *, uint32_t, uint16_t))(AT(object, 0x1b44), id, AT(object, 0xff4), AT(object, 0x7e8), U32(object, 0x80), 0xf100); }
static uint32_t *row(void *object, uint32_t list, uint32_t offset) { return (void *)AT(PTR(PTR(object, 0x10dc), list), offset); }
/* 0x22407210: counts are retained per list; descriptor and row bases reload
 * before every pointer-to-pointer release. Counts become zero only at the end. */
void fs98_world_facilities_release(void *object)
{
    static const uint8_t s_airport_fields[] = {0, 4, 8, 12, 16, 0x60};
    uint32_t list, offset, field;
    int32_t count;
    for (list = 0; list < 2; ++list) {
        count = (int32_t)U32(PTR(object, 0x10dc), 0x24 + list * 8);
        offset = 0;
        while (count > 0) {
            for (field = 0; field < (list ? 5u : 6u); ++field)
                ((void (SC *)(void *))PTR(s_services->host(), 0x214))(AT(row(object, 0x28 + list * 8, offset), s_airport_fields[field]));
            --count; offset += list ? 0x60 : 0x64;
        }
        ((void (SC *)(void *))PTR(s_services->host(), 0x214))(AT(PTR(object, 0x10dc), 0x28 + list * 8));
    }
    U32(PTR(object, 0x10dc), 0x24) = 0;
    U32(PTR(object, 0x10dc), 0x2c) = 0;
}
static void output_header(struct facilities_life *life, uint32_t *line, uint32_t *temporary, const uint32_t *ids, uint32_t count)
{
    uint32_t combined, index;
    string_load(temporary, ids[0]);
    API(0x25558, void *(SC *)(void *, void *, const char *))(&combined, temporary, "\t");
    life_push(life, &combined, 0x255c0);
    string_copy(line, &combined);
    life_pop(life);
    for (index = 1; index < count; ++index) {
        string_load(temporary, ids[index]); string_add(line, temporary);
        string_append(line, index + 1 == count ? "\n" : "\t");
    }
}
static void output_field(uint32_t *line, uint32_t *temporary, const char *text, int first)
{
    if (!*text) { if (first) string_copy(line, temporary); else string_add(line, temporary); }
    else { if (first) string_assign(line, text); else string_append(line, text); }
}
static void output_prepare(void *object, uint32_t paragraph)
{
    output_send(object, 0xcf, 0, 0);
    API(0x2554c, void (TC *)(void *, int, int))(PTR(object, 0x1428), 0, -1);
    output_send(object, 0x303, 0, 0);
    output_format(object, paragraph, 1);
    API(0x2554c, void (TC *)(void *, int, int))(PTR(object, 0x1428), 0, 0);
    output_send(object, 0xcf, 1, 0);
    cursor(0, 0x7f02);
}
/* 0x224073b0: descriptor place arrays borrow the locals until this call ends.
 * Retail leaves the expired addresses in the descriptor after return. */
void fs98_world_facilities_output(void *object, uint32_t resource, uint32_t mode)
{
    struct facilities_life life;
    uint32_t title, line, temporary, regions[50], countries[50], states[50];
    const char *places[3][50];
    uint32_t index, offset, end_offset;
    int32_t count, ends;
    int airports_present = 1, navaids_present = 1;
    uint8_t message[28];
    void *descriptor;
    life_begin(&life);
    string_construct(&life, &title); string_construct(&life, &line); string_construct(&life, &temporary);
    for (index = 0; index < 50; ++index) string_construct(&life, &regions[index]);
    for (index = 0; index < 50; ++index) string_construct(&life, &countries[index]);
    for (index = 0; index < 50; ++index) string_construct(&life, &states[index]);
    string_load(&title, resource);
    if (mode == 20) {
        U32(PTR(object, 0x10dc), 0x10) = 1;
        PTR(PTR(object, 0x10dc), 0x14) = places[0]; places[0][0] = 0;
        string_load(&countries[0], 0xf933); places[1][0] = (char *)countries[0];
        PTR(PTR(object, 0x10dc), 0x18) = places[1]; places[2][0] = (char *)title;
        PTR(PTR(object, 0x10dc), 0x1c) = places[2];
    } else if (mode == 10) {
        if (resource == 0xf934) {
            U32(PTR(object, 0x10dc), 0x10) = 2;
            string_load(&regions[0], 0xf997); places[0][0] = (char *)regions[0]; places[0][1] = 0;
            PTR(PTR(object, 0x10dc), 0x14) = places[0];
            string_load(&countries[0], 0xf993); places[1][0] = (char *)countries[0];
            string_load(&countries[1], 0xf992); places[1][1] = (char *)countries[1];
            PTR(PTR(object, 0x10dc), 0x18) = places[1]; PTR(PTR(object, 0x10dc), 0x1c) = places[2];
            places[2][0] = places[2][1] = 0;
        } else if (resource == 0xf931) {
            U32(PTR(object, 0x10dc), 0x10) = 3;
            string_load(&regions[0], 0xf997); places[0][0] = (char *)regions[0];
            string_load(&regions[1], 0xf994); places[0][1] = (char *)regions[1];
            string_load(&regions[2], 0xf995); places[0][2] = (char *)regions[2];
            PTR(PTR(object, 0x10dc), 0x14) = places[0];
            string_load(&countries[0], 0xf996); places[1][0] = (char *)countries[0];
            PTR(PTR(object, 0x10dc), 0x18) = places[1]; places[1][1] = places[1][2] = 0;
            PTR(PTR(object, 0x10dc), 0x1c) = places[2]; places[2][0] = places[2][1] = places[2][2] = 0;
        } else {
            U32(PTR(object, 0x10dc), 0x10) = 1; places[0][0] = (char *)title;
            PTR(PTR(object, 0x10dc), 0x14) = places[0]; PTR(PTR(object, 0x10dc), 0x18) = places[1]; places[1][0] = 0;
            PTR(PTR(object, 0x10dc), 0x1c) = places[2]; places[2][0] = 0;
        }
    }
    if (U32(object, 0x197c) == API(0x2589c, uint32_t (SC *)(uint32_t, int))(U32(object, 0x197c), -21)) {
        static const uint32_t s_fields[] = {0xf970, 0xf971, 0xf972, 0xf973, 0xf974};
        static const uint32_t s_runways[] = {0xf975, 0xf977, 0xf976, 0xf978, 0xf979};
        output_prepare(object, 0x14f0);
        U32(PTR(object, 0x10dc), 0x20) = 1;
        descriptor = PTR(object, 0x10dc); ((void (SC *)(void *))PTR(descriptor, 12))(descriptor);
        count = (int32_t)U32(PTR(object, 0x10dc), 0x24); output_format(object, 0x1478, 0);
        if (!count) airports_present = 0;
        else {
            string_assign(&line, "\n"); output(object, line);
            for (offset = 0; count > 0; --count, offset += 0x64) {
                output_format(object, 0x143c, 0);
                string_assign(&line, PTR(row(object, 0x28, offset), 12)); string_append(&line, ", ");
                string_load(&temporary, 0xf933);
                index = API(0x256c8, int (*)(const char *, const char *))((char *)temporary, PTR(row(object, 0x28, offset), 4)) ? 4 : 8;
                string_append(&line, PTR(row(object, 0x28, offset), index)); string_append(&line, " - ");
                string_append(&line, PTR(row(object, 0x28, offset), 16)); string_append(&line, "\n\n"); output(object, line);
                output_format(object, 0x14b4, 0); output_header(&life, &line, &temporary, s_fields, 5); output(object, line);
                output_format(object, 0x1478, 0);
                string_load(&temporary, 0xf97a); output_field(&line, &temporary, (char *)row(object, 0x28, offset) + 0x14, 1); string_append(&line, "\t");
                for (index = 0; index < 4; ++index) {
                    static const uint8_t s_offsets[] = {0x49, 0x1f, 0x34, 0x51};
                    output_field(&line, &temporary, (char *)row(object, 0x28, offset) + s_offsets[index], 0);
                    string_append(&line, index == 3 ? "\n\n" : "\t");
                }
                output(object, line);
                output_format(object, 0x14b4, 0); output_header(&life, &line, &temporary, s_runways, 5); output(object, line);
                output_format(object, 0x1478, 0);
                ends = (int32_t)U32(row(object, 0x28, offset), 0x5c);
                for (end_offset = 0; ends > 0; --ends, end_offset += 0x2c) {
                    uint32_t number, suffix, surface;
                    uint32_t runway_text;
                    void *end;
                    string_load(&temporary, 0xf97a);
                    end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); number = U32(end, 0);
                    if (!number) string_copy(&line, &temporary);
                    else {
                        string_construct(&life, &runway_text);
                        end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); number = U32(end, 0);
                        if ((int32_t)number > 36) {
                            if (number <= 40) { static const uint32_t s_special[] = {0xf98e, 0xf98d, 0xf98f, 0xf990}; string_load(&temporary, s_special[number - 37]); }
                            string_copy(&line, &temporary);
                        } else {
                            suffix = U32(end, 4);
                            if (suffix <= 4) {
                                if (suffix == 4) string_assign(&line, "H");
                                API(0x25698, char *(*)(int, char *, int))((int)number, (char *)runway_text, 10);
                                if (suffix == 4) string_append(&line, (char *)runway_text);
                                else { string_assign(&line, (char *)runway_text); if (suffix) string_append(&line, suffix == 1 ? "L" : suffix == 2 ? "R" : "C"); }
                            }
                        }
                        life_pop(&life);
                    }
                    string_append(&line, "\t"); string_load(&temporary, 0xf97a);
                    end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); output_field(&line, &temporary, (char *)end + 12, 0);
                    string_append(&line, "\t"); end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); surface = U32(end, 8);
                    if (surface >= 1 && surface <= 10) { static const uint32_t s_surface[] = {0xf97c,0xf97d,0xf97f,0xf97e,0xf988,0xf989,0xf98a,0xf98b,0xf98c,0xf987}; string_load(&temporary, s_surface[surface - 1]); }
                    string_add(&line, &temporary); string_append(&line, "\t"); string_load(&temporary, 0xf97a);
                    end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); output_field(&line, &temporary, (char *)end + 0x14, 0); string_append(&line, "\t");
                    end = AT(PTR(row(object, 0x28, offset), 0x60), end_offset); output_field(&line, &temporary, (char *)end + 0x1f, 0); string_append(&line, "\n"); output(object, line);
                }
                string_assign(&line, "\n\n"); output(object, line);
                API(0x2588c, int (SC *)(void *, uint32_t, uint32_t, uint32_t, uint32_t))(message, 0, 0, 0, 0);
            }
            fs98_world_facilities_release(object);
        }
    } else {
        static const uint32_t s_fields[] = {0xf980,0xf970,0xf981,0xf972,0xf973,0xf971};
        output_prepare(object, 0x158c);
        U32(PTR(object, 0x10dc), 0x20) = 2;
        descriptor = PTR(object, 0x10dc); ((void (SC *)(void *))PTR(descriptor, 12))(descriptor);
        count = (int32_t)U32(PTR(object, 0x10dc), 0x2c);
        if (!count) navaids_present = 0;
        else {
            for (offset = 0; count > 0; --count, offset += 0x60) {
                string_assign(&line, "\n"); output(object, line); output_format(object, 0x143c, 0);
                string_assign(&line, PTR(row(object, 0x30, offset), mode == 20 ? 8 : 4)); string_append(&line, " - ");
                string_append(&line, PTR(row(object, 0x30, offset), 16)); string_append(&line, "\n\n"); output(object, line);
                output_format(object, 0x14b4, 0); output_header(&life, &line, &temporary, s_fields, 6); output(object, line); output_format(object, 0x1478, 0);
                string_load(&temporary, 0xf97a); index = U32(row(object, 0x30, offset), 0x5c);
                if (index & 1) string_load(&temporary, 0xf982);
                else if (index & 4) string_load(&temporary, 0xf985);
                else if ((index & 8) && (index & 0xf982)) string_load(&temporary, 0xf984);
                else if (index & 2) string_load(&temporary, 0xf986);
                string_copy(&line, &temporary); string_append(&line, "\t"); string_load(&temporary, 0xf97a);
                for (index = 0; index < 5; ++index) {
                    static const uint8_t s_offsets[] = {0x14,0x51,0x1f,0x34,0x49};
                    output_field(&line, &temporary, (char *)row(object, 0x30, offset) + s_offsets[index], 0); string_append(&line, index == 4 ? "\n" : "\t");
                }
                output(object, line); API(0x2588c, int (SC *)(void *, uint32_t, uint32_t, uint32_t, uint32_t))(message,0,0,0,0);
            }
            string_assign(&line, "\n\n"); output(object, line); fs98_world_facilities_release(object);
        }
    }
    if (!airports_present || !navaids_present) {
        output_format(object, 0x14b4, 0); string_load(&temporary, !airports_present ? 0xf97b : 0xf991);
        string_assign(&line, "\n"); string_add(&line, &temporary); output(object, line);
    }
    cursor(0, 0x7f00); life_end(&life);
}
/* 0x224086e0: index 100 aliases +0x1420, the history index itself.
 * Retain its reload after storing the mode through that aliased entry. */
void fs98_world_facilities_transition(void *object, uint32_t mode, uint32_t resource, uint32_t back)
{
    struct facilities_life life;
    uint32_t text;
    if (mode == 40) {
        image(object, 0x70a); invalidate(object); life_begin(&life); string_construct(&life, &text); string_load(&text, resource);
        API(0x257e0, void (TC *)(void *, const char *))(AT(object, 0x1cfc), (char *)text);
        call1(0x2573c, AT(object, 0x195c), 1);
        fs98_world_facilities_output(object, resource, U32(object, 0x10f4));
        U32(object, 0x10f4) = 40;
        show(PTR(object, 0x1428),5); enable(AT(object, 0x1b44),0); show(AT(object, 0x1cfc),5); show(AT(object, 0x195c),5); show(AT(object, 0x1a50),5);
        enable(AT(object, 0x1860),1); call0(0x25590, AT(object, 0x1628)); life_end(&life);
    } else if (mode == 10 || mode == 20) {
        if (mode == 10) U32(object,0x10e8) = 255;
        U32(object,0x10f4) = mode; U32(object,0x10f8) = mode == 10 ? 9 : 50;
        PTR(object,0x10fc) = (void *)(mode == 10 ? s_facilities_regions : s_facilities_countries);
        enable(AT(object,0x1b44),1); image(object,mode == 10 ? 0x6a4 : 0x6c1); invalidate(object); show(AT(object,0x1b44),5);
        U32(object,0x10e0) = API(0x25880,uint32_t (SC *)(uint32_t,uint32_t,uint32_t,int,int,uint32_t))(s_services->module(),mode == 10 ? 0x6c3 : 0x6d6,0,0,0,0x2040);
        show(AT(object,0x1cfc),0); show(AT(object,0x195c),0); show(AT(object,0x1a50),0); show(PTR(object,0x1428),0);
        enable(AT(object,0x1860),mode == 20); call0(0x25590,AT(object,0x1628));
    }
    if (!back) {
        uint32_t index = U32(object,0x1420), next = index + 1;
        if ((int32_t)next <= 100) index = next;
        U32(object,0x1420) = index; U32(object,0x1100+index*8) = mode; U32(object,0x1104+U32(object,0x1420)*8) = resource;
        if ((int32_t)U32(object,0x1420) > 0) enable(AT(object,0x1764),1);
    }
}
/* 0x22408a40: palette index from the bottom-up bitmap's aligned scanline. */
int fs98_world_facilities_pixel(void *object, uint32_t bitmap, int x, int y)
{
    uint8_t data[0x54];
    int32_t stride;
    (void)object;
    if (!bitmap || !API(0x254c0,int (SC *)(uint32_t,int,void *))(bitmap,0x54,data)) return -1;
    stride = (int32_t)((U32(data,4)+3)&~3u);
    return *(uint8_t *)((uintptr_t)PTR(data,0x14) + (uint32_t)((U32(data,8)-(uint32_t)y-1)*(uint32_t)stride) + (uint32_t)x);
}
/* 0x22408aa0 */
int fs98_world_facilities_paint(void *object, uint32_t id)
{
    struct facilities_life life;
    uint8_t dc[16], client[20], dib[112];
    uint32_t index, offset, old;
    void *node;
    life_begin(&life); life_construct(&life,dc,0x254f8,0x25538);
    API(0x25530,void *(TC *)(void *,void *))(client,object); life_push(&life,client,0x25534);
    if (!id) { invalidate(object); API(0x2587c,int (SC *)(uint32_t))(U32(object,0x1b64)); }
    for (index=0; (int32_t)index<(int32_t)U32(object,0x10f8); ++index) if (U32(PTR(object,0x10fc),index*20)==id) break;
    if ((int32_t)index>=(int32_t)U32(object,0x10f8)) { life_end(&life); return 0; }
    life_construct(&life,dib,0x256dc,0x256e4); offset=index*20;
    node=AT(PTR(object,0x10fc),offset);
    API(0x256e0,int (TC *)(void *,uint32_t,uint32_t,void *,uint32_t))(dib,U32(node,8),s_services->module(),0,0);
    old=API(0x254c4,uint32_t (SC *)(uint32_t))(U32(client,4)); call1(0x2553c,dc,old);
    API(0x25540,void *(SC *)(uint32_t,uint32_t))(U32(dc,4),U32(dib,0x60));
    API(0x25544,void *(TC *)(void *,void *,int))(client,AT(object,0x7e8),0);
    API(0x254d0,uint32_t (SC *)(uint32_t))(U32(client,4));
    node=AT(PTR(object,0x10fc),offset);
    API(0x254c8,int (SC *)(uint32_t,int,int,int,int,uint32_t,int,int,uint32_t))(U32(client,4),(int)U32(node,12),(int)U32(node,16),(int)U32(dib,8),(int)U32(dib,12),U32(dc,4),0,0,0xcc0020);
    life_end(&life); return 1;
}
/* 0x224071c0 tests screen coordinates against the map only if active. */
int fs98_world_facilities_hit(void *object,int x,int y)
{
    struct fs98_world_rect rect;
    uint32_t window=API(0x25864,uint32_t (SC *)(void))();
    if (API(0x255cc,void *(SC *)(uint32_t))(window)!=object) return 0;
    API(0x258a8,int (SC *)(uint32_t,void *))(U32(object,0x1b64),&rect);
    return API(0x258ac,int (SC *)(void *,int,int))(&rect,x,y);
}
static void activate(void *object,uint32_t selected)
{
    uint32_t index;
    for(index=0;(int32_t)index<(int32_t)U32(object,0x10f8);++index) if(U32(PTR(object,0x10fc),index*20)==selected) {
        if(U32(object,0x10f4)==10&&selected==0xc1) fs98_world_facilities_transition(object,20,0,0);
        else fs98_world_facilities_transition(object,40,U32(PTR(object,0x10fc),index*20+4),0);
        break;
    }
}
void fs98_world_facilities_mouse(void *object,uint32_t hit,int x,int y,int click)
{
    void *picture=AT(object,0x1b44);
    if(fs98_world_facilities_hit(object,x,y)&&API(0x255c4,int (TC *)(void *))(picture)) {
        if(click) {
            activate(object,U32(object,0x10e4)); cursor(0,0x7f00); return;
        } else {
            int32_t point[2]={x,y}; uint32_t selected;
            API(0x2586c,int (SC *)(uint32_t,void *))(U32(object,0x1b64),point);
            selected=(uint32_t)fs98_world_facilities_pixel(object,U32(object,0x10e0),point[0],point[1]); U32(object,0x10e4)=selected;
            if((int32_t)selected<255) { if(!s_cursor_live) {cursor(s_services->module(),0x6c2);s_cursor_live=1;} }
            else if(s_cursor_live) {cursor(0,0x7f00);s_cursor_live=0;}
            if((selected!=U32(object,0x10e8)||U32(object,0x1424)==1)&&(int32_t)selected<255) {
                call0(0x25590,picture); s_paint_result=fs98_world_facilities_paint(object,0);
                s_paint_result=fs98_world_facilities_paint(object,U32(object,0x10e4)); U32(object,0x1424)=0;
            }
            if((int32_t)U32(object,0x10e4)<255)U32(object,0x10e8)=U32(object,0x10e4);
        }
    }
    (void)hit; call0(0x25584,object);
}
/* 0x22409230 */
int fs98_world_facilities_command(void *object,uint32_t command,uint32_t parameter)
{
    uint32_t id=command&0xffff,code=command>>16,selected,index,count;void *nodes;
    if(id==(uint32_t)API(0x255d8,int (TC *)(void *))(AT(object,0x1b44))&&U32(object,0x10f4)) {
        if(code==0x100) {
            selected=U32(object,0x10e8);
            if(selected==0xffffffff)U32(object,0x10e8)=0xc1;
            else if(selected!=255)s_paint_result=fs98_world_facilities_paint(object,selected);
        } else if(code==0x200) {U32(object,0x1424)=1;s_paint_result=fs98_world_facilities_paint(object,0);}
        else if(code==0x7e9&&U32(object,0x1c08)==0x20) {U32(object,0x10e4)=U32(object,0x10e8);activate(object,U32(object,0x10e8));cursor(0,0x7f00);}
    } else if(code==1) {
        uint32_t focus=API(0x258bc,uint32_t (SC *)(void))();
        if(API(0x255cc,void *(SC *)(uint32_t))(focus)==AT(object,0x1b44)&&(int32_t)id>=3&&(int32_t)id<=4) {
            count=U32(object,0x10f8);nodes=PTR(object,0x10fc);selected=U32(object,0x10e8);
            for(index=0;(int32_t)index<(int32_t)count;++index)if(U32(nodes,index*20)==selected) {
                if(id==3){++index;if((int32_t)index>=(int32_t)count)index=0;}else if(!index--)index=count-1;
                U32(object,0x10e8)=U32(nodes,index*20);s_paint_result=fs98_world_facilities_paint(object,0);s_paint_result=fs98_world_facilities_paint(object,U32(object,0x10e8));break;
            }
        }
    }
    return API(0x257ac,int (TC *)(void *,uint32_t,uint32_t))(object,command,parameter);
}
void fs98_world_facilities_accept(void *object)
{
    uint32_t focus=API(0x258bc,uint32_t (SC *)(void))();
    if(API(0x255cc,void *(SC *)(uint32_t))(focus)==AT(object,0x1b44)) {
        /* The USA exception tests +0x10e4; lookup uses +0x10e8. */
        uint32_t index,selected=U32(object,0x10e8);
        for(index=0;(int32_t)index<(int32_t)U32(object,0x10f8);++index) if(U32(PTR(object,0x10fc),index*20)==selected) {
            if(U32(object,0x10f4)==10&&U32(object,0x10e4)==0xc1)fs98_world_facilities_transition(object,20,0,0);
            else fs98_world_facilities_transition(object,40,U32(PTR(object,0x10fc),index*20+4),0);
            break;
        }
        cursor(0,0x7f00);
    }
}
void fs98_world_facilities_close(void *object)
{
    U32(object,0x1db4)=2;U32(PTR(object,0x10dc),0)=0;
    API(0x254bc,int (SC *)(uint32_t))(U32(object,0x10e0));
    if(PTR(object,0x1428)) {void *output=PTR(object,0x1428);((void *(TC *)(void *,uint32_t))PTR(PTR(output,0),4))(output,1);}
    call0(0x257b4,object);
}
void fs98_world_facilities_home(void *object) {fs98_world_facilities_transition(object,10,U32(object,0x1104+U32(object,0x1420)*8),0);}
void fs98_world_facilities_refresh(void *object)
{
    uint32_t index=U32(object,0x1420),mode=index?U32(object,0x10f8+index*8):10;
    fs98_world_facilities_output(object,U32(object,0x1104+index*8),mode);
    API(0x25868,int (SC *)(uint32_t,void *))(U32(object,0x1b64),0);
}
void fs98_world_facilities_back(void *object)
{
    uint32_t index=U32(object,0x1420);if((int32_t)(index-1)>=0)--index;
    U32(object,0x1420)=index;fs98_world_facilities_transition(object,U32(object,0x1100+index*8),U32(object,0x1104+index*8),1);
    if(!U32(object,0x1420)){enable(AT(object,0x1764),0);call0(0x25590,AT(object,0x1628));}
}
void fs98_world_facilities_deinit(void)
{if(s_font_constructed){s_font_constructed=0;call0(0x2579c,s_font);}s_font_live=0;s_cursor_live=s_paint_result=0;}
void *fs98_world_facilities_control_delete(void *object,uint32_t flags,int output_control)
{call0(output_control?0x254f0:0x25564,object);if(flags&1)API(0x25520,void (*)(void *))(object);return object;}
struct facilities_allocation {struct fs98_world_unwind frame;void *object;};
static void facilities_allocation_cleanup(struct fs98_world_unwind *frame)
{API(0x25520,void (*)(void *))(((struct facilities_allocation *)frame)->object);}
/* 0x22408c80: native rich-edit allocation and per-device text formats. */
int fs98_world_facilities_init(void *object)
{
    struct facilities_life life;
    uint32_t text,dc,dpi_x,dpi_y,index;
    uint8_t dib[112];struct fs98_world_rect rect;void *output_control;
    static const uint32_t s_buttons[]={0x1628,0x1860,0x1764};
    static const uint32_t s_characters[]={0x143c,0x1478,0x14b4};
    static const uint32_t s_tabs0[]={1200,2500,4400,6300};
    static const uint32_t s_tabs1[]={1000,1800,2700,4600,6600};
    if(!API(0x257c0,int (TC *)(void *))(object))return 1;
    if(!((int (TC *)(void *,uint32_t,uint32_t,uint32_t))PTR(PTR(object,0),0xe4))(object,0x14f,s_services->resource_module(),15))return 0;
    API(0x257c4,int (TC *)(void *,uint32_t,void *,void *,uint32_t))(AT(object,0x1c14),0xf831,AT(object,0xff4),AT(object,0x7e8),2);
    life_begin(&life);string_construct(&life,&text);
    API(0x255b8,int (TC *)(void *,void *))(AT(object,0x1c14),&text);
    API(0x255bc,void (TC *)(void *,const char *))(object,(char *)text);
    life_construct(&life,dib,0x256dc,0x256e4);
    API(0x256e0,int (TC *)(void *,uint32_t,uint32_t,void *,uint32_t))(dib,0x70b,U32(object,0x80),AT(object,0x7e8),0);
    for(index=0;index<2;++index){void *button=AT(object,index?0x1a50:0x195c);((int (TC *)(void *,void *,uint32_t,void *,void *,uint32_t))PTR(PTR(button,0),0xc4))(button,AT(object,0x530),0x134,dib,AT(object,0x7e8),s_services->resource_module());}
    if(!s_font_live){void *font;s_font_live=1;font=API(0x257d0,void *(SC *)(int))(12);API(0x2578c,void *(TC *)(void *,void *))(s_font,font);s_font_constructed=1;s_services->at_exit(fs98_world_facilities_deinit);}
    s_font[2]=0xffffff;API(0x257dc,void (TC *)(void *,void *))(AT(object,0x1cfc),s_font);
    API(0x25794,int (TC *)(void *,uint32_t,void *,void *,uint32_t,uint16_t))(AT(object,0x1b44),0x6a4,AT(object,0xff4),AT(object,0x7e8),U32(object,0x80),0xf100);
    for(index=0;index<3;++index){struct fs98_world_rect zero={0,0,0,0};API(0x257c8,int (TC *)(void *,void *,void *,uint32_t,void *,void *,uint32_t,int,uint32_t,int,int,struct fs98_world_rect))(AT(object,s_buttons[index]),AT(object,0x5a0),AT(object,0x610),0x132,AT(object,0xff4),AT(object,0x7e8),1,-1,s_services->resource_module(),0,0,zero);}
    API(0x25870,int (SC *)(void *,void *))(&rect,AT(object,0x142c));API(0x25744,void (SC *)(void *,void *))(&rect,AT(object,0x1724));
    *(struct fs98_world_rect *)AT(object,0x142c)=rect;
    ((int (TC *)(void *))PTR(PTR(AT(object,0x1724),0),0x60))(AT(object,0x1724));
    API(0x254b4,void (SC *)(void))();
    output_control=API(0x2550c,void *(*)(uint32_t))(0x40);
    /* CWnd construction has no external throwing boundary before its return;
     * allocation cleanup is registered separately below. */
    if(output_control){
        struct facilities_allocation allocation;
        allocation.object=output_control;
        world_unwind_enter(&allocation.frame,facilities_allocation_cleanup);
        call0(0x255a4,output_control);PTR(output_control,0)=s_services->output_vtable;
        world_unwind_leave(&allocation.frame);
    }
    PTR(object,0x1428)=output_control;
    API(0x25680,int (TC *)(void *,uint32_t,void *,void *,uint32_t))(output_control,0x403108c4,AT(object,0x142c),object,0xede);
    dc=API(0x25874,uint32_t (SC *)(uint32_t))(0);dpi_x=API(0x254cc,uint32_t (SC *)(uint32_t,int))(dc,0x58);dpi_y=API(0x254cc,uint32_t (SC *)(uint32_t,int))(dc,0x5a);API(0x25878,int (SC *)(uint32_t,uint32_t))(0,dc);
    for(index=0;index<3;++index){uint32_t offset=s_characters[index];U32(object,offset)=0x3c;U32(object,offset+4)=0xa0000007;U32(object,offset+8)=index==0?5:index==1?0:1;U32(object,offset+12)=API(0x254e0,uint32_t (SC *)(int,int,int))(index?160:200,96,(int)dpi_y);U32(object,offset+16)=0;U32(object,offset+20)=0;AT(object,offset)[24]=AT(object,offset)[25]=0;API(0x254e8,char *(SC *)(char *,const char *,int))((char *)AT(object,offset+26),index?"Verdana":"Arial",32);}
    for(index=0;index<2;++index){uint32_t offset=index?0x158c:0x14f0,j;U32(object,offset)=0x9c;U32(object,offset+4)=0x19;U32(object,offset+12)=0x64;*(uint16_t *)AT(object,offset+24)=1;*(uint16_t *)AT(object,offset+26)=(uint16_t)(index?5:4);for(j=0;j<(index?5u:4u);++j)U32(object,offset+28+j*4)=API(0x254e0,uint32_t (SC *)(int,int,int))((int)(index?s_tabs1[j]:s_tabs0[j]),96,(int)dpi_x);}
    U32(object,0x1420)=0xffffffff;fs98_world_facilities_transition(object,10,0,0);U32(object,0x10e8)=0xffffffff;
    if(PTR(object,0x10dc)&&U32(object,0x10dc)!=0xcdcdcdcd)call0(0x25590,AT(object,0x1628));
    life_end(&life);return 0;
}
