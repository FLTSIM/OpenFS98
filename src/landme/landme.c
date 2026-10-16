/* LANDME.DLL -- module id 26: the "Land Me" landing assistant.
 *
 * MAIN starts the assistant through +0x18.  A CHAIN list 3 callback then finds
 * the closest runway through FE, builds its midpoint and orientation, and
 * steers GLOBAL's requested position, heading, speed and altitude through a
 * twelve-phase approach.  Guidance messages go through WINMSG; phase one may
 * also post command 0x100d7 to the host.
 *
 * Retail is one hand-written assembly state machine.  This is a transliteration
 * of all 4,094 bytes.  The complete 0x854-byte .data image is retained because
 * the code's tables are unaligned and its state offsets are part of the useful
 * evidence.  It is still a 32-bit compatibility implementation: the image
 * contains live pointers and the imported ABI is Win32 stdcall.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define LANDME_IMAGE_SIZE 0x854
#define LANDME_IMPORTS    0x7f8
#define LANDME_LINKAGE    0x838

typedef uint16_t __attribute__((may_alias)) landme_u16;
typedef uint32_t __attribute__((may_alias)) landme_u32;

struct landme_image {
    uint8_t bytes[LANDME_IMAGE_SIZE];
};

extern struct landme_image landme_image;

#define D              (landme_image.bytes)
#define D8(offset)     (*(uint8_t *)(D + (offset)))
#define D16(offset)    (*(landme_u16 *)(D + (offset)))
#define D32(offset)    (*(landme_u32 *)(D + (offset)))
#define DPTR(offset)   (*(uint8_t **)(D + (offset)))

#define IMPORT(index)  (*(void **)(D + LANDME_IMPORTS + 8 * (index) + 4))
#define HOST           IMPORT(0)
#define G              ((uint8_t *)IMPORT(1))
#define G8(offset)     (*(uint8_t *)(G + (offset)))
#define G16(offset)    (*(landme_u16 *)(G + (offset)))
#define G32(offset)    (*(landme_u32 *)(G + (offset)))
#define MAIN           IMPORT(2)
#define CHAIN          IMPORT(3)
#define FE             IMPORT(4)
#define UTIL           IMPORT(5)
#define WINMSG         IMPORT(6)

#define SLOT(module, offset, type) \
    (*(type *)((uint8_t *)(module) + (offset)))

typedef void (FS98_STDCALL *landme_tick_fn)(uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *chain_change_fn)(uint32_t, landme_tick_fn);
typedef void (FS98_STDCALL *main_land_fn)(void *, void *, void *, void *);
typedef uint32_t (FS98_STDCALL *fe_search_fn)(void *, uint32_t, void *);
typedef uint32_t (FS98_STDCALL *fe_next_fn)(uint16_t, void *, void *);
typedef void (FS98_STDCALL *util_sincos_fn)(int16_t, int16_t *, int16_t *);
typedef void (FS98_STDCALL *util_rotate_fn)(int32_t, int32_t, int16_t, int16_t,
                                            int32_t *, int32_t *);
typedef int32_t (FS98_STDCALL *util_atan_fn)(int32_t, int32_t);
typedef void (FS98_STDCALL *util_convert_fn)(const void *, const void *, void *);
typedef void (FS98_STDCALL *util_unpack_fn)(const void *, void *);
typedef void (FS98_STDCALL *winmsg_show_fn)(const char *, uint32_t, uint32_t,
                                            uint32_t);

#define CHAIN_ADD       SLOT(CHAIN, 0x18, chain_change_fn)
#define CHAIN_REMOVE    SLOT(CHAIN, 0x1c, chain_change_fn)
#define MAIN_LAND       SLOT(MAIN, 0x70, main_land_fn)
#define FE_NEXT         SLOT(FE, 0x20, fe_next_fn)
#define FE_SEARCH       SLOT(FE, 0x54, fe_search_fn)
#define UTIL_SINCOS     SLOT(UTIL, 0x2c, util_sincos_fn)
#define UTIL_ROTATE     SLOT(UTIL, 0x34, util_rotate_fn)
#define UTIL_ATAN       SLOT(UTIL, 0x3c, util_atan_fn)
#define UTIL_METRES_TO_LONGITUDE SLOT(UTIL, 0x40, util_convert_fn)
#define UTIL_LONGITUDE_TO_METRES SLOT(UTIL, 0x48, util_convert_fn)
#define UTIL_UNPACK     SLOT(UTIL, 0x60, util_unpack_fn)
#define WINMSG_SHOW     SLOT(WINMSG, 0x18, winmsg_show_fn)

/* State at retail's offsets. */
#define LAND_SPEED          D32(0x000)
#define LAND_END_A          (D + 0x004)
#define LAND_FORWARD        D32(0x020)
#define LAND_END_B          (D + 0x024)
#define LAND_TARGET         (D + 0x03c)
#define LAND_BEST_LOW       D32(0x054)
#define LAND_BEST_HIGH      D16(0x058)
#define LAND_X              D32(0x05a)
#define LAND_ALTITUDE       D32(0x05e)
#define LAND_FORWARD_NOW    D32(0x062)
#define LAND_SCRIPT_CURSOR  DPTR(0x066)
#define LAND_SINE           D16(0x06a)
#define LAND_COSINE         D16(0x06c)
#define LAND_PHASE_OFFSET   D32(0x06e)
#define LAND_RUNWAY_ANGLE   D16(0x072)
#define LAND_RUNWAY_KIND    D16(0x074)
#define LAND_SCRIPT         DPTR(0x076)
#define LAND_DELAY          D32(0x07a)
#define LAND_RUNWAY_END_A   D8(0x07e)
#define LAND_RUNWAY_END_B   D8(0x07f)
#define LAND_SAVED_278      D32(0x080)
#define LAND_PENDING_STATE  D32(0x084)
#define LAND_STATE          D32(0x088)
#define LAND_MESSAGE        ((char *)D + 0x0a8)

