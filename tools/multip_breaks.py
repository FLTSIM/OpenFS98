#!/usr/bin/env python3
"""Build isolated MULTIP transport mutations and require completed comparisons.

Run after `make multipdiff`. Crashes, timeouts and fixture failures do not
count as catches. All generated sources and DLLs live in /tmp.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MUTATIONS = [
    ("busy retry", "} while ((uint32_t)result == 0x8877010eu);", "} while (0);"),
    ("ignored HRESULT", "    if (HOST_FN(0x214, multip_free_fn)(&record->payload) != 0)",
     "    if (result != 0) return -1;\n"
     "    if (HOST_FN(0x214, multip_free_fn)(&record->payload) != 0)"),
    ("send arguments", "record->to_id, record->flags, record->payload,",
     "record->flags, record->to_id, record->payload,"),
    ("payload cleanup", "if (HOST_FN(0x214, multip_free_fn)(&record->payload) != 0)", "if (0)"),
    ("plain drain loop", "    while (multip_queue_count((struct multip_queue *)(uintptr_t)\n"
     "                              M32(M_SEND_QUEUE_PLAIN)) != 0)",
     "    if (multip_queue_count((struct multip_queue *)(uintptr_t)\n"
     "                           M32(M_SEND_QUEUE_PLAIN)) != 0)"),
    ("flagged drain limit", "    return multip_send_one((struct multip_queue *)(uintptr_t)\n"
     "                           M32(M_SEND_QUEUE_FLAGS)) != 0 ? -1 : 0;",
     "    while (multip_queue_count((struct multip_queue *)(uintptr_t)\n"
     "                              M32(M_SEND_QUEUE_FLAGS)) != 0)\n"
     "        if (multip_send_one((struct multip_queue *)(uintptr_t)\n"
     "                            M32(M_SEND_QUEUE_FLAGS)) != 0) return -1;\n"
     "    return 0;"),
    ("dequeue tail", "    if (queue->head == NULL)\n        queue->tail = NULL;",
     "    if (queue->head == NULL)\n        queue->tail = node;"),
    ("flush priority", "for (offset = M_SEND_QUEUE_PLAIN; ; offset = M_SEND_QUEUE_FLAGS)",
     "for (offset = M_SEND_QUEUE_FLAGS; ; offset = M_SEND_QUEUE_PLAIN)"),
    ("node free failure", "    return result != 0 ? -1 : 0;", "    (void)result;\n    return 0;"),
]

RECEIVE_MUTATIONS = [
    ("receive retry", "if ((uint32_t)result != 0x8877001eu)", "if (1)"),
    ("receive flags", "receive(directplay, &record->from_id, &record->to_id, 1,",
     "receive(directplay, &record->from_id, &record->to_id, 0,"),
    ("receive timestamp", "record->timestamp = HOST_FN(0x38, multip_ticks_fn)();",
     "record->timestamp = 0;"),
    ("receive success flag", "    *received = 1;\n    record->timestamp",
     "    *received = 0;\n    record->timestamp"),
    ("receive error cleanup", "if (record->payload != NULL &&\n            HOST_FN",
     "if (0 &&\n            HOST_FN"),
    ("receive dequeue flag", "    if (*record != NULL)\n        *received = 1;",
     "    if (*record != NULL)\n        *received = 0;"),
]

WORKER_MUTATIONS = [
    ("event setup order",
     "    M32(M_SEND_EVENT) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));\n"
     "    M32(M_SEND_STOP) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));",
     "    M32(M_SEND_STOP) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));\n"
     "    M32(M_SEND_EVENT) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));"),
    ("repeated start guard", "if (M32(M_RECEIVE_RUNNING) == 0)",
     "if (M32(M_RECEIVE_RUNNING) != 0)"),
    ("thread creation failure result", "(LPDWORD)(uintptr_t)(D + M_RECEIVE_THREAD_ID)));",
     "(LPDWORD)(uintptr_t)(D + M_RECEIVE_THREAD_ID)));\n"
     "        if (M32(M_RECEIVE_THREAD) == 0) return -1;"),
    ("thread security descriptor", "[0x538] = 12,", "[0x538] = 0,"),
    ("receive thread handle retention",
     "    WaitForSingleObject((HANDLE)(uintptr_t)M32(M_RECEIVE_THREAD), 100);",
     "    WaitForSingleObject((HANDLE)(uintptr_t)M32(M_RECEIVE_THREAD), 100);\n"
     "    CloseHandle((HANDLE)(uintptr_t)M32(M_RECEIVE_THREAD));"),
    ("shutdown wait duration",
     "WaitForSingleObject((HANDLE)(uintptr_t)M32(M_SEND_THREAD), 100);",
     "WaitForSingleObject((HANDLE)(uintptr_t)M32(M_SEND_THREAD), 101);"),
    ("send thread handle close",
     "return CloseHandle((HANDLE)(uintptr_t)M32(M_SEND_THREAD)) ? 0 : -1;", "return 0;"),
    ("send stop queue clearing",
     "if (multip_queue_clear((struct multip_queue *)(uintptr_t)\n"
     "                          M32(M_SEND_QUEUE_FLAGS)) != 0)", "if (0)"),
    ("send worker error handling",
     "        else if (wait_result == WAIT_OBJECT_0 + 1)\n            multip_send_drain();",
     "        else if (wait_result == WAIT_OBJECT_0 + 1)\n"
     "            if (multip_send_drain() != 0) return 0xffffffffu;"),
    ("receive until empty", "} while (received != 0);", "} while (0);"),
    ("receive worker failure flag",
     "                if (multip_receive(&received) != 0)\n                    return 0xffffffffu;",
     "                if (multip_receive(&received) != 0) {\n"
     "                    M32(M_RECEIVE_RUNNING) = 0; return 0xffffffffu; }"),
    ("queue clear record free",
     "if (HOST_FN(0x214, multip_free_fn)((void **)queue->tail) != 0)", "if (0)"),
    ("queue clear failure lock",
     "        if (HOST_FN(0x214, multip_free_fn)((void **)&queue->tail) != 0)\n"
     "            return -1;",
     "        if (HOST_FN(0x214, multip_free_fn)((void **)&queue->tail) != 0) {\n"
     "            LeaveCriticalSection(&queue->lock); return -1; }"),
    ("shared start flag", "        M32(M_WORKERS_STARTED) = 1;",
     "        M32(M_WORKERS_STARTED) = 0;"),
    ("shared stop flag", "        M32(M_WORKERS_STARTED) = 0;",
     "        M32(M_WORKERS_STARTED) = 1;"),
    ("queue critical section deletion", "    DeleteCriticalSection(&(*queue)->lock);", "    (void)0;"),
]

DIRECTPLAY_MUTATIONS = [
    ("loader export selection", "if (M32(M_DIRECTPLAY_ENUMERATE) != 0)",
     "if (M32(M_DIRECTPLAY_CREATE) != 0)"),
    ("loader status retention", "        M32(M_DIRECTPLAY_LOADED) = 1;",
     "        M32(M_DIRECTPLAY_LOADED) = 1;\n    else M32(M_DIRECTPLAY_LOADED) = 0;"),
    ("export descriptor order",
     "M32(M_DIRECTPLAY_EXPORTS + 0x04) = D + M_DIRECTPLAY_ENUMERATE;",
     "M32(M_DIRECTPLAY_EXPORTS + 0x04) = D + M_DIRECTPLAY_LOBBY;"),
    ("unload status retention", "    M32(M_DIRECTPLAY) = 0;",
     "    M32(M_DIRECTPLAY) = 0;\n    M32(M_DIRECTPLAY_LOADED) = 0;"),
    ("unload release state", "    void *directplay = (void *)(uintptr_t)M32(M_DIRECTPLAY);",
     "    void *directplay = (void *)(uintptr_t)M32(M_DIRECTPLAY);\n    M32(M_DIRECTPLAY) = 0;"),
    ("first query leak",
     "    if (query(created, &s_iid_directplay2a, &queried) != 0)\n        return -1;",
     "    if (query(created, &s_iid_directplay2a, &queried) != 0) {\n"
     "        release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)created + 0x08);\n"
     "        release(created); return -1; }"),
    ("second query object", "query(created, &s_iid_directplay3a,",
     "query(queried, &s_iid_directplay3a,"),
    ("second query failure result", "        M32(M_DIRECTPLAY_QUERIED) = 1;",
     "        M32(M_DIRECTPLAY_QUERIED) = 1;\n    else return -1;"),
    ("temporary release order",
     "    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)queried + 0x08);\n"
     "    release(queried);\n"
     "    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)created + 0x08);\n"
     "    release(created);",
     "    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)created + 0x08);\n"
     "    release(created);\n"
     "    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)queried + 0x08);\n"
     "    release(queried);"),
    ("factory outer argument", "(provider, &created, NULL) != 0)",
     "(provider, &created, (void *)1) != 0)"),
    ("query flag reset", "    M32(M_DIRECTPLAY_QUERIED) = 0;",
     "    M32(M_DIRECTPLAY_QUERIED) = 1;"),
]


def check_variant(directory, source, slice_name):
    source_path = directory / "multip.c"
    source_path.write_text(source)
    command = [
        "i686-w64-mingw32-gcc", "-std=c99", "-Os", "-Wall", "-Wextra",
        "-I" + str(ROOT / "include"), "-ffreestanding", "-fno-builtin",
        "-fno-asynchronous-unwind-tables", "-fomit-frame-pointer",
        "-DMULTIP_DIFFTEST", "-shared", "-nostdlib",
        "-Wl,--enable-stdcall-fixup", "-Wl,--entry=_DllMain@12",
        "-Wl,--disable-dynamicbase", "-Wl,--disable-nxcompat",
        "-Wl,--image-base=0x20850000", "-o", str(directory / "MULTIP_ours.dll"),
        str(source_path), str(ROOT / "bin/multip.res"),
        str(ROOT / "src/multip/multip.def"), "-lkernel32",
    ]
    subprocess.run(command, check=True, capture_output=True, text=True)
    result = subprocess.run(
        ["wine", "multipdiff.exe", "100"], cwd=directory,
        env={**os.environ, "WINEDEBUG": "-all"}, capture_output=True,
        text=True, timeout=60,
    )
    label = {"send": "send-one/drain/flush", "receive": "receive-one/dequeue",
             "workers": "worker lifecycles",
             "directplay": "DirectPlay loader/lifetime"}[slice_name]
    prefix = f"MULTIP {label}: 100 scenarios, "
    lines = [line for line in result.stdout.splitlines() if line.startswith(prefix)]
    if len(lines) != 1 or result.returncode not in (0, 1):
        raise RuntimeError("Harness did not finish normally:\n" + result.stdout + result.stderr)
    differences = int(lines[0][len(prefix):].split()[0])
    if (differences != 0) != (result.returncode == 1):
        raise RuntimeError("Harness failure outside selected comparison:\n" + result.stdout)
    return differences


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slice", choices=("send", "receive", "workers", "directplay", "all"), default="all")
    args = parser.parse_args()
    variants = []
    if args.slice in ("send", "all"):
        variants.extend(("send", *mutation) for mutation in MUTATIONS)
    if args.slice in ("receive", "all"):
        variants.extend(("receive", *mutation) for mutation in RECEIVE_MUTATIONS)
    if args.slice in ("workers", "all"):
        variants.extend(("workers", *mutation) for mutation in WORKER_MUTATIONS)
    if args.slice in ("directplay", "all"):
        variants.extend(("directplay", *mutation) for mutation in DIRECTPLAY_MUTATIONS)
    source = (ROOT / "src/multip/multip.c").read_text()
    with tempfile.TemporaryDirectory(prefix="multip-breaks-", dir="/tmp") as temp:
        directory = Path(temp)
        for name in ("multipdiff.exe", "MULTIP_retail.dll"):
            shutil.copy2(ROOT / "bin/difftest" / name, directory / name)
        if check_variant(directory, source, "send") != 0:
            raise RuntimeError("Unmodified baseline differs")
        print("baseline: 0 differences", flush=True)
        for slice_name, name, before, after in variants:
            if source.count(before) != 1:
                raise RuntimeError(f"Mutation {name!r} no longer matches exactly once")
            differences = check_variant(directory, source.replace(before, after), slice_name)
            if differences == 0:
                raise RuntimeError(f"Mutation {name!r} was not caught")
            print(f"{name}: caught ({differences} differing scenarios)", flush=True)
    print(f"MULTIP {args.slice} mutations: {len(variants)}/{len(variants)} caught")


if __name__ == "__main__":
    main()
