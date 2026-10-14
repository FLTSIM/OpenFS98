/* How UTIL's services reach the three modules it imports.
 *
 * Retail reads its ImportTable slot on every call -- `mov eax, ds:<slot>` /
 * `call [eax+N]` -- so a service works the moment the loader has bound the
 * table, whether or not UTIL's own init has run yet.  The module build does
 * the same: FS98_UTIL_MODULE makes these read ImportTable directly.
 *
 * The differential harnesses have no ImportTable of ours to bind, so without
 * FS98_UTIL_MODULE the same names are plain pointers the harness sets with
 * fs98_util_set_host / _set_global / _set_numeric.
 */
#ifndef FS98_UTIL_IMPORTS_H
#define FS98_UTIL_IMPORTS_H

#ifdef FS98_UTIL_MODULE
#  include <fs98/linkage.h>
extern struct fs98_import ImportTable[];
#  define FS98_UTIL_HOST    ((void *)ImportTable[0].link)   /* module 1  */
#  define FS98_UTIL_GLOBAL  ((void *)ImportTable[1].link)   /* module 2  */
#  define FS98_UTIL_NUMERIC ((void *)ImportTable[2].link)   /* module 17 */
#else
extern void *fs98_util_host;
extern void *fs98_util_global;
extern void *fs98_util_numeric;
#  define FS98_UTIL_HOST    fs98_util_host
#  define FS98_UTIL_GLOBAL  fs98_util_global
#  define FS98_UTIL_NUMERIC fs98_util_numeric
#endif

#endif /* FS98_UTIL_IMPORTS_H */
