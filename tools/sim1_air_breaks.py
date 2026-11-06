#!/usr/bin/env python3
"""Rebuilt caller-boundary defects. These do not test AIR record decoding."""
import fcntl
import json
import os
from pathlib import Path

from sim1_breaks import run

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "bin/sim1-diff"
CASES = [
    ("query-key", "arguments[1] = 2;", "arguments[1] = 1;"),
    ("open-extension", "arguments[2] = 0xf;", "arguments[2] = 0x14;"),
    ("open-mode", "arguments[2] = 0xf;\n    arguments[3] = 0;", "arguments[2] = 0xf;\n    arguments[3] = 1;"),
    ("open-error", "if (registers->eax != 0) { registers->eax = 0; return; }", "if (0) { registers->eax = 0; return; }"),
    ("size-error-close", "registers->eax = 0;\n        return;\n    }\n    registers->ecx = size;",
     "arguments[0] = file;\n        sim1_import_call(registers, 0, 0x13c, 1, arguments);\n"
     "        registers->eax = 0;\n        return;\n    }\n    registers->ecx = size;"),
    ("allocation-count", "arguments[0] = 1;\n    arguments[1] = size;", "arguments[0] = 2;\n    arguments[1] = size;"),
    ("allocation-flags", "arguments[2] = (uintptr_t)&data;\n    arguments[3] = 0;", "arguments[2] = (uintptr_t)&data;\n    arguments[3] = 1;"),
    ("read-size", "arguments[1] = data;\n    arguments[2] = size;", "arguments[1] = data;\n    arguments[2] = size - 1;"),
    ("short-read", "registers->eax == 0 && got == size", "registers->eax == 0 && got <= size"),
    ("read-error", "registers->eax == 0 && got == size", "got == size"),
    ("buffer-free", "sim1_import_call(registers, 0, 0x214, 1, arguments);", "(void)arguments;"),
    ("file-close", "sim1_import_call(registers, 0, 0x13c, 1, arguments);", "(void)arguments;"),
    ("parser-result", "result = registers->eax;", "result = !registers->eax;"),
    ("descriptor-copy-size", "arguments[2] = 0x24;", "arguments[2] = 0x28;"),
    ("descriptor-scale", "fmull 0x728(%3)", "fmull 0x720(%3)"),
    ("descriptor-final-width", "movw %%ax, 0x175a(%3)", "movl %%eax, 0x175a(%3)"),
    ("descriptor-edx", "registers->edx = edx;", "registers->edx = 0;"),
    ("init-callback", "arguments[0] = 5; arguments[1] = (uintptr_t)sim1_chain_frame;",
     "arguments[0] = 6; arguments[1] = (uintptr_t)sim1_chain_frame;"),
    ("init-load-error", "sim1_load_air_core(registers);\n    sim1_publish_core(registers);",
     "sim1_load_air_core(registers);\n    if (registers->eax == 0) return;\n    sim1_publish_core(registers);"),
    ("init-publication-order", "sim1_load_air_core(registers);\n    sim1_publish_core(registers);",
     "sim1_publish_core(registers);\n    sim1_load_air_core(registers);"),
]


def main():
    env = dict(os.environ, WINEPREFIX=str(OUTPUT / "wineprefix"), WINEARCH="win32", WINEDEBUG="-all")
    source = (ROOT / "src/sim1/sim1.c").read_text()
    air = (ROOT / "src/sim1/sim1_air.inc").read_text()
    with (OUTPUT / "wine.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        baseline = run(["wine", "bin/sim1-diff/sim1airdiff.exe", "432", "relocate"], env)
        if baseline.returncode or "PASS:" not in baseline.stdout:
            raise SystemExit("AIR boundary baseline failed:\n" + baseline.stdout)
        results = []
        for name, old, new in CASES:
            expected_count = 4 if name == "descriptor-scale" else 1
            if air.count(old) != expected_count:
                raise SystemExit(f"{name}: mutation anchor moved ({air.count(old)} matches)")
            directory = OUTPUT / "air-breaks" / name
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "sim1.c").write_text(source)
            (directory / "sim1_air.inc").write_text(air.replace(old, new, 1))
            dll = directory / "SIM1_AIR_TEST.DLL"
            dll.unlink(missing_ok=True)
            built = run(["i686-w64-mingw32-gcc", "-std=c99", "-O2", "-Wall", "-Wextra",
                         "-fno-strict-aliasing", "-DSIM1_AIR_TEST", "-Iinclude", "-Isrc/sim1", "-Ibin/sim1-diff",
                         "-shared", "-Wl,--disable-dynamicbase", "-Wl,--image-base,0x22000000", "-o", str(dll),
                         str(directory / "sim1.c"), "src/sim1/sim1_stack.S", "src/sim1/sim1_state_stack.S",
                         "src/sim1/sim1_velocity.S", "tools/sim1_air_test.def"])
            (directory / "build.log").write_text(built.stdout)
            if built.returncode: raise SystemExit(f"{name}: build failed:\n{built.stdout}")
            result = run(["wine", "bin/sim1-diff/sim1airdiff.exe", "432", "relocate", str(dll)], env)
            (directory / "result.log").write_text(result.stdout)
            caught = (result.returncode == 1 and "side=1" in result.stdout and
                      "unexpected exception" not in result.stdout and "PASS:" not in result.stdout)
            results.append({"name": name, "caught": caught, "returncode": result.returncode})
            print(f"{name}: {'caught' if caught else 'FAILED'}", flush=True)
            if not caught: raise SystemExit(result.stdout)
        (OUTPUT / "air-breaks/results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(f"PASS: {len(results)}/{len(results)} AIR caller mutations detected (decoder modeled)")


if __name__ == "__main__":
    main()