static void FS98_STDCALL landme_tick(uint32_t first, uint32_t second);
static void landme_state_setup(void);
static void landme_state_choose(void);
static void landme_state_follow(void);
static void landme_state_finish(void);
static void landme_state_delay(void);
static void landme_state_abort(void);

/* A 48-bit value here is the low word at +2 and signed high dword at +4. */
static uint32_t landme_sub48_high(uint16_t left_low, uint32_t left_high,
                                  uint16_t right_low, uint32_t right_high)
{
    return left_high - right_high - (left_low < right_low);
}

static void landme_set48(uint8_t *value, uint16_t low, uint32_t high)
{
    *(landme_u16 *)(value + 2) = low;
    *(landme_u32 *)(value + 4) = high;
}

/* .text+0xf45 -- UTIL expands the record's first seven bytes.  LANDME then
 * replaces the third component with a signed 24-bit field one byte later. */
static void landme_unpack_endpoint(const uint8_t *record, uint8_t *position)
{
    uint32_t packed;

    UTIL_UNPACK(record, position);
    packed = ((uint32_t)*(const landme_u16 *)(record + 7) << 8) | record[6];
    packed <<= 8;
    *(landme_u16 *)(position + 0x12) = (uint16_t)packed;
    *(landme_u32 *)(position + 0x14) = (uint32_t)((int32_t)packed >> 16);
}

/* .text+0xddf -- midpoint of the selected runway's endpoints, its forward
 * vector and its heading.  The add/rcr pairs average a 49-bit unsigned sum. */
