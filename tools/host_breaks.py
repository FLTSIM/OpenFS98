#!/usr/bin/env python3
"""Break OpenFS98's HOST (src/host/host.c) on purpose, one place at a time, and
check that hostdiff notices.

Each break is a small edit to a copy of host.c -- the kind of slip a
transliteration makes -- built as the harness's HOST_ours.dll and run against
the real host for N iterations (hostdiff exits non-zero when any slot differs).
At the end the real build goes back.  A missed break is a gap in the harness or
an equivalence; either way it is reported.

Usage: tools/host_breaks.py [N] [name...]   (from the repository root, after
       make hostdiff)
"""
import os
import shutil
import subprocess
import sys

args = sys.argv[1:]
N = int(args.pop(0)) if args and args[0].isdigit() else 4000
ONLY = args
BREAK = "bin/difftest/host_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    ("const-one",   "static uint32_t FS98_STDCALL host_01c(void)\n{\n    return 1;",
                    "static uint32_t FS98_STDCALL host_01c(void)\n{\n    return 2;"),
    ("global-20",   "return M32(V(0x41719c));", "return M32(V(0x417198));"),
    ("global-34",   "return M32(V(0x418850));", "return M32(V(0x418854));"),
    ("past-table",  "return V(0x417ec0);", "return V(0x417ec4);"),
    ("struct-5c",   "return V(0x41c240);", "return V(0x41c244);"),
    ("struct-60",   "return V(0x41bb80);", "return V(0x41bb84);"),
    ("copy-18-a",   "M32(V(0x418830)) = M32(arg + 0x18);", "M32(V(0x418830)) = M32(arg + 0x1c);"),
    ("copy-18-f",   "M32(V(0x418844)) = M32(arg + 0xbc);", "M32(V(0x418844)) = M32(arg + 0xc0);"),
    ("set-2c",      "M32(V(0x4187ec)) = 1;", "M32(V(0x4187ec)) = 2;"),
    ("read-54-a",   "M32(out1) = M32(V(0x41880c));", "M32(out1) = M32(V(0x418810));"),
    ("read-54-b",   "M32(out2) = M32(V(0x418828));", "M32(out2) = M32(V(0x41882c));"),
    ("strcpy-nul",  "    while ((*d++ = *s++) != '\\0')\n        ;\n    return dst;\n}\n\n/* +0x2b0",
                    "    while ((*d++ = *s++) != '\\0')\n        ;\n    d[-1] = 'x';\n    return dst;\n}\n\n/* +0x2b0"),
    ("strcat-start","    while (*d != '\\0')\n        d++;\n    while ((*d++ = *s++) != '\\0')",
                    "    while (*d != '\\0')\n        d++;\n    d++;\n    while ((*d++ = *s++) != '\\0')"),
    ("strlen-null", "    if (p == 0)\n        return 0;", "    if (p == 0)\n        return 1;"),
    ("strlen-len",  "    while (p[n] != '\\0')\n        n++;\n    return n;",
                    "    while (p[n] != '\\0')\n        n++;\n    return n + 1;"),
    ("strcpy-const","const char *s = (const char *)(uintptr_t)V(0x41bca0);",
                    "const char *s = (const char *)(uintptr_t)V(0x41bca1);"),
    ("unc-byte",    "p[0] == 0x5c && p[1] == 0x5c", "p[0] == 0x5c && p[1] == 0x2f"),
    ("unc-null",    "return (p != 0 && p[0]", "return (p[0]"),
    ("memset-val",  "d[i] = (uint8_t)val;", "d[i] = (uint8_t)(val + 1);"),
    ("memset-count","for (i = 0; i < count; i++)\n        d[i]", "for (i = 0; i + 1 < count; i++)\n        d[i]"),
    ("memcmp-sign", "return p[i] < q[i] ? (uint32_t)-1 : 1;", "return p[i] < q[i] ? 1 : (uint32_t)-1;"),
    ("set-288",     "M32(V(0x417194)) = v;", "M32(V(0x417194)) = v + 1;"),
    ("compact-byte","M8(dst + 3 * i + 2) = M8(V(0x4171a4) + 4 * i + 2);",
                    "M8(dst + 3 * i + 2) = M8(V(0x4171a4) + 4 * i + 3);"),
    ("compact-stride", "M8(dst + 3 * i + 0) = M8(V(0x4171a4) + 4 * i + 0);",
                       "M8(dst + 3 * i + 0) = M8(V(0x4171a4) + 3 * i + 0);"),
    ("strcmp-sign", "return *p < *q ? (uint32_t)-1 : 1;", "return *p < *q ? 1 : (uint32_t)-1;"),
    ("flag-2cc",    "return M32(V(0x41a3cc)) == 0 ? 1 : 0;", "return M32(V(0x41a3cc)) == 0 ? 0 : 1;"),
    ("clear-304",   "M32(p) = 0;\n    return p;", "M32(p) = 1;\n    return p;"),
    ("stub-108",    "static uint32_t FS98_STDCALL host_zero2(uint32_t a, uint32_t b) { (void)a; (void)b; return 0; }",
                    "static uint32_t FS98_STDCALL host_zero2(uint32_t a, uint32_t b) { (void)a; (void)b; return 1; }"),
    ("alloc-zero",  "        for (i = 0; i < size; i++)\n            p[i] = 0;\n    }\n    return 0;",
                    "        for (i = 1; i < size; i++)\n            p[i] = 0;\n    }\n    return 0;"),
    ("alloc-fail",  "        if (flags == 4)\n            return (uint32_t)-1;\n        host_purge",
                    "        if (flags == 4)\n            return 0;\n        host_purge"),
    ("realloc-fail","            M32(pp) = ADDR(old);\n            return (uint32_t)-1;",
                    "            return (uint32_t)-1;"),
    ("free-null",   "        free((void *)(uintptr_t)M32(pp));\n        M32(pp) = 0;",
                    "        free((void *)(uintptr_t)M32(pp));"),
    ("memmove-len", "memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, size);",
                    "memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, size ? size - 1 : 0);"),
    ("grow-step",   "uint32_t cap = L_CAP(l) + L_GROW(l);", "uint32_t cap = L_CAP(l) + L_GROW(l) + 1;"),
    ("grow-realloc","        if (count == 0)\n            host_20c(1, L_SIZE(l) * cap, l + 0x18, 0);\n        else\n            host_210(1, L_SIZE(l) * cap, l + 0x18);",
                    "        if (count == 0)\n            host_20c(1, L_SIZE(l) * cap, l + 0x18, 0);\n        else\n            host_20c(1, L_SIZE(l) * cap, l + 0x18, 1);"),
    ("append-zero", "    for (j = 0; j < size; j++)\n        p[j] = 0;\n    return i * L_SIZE(l) + L_DATA(l);",
                    "    for (j = 1; j < size; j++)\n        p[j] = 0;\n    return i * L_SIZE(l) + L_DATA(l);"),
    ("queue-flag",  "        L_PEND(l) = 1;\n        e = host_2f8(HOST_QUEUE);", "        e = host_2f8(HOST_QUEUE);"),
    ("queue-index", "        M32(e + 4) = (uint32_t)i;", "        M32(e + 4) = (uint32_t)i + 1;"),
    ("remove-past", "    if (i < count) {\n        uint32_t size = L_SIZE(l)", "    if (i <= count) {\n        uint32_t size = L_SIZE(l)"),
    ("remove-ret",  "        return size;                /* eax", "        return 0;                /* eax"),
    ("remove-index","    return host_list_remove_at(l, (int32_t)((elem - L_DATA(l)) / L_SIZE(l)));",
                    "    return host_list_remove_at(l, (int32_t)((elem - L_DATA(l) + 1) / L_SIZE(l)));"),
    ("flush-match", "            if (M32(q + n * 8 - 8) == l) {", "            if (M32(q + n * 8 - 8) != 0) {"),
    ("flush-reload","                host_list_remove_at(HOST_QUEUE, (int32_t)(n - 1));\n                q = L_DATA(HOST_QUEUE);",
                    "                host_list_remove_at(HOST_QUEUE, (int32_t)(n - 1));"),
    ("flush-flag",  "    L_PEND(l) = 0;\n}", "}"),
    ("walk-order",  "        r = ((host_walk_fn)(uintptr_t)fn)(i * L_SIZE(l) + L_DATA(l), ctx);",
                    "        r = ((host_walk_fn)(uintptr_t)fn)((L_COUNT(l) - i - 1) * L_SIZE(l) + L_DATA(l), ctx);"),
    ("walk-depth",  "    L_ITER(l) = L_ITER(l) + 1;\n    for (i = 0; i < L_COUNT(l); i++) {\n        r = ((host_walk_fn)(uintptr_t)fn)(i",
                    "    for (i = 0; i < L_COUNT(l); i++) {\n        r = ((host_walk_fn)(uintptr_t)fn)(i"),
    ("walkback-idx","((L_COUNT(l) - i - 1) * L_SIZE(l) + L_DATA(l), ctx);\n        if (r != 0)\n            break;\n    }\n    L_ITER(l) = L_ITER(l) - 1;\n    host_list_flush(l);\n    return r;\n}\n\n/* 0x4104e0",
                    "((L_COUNT(l) - i) * L_SIZE(l) + L_DATA(l), ctx);\n        if (r != 0)\n            break;\n    }\n    L_ITER(l) = L_ITER(l) - 1;\n    host_list_flush(l);\n    return r;\n}\n\n/* 0x4104e0"),
    ("walkback-stop","        r = ((host_walk_fn)(uintptr_t)fn)((L_COUNT(l) - i - 1) * L_SIZE(l) + L_DATA(l), ctx);\n        if (r != 0)\n            break;",
                    "        r = ((host_walk_fn)(uintptr_t)fn)((L_COUNT(l) - i - 1) * L_SIZE(l) + L_DATA(l), ctx);\n        if (r != 0 && i > 0)\n            break;"),
    ("find-indirect","const uint8_t *b = (const uint8_t *)(uintptr_t)(indirect ? M32(key) : key);",
                    "const uint8_t *b = (const uint8_t *)(uintptr_t)key;"),
    ("find-len",    "        while (k < len && a[k] == b[k])", "        while (k + 1 < len && a[k] == b[k])"),
    ("find-ret",    "    return L_SIZE(l) * (uint32_t)i + L_DATA(l);\n}", "    return L_SIZE(l) * (uint32_t)(i + 1) + L_DATA(l);\n}"),
    ("listfree-ret","        return l + 0x18;", "        return 0;"),
    ("qsort-tramp", "        ADDR(a), ADDR(b));", "        ADDR(b), ADDR(a));"),
    ("qsort-park",  "    M32(V(0x41a24c)) = cmp;\n    qsort(", "    M32(V(0x41a248)) = cmp;\n    M32(V(0x41a24c)) = cmp;\n    qsort("),
    ("bsearch-n",   "return ADDR(bsearch((const void *)(uintptr_t)key, (const void *)(uintptr_t)base, n, width,",
                    "return ADDR(bsearch((const void *)(uintptr_t)key, (const void *)(uintptr_t)base, n ? n - 1 : 0, width,"),
    ("swap-order",  "    host_21c(a, b, size);\n    host_21c(b, tmp, size);", "    host_21c(b, tmp, size);\n    host_21c(a, b, size);"),
    ("strncpy-n",   "return ADDR(strncpy((char *)(uintptr_t)dst, (const char *)(uintptr_t)src, n));",
                    "return ADDR(strncpy((char *)(uintptr_t)dst, (const char *)(uintptr_t)src, n ? n - 1 : 0));"),
    ("strdup-null", "    if (s == 0)\n        return 0;\n    host_20c(1, host_2b8(s) + 1", "    if (s == 0)\n        return 1;\n    host_20c(1, host_2b8(s) + 1"),
    ("strcmpi-raw", "return (uint32_t)_strcmpi((const char *)(uintptr_t)a, (const char *)(uintptr_t)b);",
                    "return (uint32_t)strcmp((const char *)(uintptr_t)a, (const char *)(uintptr_t)b);"),
    ("strip-skip",  "            host_2a8(p, i + s + 1);\n        } else {", "            host_2a8(p, i + s + 1);\n            p++;\n        } else {"),
    ("opt-prev",    "if ((c == '/' || c == '-') && M8(p - 1) == ' ' &&", "if ((c == '/' || c == '-') &&"),
    ("opt-sep",     "(M8(host_2b8(name) + p + 1) == ':' || M8(host_2b8(name) + p + 1) == ' '))",
                    "(M8(host_2b8(name) + p + 1) == ':'))"),
    ("opt-end",     "        if ((c == '/' || c == '-') && M8(e - 1) == ' ')\n            break;",
                    "        if (c == '/' && M8(e - 1) == ' ')\n            break;"),
    ("opt-fit",     "    if (n >= len)\n        return (uint32_t)-1;", "    if (n > len)\n        return (uint32_t)-1;"),
    ("opt-lead",    "    while (M8(q) != 0 && M8(q) == ' ')", "    while (0 && M8(q) == ' ')"),
    ("opt-trail",   "            M8(t) = 0;\n            t = ADDR(CharPrevA(", "            t = ADDR(CharPrevA("),
    ("opt-trail-start", "        while (t != buf) {\n            M8(t) = 0;", "        while (t + 1 != buf) {\n            M8(t) = 0;"),
    ("opt-colon",   "    if (v == 0 || M8(v - 1) != ':')", "    if (v == 0)"),
    ("opt-int",     "        M32(out) = (uint32_t)atoi(buf);", "        M32(out) = (uint32_t)atoi(buf + 1);"),
    ("opt-double",  "        *(double *)(uintptr_t)out = atof(buf);", "        *(double *)(uintptr_t)out = (float)atof(buf);"),
    ("opt-given",   "    return (M8(v - 1) != ' ') + 1;", "    return (M8(v - 1) == ':') + 1;",
                    "the finder only returns past a ':' or a ' '"),
    ("ext-builtin", "    for (i = 0; i < 0x25; i++)\n        if (host_str_eq(ext,", "    for (i = 0; i < 0x24; i++)\n        if (host_str_eq(ext,"),
    ("ext-dyn-id",  "            M32(out) = (uint32_t)i + 0x25;", "            M32(out) = (uint32_t)i + 0x24;"),
    ("ext-upper",   "static uint32_t host_ext_lookup(uint32_t ext, uint32_t out)\n{\n    int32_t i, n;\n    _strupr((char *)(uintptr_t)ext);",
                    "static uint32_t host_ext_lookup(uint32_t ext, uint32_t out)\n{\n    int32_t i, n;",
                    "+0x1ac upper-cases first, and +0x188 falls back to +0x1ac on a miss"),
    ("ext-new-id",  "    M32(out) = M32(V(0x4188ac)) + 0x25;", "    M32(out) = M32(V(0x4188ac)) + 0x24;"),
    ("ext-grow",    "        host_210(1, n * 4 + 4, V(0x4188b0));", "        host_210(1, n * 4, V(0x4188b0));"),
    ("split-empty", "    if (fname != 0) host_2a8(fname, V(0x4187f8));", ""),
    ("split-unc",   "        host_2a8(ADDR(unc) + 2, path + 2);", "        host_2a8(ADDR(unc) + 2, path + 3);"),
    ("split-unc-cut","        M8(p) = 0;\n        host_2a8(drive, ADDR(unc));", "        host_2a8(drive, ADDR(unc));"),
    ("split-unc-drive","    if (drive != 0 && host_168(path) != 0) {", "    if (host_168(path) != 0) {"),
    ("split-id0",   "    if (ext_id != 0)\n        M32(ext_id) = 0;", ""),
    ("split-noext", "    if (ext[0] != 0 && ext_id != 0 && host_ext_lookup", "    if (ext_id != 0 && host_ext_lookup",
                    "\"\" looks up as id 0, built-in entry 0 being the host's empty string -- the 0 already written"),
    ("str-order",   "    n = host_30c(HOST_STRMODS, ADDR(req), ADDR(host_str_cb));", "    n = host_308(HOST_STRMODS, ADDR(req), ADDR(host_str_cb));"),
    ("str-fallback","        n = (uint32_t)LoadStringA((HINSTANCE)(uintptr_t)M32(V(0x41882c)), id,", "        n = (uint32_t)LoadStringA((HINSTANCE)(uintptr_t)M32(V(0x418828)), id,"),
    ("str-len",     "                                 (char *)(uintptr_t)M32(req + 4), (int)M32(req + 8));",
                    "                                 (char *)(uintptr_t)M32(req + 4), (int)M32(req + 8) - 1);"),
    ("strdup-res",  "        host_20c(1, n + 1, out, 0);\n        host_2a8(M32(out), ADDR(buf));", "        host_20c(1, n + 1, out, 0);\n        host_2a8(M32(out), ADDR(buf) + 1);"),
    ("resdup-null", "    char buf[0x400];\n    uint32_t n;\n    M32(out) = 0;", "    char buf[0x400];\n    uint32_t n;"),
    ("mod-key",     "    uint32_t rec = host_310(HOST_MODULES, 0xc, ADDR(&id), 4, 0), e;", "    uint32_t rec = host_310(HOST_MODULES, 0x8, ADDR(&id), 4, 0), e;"),
    ("mod-loaded",  "    if (M32(rec + 0x10) == 0) {", "    if (M32(rec + 0x10) == 1) {"),
    ("mod-hmod",    "    M32(e) = M32(rec + 8);", "    M32(e) = M32(rec + 0xc);"),
    ("mod-out",     "    M32(out) = M32(rec + 8);\n    return 0;", "    return 0;"),
    ("drop-ret",    "    host_300(HOST_STRMODS, e);\n    return 0;", "    host_300(HOST_STRMODS, e);\n    return 1;"),
    ("drop-none",   "    if (e == 0)\n        return (uint32_t)-1;\n    host_300(HOST_STRMODS, e);", "    if (e == 0)\n        return (uint32_t)-1;"),
    ("names-flag",  "        M32(V(0x41a240)) = 1;\n", ""),
    ("names-len",   "        M32(V(0x41a244)) = host_2a0(0x8012, V(0x418108), 0x32);", "        M32(V(0x41a244)) = host_2a0(0x8012, V(0x418108), 0x32) + 1;"),
    ("names-eax",   "        M32(V(0x41a248)) = eax;\n    }\n    return eax;", "        M32(V(0x41a248)) = eax;\n    }\n    return 0;"),
    ("name-range",  "    if (c >= 0x11 && c <= 0x50)", "    if (c >= 0x11 && c <= 0x4f)"),
    ("name-axis",   "                                  (const char *)(uintptr_t)V(0x418140), c);", "                                  (const char *)(uintptr_t)V(0x418140), c - 1);"),
    ("name-clear",  "    (void)unused;\n    M8(buf) = 0;", "    (void)unused;"),
    ("code-advance","        M32(out) = 0x11;\n        s += M32(V(0x41a244));", "        M32(out) = 0x11;"),
    ("code-cap",    "        if ((int32_t)eax > 0x50)", "        if ((int32_t)eax >= 0x50)"),
    ("code-axis",   "        M32(out) = 1;\n        s += M32(V(0x41a248));", "        M32(out) = 2;\n        s += M32(V(0x41a248));"),
    ("code-ret",    "            M32(out) = 0;\n    }\n    return eax;\n}", "            M32(out) = 0;\n    }\n    return 0;\n}"),
    ("name-colon",  "    case ':': case '\\\\':\n        return !path;", "    case ':':\n        return !path;"),
    ("name-bad",    "    case '\"': case '&': case '*':", "    case '\"': case '*':"),
    ("name-empty",  "    return n != 0 ? 0 : 0x1003;", "    return n != 0 ? 0 : 0x1002;"),
    ("name-null",   "    if (s == 0)\n        return (uint32_t)-1;\n    while (M8(s) != 0) {", "    if (s == 0)\n        return 0x1003;\n    while (M8(s) != 0) {"),
    ("join-colon",  "        if (host_2b8(b) == 1)\n            host_2b0(b, V(0x4180c0));", "        if (host_2b8(b) == 2)\n            host_2b0(b, V(0x4180c0));"),
    ("join-cut",    "        M8(host_2b8(buf) + buf - seplen) = 0;", "        M8(host_2b8(buf) + buf - seplen + 1) = 0;"),
    ("join-dircut", "        host_2b0(b, dir);\n        host_cut_sep(b, seplen);", "        host_2b0(b, dir);"),
    ("join-sep",    "        if (M8(name) != '\\\\' && host_2b8(b) != 0)", "        if (M8(name) != '\\\\')"),
    ("join-fit",    "    if (host_2b8(b) >= 0x104) {", "    if (host_2b8(b) > 0x104) {"),
    ("join-clear",  "        M8(out) = 0;\n        return (uint32_t)-1;\n    }\n    host_2a8(out, b);", "        return (uint32_t)-1;\n    }\n    host_2a8(out, b);"),
    ("img-fmt",     "    if ((int32_t)fmt < 0 || (int32_t)fmt >= 6)", "    if ((int32_t)fmt < 0 || (int32_t)fmt >= 5)"),
    ("img-round",   "    M32(p + 0xc) = (M32(p + 0xc) + 3) & ~3u;", "    M32(p + 0xc) = (M32(p + 0xc) + 3) & ~1u;"),
    ("img-bpp",     "#define IMG_BPP(f)  M32(V(0x4180e8) + 4 * (f))", "#define IMG_BPP(f)  M32(V(0x4180e4) + 4 * (f))"),
    ("img-rows",    "        d += M32(p + 0xc);", "        d += M32(p + 4);"),
    ("img-out",     "    host_img_rows(p);\n    M32(out) = p;", "    host_img_rows(p);"),
    ("img-zero",    "    host_20c(1, h * stride, p + 0x18, 1);", "    host_20c(1, h * stride, p + 0x18, 0);"),
    ("img-resize-h","    M32(p + 8) = h;\n    stride", "    M32(p + 8) = w;\n    stride"),
    ("img-free",    "    host_214(ADDR(&p));\n    return 0;", "    host_214(ADDR(&p));\n    return 1;"),
    ("acc-id",      "    M32(t + 4) = M32(V(0x418294));\n    M32(t + 8) = 0;", "    M32(t + 4) = M32(V(0x418294)) + 1;\n    M32(t + 8) = 0;"),
    ("acc-wrap",    "    if (M32(V(0x418294)) == 0)\n        M32(V(0x418294)) = 1;", "    if (M32(V(0x418294)) == 0)\n        M32(V(0x418294)) = 0;"),
    ("acc-key",     "    return host_310(HOST_ACCELS, 4, key, 4, 0);", "    return host_310(HOST_ACCELS, 0, key, 4, 0);"),
    ("acc-shift",   "    if (M8(a) & 0x4)\n        M32(e + 4) = 1;", "    if (M8(a) & 0x4)\n        M32(e + 4) = 2;"),
    ("acc-virt",    "    if (M8(a) & 0x1)\n        M32(e + 4) |= 8;", "    if (M8(a) & 0x2)\n        M32(e + 4) |= 8;"),
    ("acc-key-off", "    M32(e + 8) = M16(a + 4) + 0x10000u;", "    M32(e + 8) = M16(a + 4);"),
    ("acc-destroy-zero", "    M32(id) = 0;\n    host_300(HOST_ACCELS, t);", "    host_300(HOST_ACCELS, t);"),
    ("acc-index",   "    uint32_t t = i < L_COUNT(HOST_ACCELS)", "    uint32_t t = i <= L_COUNT(HOST_ACCELS)"),
    ("acc-load-host","        h = ADDR(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(V(0x41882c)),", "        h = ADDR(LoadAcceleratorsA((HINSTANCE)(uintptr_t)0,"),
    ("acc-load-out","    if (h == 0) {\n        M32(out) = 0;\n        return (uint32_t)-1;\n    }\n    n = (uint32_t)CopyAcceleratorTableA", "    if (h == 0) {\n        return (uint32_t)-1;\n    }\n    n = (uint32_t)CopyAcceleratorTableA"),
    ("acc-enable",  "    M32(t) = 1;\n    return 0;", "    M32(t) = 2;\n    return 0;"),
    ("acc-enable0", "static uint32_t FS98_STDCALL host_1e8(uint32_t id)\n{\n    uint32_t t;\n    if (id == 0)\n        return (uint32_t)-1;",
                    "static uint32_t FS98_STDCALL host_1e8(uint32_t id)\n{\n    uint32_t t;"),
    ("acc-clear",   "    host_214(t + 8);\n    M32(t + 0xc) = 0;\n    return 0;\n}", "    host_214(t + 8);\n    return 0;\n}"),
    ("acc-add-at",  "    host_21c(M32(t + 0xc) * 12 + M32(t + 8), entries, n * 12);", "    host_21c(M32(t + 8), entries, n * 12);"),
    ("acc-add-grow","        host_210(1, (M32(t + 0xc) + n) * 12, t + 8);", "        host_210(1, (M32(t + 0xc) + n) * 12 + 12, t + 8);",
                    "the array's spare capacity is never read; only count entries are"),
    ("acc-rm-match","                M32(e + 8) == M32(entries + 8)) {", "                1) {"),
    ("acc-rm-zero", "                host_218(M32(t + 0xc) * 12 + M32(t + 8) - 0xc, 0, 0xc);", "",
                    "the zeroed slots lie past the final count, which the closing realloc cuts to"),
    ("acc-rm-step", "                at -= 0xc;\n                removed++;\n                j--;", "                removed++;\n                j--;"),
    ("acc-rm-free", "    if (cnt == 0) {\n        host_214(t + 8);\n        return 0;\n    }", ""),
    ("acc-rm-len",  "                host_21c(e, M32(t + 8) + next, cnt * 12 - next);", "                host_21c(e, M32(t + 8) + next, (cnt - removed) * 12 - next);",
                    "the extra bytes moved are the stale tail, past the live count"),
    ("acc-read-past","    if (start > c) {", "    if (start >= c) {",
                    "start == count gives *n 0 and an empty copy either way"),
    ("acc-read-mode","    if (mode != 2)\n        return 0;", "    if (mode == 0)\n        return 0;"),
    ("acc-read-n",  "    M32(n) = c - start;", "    M32(n) = c;"),
    ("kn-ctrl-bit", "    if (mods & 2)\n        host_2b0(buf, KN_CTRL);", "    if (mods & 1)\n        host_2b0(buf, KN_CTRL);"),
    ("kn-order",    "    if (mods & 1)\n        host_2b0(buf, KN_SHIFT);\n    if (mods & 4)\n        host_2b0(buf, KN_ALT);",
                    "    if (mods & 4)\n        host_2b0(buf, KN_ALT);\n    if (mods & 1)\n        host_2b0(buf, KN_SHIFT);"),
    ("kn-vk0",      "    case 0x00: return 0;", "    case 0x00: return 1;"),
    ("kn-cancel",   "    case 0x03: case 0x13: w = KN_BREAK; break;", "    case 0x13: w = KN_BREAK; break;"),
    ("kn-lwin",     "    case 0x5b: w = KN_LWIN; break;", "    case 0x5b: w = KN_RWIN; break;"),
    ("kn-numenter", "    case 0x87: w = KN_NUMENTER; break;", "    case 0x88: w = KN_NUMENTER; break;"),
    ("kn-room",     "    return host_2ac(buf + n, w, len - n - 1);", "    return host_2ac(buf + n, w, len - n);"),
    ("kn-dontcare", "    uint32_t sc = MapVirtualKeyA(vk, 0) | 0x200;", "    uint32_t sc = MapVirtualKeyA(vk, 0);"),
    ("kn-parse-mods","        mods |= 4;\n        s += host_2b8(KN_ALT);", "        mods |= 4;"),
    ("kn-parse-ctrl","        mods = 2;\n        s += host_2b8(KN_CTRL);", "        mods = 3;\n        s += host_2b8(KN_CTRL);"),
    ("kn-parse-break","    if (host_key_word(s, KN_BREAK))\n        vk = 0x13;", "    if (host_key_word(s, KN_BREAK))\n        vk = 0x03;"),
    ("kn-parse-order","    else if (host_key_word(s, KN_NUMDIV))\n        vk = 0x6f;\n    else if (host_key_word(s, KN_NUMENTER))\n        vk = 0x87;",
                    "    else if (host_key_word(s, KN_NUMENTER))\n        vk = 0x87;\n    else if (host_key_word(s, KN_NUMDIV))\n        vk = 0x6f;"),
    ("kn-parse-case","            r = (uint32_t)_strcmpi((const char *)(uintptr_t)s, text);", "            r = (uint32_t)strcmp((const char *)(uintptr_t)s, text);"),
    ("kn-parse-none","        for (vk = 0; vk < 0x100; vk++) {", "        for (vk = 0; vk < 0xff; vk++) {"),
    ("kn-parse-ret","    M32(out) = (mods << 8) + vk;\n    return r;", "    M32(out) = (mods << 8) + vk;\n    return 0;"),
]


