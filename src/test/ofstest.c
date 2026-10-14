/* OFSTEST.DLL -- a test module that runs inside the sim.
 *
 * The host cannot be loaded beside an external test program: it has no .reloc,
 * so it only works at 0x400000, and that address is already committed in any
 * wine process.  So services that call the host have been unverifiable.
 *
 * The way round it is the mechanism the sim already provides.  Every DLL in
 * Modules\ is loaded by the host, assigned its module id, and handed pointers
 * to the modules it imports -- including the host itself.  A module whose only
 * job is to run assertions therefore gets exactly the access a test needs, from
 * the real loader, inside the real process.
 *
 * It takes module id 5, which the retail set leaves unused, imports the host,
 * GLOBAL and UTIL, and writes its results to a file because there is nowhere
 * else for output to go.  Everything here is deliberately defensive: this code
 * runs during the sim's startup, and a fault in it means the sim does not
 * start.
 */

#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define OFSTEST_LOG "C:\\openfs98-test.log"

/* ---- the services under test, from the reimplementation ------------------- */
const char * FS98_STDCALL fs98_util_longer_string(const char *, const char *);
void                      fs98_util_set_host(void *);
void                      fs98_util_rand_seed(uint32_t);
uint32_t                  fs98_util_rand_get(void);
void                      fs98_util_frand_seed(double);
double                    fs98_util_frand_get(void);
void     FS98_STDCALL     fs98_util_init(void);

typedef int32_t (FS98_STDCALL *entry_cb)(const char *, char *);
int32_t FS98_STDCALL fs98_util_list_tree(void **, int32_t *, const char *,
                                         const char *, int32_t);
int32_t FS98_STDCALL fs98_util_is_dir(const char *);
int32_t FS98_STDCALL fs98_util_catalogue(uint32_t, uint32_t, int32_t, int32_t *,
                                         void **, entry_cb, entry_cb, entry_cb,
                                         int32_t);

/* ---- the host services this module reaches for ---------------------------- */

struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,   0 },
    { FS98_MOD_GLOBAL, 0 },
    { FS98_MOD_UTIL,   0 },
    { 0, 0 }
};

/* ---- a log, written without any C runtime --------------------------------- */

static HANDLE log_handle = INVALID_HANDLE_VALUE;
static unsigned pass_count, fail_count;

static void emit(const char *s)
{
    DWORD wrote;
    unsigned n = 0;
    if (log_handle == INVALID_HANDLE_VALUE)
        return;
    while (s[n])
        n++;
    WriteFile(log_handle, s, n, &wrote, 0);
}

static void emit_u(uint32_t v)
{
    char buf[12];
    int i = 11;
    buf[11] = 0;
    if (!v) { emit("0"); return; }
    while (v && i > 0) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
    emit(buf + i);
}

static void emit_x(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = hex[(v >> (28 - i * 4)) & 0xf];
    buf[10] = 0;
    emit(buf);
}

static void check(int ok, const char *what)
{
    emit(ok ? "  pass  " : "  FAIL  ");
    emit(what);
    emit("\r\n");
    if (ok) pass_count++; else fail_count++;
}

/* ---- no C runtime, so the few string helpers the tests need --------------- */

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_has(const char *s, const char *needle)
{
    for (; *s; s++) {
        const char *a = s, *b = needle;
        while (*b && *a == *b) { a++; b++; }
        if (!*b)
            return 1;
    }
    return 0;
}

static char *str_put(char *d, const char *s)
{
    while ((*d = *s) != 0) { d++; s++; }
    return d;
}

static int mem_eq(const void *a, const void *b, uint32_t n)
{
    const unsigned char *x = a, *y = b;
    while (n--)
        if (*x++ != *y++)
            return 0;
    return 1;
}

/* ---- a scratch directory tree --------------------------------------------- */

/* Built before the enumeration tests and torn down after.  Every directory
 * holds at least one .BGL so that slot 0x94's first loop always sets the
 * shared entry buffer before its second loop reads it -- otherwise retail
 * would be reading stack garbage, which no reimplementation can match. */
struct tree_node { const char *path; uint32_t size; int dir, ro; };