static void landme_runway_frame(void)
{
    uint32_t component;
    uint64_t sum;
    uint8_t longitude[8] = { 0 };
    uint8_t metres[8] = { 0 };

    sum = (((uint64_t)D32(0x008) << 16) | D16(0x006))
        + (((uint64_t)D32(0x028) << 16) | D16(0x026));
    sum >>= 1;
    D16(0x03e) = (uint16_t)sum;
    D32(0x040) = (uint32_t)(sum >> 16);

    sum = (((uint64_t)D32(0x010) << 16) | D16(0x00e))
        + (((uint64_t)D32(0x030) << 16) | D16(0x02e));
    sum >>= 1;
    D16(0x046) = (uint16_t)sum;
    D32(0x048) = (uint32_t)(sum >> 16);

    sum = (((uint64_t)D32(0x018) << 16) | D16(0x016))
        + (((uint64_t)D32(0x038) << 16) | D16(0x036));
    sum >>= 1;
    D16(0x04e) = (uint16_t)sum;
    D32(0x050) = (uint32_t)(sum >> 16);

    LAND_FORWARD = landme_sub48_high(D16(0x006), D32(0x008),
                                      D16(0x03e), D32(0x040));
    landme_set48(longitude,
                 (uint16_t)(D16(0x00e) - D16(0x046)),
                 landme_sub48_high(D16(0x00e), D32(0x010),
                                    D16(0x046), D32(0x048)));
    UTIL_LONGITUDE_TO_METRES(longitude, LAND_END_A, metres);
    component = *(landme_u32 *)(metres + 4);
    D32(0x01c) = component;
    LAND_RUNWAY_ANGLE = (uint16_t)UTIL_ATAN(component, LAND_FORWARD);
    UTIL_SINCOS((int16_t)LAND_RUNWAY_ANGLE,
                (int16_t *)(D + 0x06a), (int16_t *)(D + 0x06c));
}

/* .text+0xa93 -- aircraft offset from the runway midpoint, converted to
 * metres and rotated into runway-local axes. */
static void landme_update_position(void)
{
    uint8_t longitude[8] = { 0 };
    uint8_t metres[8] = { 0 };
    int32_t x;
    int32_t forward;

    LAND_FORWARD_NOW = landme_sub48_high(G16(0x562), G32(0x564),
                                          D16(0x03e), D32(0x040));
    LAND_ALTITUDE = landme_sub48_high(G16(0x572), G32(0x574),
                                      D16(0x04e), D32(0x050));
    landme_set48(longitude,
                 (uint16_t)(G16(0x56a) - D16(0x046)),
                 landme_sub48_high(G16(0x56a), G32(0x56c),
                                    D16(0x046), D32(0x048)));
    UTIL_LONGITUDE_TO_METRES(longitude, G + 0x560, metres);
    x = *(int32_t *)(metres + 4);
    UTIL_ROTATE(x, (int32_t)LAND_FORWARD_NOW,
                (int16_t)LAND_SINE, (int16_t)LAND_COSINE, &x, &forward);
    LAND_X = (uint32_t)x;
    LAND_FORWARD_NOW = (uint32_t)forward;
}

/* .text+0xb8f -- FE's search result is a sequence of scenery records.  Type 7
 * is a runway; the nearest one supplies two packed endpoints, an angle, an
 * airport kind and the two runway-number bytes. */
