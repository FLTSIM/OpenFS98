/* SIMSUI's free helpers: its heap (HOST's, through the import table at .data +0x2c) and the
 * parsers for the control specs its string resources hold ("name#x,y", "name#l,t,r,b",
 * "name,a,b,c"). */
#include "simsui_internal.h"

#define HOST_TABLE G_PTR(0x2002c)            /* HOST's service table, from the import link */
#define INSTANCE G32(0x213b0)                /* SIMSUI's instance, for its strings */
#define LOAD_ERROR 0x2005c                   /* the text shown when a spec will not load */
#define TOKEN_ROOM 32

/* 0x20471b00, ui_new (and ui_new_db): HOST's allocator, pool 5; it stores the block over
 * its size argument. */
void *fs98_simsui_ui_new(uint32_t size)
{
    void *volatile result = (void *)(uintptr_t)size;
    ((void (SC *)(uint32_t, uint32_t, void *volatile *, uint32_t))PTR(HOST_TABLE, 0x20c))(5, size, &result, 1);
    return result;
}
/* 0x20471b20, ui_delete */
void fs98_simsui_ui_delete(void *block)
{
    void *volatile slot = block;
    ((void (SC *)(void *volatile *))PTR(HOST_TABLE, 0x214))(&slot);
}
/* 0x20471b40: the first `wanted` in text, by characters, or null */
char *fs98_simsui_find_char(char *text, int32_t wanted)
{
    char wanted_char = (char)wanted;
    char *at = text;
    char character = *at;
    if (character != wanted_char)
        while (character) {
            at = CALL_STD(CharNextA, char *, (const char *))(at);
            character = *at;
            if (character == wanted_char) break;
        }
    return *at == wanted_char ? at : 0;
}
/* 0x20471b80: text up to `separator` into out (all of it when there is none, and then
 * null); where the next token starts */
char *fs98_simsui_next_token(char *text, char *out, char separator)
{
    char *found, *next, *from, *to;
    if (!text) return 0;
    found = fs98_simsui_find_char(text, separator);
    if (!found) {
        char *source = text;
        while ((*out++ = *source++) != 0) {}
        return 0;
    }
    next = CALL_STD(CharNextA, char *, (const char *))(found);
    if (out) {
        from = text; to = out;
        while (from != next) *to++ = *from++;
        if (from != text) to = CALL_STD(CharPrevA, char *, (const char *, const char *))(out, to);
        *to = 0;
    }
    return next;
}
/* the name before '#' into a CString */
static char *spec_name(const char *spec, void *name)
{
    char *buffer = CALL_THIS(MFC_2915, char *, (void *, int32_t))(name, 0x100);
    char *next = fs98_simsui_next_token((char *)spec, buffer, '#');
    CALL_THIS(MFC_5572, void, (void *, int32_t))(name, -1);
    return next;
}
#define ATOI(text) CALL_C(atoi, int32_t, (const char *))(text)
/* 0x20471c00: "name#x,y" */
int32_t fs98_simsui_parse_point(const char *spec, void *name, int32_t *point)
{
    char token[TOKEN_ROOM];
    char *next = spec_name(spec, name);
    if (!next) return 0;
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    point[0] = ATOI(token);
    fs98_simsui_next_token(next, token, ',');
    point[1] = ATOI(token);
    return 1;
}
/* 0x20471ca0: "name#x,y,n" */
int32_t fs98_simsui_parse_point_and(const char *spec, void *name, int32_t *point, int32_t *value)
{
    char token[TOKEN_ROOM];
    char *next = spec_name(spec, name);
    if (!next) return 0;
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    point[0] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    point[1] = ATOI(token);
    fs98_simsui_next_token(next, token, ',');
    *value = ATOI(token);
    return 1;
}
/* 0x20471d70: "name#l,t,r,b" */
int32_t fs98_simsui_parse_rect(const char *spec, void *name, int32_t *rect)
{
    char token[TOKEN_ROOM];
    char *next = spec_name(spec, name);
    if (!next) return 0;
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[0] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[1] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[2] = ATOI(token);
    fs98_simsui_next_token(next, token, ',');
    rect[3] = ATOI(token);
    return 1;
}
/* 0x20471e60: "name#l,t,r,b,n" */
int32_t fs98_simsui_parse_rect_and(const char *spec, void *name, int32_t *rect, int32_t *value)
{
    char token[TOKEN_ROOM];
    char *next = spec_name(spec, name);
    if (!next) return 0;
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[0] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[1] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[2] = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    if (!next) return 0;
    rect[3] = ATOI(token);
    fs98_simsui_next_token(next, token, ',');
    *value = ATOI(token);
    return 1;
}
/* 0x20471f80: string `id` as "name,a,b,c" (b a byte). A string that will not load is
 * reported in a message box. */
int32_t fs98_simsui_parse_font_spec(uint32_t id, void *name, int32_t *a, uint8_t *b, int32_t *c)
{
    uint8_t text[4];
    char token[TOKEN_ROOM];
    char *buffer, *next;
    CALL_THIS(MFC_540, void, (void *))(text);
    buffer = CALL_THIS(MFC_2915, char *, (void *, int32_t))(text, 0x100);
    if (!CALL_STD(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(INSTANCE, id, buffer, 0x100)) {
        CALL_STD(MFC_1200, int32_t, (const char *, uint32_t, uint32_t))(IMAGE(LOAD_ERROR), 0, 0);
        CALL_THIS(MFC_800, void, (void *))(text);
        return 0;
    }
    CALL_THIS(MFC_5572, void, (void *, int32_t))(text, -1);
    buffer = CALL_THIS(MFC_2915, char *, (void *, int32_t))(name, 0x100);
    next = fs98_simsui_next_token(CALL_THIS(MFC_2915, char *, (void *, int32_t))(text, 0), buffer, ',');
    CALL_THIS(MFC_5572, void, (void *, int32_t))(name, -1);
    if (!next) {
        CALL_THIS(MFC_800, void, (void *))(text);
        return 0;
    }
    next = fs98_simsui_next_token(next, token, ',');
    *a = ATOI(token);
    next = fs98_simsui_next_token(next, token, ',');
    *b = (uint8_t)ATOI(token);
    fs98_simsui_next_token(next, token, ',');
    *c = ATOI(token);
    CALL_THIS(MFC_800, void, (void *))(text);
    return 1;
}
