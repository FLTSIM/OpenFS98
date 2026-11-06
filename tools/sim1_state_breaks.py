#!/usr/bin/env python3
"""Isolated source mutations for SIM1 records, host ownership and deinit."""
import fcntl
import json
import os
from pathlib import Path

from sim1_breaks import run

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "bin/sim1-diff"
CASES = [
    ("writer-first-bits", '__asm__ volatile("fldl %1\\n\\tfstpl (%0)"\n'
     '                     : : "r"(out), "m"(*(double *)(D + 0x1538)) : "st", "memory");',
     "memcpy(out, D + 0x1538, 8);"),
    ("writer-pair-order", "sim1_record_pair(out, 0x08, 0x1540", "sim1_record_pair(out, 0x08, 0x1548"),
    ("writer-flag-width", "sim1_write32(out + 0x54, *(volatile uint8_t *)(sim1_dependency(1) + 0x366));",
     "out[0x54] = *(volatile uint8_t *)(sim1_dependency(1) + 0x366);"),
    ("writer-register", "registers->ecx = high;", "registers->ecx = low;"),
    ("save-tag", "arguments[0] = 0x38e;\n    arguments[1] = 0xc0;", "arguments[0] = 0x38f;\n    arguments[1] = 0xc0;"),
    ("save-error-check", "if (registers->eax == 0) {\n        registers->eax = (uintptr_t)sim1_dependency(5);",
     "if (registers->eax != 0) {\n        registers->eax = (uintptr_t)sim1_dependency(5);"),
    ("save-write-size", "arguments[1] = 0xc0;\n        arguments[2] = 0;",
     "arguments[1] = 0xbf;\n        arguments[2] = 0;"),
    ("retain-no-free", "if (sim1_read32(global + 0x11d8) != 0)", "if (0)"),
    ("retain-flags", "arguments[3] = 1;", "arguments[3] = 0;"),
    ("deinit-restore-gate", "if (sim1_read32(global + 0x604) == 1)", "if (sim1_read32(global + 0x604) != 0)"),
    ("deinit-engine-gate", "if (global[0x609] == 1 &&", "if (global[0x609] != 0 &&"),
    ("deinit-loaded-gate", "&& sim1_read32(D + 0x1778) == 0", "&& 1"),
    ("deinit-engine-width", "*(volatile uint16_t *)(global + offsets[i]) = 3;",
     "*(volatile uint32_t *)(global + offsets[i]) = 3;"),
    ("load-flag", "sim1_write32(D + 0x1778, 1);", "sim1_write32(D + 0x1778, 0);"),
    ("save-message", "message == 0x23", "message == 0x24"),
    ("read-field-order", "sim1_read_pair(record, 0x20, 0x13f0)", "sim1_read_pair(record, 0x20, 0x13f8)"),
    ("read-flag-width", "*(volatile uint8_t *)(sim1_dependency(1) + 0x366) = record[0x54];",
     "sim1_write32(sim1_dependency(1) + 0x366, record[0x54]);"),
    ("read-angle-factor", "fmull 0x678(%1)", "fmull 0x680(%1)"),
    ("angle-store-width", "fistps %2", "fistpl %2"),
    ("angle-sine", "fsin\\n\\tfstpl %3", "fcos\\n\\tfstpl %3"),
    ("heading-argument", "arguments[0] = (uint32_t)x; arguments[1] = (uint32_t)y;",
     "arguments[0] = (uint32_t)y; arguments[1] = (uint32_t)x;"),
    ("heading-sign", "(D + 0x3900) - angle", "(D + 0x3900) + angle"),
    ("legacy-angle-sign", "fildq (%%esp)", "fildl (%%esp)"),
    ("legacy-angle-low", "movzwl 0x32(%0), %%ecx", "movzwl 0x30(%0), %%ecx"),
    ("legacy-rate-sign", "value = (int16_t)*(const volatile uint16_t *)", "value = (uint16_t)*(const volatile uint16_t *)"),
    ("legacy-rate-axis", "rate_sources[] = {6, 10, 8}", "rate_sources[] = {6, 8, 10}"),
    ("legacy-velocity-sign", "value = (int32_t)packed;", "value = (int32_t)(packed & 0x7fffffff);"),
    ("legacy-altitude-sign", "wide = (uint64_t)sim1_read32(record + 0x3c);",
     "wide = (int64_t)(int32_t)sim1_read32(record + 0x3c);"),
    ("legacy-velocity-scale", '"m"(*(double *)(D + 0x720))', '"m"(*(double *)(D + 0x728))'),
    ("fallback-extended-zero", "sim1_write32(D + 0x1764, 0);", "sim1_write32(D + 0x1764, 1);"),
    ("fallback-size", "if (size != 0xc0)", "if (size < 0xc0)"),
    ("legacy-load-tag", "else if (registers->eax == 0x5a)", "else if (registers->eax == 0x59)"),
    ("restore-retained", "if (registers->eax != 0) {", "if (0) {"),
    ("restore-precedence", "arguments[0] = 0x38e;\n    arguments[1] = (uintptr_t)&record;",
     "arguments[0] = 0x5a;\n    arguments[1] = (uintptr_t)&record;"),
    ("restore-retained-size", "sim1_record_read_core(registers, (const uint8_t *)(uintptr_t)registers->eax, 0xc0);",
     "sim1_record_read_core(registers, (const uint8_t *)(uintptr_t)registers->eax, 0x58);"),
    ("restore-free", "arguments[0] = (uintptr_t)&record;\n    sim1_import_call(registers, 0, 0x214, 1, arguments);",
     "arguments[0] = (uintptr_t)&record;\n    (void)arguments;"),
    ("velocity-mode", "velocity", "jne .Lvelocity_other", "ja .Lvelocity_other"),
    ("velocity-negation", "velocity", "    fchs\n", "    nop\n"),
    ("velocity-cosine", "velocity", "fld QWORD PTR [esp + 12]\n    fmul QWORD PTR [_sim1_image + 0x1570]",
     "fld QWORD PTR [esp + 12]\n    fmul QWORD PTR [_sim1_image + 0x1578]"),
    ("velocity-scratch", "velocity", "faddp st(2), st\n    fst QWORD PTR [esp + 4]", "faddp st(2), st\n    nop"),
    ("velocity-final-sign", "velocity", "faddp st(1), st\n    fstp QWORD PTR [edx]", "fsubp st(1), st\n    fstp QWORD PTR [edx]"),
]