static void landme_find_runway(void)
{
    uint32_t length;
    uint16_t kind = 0;

    FE_SEARCH(G + 0x560, 100000, G + 0x3e4);
    LAND_BEST_HIGH = 0xffff;
    do {
        const uint8_t *record;
        uint32_t longitude;
        int32_t latitude;
        uint8_t delta[8] = { 0 };
        uint8_t metres[8] = { 0 };
        uint64_t distance;
        uint64_t best;

        if (FE_NEXT(kind, D + 0x0cf, &length) == 0)
            break;
        record = (const uint8_t *)(uintptr_t)D32(0x0cf);
        kind = 1;
        if (record[0] != 7)
            continue;

        longitude = ((uint32_t)*(const landme_u16 *)(record + 6) << 16)
                  | ((uint32_t)record[5] << 8);
        longitude -= G32(0x56c);
        landme_set48(delta, 0, longitude);
        UTIL_LONGITUDE_TO_METRES(delta, G + 0x560, metres);
        longitude = *(landme_u32 *)(metres + 4);

        latitude = (int32_t)(int16_t)*(const landme_u16 *)(record + 3);
        latitude = (int32_t)((uint32_t)latitude << 8);
        latitude = (int32_t)(((uint32_t)latitude & 0xffffff00u) | record[2]);
        latitude = (int32_t)((uint32_t)latitude << 1);
        latitude -= (int32_t)G32(0x564);

        distance = (uint64_t)((int64_t)(int32_t)longitude *
                              (int32_t)longitude);
        distance += (uint64_t)((int64_t)latitude * latitude);
        distance &= 0xffffffffffffULL;
        best = ((uint64_t)LAND_BEST_HIGH << 32) | LAND_BEST_LOW;
        if (distance >= best)
            continue;

        LAND_BEST_LOW = (uint32_t)distance;
        LAND_BEST_HIGH = (uint16_t)(distance >> 32);
        LAND_RUNWAY_KIND = *(const landme_u16 *)(record + 0x14);
        LAND_RUNWAY_END_A = record[0x35];
        LAND_RUNWAY_END_B = record[0x36];
        landme_unpack_endpoint(record + 2, LAND_END_A);
        landme_unpack_endpoint(record + 0x0b, LAND_END_B);
        FS98_HOST_MEMSET(HOST)(LAND_MESSAGE, 0, 0x23);
        FS98_HOST_MEMMOVE(HOST)(LAND_MESSAGE, record + 0x16, 0x1f);
    } while (1);

    landme_runway_frame();
    if ((int16_t)G16(0xef0) >= 5) {
        uint16_t direction = (uint16_t)(LAND_RUNWAY_ANGLE + 0x8000u);
        uint8_t index;

        direction = (uint16_t)(direction - G16(0xef2));
        direction = (uint16_t)(direction + 0x4000u);
        if ((int16_t)direction < 0) {
            for (index = 0; index < 0x18; index++) {
                uint8_t value = LAND_END_A[index];
                LAND_END_A[index] = LAND_END_B[index];
                LAND_END_B[index] = value;
            }
            index = LAND_RUNWAY_END_A;
            LAND_RUNWAY_END_A = LAND_RUNWAY_END_B;
            LAND_RUNWAY_END_B = index;
        }
    }

    {
        uint8_t runway = LAND_RUNWAY_END_A;
        uint8_t number = runway & 0x3f;
        uint8_t tens = (uint8_t)(number / 10 + '0');
        uint8_t ones = (uint8_t)(number % 10 + '0');
        uint8_t side = D8(0x0cb + (runway >> 6));
        int32_t cursor = 0x1e;

        do {
            cursor--;
            if (cursor == 0)
                break;
        } while ((uint8_t)LAND_MESSAGE[cursor] == ' ');
        if (cursor != 0) {
            LAND_MESSAGE[cursor + 2] = (char)tens;
            LAND_MESSAGE[cursor + 3] = (char)ones;
            LAND_MESSAGE[cursor + 4] = (char)side;
        }
    }
}

/* .text+0xa21 -- a string resource shown in WINMSG slot 4 for 90 frames. */
static void landme_show(uint32_t string_id)
{
    char message[0x100];

    FS98_HOST_LOADSTRING(HOST)(string_id, message, sizeof(message));
    WINMSG_SHOW(message, 4, 90, 0);
}

/* .text+0x7ed -- configure a phase's requested position from one of the nine
 * 0x14-byte records at .data+0x743. */
static void landme_set_target(uint32_t config_offset)
{
    const uint8_t *config = D + config_offset;
    int32_t across;
    int32_t forward;
    int32_t longitude_step;
    uint8_t converted[8] = { 0 };
    uint32_t altitude;
    uint16_t low;

    G16(0x872) = *(const landme_u16 *)(config + 0x10);
    G16(0x870) = (uint16_t)(LAND_RUNWAY_ANGLE
                           + *(const landme_u16 *)(config + 0x12));
    forward = *(const int32_t *)(config + 0x0c);
    if (*(const uint32_t *)config == 1)
        forward += (int32_t)LAND_FORWARD;
    else if (*(const uint32_t *)config == 2)
        forward -= (int32_t)LAND_FORWARD;

    UTIL_ROTATE(*(const int32_t *)(config + 4), forward,
                -(int16_t)LAND_SINE, (int16_t)LAND_COSINE,
                &across, &forward);
    G16(0x85a) = 0;
    G32(0x85c) = D32(0x040) + (uint32_t)forward;

    longitude_step = (int32_t)((uint32_t)across << 16);
    UTIL_METRES_TO_LONGITUDE(&longitude_step, LAND_TARGET, converted);
    G16(0x862) = 0;
    G32(0x864) = D32(0x048) + *(landme_u32 *)(converted + 4);

    G16(0x86a) = 0;
    low = D16(0x04e);
    altitude = D32(0x050) + *(const landme_u32 *)(config + 8)
             + ((uint32_t)low + 0x8000u > 0xffffu);
    G32(0x86c) = altitude;
    G32(0x854) = 1;
    LAND_STATE = 3;
}

