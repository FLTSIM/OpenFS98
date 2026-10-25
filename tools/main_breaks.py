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
