/* NUMERIC.DLL -- module id 17, numeric formatting and parsing services.
 *
 * The retail module is hand-written assembly.  The formatter below therefore
 * follows its register-sized operations deliberately: partial-word subtracts,
 * unsigned multiplies, fixed-width digit chains, and the original rounding
 * quirks are part of the interface.
 */
#include <stdint.h>

#if defined(__MINGW32__)
__declspec(dllimport) double __cdecl floor(double value);
__declspec(dllimport) int __cdecl sprintf(char *output, const char *format, ...);
#else
#  include <math.h>
#  include <stdio.h>
#endif

#ifdef FS98_NUMERIC_MODULE
#  include <windows.h>
#endif

#include "fs98/numeric.h"
#include "numeric_imports.h"

#include "numeric_data.inc"

#define FS98_NUMERIC_U8(offset)  (*(uint8_t *)(fs98_numeric_data + (offset)))
#define FS98_NUMERIC_U16(offset) (*(uint16_t *)(fs98_numeric_data + (offset)))
#define FS98_NUMERIC_U32(offset) (*(uint32_t *)(fs98_numeric_data + (offset)))
#define FS98_NUMERIC_U64(offset) (*(uint64_t *)(fs98_numeric_data + (offset)))
#define FS98_NUMERIC_DBL(offset) (*(double *)(fs98_numeric_data + (offset)))

#define FS98_NUMERIC_SLOT(link, offset, function_type) \
    (*(function_type *)((uint8_t *)(link) + (offset)))

typedef int32_t (FS98_STDCALL *fs98_numeric_load_string_fn)(uint32_t id,
                                                     char *buffer,
                                                     int32_t length);
typedef int32_t (FS98_STDCALL *fs98_numeric_config_fn)(const char *section,
                                                const char *key,
                                                int32_t *value);
typedef void (FS98_STDCALL *fs98_numeric_fixed_to_double_fn)(double *output,
                                                        const void *input);

#ifndef FS98_NUMERIC_MODULE
void *fs98_numeric_host;
void *fs98_numeric_global;
void *fs98_numeric_util;

void fs98_numeric_set_host(void *host)     { fs98_numeric_host = host; }
void fs98_numeric_set_global(void *global) { fs98_numeric_global = global; }
void fs98_numeric_set_util(void *util)     { fs98_numeric_util = util; }
#endif

static uint32_t fs98_numeric_strlen(const char *text)
{
    uint32_t length = 0;

    while (text[length] != '\0')
        length++;
    return length;
}

static double fs98_numeric_rdata_double(uint32_t index)
{
    union {
        uint64_t bits;
        double value;
    } constant;

    constant.bits = fs98_numeric_rdata[index];
    return constant.value;
}

static int64_t fs98_numeric_load_s48(const void *input)
{
    const uint8_t *bytes = (const uint8_t *)input;
    uint32_t low = *(const uint32_t *)(bytes + 2);
    int32_t high = *(const int16_t *)(bytes + 6);

    return ((int64_t)high << 32) | low;
}

static uint32_t fs98_numeric_abs_s32(int32_t signed_value, char **output_cursor)
{
    char sign = '+';
    uint32_t value = (uint32_t)signed_value;

    if (signed_value < 0) {
        sign = '-';
        value = (uint32_t)(0u - value);
    }
    *(*output_cursor)++ = sign;
    return value;
}

static uint64_t fs98_numeric_abs_s64(int64_t signed_value, char **output_cursor)
{
    char sign = '+';
    uint64_t magnitude = (uint64_t)signed_value;

    if (signed_value < 0) {
        sign = '-';
        magnitude = 0ull - magnitude;
    }
    *(*output_cursor)++ = sign;
    return magnitude;
}

/* Print exactly digit_count digits.  Retail adds '0' to AL, so an over-range
 * quotient becomes punctuation (or wraps in one byte) instead of carrying. */
static void fs98_numeric_digits(uint32_t value, int digit_count, char **output_cursor)
{
    uint32_t offset = 0x130u + (uint32_t)(9 - digit_count) * 4u;

    while (digit_count > 1) {
        uint32_t divisor = FS98_NUMERIC_U32(offset);
        uint32_t digit = value / divisor;

        *(*output_cursor)++ = (char)((uint8_t)digit + (uint8_t)'0');
        value %= divisor;
        offset += 4;
        digit_count--;
    }
    *(*output_cursor)++ = (char)((uint8_t)value + (uint8_t)'0');
}

static void fs98_numeric_fixed(uint64_t value64, int fraction_bits,
                          int integer_digits, char **output_cursor)
{
    uint32_t mask = (1u << fraction_bits) - 1u;
    uint32_t scale;
    int fraction_digits;
    uint32_t integer = (uint32_t)(value64 >> fraction_bits);
    uint32_t fraction = (uint32_t)value64 & mask;

    if (fraction_bits == 4) {
        scale = 10;
        fraction_digits = 1;
    } else if (fraction_bits == 8) {
        scale = 100;
        fraction_digits = 2;
    } else if (fraction_bits == 12) {
        scale = 1000;
        fraction_digits = 3;
    } else {
        scale = 10000;
        fraction_digits = 4;
    }

    fs98_numeric_digits(integer, integer_digits, output_cursor);
    *(*output_cursor)++ = '.';
    fraction = (fraction * scale) >> fraction_bits;
    fs98_numeric_digits(fraction, fraction_digits, output_cursor);
}

static uint64_t fs98_numeric_metres_to_feet(uint32_t value)
{
    return ((uint64_t)value * 0x347e5u) >> 16;
}

static void fs98_numeric_degrees(uint16_t word_value, char **output_cursor)
{
    uint32_t value = (uint32_t)word_value * FS98_NUMERIC_U32(0x150);

    if (value == 0)
        value += 0x1680000u;
    fs98_numeric_fixed(value >> 8, 8, 3, output_cursor);
}

static int64_t fs98_numeric_x87_scale32(int64_t value64, int32_t multiplier,
                                 int32_t divisor)
{
    int64_t output64;

#if defined(__i386__) && !defined(FS98_PORTABLE_FP)
    __asm__ volatile(
        "fildll %1\n\t"
        "fimull %2\n\t"
        "fidivl %3\n\t"
        "fistpll %0\n\t"
        "fwait"
        : "=m" (output64)
        : "m" (value64), "m" (multiplier), "m" (divisor)
        : "st");
#else
    output64 = (int64_t)(((long double)value64 * multiplier) / divisor);
#endif
    return output64;
}

static int64_t fs98_numeric_x87_scale64(int64_t value64, int32_t multiplier,
                                 int64_t divisor64)
{
    int64_t output64;

#if defined(__i386__) && !defined(FS98_PORTABLE_FP)
    __asm__ volatile(
        "fildll %1\n\t"
        "fimull %2\n\t"
        "fildll %3\n\t"
        /* GAS names the no-operand form from AT&T's operand order: fdivrp
         * emits the retail fdivp st(1),st division (numerator/divisor). */
        "fdivrp\n\t"
        "fistpll %0\n\t"
        "fwait"
        : "=m" (output64)
        : "m" (value64), "m" (multiplier), "m" (divisor64)
        : "st");
#else
    output64 = (int64_t)(((long double)value64 * multiplier) / divisor64);
#endif
    return output64;
}

struct fs98_numeric_div_state {
    uint64_t remainder;
};

static uint32_t fs98_numeric_restoring_divide(
    struct fs98_numeric_div_state *state, uint32_t table_offset, int bit_count,
    int shift)
{
    uint64_t divisor_value = FS98_NUMERIC_U64(table_offset);
    uint32_t quotient = 0;
    int bit_index;

    if (shift < 0)
        state->remainder >>= -shift;
    else if (shift > 0)
        state->remainder <<= shift;

    for (bit_index = 0; bit_index < bit_count; bit_index++) {
        uint64_t trial = state->remainder - divisor_value;

        quotient <<= 1;
        if ((trial & 0x8000000000000000ull) == 0) {
            state->remainder = trial;
            quotient++;
        }
        if (bit_index + 1 < bit_count)
            state->remainder <<= 1;
    }
    return quotient;
}

