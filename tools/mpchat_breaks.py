#!/usr/bin/env python3
"""Break OpenFS98's MPCHAT on purpose, one place at a time, and check that
mpchatdiff notices.

Each break is a small edit to a copy of src/mpchat/mpchat.c -- the kind of
slip a transliteration makes: a constant, an offset, an order, a dropped
call, a comparison.  The copy is built as the harness's MPCHAT_ours.dll and
run for N scenarios (mpchatdiff exits non-zero when any differ).  At the end
the real build goes back.  A missed break is a gap in the harness or an
equivalence; either way it is reported, and docs/modules/mpchat.md says which.

Usage: tools/mpchat_breaks.py [N] [name...]       (from the repository root,
       after make mpchatdiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/mpchat/mpchat.c"
TMP = "bin/difftest/mpchat_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    # the window
    ("visible-bit", "GWL_STYLE) & WS_VISIBLE;", "GWL_STYLE) & WS_DISABLED;"),
    ("show-cmd", "    ShowWindow(WND(M32(S_WND)), SW_SHOWNOACTIVATE);\n    UpdateWindow(WND(M32(S_WND)));\n}\n\n/* .text+0x1580",
     "    ShowWindow(WND(M32(S_WND)), SW_SHOW);\n    UpdateWindow(WND(M32(S_WND)));\n}\n\n/* .text+0x1580"),
    ("hide-update", "    ShowWindow(WND(M32(S_WND)), SW_HIDE);\n    UpdateWindow(WND(M32(S_WND)));",
     "    ShowWindow(WND(M32(S_WND)), SW_HIDE);"),
    ("focus-entry", "    SetFocus(WND(M32(S_ENTRY)));\n}\n\n/* .text+0x14a0", "    SetFocus(WND(M32(S_CHAT)));\n}\n\n/* .text+0x14a0"),
    ("destroy", "    DestroyWindow(WND(M32(S_WND)));", "    DestroyWindow(WND(M32(S_CHAT)));"),
    # the list
    ("fill-reset", "    SendMessageA(WND(M32(S_LIST)), LB_RESETCONTENT, 0, 0);\n", ""),
    ("fill-order", "    if (M32(S_OTHERS) != 0 && M32(M32(S_OTHERS)) != 0)\n        for (i = 0, at = 0; i < M32(M32(S_OTHERS));",
     "    if (M32(S_PLAYERS) != 0 && M32(M32(S_PLAYERS)) != 0)\n        for (i = 0, at = 0; i < M32(M32(S_PLAYERS));"),
    ("fill-stride", "i < M32(M32(S_OTHERS)); i++, at += RECORD_SIZE)", "i < M32(M32(S_OTHERS)); i++, at += 0x80)"),
    ("fill-count", "i < M32(M32(S_PLAYERS)); i++, at += RECORD_SIZE)", "i < M32(M32(S_PLAYERS)) - 1; i++, at += RECORD_SIZE)"),
    ("fill-guard", "    if (M32(S_OTHERS) != 0 && M32(M32(S_OTHERS)) != 0)\n", "    if (M32(S_OTHERS) != 0)\n",
     "the loop's own test is the same comparison of the same word"),
    ("record-size", "#define RECORD_SIZE 0x100", "#define RECORD_SIZE 0x104"),
    # the transcript
    ("sel-test", "(uint32_t)sel.min != len) {\n        end.max", "0) {\n        end.max"),
    ("sel-end", "        end.max = (int32_t)len;\n        end.min = (int32_t)len;", "        end.max = (int32_t)len;\n        end.min = 0;"),
    ("newline-empty", "    if (len != 0 && newlines != 0)", "    if (newlines != 0)"),
    ("newline-count", "        while (--newlines);", "        while (0);"),
    ("indent", "& 0x168) + 0x28);", "& 0x160) + 0x28);"),
    ("indent-base", "& 0x168) + 0x28);", "& 0x168) + 0x20);"),
    ("pf-mask", "    pf.mask = 1; ", "    pf.mask = 9; "),
    ("cf-mask", "    cf.mask = 0x40000001u;", "    cf.mask = 0x00000001u;"),
    ("cf-bold", "    cf.effects = bold != 0;", "    cf.effects = 0;",
     "both callers pass bold 0"),
    ("cf-colour", "    cf.colour = colour;", "    cf.colour = 0;"),
    ("cf-scope", "0x444, 1, (LPARAM)&cf);", "0x444, 0, (LPARAM)&cf);"),
    ("restore-sel", "    if ((uint32_t)sel.min != (uint32_t)sel.max || (uint32_t)sel.min != len)\n        SendMessageA(WND(hwnd), 0x437, 0, (LPARAM)&sel);\n", ""),
    ("scroll-fail", "    if (GetScrollInfo(WND(hwnd), SB_VERT, &after) == 0)\n        return;", "    GetScrollInfo(WND(hwnd), SB_VERT, &after);"),
    ("scroll-after", "    if (after.nMax <= (int32_t)", "    if (after.nMax < (int32_t)"),
    ("scroll-before", "before.nPos <= (int32_t)", "before.nPos < (int32_t)"),
    ("scroll-got", "    if (got != 0 && before.nPos", "    if (before.nPos"),
    ("scroll-signed", "    if (after.nMax <= (int32_t)(after.nPage + (uint32_t)after.nPos))", "    if ((uint32_t)after.nMax <= after.nPage + (uint32_t)after.nPos)"),
    ("scroll-code", "                     SB_THUMBPOSITION, 0);", "                     SB_THUMBTRACK, 0);"),
    ("scroll-pos", "<< 16) - after.nPage + (uint32_t)after.nMax) << 16)", "<< 16) + (uint32_t)after.nMax) << 16)"),
    # a line of chat
    ("say-colon", "    H_STRCAT(ADDR(line), S_COLON);", "    H_STRCAT(ADDR(line), S_SPACE);"),
    ("say-colour", "ADDR(line), 0xff0000, 2, 0, 0);", "ADDR(line), 0x0000ff, 2, 0, 0);"),
    ("say-newlines", "ADDR(line), 0xff0000, 2, 0, 0);", "ADDR(line), 0xff0000, 1, 0, 0);"),
    ("say-indent", "    append(M32(S_CHAT), text, 0, 1, 1, 0);", "    append(M32(S_CHAT), text, 0, 1, 0, 0);"),
    ("say-local", "    if (is_visible() || local != 0)", "    if (is_visible())"),
    ("say-visible", "    if (is_visible() || local != 0)", "    if (local != 0)"),
    ("say-slot", "    WINMSG_SHOW(ADDR(line), 6, 0x5a, 0);", "    WINMSG_SHOW(ADDR(line), 5, 0x5a, 0);"),
    ("say-time", "    WINMSG_SHOW(S_SPACE, 6, 0x5a, 1);", "    WINMSG_SHOW(S_SPACE, 6, 0x5b, 1);"),
    ("say-append", "    WINMSG_SHOW(text, 6, 0x5a, 1);", "    WINMSG_SHOW(text, 6, 0x5a, 0);"),
    ("send-type", "    M32(S_MSG) = MSG_CHAT;", "    M32(S_MSG) = MSG_CHAT + 1;"),
    ("send-len", "    n = H_STRLEN(S_MSG + 8) + 1;", "    n = H_STRLEN(S_MSG + 8);"),
    ("send-size", "    MULTIP_SEND(0, S_MSG, n + 8, 0);", "    MULTIP_SEND(0, S_MSG, n + 4, 0);"),
    ("send-to", "    MULTIP_SEND(0, S_MSG, n + 8, 0);", "    MULTIP_SEND(1, S_MSG, n + 8, 0);"),
    ("send-store", "    M32(S_LOCAL) = local;\n", ""),
    ("send-name", "local != 0 ? M32(local) : S_EMPTY", "local != 0 ? M32(local) : S_SPACE"),
    ("send-flag", "S_EMPTY, text, 1);", "S_EMPTY, text, 0);"),
    ("entry-limit", "buf, 0x3ff);", "buf, 0x400);"),
    ("entry-clear", "    SetWindowTextA(WND(M32(S_ENTRY)), STR(S_EMPTY));\n", ""),
    ("entry-empty", "    if (H_STRLEN(ADDR(buf)) == 0)\n        return;\n", ""),
    # layout
    ("split", "#define SPLIT(right) (((uint32_t)(right) * M32(S_SPLIT)) >> 16)", "#define SPLIT(right) (((uint32_t)(right) * M32(S_SPLIT)) >> 15)"),
    ("split-signed", "#define SPLIT(right) (((uint32_t)(right) * M32(S_SPLIT)) >> 16)", "#define SPLIT(right) ((uint32_t)((int32_t)(right) * (int32_t)M32(S_SPLIT) >> 16))"),
    ("size-chat", "(int)(split - 3), (int)(bottom - 0x23),\n                     SWP_NOZORDER);", "(int)(split - 2), (int)(bottom - 0x23),\n                     SWP_NOZORDER);"),
    ("size-list", "(int)(split + 1), 2, (int)(right - split - 1),\n                     (int)(bottom - 0x23), SWP_NOZORDER);", "(int)(split + 1), 2, (int)(right - split),\n                     (int)(bottom - 0x23), SWP_NOZORDER);"),
    ("size-entry", "(int)(right - 0x50), 0x19,\n                     SWP_NOZORDER);", "(int)(right - 0x51), 0x19,\n                     SWP_NOZORDER);"),
    ("size-button", "(int)(right - 0x4b), (int)(bottom - 0x1e), 0x46, 0x19,\n                     SWP_NOZORDER);", "(int)(right - 0x4b), (int)(bottom - 0x1d), 0x46, 0x19,\n                     SWP_NOZORDER);"),
    ("size-flags", "(int)(bottom - 0x23),\n                     SWP_NOZORDER);", "(int)(bottom - 0x23),\n                     SWP_NOZORDER | SWP_NOMOVE);"),
    ("create-chat-style", "0x50a30904", "0x50a30900"),
    ("create-chat-h", "(int)(split - 3), (int)(bottom - 0x23), hwnd, (HMENU)0x100", "(int)(split - 3), (int)(bottom - 0x22), hwnd, (HMENU)0x100"),
    ("create-list-id", "(HMENU)0x102", "(HMENU)0x104"),
    ("create-list-split", "        split = SPLIT(right);\n        h = (uint32_t)(uintptr_t)CreateWindowExA(\n            WS_EX_CLIENTEDGE, STR(S_LISTBOX)",
     "        h = (uint32_t)(uintptr_t)CreateWindowExA(\n            WS_EX_CLIENTEDGE, STR(S_LISTBOX)",
     "split is recomputed from the same right and the same unwritten +0x80"),
    ("create-entry-y", "0x50830080, 2, (int)(bottom - 0x1e),", "0x50830080, 2, (int)(bottom - 0x1f),"),
    ("create-button-text", "STR(M32(S_SEND_TEXT)), 0x50030000", "STR(S_EMPTY), 0x50030000"),
    ("create-button-ex", "            0, STR(S_BUTTON)", "            WS_EX_CLIENTEDGE, STR(S_BUTTON)"),
    ("create-instance", "(HMENU)0x101,\n            (HINSTANCE)(uintptr_t)M32(S_MAIN_INST), 0);", "(HMENU)0x101,\n            (HINSTANCE)(uintptr_t)M32(S_INSTANCE), 0);"),
    ("create-show", "        ShowWindow(WND(h), SW_SHOWNOACTIVATE);\n        UpdateWindow(WND(M32(S_LIST)));", "        UpdateWindow(WND(M32(S_LIST)));"),
    ("defid", "DefDlgProcA(hwnd, DM_SETDEFID, 0x103, 0);", "SendMessageA(hwnd, DM_SETDEFID, 0x103, 0);"),
    # the procedure
    ("proc-gt", "    if (msg > 5) {", "    if (msg >= 5) {"),
    ("proc-setfocus", "        if (msg == WM_SETFOCUS) {\n            SetFocus(WND(M32(S_ENTRY)));", "        if (msg == WM_SETFOCUS) {\n            SetFocus(WND(M32(S_CHAT)));"),
    ("proc-close", "            H_POST(CMD_CHAT, 0);\n            return 0;", "            H_POST(CMD_CHAT, 0);"),
    ("proc-post", "            H_POST(CMD_CHAT, 0);", "            H_POST(CMD_CHAT, 1);"),
    ("proc-en", "(uint32_t)wparam >> 16 == 0x100)", "(uint32_t)wparam >> 16 == 0x200)"),
    ("proc-setsel", "EM_SETSEL, (WPARAM)-1, 0);", "EM_SETSEL, 0, -1);"),
    ("proc-bn", "== 0x103 && (uint32_t)wparam >> 16 == 0) {", "== 0x103) {"),
    ("proc-focus", "                SetFocus(WND(M32(S_MAIN_WND)));", "                SetFocus(WND(M32(S_ENTRY)));"),
    ("proc-default", "    return DefDlgProcA(hwnd, msg, wparam, lparam);\n}", "    return msg == WM_SIZE ? 0 : DefDlgProcA(hwnd, msg, wparam, lparam);\n}"),
    # the dialog
    ("dlg-ex", "        WS_EX_TOOLWINDOW, STR(S_CLASS)", "        0, STR(S_CLASS)"),
    ("dlg-size", "0x84cc0000, 0, 0, 0x190, 0xc8,", "0x84cc0000, 0, 0, 0x190, 0xc9,"),
    ("dlg-parent", "        WND(M32(S_MAIN_WND)), 0, (HINSTANCE)", "        0, 0, (HINSTANCE)"),
    ("dlg-show", "    ShowWindow(WND(h), SW_SHOWNOACTIVATE);\n    M32(S_PLAYERS)", "    ShowWindow(WND(h), SW_SHOW);\n    M32(S_PLAYERS)"),
    ("dlg-list", "    M32(S_PLAYERS) = MULTIP_LIST();\n", ""),
    ("dlg-return", "    UpdateWindow(WND(M32(S_WND)));\n    return M32(S_WND);", "    UpdateWindow(WND(M32(S_WND)));\n    return h;",
     "nothing between the store and the return can write +0xd8"),
    ("dlg-table", "    M32(D + MC_LINKAGE + 0x14) = D + MC_DIALOGS;", "    M32(D + MC_LINKAGE + 0x14) = D + MC_DIALOGS + 12;"),
    # the callbacks
    ("key-toggle", "        else\n            show();\n        M32(out) = 1;\n    } else if", "        else\n            show();\n        M32(out) = 2;\n    } else if"),
    ("key-focus", "            show();\n        focus_entry();", "            show();"),
    ("key-show", "        if (!is_visible())\n            show();\n        focus_entry();", "        show();\n        focus_entry();"),
    ("key-id", "    } else if (command == 0x10171) {", "    } else if (command == 0x10172) {"),
    ("menu-id", "    if (command != CMD_CHAT)\n        return;\n    if (is_visible())", "    if (command != CMD_CHAT + 1)\n        return;\n    if (is_visible())"),
    ("menu-swap", "    if (is_visible())\n        hide();\n    else\n        show();\n}\n\n/* .text+0x13c0", "    if (is_visible())\n        show();\n    else\n        hide();\n}\n\n/* .text+0x13c0"),
    ("situation-msg", "    if (message != 0x40)", "    if (message != 0x41)"),
    ("situation-flags", "        H_MENU_STATE(CMD_CHAT, 1);", "        H_MENU_STATE(CMD_CHAT, 3);"),
    ("begin-id", "    H_DIALOG(0xff00, 0);", "    H_DIALOG(0xff01, 0);"),
    ("begin-fill", "    H_DIALOG(0xff00, 0);\n    fill_list();", "    H_DIALOG(0xff00, 0);"),
    ("begin-menu", "    H_MENU_ADD(0x1108, CMD_CHAT, CMD_CHAT, 0);", "    H_MENU_ADD(0x1109, CMD_CHAT, CMD_CHAT, 0);"),
    ("begin-order", "    CHAIN_ADD(0xb, ADDR(on_menu));\n    CHAIN_ADD(0xc, ADDR(on_command));", "    CHAIN_ADD(0xc, ADDR(on_command));\n    CHAIN_ADD(0xb, ADDR(on_menu));"),
    ("end-order", "    CHAIN_REMOVE(0xc, ADDR(on_command));\n    CHAIN_REMOVE(0xb, ADDR(on_menu));", "    CHAIN_REMOVE(0xb, ADDR(on_menu));\n    CHAIN_REMOVE(0xc, ADDR(on_command));"),
    ("end-menu", "    H_MENU_REMOVE(CMD_CHAT);\n", ""),
    ("end-destroy", "    H_MENU_REMOVE(CMD_CHAT);\n    destroy();", "    H_MENU_REMOVE(CMD_CHAT);"),
    ("end-local", "    M32(S_OTHERS) = 0;\n    M32(S_LOCAL) = 0;", "    M32(S_OTHERS) = 0;"),
    ("end-others", "    M32(S_PLAYERS) = 0;\n    M32(S_OTHERS) = 0;", "    M32(S_PLAYERS) = 0;"),
    ("message-type", "    if (M32(M32(args + 4)) != MSG_CHAT)", "    if (M32(M32(args + 4)) != MSG_CHAT + 1)"),
    ("message-unknown", "        say(M32(S_UNKNOWN), M32(args + 4) + 8, 0);", "        say(S_EMPTY, M32(args + 4) + 8, 0);"),
    ("message-text", "        say(M32(player), M32(args + 4) + 8, 0);", "        say(M32(player), M32(args + 4) + 9, 0);"),
    ("message-local", "        say(M32(player), M32(args + 4) + 8, 0);", "        say(M32(player), M32(args + 4) + 8, 1);"),
    ("session-3", "        M32(S_PLAYERS) = arg;\n", "        M32(S_OTHERS) = arg;\n"),
    ("session-4", "        M32(S_OTHERS) = arg;\n", "        M32(S_PLAYERS) = arg;\n"),
    ("session-4-fill", "        M32(S_OTHERS) = arg;\n        fill_list();", "        M32(S_OTHERS) = arg;"),
    # the Linkage
    ("init-riched", "    M32(S_RICHED_MOD) = (uint32_t)(uintptr_t)LoadLibraryA(STR(S_RICHED));\n", ""),
    ("init-strings", "    H_STRING(0x9401, S_SEND_TEXT);\n    H_STRING(0x9402, S_UNKNOWN);", "    H_STRING(0x9401, S_UNKNOWN);\n    H_STRING(0x9402, S_SEND_TEXT);"),
    ("init-style", "CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;", "CS_HREDRAW | CS_VREDRAW;"),
    ("init-extra", "    wc.cbWndExtra = DLGWINDOWEXTRA;", "    wc.cbWndExtra = 0;"),
    ("init-brush", "GetStockObject(LTGRAY_BRUSH)", "GetStockObject(WHITE_BRUSH)"),
    ("init-instance", "    wc.hInstance = (HINSTANCE)(uintptr_t)M32(S_INSTANCE);", "    wc.hInstance = (HINSTANCE)(uintptr_t)M32(S_MAIN_INST);"),
    ("init-return", "    return CHAIN_ADD(0x1d, ADDR(on_session));", "    CHAIN_ADD(0x1d, ADDR(on_session));\n    return 0;"),
    ("deinit-unregister", "    UnregisterClassA(STR(S_CLASS), (HINSTANCE)(uintptr_t)M32(S_INSTANCE));\n", ""),
    ("deinit-leave", "    H_MODULE_OUT(M32(S_INSTANCE));", "    H_MODULE_OUT(0);"),
    ("deinit-free", "    H_FREE(S_TITLE);\n    H_FREE(S_SEND_TEXT);", "    H_FREE(S_SEND_TEXT);\n    H_FREE(S_TITLE);"),
    ("deinit-library", "    FreeLibrary((HMODULE)(uintptr_t)M32(S_RICHED_MOD));\n", ""),
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
        if os.path.exists("bin/difftest/MPCHAT_ours.dll"):
            os.remove("bin/difftest/MPCHAT_ours.dll")
        build = subprocess.run(["make", "-s", "mpchat-break-build", "MPCHAT_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "mpchatdiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("mpchatdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "mpchatdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
