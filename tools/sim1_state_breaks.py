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
    ("save-tag", "arguments[0] = 0x38e;", "arguments[0] = 0x38f;"),
    ("save-error-check", "if (registers->eax == 0)", "if (registers->eax != 0)"),
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
]


def main():
    env = dict(os.environ, WINEPREFIX=str(OUTPUT / "wineprefix"), WINEARCH="win32", WINEDEBUG="-all")
    source = (ROOT / "src/sim1/sim1.c").read_text()
    state = (ROOT / "src/sim1/sim1_state.inc").read_text()
    with (OUTPUT / "wine.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        baseline = run(["wine", "bin/sim1-diff/sim1statediff.exe", "144", "relocate"], env)
        if baseline.returncode or "PASS:" not in baseline.stdout:
            raise SystemExit("state baseline failed:\n" + baseline.stdout)
        results = []
        for name, old, new in CASES:
            expected_count = 3 if name == "read-angle-factor" else 1
            if state.count(old) != expected_count:
                raise SystemExit(f"{name}: mutation anchor moved ({state.count(old)} matches)")
            directory = OUTPUT / "state-breaks" / name
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "sim1.c").write_text(source)
            (directory / "sim1_state.inc").write_text(state.replace(old, new, 1))
            dll = directory / "SIM1_STATE_TEST.DLL"
            dll.unlink(missing_ok=True)
            built = run(["i686-w64-mingw32-gcc", "-std=c99", "-O2", "-Wall", "-Wextra",
                         "-fno-strict-aliasing", "-Iinclude", "-Ibin/sim1-diff", "-shared",
                         "-Wl,--disable-dynamicbase", "-Wl,--image-base,0x22000000", "-o", str(dll),
                         str(directory / "sim1.c"), "src/sim1/sim1_stack.S", "src/sim1/sim1_state_stack.S",
                         "tools/sim1_state_test.def"])
            (directory / "build.log").write_text(built.stdout)
            if built.returncode: raise SystemExit(f"{name}: build failed\n{built.stdout}")
            result = run(["wine", "bin/sim1-diff/sim1statediff.exe", "144", "relocate", str(dll)], env)
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