static void fs98_numeric_angle_dms(uint64_t value64, int latitude,
                              char **output_cursor)
{
    struct fs98_numeric_div_state state;
    uint32_t base = latitude ? 0x178u : 0x158u;
    uint32_t degrees;
    uint32_t minutes;
    uint32_t seconds;
    uint32_t fraction;

    state.remainder = value64;
    degrees = fs98_numeric_restoring_divide(&state, base, 9, -1);
    fs98_numeric_digits(degrees, 3, output_cursor);
    *(*output_cursor)++ = '*';
    *(*output_cursor)++ = ' ';

    minutes = fs98_numeric_restoring_divide(&state, base + 8, 6, 1);
    fs98_numeric_digits(minutes, 2, output_cursor);
    *(*output_cursor)++ = '\'';
    *(*output_cursor)++ = ' ';

    seconds = fs98_numeric_restoring_divide(&state, base + 16, 6, 1);
    fs98_numeric_digits(seconds, 2, output_cursor);
    *(*output_cursor)++ = '.';

    fraction = fs98_numeric_restoring_divide(&state, base + 24, 14, 0);
    fs98_numeric_digits(fraction, 4, output_cursor);
    *(*output_cursor)++ = '"';
}

static void fs98_numeric_fractional_angle(const void *input, int latitude,
                                     char **output_cursor)
{
    fs98_numeric_fixed_to_double_fn convert;
    double value_double;
    double power = FS98_NUMERIC_DBL(0x1a8);
    int64_t minutes64;
    int32_t degrees;
    int32_t digit_count = (int32_t)FS98_NUMERIC_U32(0x1a0);
    const uint8_t *bytes = (const uint8_t *)input;
    char *start = *output_cursor;
    const char *format_string;

    if (latitude) {
        *start = (*(const int32_t *)(bytes + 4) < 0) ?
                   (char)FS98_NUMERIC_U8(0x320) : (char)FS98_NUMERIC_U8(0x1f4);
        convert = FS98_NUMERIC_SLOT(FS98_NUMERIC_UTIL, 0xc8,
                              fs98_numeric_fixed_to_double_fn);
        convert(&value_double, input);
        value_double *= fs98_numeric_rdata_double(0);
        if (value_double < 0.0)
            value_double = -value_double;
    } else {
        *start = (*(const int32_t *)(bytes + 4) < 0) ?
                   (char)FS98_NUMERIC_U8(0x1fc) : (char)FS98_NUMERIC_U8(0x1f5);
        convert = FS98_NUMERIC_SLOT(FS98_NUMERIC_UTIL, 0xc0,
                              fs98_numeric_fixed_to_double_fn);
        convert(&value_double, input);
        value_double *= fs98_numeric_rdata_double(3);
        if (value_double >= 180.0)
            value_double = 360.0 - value_double;
    }

    degrees = (int32_t)floor(value_double);
    minutes64 = (int64_t)(((long double)value_double - degrees) * 60.0L *
                        (long double)power + 0.5L);
    if (minutes64 == (int64_t)(60.0 * power)) {
        minutes64 -= (int64_t)(60.0 * power);
        degrees++;
    }

    format_string = (const char *)(fs98_numeric_data + 0x94 - digit_count * 12);
    sprintf(start + 1, format_string, degrees, (double)minutes64 / power);
    *output_cursor += fs98_numeric_strlen(start);
}

static void fs98_numeric_format_longitude(const void *input, char **output_cursor)
{
    int64_t signed_value;
    uint64_t magnitude;

    if (FS98_NUMERIC_U32(0x19c) != 0) {
        fs98_numeric_fractional_angle(input, 0, output_cursor);
        return;
    }

    signed_value = fs98_numeric_load_s48(input);
    if (signed_value < 0) {
        *(*output_cursor)++ = (char)FS98_NUMERIC_U8(0x1fc);
        magnitude = 0ull - (uint64_t)signed_value;
    } else {
        *(*output_cursor)++ = (char)FS98_NUMERIC_U8(0x1f5);
        magnitude = (uint64_t)signed_value;
    }
    magnitude = (magnitude + 0x2a6bull) << 16;
    fs98_numeric_angle_dms(magnitude, 0, output_cursor);
}

static void fs98_numeric_format_latitude(const void *input, char **output_cursor)
{
    int64_t signed_value;
    uint64_t magnitude;

    if (FS98_NUMERIC_U32(0x19c) != 0) {
        fs98_numeric_fractional_angle(input, 1, output_cursor);
        return;
    }

    signed_value = fs98_numeric_load_s48(input);
    if (signed_value < 0) {
        *(*output_cursor)++ = (char)FS98_NUMERIC_U8(0x320);
        magnitude = 0ull - (uint64_t)signed_value;
    } else {
        *(*output_cursor)++ = (char)FS98_NUMERIC_U8(0x1f4);
        magnitude = (uint64_t)signed_value;
    }
    magnitude = (magnitude + 101ull) << 16;
    fs98_numeric_angle_dms(magnitude, 1, output_cursor);
}

static void fs98_numeric_trim_zeros(char *output)
{
    char *digit = output;

    if (*digit == '-' || *digit == '+')
        digit++;
    while (digit[0] == '0' && digit[1] >= '0' && digit[1] <= '9') {
        char *source = digit + 1;
        char *dest = digit;

        do {
            *dest++ = *source;
        } while (*source++ != '\0');
    }
}

static void fs98_numeric_format_bcd(uint16_t word_value, int append_channel,
                               char **output_cursor)
{
    uint8_t nibble_0 = (uint8_t)(word_value & 0xf);
    uint8_t nibble_1 = (uint8_t)((word_value >> 4) & 0xf);
    uint8_t nibble_2 = (uint8_t)((word_value >> 8) & 0xf);
    uint8_t nibble_3 = (uint8_t)((word_value >> 12) & 0xf);

    *(*output_cursor)++ = '1';
    *(*output_cursor)++ = (char)(nibble_3 + '0');
    *(*output_cursor)++ = (char)(nibble_2 + '0');
    *(*output_cursor)++ = '.';
    *(*output_cursor)++ = (char)(nibble_1 + '0');
    *(*output_cursor)++ = (char)(nibble_0 + '0');
    if (append_channel)
        *(*output_cursor)++ = (nibble_0 == 0 || nibble_0 == 5) ? '0' : '5';
}

static void fs98_numeric_format_ground_shift(const uint8_t *bytes, void *global,
                                        char **output_cursor)
{
    uint32_t value = *(const uint32_t *)bytes;
    uint32_t high = ((int32_t)value < 0) ? 0xffffffffu : 0u;

    value += *(uint32_t *)((uint8_t *)global + 0x20);
    *(*output_cursor)++ = ((int32_t)value < 0) ? '-' : '+';
    if ((int32_t)value < 0)
        value = 0u - value;
    /* EDX is the CDQ of the original input and survives the add and the
     * EAX-only negate before this SHRD. */
    value = (value >> 8) | (high << 24);
    fs98_numeric_digits(value, 6, output_cursor);
}