static const struct tree_node tree[] = {
    { "C:\\openfs98-enum",                       0, 1, 0 },
    { "C:\\openfs98-enum\\sub1",                 0, 1, 0 },
    { "C:\\openfs98-enum\\sub1\\deep",           0, 1, 0 },
    { "C:\\openfs98-enum\\.hidden",              0, 1, 0 },
    { "C:\\openfs98-enum\\dir.BGL",              0, 1, 0 },
    { "C:\\openfs98-enum\\Flights",              0, 1, 0 },
    { "C:\\openfs98-enum\\a.BGL",               10, 0, 0 },
    { "C:\\openfs98-enum\\b.bgl",               20, 0, 0 },
    { "C:\\openfs98-enum\\ro.BGL",              30, 0, 1 },
    { "C:\\openfs98-enum\\c.txt",                5, 0, 0 },
    { "C:\\openfs98-enum\\README",               7, 0, 0 },
    { "C:\\openfs98-enum\\Flights\\fl.BGL",      1, 0, 0 },
    { "C:\\openfs98-enum\\sub1\\d.BGL",         40, 0, 0 },
    { "C:\\openfs98-enum\\sub1\\e.STN",         50, 0, 0 },
    { "C:\\openfs98-enum\\sub1\\deep\\f.BGL",   60, 0, 0 },
    { "C:\\openfs98-enum\\.hidden\\g.BGL",      70, 0, 0 },
    { "C:\\openfs98-enum\\dir.BGL\\x.BGL",      80, 0, 0 },

    /* slot 0x98 names its directories by string id, and the ids resolve to
     * relative paths -- 0x9004 SITUATIO, 0x9002 PILOTS -- so a catalogue tree
     * is just those two names under a directory the test changes into. */
    { "C:\\openfs98-cat",                        0, 1, 0 },
    { "C:\\openfs98-cat\\SITUATIO",              0, 1, 0 },
    { "C:\\openfs98-cat\\PILOTS",                0, 1, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\plain.STN",   1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\long.STN",    1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\empty.STN",   1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\kill.STN",    1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\veto.STN",    1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\nodesc.STN",  1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\nobody.STN",  1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\Zed.STN",     1, 0, 0 },
    { "C:\\openfs98-cat\\SITUATIO\\other.BGL",   1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\mine.STN",      1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\ro.STN",        1, 0, 1 },
    { "C:\\openfs98-cat\\PILOTS\\empty.STN",     1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\kill.STN",      1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\veto.STN",      1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\nodesc.STN",    1, 0, 0 },
    { "C:\\openfs98-cat\\PILOTS\\long.STN",      1, 0, 0 },
};
#define TREE_N (sizeof tree / sizeof tree[0])

static void tree_teardown(void)
{
    int i;
    for (i = (int)TREE_N - 1; i >= 0; i--) {
        SetFileAttributesA(tree[i].path, FILE_ATTRIBUTE_NORMAL);
        if (tree[i].dir)
            RemoveDirectoryA(tree[i].path);
        else
            DeleteFileA(tree[i].path);
    }
}

static int tree_build(void)
{
    static const char filler[128];
    unsigned i;
    tree_teardown();
    for (i = 0; i < TREE_N; i++) {
        if (tree[i].dir) {
            if (!CreateDirectoryA(tree[i].path, 0))
                return 0;
        } else {
            DWORD wrote;
            HANDLE f = CreateFileA(tree[i].path, GENERIC_WRITE, 0, 0,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if (f == INVALID_HANDLE_VALUE)
                return 0;
            WriteFile(f, filler, tree[i].size, &wrote, 0);
            CloseHandle(f);
            if (tree[i].ro)
                SetFileAttributesA(tree[i].path, FILE_ATTRIBUTE_READONLY);
        }
    }
    return 1;
}

/* ---- callbacks for slot 0x98, pure functions of the path ------------------ */

static unsigned cb_calls;

/* "kill" refuses and leaves no title; "veto" refuses but writes one -- the
 * pair that tells the first directory's handling from the second's. */
static int32_t FS98_STDCALL cb_name(const char *path, char *title)
{
    cb_calls++;
    if (str_has(path, "kill"))  return 0;
    if (str_has(path, "veto"))  { str_put(title, "vetoed"); return 0; }
    if (str_has(path, "empty")) return 1;
    if (str_has(path, "long"))  { str_put(title, "A much longer display title"); return 1; }
    str_put(title, "t");
    return 1;
}

static int32_t FS98_STDCALL cb_desc(const char *path, char *out)
{
    cb_calls++;
    str_put(str_put(out, "desc of "), path);
    return !str_has(path, "nodesc");
}

static int32_t FS98_STDCALL cb_body(const char *path, char *out)
{
    cb_calls++;
    str_put(str_put(out, "body of "), path);
    return !str_has(path, "nobody");
}

static int32_t FS98_STDCALL cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

/* ---- the host's enumeration cluster, pinned directly ---------------------- */

static void test_host_files(void *host)
{
    char buf[0x200], drive[0x104], dir[0x108], name[0x104];
    int32_t id, id2, r;

    /* +0x2c0 _strcmpi */
    {
        fs98_host_stricmp_fn f = FS98_HOST_STRICMP(host);
        check(f("Flights", "FLIGHTS") == 0, "host stricmp ignores case");
        check(f("abc", "abd") < 0 && f("abd", "abc") > 0, "host stricmp orders");
    }

    /* +0x22c qsort, through the stdcall trampoline */
    {
        uint32_t v[7] = { 5, 3, 9, 1, 7, 3, 0 };
        FS98_HOST_QSORT(host)(v, 7, 4, cmp_u32);
        check(v[0] == 0 && v[1] == 1 && v[2] == 3 && v[3] == 3 && v[4] == 5
              && v[5] == 7 && v[6] == 9, "host qsort sorts with a stdcall comparator");
    }

    /* +0x2f8 / +0x300 the growable array */
    {
        struct fs98_host_list l = { 0, 0, 4, 8, 0, 0, 0 };
        uint32_t *e, i, zeroed = 1;
        for (i = 0; i < 6; i++) {
            e = FS98_HOST_LIST_ADD(host)(&l);
            if (e[0] || e[1]) zeroed = 0;
            e[0] = i; e[1] = ~i;
        }
        check(l.count == 6 && l.capacity == 8, "host list grows by its step: 6 in 8");
        check(zeroed, "host list hands out zeroed elements");
        e = l.data;
        FS98_HOST_LIST_REMOVE(host)(&l, &e[2 * 2]);
        check(l.count == 5 && e[4] == 3 && e[5] == ~3u && e[8] == 5,
              "host list remove closes the gap");
        FS98_HOST_LIST_REMOVE(host)(&l, &e[4 * 2]);
        check(l.count == 4 && e[6] == 4, "host list remove of the last just shrinks");
        FS98_HOST_FREE(host)(&l.data);
        check(l.data == 0, "host free nulls the list's buffer");
    }

    /* +0x184 path join */
    {
        fs98_host_path_join_fn j = FS98_HOST_PATH_JOIN(host);
        r = j(buf, "C", "\\x", "y");
        check(r == 0 && str_eq(buf, "C:\\x\\y"), "host join: a one-letter drive gets a colon");
        r = j(buf, "C:\\", "x\\", "y");
        check(r == 0 && str_eq(buf, "C:\\x\\y"), "host join: separators added and stripped");
        r = j(buf, 0, "a", "z\\");
        check(r == 0 && str_eq(buf, "a\\z\\"), "host join: the name keeps its trailing slash");
        r = j(buf, 0, 0, 0);
        check(r == 0 && buf[0] == 0, "host join of nothing is empty");
        {
            char big[300];
            unsigned i;
            for (i = 0; i < 299; i++) big[i] = 'q';
            big[299] = 0;
            buf[0] = 'X';
            r = j(buf, 0, big, 0);
            check(r == -1 && buf[0] == 0, "host join: too long clears out and fails");
        }
        check(j(0, "C", 0, 0) == -1, "host join: null out fails");
    }

    /* +0x188 path split */
    {
        fs98_host_path_split_fn sp = FS98_HOST_PATH_SPLIT(host);
        id = 12345;
        r = sp("C:\\a\\b.BGL", drive, dir, name, &id);
        check(r == 0 && str_eq(drive, "C:") && str_eq(dir, "\\a\\") && str_eq(name, "b")
              && id == 1, "host split: drive, dir, name, and .BGL as id 1");
        r = sp("\\\\srv\\share\\f.stn", drive, dir, name, &id);
        check(r == 0 && str_eq(drive, "\\\\srv") && str_eq(dir, "\\share\\")
              && str_eq(name, "f") && id == 2,
              "host split: a UNC server is the drive; .stn is id 2 regardless of case");
        id = 0;
        r = sp("x.txt", 0, 0, 0, &id);
        id2 = 0;
        sp("y.TXT", 0, 0, 0, &id2);
        emit("  .TXT registered as "); emit_u((uint32_t)id); emit("\r\n");
        check(r == 0 && id >= 0x25 && id2 == id,
              "host split registers an unknown extension, and reuses its id");
        drive[0] = dir[0] = name[0] = 'X';
        r = sp("", drive, dir, name, 0);
        check(r == 0 && !drive[0] && !dir[0] && !name[0], "host split of \"\" empties everything");
        check(sp(0, drive, dir, name, 0) == -1, "host split of null fails");
    }

    /* +0x164 / +0x160 / +0x15c enumeration */
    {
        struct fs98_host_find *h = 0;
        struct fs98_host_find_entry e;
        int seen_a = 0, seen_ro = 0, seen_dir = 0, seen_dot = 0, n = 0;

        r = FS98_HOST_FIND_FIRST(host)(&h, "C:\\openfs98-enum", 0, FS98_HOST_EXT_ANY);
        check(r == 0 && h != 0, "host find-first opens a search");
        if (r == 0) {
            while (FS98_HOST_FIND_NEXT(host)(h, &e) == 0) {
                n++;
                if (e.name[0] == '.' && (e.name[1] == 0 || e.name[1] == '.'))
                    seen_dot = 1;
                if (str_eq(e.name, "a"))
                    seen_a = (e.type == 1 && e.size == 10 && e.writable == 1);
                if (str_eq(e.name, "ro"))
                    seen_ro = (e.type == 1 && e.size == 30 && e.writable == 0);
                if (str_eq(e.name, "sub1"))
                    seen_dir = (e.type == FS98_HOST_TYPE_DIR && e.size == 0);
            }
            FS98_HOST_FIND_CLOSE(host)(&h);
        }
        emit("  entries under openfs98-enum: "); emit_u((uint32_t)n); emit("\r\n");
        check(seen_a,   "host find-next: a file's name loses its extension; id, size, writable");
        check(seen_ro,  "host find-next: a read-only file is not writable");
        check(seen_dir, "host find-next: a directory is type -2, size 0");
        check(!seen_dot, "host find-next skips . and ..");
        check(h == 0, "host find-close nulls the handle");

        h = (struct fs98_host_find *)0x1234;
        r = FS98_HOST_FIND_FIRST(host)(&h, "C:\\openfs98-enum\\nosuch", 0, 1);
        check(r == -1 && h == (struct fs98_host_find *)0x1234,
              "host find-first failing leaves the handle alone");

        /* the empty-directory quirk: "" is the drive root, null is the cwd */
        r = FS98_HOST_FIND_FIRST(host)(&h, 0, "openfs98-enum", FS98_HOST_EXT_NONE);
        check(r == -1, "host find-first with a null dir searches the cwd");
        if (r == 0) FS98_HOST_FIND_CLOSE(host)(&h);
        r = FS98_HOST_FIND_FIRST(host)(&h, "", "openfs98-enum", FS98_HOST_EXT_NONE);
        check(r == 0, "host find-first with an EMPTY dir searches the drive root");
        if (r == 0) FS98_HOST_FIND_CLOSE(host)(&h);
    }
}

/* ---- UTIL 0x94 / 0xa0 / 0x98 against retail ------------------------------- */

typedef int32_t (FS98_STDCALL *list_tree_fn)(void **, int32_t *, const char *,
                                             const char *, int32_t);
typedef int32_t (FS98_STDCALL *is_dir_fn)(const char *);
typedef int32_t (FS98_STDCALL *catalogue_fn)(uint32_t, uint32_t, int32_t, int32_t *,
                                             void **, entry_cb, entry_cb, entry_cb,
                                             int32_t);

static void compare_tree(void *host, void *util, const char *dir,
                         const char *pattern, int32_t ext, int32_t expect,
                         const char *what)
{
    list_tree_fn retail = *(list_tree_fn *)((unsigned char *)util + 0x94);
    void *rd = 0, *od = 0;
    int32_t rn = -1, on = -1, rr, orr;

    rr  = retail(&rd, &rn, dir, pattern, ext);
    orr = fs98_util_list_tree(&od, &on, dir, pattern, ext);
    emit("  0x94 "); emit(what); emit(": retail "); emit_u((uint32_t)rn);
    emit(", ours "); emit_u((uint32_t)on); emit("\r\n");
    if (rn > 0 && rn == on && !mem_eq(rd, od, (uint32_t)rn * 0x108)) {
        uint32_t k = 0;
        while (((unsigned char *)rd)[k] == ((unsigned char *)od)[k]) k++;
        emit("    first difference at record "); emit_u(k / 0x108);
        emit(" +"); emit_x(k % 0x108);
        emit(": retail \""); emit((char *)rd + k / 0x108 * 0x108);
        emit("\" type "); emit_x(*(uint32_t *)((char *)rd + k / 0x108 * 0x108 + 0x104));
        emit(", ours \""); emit((char *)od + k / 0x108 * 0x108);
        emit("\" type "); emit_x(*(uint32_t *)((char *)od + k / 0x108 * 0x108 + 0x104));
        emit("\r\n");
    }
    check(rr == orr && rn == on && rn == expect
          && (rn == 0 ? (rd == 0 && od == 0) : mem_eq(rd, od, (uint32_t)rn * 0x108)),
          what);
    if (rd) FS98_HOST_FREE(host)(&rd);
    if (od) FS98_HOST_FREE(host)(&od);
}

static void compare_is_dir(void *util, const char *path, int32_t expect)
{
    is_dir_fn retail = *(is_dir_fn *)((unsigned char *)util + 0xa0);
    int32_t r = retail(path), o = fs98_util_is_dir(path);
    emit("  0xa0 \""); emit(path ? path : "(null)"); emit("\" -> retail ");
    emit_u((uint32_t)r); emit(", ours "); emit_u((uint32_t)o); emit("\r\n");
    check(r == o && r == expect, "UTIL +0xa0 matches retail and expectation");
}

static void compare_catalogue(void *host, void *util, uint32_t a, uint32_t b,
                              int32_t ext, entry_cb n, entry_cb d, entry_cb y,
                              int32_t sort, int32_t expect, const char *what)
{
    catalogue_fn retail = *(catalogue_fn *)((unsigned char *)util + 0x98);
    void *rd = (void *)1, *od = (void *)1;
    int32_t rn = -1, on = -1, rr, orr;
    unsigned rc, oc;

    cb_calls = 0;
    rr = retail(a, b, ext, &rn, &rd, n, d, y, sort);
    rc = cb_calls; cb_calls = 0;
    orr = fs98_util_catalogue(a, b, ext, &on, &od, n, d, y, sort);
    oc = cb_calls;
    emit("  0x98 "); emit(what); emit(": retail "); emit_u((uint32_t)rn);
    emit(" ("); emit_u(rc); emit(" callbacks), ours "); emit_u((uint32_t)on);
    emit(" ("); emit_u(oc); emit(")\r\n");
    check(rr == orr && rn == on && rc == oc
          && (expect < 0 ? rn > 0 : rn == expect)
          && (rn == 0 ? (rd == 0 && od == 0) : mem_eq(rd, od, (uint32_t)rn * 0xa08)),
          what);
    if (rn > 0 && rd) {
        unsigned i;
        emit("    ");
        for (i = 0; i < (unsigned)rn && i < 12; i++) {
            emit((char *)rd + i * 0xa08); emit("/");
            emit_u(*(uint32_t *)((char *)rd + i * 0xa08 + 0xa04)); emit(" ");
        }
        emit("\r\n");
    }
    if (rn > 0 && rd) FS98_HOST_FREE(host)(&rd);
    if (on > 0 && od) FS98_HOST_FREE(host)(&od);
}

static void test_util_files(void *host, void *util)
{
    char cwd[MAX_PATH];

    compare_tree(host, util, "C:\\openfs98-enum", "*", 1, 8,
                 "UTIL +0x94 matches retail: *.BGL, recursively");
    compare_tree(host, util, "C:\\openfs98-enum", "*", FS98_HOST_EXT_ANY, 15,
                 "UTIL +0x94 matches retail: *.*, recursively");
    compare_tree(host, util, "C:\\openfs98-enum", "?", 1, 5,
                 "UTIL +0x94 matches retail: ?.BGL");
    compare_tree(host, util, "C:\\openfs98-enum", "README", FS98_HOST_EXT_NONE, 1,
                 "UTIL +0x94 matches retail: an exact name, no extension");
    compare_tree(host, util, "C:\\openfs98-enum\\", "*", 2, 1,
                 "UTIL +0x94 matches retail: *.STN, dir with a trailing slash");
    compare_tree(host, util, "C:\\openfs98-enum\\nosuch", "*", 1, 0,
                 "UTIL +0x94 matches retail: a missing directory");

    compare_is_dir(util, "C:\\openfs98-enum\\sub1", 1);
    compare_is_dir(util, "C:\\openfs98-enum\\SUB1", 1);
    compare_is_dir(util, "C:\\openfs98-enum\\sub1\\", 0);
    compare_is_dir(util, "C:\\openfs98-enum\\a", 0);
    compare_is_dir(util, "C:\\openfs98-enum\\dir.BGL", 0);  /* .BGL is split off */
    compare_is_dir(util, "C:\\openfs98-enum\\nosuch", 0);
    compare_is_dir(util, "C:\\openfs98-enum", 1);
    compare_is_dir(util, "openfs98-enum", 1);      /* the drive-root quirk */
    compare_is_dir(util, "Modules", 0);            /* ...and its other face */
    compare_is_dir(util, ".\\Modules", 1);
    compare_is_dir(util, "", 0);
    compare_is_dir(util, 0, 0);

    GetCurrentDirectoryA(sizeof cwd, cwd);
    if (SetCurrentDirectoryA("C:\\openfs98-cat")) {
        compare_catalogue(host, util, 0x9004, 0x9002, 2, cb_name, cb_desc, cb_body, 1, 9,
                          "UTIL +0x98 matches retail: both dirs, all callbacks, by name");
        compare_catalogue(host, util, 0x9004, 0x9002, 2, cb_name, cb_desc, cb_body, 2, 9,
                          "UTIL +0x98 matches retail: by path");
        compare_catalogue(host, util, 0x9004, 0x9002, 2, cb_name, cb_desc, cb_body, 0, 9,
                          "UTIL +0x98 matches retail: unsorted");
        compare_catalogue(host, util, 0x9004, 0x9002, 2, 0, 0, 0, 1, 15,
                          "UTIL +0x98 matches retail: no callbacks");
        compare_catalogue(host, util, 0x9004, 0, 1, cb_name, 0, 0, 1, 1,
                          "UTIL +0x98 matches retail: first dir only, *.BGL");
        compare_catalogue(host, util, 0x9004, 0x9002, 0x1d, cb_name, 0, 0, 1, 0,
                          "UTIL +0x98 matches retail: nothing matches");
        compare_catalogue(host, util, 0, 0x9002, 2, cb_name, cb_desc, cb_body, 1, 5,
                          "UTIL +0x98 matches retail: second dir only");
        SetCurrentDirectoryA(cwd);
    } else {
        check(0, "could not change into the catalogue tree");
    }

    /* and over the game's own folders, as MAIN and DEMO call it */
    compare_catalogue(host, util, 0x9004, 0x9002, 2, 0, 0, 0, 1, -1,
                      "UTIL +0x98 matches retail: the real flights");
    compare_catalogue(host, util, 0x9007, 0x9002, 0x1d, 0, 0, 0, 1, -1,
                      "UTIL +0x98 matches retail: the real videos");
}

/* ---- the tests ------------------------------------------------------------ */

static void run_tests(void)
{
    void *host   = ImportTable[0].link;
    void *global = ImportTable[1].link;
    void *util   = ImportTable[2].link;

    emit("OpenFS98 in-process test module\r\n");

    emit("host pointer   "); emit_x((uint32_t)(uintptr_t)host);   emit("\r\n");
    emit("GLOBAL pointer "); emit_x((uint32_t)(uintptr_t)global); emit("\r\n");
    emit("UTIL pointer   "); emit_x((uint32_t)(uintptr_t)util);   emit("\r\n");

    check(host   != 0, "the loader bound the host");
    check(global != 0, "the loader bound GLOBAL");
    check(util   != 0, "the loader bound UTIL");
    if (!host || !global || !util)
        return;

    check(*(uint32_t *)global == FS98_MOD_GLOBAL, "GLOBAL reports module id 2");
    check(*(uint32_t *)util   == FS98_MOD_UTIL,   "UTIL reports module id 20");
    /* Everything below compares against the UTIL the loader bound, so it has
     * to be retail's -- whose Linkage sits at 0x208e5020 -- or the comparisons
     * are of the reimplementation with itself. */
    check(util == (void *)0x208e5020, "the UTIL under comparison is retail's");

    /* host +0x2b8: the null-safe strlen UTIL measures strings through */
    {
        fs98_host_strlen_fn hstrlen = FS98_HOST_STRLEN(host);
        check(hstrlen("") == 0,        "host strlen of an empty string is 0");
        check(hstrlen("abcd") == 4,    "host strlen of \"abcd\" is 4");
        check(hstrlen(0) == 0,         "host strlen tolerates a null pointer");
    }

    /* host +0x38: the millisecond clock */
    {
        fs98_host_ticks_fn ticks = FS98_HOST_TICKS(host);
        uint32_t a = ticks(), bq = ticks();
        emit("  ticks "); emit_u(a); emit(" then "); emit_u(bq); emit("\r\n");
        check(bq >= a, "host clock does not run backwards");
    }

    /* UTIL +0x9c against the reimplementation, both measuring through the
     * real host rather than a stub */
    {
        typedef const char * (FS98_STDCALL *util_longer_fn)(const char *, const char *);
        util_longer_fn rlonger = *(util_longer_fn *)((unsigned char *)util + 0x9c);
        static const char *s[] = { "", "a", "ab", "abc", "hello world" };
        unsigned i, j, bad = 0;
        fs98_util_set_host(host);
        for (i = 0; i < 5; i++)
            for (j = 0; j < 5; j++)
                if (rlonger(s[i], s[j]) != fs98_util_longer_string(s[i], s[j]))
                    bad++;
        check(bad == 0, "UTIL +0x9c matches retail across 25 string pairs");
    }

    /* The host's dialog registry at 0x417ee0 -- a host list of 16-byte
     * { id, module, index, flags } records, filled at startup from every
     * module's Linkage+0x14.  UTIL's alerts open ids 2 and 3; both must be
     * English\Alerts.DLL's (module 0x80000009), 2 modal and 3 modeless. */
    {
        const struct fs98_host_list *reg = (const struct fs98_host_list *)(uintptr_t)0x417ee0;
        int seen2 = 0, seen3 = 0;
        int32_t k;
        emit("  dialog registry: "); emit_u((uint32_t)reg->count); emit(" entries\r\n");
        if (reg->elem_size == 16 && reg->count > 0 && reg->count < 1000 && reg->data)
            for (k = 0; k < reg->count; k++) {
                const uint32_t *e = (const uint32_t *)reg->data + 4 * k;
                if (e[0] == 2) seen2 = (e[1] == 0x80000009u && e[3] == 0);
                if (e[0] == 3) seen3 = (e[1] == 0x80000009u && e[3] == 4);
            }
        check(seen2, "dialog 2 is Alerts.DLL's, modal");
        check(seen3, "dialog 3 is Alerts.DLL's, modeless (flag 4)");
    }

    /* the host's enumeration cluster, then UTIL's services built on it */
    if (tree_build()) {
        test_host_files(host);
        test_util_files(host, util);
    } else {
        check(0, "could not build the scratch tree under C:\\");
    }
    tree_teardown();

    emit("\r\n");
    emit("passed "); emit_u(pass_count);
    emit(", failed "); emit_u(fail_count);
    emit("\r\n");
}

/* ---- module plumbing ------------------------------------------------------ */

static void ofstest_init(void)
{
    log_handle = CreateFileA(OFSTEST_LOG, GENERIC_WRITE, FILE_SHARE_READ, 0,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    run_tests();
    if (log_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(log_handle);
        log_handle = INVALID_HANDLE_VALUE;
    }
}

static void ofstest_deinit(void) { }

struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
} Linkage = { 5, ofstest_init, ofstest_deinit };

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