def main():
    env = dict(os.environ, WINEPREFIX=str(OUTPUT / "wineprefix"), WINEARCH="win32", WINEDEBUG="-all")
    source = (ROOT / "src/sim1/sim1.c").read_text()
    state = (ROOT / "src/sim1/sim1_state.inc").read_text()
    velocity = (ROOT / "src/sim1/sim1_velocity.S").read_text()
    with (OUTPUT / "wine.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        baseline = run(["wine", "bin/sim1-diff/sim1statediff.exe", "432", "relocate"], env)
        if baseline.returncode or "PASS:" not in baseline.stdout:
            raise SystemExit("state baseline failed:\n" + baseline.stdout)
        results = []
        for case in CASES:
            if len(case) == 3:
                name, old, new = case
                kind = "state"
            else:
                name, kind, old, new = case
            text = state if kind == "state" else velocity
            expected_count = 3 if name in ("read-angle-factor", "legacy-angle-sign") else 1
            if text.count(old) != expected_count:
                raise SystemExit(f"{name}: mutation anchor moved ({text.count(old)} matches)")
            directory = OUTPUT / "state-breaks" / name
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "sim1.c").write_text(source)
            (directory / "sim1_state.inc").write_text(state.replace(old, new, 1) if kind == "state" else state)
            (directory / "sim1_velocity.S").write_text(velocity.replace(old, new, 1) if kind == "velocity" else velocity)
            dll = directory / "SIM1_STATE_TEST.DLL"
            dll.unlink(missing_ok=True)
            built = run(["i686-w64-mingw32-gcc", "-std=c99", "-O2", "-Wall", "-Wextra",
                         "-fno-strict-aliasing", "-Iinclude", "-Ibin/sim1-diff", "-shared",
                         "-Wl,--disable-dynamicbase", "-Wl,--image-base,0x22000000", "-o", str(dll),
                         str(directory / "sim1.c"), "src/sim1/sim1_stack.S", "src/sim1/sim1_state_stack.S",
                         str(directory / "sim1_velocity.S"), "tools/sim1_state_test.def"])
            (directory / "build.log").write_text(built.stdout)
            if built.returncode: raise SystemExit(f"{name}: build failed\n{built.stdout}")
            result = run(["wine", "bin/sim1-diff/sim1statediff.exe", "432", "relocate", str(dll)], env)
            (directory / "result.log").write_text(result.stdout)
            caught = (result.returncode == 1 and "side=1" in result.stdout and
                      "unexpected exception" not in result.stdout and "PASS:" not in result.stdout)
            results.append({"name": name, "caught": caught, "returncode": result.returncode})
            print(f"{name}: {'caught' if caught else 'FAILED'}", flush=True)
            if not caught: raise SystemExit(result.stdout)
        (OUTPUT / "state-breaks/results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(f"PASS: {len(results)}/{len(results)} state mutations detected")


if __name__ == "__main__":
    main()
