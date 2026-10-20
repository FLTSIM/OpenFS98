#!/usr/bin/env python3
"""Break OpenFS98's G3D on purpose, one place at a time, and check that
g3ddiff notices.

Each break is a small edit to a copy of src/g3d/g3d.c -- the kind of slip a
transliteration makes: a boundary, an offset, a flag, a dropped store.  The
copy is built as the harness's G3D_ours.dll and run for N scenarios (g3ddiff
exits non-zero when any differ).  At the end the real build goes back.  A
missed break is a gap in the harness or an equivalence; either way it is
reported, and docs/modules/g3d.md says which.

The list grows with the transliteration, a stage at a time.

Usage: tools/g3d_breaks.py [N] [name...]       (from the repository root,
       after make g3ddiff)
"""
import os
import shutil
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/g3d/g3d.c"
TMP = "bin/difftest/g3d_break.c"
LIMIT = 900              # seconds a break's run may take before it counts as a hang

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent (or unreachable), reported as such.
BREAKS = [
    # stage 1: the transform core and the small services
    ("norm-clamp", "    if (bit <= 3) {                             /* 0x206e13c7 */",
     "    if (bit <= 1) {                             /* 0x206e13c7 */"),
    ("norm-clamp-edge", "    if (bit <= 3) {                             /* 0x206e13c7 */",
     "    if (bit <= 2) {                             /* 0x206e13c7 */",
     "at bit 3 both branches shift by 25 (0x1c - 3 = 0x19)"),
    ("norm-shift", "        SET_CL(0x19);\n", "        SET_CL(0x18);\n"),
    ("norm-zero", "    edx = ecx;\n    if (ebx == 0)\n        return;",
     "    edx = ecx;\n    if (ebx == 0) {\n        M8(V(0x206f9122)) = 0;\n        return;\n    }"),
    ("sat-sign", "((int32_t)d < 0) != ((int32_t)edx < 0) ? 0x80000001u",
     "((int32_t)d < 0) == ((int32_t)edx < 0) ? 0x80000001u"),
    ("scale-shift", "    ecx += 8;\n    n = CL & 31;", "    ecx += 7;\n    n = CL & 31;"),
    ("length-round", "            ebx &= 0xfffffffeu;\n", "\n"),
    ("length-0x15", "        if (SI == 0x15) {", "        if (SI == 0x16) {"),
    ("length-top", "    if (SI <= 0x93) {", "    if (SI < 0x93) {"),
    ("tick-reload", "    if (M8(V(0x206f915b)) & 0x80) {", "    if (M8(V(0x206f915b)) == 0xff) {"),
    ("leftover-ecx", "    if ((int32_t)ecx > 0)\n        eax = ecx + edi;\n    else\n        eax = 0;\n}",
     "    if ((int32_t)ecx > 0)\n        eax = ecx + edi;\n    else\n        eax = 0;\n    ecx = 0;\n}"),

    # stage 2: the interpreter, init, and the opcodes that do not draw
    ("stop-cleared", "            if (eax != 0) {\n                M32(V(0x206fd204)) = 0;\n                break;",
     "            if (eax != 0) {\n                break;"),
    ("top-opcode", "            if (AX > 0x93)\n                break;", "            if (AX >= 0x93)\n                break;"),
    ("run-c90", "    M8(V(0x206f9125)) = AL;\n    M16(ecx + 0xc90) = AX;\n    M16(V(0x206fd6c8)) = AX;\n    M32(V(0x206f9126)) = edx;",
     "    M8(V(0x206f9125)) = AL;\n    M16(V(0x206fd6c8)) = AX;\n    M32(V(0x206f9126)) = edx;"),
    ("context-scale", "    M16(V(0x206f9159)) = AX;\n    eax = arg[0];", "    eax = arg[0];"),
    ("model-v", "    if (M8(esi) != 0x76)", "    if (M8(esi) != 0x75)"),
    ("model-even", "    model &= 0xfffffffeu;\n", "\n"),
    ("model-eye", "    eax >>= 9;\n    M16(edi + 0xe06) = AX;", "    eax >>= 8;\n    M16(edi + 0xe06) = AX;"),
    ("counted-swap", "    SET_AL(M8(V(0x206f915c)));\n    M8(V(0x206f915d)) = AL;",
     "    SET_AL(M8(V(0x206f915b)));\n    M8(V(0x206f915d)) = AL;"),
    ("end-length", "    M32(V(0x206fd204)) = 1;\n    eax += 2;", "    M32(V(0x206fd204)) = 1;\n    eax += 4;"),
    ("jump-from", "    esi += eax;\n    M32(V(0x206f9126)) = esi;\n}", "    esi += eax + 2;\n    M32(V(0x206f9126)) = esi;\n}"),
    ("case-bound", "    if (BX >= CX)\n        SET_BX(0xffff);", "    if (BX > CX)\n        SET_BX(0xffff);"),
    ("case-16bit", "    ebx += ebx;\n    ebx = ebx + esi + 8;", "    ebx += ebx;\n    ebx &= 0xffff;\n    ebx = ebx + esi + 8;"),
    ("range-low", "(int16_t)BX < (int16_t)M16(at + 2)", "(int16_t)BX <= (int16_t)M16(at + 2)"),
    ("range-high", "(int16_t)BX > (int16_t)M16(at + 4)", "(int16_t)BX >= (int16_t)M16(at + 4)"),
    ("range-three", "    if (in_range(esi + 4) && in_range(esi + 0xa) && in_range(esi + 0x10))",
     "    if (in_range(esi + 4) && in_range(esi + 0xa))"),
    ("mask-length", "    if (AX != 0)\n        edx = 8;", "    if (AX != 0)\n        edx = 6;"),
    ("context-add", "    if (eax != 0) {\n        ebx += eax;\n        return;\n    }",
     "    if (eax != 0) {\n        ebx += eax + 2;\n        return;\n    }"),
    ("unmapped-hook", "    if (ebx != G)\n        return;\n    variable_unmapped();",
     "    if (ebx != G || 1)\n        return;\n    variable_unmapped();"),
    ("scratch-word", "    ebx = V(0x207087e0);\n    eax = 0;", "    ebx = V(0x207087e2);\n    eax = 0;"),
    ("set-byte", "    lods16();\n    M16(ebx) = AX;\n    M32(V(0x206f9126)) = esi;",
     "    lods16();\n    M8(ebx) = AL;\n    M32(V(0x206f9126)) = esi;"),
    ("move-direction", "    variable_at(esi + 2);\n    SET_AX(M16(edi));\n    M16(ebx) = AX;",
     "    variable_at(esi + 2);\n    SET_AX(M16(ebx));\n    M16(edi) = AX;"),
    ("move-unsigned", "    ebx = M16(esi + 4);\n    table_address();\n    SET_CX(M16(edi));",
     "    ebx = (uint32_t)(int32_t)(int16_t)M16(esi + 4);\n    table_address();\n    SET_CX(M16(edi));",
     "differs only for variable numbers from 0x8000, which index past the image on both sides"),
    ("no-context-kept", "    M16(ebx) = AX;\n    M32(V(0x206f91b7)) = keep;", "    M16(ebx) = AX;\n"),
    ("context-end", "    if (AX == 0)\n        M32(V(0x206f91b7)) = 0;", "    if (AX != 0)\n        M32(V(0x206f91b7)) = 0;"),
    ("scale-edge", "    if ((int16_t)operand < 0x20) {", "    if ((int16_t)operand < 0x1f) {"),
    ("scale-equal", "    if ((int16_t)operand < 0x20) {", "    if ((int16_t)operand <= 0x20) {",
     "at 0x20 the shift is by 0"),
    ("scale-count", "    SET_CL(0x20 - BL);", "    SET_CL(0x1f - BL);"),
    ("op17-test", "    if (M16(esi + 2) != AX)\n        SET_AX(M16(edi + 0xcb6));", "    if (M16(esi + 2) == AX)\n        SET_AX(M16(edi + 0xcb6));"),
    ("mirror", "    if (AX == 2) {\n        edi = G;", "    if (AX == 3) {\n        edi = G;"),
    ("level-equal", "    if (AX < M16(V(0x206fd720)))\n        return;", "    if (AX <= M16(V(0x206fd720)))\n        return;"),
    ("op87-step", "    if (M16(edi + 0xcbe) == 1)", "    if (M16(edi + 0xcbe) == 2)"),
    ("op81-and", "    SET_AX(AX & M16(edi + 0xcd0));", "    SET_AX(AX | M16(edi + 0xcd0));"),
    ("op04-reload", "        M32(V(0x206fd6d0)) = 0x64;", "        M32(V(0x206fd6d0)) = 0x63;"),
    ("op86-length", "    M32(V(0x206f9126)) += 3;", "    M32(V(0x206f9126)) += 2;"),
    ("op8f-field", "    M32(ecx + 0x1298) = eax;", "    M32(ecx + 0x129c) = eax;"),
    ("op92-field", "static void op_92(void) { op_word_to_global(0x324); }", "static void op_92(void) { op_word_to_global(0x326); }"),
    ("absolute-return", "    eax = keep;\n    eax += 6;", "    eax = keep;\n    eax += 4;"),
    ("via-return", "    interpret();\n    M32(V(0x206f9126)) = keep;\n}\n\n/* 0x206e4422",
     "    interpret();\n    M32(V(0x206f9126)) = keep + 2;\n}\n\n/* 0x206e4422"),
    ("queue-clamp", "    if (ebx > 4)\n        ebx = 4;", "    if (ebx > 3)\n        ebx = 3;"),
    ("queue-header", "    eax += ebx;\n    eax += 0x14;\n}", "    eax += ebx;\n    eax += 0x10;\n}"),
    ("table-size-word", "    M16(edi + 4) = (uint16_t)size;              /* pop word [edi+4] */\n", ""),
    ("oldest-ties", "!(AX > M16(ebx + 0x12))", "!(AX >= M16(ebx + 0x12))"),
    ("unloaded-flag", "            M8(ebx) = 0;\n            M8(ebx + 0xd) = 0;\n", "            M8(ebx) = 0;\n"),
    ("step-wrap", "    if (!(SI < 0x12))\n        SET_SI(0);", "    if (!(SI < 0x11))\n        SET_SI(0);"),
    ("haze-neg", "    SET_DX(-DX);\n    M16(V(0x206fdc3f)) = DX;", "    M16(V(0x206fdc3f)) = DX;"),
    ("queue-freed", "    deinit_parts();\n    queue_free();", "    deinit_parts();"),
    # stage 3: drawing
    ("line-both-out", "    SET_CH(CH & CL);\n    if (CH != 0)\n        goto done;\n    if (CL != 0)\n        goto clip_a;",
     "    SET_CH(CH & CL);\n    if (CL != 0)\n        goto clip_a;"),
    ("line-tries", "    M8(V(0x206f9167)) = 5;\nclip_b:", "    M8(V(0x206f9167)) = 4;\nclip_b:"),
    ("line-cached", "    if (M8(V(0x206f9120)) & 1)\n        line_project();\n    else\n        line_cached();",
     "    line_project();"),
    ("crossing-carry", "    eax = (edx & 1) << 31;                      /* sar edx,1 / rcr eax,1 */",
     "    eax = 0;"),
    ("crossing-double", "    p = (int64_t)((uint64_t)p << 1);            /* add eax,eax / adc edx,edx */\n    eax = (uint32_t)p;\n    edx = (uint32_t)((uint64_t)p >> 32);\n    esi += edx;",
     "    eax = (uint32_t)p;\n    edx = (uint32_t)((uint64_t)p >> 32);\n    esi += edx;"),
    ("onto-y-low-neg", "    M32(V(0x206f90c0)) = esi;\n    edx = (uint32_t)-(int32_t)edx;\n    M32(V(0x206f90c4)) = edx;",
     "    M32(V(0x206f90c0)) = esi;\n    M32(V(0x206f90c4)) = edx;"),
    ("onto-x-high-sub", "    ebx -= M32(V(0x206f90c0));\n    ebx -= M32(V(0x206f90d4));\n    edx -= M32(V(0x206f90c0));\n    ecx = M32(V(0x206f90d0));",
     "    ebx -= M32(V(0x206f90c0));\n    ebx -= M32(V(0x206f90d4));\n    edx += M32(V(0x206f90c0));\n    ecx = M32(V(0x206f90d0));"),
    ("first-outside", "    M8(V(0x206f92e4)) = 0;\n    if ((int32_t)edx > (int32_t)eax) {\n        M8(V(0x206f92e4)) = 1;\n        return;\n    }\n    M8(V(0x206f9237)) = 0;",
     "    M8(V(0x206f92e4)) = 0;\n    if ((int32_t)edx >= (int32_t)eax) {\n        M8(V(0x206f92e4)) = 1;\n        return;\n    }\n    M8(V(0x206f9237)) = 0;"),
    ("stage-reenter", "    if (M8(V(flag_out)) == 1) {                                               \\\n        if (out_test) {",
     "    if (M8(V(flag_out)) == 2) {                                               \\\n        if (out_test) {"),
    ("stage2-plane", "STAGE(stage2, 0x206f9238, first1, 0x206f92e5, 0x206f92ca,\n      (int32_t)ecx > (int32_t)eax,",
     "STAGE(stage2, 0x206f9238, first1, 0x206f92e5, 0x206f92ca,\n      (int32_t)ecx >= (int32_t)eax,"),
    ("emit-zero-w", "        eax = 0x10000;                          /* 0x206e1eb1 */", "        eax = 0x8000;                           /* 0x206e1eb1 */"),
    ("emit-clamp-sign", "        uint32_t neg = (int32_t)edx < 0;\n\n        edx = eax;", "        uint32_t neg = 0;\n\n        edx = eax;"),
    ("close-count", "    M32(edi + 0x2f0) -= 1;\n    if ((int32_t)M32(edi + 0x2f0) < 0)", "    if ((int32_t)M32(edi + 0x2f0) < 0)"),
    ("close-stage3", "        LOAD_VERTEX(0x206f923a);\n        close3();\n", ""),
    ("project-inc", "    ecx++;\n    idiv32(ecx);\n    ebp <<= 16;", "    idiv32(ecx);\n    ebp <<= 16;"),
    ("line-cache-y", "    M32(V(0x206f92ea)) = edx;\n    eax = G;", "    M32(V(0x206f92ea)) = ecx;\n    eax = G;"),
    ("point-cone", "    eax ^= edx;\n    if ((int32_t)eax >= (int32_t)ebp)\n        return;\n    eax = ecx;",
     "    eax ^= edx;\n    if ((int32_t)eax > (int32_t)ebp)\n        return;\n    eax = ecx;"),
    ("point-depth", "        div32(ecx);\n        ecx = c;", "        idiv32(ecx);\n        ecx = c;"),
    ("highlight", "        eax = 0xf015;\n    M32(edi + 0x2ec) = eax;", "        eax = 0xf014;\n    M32(edi + 0x2ec) = eax;"),
    ("surface-flag", "    SET_AX(AX + 1);\n    M16(edi + 0x2f4) = AX;", "    M16(edi + 0x2f4) = AX;"),
    ("start-slot", "    esi += 0x3e70;", "    esi += 0x3e60;"),
    ("continue-gate", "    if (M32(edi + 0x2ec) != 0x6900 && M16(edi + 0xdca) != 0)\n        return;\n    if (M8(V(0x206f9120)) == 0) {",
     "    if (M16(edi + 0xdca) != 0)\n        return;\n    if (M8(V(0x206f9120)) == 0) {"),
    ("continue-restore", "        SET_AL(M8(V(0x206f911d)));\n        M8(V(0x206f911c)) = AL;\n    }\n    vertex_b();",
     "    }\n    vertex_b();"),
    ("close-map", "    if (M16(edi + 0xdca) != 0)\n        M32(edi + 0x3c0) = 0x6800;\n    polygon_close();", "    polygon_close();"),
    ("index-swap", "    ebx = AX;\n    { uint16_t t = M16(V(0x206fd6ca)); M16(V(0x206fd6ca)) = AX; SET_AX(t); }",
     "    ebx = AX;\n    { uint16_t t = M16(V(0x206fd6ca)); SET_AX(t); }"),
    ("index-trivial", "    SET_AL(AL & M8(V(0x206f911e)));\n    if (AL != 0)\n        return;", "    SET_AL(AL & M8(V(0x206f911e)));\n    if (AL == 0xff)\n        return;"),
    ("colour-word", "    SET_AX(M16(V(0x206f90f0)));\n    M16(ebx + 0xe) = AX;", "    SET_AX(M16(V(0x206f90f2)));\n    M16(ebx + 0xe) = AX;"),
    ("define-shift", "    SET_AL(M8(V(0x206f9122)));\n    M8(edi + 0xd) = AL;\n    M32(V(0x206f9126)) += 0xa;", "    M32(V(0x206f9126)) += 0xa;"),
    ("sphere-sqrt2", "    SET_DX(0xb504);", "    SET_DX(0xb505);"),
    ("sphere-behind", "    if ((int16_t)AX <= (int16_t)SI)\n        goto out;\n    SET_BP(DX);", "    if ((int16_t)AX < (int16_t)SI)\n        goto out;\n    SET_BP(DX);"),
    ("sphere-part", "    if ((int16_t)BX <= (int16_t)BP)\n        goto part;\n    SET_AX(2);", "    SET_AX(2);"),
    ("reject-skip", "        if (in)\n            eax = 0xc;", "        if (in)\n            eax = 0xe;"),
    ("point-map", "    edi = G;\n    if (M16(edi + 0xdca) == 0) {\n        esi += 2;\n        edi = V(0x206f90c0);",
     "    edi = G;\n    if (1) {\n        esi += 2;\n        edi = V(0x206f90c0);"),
    ("import-ecx", "        \"movl %ecx, _g3d_ecx\\n\\tmovl %edx, _g3d_edx\\n\\t\"",
     "        \"movl %edx, _g3d_edx\\n\\t\""),

    # stage 4: backface tests, shading, the shaded, textured and shaded
    # textured clippers, the fast paths
    ("shade-clamp", "    if (!(AX & 0x8000))\n        SET_AX(0);\n    SET_AX(-AX);", "    if (AX & 0x8000)\n        SET_AX(0);\n    SET_AX(-AX);"),
    ("shade-ambient", "    SET_DX(DX + M16(V(0x206f91bb)));\n    SET_DX(DX + M16(V(0x206f91bb)));", "    SET_DX(DX + M16(V(0x206f91bb)));"),
    ("map-sun", "        a0 = V(0x206fd901), a1 = V(0x206fd905), a2 = V(0x206fd909);", "        a0 = V(0x206fd901), a1 = V(0x206fd903), a2 = V(0x206fd909);"),
    ("plane-distance", "        edx -= M32(esi + 6);\n        sf = edx >> 31;", "        edx -= M32(esi + 6) + 1;\n        sf = edx >> 31;"),
    ("plane-scale", "    eax = M32(esi + 6);\n    SET_CX(M16(V(0x206f9159)));\n    eax = sar32(eax, CL);", "    eax = M32(esi + 6);\n    SET_CX(M16(V(0x206f9159)));\n    eax = sar32(eax, CL + 1);"),
    ("face-eye-y", "        eax -= M32(edi + 0xce0);\n        SET_DX(M16(esi + 8));", "        eax -= M32(edi + 0xce4);\n        SET_DX(M16(esi + 8));"),
    ("face-map-sbb", "        SET_DX(DX + 1 - c);                     /* sbb dx,0xffff */", "        SET_DX(DX + 1);"),
    ("face-skip", "        ecx += ecx;                             /* 0x206e411d */\n        ecx += 0x10;", "        ecx += ecx;                             /* 0x206e411d */\n        ecx += 0xe;"),
    ("face-map-close", "    if (M16(edi + 0xdca) != 0)\n        M32(edi + 0x3c0) = 0x6800;\n    polygon_close();\n}", "    polygon_close();\n}"),
    ("facet-1298", "        if (M32(edi + 0x1298) != 0)\n            goto off;", "        if (M32(edi + 0x1298) == 0)\n            goto off;"),
    ("separation-skip", "    if (!sf)\n        eax = 0xe;", "    if (!sf)\n        eax = 0x10;"),
    ("shaded-cross", "    M32(V(0x206ff7d0)) += edx;\n    eax = ebp;\n    ecx = keep;", "    M32(V(0x206ff7d0)) -= edx;\n    eax = ebp;\n    ecx = keep;"),
    ("textured-cross-v", "    eax = M32(V(0x206f922b)) - M32(V(0x206f9223));\n    p = (int64_t)(int32_t)eax * (int32_t)ebp;\n    p = (int64_t)((uint64_t)p << 1);\n    eax = (uint32_t)p;\n    edx = (uint32_t)((uint64_t)p >> 32);\n    M32(V(0x206f9223)) += edx;\n    eax = ebp;",
     "    eax = M32(V(0x206f922b)) - M32(V(0x206f9223));\n    p = (int64_t)(int32_t)eax * (int32_t)ebp;\n    eax = (uint32_t)p;\n    edx = (uint32_t)((uint64_t)p >> 32);\n    M32(V(0x206f9223)) += edx;\n    eax = ebp;"),
    ("textured-offset", "    SET_BP(BP + M16(edi + 0xc9e));\n    M16(V(0x206f921f)) = 0;", "    SET_BP(BP + M16(edi + 0xc96));\n    M16(V(0x206f921f)) = 0;"),
    ("textured-next-wrap", "    textured_uv(0);\n    lods16();\n    SET_AX(AX << 4);\n    eax = AX;", "    textured_uv(0);\n    lods16();\n    eax = AX;\n    eax <<= 4;"),
    ("fast-depth", "    M16(V(0x206ff948)) = depth;\n    if (M16(V(0x206ff948)) > 4)", "    M16(V(0x206ff948)) = depth;\n    if (M16(V(0x206ff948)) > 3)"),
    ("fast-ratio", "    if ((uint32_t)(((uint64_t)lo << 32) / hi) > M32(V(0x206f9178)))\n        return 1;\n    return 0;\n}\n\n/* 0x206f0d6e */",
     "    if ((uint32_t)(((uint64_t)lo << 32) / hi) >= M32(V(0x206f9178)))\n        return 1;\n    return 0;\n}\n\n/* 0x206f0d6e */"),
    ("fast-quad-order", "    for (i = 3; i >= 0; i--)\n        fast_quad(depth, q[i]);", "    for (i = 0; i < 4; i++)\n        fast_quad(depth, q[i]);"),
    ("fast-12a4", "    M16(edi + 0x12a2) = 1;\n    if (M32(edi + 0x12a4) & 8)\n        return 0;\n    if (count == 3 || count == 4) {\n        textured_fast_shape(count);",
     "    M16(edi + 0x12a2) = 1;\n    if (M32(edi + 0x12a4) & 4)\n        return 0;\n    if (count == 3 || count == 4) {\n        textured_fast_shape(count);"),
    ("plane-facet-c90", "    SET_AX(AX + AX);\n    M16(edi + 0xc90) = AX;\n    polygon_count();", "    M16(edi + 0xc90) = AX;\n    polygon_count();"),
    ("plane-facet-skip", "        SET_CX(CX + 0xe);\n        ecx = CX;", "        SET_CX(CX + 0xc);\n        ecx = CX;"),
    ("st-cross-shade", "    M32(V(0x206f9213)) += edx;\n    eax = ebp;", "    M32(V(0x206f9213)) -= edx;\n    eax = ebp;"),
    ("st-emit-offset", "    ebp += M32(esi + 0xc9c);", "    ebp += M32(esi + 0xc9e);"),
    ("st-emit-shade", "    HG2D_SHADED_TEXTURED_VERTEX(esi, edx, ecx, eax, ebx, ebp, edi);", "    HG2D_SHADED_TEXTURED_VERTEX(esi, edx, ecx, eax, ebx, ebp, ebx);"),
    ("st-point-shade", "    lods32();\n    SET_AX(0);\n    M32(V(0x206f9213)) = eax;\n    edi = G;", "    lods32();\n    M32(V(0x206f9213)) = eax;\n    edi = G;"),
    ("st-close-shade", "        edi = M32(V(0x206f921b));\n        close0_st();", "        edi = M32(V(0x206f9213));\n        close0_st();"),
    ("st-first-uv", "    st_uv(1);", "    st_uv(0);"),
    ("st-stage2", "         (int32_t)ecx > (int32_t)eax, onto_y_high_st, stage3_st)", "         (int32_t)ecx >= (int32_t)eax, onto_y_high_st, stage3_st)"),
    ("st-fast-depth", "    if (M16(V(0x206ff858)) > 4)", "    if (M16(V(0x206ff858)) > 3)"),
    ("st-fast-mid", "    m01 = st_mid(&v[0], &v[1], 0x206ff7f8);", "    m01 = st_mid(&v[0], &v[1], 0x206ff7fc);"),
    ("st-fast-order", "    for (i = 3; i >= 0; i--)\n        st_fast_triangle(depth, t[i]);", "    for (i = 0; i < 4; i++)\n        st_fast_triangle(depth, t[i]);"),
    ("st-fast-shade", "        eax = M32(ebx + 0xc);\n        SET_AX(0);\n        v[slot].c[0] = eax;", "        eax = M32(ebx + 0xc);\n        v[slot].c[0] = eax;"),
    ("st-face-skip", "        uint32_t p = (uint32_t)(uint16_t)count * 6;   /* 0x206ec5ce */", "        uint32_t p = (uint32_t)(uint16_t)count * 5;   /* 0x206ec5ce */"),
]


def main():
    # its own copy, so a hang can be killed without touching other runs
    shutil.copy("bin/difftest/g3ddiff.exe", "bin/difftest/g3ddiff_break.exe")
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
        if os.path.exists("bin/difftest/G3D_ours.dll"):
            os.remove("bin/difftest/G3D_ours.dll")
        build = subprocess.run(["make", "-s", "g3d-break-build", "G3D_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0 or not os.path.exists("bin/difftest/G3D_ours.dll"):
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        # A break can make ours loop where retail does not (a jump that
        # lands backwards): that is caught, and the run is cut at the limit.
        try:
            run = subprocess.run(["wine", "g3ddiff_break.exe", str(N)], cwd="bin/difftest",
                                 capture_output=True, text=True, timeout=LIMIT,
                                 env=dict(os.environ, WINEDEBUG="-all"))
            caught = run.returncode != 0
            last = [l for l in run.stdout.splitlines() if l.startswith("g3ddiff:")]
        except subprocess.TimeoutExpired:
            subprocess.run(["pkill", "-f", "g3ddiff_brea[k].exe"])
            run = subprocess.CompletedProcess([], 1, "hung past %d s" % LIMIT, "")
            caught = True
            last = ["hung past %d s" % LIMIT]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "g3ddiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