char *FS98_STDCALL fs98_numeric_format(const void *input, uint32_t format,
                                       char *output)
{
    const uint8_t *bytes = (const uint8_t *)input;
    void *global = FS98_NUMERIC_GLOBAL;
    char *start = output;
    char *write;
    uint32_t id = format & ~0x10000u;
    uint32_t value;
    uint64_t wide_value;
    int32_t signed_value;
    uint16_t word_value;
    int16_t units;

    if (id >= 0x51)
        return 0;
    if (start == 0)
        start = (char *)(fs98_numeric_data + 0x220);
    write = start;
    units = *(int16_t *)((uint8_t *)global + 0xc18);

    switch (id) {
    case 0x00:
        fs98_numeric_digits(*(const uint16_t *)bytes, 5, &write);
        break;
    case 0x01:
        fs98_numeric_fixed(*(const uint32_t *)bytes, 16, 5, &write);
        break;
    case 0x02:
        fs98_numeric_fixed(*(const uint32_t *)bytes + 0x40000000u,
                      16, 5, &write);
        break;
    case 0x03:
        signed_value = *(const int32_t *)bytes;
        signed_value = (int32_t)((uint32_t)signed_value +
                         *(uint32_t *)((uint8_t *)global + 0x20));
        value = fs98_numeric_abs_s32(signed_value, &write);
        fs98_numeric_digits((uint32_t)(fs98_numeric_metres_to_feet(value) >> 8),
                       6, &write);
        break;
    case 0x04:
        word_value = *(const uint16_t *)bytes;
        word_value = (uint16_t)(word_value -
                           *(uint16_t *)((uint8_t *)global + 0x2a0));
        word_value = (uint16_t)(word_value -
                           *(uint16_t *)((uint8_t *)global + 0x2a2));
        fs98_numeric_degrees(word_value, &write);
        break;
    case 0x05: case 0x08: case 0x09: case 0x0d:
    case 0x10: case 0x11: case 0x18: case 0x35:
        break;
    case 0x06:
        fs98_numeric_digits((uint32_t)fs98_numeric_metres_to_feet(
                           *(const uint16_t *)bytes), 6, &write);
        break;
    case 0x07:
        fs98_numeric_digits(*bytes, 3, &write);
        break;
    case 0x0a:
        signed_value = *(const int32_t *)bytes;
        signed_value += (signed_value < 0) ? -0x27 : 0x27;
        value = fs98_numeric_abs_s32(signed_value, &write);
        fs98_numeric_digits((uint32_t)(fs98_numeric_metres_to_feet(value) >> 8),
                       6, &write);
        break;
    case 0x0b:
        fs98_numeric_digits(*bytes, 2, &write);
        break;
    case 0x0c:
        fs98_numeric_digits(*bytes, 3, &write);
        break;
    case 0x0e:
        fs98_numeric_degrees(*(const uint16_t *)bytes, &write);
        break;
    case 0x0f:
        value = fs98_numeric_abs_s32(*(const int16_t *)bytes, &write);
        fs98_numeric_digits(value, 5, &write);
        break;
    case 0x12:
        fs98_numeric_format_bcd(*(const uint16_t *)bytes, 1, &write);
        break;
    case 0x13:
        value = (uint32_t)*(const uint16_t *)bytes * FS98_NUMERIC_U32(0x154);
        fs98_numeric_fixed(value >> 8, 8, 3, &write);
        break;
    case 0x14:
        fs98_numeric_format_longitude(input, &write);
        break;
    case 0x15:
        fs98_numeric_format_latitude(input, &write);
        break;
    case 0x16:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        wide_value = fs98_numeric_x87_scale32((int64_t)wide_value, 0x347e5,
                                      0x1000000);
        fs98_numeric_fixed((uint64_t)wide_value, 8, 6, &write);
        break;
    case 0x17:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        fs98_numeric_fixed(wide_value >> 8, 8, 6, &write);
        break;
    case 0x19:
        word_value = *(const uint16_t *)bytes;
        *write++ = (char)(((word_value >> 12) & 0xf) + '0');
        *write++ = (char)(((word_value >> 8) & 0xf) + '0');
        *write++ = (char)(((word_value >> 4) & 0xf) + '0');
        *write++ = (char)((word_value & 0xf) + '0');
        break;
    case 0x1a:
        fs98_numeric_digits(*(const uint32_t *)bytes, 6, &write);
        break;
    case 0x1b:
        fs98_numeric_digits(*(const uint32_t *)bytes, 7, &write);
        break;
    case 0x1c:
        fs98_numeric_fixed(*(const uint32_t *)bytes, 16, 2, &write);
        break;
    case 0x1d:
        value = fs98_numeric_abs_s32(*(const int32_t *)bytes, &write);
        fs98_numeric_digits(value >> 8, 6, &write);
        break;
    case 0x1e:
        fs98_numeric_format_ground_shift(bytes, global, &write);
        break;
    case 0x1f:
        fs98_numeric_digits(*(const uint16_t *)bytes, 1, &write);
        break;
    case 0x20:
        fs98_numeric_digits(*(const uint16_t *)bytes, 2, &write);
        break;
    case 0x21:
        fs98_numeric_digits(*(const uint16_t *)bytes, 3, &write);
        break;
    case 0x22:
        fs98_numeric_digits(*(const uint16_t *)bytes, 4, &write);
        break;
    case 0x23:
        value = ((uint32_t)*(const uint16_t *)bytes + 0x5bu) * 360u;
        fs98_numeric_digits(value >> 16, 3, &write);
        break;
    case 0x24:
        fs98_numeric_digits(((uint32_t)*(const uint16_t *)bytes + 8u) >> 4,
                       4, &write);
        break;
    case 0x25:
        value = (uint32_t)*(const uint16_t *)bytes >> 1;
        value = ((value << 16) + 0x15ac3u) / 0x43bau;
        fs98_numeric_fixed(value >> 2, 8, 2, &write);
        break;
    case 0x26:
        signed_value = ((int32_t)*(const int16_t *)bytes + 0x80) >> 8;
        value = fs98_numeric_abs_s32(signed_value, &write);
        fs98_numeric_digits(value, 3, &write);
        break;
    case 0x27:
        signed_value = (int32_t)*(const int16_t *)bytes * 18 + 0x500;
        if (signed_value < 0)
            signed_value -= 0xa00;
        signed_value = (signed_value + 18) / 0xa00 + 0x20;
        value = fs98_numeric_abs_s32(signed_value, &write);
        fs98_numeric_digits(value, 3, &write);
        break;
    case 0x28: case 0x29: case 0x2a: case 0x2b:
        fs98_numeric_fixed(*(const uint16_t *)bytes, 4,
                      (int)(id - 0x27), &write);
        break;
    case 0x2c: case 0x2d: case 0x2e:
        fs98_numeric_fixed(*(const uint16_t *)bytes, 8,
                      (int)(id - 0x2b), &write);
        break;
    case 0x2f: case 0x30:
        fs98_numeric_fixed(*(const uint16_t *)bytes, 12,
                      (int)(id - 0x2e), &write);
        break;
    case 0x31:
        fs98_numeric_fixed(*(const uint16_t *)bytes, 16, 1, &write);
        break;
    case 0x32:
        fs98_numeric_format_bcd(*(const uint16_t *)bytes, 0, &write);
        break;
    case 0x33:
        wide_value = fs98_numeric_metres_to_feet(*(const uint32_t *)bytes);
        value = (uint32_t)(wide_value / 0x17c0u);
        if ((uint32_t)(wide_value % 0x17c0u) >= 0xbe0u)
            value++;
        fs98_numeric_digits(value, 5, &write);
        break;
    case 0x34:
        value = *(const uint32_t *)bytes / FS98_NUMERIC_U32(0x144);
        if (*(const uint32_t *)bytes % FS98_NUMERIC_U32(0x144) >= 0x1f4u)
            value++;
        fs98_numeric_digits(value, 5, &write);
        break;
    case 0x36:
        word_value = (uint16_t)(*(const uint16_t *)bytes -
                           *(uint16_t *)((uint8_t *)global + 0x2a0));
        fs98_numeric_degrees(word_value, &write);
        break;
    case 0x37:
        wide_value = (uint64_t)*(const uint32_t *)bytes * 0x2000000ull;
        value = (uint32_t)(wide_value >> 32);
        if (value == 0 && (uint32_t)wide_value != 0)
            value++;
        fs98_numeric_digits(value, 6, &write);
        break;
    case 0x38:
        if (units == 2)
            fs98_numeric_digits(*(const uint16_t *)bytes, 5, &write);
        else
            fs98_numeric_digits((uint32_t)fs98_numeric_metres_to_feet(
                               *(const uint16_t *)bytes), 6, &write);
        break;
    case 0x39:
        if (units == 2) {
            fs98_numeric_format_ground_shift(bytes, global, &write);
        } else {
            signed_value = (int32_t)((uint32_t)*(const int32_t *)bytes +
                             *(uint32_t *)((uint8_t *)global + 0x20));
            value = fs98_numeric_abs_s32(signed_value, &write);
            fs98_numeric_digits((uint32_t)(fs98_numeric_metres_to_feet(value) >> 8),
                           6, &write);
        }
        break;
    case 0x3a:
        signed_value = *(const int32_t *)bytes;
        if (units == 2) {
            value = fs98_numeric_abs_s32(signed_value, &write);
            fs98_numeric_digits(value >> 8, 6, &write);
        } else {
            signed_value += (signed_value < 0) ? -0x27 : 0x27;
            value = fs98_numeric_abs_s32(signed_value, &write);
            fs98_numeric_digits((uint32_t)(fs98_numeric_metres_to_feet(value) >> 8),
                           6, &write);
        }
        break;
    case 0x3b:
        if (units == 0) {
            value = (uint32_t)*(const uint16_t *)bytes >> 1;
            value = ((value << 16) + 0x15ac3u) / 0x43bau;
            fs98_numeric_fixed(value >> 2, 8, 2, &write);
        } else {
            fs98_numeric_digits(((uint32_t)*(const uint16_t *)bytes + 8u) >> 4,
                           4, &write);
        }
        break;
    case 0x3c:
        if (units == 0) {
            signed_value = (int32_t)*(const int16_t *)bytes * 18 + 0x500;
            if (signed_value < 0)
                signed_value -= 0xa00;
            signed_value = (signed_value + 18) / 0xa00 + 0x20;
        } else {
            signed_value = ((int32_t)*(const int16_t *)bytes + 0x80) >> 8;
        }
        value = fs98_numeric_abs_s32(signed_value, &write);
        fs98_numeric_digits(value, 3, &write);
        break;
    case 0x3d:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        if (units == 2) {
            fs98_numeric_fixed(wide_value >> 8, 8, 6, &write);
        } else {
            wide_value = fs98_numeric_x87_scale32((int64_t)wide_value, 0x347e5,
                                          0x1000000);
            fs98_numeric_fixed((uint64_t)wide_value, 8, 6, &write);
        }
        break;
    case 0x3e:
        wide_value = fs98_numeric_metres_to_feet(*(const uint32_t *)bytes);
        fs98_numeric_digits((uint32_t)(wide_value >> 16), 5, &write);
        break;
    case 0x3f:
        if (units == 2)
            fs98_numeric_digits(*(const uint32_t *)bytes >> 16, 5, &write);
        else {
            wide_value = fs98_numeric_metres_to_feet(*(const uint32_t *)bytes);
            fs98_numeric_digits((uint32_t)(wide_value >> 16), 5, &write);
        }
        break;
    case 0x40:
        fs98_numeric_digits(*(const uint32_t *)bytes >> 16, 5, &write);
        break;
    case 0x41: case 0x42: case 0x43: case 0x44:
        value = fs98_numeric_abs_s32(*(const int16_t *)bytes, &write);
        fs98_numeric_fixed(value, 4, (int)(id - 0x40), &write);
        break;
    case 0x45: case 0x46: case 0x47:
        value = fs98_numeric_abs_s32(*(const int16_t *)bytes, &write);
        fs98_numeric_fixed(value, 8, (int)(id - 0x44), &write);
        break;
    case 0x48: case 0x49:
        value = fs98_numeric_abs_s32(*(const int16_t *)bytes, &write);
        fs98_numeric_fixed(value, 12, (int)(id - 0x47), &write);
        break;
    case 0x4a:
        value = fs98_numeric_abs_s32(*(const int16_t *)bytes, &write);
        fs98_numeric_fixed(value, 16, 1, &write);
        break;
    case 0x4b: case 0x4c: case 0x4d:
        word_value = *(const uint16_t *)bytes;
        if (id >= 0x4c)
            word_value = (uint16_t)(word_value -
                               *(uint16_t *)((uint8_t *)global + 0x2a0));
        if (id >= 0x4d)
            word_value = (uint16_t)(word_value -
                               *(uint16_t *)((uint8_t *)global + 0x2a2));
        value = (uint32_t)word_value * FS98_NUMERIC_U32(0x150);
        value = (value >> 16) + ((value & 0x8000u) != 0);
        if (value == 0)
            value += 360;
        fs98_numeric_digits(value, 3, &write);
        break;
    case 0x4e:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        wide_value = fs98_numeric_x87_scale64((int64_t)wide_value, 0x347e5,
                                      0x100000000ll);
        fs98_numeric_digits((uint32_t)wide_value, 6, &write);
        break;
    case 0x4f:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        fs98_numeric_digits((uint32_t)(wide_value >> 16), 6, &write);
        break;
    case 0x50:
        wide_value = fs98_numeric_abs_s64(fs98_numeric_load_s48(input), &write);
        if (units == 2)
            fs98_numeric_digits((uint32_t)(wide_value >> 16), 6, &write);
        else {
            wide_value = fs98_numeric_x87_scale64((int64_t)wide_value, 0x347e5,
                                          0x100000000ll);
            fs98_numeric_digits((uint32_t)wide_value, 6, &write);
        }
        break;
    }

    *write = '\0';
#ifdef FS98_NUMERIC_TEST_BREAK
    if (id == 0 && write != start)
        write[-1] ^= 1;
#endif
    if ((format & 0x10000u) != 0)
        fs98_numeric_trim_zeros(start);
    return start;
}

