#!/usr/bin/env python3
"""Break OpenFS98's MAIN on purpose, one place at a time, and check that
maindiff notices.

MAIN is done a slice at a time, so the breaks are grouped by the suite
that verifies them.  Each is a small edit to one file of a copy of
src/main -- the kind of slip a transliteration makes -- built as the
harness's MAIN_ours.dll and run for N scenarios of its suite (maindiff
exits non-zero when any differ).  At the end the real build goes back.  A
missed break is a gap in the harness or an equivalence; either way it is
reported, and docs/modules/main.md says which.

Usage: tools/main_breaks.py [N] [suite] [name...]   (from the repository
       root, after make maindiff)
"""
import os
import shutil
import subprocess
import sys

args = sys.argv[1:]
N = int(args.pop(0)) if args and args[0].isdigit() else 2000
SUITE = args.pop(0) if args else "records"
ONLY = args
COPY = "bin/difftest/main_break"

# suite -> [(name, file, old, new[, why it cannot show])].  A fifth element
# marks a break proven equivalent, reported as such.
BREAKS = {
    "records": [
        ("end-empty", "main_records.inc", "    if (M32(S_PENDING) == 0)\n        return 0;\n    H_ALLOC(1, M32(S_PENDING), ADDR(&zeros), 1);",
         "    if (M32(S_PENDING) == 0)\n        return 1;\n    H_ALLOC(1, M32(S_PENDING), ADDR(&zeros), 1);"),
        ("end-zero", "main_records.inc", "ADDR(&zeros), 1);", "ADDR(&zeros), 0);"),
        ("end-clear", "main_records.inc", "    r = H_FREE(ADDR(&zeros));\n    M32(S_PENDING) = 0;", "    r = H_FREE(ADDR(&zeros));"),
        ("current-none", "main_records.inc", "    if (M32(S_RECORD) == 0)\n        return 0xffffffffu;\n    M32(out)",
         "    if (M32(S_RECORD) == 0)\n        return 0;\n    M32(out)"),
        ("read-zero", "main_records.inc", "    if (take == 0 || n == 0) {", "    if (take == 0) {"),
        ("read-got", "main_records.inc", "        if (got != 0)\n            M32(got) = 0;\n        return 0xffffffffu;",
         "        return 0xffffffffu;"),
        ("read-clamp", "main_records.inc", "    if (take >= n)\n        take = n;", "    if (take > n + 1)\n        take = n;"),
        ("read-advance", "main_records.inc", "    M32(S_READ_AT) = M32(S_READ_AT) + take;\n", ""),
        ("read-left", "main_records.inc", "    M32(S_READ_LEFT) = M32(S_READ_LEFT) - take;\n", ""),
        ("dispatch-left", "main_records.inc", "    M32(S_READ_LEFT) = M32(rec + 8);", "    M32(S_READ_LEFT) = M32(rec + 4);"),
        ("dispatch-at", "main_records.inc", "    M32(S_READ_AT) = M32(rec + 0xc);", "    M32(S_READ_AT) = M32(rec + 0xc) + 1;"),
        ("dispatch-clear", "main_records.inc", "    r = CHAIN_SEND(0x14, 0x22, rec);\n    M32(S_RECORD) = 0;", "    r = CHAIN_SEND(0x14, 0x22, rec);"),
        ("dispatch-message", "main_records.inc", "CHAIN_SEND(0x14, 0x22, rec)", "CHAIN_SEND(0x14, 0x22, 0)"),
        ("save-pad", "main_records.inc", "    CHAIN_SEND(0x14, 0x23, 0);\n    r = record_end();", "    CHAIN_SEND(0x14, 0x23, 0);\n    r = 0;"),
        ("save-clear", "main_records.inc", "    r = record_end();\n    M32(S_FILE) = 0;", "    r = record_end();"),
        ("begin-size", "main_records.inc", "    if (M32(S_FILE) == 0 || want == 0)", "    if (M32(S_FILE) == 0)"),
        ("begin-end", "main_records.inc", "        return 0xffffffffu;\n    record_end();\n    word = tag;", "        return 0xffffffffu;\n    word = tag;"),
        ("begin-small", "main_records.inc", "    if ((int32_t)tag < 1000) {", "    if ((int32_t)tag <= 1000) {"),
        ("begin-signed", "main_records.inc", "    if ((int32_t)tag < 1000) {", "    if (tag < 1000) {"),
        ("begin-short-len", "main_records.inc", "        word = want + 4;", "        word = want + 2;"),
        ("begin-long-len", "main_records.inc", "        word = want + 6;", "        word = want + 4;"),
        ("begin-long-width", "main_records.inc", "ADDR(&word), 4, ADDR(&size));", "ADDR(&word), 2, ADDR(&size));"),
        ("begin-pending", "main_records.inc", "    M32(S_PENDING) = want;", "    M32(S_PENDING) = size;"),
        ("write-none", "main_records.inc", "    if (M32(S_FILE) == 0 || owed == 0) {", "    if (owed == 0) {",
         "+0x9760 is set non-zero only by +0x5c, which needs a file, and the save zeroes it before it clears the file"),
        ("write-clamp", "main_records.inc", "    if (n < owed) {", "    if (n <= owed) {",
         "at n == owed both branches leave nothing owed and n as it was"),
        ("write-count", "main_records.inc", "        M32(wrote) = done;", "        M32(wrote) = n;"),
        ("find-size-zero", "main_records.inc", "    if (size != 0)\n        M32(size) = 0;\n    M32(out) = 0;", "    M32(out) = 0;"),
        ("find-ext", "main_records.inc", "    if (H_EXISTS(path, 2) != 0)", "    if (H_EXISTS(path, 0) != 0)"),
        ("find-open-mode", "main_records.inc", "H_FILE_OPEN(ADDR(&file), path, 2, 0)", "H_FILE_OPEN(ADDR(&file), path, 2, 1)"),
        ("find-small", "main_records.inc", "            if ((int32_t)id < 1000) {", "            if ((int32_t)id < 999) {"),
        ("find-header", "main_records.inc", "M8(buf + at) - 4;", "M8(buf + at) - 2;"),
        ("find-end", "main_records.inc", "            } else if (id == 0xffff) {", "            } else if (id == 0xfffe) {"),
        ("find-long", "main_records.inc", "                         M8(buf + at + 1)) << 8) + M8(buf + at) - 6;", "                         M8(buf + at + 1)) << 8) + M8(buf + at) - 4;"),
        ("find-long-step", "main_records.inc", "                at += 4;", "                at += 2;"),
        ("find-copy-at", "main_records.inc", "H_MEMMOVE(M32(out), buf + at, body);", "H_MEMMOVE(M32(out), buf + at - 2, body);"),
        ("find-size-out", "main_records.inc", "                if (size != 0)\n                    M32(size) = body;\n", ""),
        ("find-step", "main_records.inc", "            at += body;\n        } while (at < length);", "            at += body + 1;\n        } while (at < length);"),
        ("find-bound", "main_records.inc", "        } while (at < length);", "        } while ((int32_t)at < (int32_t)length);"),
        ("find-free", "main_records.inc", "    H_FREE(ADDR(&buf));\n    return 0xffffffffu;\n}", "    return 0xffffffffu;\n}"),
        ("flight-path", "main_records.inc", "#define S_FIND_PATH   V(0x208040dc)", "#define S_FIND_PATH   V(0x208040dd)"),
    ],
    "situations": [
        ("cb59-byte", "main_situation.inc", "if ((M16(S_TAG_59) & 0xff) == 0)", "if ((M16(S_TAG_59) & 0xffff) == 0)"),
        ("cb59-field", "main_situation.inc", "            M32(G + 0xc54) = 1;\n        else", "            M32(G + 0xc58) = 1;\n        else"),
        ("cb92-save", "main_situation.inc", "        M32(S_CB_C68) = M8(G + 0xc68);", "        M32(S_CB_C68) = M16(G + 0xc68);"),
        ("cb93-load", "main_situation.inc", "        M32(G + 0xc54) = M32(S_CB_C58);", "        M32(G + 0xc58) = M32(S_CB_C58);"),
        ("cb35-value", "main_situation.inc", "        if (v != 0)\n            v = 0x3fff;\n        M32(G + 0xbd0) = v;", "        if (v != 0)\n            v = 0x3ffe;\n        M32(G + 0xbd0) = v;"),
        ("cb5e-shift", "main_situation.inc", "        v = (M32(G + 0xbdc) & 0xffff) >> 1;", "        v = (M32(G + 0xbdc) & 0xffff) >> 2;"),
        ("cb5f-mask", "main_situation.inc", "        v = M32(G + 0xbe8) & 0xff;", "        v = M32(G + 0xbe8) & 0xffff;"),
        ("cb5f-four", "main_situation.inc", "        M32(G + 0xbf0) = v;\n        M32(G + 0xbf4) = v;", "        M32(G + 0xbf0) = v;"),
        ("call-map", "main_situation.inc", "    case 0x207f2dbc: return cb_5e(tag, saving);", "    case 0x207f2dbc: return cb_5f(tag, saving);"),
        ("field-81", "main_situation.inc", "    if (code == 0x81) return 2;", "    if (code == 0x81) return 1;"),
        ("field-83", "main_situation.inc", "    if (code == 0x83) return 3;", "    if (code == 0x83) return 4;"),
        ("field-low", "main_situation.inc", "    return code & 0x7f;", "    return code & 0x3f;",
         "every size code in retail's table is 0x80..0x83 or 0x20, which both masks keep"),
        ("checked-mode", "main_situation.inc", "    if (E_MODE(e) == 0xa)\n        return 0;", "    if (E_MODE(e) == 0xb)\n        return 0;"),
        ("checked-code", "main_situation.inc", "    return write_raw(src, n) != n ? 0x1001 : 0;", "    return write_raw(src, n) != n ? 0x1002 : 0;"),
        ("write-callback", "main_situation.inc", "            r = table_call(E_CALL(e), E_TAG(e), 1);", "            r = table_call(E_CALL(e), E_TAG(e), 0);"),
        ("write-hdr", "main_situation.inc", "        hdr = header.tag < 1000 ? 4 : 6;", "        hdr = header.tag <= 1000 ? 4 : 6;",
         "no tag in retail's table is 1000"),
        ("write-len", "main_situation.inc", "        header.length = hdr + entry_size(e);", "        header.length = entry_size(e);"),
        ("write-kind5", "main_situation.inc", "            if (E_KIND(e) == 5)\n                base = M32(at);\n", "",
         "no entry of retail's table has kind 5"),
        ("write-base", "main_situation.inc", "        base = G;\n        if (E_KIND(e) != 0) {", "        base = G;\n        if (E_KIND(e) > 1) {"),
        ("write-next", "main_situation.inc", "            r = write_fields(e, base, E_NEXT(e));\n            e = E_NEXT(e);", "            r = write_fields(e, base, E_NEXT(e));\n            e = E_NEXT(e) + 0x12;"),
        ("write-records", "main_situation.inc", "    save_records(M32(S_SAVE_FILE));\n", ""),
        ("write-end", "main_situation.inc", "    header.tag = 0xffff;", "    header.tag = 0xfffe;"),
        ("save-b20", "main_situation.inc", "    M32(S_OLD_B20) = M32(G + 0xb20);", "    M32(S_OLD_B20) = M32(G + 0xb24);"),
        ("save-title", "main_situation.inc", "    H_STRNCPY(S_TITLE, title, 0x1e);", "    H_STRNCPY(S_TITLE, title, 0x1f);"),
        ("save-desc-size", "main_situation.inc", "    M16(S_TAG2_SIZE) = 0x100;", "    M16(S_TAG2_SIZE) = 0xff;"),
        ("save-aircraft-len", "main_situation.inc", "    M16(S_TAG61_SIZE) = (uint16_t)(H_STRLEN(S_AIRCRAFT) + 1);", "    M16(S_TAG61_SIZE) = (uint16_t)(H_STRLEN(S_AIRCRAFT));"),
        ("save-dir", "main_situation.inc", "    H_LOADSTRING(0x9002, ADDR(dir), 0x100);", "    H_LOADSTRING(0x9004, ADDR(dir), 0x100);"),
        ("save-close", "main_situation.inc", "    H_FILE_CLOSE(M32(S_SAVE_FILE));\n    M32(S_SAVE_FILE) = 0;", "    M32(S_SAVE_FILE) = 0;"),
        ("save-current", "main_situation.inc", "    H_STRNCPY(S_FLIGHT + 4, ADDR(path), 0x103);", "    H_STRNCPY(S_FLIGHT + 4, ADDR(path), 0x104);"),
        ("before-chain", "main_situation.inc", "    CHAIN_SEND(0x14, 0x20, 0);", "    CHAIN_SEND(0x14, 0x21, 0);"),
        ("before-keep", "main_situation.inc", "    M32(S_KEEP_E) = 1;", "    M32(S_KEEP_E) = 0;"),
        ("before-5f4", "main_situation.inc", "    M32(G + 0x5f4) = 0xffffffffu;", "    M32(G + 0x5f4) = 0;"),
        ("take-c", "main_situation.inc", "        if (M32(G + 0x1248) == 0) return;", "        if (M32(G + 0x124c) == 0) return;"),
        ("take-e", "main_situation.inc", "        M32(S_KEEP_E) = 0;\n        break;", "        break;",
         "no entry of retail's table has mode 0xe"),
        ("take-b", "main_situation.inc", "    case 0x0: case 0xa: case 0xb:", "    case 0x0: case 0xa:"),
        ("parse-small", "main_situation.inc", "        if (tag < 1000) {\n            length = M16(at);", "        if (tag < 999) {\n            length = M16(at);"),
        ("parse-body", "main_situation.inc", "        body = length - hdr;", "        body = length - 4;"),
        ("parse-skip", "main_situation.inc", "            M32(S_CURSOR) = M32(S_CURSOR) + body;\n            continue;", "            M32(S_CURSOR) = M32(S_CURSOR) + length;\n            continue;"),
        ("parse-next", "main_situation.inc", "            e = E_SIZE(e) == 0xffff ? E_NEXT(e) : e + 0x12;", "            e = e + 0x12;"),
        ("parse-kind1", "main_situation.inc", "        case 1:\n            base = 0;\n            break;", "        case 1:\n            base = G;\n            break;"),
        ("parse-varlen", "main_situation.inc", "            n = M32(S_REC + 8);", "            n = M32(S_REC + 4);"),
        ("parse-advance", "main_situation.inc", "            take(e, base + dst, n);\n            M32(S_CURSOR) = M32(S_CURSOR) + n;", "            take(e, base + dst, n);\n            M32(S_CURSOR) = M32(S_CURSOR) + M32(S_REC + 8);"),
        ("parse-callback", "main_situation.inc", "            table_call(E_CALL(e), E_TAG(e), 0);", "            table_call(E_CALL(e), E_TAG(e), 1);"),
        ("season-table", "main_situation.inc", "        a = SEASON_TABLE + 2 * (uint32_t)M16(G + 0x248);", "        a = SEASON_TABLE + 2 * (uint32_t)M8(G + 0x248);"),
        ("season-year", "main_situation.inc", "        M16(G + 0x240) = now[0];", "        M16(G + 0x240) = now[1];"),
        ("season-call", "main_situation.inc", "    MAIN_74(1, M16(G + 0x23e));", "    MAIN_74(0, M16(G + 0x23e));"),
        ("clouds-value", "main_situation.inc", "            M32(G + fields[i]) = 0x7fffff;", "            M32(G + fields[i]) = 0x7ffff0;"),
        ("after-c00", "main_situation.inc", "    if (w != 0)\n        M16(G + 0xc00) = w;", "    M16(G + 0xc00) = w;"),
        ("after-5f4", "main_situation.inc", "    else if (M32(G + 0x5f4) >= 4)", "    else if (M32(G + 0x5f4) > 4)",
         "GLOBAL +0x5f4 is set to -1 before every load and only its low byte is loaded (record 0x87's one field is a byte), so it is never 4"),
        ("after-clamp", "main_situation.inc", "    if (M16(G + 0x1074) > 0x7fff || M16(G + 0x1074) == 0)", "    if (M16(G + 0x1074) > 0x7fff)"),
        ("after-controls", "main_situation.inc", "    CALL(CONTROLS, 0x38, f2)(0, 1);", "    CALL(CONTROLS, 0x38, f2)(1, 0);"),
        ("after-100", "main_situation.inc", "    if (M16(G + 0xc44) > 100)", "    if (M16(G + 0xc44) >= 100)",
         "at exactly 100 both store 100"),
        ("after-aircraft", "main_situation.inc", "    if (M32(G + 0x1244) == 0)\n        CALL(ACONTAIN", "    if (M32(G + 0x1244) != 0)\n        CALL(ACONTAIN"),
        ("after-84c", "main_situation.inc", "    if (M32(G + 0x850) == 0)\n        M32(G + 0x84c) = 0;", "    M32(G + 0x84c) = 0;"),
        ("after-354", "main_situation.inc", "    if (M16(G + 0x354) & 0x7777)", "    if (M16(G + 0x354) & 0x7fff)"),
        ("after-e64", "main_situation.inc", "    if (M16(G + 0xe64) != 2 && M16(G + 0xe64) != 4)", "    if (M16(G + 0xe64) != 2)"),
        ("after-post", "main_situation.inc", "    if (M16(G + 0x5dc) != 0)\n        H_POST(0x100c3, 0);", "    if (M16(G + 0x5dc) == 0)\n        H_POST(0x100c3, 0);"),
        ("after-b20", "main_situation.inc", "    if (M32(G + 0x1214) == 2)\n        M32(G + 0xb20) = M32(S_OLD_B20);", "    if (M32(G + 0x1214) != 2)\n        M32(G + 0xb20) = M32(S_OLD_B20);"),
        ("after-365", "main_situation.inc", "    M8(G + 0x365) = 0xf;", "    M8(G + 0x365) = 0xe;"),
        ("load-366", "main_situation.inc", "    M8(G + 0x366) = 0;\n    before_load();", "    before_load();"),
        ("current-mode", "main_situation.inc", "S_LOADING + 4, M32(S_LOADING), 0);", "S_LOADING + 4, M32(S_LOADING), 1);"),
        ("current-zero", "main_situation.inc", "    H_ALLOC(1, size, ADDR(&buf), 1);\n    H_FILE_READ(M32(S_LOAD_FILE)", "    H_ALLOC(1, size, ADDR(&buf), 0);\n    H_FILE_READ(M32(S_LOAD_FILE)"),
        ("reload-alert", "main_situation.inc", "    UTIL_ALERT(1, 1, ADDR(text), 0, 0, 0, 0, 0);", "    UTIL_ALERT(1, 0, ADDR(text), 0, 0, 0, 0, 0);"),
        ("default-ext", "main_situation.inc", "    if (H_EXISTS(G + 0x24, 2) == 0) {", "    if (H_EXISTS(G + 0x24, 0) == 0) {"),
        ("default-name", "main_situation.inc", "    H_LOADSTRING(0x9016, ADDR(name), 0x100);", "    H_LOADSTRING(0x9002, ADDR(name), 0x100);"),
        ("default-flight-ext", "main_situation.inc", "    f.ext = 2;", "    f.ext = 3;"),
        ("flight-weather", "main_situation.inc", "    return CALL(WEATHERM, 0x24, f0)();", "    CALL(WEATHERM, 0x24, f0)();\n    return 0;"),
        ("flight-set", "main_situation.inc", "        set_flight(f);\n        reload_flight();", "        reload_flight();"),
        ("file-free", "main_situation.inc", "    load_records();\n    return H_FREE(ADDR(&buf));", "    load_records();\n    return 0;"),
    ],
    "nav": [
        ("z-borrow", "main_nav.inc", "M32(to + 4) - M32(from + 4) - borrow", "M32(to + 4) - M32(from + 4)"),
        ("z-swap", "main_nav.inc", "z = (int32_t)(M32(to + 4) - M32(from + 4) - borrow);", "z = (int32_t)(M32(from + 4) - M32(to + 4) - borrow);"),
        ("sentinel", "main_nav.inc", "0xffffd8f1u", "0xffffd8f0u"),
        ("sar8", "main_nav.inc", "e = (int32_t)M32(gp + 0x20) >> 8;", "e = (int32_t)M32(gp + 0x20) >> 4;"),
        ("y-neg", "main_nav.inc", "y = -e;", "y = e;",
         "out.y reaches nothing but its own square in the magnitude, and the bearing uses only out.x and out.z, so its sign is never observed"),
        ("y-field", "main_nav.inc", "int32_t e = (int32_t)M32(from + 0x14);", "int32_t e = (int32_t)M32(to + 0x14);"),
        ("y-to", "main_nav.inc", "e -= (int32_t)M32(to + 0x14);", "e -= (int32_t)M32(from + 0x14);"),
        ("x-latdelta", "main_nav.inc", "*(uint32_t *)(latbuf + 4) = M32(to + 0xc) - M32(from + 0xc) - borrow;", "*(uint32_t *)(latbuf + 4) = M32(to + 0xc) - M32(from + 0xc);"),
        ("sum-y", "main_nav.inc", "sum += (int64_t)y * y;", "sum += (int64_t)y;"),
        ("sum-z", "main_nav.inc", "    sum += (int64_t)z * z;\n", ""),
        ("scale-x2", "main_nav.inc", "x = (int32_t)((uint32_t)x << 1);", "x = (int32_t)((uint32_t)x << 2);"),
        ("bearing-args", "main_nav.inc", "nav_bearing((int32_t)out[0], (int32_t)out[2]);", "nav_bearing((int32_t)out[0], (int32_t)out[1]);"),
        ("range-72e0", "main_nav.inc", "M32(V(0x208072e0)) = range;", "M32(V(0x208072e4)) = range;"),
        ("range-out", "main_nav.inc", "    M32(range_out) = range;\n", ""),
    ],
    "navio": [
        ("readout-which1", "main_nav.inc", "    if (which == 1) {", "    if (which == 0) {"),
        ("readout-1-word", "main_nav.inc", "M16(out) = M16(V(0x20807482));", "M16(out) = M16(V(0x20807480));"),
        ("readout-2-word", "main_nav.inc", "M16(out) = M16(V(0x20807484));", "M16(out) = M16(V(0x20807486));"),
        ("readout-miss", "main_nav.inc", "    }\n    return 0xffffffffu;\n}", "    }\n    return 0;\n}"),
        ("select-e8-read", "main_nav.inc", "M32(out) = M32(V(0x208072e8));", "M32(out) = M32(V(0x208072ec));"),
        ("select-ec-read", "main_nav.inc", "M32(out) = M32(V(0x208072ec));", "M32(out) = M32(V(0x208072e8));"),
        ("select-f0", "main_nav.inc", "M32(out) = M32(V(0x208072f0));", "M32(out) = M32(V(0x208072f4));"),
        ("select-f4", "main_nav.inc", "M32(out) = M32(V(0x208072f4));", "M32(out) = M32(V(0x208072f0));"),
        ("select-slot", "main_nav.inc", "CALL(MULTIP, 0x38, f1)(V(0x208072e8))", "CALL(MULTIP, 0x34, f1)(V(0x208072e8))"),
        ("select-arg", "main_nav.inc", "CALL(MULTIP, 0x34, f1)(V(0x208072ec))", "CALL(MULTIP, 0x34, f1)(V(0x208072e8))"),
        ("select-fail", "main_nav.inc", "if (CALL(MULTIP, 0x38, f1)(V(0x208072e8)) != 0)", "if (CALL(MULTIP, 0x38, f1)(V(0x208072e8)) == 0)"),
        ("select-default", "main_nav.inc", "    default:\n        return 0;", "    default:\n        return 0xffffffffu;"),
    ],
    "navdesc": [
        ("fail-check", "main_nav.inc", "CALL(FE, 0x2c, f3)(which, arg2, ADDR(buf)) == 0", "CALL(FE, 0x2c, f3)(which, arg2, ADDR(buf)) != 0"),
        ("fe-swap", "main_nav.inc", "CALL(FE, 0x2c, f3)(which, arg2, ADDR(buf))", "CALL(FE, 0x2c, f3)(arg2, which, ADDR(buf))"),
        ("memset-n", "main_nav.inc", "H_MEMSET(out, 0, 0x58);", "H_MEMSET(out, 0, 0x54);"),
        ("fail-ret", "main_nav.inc", "H_MEMSET(out, 0, 0x58);\n        return 0xffffffffu;", "H_MEMSET(out, 0, 0x58);\n        return 0;"),
        ("desc-0", "main_nav.inc", "    desc[0] = 3;", "    desc[0] = 2;"),
        ("desc-1", "main_nav.inc", "    desc[1] = 3;", "    desc[1] = 4;"),
        ("desc-2", "main_nav.inc", "    desc[2] = 6;", "    desc[2] = 7;"),
        ("desc-dest", "main_nav.inc", "    desc[3] = V(0x20807248);", "    desc[3] = V(0x2080724c);"),
        ("mm-src", "main_nav.inc", "H_MEMMOVE(out, V(0x20807248), 0x58)", "H_MEMMOVE(out, V(0x2080724c), 0x58)"),
    ],
    "season": [
        ("zone-div", "main_season.inc", "/ 0x555u", "/ 0x554u"),
        ("zone-cmp", "main_season.inc", "if (ax >= 0x17)", "if (ax >= 0x18)"),
        ("zone-sub", "main_season.inc", "ax = (uint16_t)(ax - 0x30);", "ax = (uint16_t)(ax - 0x2f);"),
        ("zone-sar", "main_season.inc", "(int16_t)ax >> 1", "(int16_t)ax >> 2"),
        ("zone-mul", "main_season.inc", "ax = (uint16_t)(ax * 0x3cu);", "ax = (uint16_t)(ax * 0x3du);"),
        ("rec-dchar", "main_season.inc", "if (M8(rec) == 0x64) {", "if (M8(rec) == 0x65) {"),
        ("rec-off", "main_season.inc", "M16(S_TZ_MIN) = M16(rec + 0xe);", "M16(S_TZ_MIN) = M16(rec + 0xc);"),
        ("rec-prio-store", "main_season.inc", "M8(S_TZ_PRIO) = M8(rec + 0x10);", "M8(S_TZ_PRIO) = M8(rec + 0x11);"),
        ("rec-dst-store", "main_season.inc", "M8(S_TZ_DST) = M8(rec + 0x11);", "M8(S_TZ_DST) = M8(rec + 0x10);"),
        ("rec-prio-ge", "main_season.inc", "if (M8(rec + 0x10) > M8(S_TZ_PRIO)) {", "if (M8(rec + 0x10) >= M8(S_TZ_PRIO)) {"),
        ("dst-bound", "main_season.inc", "(int8_t)dst <= 5", "(int8_t)dst <= 6",
         "the sixth table entry (start 3, end 0) is an empty window, so a daylight rule of 6 leaves the time unchanged either way"),
        ("dst-lo", "main_season.inc", "(int16_t)day >= (int16_t)dst_word(e)", "(int16_t)day > (int16_t)dst_word(e)"),
        ("dst-hi", "main_season.inc", "(int16_t)day < (int16_t)dst_word(e + 2)", "(int16_t)day <= (int16_t)dst_word(e + 2)"),
        ("dst-sub", "main_season.inc", "M16(S_TZ_MIN) - dst_word(e + 4)", "M16(S_TZ_MIN) + dst_word(e + 4)"),
        ("dst-active", "main_season.inc", "M16(gp + 0x244) = 1;", "M16(gp + 0x244) = 0;"),
        ("off-246", "main_season.inc", "M16(gp + 0x246) = M16(S_TZ_MIN);", "M16(gp + 0x248) = M16(S_TZ_MIN);"),
        ("clock-a", "main_season.inc", "M32(S_HAVE_D) != 0 && a == 0", "M32(S_HAVE_D) != 0 && a == 1"),
        ("clock-haved", "main_season.inc", "if (M32(S_HAVE_D) != 0 && a == 0) {", "if (M32(S_HAVE_D) == 0 && a == 0) {"),
        ("clock-localsrc", "main_season.inc", "0x3cu * M8(gp + 0x23b)", "0x3cu * M8(gp + 0x238)"),
        ("clock-sub", "main_season.inc", "m = (uint16_t)(m - M16(gp + 0x246));", "m = (uint16_t)(m + M16(gp + 0x246));"),
        ("clock-store", "main_season.inc", "M8(gp + 0x238) = (uint8_t)m;", "M8(gp + 0x238) = (uint8_t)(m >> 8);"),
        ("hm-mod", "main_season.inc", "(ax % 60u)", "(ax % 59u)"),
        ("found-d", "main_season.inc", "M32(S_FOUND_D) = 1;", "M32(S_FOUND_D) = 0;"),
        ("snap-56c", "main_season.inc", "M32(buf + 4) = M32(gp + 0x56c);", "M32(buf + 4) = M32(gp + 0x560);"),
    ],
    "clock": [
        ("src-26", "main_season.inc", "clock_digits(M8(gp + 0x23a))", "clock_digits(M8(gp + 0x239))"),
        ("dst-26", "main_season.inc", "M16(gp + 0xc26) =", "M16(gp + 0xc27) ="),
        ("src-20", "main_season.inc", "clock_digits(M8(gp + 0x238))", "clock_digits(M8(gp + 0x23a))"),
        ("tens", "main_season.inc", "(v / 10) + 0x30", "(v / 100) + 0x30"),
        ("units", "main_season.inc", "(v % 10) + 0x30", "(v % 100) + 0x30"),
        ("ascii", "main_season.inc", "+ 0x30) << 8", "+ 0x31) << 8"),
    ],
    "view": [
        ("active-sign", "main_nav.inc", "M32(M32(V(0x20807fc4)) + 8) >> 31", "M32(M32(V(0x20807fc4)) + 8) >> 30"),
        ("active-field", "main_nav.inc", "M32(M32(V(0x20807fc4)) + 8)", "M32(M32(V(0x20807fc4)) + 0xc)"),
        ("active-ptr", "main_nav.inc", "M32(M32(V(0x20807fc4)) + 8) >> 31", "M32(M32(V(0x20807fc8)) + 8) >> 31"),
        ("start-swap", "main_nav.inc", "view_active() ? M32(V(0x20807fc8)) : 0xffffffffu", "view_active() ? 0xffffffffu : M32(V(0x20807fc8))"),
        ("inc", "main_nav.inc", "i = i + 1;", "i = i + 2;"),
        ("limit-addr", "main_nav.inc", "if (i >= M32(V(0x2080fda4)))", "if (i >= M32(V(0x2080fda8)))"),
        ("store-addr", "main_nav.inc", "M32(V(0x20807fc8)) = i;", "M32(V(0x20807fca)) = i;"),
        ("post-1625", "main_nav.inc", "CALL(HOST, 0x3c, f2)(0x1625, 0)", "CALL(HOST, 0x3c, f2)(0x1624, 0)"),
        ("post-300", "main_nav.inc", "i + 0x300", "i + 0x301"),
        ("post-cond", "main_nav.inc", "if (i == 0xffffffffu)", "if (i != 0xffffffffu)"),
    ],
    "menu": [
        ("start", "main_nav.inc", "for (id = 0x1001; id <= 0x1006; id++)", "for (id = 0x1002; id <= 0x1006; id++)"),
        ("end", "main_nav.inc", "id <= 0x1006", "id <= 0x1005"),
        ("state", "main_nav.inc", "CALL(HOST, 0x254, f2)(id, 2);", "CALL(HOST, 0x254, f2)(id, 3);"),
        ("slot", "main_nav.inc", "CALL(HOST, 0x254, f2)(id, 2);", "CALL(HOST, 0x258, f2)(id, 2);"),
        ("enable-state", "main_nav.inc", "CALL(HOST, 0x254, f2)(id, 0);", "CALL(HOST, 0x254, f2)(id, 1);"),
        ("enable-end", "main_nav.inc", "id < 0x1007", "id < 0x1006"),
    ],
    "obj": [
        ("stride", "main_nav.inc", "id * 181u * 4u", "id * 180u * 4u"),
        ("base", "main_nav.inc", "V(0x20809794)", "V(0x20809798)"),
        ("active-check", "main_nav.inc", "if (M32(a) != 0)", "if (M32(a) == 0)"),
        ("clear-slot", "main_nav.inc", "M32(a) = 0;", "M32(a) = 1;"),
        ("clear-handle", "main_nav.inc", "M32(handle) = 0;", "M32(handle) = 1;"),
        ("ret", "main_nav.inc", "return id * 181u;", "return id * 180u;"),
    ],
    "clamp": [
        ("max-const", "main_nav.inc", "> 0x97c449)", "> 0x97c44a)"),
        ("max-set", "main_nav.inc", "M32(gp + 0x564) = 0x97c449u;", "M32(gp + 0x564) = 0x97c448u;"),
        ("min-const", "main_nav.inc", "< (int32_t)0xff683bb7u)", "< (int32_t)0xff683bb6u)"),
        ("min-set", "main_nav.inc", "M32(gp + 0x564) = 0xff683bb7u;", "M32(gp + 0x564) = 0xff683bb8u;"),
        ("max-op", "main_nav.inc", "(int32_t)M32(gp + 0x564) > 0x97c449", "(int32_t)M32(gp + 0x564) >= 0x97c449",
         "at exactly 0x97c449 the one branch leaves it and the other sets it to 0x97c449, the same value"),
        ("field", "main_nav.inc", "if ((int32_t)M32(gp + 0x564) > 0x97c449)", "if ((int32_t)M32(gp + 0x568) > 0x97c449)"),
    ],
    "objalloc": [
        ("full-bound", "main_nav.inc", "if (index >= 20)", "if (index >= 21)"),
        ("flag-check", "main_nav.inc", "if ((M32(handle) & 0xf0000) &&", "if ((M32(handle) & 0xe0000) &&"),
        ("fast-id", "main_nav.inc", "M32(V(0x20809794) + (M32(handle) & 0xff) * 181u * 4u) == 1)",
         "M32(V(0x20809794) + (M32(handle) & 0xff) * 181u * 2u) == 1)"),
        ("kind1-field", "main_nav.inc", "M16(V(0x2080979c) + ent) = 0x254;", "M16(V(0x2080979c) + ent) = 0x255;"),
        ("kind0-field", "main_nav.inc", "M16(V(0x2080979c) + ent) = 0x950;", "M16(V(0x2080979c) + ent) = 0x951;"),
        ("kind-else", "main_nav.inc", "    } else if (kind == 0) {", "    } else if (kind <= 2) {"),
        ("active-mark", "main_nav.inc", "M32(V(0x20809794) + ent) = 1;", "M32(V(0x20809794) + ent) = 3;"),
        ("handle-out", "main_nav.inc", "M32(handle) = index | 0xf0000u;", "M32(handle) = index | 0xe0000u;"),
        ("tmpl-count", "main_nav.inc", "    for (i = 0; i < 9; i++)", "    for (i = 0; i < 8; i++)"),
        ("subbuf-count", "main_nav.inc", "for (i = 1; i < 0xf; i++) {", "for (i = 1; i < 0xe; i++) {"),
        ("sub-src", "main_nav.inc", "H_MEMMOVE(M32(V(0x20809a00) + ent + i * 4u), buf, 0xaa);",
         "H_MEMMOVE(M32(V(0x20809a00) + ent + i * 4u), buf + 1, 0xaa);"),
        ("kind-template", "main_nav.inc", "M32(V(0x208062f0) + kind * 4u)", "M32(V(0x208062f0) + (kind + 1) * 4u)"),
        ("first-fill", "main_nav.inc", "H_MEMMOVE(buf, V(0x20806240), 0xaa);", "H_MEMMOVE(buf, V(0x20806240), 0xa9);"),
        ("scratch", "main_nav.inc", "M32(V(0x2080d140)) = buf + 0x42;", "M32(V(0x2080d140)) = buf + 0x43;"),
        ("fast-eq", "main_nav.inc", "* 181u * 4u) == 1)\n        return 0;", "* 181u * 4u) != 0)\n        return 0;",
         "the table's active word is only ever 0 or 1, so == 1 and != 0 choose the fast path on exactly the same entries"),
    ],
    "eye": [
        ("box-ptr", "main_eye.inc", "uint32_t box = M32(view + 0x1c);", "uint32_t box = M32(view + 0x20);"),
        ("ctx", "main_eye.inc", "uint32_t ctx = view + 0x80;", "uint32_t ctx = view + 0x81;"),
        ("gate-g14", "main_eye.inc", "M32(box + 0x14) != 0 ||", "M32(box + 0x14) == 0 ||"),
        ("gate-g18", "main_eye.inc", "M32(box + 0x18) != 0)", "M32(box + 0x18) == 0)"),
        ("stamp", "main_eye.inc", "M32(gp + 0x2ec) = 0xf00f;", "M32(gp + 0x2ec) = 0xf00e;"),
        ("box-shift", "main_eye.inc", "d = (int32_t)((uint32_t)extent >> 6);", "d = (int32_t)((uint32_t)extent >> 7);"),
        ("box-seg1", "main_eye.inc", "CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)(lo - d), (uint32_t)hi);",
         "CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)(lo + d), (uint32_t)hi);"),
        ("cross-shift2", "main_eye.inc", "? (int32_t)((uint32_t)extent >> 5)", "? (int32_t)((uint32_t)extent >> 6)"),
        ("cross-shift3", "main_eye.inc", ": (int32_t)((uint32_t)extent >> 4);", ": (int32_t)((uint32_t)extent >> 3);"),
        ("cross-2d", "main_eye.inc", "(uint32_t)(lo - 2 * d), (uint32_t)hi, (uint32_t)(lo - d)",
         "(uint32_t)(lo - 3 * d), (uint32_t)hi, (uint32_t)(lo - d)"),
        ("cross-last", "main_eye.inc", "(uint32_t)(lo + 2 * d), (uint32_t)hi);", "(uint32_t)(lo + 3 * d), (uint32_t)hi);"),
        ("style3", "main_eye.inc", "    case 2:\n    case 3:", "    case 2:"),
        ("done-eq", "main_eye.inc", "if (M32(box + 0x10) == 1 ||", "if (M32(box + 0x10) != 0 ||",
         "the box's done flag is only ever 0 or 1, so == 1 and != 0 gate on the same boxes"),
    ],
    "sun": [
        ("z-neg", "main_sun.inc", "M16(gp + 0xb32) = (uint16_t)(-z);", "M16(gp + 0xb32) = (uint16_t)z;"),
        ("x-neg", "main_sun.inc", "M16(gp + 0xb2a) = (uint16_t)(-x);", "M16(gp + 0xb2a) = (uint16_t)x;"),
        ("y-sub", "main_sun.inc", "(int16_t)M16(SUN_T25)) - t);", "(int16_t)M16(SUN_T25)) + t);"),
        ("day-angle", "main_sun.inc", "ax = (uint16_t)(ax + 0x650);", "ax = (uint16_t)(ax + 0x651);"),
        ("lat-offset", "main_sun.inc", "bx = (uint16_t)(ax + 0xc000);", "bx = (uint16_t)(ax + 0xc001);"),
        ("flag-init", "main_sun.inc", "M16(gp + 0xb34) = 1;", "M16(gp + 0xb34) = 0;"),
        ("band2", "main_sun.inc", "M16(gp + 0x116e) = 2;", "M16(gp + 0x116e) = 3;"),
        ("cba-min", "main_sun.inc", "M16(gp + 0xb54) != 0 ? 0x1c00 : 0x1000;", "M16(gp + 0xb54) != 0 ? 0x1c00 : 0x1001;"),
        ("light-shr", "main_sun.inc", "M16(gp + 0x12a0) = (uint16_t)(ax >> 5);", "M16(gp + 0x12a0) = (uint16_t)(ax >> 4);"),
        ("gate", "main_sun.inc", "if (M16(SUN_GATE) < 0x1000)", "if (M16(SUN_GATE) <= 0x1000)"),
        ("div-repair", "main_sun.inc", "return q > 0xffff ? 0xffff : (uint16_t)q;", "return q > 0xffff ? 0xfffe : (uint16_t)q;"),
        ("idiv-sat", "main_sun.inc", "return neg ? (int16_t)0x8001 : (int16_t)0x7fff;", "return neg ? (int16_t)0x8001 : (int16_t)0x7ffe;",
         "both idiv sites discard the low bits of the quotient (block 4 by sar 1 into an angle, the helper by shr 5), so the two low-order saturation values cannot be told apart; the sign still shows, which idiv-neg catches"),
        ("idiv-neg", "main_sun.inc", "int neg = (den < 0) != ((int16_t)(num >> 16) < 0);",
         "int neg = (den < 0) == ((int16_t)(num >> 16) < 0);"),
        ("light-zero-op", "main_sun.inc", "        if (dx > 0)\n            dx = 0;", "        if (dx >= 0)\n            dx = 0;",
         "at dx == 0 the sub left zero; zeroing it or keeping it is the same value, and it negates and divides to zero either way"),
    ],
}


def main():
    results = []
    for entry in BREAKS[SUITE]:
        name, path, old, new = entry[:4]
        if ONLY and name not in ONLY:
            continue
        source = open(os.path.join("src/main", path)).read()
        if source.count(old) != 1:
            print("%-20s cannot apply: %d matches" % (name, source.count(old)))
            results.append((name, "unapplied"))
            continue
        shutil.rmtree(COPY, ignore_errors=True)
        shutil.copytree("src/main", COPY)
        open(os.path.join(COPY, path), "w").write(source.replace(old, new))
        if os.path.exists("bin/difftest/MAIN_ours.dll"):
            os.remove("bin/difftest/MAIN_ours.dll")
        build = subprocess.run(["make", "-s", "main-break-build", "MAIN_BREAK_DIR=" + COPY],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "maindiff.exe", str(N), SUITE], cwd="bin/difftest",
                             capture_output=True, text=True, errors="replace",
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("maindiff")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 4 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[4] + ")" if len(entry) > 4 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    shutil.rmtree(COPY, ignore_errors=True)
    subprocess.run(["make", "-s", "maindiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%s: %d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (SUITE, len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
