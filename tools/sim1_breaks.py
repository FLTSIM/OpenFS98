#!/usr/bin/env python3
"""Build isolated source mutations and demand actual SIM1 comparison failures."""
import fcntl
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "bin/sim1-diff"
VECTOR_ASM = '''    __asm__ volatile("fldl %3\\n\\tfstpl (%0)\\n\\tfldl %4\\n\\tfstpl (%1)\\n\\t"
                     "fldl %5\\n\\tfstpl (%2)"
        : : "r"(a), "r"(b), "r"(c), "m"(*(double *)(D + 0x13c8)),
            "m"(*(double *)(D + 0x13b8)), "m"(*(double *)(D + 0x13c0))
        : "st", "memory");'''
CASES = [
    ("descriptor-short", "c", "i < 0x2c", "i < 0x2b", "slot=18", "output/guards"),
    ("descriptor-long", "c", "i < 0x2c", "i < 0x2d", "slot=18", "output/guards"),
    ("descriptor-source", "c", "source = D + 0x1730", "source = D + 0x1731", "slot=18", "output/guards"),
    ("word-wide", "c", "uint16_t value;", "uint32_t value;", "slot=28", "output/guards"),
    ("word-eax-high", "S", "movw %ax, 8(%esp)", "movl %eax, 8(%esp)", "slot=28", "integer registers/ESP"),
    ("scaled-float", "c", "fildl %1", "flds %1", "slot=2c", "output/guards"),
    ("scaled-factor", "c", "scale = 0x1p-15", "scale = 0x1p-14", "slot=2c", "output/guards"),
    ("scaled-ecx", "S", "movl 8(%esp), %ecx", "nop", "slot=2c", "integer registers/ESP"),
    ("vector-order", "c", "D + 0x13c8", "D + 0x13c0", "slot=30", "output/guards"),
    ("vector-bitcopy", "c", VECTOR_ASM,
     "    memcpy(a, D + 0x13c8, 8); memcpy(b, D + 0x13b8, 8); memcpy(c, D + 0x13c0, 8);",
     "slot=30", ("output/guards", "x87")),
    ("vector-edx", "S", "movl 12(%esp), %edx", "movl 8(%esp), %edx", "slot=30", "integer registers/ESP"),
    ("vector-stack", "S", "ret $12", "ret $8", "slot=30", "integer registers/ESP"),
    ("reset-signed", "c", "wide = (uint64_t)value", "wide = (int64_t)(int32_t)value", "slot=34", "complete seeded state"),
    ("reset-clear", "c", "D + 0x13f8", "D + 0x1400", "slot=34", "complete seeded state"),
    ("reset-operation", "c", "fmull %2\\n\\tfldl %1", "faddl %2\\n\\tfldl %1", "slot=34", "complete seeded state"),
    ("wind-source", "c", "D + 0x640", "D + 0x648", "slot=38", "x87"),
]


def run(command, env=None, timeout=60):
    return subprocess.run(command, cwd=ROOT, env=env, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)


def main():
    env = dict(os.environ, WINEPREFIX=str(OUTPUT / "wineprefix"), WINEARCH="win32", WINEDEBUG="-all")
    source = (ROOT / "src/sim1/sim1.c").read_text()
    assembly = (ROOT / "src/sim1/sim1_stack.S").read_text()
    with (OUTPUT / "wine.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        baseline = run(["wine", "bin/sim1-diff/sim1diff.exe", "24", "relocate"], env)
        if baseline.returncode or "PASS:" not in baseline.stdout:
            raise SystemExit("SIM1 baseline failed:\n" + baseline.stdout)
        results = []
        for name, kind, old, new, slot, reason in CASES:
            directory = OUTPUT / "breaks" / name
            directory.mkdir(parents=True, exist_ok=True)
            text = source if kind == "c" else assembly
            # The ECX restore instruction appears in two wrappers; mutate only
            # the first (scaled). Every other anchor must be unique.
            expected_count = 2 if name == "scaled-ecx" else 1
            if text.count(old) != expected_count:
                raise SystemExit(f"{name}: source anchor moved")
            changed = text.replace(old, new, 1)
            c_file = directory / "sim1.c"
            s_file = directory / "sim1_stack.S"
            c_file.write_text(changed if kind == "c" else source)
            s_file.write_text(changed if kind == "S" else assembly)
            dll = directory / "SIM1_PARTIAL.DLL"
            dll.unlink(missing_ok=True)
            built = run(["i686-w64-mingw32-gcc", "-std=c99", "-O2", "-Wall", "-Wextra",
                         "-fno-strict-aliasing", "-Iinclude", "-Ibin/sim1-diff", "-shared",
                         "-Wl,--disable-dynamicbase", "-Wl,--image-base,0x22000000", "-o", str(dll),
                         str(c_file), str(s_file), "src/sim1/sim1.def"])
            (directory / "build.log").write_text(built.stdout)
            if built.returncode:
                raise SystemExit(f"{name}: build failed:\n{built.stdout}")
            result = run(["wine", "bin/sim1-diff/sim1diff.exe", "24", "relocate", str(dll)], env)
            (directory / "result.log").write_text(result.stdout)
            reasons = reason if isinstance(reason, tuple) else (reason,)
            caught = (result.returncode == 1 and slot in result.stdout and
                      any(item in result.stdout for item in reasons))
            results.append({"name": name, "caught": caught, "returncode": result.returncode})
            print(f"{name}: {'caught' if caught else 'FAILED'}", flush=True)
            if not caught:
                raise SystemExit(result.stdout)
        (OUTPUT / "breaks/results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(f"PASS: {len(results)}/{len(results)} rebuilt mutations detected")


if __name__ == "__main__":
    main()