void FS98_STDCALL fs98_numeric_init(void)
{
    fs98_numeric_load_string_fn load_string;
    fs98_numeric_config_fn config;
    char directions[12];
    int32_t digit_count;
    double power = 1.0;

    load_string = FS98_NUMERIC_SLOT(FS98_NUMERIC_HOST, 0x2a0,
                             fs98_numeric_load_string_fn);
    load_string(0x9100, directions, 10);
    FS98_NUMERIC_U8(0x1f4) = (uint8_t)directions[0];
    FS98_NUMERIC_U8(0x320) = (uint8_t)directions[2];
    FS98_NUMERIC_U8(0x1f5) = (uint8_t)directions[4];
    FS98_NUMERIC_U8(0x1fc) = (uint8_t)directions[6];

    config = FS98_NUMERIC_SLOT(FS98_NUMERIC_HOST, 0xa8, fs98_numeric_config_fn);
    config((const char *)(fs98_numeric_data + 0x1cc),
              (const char *)(fs98_numeric_data + 0x1d4),
              (int32_t *)(fs98_numeric_data + 0x19c));
    config((const char *)(fs98_numeric_data + 0x1cc),
              (const char *)(fs98_numeric_data + 0x1b0),
              (int32_t *)(fs98_numeric_data + 0x1a0));

    digit_count = (int32_t)FS98_NUMERIC_U32(0x1a0);
    if (digit_count == 0)
        digit_count = 1;
    if (digit_count > 4)
        digit_count = 4;
    FS98_NUMERIC_U32(0x1a0) = (uint32_t)digit_count;
    while (digit_count-- > 0)
        power *= 10.0;
    FS98_NUMERIC_DBL(0x1a8) = power;
}

void FS98_STDCALL fs98_numeric_deinit(void)
{
}

/* ---- text-to-value service ------------------------------------------------
 *
 * Every retail parser first produces a signed 48.16 intermediate.  The four
 * validation modes below are the combinations used by its tiny wrappers:
 * integer/fixed-point and signed/unsigned.
 */
enum fs98_numeric_parse_mode {
    FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
    FS98_NUMERIC_PARSE_SIGNED_INTEGER,
    FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
    FS98_NUMERIC_PARSE_SIGNED_FIXED
};