def main():
    results = []
    src = open("src/host/host.c").read()
    for entry in BREAKS:
        name, old, new = entry[:3]
        if ONLY and name not in ONLY:
            continue
        if src.count(old) != 1:
            print("%-16s cannot apply: %d matches" % (name, src.count(old)))
            results.append((name, "unapplied"))
            continue
        open(BREAK, "w").write(src.replace(old, new))
        if os.path.exists("bin/difftest/HOST_ours.dll"):
            os.remove("bin/difftest/HOST_ours.dll")
        build = subprocess.run(["make", "-s", "host-break-build", "HOST_BREAK=" + BREAK],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-16s does not build:\n%s" % (name, build.stderr[-1500:]))
            results.append((name, "unbuilt"))
            continue
        # A break that corrupts the heap can hang rather than differ; a hang
        # is a catch.
        run = subprocess.run(["timeout", "120", "wine", "hostdiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True, errors="replace",
                             env=dict(os.environ, WINEDEBUG="-all"))
        if run.returncode == 124:
            subprocess.run(["pkill", "-f", "hostdiff.exe"])
            run.stdout += "\nhostdiff: hung (timeout)"
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("hostdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-16s %s  %s%s" % (name, verdict, last[-1] if last else run.stdout.strip()[-120:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    if os.path.exists(BREAK):
        os.remove(BREAK)
    subprocess.run(["make", "-s", "hostdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("host: %d breaks, %d caught, %d equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