/* .text+0x916 -- phase 5 delegates its target calculation to MAIN, then asks
 * for a point 305 metres above the runway or terrain, whichever is higher. */
static void landme_main_target(void)
{
    uint32_t ignored;
    uint32_t ground;
    int32_t above_ground;

    MAIN_LAND(LAND_TARGET, G + 0x560, G + 0x870, &ignored);
    G16(0x872) = 0;
    G16(0x85a) = 0;
    G32(0x85c) = D32(0x040);
    G16(0x862) = 0;
    G32(0x864) = D32(0x048);
    G16(0x86a) = 0;
    G32(0x86c) = D32(0x050) + 0x131;
    ground = G32(0x020) >> 8;
    above_ground = (int32_t)(G32(0x574) - ground);
    if (above_ground <= 0x1e)
        G32(0x86c) = ground + 0x131;
    G32(0x854) = 1;
}

/* .text+0x9ca -- phase 11 retains the prior horizontal target and changes
 * only heading and altitude. */
static void landme_final_target(void)
{
    uint32_t ground;
    int32_t above_ground;

    G16(0x870) = (uint16_t)((G32(0x580) >> 16) + 0x4000u);
    G16(0x86a) = 0;
    G32(0x86c) = D32(0x050) + 0x131;
    ground = G32(0x020) >> 8;
    above_ground = (int32_t)(G32(0x574) - ground);
    if (above_ground <= 0x1e)
        G32(0x86c) = ground + 0x131;
    G32(0x854) = 1;
}

static void landme_phase_setup(uint32_t phase)
{
    switch (phase) {
    case 1:
        landme_set_target(0x743);
        G16(0x878) = 0x41;
        LAND_SPEED = 0x3fff;
        G16(0x87c) = 0x3fff;
        if ((G16(0x116e) & 1) == 0 && G32(0x28c) == 0)
            FS98_HOST_POST_COMMAND(HOST)(0x100d7, 0);
        break;
    case 2:
        landme_set_target(0x757);
        G16(0x878) = 0x4b;
        LAND_SPEED = 0x0fff;
        G16(0x87c) = 0x3fff;
        break;
    case 3:
        landme_set_target(0x76b);
        G16(0x878) = 0x4b;
        LAND_SPEED = 0x1fff;
        G16(0x87c) = 0x3fff;
        break;
    case 4:
        landme_set_target(0x77f);
        G16(0x878) = 0x46;
        LAND_SPEED = 0x1fff;
        break;
    case 5:
        landme_main_target();
        G16(0x878) = 0x78;
        LAND_SPEED = 0;
        break;
    case 6:
        landme_set_target(0x793);
        G16(0x878) = 0x50;
        LAND_SPEED = 0x0fff;
        G16(0x87c) = 0x3fff;
        break;
    case 7:
        landme_set_target(0x7a7);
        G16(0x878) = 0x5a;
        LAND_SPEED = 0x0fff;
        G16(0x87c) = 0x3fff;
        break;
    case 8:
        landme_set_target(0x7bb);
        G16(0x878) = 0x50;
        LAND_SPEED = 0;
        G16(0x87c) = 0;
        break;
    case 9:
        landme_set_target(0x7cf);
        G16(0x878) = 0x50;
        LAND_SPEED = 0;
        G16(0x87c) = 0;
        break;
    case 10:
        landme_set_target(0x7bb);
        G16(0x878) = 0x50;
        LAND_SPEED = 0x0fff;
        G16(0x87c) = 0;
        break;
    case 11:
        landme_final_target();
        G16(0x878) = 0x78;
        LAND_SPEED = 0;
        G16(0x87c) = 0;
        break;
    }
}

/* .text+0x1277 -- choose a guidance phase from the runway-local boxes in the
 * script.  Type 1 and 2 offset the forward axis by opposite runway ends;
 * type 3 uses the runway's half-length as a symmetric bound. */