static int32_t fs98_numeric_validate(const char *text,
                                   enum fs98_numeric_parse_mode mode)
{
    int has_digit = 0;
    int digits_ended = 0;
    int has_sign = 0;
    int has_minus = 0;
    int has_decimal = 0;
    const uint8_t decimal_char = FS98_NUMERIC_U8(0x198);

    while (*text != '\0') {
        uint8_t byte = (uint8_t)*text++;

        if (byte >= '0' && byte <= '9') {
            if (digits_ended)
                return 4;
            has_digit = 1;
        } else if (byte == '-') {
            /* QUIRK: retail never sets the flag it checks for a repeated
             * minus, so "--1" is accepted here (and rejected later only by
             * unsigned wrappers). */
            if (has_digit)
                return 8;
            has_sign = 1;
            has_minus = 1;
        } else if (byte == '+') {
            if (has_sign || has_digit)
                return 8;
            has_sign = 1;
        } else if (byte == decimal_char) {
            if (has_decimal)
                return 8;
            has_decimal = 1;
        } else if (byte == ' ' || byte == '\t') {
            if (has_digit)
                digits_ended = 1;
        } else {
            return 4;
        }
    }

    if (!has_digit)
        return 9;
    if ((mode == FS98_NUMERIC_PARSE_UNSIGNED_INTEGER ||
         mode == FS98_NUMERIC_PARSE_SIGNED_INTEGER) && has_decimal)
        return 10;
    if ((mode == FS98_NUMERIC_PARSE_UNSIGNED_INTEGER ||
         mode == FS98_NUMERIC_PARSE_UNSIGNED_FIXED) && has_minus)
        return 14;
    return 0;
}

static int32_t fs98_numeric_parse_fixed(const char *text,
                                      enum fs98_numeric_parse_mode mode,
                                      uint64_t *result_value)
{
    uint64_t integer_value = 0;
    uint32_t fraction = 0;
    uint32_t place = 1000;
    int negative = 0;
    int32_t result;
    const uint8_t decimal_char = FS98_NUMERIC_U8(0x198);

    result = fs98_numeric_validate(text, mode);
    if (result != 0)
        return result;

    while (*text != '\0') {
        uint8_t byte = (uint8_t)*text++;

        if (byte == '-') {
            negative = 1;
        } else if (byte == decimal_char) {
            while (*text != '\0') {
                uint32_t digit = (uint8_t)*text++ & 0xfu;

                if (place == 0 && digit != 0)
                    result = 13;
                fraction += digit * place;
                place /= 10;
            }
            break;
        } else if (byte >= '0' && byte <= '9') {
            integer_value = integer_value * 10ull + (byte - '0');
        }
    }

    *result_value = (integer_value << 16) |
                 (uint64_t)(((uint64_t)fraction << 16) / 10000u);
    if (negative)
        *result_value = 0ull - *result_value;
    return result;
}

static int64_t fs98_numeric_feet_to_metres(int32_t signed_value)
{
    uint32_t magnitude = (uint32_t)signed_value;
    uint64_t product_value;
    uint64_t result64;

    if (signed_value < 0)
        magnitude = 0u - magnitude;
    product_value = (uint64_t)magnitude * 0x4e07625fu;
    result64 = (product_value >> 16) +
                (((uint16_t)product_value & 0x8000u) != 0);
    if (signed_value < 0)
        result64 = 0ull - result64;
    return (int64_t)result64;
}

static int32_t fs98_numeric_check_u16(uint16_t word_value, const void *range)
{
    const uint16_t *bounds = (const uint16_t *)range;

    if (bounds != 0 && word_value < bounds[0])
        return 5;
    if (bounds != 0 && word_value > bounds[1])
        return 6;
    return 0;
}

static int32_t fs98_numeric_check_s16(int16_t signed_word, const void *range)
{
    const int16_t *bounds = (const int16_t *)range;

    if (bounds != 0 && signed_word < bounds[0])
        return 5;
    if (bounds != 0 && signed_word > bounds[1])
        return 6;
    return 0;
}

static int32_t fs98_numeric_check_u32(uint32_t value, const void *range)
{
    const uint32_t *bounds = (const uint32_t *)range;

    if (bounds != 0 && value < bounds[0])
        return 5;
    if (bounds != 0 && value > bounds[1])
        return 6;
    return 0;
}

static int32_t fs98_numeric_check_s32(int32_t signed_value, const void *range)
{
    const int32_t *bounds = (const int32_t *)range;

    if (bounds != 0 && signed_value < bounds[0])
        return 5;
    if (bounds != 0 && signed_value > bounds[1])
        return 6;
    return 0;
}

static int32_t fs98_numeric_store_u16(uint16_t word_value, void *output,
                               const void *range, int32_t result)
{
    int32_t range_result = fs98_numeric_check_u16(word_value, range);

    if (range_result != 0)
        return range_result;
    *(uint16_t *)output = word_value;
    return (result == 13) ? 13 : 0;
}

static int32_t fs98_numeric_store_s16(int16_t signed_word, void *output,
                               const void *range, int32_t result)
{
    int32_t range_result = fs98_numeric_check_s16(signed_word, range);

    if (range_result != 0)
        return range_result;
    *(int16_t *)output = signed_word;
    return (result == 13) ? 13 : 0;
}

static int32_t fs98_numeric_store_u32(uint32_t value, void *output,
                               const void *range, int32_t result)
{
    int32_t range_result = fs98_numeric_check_u32(value, range);

    if (range_result != 0)
        return range_result;
    *(uint32_t *)output = value;
    return (result == 13) ? 13 : 0;
}

static int32_t fs98_numeric_store_s32(int32_t signed_value, void *output,
                                      const void *range, int32_t result)
{
    int32_t range_result = fs98_numeric_check_s32(signed_value, range);

    if (range_result != 0)
        return range_result;
    *(int32_t *)output = signed_value;
    return (result == 13) ? 13 : 0;
}

static int32_t fs98_numeric_store_s48(int64_t value, void *output,
                                      const void *range, int32_t result)
{
    int32_t high = (int32_t)(value >> 16);
    uint16_t low = (uint16_t)value;

    if (range != 0) {
        const uint8_t *bytes = (const uint8_t *)range;
        int32_t minimum_high = *(const int32_t *)(bytes + 4);
        uint16_t minimum_low = *(const uint16_t *)(bytes + 2);
        int32_t maximum_high = *(const int32_t *)(bytes + 12);
        uint16_t maximum_low = *(const uint16_t *)(bytes + 10);

        if (high < minimum_high ||
            (high == minimum_high && low < minimum_low))
            return 5;
        /* Retail treats the upper fixed-point endpoint as exclusive. */
        if (high > maximum_high ||
            (high == maximum_high && low >= maximum_low))
            return 6;
    }

    *(uint16_t *)((uint8_t *)output + 2) = low;
    *(int32_t *)((uint8_t *)output + 4) = high;
    return (result == 13) ? 13 : 0;
}

static int32_t fs98_numeric_parse_degrees(const char *text, void *output,
                                        const void *range, uint16_t offset_a,
                                        uint16_t offset_b, int integer_only)
{
    uint64_t value64;
    uint32_t value;
    uint16_t output_word;
    int32_t result = fs98_numeric_parse_fixed(
        text, integer_only ? FS98_NUMERIC_PARSE_UNSIGNED_INTEGER :
                             FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
        &value64);

    if (result != 0 && result != 13)
        return result;
    if ((value64 >> 32) != 0)
        return 11;
    value = (uint32_t)value64;
    if (value == 0)
        return 5;
    if (value > 360u * 65536u)
        return 6;
    if (value == 360u * 65536u)
        value = 0;
    output_word = (uint16_t)(value / 360u + offset_a + offset_b);
    return fs98_numeric_store_u16(output_word, output, range, result);
}

