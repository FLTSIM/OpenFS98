#ifndef FS98_NUMERIC_H
#define FS98_NUMERIC_H

#include <stdint.h>
#include <fs98/linkage.h>

#ifndef FS98_STDCALL
#  define FS98_STDCALL __attribute__((stdcall))
#endif

char *FS98_STDCALL fs98_numeric_format(const void *value, uint32_t format,
                                       char *output);
int32_t FS98_STDCALL fs98_numeric_parse(const char *text, uint32_t format,
                                      void *output);
int32_t FS98_STDCALL fs98_numeric_parse_internal(const char *text,
                                               uint32_t format,
                                               void *output,
                                               const void *range);
void FS98_STDCALL fs98_numeric_init(void);
void FS98_STDCALL fs98_numeric_deinit(void);

#ifndef FS98_NUMERIC_MODULE
void fs98_numeric_set_host(void *host);
void fs98_numeric_set_global(void *global);
void fs98_numeric_set_util(void *util);
#endif

#endif /* FS98_NUMERIC_H */
