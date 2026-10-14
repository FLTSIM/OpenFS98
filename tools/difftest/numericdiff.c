/* Differential test for NUMERIC.DLL's formatter.
 *
 * Retail and the reimplementation are given the same fake host, GLOBAL block
 * and UTIL conversion slots.  Every format id is exercised with edge values,
 * both zero-trimming states, all unit modes, both angle display modes and all
 * four fractional-minute precisions.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fs98/numeric.h"

typedef char *(FS98_STDCALL *fs98_numeric_format_fn)(const void *value,
                                                     uint32_t format,
                                                     char *output);
typedef void (FS98_STDCALL *fs98_numeric_init_fn)(void);
typedef int32_t (FS98_STDCALL *fs98_numeric_parse_fn)(const char *text,
                                                    uint32_t format,
                                                    void *output);
typedef int32_t (FS98_STDCALL *fs98_numeric_parse_range_fn)(
    const char *text, uint32_t format, void *output, const void *range);

static uint8_t host_blob[0x320];
static uint8_t global_blob[0xd00];
static uint8_t util_blob[0xd0];
static int32_t display_fractional = 1;
static int32_t fractional_digits = 2;
static uint32_t format_vector;
static uint32_t parse_vector;
static const char *side;

#define TEST_SLOT(table, offset, function) \
    (*(void **)((table) + (offset)) = (void *)(function))

static int32_t FS98_STDCALL stub_load_string(uint32_t id, char *buffer,
                                           int32_t length)
{
    static const char directions[] = "N S E W";
    int32_t index;

    (void)id;
    for (index = 0; index + 1 < length && directions[index] != '\0';
         index++)
        buffer[index] = directions[index];
    if (length > 0)
        buffer[index] = '\0';
    return index;
}

static int32_t FS98_STDCALL stub_config(const char *section,
                                      const char *key, int32_t *value)
{
    (void)section;
    if (strcmp(key, "DISPLAY_FRACTIONAL_MINUTES") == 0)
        *value = display_fractional;
    else if (strcmp(key, "FRACTIONAL_MINUTE_DIGITS") == 0)
        *value = fractional_digits;
    return *value;
}

static uint32_t FS98_STDCALL stub_strlen(const char *text)
{
    uint32_t length = 0;

    if (text == 0)
        return 0;
    while (text[length] != '\0')
        length++;
    return length;
}

static void FS98_STDCALL stub_fixed48_to_double(double *output,
                                                const void *input)
{
    const uint8_t *input_bytes = (const uint8_t *)input;
    uint32_t high = *(const uint32_t *)(input_bytes + 4);
    uint16_t low_word = *(const uint16_t *)(input_bytes + 2);

    *output = (double)high + (double)low_word / 65536.0;
}

static void FS98_STDCALL stub_fixed48_to_double_signed(double *output,
                                                       const void *input)
{
    const uint8_t *input_bytes = (const uint8_t *)input;
    int32_t high = *(const int32_t *)(input_bytes + 4);
    uint16_t low_word = *(const uint16_t *)(input_bytes + 2);

    *output = (double)high + (double)low_word / 65536.0;
}

static LONG WINAPI exception_filter(EXCEPTION_POINTERS *exception)
{
    printf("fault in %s at vector %lu: exception 0x%08lx at %p\n",
           side ? side : "setup", (unsigned long)format_vector,
           (unsigned long)exception->ExceptionRecord->ExceptionCode,
           exception->ExceptionRecord->ExceptionAddress);
    fflush(stdout);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

static uint32_t test_rand(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int test_one(fs98_numeric_format_fn retail,
                    const uint8_t *value_bytes, uint32_t format,
                    uint32_t case_index, uint32_t raw_low, uint32_t raw_high)
{
    char retail_text[128];
    char our_text[128];
    char *retail_output;
    char *our_output;

    memset(retail_text, 0xa5, sizeof retail_text);
    memset(our_text, 0xa5, sizeof our_text);
    format_vector++;
    side = "retail";
    retail_output = retail(value_bytes, format, retail_text);
    side = "ours";
    our_output = fs98_numeric_format(value_bytes, format, our_text);
    side = 0;

    if ((retail_output == 0) != (our_output == 0) ||
        (retail_output != 0 && strcmp(retail_text, our_text) != 0)) {
        printf("mismatch vector %lu case=%lu id=0x%05lx "
               "raw=%08lx:%08lx retail=[%s] ours=[%s]\n",
               (unsigned long)format_vector, (unsigned long)case_index,
               (unsigned long)format, (unsigned long)raw_high,
               (unsigned long)raw_low,
               retail_output ? retail_text : "<null>",
               our_output ? our_text : "<null>");
        return 1;
    }
    return 0;
}

static int test_parse_one(fs98_numeric_parse_fn retail,
                          const char *text, uint32_t format,
                          uint32_t case_index)
{
    uint8_t retail_data[24];
    uint8_t our_data[24];
    int32_t retail_result;
    int32_t our_result;

    memset(retail_data, 0xa5, sizeof retail_data);
    memset(our_data, 0xa5, sizeof our_data);
    parse_vector++;
    side = "retail parser";
    retail_result = retail(text, format, retail_data);
    side = "our parser";
    our_result = fs98_numeric_parse(text, format, our_data);
    side = 0;

    if (retail_result != our_result ||
        memcmp(retail_data, our_data, sizeof retail_data) != 0) {
        uint32_t index;

        printf("parse mismatch vector %lu case=%lu id=0x%05lx text=[%s] "
               "ret=%ld/%ld retail=", (unsigned long)parse_vector,
               (unsigned long)case_index, (unsigned long)format, text,
               (long)retail_result, (long)our_result);
        for (index = 0; index < 8; index++)
            printf("%02x", retail_data[index]);
        printf(" ours=");
        for (index = 0; index < 8; index++)
            printf("%02x", our_data[index]);
        printf("\n");
        return 1;
    }
    return 0;
}

static int test_parse_range_one(fs98_numeric_parse_range_fn retail,
                                const char *text, uint32_t format,
                                const void *range, uint32_t case_index)
{
    uint8_t retail_data[24];
    uint8_t our_data[24];
    int32_t retail_result;
    int32_t our_result;

    memset(retail_data, 0xa5, sizeof retail_data);
    memset(our_data, 0xa5, sizeof our_data);
    parse_vector++;
    side = "retail range parser";
    retail_result = retail(text, format, retail_data, range);
    side = "our range parser";
    our_result = fs98_numeric_parse_internal(text, format, our_data, range);
    side = 0;

    if (retail_result != our_result ||
        memcmp(retail_data, our_data, sizeof retail_data) != 0) {
        uint32_t index;

        printf("range mismatch vector %lu case=%lu id=0x%05lx text=[%s] "
               "ret=%ld/%ld retail=", (unsigned long)parse_vector,
               (unsigned long)case_index, (unsigned long)format, text,
               (long)retail_result, (long)our_result);
        for (index = 0; index < 8; index++)
            printf("%02x", retail_data[index]);
        printf(" ours=");
        for (index = 0; index < 8; index++)
            printf("%02x", our_data[index]);
        printf("\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    static const uint64_t edge_values[] = {
        0ull, 1ull, 5ull, 9ull, 10ull, 15ull, 16ull, 39ull, 40ull,
        90ull, 91ull, 100ull, 101ull, 127ull, 128ull, 255ull, 256ull,
        499ull, 500ull, 999ull, 1000ull, 0x7fffull, 0x8000ull,
        0xffffull, 0x10000ull, 0x7fffffffull, 0x80000000ull,
        0xffffffffull, 0x0000000100000000ull,
        0x00007fffffffffffll, 0x0000800000000000ull,
        0x7fffffffffffffffull, 0x8000000000000000ull,
        0xffffffffffffffffull, 0x123456789abcdef0ull,
        0xfedcba9876543210ull
    };
    static const int32_t ground_values[] = { 0, 0x12345, -0x23456 };
    HMODULE retail_module;
    struct fs98_import *imports;
    uint8_t *linkage;
    fs98_numeric_init_fn retail_init;
    fs98_numeric_format_fn retail_format;
    fs98_numeric_parse_fn retail_parse;
    fs98_numeric_parse_range_fn retail_parse_range;
    uint32_t seed = 0x6e756d31u;
    uint32_t case_index = 0;
    uint32_t display;
    uint32_t digits;
    uint32_t units;
    uint32_t ground;
    uint32_t flag;
    uint32_t id;
    uint32_t edge;
    uint32_t random_index;
    int failures = 0;

    SetUnhandledExceptionFilter(exception_filter);
    retail_module = LoadLibraryA("NUMERIC_retail.dll");
    if (retail_module == 0) {
        printf("cannot load NUMERIC_retail.dll (error %lu)\n",
               (unsigned long)GetLastError());
        return 2;
    }

    imports = (struct fs98_import *)GetProcAddress(retail_module,
                                                  "ImportTable");
    linkage = (uint8_t *)GetProcAddress(retail_module, "Linkage");
    if (imports == 0 || linkage == 0) {
        printf("retail NUMERIC exports are missing\n");
        return 2;
    }

    TEST_SLOT(host_blob, 0xa8, stub_config);
    TEST_SLOT(host_blob, 0x2a0, stub_load_string);
    TEST_SLOT(host_blob, 0x2b8, stub_strlen);
    TEST_SLOT(util_blob, 0xc0, stub_fixed48_to_double);
    TEST_SLOT(util_blob, 0xc8, stub_fixed48_to_double_signed);
    imports[0].link = (struct fs98_linkage *)host_blob;
    imports[1].link = (struct fs98_linkage *)global_blob;
    imports[2].link = (struct fs98_linkage *)util_blob;
    fs98_numeric_set_host(host_blob);
    fs98_numeric_set_global(global_blob);
    fs98_numeric_set_util(util_blob);

    retail_init = *(fs98_numeric_init_fn *)(linkage + 0x04);
    retail_format = *(fs98_numeric_format_fn *)(linkage + 0x18);
    retail_parse = *(fs98_numeric_parse_fn *)(linkage + 0x1c);
    retail_parse_range = *(fs98_numeric_parse_range_fn *)(linkage + 0x20);

    for (display = 0; display <= 1 && failures < 20; display++) {
        uint32_t first_digits = display ? 1u : 2u;
        uint32_t last_digits = display ? 4u : 2u;

        for (digits = first_digits; digits <= last_digits; digits++) {
            display_fractional = (int32_t)display;
            fractional_digits = (int32_t)digits;
            retail_init();
            fs98_numeric_init();

            for (units = 0; units < 3 && failures < 20; units++) {
                *(int16_t *)(global_blob + 0xc18) = (int16_t)units;
                for (ground = 0;
                     ground < sizeof ground_values / sizeof ground_values[0];
                     ground++) {
                    *(int32_t *)(global_blob + 0x20) = ground_values[ground];
                    *(uint16_t *)(global_blob + 0x2a0) =
                        (uint16_t)(ground * 0x1111u);
                    *(uint16_t *)(global_blob + 0x2a2) =
                        (uint16_t)(ground * 0x0203u);
                    for (flag = 0; flag < 2; flag++) {
                        for (id = 0; id < 0x51; id++) {
                            for (edge = 0;
                                 edge < sizeof edge_values /
                                          sizeof edge_values[0];
                                 edge++) {
                                uint64_t wide_value = edge_values[edge];
                                failures += test_one(
                                    retail_format,
                                    (const uint8_t *)&wide_value,
                                    id | (flag ? 0x10000u : 0u),
                                    case_index++, (uint32_t)wide_value,
                                    (uint32_t)(wide_value >> 32));
                                if (failures >= 20)
                                    break;
                            }
                            if (failures >= 20)
                                break;
                        }
                    }
                }

                for (random_index = 0; random_index < 200 && failures < 20;
                     random_index++) {
                    uint64_t wide_value = test_rand(&seed);
                    wide_value |= (uint64_t)test_rand(&seed) << 32;
                    for (id = 0; id < 0x51; id++)
                        failures += test_one(retail_format,
                            (const uint8_t *)&wide_value, id, case_index++,
                            (uint32_t)wide_value, (uint32_t)(wide_value >> 32));
                }
            }
        }
    }

    printf("numeric formatter: %lu comparisons, %d mismatches\n",
           (unsigned long)format_vector, failures);

    if (failures == 0) {
        static const uint8_t zero_range[16] = { 0 };
        static const uint8_t pattern_range[16] = {
            0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde,
            0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0xed, 0x0f
        };
        static const char *parse_cases[] = {
            "", " ", "0", "+0", "-0", "1", "9", "10", "99",
            "100", "999", "1000", "65535", "65536", "-1", "+1",
            ".", ".5", "1.", "1.0", "1.2345", "1.23450",
            "1.23456", "-123.5", "  +12.50  ", "12 3", "++1",
            "--1", "1,2", "abc", "360", "360.0", "100000",
            "123.455", "123.450", "E123* 45.67'", "W001* 02' 03.4000\"",
            "N45* 30.00'", "S89* 59' 59.9999\""
        };
        uint32_t parse_case = 0;

        for (display = 0; display <= 1 && failures < 20; display++) {
            display_fractional = (int32_t)display;
            fractional_digits = 2;
            retail_init();
            fs98_numeric_init();
            for (units = 0; units < 3 && failures < 20; units++) {
                *(int16_t *)(global_blob + 0xc18) = (int16_t)units;
                for (id = 0; id < 0x51 && failures < 20; id++) {
                    for (edge = 0;
                         edge < sizeof edge_values / sizeof edge_values[0];
                         edge++) {
                        uint64_t wide_value = edge_values[edge];
                        char formatted[128];

                        retail_format(&wide_value, id, formatted);
                        failures += test_parse_one(retail_parse, formatted,
                                                   id, parse_case++);
                        if (failures >= 20)
                            break;
                    }
                    for (edge = 0; failures < 20 &&
                         edge < sizeof parse_cases /
                                  sizeof parse_cases[0]; edge++)
                        failures += test_parse_one(retail_parse,
                            parse_cases[edge], id, parse_case++);
                    for (edge = 0; failures < 20 && edge < 8; edge++) {
                        const void *range = (edge & 1) ?
                            (const void *)pattern_range :
                            (const void *)zero_range;
                        const char *text = parse_cases[edge / 2];

                        failures += test_parse_range_one(
                            retail_parse_range, text, id, range,
                            parse_case++);
                    }
                }
            }
        }
        printf("numeric parser: %lu comparisons, %d mismatches\n",
               (unsigned long)parse_vector, failures);
    }
    FreeLibrary(retail_module);
    return failures != 0;
}