static int32_t fs98_numeric_parse_scaled_word(const char *text, void *output,
                                       const void *range, uint32_t scale,
                                       uint32_t maximum)
{
    uint64_t value64;
    uint64_t product_value;
    uint32_t shifted;
    uint16_t word_value;
    int32_t result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                        &value64);

    if (result != 0 && result != 13)
        return result;
    if ((value64 >> 32) != 0)
        return 11;
    product_value = (uint64_t)(uint32_t)value64 * scale;
    shifted = (uint32_t)(product_value >> 16);
    word_value = (uint16_t)shifted;
    if (((uint16_t)product_value & 0x8000u) != 0) {
        word_value++;
        if (word_value == 0)
            return 11;
    }
    if (word_value > maximum)
        return 11;
    return fs98_numeric_store_u16(word_value, output, range, result);
}

static int32_t fs98_numeric_parse_com(const char *text, void *output,
                               const void *range, int format_32)
{
    uint64_t value64;
    uint32_t value;
    uint16_t integer_word;
    uint16_t fraction_word;
    uint16_t word_value;
    uint32_t product;
    uint8_t remainder_byte;
    int32_t result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                        &value64);

    if (result != 0 && result != 13)
        return result;
    if ((value64 >> 32) != 0)
        return 11;
    value = (uint32_t)value64;
    if (!format_32) {
        if (value > 0x88f999u)
            return 11;
        if (value < 0x120000u ||
            (value > 0x24f999u && value < 0x640000u))
            return 12;
    } else {
        if (value > 0x75f333u)
            return 11;
        if (value < 0x6c0000u)
            return 12;
    }

    integer_word = (uint16_t)(value >> 16);
    if (integer_word >= 100) {
        integer_word = (uint16_t)(integer_word - 100);
        if (integer_word > 100)
            return 11;
    }
    word_value = (uint16_t)(((integer_word / 10) << 12) |
                       ((integer_word % 10) << 8));

    fraction_word = (uint16_t)value;
    product = (uint32_t)fraction_word * 10u + 10u;
    word_value |= (uint16_t)((product >> 16) << 4);
    remainder_byte = (uint8_t)(product >> 8);
    if (!format_32) {
        if (remainder_byte != 0) {
            word_value += 2;
            if (remainder_byte != 0x40) {
                word_value += 3;
                if (remainder_byte != 0x80)
                    word_value += 2;
            }
        }
    } else if (remainder_byte != 0) {
        word_value += 5;
    }
    return fs98_numeric_store_u16(word_value, output, range, result);
}

static void fs98_numeric_skip_spaces(const char **text_cursor)
{
    while (**text_cursor == ' ' || **text_cursor == '\t')
        (*text_cursor)++;
}

struct fs98_numeric_angle_parts {
    uint32_t degrees;
    uint32_t minutes;
    uint32_t minute_fraction;
    uint32_t seconds;
    uint32_t second_fraction;
};

static int32_t fs98_numeric_read_angle_number(const char **text_cursor,
                                            uint32_t *integer_out,
                                            uint32_t *fraction_out)
{
    const char *text = *text_cursor;
    uint32_t integer = 0;
    uint32_t decimal = 0;
    uint32_t place = 1000;

    fs98_numeric_skip_spaces(&text);
    if (*text == '+')
        text++;
    else if (*text == '-')
        return 14;
    while (*text >= '0' && *text <= '9') {
        integer = integer * 10u + (uint32_t)(*text - '0');
        text++;
    }
    if ((uint8_t)*text == FS98_NUMERIC_U8(0x198)) {
        text++;
        while (*text >= '0' && *text <= '9') {
            decimal += (uint32_t)(*text - '0') * place;
            place /= 10u;
            text++;
        }
    }
    *text_cursor = text;
    *integer_out = integer;
    *fraction_out = (decimal << 16) / 10000u;
    return 0;
}

static int32_t fs98_numeric_read_angle_part(const char **text_cursor,
                                          struct fs98_numeric_angle_parts *parts,
                                          int default_part)
{
    uint32_t integer;
    uint32_t fraction;
    int part = default_part;
    int32_t result;

    fs98_numeric_skip_spaces(text_cursor);
    if (**text_cursor == '\0')
        return 0;
    result = fs98_numeric_read_angle_number(text_cursor, &integer, &fraction);
    if (result != 0)
        return result;
    fs98_numeric_skip_spaces(text_cursor);
    if (**text_cursor == '*' || (uint8_t)**text_cursor == 0xb9u) {
        part = 6;
        (*text_cursor)++;
    } else if (**text_cursor == '\'') {
        part = 7;
        (*text_cursor)++;
    } else if (**text_cursor == '"') {
        part = 8;
        (*text_cursor)++;
    } else if (**text_cursor != '\0' &&
               (**text_cursor < '0' || **text_cursor > '9')) {
        return 4;
    }

    if (part == 6) {
        parts->degrees = integer;
    } else if (part == 7) {
        parts->minutes = integer;
        parts->minute_fraction = fraction;
    } else if (part == 8) {
        parts->seconds = integer;
        parts->second_fraction =
            (fraction * 10000u + 0x8000u) >> 16;
    } else {
        return 4;
    }
    return 0;
}

static uint64_t fs98_numeric_round_shift(uint64_t value64, int bit_count)
{
    if (bit_count <= 0)
        return value64;
    value64 >>= bit_count - 1;
    value64++;
    return value64 >> 1;
}

static int32_t fs98_numeric_build_angle(
    const struct fs98_numeric_angle_parts *parts, int latitude,
    uint64_t *result_value)
{
    static const uint64_t longitude_scale[4] = {
        0x00b60b60b60b60b6ull, 0x0308b91419ca252aull,
        0x033c812681e8b02dull, 0x00054d6bf8a3b0d8ull
    };
    static const uint64_t latitude_scale[4] = {
        0x0001b21a8e38e38eull, 0x00073c2d097b425eull,
        0x0007b7a781948b0full, 0x00000ca4e97b6bcbull
    };
    const uint64_t *scale = latitude ? latitude_scale :
                                           longitude_scale;
    uint64_t value64;

    if (parts->second_fraction > 9999u ||
        parts->seconds > 59u || parts->minutes > 59u ||
        parts->degrees >= (uint32_t)(latitude ? 90 : 180))
        return 11;

    value64 = scale[3] * parts->second_fraction;
    value64 = fs98_numeric_round_shift(value64, 6);
    value64 += scale[2] * parts->seconds;
    value64 = fs98_numeric_round_shift(value64, 6);
    value64 += scale[1] * parts->minutes;
    value64 = fs98_numeric_round_shift(value64, 8);
    value64 += scale[0] * parts->degrees;
    *result_value = value64;
    return 0;
}

static int32_t fs98_numeric_parse_angle_text(const char *text, int latitude,
                                      void *output, const void *range)
{
    void *global = FS98_NUMERIC_GLOBAL;
    struct fs98_numeric_angle_parts parts = { 0, 0, 0, 0, 0 };
    uint64_t value64;
    uint16_t output_word;
    int32_t high;
    int64_t raw;
    int negative = latitude ?
        (*(int32_t *)((uint8_t *)global + 0xda8) != 2) :
        (*(int32_t *)((uint8_t *)global + 0xdac) == 1);
    int32_t result;

    fs98_numeric_skip_spaces(&text);
    if (*text == (char)FS98_NUMERIC_U8(latitude ? 0x1f4 : 0x1f5) ||
        *text == (char)FS98_NUMERIC_U8(latitude ? 0x320 : 0x1fc)) {
        negative = (*text ==
                     (char)FS98_NUMERIC_U8(latitude ? 0x320 : 0x1fc));
        text++;
    } else if ((*text < '0' || *text > '9') && *text != '\0') {
        return 16;
    }

    result = fs98_numeric_read_angle_part(&text, &parts, 6);
    if (result != 0)
        return result;
    result = fs98_numeric_read_angle_part(&text, &parts, 7);
    if (result != 0)
        return result;
    if (parts.minute_fraction != 0) {
        uint32_t product = parts.minute_fraction * 60u;

        parts.seconds = product >> 16;
        parts.second_fraction =
            ((product & 0xffffu) * 10000u + 0x8000u) >> 16;
    } else {
        result = fs98_numeric_read_angle_part(&text, &parts, 8);
        if (result != 0)
            return result;
    }
    result = fs98_numeric_build_angle(&parts, latitude, &value64);
    if (result != 0)
        return result;
    if (negative)
        value64 = 0ull - value64;

    high = (int32_t)(value64 >> 32);
    output_word = (uint16_t)(value64 >> 16);
    if (((uint16_t)value64 & 0x8000u) != 0)
        output_word++;
    raw = ((int64_t)high << 16) | output_word;

    if (range != 0) {
        int64_t minimum64 = fs98_numeric_load_s48(range);
        int64_t maximum64 = fs98_numeric_load_s48((const uint8_t *)range + 8);
        if (raw < minimum64)
            return 5;
        if (raw > maximum64)
            return 6;
    }
    *(uint16_t *)((uint8_t *)output + 2) = output_word;
    *(int32_t *)((uint8_t *)output + 4) = high;
    return 0;
}

