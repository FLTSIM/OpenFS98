/* The display settings the runtime reads from the HARDWARE section of the configuration
 * (the service +0x31c reads and writes them for the options dialogs: src/host/host.c, "display
 * settings", whose block layout this shares). */
#include "host_rt.h"

#define HARDWARE STR(0x416120)
#define CONFIGURATION STR(0x41c120)    /* the configuration file's path */

/* 0x4144c0: an option flag from the HARDWARE section, set or cleared in the settings. */
void SC fs98_host_rt_flag_read(uint8_t *settings, uint32_t mask, const char *key, int32_t fallback)
{
    if (CALL_STD(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
            HARDWARE, key, fallback, CONFIGURATION) != 0)
        D32(settings, 0xd0) |= mask;
    else
        D32(settings, 0xd0) &= ~mask;
}

/* 0x414400: DirectDrawEnumerate's callback: the primary display's (no GUID) name as the
 * device; enumeration stops there. */
int32_t SC fs98_host_rt_primary_found(void *guid, char *description, char *driver, uint8_t *settings)
{
    if (guid != 0)
        return 1;
    D8(settings, 0x80) = 0;
    fs98_host_rt_device_name((char *)settings, description, driver, (char *)AT(settings, 0x80));
    return 0;
}

/* a number in the mode ("640x480x16"), and the text past the 'x' after it */
static int32_t mode_number(const char **text)
{
    int32_t value = CALL_C(atoi, int32_t, (const char *))(*text);
    while (**text != 0 && **text != 'x')
        ++*text;
    if (**text != 0)
        ++*text;
    return value;
}

/* 0x414510: the display settings (0xe8 bytes): from the HARDWARE section, or with defaults
 * the option flags' defaults alone (the table at 0x418458: mask, key, default). With no
 * device and DirectDraw allowed (flag 1 clear), the primary display's. The 3D suffix split
 * off into +0x80, the mode "WxHxB[xN]" parsed into +0xd4..+0xe0; without a width, height or
 * depth, 640x480 at 16 bits (with a 3D suffix) or 8. Returns 0. */
int32_t SC fs98_host_rt_read_display(uint8_t *settings, int32_t defaults)
{
    const char *mode;
    uint32_t at;
    for (at = 0; at < 0x3a; ++at)
        D32(settings, at * 4) = 0;
    if (defaults) {
        D32(settings, 0xd0) = 0;
        if (G32(0x418458) != 0) {
            uint32_t entry = 0x418458;
            do {
                if (G32(entry + 8) != 0)
                    D32(settings, 0xd0) |= G32(entry);
                entry += 0xc;
            } while (G32(entry) != 0);
        }
    } else {
        int32_t (SC *get_string)(const char *, const char *, const char *, char *, uint32_t, const char *) =
            (int32_t (SC *)(const char *, const char *, const char *, char *, uint32_t, const char *))
                fs98_host_rt_imports[HOST_RT_IMPORT_GetPrivateProfileStringA];
        get_string(HARDWARE, STR(0x418594), STR(0x4187f8), (char *)settings, 0x80, CONFIGURATION);
        get_string(HARDWARE, STR(0x41858c), STR(0x4187f8), (char *)AT(settings, 0xa8), 0x28, CONFIGURATION);
        if (G32(0x418458) != 0) {
            uint32_t entry = 0x41845c, mask = G32(0x418458);
            do {
                fs98_host_rt_flag_read(settings, mask, (const char *)G_PTR(entry), GI32(entry + 4));
                mask = G32(entry + 8);
                entry += 0xc;
            } while (mask != 0);
        }
    }
    if (D8(settings, 0) == 0 && !(D8(settings, 0xd0) & 1))
        CALL_STD(DirectDrawEnumerateA, int32_t, (void *, void *))((void *)fs98_host_rt_primary_found, settings);
    fs98_host_rt_device_suffix((char *)settings, (char *)AT(settings, 0x80), 0);
    mode = (const char *)AT(settings, 0xa8);
    D32(settings, 0xd4) = (uint32_t)mode_number(&mode);
    D32(settings, 0xd8) = (uint32_t)mode_number(&mode);
    D32(settings, 0xdc) = (uint32_t)mode_number(&mode);
    D32(settings, 0xe0) = (uint32_t)CALL_C(atoi, int32_t, (const char *))(mode);
    if (D32(settings, 0xd4) != 0 && D32(settings, 0xd8) != 0 && D32(settings, 0xdc) != 0)
        return 0;
    D32(settings, 0xd4) = 0x280;
    D32(settings, 0xd8) = 0x1e0;
    D32(settings, 0xdc) = D8(settings, 0x80) != 0 ? 0x10 : 8;
    D32(settings, 0xe0) = 0;
    CALL_STD(lstrcpyA, char *, (char *, const char *))((char *)AT(settings, 0xa8),
                                                       STR(D8(settings, 0x80) != 0 ? 0x418580 : 0x418574));
    return 0;
}
