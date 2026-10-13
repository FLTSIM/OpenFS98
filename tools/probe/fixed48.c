/* Probe retail UTIL's 32.16 fixed-point conversions with chosen values, to see
 * what they actually do rather than what the listing appears to say. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef void (__attribute__((stdcall)) *fn_df48)(const double *, void *);
typedef void (__attribute__((stdcall)) *fn_f48d)(double *, const void *);

int main(void)
{
    HMODULE h = LoadLibraryA("UTIL_retail.dll");
    unsigned char *link;
    fn_df48 to48; fn_f48d from48;
    static const double v[] = { 1.5, -1.5, 0.25, -0.25, 3.75, -3.75,
                                100.0, -100.0, -248804.45679874322 };
    unsigned i;

    if (!h) { printf("no dll\n"); return 2; }
    link = (unsigned char *)GetProcAddress(h, "Linkage");
    to48   = (fn_df48)*(void **)(link + 0xbc);
    from48 = (fn_f48d)*(void **)(link + 0xc0);

    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        unsigned char b[8]; double back = 0;
        memset(b, 0, 8);
        to48(&v[i], b);
        from48(&back, b);
        printf("%20.8f -> hi=%08x lo=%04x  -> back %20.8f\n",
               v[i], *(uint32_t *)(b + 4), *(uint16_t *)(b + 2), back);
    }
    return 0;
}