int32_t FS98_STDCALL fs98_numeric_parse_internal(const char *text,
                                               uint32_t format,
                                               void *output,
                                               const void *range)
{
    void *global = FS98_NUMERIC_GLOBAL;
    uint32_t id = format & ~0x10000u;
    uint64_t parsed_value;
    uint32_t value;
    int32_t signed_value;
    int64_t converted_value;
    int32_t result;
    int16_t units;

    if (output == 0)
        return 2;
    if (id >= 0x51)
        return 1;
    units = *(int16_t *)((uint8_t *)global + 0xc18);

    switch (id) {
    case 0x05: case 0x08: case 0x09: case 0x0d:
    case 0x10: case 0x11: case 0x18: case 0x33:
    case 0x34: case 0x35: case 0x37:
    case 0x41: case 0x42: case 0x43: case 0x44:
    case 0x45: case 0x46: case 0x47: case 0x48:
    case 0x49: case 0x4a:
        return 1;
    case 0x00:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        if ((parsed_value >> 32) != 0 || (parsed_value >> 16) > 0xffffu)
            return 11;
        return fs98_numeric_store_u16((uint16_t)(parsed_value >> 16), output,
                                 (const void *)range, 0);
    case 0x01: case 0x40:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        if ((parsed_value >> 32) != 0)
            return 11;
        return fs98_numeric_store_u32((uint32_t)parsed_value, output,
                                 (const void *)range, result);
    case 0x02:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        if ((parsed_value >> 32) != 0)
            return 11;
        return fs98_numeric_store_s32((int32_t)((uint32_t)parsed_value - 0x40000000u),
                                 output, (const void *)range, result);
    case 0x03: case 0x39:
        if (id == 0x39 && units == 2)
            goto parse_signed_shift;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        {
            int32_t high = (int32_t)(parsed_value >> 32);
            uint32_t magnitude = (high < 0) ?
                                 0u - (uint32_t)high : (uint32_t)high;
            if (magnitude > 0xffffu)
                return 11;
        }
        signed_value = (int32_t)((int64_t)parsed_value >> 16);
        signed_value = (int32_t)fs98_numeric_feet_to_metres(signed_value);
        signed_value -= *(int32_t *)((uint8_t *)global + 0x20);
        return fs98_numeric_store_s32(signed_value, output, (const void *)range,
                                 result);
    case 0x04:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range,
                                     *(uint16_t *)((uint8_t *)global + 0x2a0),
                                     *(uint16_t *)((uint8_t *)global + 0x2a2), 0);
    case 0x06: case 0x38:
        if (id == 0x38 && units == 2)
            goto parse_u16_integer;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0)
            return result;
        if ((parsed_value >> 48) != 0 || (parsed_value >> 16) > 100000u)
            return 11;
        converted_value = fs98_numeric_feet_to_metres(
            (int32_t)(parsed_value >> 16));
        return fs98_numeric_store_u16(
            (uint16_t)((uint64_t)converted_value >> 16), output,
                                 (const void *)range, 0);
    case 0x07:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)(parsed_value >> 16);
        if ((parsed_value >> 32) != 0 || value > 255u)
            return 11;
        if (range != 0 && value < *(uint8_t *)range)
            return 5;
        if (range != 0 && value > *((uint8_t *)range + 1))
            return 6;
        *(uint8_t *)output = (uint8_t)value;
        return 0;
    case 0x0a: case 0x3a:
        if (id == 0x3a && units == 2)
            goto parse_signed_shift;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        converted_value = (int64_t)parsed_value >> 16;
        if (converted_value < -0x3482bb0ll ||
            converted_value > 0x3482bb0ll)
            return 11;
        signed_value = (int32_t)(fs98_numeric_feet_to_metres(
            (int32_t)converted_value) >> 8);
        return fs98_numeric_store_s32(signed_value, output, (const void *)range, 0);
    case 0x0b: case 0x0c:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)(parsed_value >> 16);
        if ((parsed_value >> 32) != 0 || value > (id == 0x0b ? 10u : 100u))
            return 11;
        if (range != 0 && value < *(uint8_t *)range)
            return 5;
        if (range != 0 && value > *((uint8_t *)range + 1))
            return 6;
        *(uint8_t *)output = (uint8_t)value;
        return 0;
    case 0x0e:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range, 0, 0, 0);
    case 0x0f:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        converted_value = (int64_t)parsed_value >> 16;
        if (converted_value < -32768 || converted_value > 32767)
            return 11;
        return fs98_numeric_store_s16((int16_t)converted_value, output,
                                 (const void *)range, 0);
    case 0x12: case 0x32:
        return fs98_numeric_parse_com(text, output, (const void *)range,
                                 id == 0x32);
    case 0x13:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        if ((parsed_value >> 32) != 0)
            return 11;
        value = (uint32_t)(parsed_value >> 16);
        if (value >= 0x200u)
            return 11;
        if (value == 0 && (uint16_t)parsed_value < 0x4000u)
            return 12;
        value = (uint32_t)parsed_value >> 10;
        return fs98_numeric_store_u16((uint16_t)value, output,
                                 (const void *)range, result);
    case 0x14:
        return fs98_numeric_parse_angle_text(text, 0, output,
                                        (const void *)range);
    case 0x15:
        return fs98_numeric_parse_angle_text(text, 1, output,
                                        (const void *)range);
    case 0x16: case 0x3d: case 0x4e:
        if ((id == 0x3d && units == 2))
            goto parse_s48_fixed;
        result = fs98_numeric_parse_fixed(text,
            id == 0x4e ? FS98_NUMERIC_PARSE_SIGNED_INTEGER : FS98_NUMERIC_PARSE_SIGNED_FIXED,
            &parsed_value);
        if (result != 0 && result != 13)
            return result;
        signed_value = (int32_t)((int64_t)parsed_value >> 16);
        converted_value = fs98_numeric_feet_to_metres(signed_value);
        goto store_s48;
    case 0x17: case 0x4f:
parse_s48_fixed:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        converted_value = (int64_t)parsed_value;
store_s48:
        return fs98_numeric_store_s48(converted_value, output, range, result);
    case 0x19:
        {
            const char *octal = text;

            while (*octal != '\0') {
                if (*octal < '0' || *octal > '7')
                    return 4;
                octal++;
            }
        }
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)(parsed_value >> 16);
        if ((parsed_value >> 32) != 0 || value > 7777u)
            return 15;
        {
            uint16_t bcd = 0;
            int digit_index;
            for (digit_index = 0; digit_index < 4; digit_index++) {
                bcd = (uint16_t)((bcd >> 4) |
                                 ((value % 10u) << 12));
                value /= 10;
            }
            return fs98_numeric_store_u16(bcd, output,
                                     (const void *)range, 0);
        }
    case 0x1a: case 0x1b:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)(parsed_value >> 16);
        if ((parsed_value >> 48) != 0 ||
            value > (id == 0x1a ? 999999u : 9999999u))
            return 11;
        return fs98_numeric_store_u32(value, output, (const void *)range, 0);
    case 0x1c:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        if ((parsed_value >> 32) != 0 || (parsed_value >> 16) > 99u)
            return 11;
        return fs98_numeric_store_u32((uint32_t)parsed_value, output,
                                 (const void *)range, result);
    case 0x1d: case 0x1e:
