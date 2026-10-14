/* UTIL.DLL's file-listing services: slots 0x94, 0x98 and 0xa0.
 *
 * None of these touches the file system directly.  Every step goes through the
 * host's enumeration cluster -- find-first/next/close at +0x164/+0x160/+0x15c,
 * path join and split at +0x184/+0x188, the growable array at +0x2f8/+0x300,
 * qsort and _strcmpi at +0x22c/+0x2c0 -- so these are thin, and they can only
 * be verified in-process, through ofstest, against the real host.
 *
 * The prototypes they lean on, and what each host service does, are in
 * include/fs98/host.h.
 */

#include <stdint.h>

#include <fs98/host.h>

#include "util_imports.h"

const char * FS98_STDCALL fs98_util_longer_string(const char *a, const char *b);

#define H FS98_UTIL_HOST

/* ---- slot 0x94: every file under a directory, recursively ----------------
 *
 * Collects  <dir>\<name>  plus its size for each match of pattern+ext in
 * `dir`, then recurses into every subdirectory whose name does not start with
 * a dot.  Matches come before the subdirectories' contents.  The paths have
 * NO EXTENSION -- +0x160 strips it -- and, unlike +0x98, nothing records what
 * it was, so with ext -1 "a.BGL" and "a.TXT" come back as the same path.
 *
 * Each 0x108-byte record is a 0x104-byte path and the uint32 file size (0 for
 * a directory that matched).  The array is built in a static list and handed
 * to the caller whole, who frees it through the host; the list is then reset.
 *
 * One entry buffer serves both loops, as in retail.  The second loop decides
 * what to recurse into by `type`, and a file with an extension the host has
 * not registered leaves `type` as it was -- inheriting the previous entry's,
 * or in retail's very first iteration, stack garbage.  Here it starts at 0.
 * That only matters if the garbage is exactly -2, and then it costs a
 * recursion into a file, which finds nothing.
 */
static struct fs98_host_list fs98_util_list94 = { 0, 0, 0x20, 0x108, 0, 0, 0 };

static void FS98_STDCALL fs98_util_walk(const char *dir, const char *pattern,
                                        int32_t ext)
{
    char *sub;
    struct fs98_host_find *h;
    struct fs98_host_find_entry e;

    e.type = 0;
    FS98_HOST_ALLOC(H)(1, 0x105, (void **)&sub, 1);

    if (FS98_HOST_FIND_FIRST(H)(&h, dir, pattern, ext) != -1) {
        while (FS98_HOST_FIND_NEXT(H)(h, &e) == 0) {
            char *rec = FS98_HOST_LIST_ADD(H)(&fs98_util_list94);
            FS98_HOST_PATH_JOIN(H)(rec, 0, dir, e.name);
            *(uint32_t *)(rec + 0x104) = e.size;
        }
        FS98_HOST_FIND_CLOSE(H)(&h);
    }

    if (FS98_HOST_FIND_FIRST(H)(&h, dir, "*", FS98_HOST_EXT_ANY) != -1) {
        while (FS98_HOST_FIND_NEXT(H)(h, &e) == 0) {
            if (e.type == FS98_HOST_TYPE_DIR && e.name[0] != '.') {
                FS98_HOST_PATH_JOIN(H)(sub, 0, dir, e.name);
                fs98_util_walk(sub, pattern, ext);
            }
        }
        FS98_HOST_FIND_CLOSE(H)(&h);
    }

    FS98_HOST_FREE(H)((void **)&sub);
}

int32_t FS98_STDCALL fs98_util_list_tree(void **data, int32_t *count,
                                         const char *dir, const char *pattern,
                                         int32_t ext)
{
    fs98_util_walk(dir, pattern, ext);

    *data  = fs98_util_list94.data;
    *count = fs98_util_list94.count;

    fs98_util_list94.count     = 0;
    fs98_util_list94.capacity  = 0;
    fs98_util_list94.grow      = 0x20;
    fs98_util_list94.elem_size = 0x108;
    fs98_util_list94.iterating = 0;
    fs98_util_list94.pending   = 0;
    fs98_util_list94.data      = 0;
    return 0;
}

/* ---- slot 0xa0: is this path a directory? ---------------------------------
 *
 * Splits the path, rejoins drive and directory, lists that, and answers 1 if
 * some entry is a directory whose name matches the last component,
 * case-insensitively.  It does not stop at the first match.
 *
 * A path with no directory part -- "FLIGHTS" rather than ".\FLIGHTS" --
 * rejoins to "", and +0x164 turns "" into the root of the current drive (see
 * host.h).  So retail answers that question about C:\, not about the current
 * directory.  Reproduced by construction, since the same host does the work.
 *
 * The type-is-stale caveat from slot 0x94 applies too: a file with an
 * unregistered extension directly after a directory of the same name counts.
 */
int32_t FS98_STDCALL fs98_util_is_dir(const char *path)
{
    char name[0x104] = "";
    char joined[0x108] = "";
    char dir[0x108] = "";
    char drive[0x104] = "";
    struct fs98_host_find *h;
    struct fs98_host_find_entry e;
    int32_t found = 0;

    e.type = 0;
    if (FS98_HOST_PATH_SPLIT(H)(path, drive, dir, name, 0) != 0)
        return 0;
    if (FS98_HOST_PATH_JOIN(H)(joined, drive, dir, 0) != 0)
        return 0;

    if (FS98_HOST_FIND_FIRST(H)(&h, joined, "*", 0) != -1) {
        while (FS98_HOST_FIND_NEXT(H)(h, &e) == 0) {
            if (e.type == FS98_HOST_TYPE_DIR && e.name[0] != '.'
                && FS98_HOST_STRICMP(H)(e.name, name) == 0)
                found = 1;
        }
        FS98_HOST_FIND_CLOSE(H)(&h);
    }
    return found;
}

