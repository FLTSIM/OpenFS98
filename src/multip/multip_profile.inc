/* Profile round-trip, .text+0x8f30 and +0x93e0. Included by multip.c. */
struct multip_profile_field {
    uint32_t key;
    uint32_t value;
    uint32_t string_size;
    uint32_t validation;
};

#include "multip_profile_fields.inc"

typedef int32_t (FS98_STDCALL *multip_profile_read_int_fn)(const char *, const char *, uint32_t *);
typedef uint32_t (FS98_STDCALL *multip_profile_read_string_fn)(const char *, const char *, char *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_profile_write_int_fn)(const char *, const char *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_profile_write_string_fn)(const char *, const char *, const char *);

static void multip_profile_validate(const struct multip_profile_field *field)
{
    uint32_t value = M32(field->value);
    uint64_t input;
    double result;

    switch (field->validation) {
    case 1:
        if (value > 255) value = 255;
        if (value == 0) value = 8;
        break;
    case 2:
        if (value > 255) value = 255;
        break;
    case 3:
        if (value == 0 || value > 100) value = 18;
        input = value;
        __asm__ volatile("fildll %1\n\tfmull %2\n\tfstpl %0"
                         : "=m"(result) : "m"(input),
                           "m"(*(double *)(void *)(s_multip_rdata + 0x1b0)) : "st");
        MD(0x158) = result;
        break;
    case 4:
        if (value < 1) value = 1;
        break;
    case 5:
        if (value < 2) value = 2;
        break;
    case 6:
        if (value < 5 || value > 30) value = 5;
        break;
    case 7:
        if (value == 0) value = 100;
        break;
    case 8:
        if (value == 0 || value > 10) value = 3;
        break;
    case 9:
        if (value < 5 || value > 45) value = 15;
        break;
    case 10:
        /* Retail reads this key into +0x174, but validates +0x178. */
        if (M32(0x178) < 5 || M32(0x178) > 45) M32(0x178) = 15;
        break;
    case 11:
        if (value < 5000) value = 5000;
        break;
    }
    M32(field->value) = value;
}

int32_t multip_profile_load(void)
{
    uint32_t index;
    const struct multip_profile_field *field;
    const char *section = (const char *)(uintptr_t)(D + 0x198);

    for (index = 0; index < sizeof(s_profile_load_fields) /
                                 sizeof(s_profile_load_fields[0]); index++) {
        field = &s_profile_load_fields[index];
        if (field->string_size != 0)
            HOST_FN(0xc8, multip_profile_read_string_fn)(section,
                (const char *)(uintptr_t)(D + field->key),
                (char *)(uintptr_t)(D + field->value), field->string_size);
        else {
            HOST_FN(0xa8, multip_profile_read_int_fn)(section,
                (const char *)(uintptr_t)(D + field->key),
                (uint32_t *)(uintptr_t)(D + field->value));
            multip_profile_validate(field);
        }
    }
    return 0;
}

int32_t multip_profile_save(void)
{
    uint32_t index;
    const struct multip_profile_field *field;
    const char *section = (const char *)(uintptr_t)(D + 0x198);

    for (index = 0; index < sizeof(s_profile_save_fields) /
                                 sizeof(s_profile_save_fields[0]); index++) {
        field = &s_profile_save_fields[index];
        if (field->string_size != 0)
            HOST_FN(0xcc, multip_profile_write_string_fn)(section,
                (const char *)(uintptr_t)(D + field->key),
                (const char *)(uintptr_t)(D + field->value));
        else
            HOST_FN(0xac, multip_profile_write_int_fn)(section,
                (const char *)(uintptr_t)(D + field->key), M32(field->value));
    }
    return 0;
}
