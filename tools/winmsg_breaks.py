#!/usr/bin/env python3
"""Break OpenFS98's WINMSG on purpose, one place at a time, and check that
winmsgdiff notices.

Each break is a small edit to a copy of src/winmsg/winmsg.c -- the kind of
slip a transliteration makes: a boundary, an offset, an order, a dropped
call.  The copy is built as the harness's WINMSG_ours.dll and run for N
scenarios (winmsgdiff exits non-zero when any differ).  At the end the real
build goes back.  A missed break is a gap in the harness or an equivalence;
either way it is reported, and docs/modules/winmsg.md says which.

Usage: tools/winmsg_breaks.py [N] [name...]       (from the repository root,
       after make winmsgdiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/winmsg/winmsg.c"
TMP = "bin/difftest/winmsg_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    # the queue
    ("insert-guard", "        return;\n    PREV(NEXT(pos)) = node;",
     "        ;\n    PREV(NEXT(pos)) = node;",
     "every caller's pos is the head when the queue is empty"),
    ("insert-empty", "    if (q_empty(q) && pos != HEAD(q))\n        return;\n    PREV(NEXT(pos)) = node;",
     "    if (q_empty(q))\n        return;\n    PREV(NEXT(pos)) = node;"),
    ("splice-order", "    PREV(NEXT(pos)) = PREV(TAIL(sub));\n    NEXT(PREV(TAIL(sub))) = NEXT(pos);",
     "    NEXT(PREV(TAIL(sub))) = NEXT(pos);\n    PREV(NEXT(pos)) = PREV(TAIL(sub));",
     "the two writes are different fields of different nodes"),
    ("splice-reset", "    NEXT(HEAD(sub)) = TAIL(sub);\n    PREV(TAIL(sub)) = HEAD(sub);\n}", "}"),
    ("pop-free", "    H_FREE(HEAD(q) + N_NEXT);", "    H_FREE(ADDR(&first));"),
    ("pop-relink", "    NEXT(HEAD(q)) = second;\n    PREV(second) = HEAD(q);",
     "    NEXT(HEAD(q)) = second;"),
    ("erase-free", "    msg_free(node);\n    H_FREE(ADDR(&node));", "    msg_free(node);"),
    ("clear-relink", "    NEXT(HEAD(q)) = TAIL(q);\n    PREV(TAIL(q)) = HEAD(q);\n    return HEAD(q);",
     "    NEXT(HEAD(q)) = TAIL(q);\n    return HEAD(q);"),
    ("clear-return", "    PREV(TAIL(q)) = HEAD(q);\n    return HEAD(q);",
     "    PREV(TAIL(q)) = HEAD(q);\n    return TAIL(q);"),
    ("msg-free-order", "    H_FREE(M32(node + N_DATA));\n    H_FREE(node + N_DATA);",
     "    H_FREE(node + N_DATA);"),
    # messages
    ("measure-width", "    M32(out) = len * 8;", "    M32(out) = len * 7;"),
    ("measure-height", "    M32(out + 4) = 0xd7;", "    M32(out + 4) = 0xd8;"),
    ("limit-margin", "R_WIDTH) - 0x1e;", "R_WIDTH) - 0x1d;"),
    ("limit-cap", "    return w < 0x2000 ? w : 0x2000;", "    return w < 0x1ff8 ? w : 0x1ff8;"),
    ("word-end", "        c = M8(text + i + 1);\n        i++;\n        if (c == 0)\n            break;",
     "        c = M8(text + i + 1);\n        if (c == 0)\n            break;\n        i++;"),
    ("word-empty", "    if (c == 0)\n        return 0;\n    while", "    if (c == 0)\n        return 1;\n    while"),
    ("make-keep", "    if (GLB32(0x1254) == 1) {", "    if (GLB32(0x1254) != 0) {"),
    ("make-speed", "    M32(msg + W_SPEED) = ((uint32_t)GLB16(0x362) >> 1) + 2;\n    return node;",
     "    M32(msg + W_SPEED) = ((uint32_t)GLB16(0x362) >> 1) + 1;\n    return node;"),
    ("make-fresh", "    M32(msg + W_FRESH) = 1;", "    M32(msg + W_FRESH) = 0;"),
    ("make-plain-speed", "        M32(msg + W_SPEED) = 0;", "        M32(msg + W_SPEED) = 2;"),
    ("scrolls-6", "#define SCROLLS(s) ((s) == 3 || (s) == 6 ||", "#define SCROLLS(s) ((s) == 3 ||"),
    ("split-cut-reset", "            p = p + cut + 1;\n            pos = 0;",
     "            p = p + cut + 1;\n            pos = 0;\n            cut = 0;"),
    ("split-share", "            part = cut * time / len;",
     "            part = (uint32_t)((uint64_t)cut * time / len);"),
    ("split-tail-share", "        part = pos * time / len;", "        part = cut * time / len;"),
    ("split-past-cut", "            p = p + cut + 1;", "            p = p + cut;"),
    ("split-fits", "        if ((int32_t)ext[0] > limit()) {\n            part",
     "        if ((int32_t)ext[0] >= limit()) {\n            part"),
    ("slot-realloc", "        H_REALLOC(1, a + b + 1, pp);", "        H_REALLOC(1, a + b + 2, pp);"),
    ("slot-prev-free", "    if (M32(pp) != 0)\n        H_FREE(pp);\n    a = H_STRLEN(prev);",
     "    a = H_STRLEN(prev);"),
    ("queue-fits", "    if ((int32_t)ext[0] > limit()) {\n        sub",
     "    if ((int32_t)ext[0] >= limit()) {\n        sub"),
    ("queue-no-time", "    if (time == 0)\n        return;\n    measure(text, H_STRLEN(text), ADDR(ext));",
     "    measure(text, H_STRLEN(text), ADDR(ext));"),
    # the append
    ("append-search", "        node = PREV(node);\n    prev", "        node = NEXT(node);\n    prev"),
    ("append-first", "        if (node == NEXT(HEAD(M32(S_QUEUE)))) {", "        if (1) {"),
    ("append-edge", "                          M32(msg + W_START)) <= w) {",
     "                          M32(msg + W_START)) < w) {"),
    ("append-new-text", "            queue_after(node, text, slot, time);",
     "            queue_after(node, joined, slot, time);"),
    ("append-len", "        M32(msg + W_LEN) = M32(msg + W_LEN) + more;", "        M32(msg + W_LEN) = M32(msg + W_LEN);"),
    ("append-time", "    time = time + M32(msg + W_TIME);", "    time = time + M32(msg + W_TIME) - 1;"),
    ("append-before", "    before = PREV(node);", "    before = PREV(PREV(node));"),
    # the frame
    ("scroll-done", "    if (M32(msg + W_SCROLL) >> 3 >= M32(msg + W_LEN)) {",
     "    if (M32(msg + W_SCROLL) >> 3 > M32(msg + W_LEN)) {"),
    ("scroll-line", "    H_MEMSET(S_LINE, 0x20, 0x400);", "    H_MEMSET(S_LINE, 0x20, 0x3ff);"),
    ("scroll-room", "    room = 0x3ff - (uint32_t)at;", "    room = 0x400 - (uint32_t)at;"),
    ("scroll-start", "    at = start / 8;", "    at = start >> 3;"),
    ("scroll-step", "    if ((int32_t)M32(msg + W_START) > 0)", "    if ((int32_t)M32(msg + W_START) >= 0)"),
    ("scroll-speed", "    M32(msg + W_SPEED) = ((uint32_t)GLB16(0x362) >> 1) + 2;\n    start",
     "    start"),
    ("scroll-colour", "    H_TEXT(M32(S_REC), 3, M32(S_COLOUR + 4 * M32(msg + W_SLOT)), S_LINE, 0);",
     "    H_TEXT(M32(S_REC), 3, M32(S_COLOUR + 4), S_LINE, 0);"),
    ("charnext", "            p = ADDR(CharNextA((LPCSTR)(uintptr_t)p));", "            p = p + 1;",
     "CharNextA steps one byte in a single-byte code page, the harness's"),
    ("plain-done", "    if (M32(msg + W_TIME) == 0) {\n        CHAIN_SEND(0x14, 0x50, node);",
     "    if (M32(msg + W_TIME) == 0) {"),
    ("plain-count", "    M32(msg + W_TIME) = M32(msg + W_TIME) - 1;\n}", "}"),
    ("blank-fill", "    H_MEMSET(M32(rec + R_BITS), M8(S_FILL), M32(rec + R_SIZE));",
     "    H_MEMSET(M32(rec + R_BITS), 0x0b, M32(rec + R_SIZE));",
     "only a line 36 bytes early reaches the fill, a speed of 289"),
    ("frame-status", "        if (!(flags & F_SHOWN) || (flags & F_NO_STATUS))",
     "        if (!(flags & F_SHOWN))"),
    ("frame-scroll-test", "    if (SCROLLS(M32(msg + W_SLOT)) && GLB32(0x1254) != 0) {",
     "    if (SCROLLS(M32(msg + W_SLOT)) && GLB32(0x1254) == 1) {"),
    # the Linkage and the situation
    ("show-forget", "        if (M32(S_TEXTS + 4 * slot) != 0)\n            H_FREE(S_TEXTS + 4 * slot);\n        clear_all();",
     "        clear_all();"),
    ("show-blank", "        blank();\n        H_WIN_UPDATE(M32(S_REC));\n    }\n    return PREV",
     "        H_WIN_UPDATE(M32(S_REC));\n    }\n    return PREV"),
    ("show-return", "    return PREV(TAIL(M32(S_QUEUE)));\n}", "    return NEXT(HEAD(M32(S_QUEUE)));\n}"),
    ("clear-slot-next", "            q = M32(S_QUEUE);\n            node = next;",
     "            q = M32(S_QUEUE);\n            node = NEXT(node);"),
    ("clear-slot-return", "    return q;\n}\n\n/* .text+0x1640", "    return 0;\n}\n\n/* .text+0x1640"),
    ("queued-first", "    uint32_t q = M32(S_QUEUE), node = NEXT(HEAD(q)), tail = TAIL(q);",
     "    uint32_t q = M32(S_QUEUE), node = NEXT(NEXT(HEAD(q))), tail = TAIL(q);"),
    ("situation-tag", "        if (M32(data) != 0x3df)", "        if (M32(data) != 0x3de)"),
    ("situation-flags", "        H_MEMMOVE(M32(S_REC) + 0x08, src, 4);", "        H_MEMMOVE(M32(S_REC) + 0x08, src, 2);"),
    ("situation-size", "        MAIN_SAVE_OPEN(0x3df, 0x24);", "        MAIN_SAVE_OPEN(0x3df, 0x28);"),
    ("situation-other", "    return message - 0x37;", "    return 0;"),
    ("close-title", "    H_WIN_DESTROY(M32(S_REC));\n    H_FREE(M32(S_REC));", "    H_WIN_DESTROY(M32(S_REC));"),
    ("init-min", "    M32(rec + 0x68) = 0x16;", "    M32(rec + 0x68) = 0x15;"),
    ("init-group", "    M32(M32(S_REC) + 0x78) = 0x5000;", "    M32(M32(S_REC) + 0x78) = 0x4000;"),
    ("deinit-texts", "    for (p = S_TEXTS; p < S_TEXTS + 0x1c; p += 4)",
     "    for (p = S_TEXTS; p < S_TEXTS + 0x18; p += 4)"),
]


def main():
    source = open(SRC).read()
    results = []
    for entry in BREAKS:
        name, old, new = entry[:3]
        if ONLY and name not in ONLY:
            continue
        if source.count(old) != 1:
            print("%-20s cannot apply: %d matches" % (name, source.count(old)))
            results.append((name, "unapplied"))
            continue
        open(TMP, "w").write(source.replace(old, new))
        if os.path.exists("bin/difftest/WINMSG_ours.dll"):
            os.remove("bin/difftest/WINMSG_ours.dll")
        build = subprocess.run(["make", "-s", "winmsg-break-build", "WINMSG_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "winmsgdiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("winmsgdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "winmsgdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