/* ---- slot 0x98: a catalogue from two directories --------------------------
 *
 * What the flight, lesson and video pickers are built on.  MAIN calls it as
 * (0x9004, 0x9002, ext 2 = .STN, ..., sort 1): the stock flights, then the
 * user's own.  Both directories are named by host string-resource ids.
 *
 * Each 0xa08-byte record:
 *
 *   +0x000  0x100  display name: the longer of the file name and the title
 *                  the name callback wrote, ties going to the title
 *   +0x100  0x400  filled by the second callback
 *   +0x500  0x400  filled by the third
 *   +0x900  0x104  path, without the extension
 *   +0xa04  int32  0 for the first directory; for the second, the entry's
 *                  writable flag, so a user's file can be deleted and a
 *                  read-only stock one cannot
 *
 * The callbacks are stdcall (path, out).  The two directories are NOT treated
 * alike, and that is reproduced:
 *
 *   first   any callback returning 0 drops the record
 *   second  return values are ignored; the record is dropped only if the name
 *           callback leaves the title empty
 *
 * Returns 1 with the array handed over, or 0 if nothing survived.  On that
 * path the static list is NOT reset -- its buffer stays allocated, at count 0,
 * and the next call appends into it.  `sort` 1 orders by display name, 2 by
 * path, anything else not at all.
 */
typedef int32_t (FS98_STDCALL *fs98_util_entry_cb)(const char *path, char *out);

static struct fs98_host_list fs98_util_list98 = { 0, 0, 0x20, 0xa08, 0, 0, 0 };

static int32_t FS98_STDCALL fs98_util_cmp_name(const void *a, const void *b)
{
    return FS98_HOST_STRICMP(H)((const char *)a, (const char *)b);
}

static int32_t FS98_STDCALL fs98_util_cmp_path(const void *a, const void *b)
{
    return FS98_HOST_STRICMP(H)((const char *)a + 0x900, (const char *)b + 0x900);
}

int32_t FS98_STDCALL fs98_util_catalogue(uint32_t stock_id, uint32_t user_id,
                                         int32_t ext, int32_t *count,
                                         void **data,
                                         fs98_util_entry_cb name_cb,
                                         fs98_util_entry_cb cb100,
                                         fs98_util_entry_cb cb500,
                                         int32_t sort)
{
    struct fs98_host_find *h;
    struct fs98_host_find_entry e;
    char title[0x100];
    char dir[0x100];
    char *rec, *path;

    e.writable = 0;
    *data  = 0;
    *count = 0;

    if (stock_id != 0) {
        FS98_HOST_LOADSTRING(H)(stock_id, dir, 0x100);
        if (FS98_HOST_FIND_FIRST(H)(&h, dir, 0, ext) == 0) {
            while (FS98_HOST_FIND_NEXT(H)(h, &e) == 0) {
                rec = FS98_HOST_LIST_ADD(H)(&fs98_util_list98);
                *(int32_t *)(rec + 0xa04) = 0;
                path = rec + 0x900;
                FS98_HOST_PATH_JOIN(H)(path, 0, dir, e.name);
                title[0] = 0;
                if (name_cb && name_cb(path, title) == 0) {
                    FS98_HOST_LIST_REMOVE(H)(&fs98_util_list98, rec);
                    continue;
                }
                FS98_HOST_STRCPY(H)(rec, fs98_util_longer_string(e.name, title));
                if ((cb100 && cb100(path, rec + 0x100) == 0)
                    || (cb500 && cb500(path, rec + 0x500) == 0))
                    FS98_HOST_LIST_REMOVE(H)(&fs98_util_list98, rec);
            }
            FS98_HOST_FIND_CLOSE(H)(&h);
        }
    }

    if (user_id != 0) {
        FS98_HOST_LOADSTRING(H)(user_id, dir, 0x100);
        if (FS98_HOST_FIND_FIRST(H)(&h, dir, 0, ext) == 0) {
            while (FS98_HOST_FIND_NEXT(H)(h, &e) == 0) {
                rec = FS98_HOST_LIST_ADD(H)(&fs98_util_list98);
                *(int32_t *)(rec + 0xa04) = e.writable;
                path = rec + 0x900;
                FS98_HOST_PATH_JOIN(H)(path, 0, dir, e.name);
                title[0] = 0;
                if (name_cb) {
                    name_cb(path, title);
                    if (title[0] == 0) {
                        FS98_HOST_LIST_REMOVE(H)(&fs98_util_list98, rec);
                        continue;
                    }
                }
                FS98_HOST_STRCPY(H)(rec, fs98_util_longer_string(e.name, title));
                if (cb100)
                    cb100(path, rec + 0x100);
                if (cb500)
                    cb500(path, rec + 0x500);
            }
            FS98_HOST_FIND_CLOSE(H)(&h);
        }
    }

    if (fs98_util_list98.count == 0)
        return 0;

    *count = fs98_util_list98.count;
    *data  = fs98_util_list98.data;

    fs98_util_list98.count     = 0;
    fs98_util_list98.capacity  = 0;
    fs98_util_list98.grow      = 0x20;
    fs98_util_list98.elem_size = 0xa08;
    fs98_util_list98.iterating = 0;
    fs98_util_list98.pending   = 0;
    fs98_util_list98.data      = 0;

    if (sort == 1)
        FS98_HOST_QSORT(H)(*data, *count, 0xa08, fs98_util_cmp_name);
    else if (sort == 2)
        FS98_HOST_QSORT(H)(*data, *count, 0xa08, fs98_util_cmp_path);
    return 1;
}