parse_signed_shift:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        signed_value = (int32_t)((int64_t)parsed_value >> 8);
        if (id == 0x1e || id == 0x39)
            signed_value -= *(int32_t *)((uint8_t *)global + 0x20);
        return fs98_numeric_store_s32(signed_value, output, (const void *)range,
                                 result);
    case 0x1f: case 0x20: case 0x21: case 0x22:
parse_u16_integer:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)(parsed_value >> 16);
        if ((parsed_value >> 32) != 0 || value >
            (id == 0x1f ? 9u : id == 0x20 ? 99u :
             id == 0x21 ? 999u : id == 0x22 ? 9999u : 65535u))
            return 11;
        return fs98_numeric_store_u16((uint16_t)value, output,
                                 (const void *)range, 0);
    case 0x23:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range, 0, 0, 0);
    case 0x24: case 0x3b:
        if (id == 0x3b && units == 0)
            goto parse_format_25;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0)
            return result;
        value = (uint32_t)parsed_value >> 12;
        if ((parsed_value >> 32) > 0x24u || value > 0x4a10u)
            return 11;
        if (value < 0x34f0u)
            return 12;
        return fs98_numeric_store_u16((uint16_t)value, output,
                                 (const void *)range, 0);
    case 0x25:
parse_format_25:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0)
            return result;
        if ((parsed_value >> 32) > 0x25u)
            return 11;
        value = (uint32_t)parsed_value;
        if (value < 25u * 65536u)
            return 12;
        if (value > 35u * 65536u)
            return 11;
        value = (uint32_t)(((uint64_t)value * 0x21dd17u) >> 28);
        return fs98_numeric_store_u16((uint16_t)value, output,
                                 (const void *)range, 0);
    case 0x26: case 0x3c:
        if (id == 0x3c && units == 0)
            goto parse_format_27;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_FIXED,
                                      &parsed_value);
        if (result != 0)
            return result;
        converted_value = (int64_t)parsed_value;
        if (converted_value < -95ll * 65536ll)
            return 12;
        if (converted_value > 75ll * 65536ll)
            return 11;
        return fs98_numeric_store_s16((int16_t)(converted_value >> 8), output,
                                 (const void *)range, 0);
    case 0x27:
parse_format_27:
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_FIXED,
                                      &parsed_value);
        if (result != 0)
            return result;
        converted_value = (int64_t)parsed_value;
        if (converted_value < -139ll * 65536ll)
            return 12;
        if (converted_value > 167ll * 65536ll)
            return 11;
        signed_value = (int32_t)converted_value - 0x200000;
        signed_value = (int32_t)(((int64_t)signed_value * 0x8e39) >> 24);
        return fs98_numeric_store_s16((int16_t)signed_value, output,
                                 (const void *)range, 0);
    case 0x28: case 0x29: case 0x2a: case 0x2b:
        return fs98_numeric_parse_scaled_word(text, output,
            (const void *)range, 10,
            id == 0x28 ? 99u : id == 0x29 ? 999u :
            id == 0x2a ? 9999u : 65535u);
    case 0x2c: case 0x2d: case 0x2e:
        return fs98_numeric_parse_scaled_word(text, output,
            (const void *)range, 100,
            id == 0x2c ? 999u : id == 0x2d ? 9999u : 65535u);
    case 0x2f: case 0x30:
        return fs98_numeric_parse_scaled_word(text, output,
            (const void *)range, 1000, id == 0x2f ? 9999u : 65535u);
    case 0x31:
        return fs98_numeric_parse_scaled_word(text, output,
                                         (const void *)range,
                                         10000, 65535);
    case 0x36:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range,
                                     *(uint16_t *)((uint8_t *)global + 0x2a0),
                                     0, 0);
    case 0x3e: case 0x3f:
        if (id == 0x3f && units == 2) {
            id = 0x40;
            result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                          &parsed_value);
            if (result != 0 && result != 13)
                return result;
            if ((parsed_value >> 32) != 0)
                return 11;
            return fs98_numeric_store_u32((uint32_t)parsed_value, output,
                                     (const void *)range, result);
        }
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_UNSIGNED_FIXED,
                                      &parsed_value);
        if (result != 0 && result != 13)
            return result;
        if (parsed_value > 0x0000000347b1ffffull)
            return 11;
        converted_value = fs98_numeric_feet_to_metres(
            (int32_t)(parsed_value >> 16));
        if ((uint64_t)converted_value >> 32)
            return 11;
        return fs98_numeric_store_u32((uint32_t)converted_value, output,
                                 (const void *)range, result);
    case 0x4b:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range, 0, 0, 1);
    case 0x4c:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range,
                                     *(uint16_t *)((uint8_t *)global + 0x2a0),
                                     0, 1);
    case 0x4d:
        return fs98_numeric_parse_degrees(text, output,
                                     (const void *)range,
                                     *(uint16_t *)((uint8_t *)global + 0x2a0),
                                     *(uint16_t *)((uint8_t *)global + 0x2a2), 1);
    case 0x50:
        if (units == 2)
            goto parse_s48_fixed;
        result = fs98_numeric_parse_fixed(text, FS98_NUMERIC_PARSE_SIGNED_INTEGER,
                                      &parsed_value);
        if (result != 0)
            return result;
        converted_value = fs98_numeric_feet_to_metres(
            (int32_t)((int64_t)parsed_value >> 16));
        goto store_s48;
    }
    return 1;
}

int32_t FS98_STDCALL fs98_numeric_parse(const char *text, uint32_t format,
                                      void *output)
{
    return fs98_numeric_parse_internal(text, format, output, 0);
}

#ifdef FS98_NUMERIC_MODULE

/* The order is retail's: host, GLOBAL, UTIL.  numeric_imports.h indexes it. */
struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,   0 },
    { FS98_MOD_GLOBAL, 0 },
    { FS98_MOD_UTIL,   0 },
    { 0, 0 }
};

static const char fs98_numeric_degrees_0[] = "%d* %2.0f'";
static const char fs98_numeric_degrees_1[] = "%d* %2.1f'";
static const char fs98_numeric_degrees_2[] = "%d* %2.2f'";
static const char fs98_numeric_degrees_3[] = "%d* %2.3f'";
static const char fs98_numeric_degrees_4[] = "%d* %2.4f'";

struct {
    uint32_t id;
    void (FS98_STDCALL *init)(void);
    void (FS98_STDCALL *deinit)(void);
    void *reserved_a[3];
    char *(FS98_STDCALL *format)(const void *, uint32_t, char *);
    int32_t (FS98_STDCALL *parse)(const char *, uint32_t, void *);
    int32_t (FS98_STDCALL *parse_internal)(const char *, uint32_t, void *,
                                          const void *);
    void *reserved_b[3];
    const char *degree_formats[5];
} Linkage = {
    FS98_MOD_NUMERIC,
    fs98_numeric_init,
    fs98_numeric_deinit,
    { 0, 0, 0 },
    fs98_numeric_format,
    fs98_numeric_parse,
    fs98_numeric_parse_internal,
    { 0, 0, 0 },
    {
        fs98_numeric_degrees_0,
        fs98_numeric_degrees_1,
        fs98_numeric_degrees_2,
        fs98_numeric_degrees_3,
        fs98_numeric_degrees_4
    }
};

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}

#endif /* FS98_NUMERIC_MODULE */
