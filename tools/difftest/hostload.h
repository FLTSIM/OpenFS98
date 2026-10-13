/* Map FLTSIM98.EXE at its own base address so its absolute addresses hold.
 *
 * The host has no .reloc section, so every address inside it -- the service
 * table, the globals its accessors read, the pointers between its functions --
 * is absolute and only correct at 0x400000.  LoadLibraryEx will happily map it
 * somewhere else, at which point the service pointers are wrong and, worse,
 * the code reads globals from whatever happens to live at the old addresses.
 *
 * So it is mapped by hand: reserve the exact base, copy the headers and the
 * sections where they belong, and stop.  Imports are deliberately not
 * resolved, which is safe for services that call none and an immediate fault
 * for services that do -- a property worth having, since it makes "this
 * service is self-contained" something the harness enforces rather than
 * assumes.
 */
#ifndef FS98_HOSTLOAD_H
#define FS98_HOSTLOAD_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char *fs98_map_host(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf, *image;
    uint32_t e, nsec, optsz, base, imgsize, hdrsize;
    long len;
    unsigned i;

    if (!f) { printf("cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)len);
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); return NULL; }
    fclose(f);

    e       = *(uint32_t *)(buf + 0x3c);
    nsec    = *(uint16_t *)(buf + e + 6);
    optsz   = *(uint16_t *)(buf + e + 20);
    base    = *(uint32_t *)(buf + e + 24 + 28);
    imgsize = *(uint32_t *)(buf + e + 24 + 56);
    hdrsize = *(uint32_t *)(buf + e + 24 + 60);

    image = (unsigned char *)VirtualAlloc((void *)(uintptr_t)base, imgsize,
                                          MEM_RESERVE | MEM_COMMIT,
                                          PAGE_EXECUTE_READWRITE);
    if (image != (unsigned char *)(uintptr_t)base) {
        printf("could not reserve 0x%08x (got %p)\n", base, (void *)image);
        free(buf);
        return NULL;
    }
    memcpy(image, buf, hdrsize);
    for (i = 0; i < nsec; i++) {
        unsigned char *sh = buf + e + 24 + optsz + i * 40;
        uint32_t va  = *(uint32_t *)(sh + 12);
        uint32_t rsz = *(uint32_t *)(sh + 16);
        uint32_t ro  = *(uint32_t *)(sh + 20);
        if (rsz) memcpy(image + va, buf + ro, rsz);
    }
    free(buf);
    return image;
}

#endif