static void landme_state_choose(void)
{
    landme_update_position();
    LAND_SCRIPT_CURSOR = LAND_SCRIPT;
    D32(0x0d3) = 0;
    for (;;) {
        uint8_t *record = LAND_SCRIPT_CURSOR;
        uint32_t type;
        int32_t bound;
        uint8_t phase;
        uint32_t phase_offset;

        D32(0x0d3)++;
        type = *(landme_u32 *)record;
        if (type == 0) {
            phase = 5;
        } else if (type >= 1 && type <= 3) {
            LAND_SCRIPT_CURSOR += 0x24;
            if ((int32_t)LAND_ALTITUDE < *(int32_t *)(record + 8) ||
                (int32_t)LAND_ALTITUDE > *(int32_t *)(record + 0x14) ||
                (int32_t)LAND_X < *(int32_t *)(record + 4) ||
                (int32_t)LAND_X > *(int32_t *)(record + 0x10))
                continue;
            if (type == 1)
                bound = (int32_t)(LAND_FORWARD_NOW - LAND_FORWARD);
            else if (type == 2)
                bound = (int32_t)(LAND_FORWARD_NOW + LAND_FORWARD);
            else {
                if ((int32_t)LAND_FORWARD_NOW > (int32_t)LAND_FORWARD ||
                    (int32_t)LAND_FORWARD_NOW <
                        (int32_t)(0u - LAND_FORWARD))
                    continue;
                bound = 0;
            }
            if (type != 3 &&
                (bound < *(int32_t *)(record + 0x18) ||
                 bound > *(int32_t *)(record + 0x0c)))
                continue;
            {
                uint16_t direction = (uint16_t)((G32(0x580) >> 16)
                                                - LAND_RUNWAY_ANGLE + 0x9000u);
                phase = record[0x1c + (direction >> 13)];
            }
            if (phase == 0)
                continue;
        } else {
            return;
        }

        phase_offset = (uint32_t)phase * 0x10;
        if (phase_offset != LAND_PHASE_OFFSET) {
            LAND_PHASE_OFFSET = phase_offset;
            landme_show(D32(0x68f + phase_offset));
        }
        landme_phase_setup(phase);
        return;
    }
}

static int32_t landme_phase_boundary(uint32_t which)
{
    int16_t cosine;
    int32_t product;
    int32_t scaled;
    int32_t threshold = *(int32_t *)(D + 0x68b + LAND_PHASE_OFFSET);
    int32_t value;

    UTIL_SINCOS((int16_t)(G16(0xe92) - G16(0x870)), 0, &cosine);
    product = (int32_t)cosine * (int16_t)G16(0xe90);
    if (which == 1) {
        scaled = (int32_t)(((int64_t)6 * product) >> 16);
        value = (int32_t)(LAND_FORWARD_NOW - LAND_FORWARD);
    } else if (which == 2) {
        scaled = (int32_t)(((int64_t)-6 * product) >> 16);
        value = (int32_t)LAND_X;
    } else {
        scaled = (int32_t)(((int64_t)6 * product) >> 16);
        value = (int32_t)(LAND_FORWARD_NOW + LAND_FORWARD);
    }
    threshold = (int32_t)((uint32_t)threshold + (uint32_t)scaled);
    if (value < threshold)
        return -1;
    if (value > threshold)
        return 1;
    return 0;
}

/* .text+0x13a3 and +0x1650..+0x1680 -- a phase remains active until its one
 * signed boundary comparison fails, then the script is scanned again. */
static void landme_state_follow(void)
{
    uint32_t phase;
    int32_t comparison = 0;

    landme_update_position();
    phase = LAND_PHASE_OFFSET >> 4;
    switch (phase) {
    case 1:
    case 8:
    case 10:
        comparison = landme_phase_boundary(3);
        if (comparison < 0)
            LAND_STATE = 2;
        break;
    case 2:
    case 3:
        comparison = landme_phase_boundary(1);
        if (comparison > 0)
            LAND_STATE = 2;
        break;
    case 4:
    case 6:
        comparison = landme_phase_boundary(2);
        if (comparison > 0)
            LAND_STATE = 2;
        break;
    case 7:
    case 9:
        comparison = landme_phase_boundary(2);
        if (comparison < 0)
            LAND_STATE = 2;
        break;
    case 11:
        if ((int32_t)G32(0x574) < *(int32_t *)(D + 0x68b + LAND_PHASE_OFFSET))
            LAND_STATE = 2;
        break;
    }
}

