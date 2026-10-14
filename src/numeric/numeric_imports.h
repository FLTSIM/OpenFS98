#ifndef FS98_NUMERIC_IMPORTS_H
#define FS98_NUMERIC_IMPORTS_H

#ifdef FS98_NUMERIC_MODULE
#  include <fs98/linkage.h>
extern struct fs98_import ImportTable[];
#  define FS98_NUMERIC_HOST   ((void *)ImportTable[0].link) /* module 1  */
#  define FS98_NUMERIC_GLOBAL ((void *)ImportTable[1].link) /* module 2  */
#  define FS98_NUMERIC_UTIL   ((void *)ImportTable[2].link) /* module 20 */
#else
extern void *fs98_numeric_host;
extern void *fs98_numeric_global;
extern void *fs98_numeric_util;
#  define FS98_NUMERIC_HOST   fs98_numeric_host
#  define FS98_NUMERIC_GLOBAL fs98_numeric_global
#  define FS98_NUMERIC_UTIL   fs98_numeric_util
#endif

#endif /* FS98_NUMERIC_IMPORTS_H */