/* .text+0x112a -- acquire the runway, clear MAIN's prior guidance fields and
 * delay before announcing the selected runway. */
static void landme_state_setup(void)
{
    int32_t discarded;
    int32_t forward;
    uint32_t zero_offsets[] = {
        0x7bc, 0x7d0, 0x800, 0x7d8, 0x804, 0x7fc,
        0x7c8, 0x7c4, 0x7c0, 0x808, 0x7dc
    };
    uint32_t index;

    landme_find_runway();
    landme_runway_frame();
    UTIL_ROTATE(D32(0x01c), LAND_FORWARD, (int16_t)LAND_SINE,
                (int16_t)LAND_COSINE, &discarded, &forward);
    LAND_FORWARD = (uint32_t)forward;

    LAND_PHASE_OFFSET = 0xffffffffu;
    if ((int16_t)LAND_RUNWAY_KIND < 1)
        LAND_SCRIPT = D + 0x0d7;
    else if (LAND_RUNWAY_KIND == 1)
        LAND_SCRIPT = D + 0x67b;
    else
        LAND_SCRIPT = D + 0x67f;

    for (index = 0; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]); index++)
        G32(zero_offsets[index]) = 0;
    G16(0xbb6) = 0;
    G16(0xbb2) = 0;
    G16(0xbba) = 0;
    landme_show(0x1f4);
    LAND_STATE = 5;
    LAND_DELAY = 0;
    LAND_SAVED_278 = G32(0x278);
    G32(0x278) = 0;
}

/* .text+0x13ef -- two frames into the delay, show the runway message (or the
 * no-runway message); five frames in, enter the chosen next state. */
static void landme_state_delay(void)
{
    uint32_t old = LAND_DELAY++;

    if (old == 1) {
        if (LAND_BEST_HIGH != 0xffff) {
            WINMSG_SHOW(LAND_MESSAGE, 4, 90, 0);
            LAND_PENDING_STATE = 2;
        } else {
            landme_show(0x214);
            LAND_PENDING_STATE = 4;
        }
    } else if (old == 4) {
        LAND_STATE = LAND_PENDING_STATE;
    }
}

/* .text+0x13b5 -- stop guidance and restore the value suppressed at setup. */
static void landme_state_finish(void)
{
    landme_show(0x1f5);
    LAND_STATE = 0;
    G32(0x84c) = 0;
    G16(0x01a) &= 0xfeff;
    G32(0x854) = 0;
    G32(0x278) = LAND_SAVED_278;
}

/* .text+0x1467 -- an external abort gets one message, then finishes on the
 * next frame. */
static void landme_state_abort(void)
{
    landme_show(0x1f8);
    LAND_STATE = 4;
}

/* .text+0xf85 -- cap the requested rate from LAND_SPEED and groundspeed. */
static void landme_update_speed(void)
{
    uint32_t groundspeed = G32(0x2bc);

    if (LAND_SPEED >= 0x3fff && groundspeed <= 0x3200)
        G16(0x87a) = 0x3fff;
    else if (LAND_SPEED >= 0x1fff && groundspeed <= 0x3c00)
        G16(0x87a) = 0x1fff;
    else if (LAND_SPEED >= 0x0fff && groundspeed <= 0x4600)
        G16(0x87a) = 0x0fff;
    else
        G16(0x87a) = 0;
}

/* .text+0xaf -- CHAIN list 3 callback. */
static void FS98_STDCALL landme_tick(uint32_t first, uint32_t second)
{
    (void)first;
    (void)second;

    if (LAND_STATE != 0) {
        if (LAND_STATE != 4) {
            if (G32(0x84c) == 0) {
                LAND_STATE = 4;
            } else if (G32(0x874) != 0) {
                LAND_STATE = 6;
                G32(0x874) = 0;
            }
        }
        switch (LAND_STATE) {
        case 1: landme_state_setup();  break;
        case 2: landme_state_choose(); break;
        case 3: landme_state_follow(); break;
        case 4: landme_state_finish(); break;
        case 5: landme_state_delay();  break;
        case 6: landme_state_abort();  break;
        }
    }
    landme_update_speed();
}

/* .text+0x20 / +0x50 -- Linkage init and deinit. */
static void landme_init(void)
{
    CHAIN_ADD(3, landme_tick);
}

static void landme_deinit(void)
{
    CHAIN_REMOVE(3, landme_tick);
}

/* .text+0x80 -- Linkage +0x18. */
static void landme_start(void)
{
    if (LAND_STATE == 0 && G32(0x84c) != 0) {
        G16(0x01a) |= 0x0100;
        LAND_STATE = 1;
    }
}

/* Retail's two dispatch tables live in .data.  The C control flow above uses
 * switches, but keep every table entry callable and preserve retail's shared
 * entries so the image remains a truthful description of the module. */
static void landme_nop(void) { }
static void landme_phase_1(void)  { landme_phase_setup(1); }
static void landme_phase_2(void)  { landme_phase_setup(2); }
static void landme_phase_3(void)  { landme_phase_setup(3); }
static void landme_phase_4(void)  { landme_phase_setup(4); }
static void landme_phase_5(void)  { landme_phase_setup(5); }
static void landme_phase_6(void)  { landme_phase_setup(6); }
static void landme_phase_7(void)  { landme_phase_setup(7); }
static void landme_phase_8(void)  { landme_phase_setup(8); }
static void landme_phase_9(void)  { landme_phase_setup(9); }
static void landme_phase_10(void) { landme_phase_setup(10); }
static void landme_phase_11(void) { landme_phase_setup(11); }

static void landme_check_1(void)
{
    if (landme_phase_boundary(3) < 0)
        LAND_STATE = 2;
}

static void landme_check_2(void)
{
    if (landme_phase_boundary(1) > 0)
        LAND_STATE = 2;
}

static void landme_check_4(void)
{
    if (landme_phase_boundary(2) > 0)
        LAND_STATE = 2;
}

static void landme_check_7(void)
{
    if (landme_phase_boundary(2) < 0)
        LAND_STATE = 2;
}

static void landme_check_11(void)
{
    if ((int32_t)G32(0x574) < *(int32_t *)(D + 0x68b + LAND_PHASE_OFFSET))
        LAND_STATE = 2;
}

struct landme_image landme_image = {{
#include "landme_data.inc"
}};

/* Retail exports point into the tail of its .data section. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _landme_image + 0x7f8\n\t"
        ".globl _Linkage\n\t.set _Linkage, _landme_image + 0x838");

static void landme_install_code_pointers(void)
{
    static void (*const state_functions[])(void) = {
        landme_nop, landme_state_setup, landme_state_choose,
        landme_state_follow, landme_state_finish, landme_state_delay,
        landme_state_abort
    };
    static void (*const phase_functions[])(void) = {
        landme_nop,       landme_nop,
        landme_phase_1,   landme_check_1,
        landme_phase_2,   landme_check_2,
        landme_phase_3,   landme_check_2,
        landme_phase_4,   landme_check_4,
        landme_phase_5,   landme_nop,
        landme_phase_6,   landme_check_4,
        landme_phase_7,   landme_check_7,
        landme_phase_8,   landme_check_1,
        landme_phase_9,   landme_check_7,
        landme_phase_10,  landme_check_1,
        landme_phase_11,  landme_check_11
    };
    uint32_t index;

    for (index = 0;
         index < sizeof(state_functions) / sizeof(state_functions[0]); index++)
        D32(0x08c + index * 4) =
            (uint32_t)(uintptr_t)state_functions[index];
    for (index = 0;
         index < sizeof(phase_functions) / sizeof(phase_functions[0]); index++)
        D32(0x683 + index * 4 + (index / 2) * 8) =
            (uint32_t)(uintptr_t)phase_functions[index];
    D32(LANDME_LINKAGE + 4) = (uint32_t)(uintptr_t)landme_init;
    D32(LANDME_LINKAGE + 8) = (uint32_t)(uintptr_t)landme_deinit;
    D32(LANDME_LINKAGE + 0x18) = (uint32_t)(uintptr_t)landme_start;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        landme_install_code_pointers();
    }
    return TRUE;
}
