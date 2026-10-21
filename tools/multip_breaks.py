#!/usr/bin/env python3
"""Build isolated MULTIP mutations and require completed comparisons.

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
    ("thread security descriptor", "M32(0x538) = 12;", "M32(0x538) = 0;"),
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


PROFILE_MUTATIONS = [
    ("maximum players zero default", "if (value == 0) value = 8;", "if (value == 0) value = 7;"),
    ("maximum observers unsigned clamp", "if (value > 255) value = 255;\n        break;", "if ((int32_t)value > 255) value = 255;\n        break;"),
    ("packet rate upper boundary", "value > 100", "value >= 100"),
    ("track name invalid default", "value > 30) value = 5", "value > 30) value = 30"),
    ("player list update wrong field", "M32(0x178) = 15", "M32(0x174) = 15"),
    ("profile integer read order", "multip_profile_validate(field);", "multip_profile_validate(field); M32(field->value) ^= 1;"),
]

GROUP_MUTATIONS = [
    ("enum consumes generic errors", "result == 0x88770082u ? -1 : 0", "result != 0 ? -1 : 0"),
    ("join retains host flag", "M32(0xaa8) = 0;", "M32(0xaa8) = 1;"),
    ("host counts wrap", "M32(0x998) + M32(0x99c)", "M32(0x998)"),
    ("observer membership destination", "M32(0xaa4), *(uint32_t *)(void *)(local + 4));\n    }\n    return 0;", "M32(0xaa0), *(uint32_t *)(void *)(local + 4));\n    }\n    return 0;"),
]

METADATA_MUTATIONS = [
    ("descriptor probe exact HRESULT", "NULL, &uiSize) != 0x8877001eu)", "NULL, &uiSize) == 0)"),
    ("descriptor allocation initialization", "uiSize, ppDescription, 0)", "uiSize, ppDescription, 1)"),
    ("capacity byte order", "(puiCounts[2] & 255) << 16", "(puiCounts[1] & 255) << 16"),
    ("capacity NOTIMPL handling", "uiResult != 0 && uiResult != 0x80004001u", "uiResult != 0"),
    ("name invalid type host update", "pcName[0] = uiHost != 0 ? 'H' : 'C';", "if (uiType == 0 || uiType == 1 || uiType == 2 || uiType == 4) pcName[0] = uiHost != 0 ? 'H' : 'C';"),
    ("name decoder unknown type", "case 'U': *puiType = 4;", "case 'U': *puiType = 3;"),
    ("player name publication flags", "pName, 2) != 0 ? -1 : 0", "pName, 0) != 0 ? -1 : 0"),
    ("collision countdown width", "M32(0x160) << 16", "M32(0x160) << 15"),
    ("player count includes local", "uiCount = M32(M_PLAYER_COUNT);", "uiCount = M32(M_PLAYER_COUNT); if (uiCount) uiCount--;"),
]


AIRCRAFT_MUTATIONS = [
    ("model selection kind", "(pAircraft, 1) == 0", "(pAircraft, 2) == 0"),
    ("model display state", "A32(pbyModel, 0x68) = 2;\n    if", "A32(pbyModel, 0x68) = 1;\n    if"),
    ("model release kind", "if (A32(pAircraft, 0xc) != 0) {", "if (A32(pAircraft, 0xc) == 1) {"),
    ("model release failure", "if (AC_FN(0x24, multip_aircraft_release_fn)(pAircraft) == 0)", "if (0)"),
]

RECORD_MUTATIONS = [
    ("received aircraft header initialization", "(1, 0x24, (void **)pAircraft, 1)", "(1, 0x24, (void **)pAircraft, 0)"),
    ("received aircraft extra state", "A32(pAircraft, 0x28) = A32(pSource, 0x28);", "A32(pAircraft, 0x28) = 0;"),
    ("received aircraft title copy", "pcTitle);\n    A32(pbyModel, 0x68)", "pcTitle + 1);\n    A32(pbyModel, 0x68)"),
    ("browser session GUID", "(const uint8_t *)pDescription + 8, 16)", "(const uint8_t *)pDescription + 12, 16)"),
    ("browser name list stride", "A32(pRecord, 0x44) = 16;", "A32(pRecord, 0x44) = 32;"),
    ("browser player identity", "A32(pRecord, 0) = uiId;", "A32(pRecord, 0) = uiType;"),
    ("browser invalid final record", "(uint32_t *)(void *)(pbyRecord + 8)) == 0", "(uint32_t *)(void *)(pbyRecord + 8)) != 0"),
    ("browser publication ready", "A32(pRecord, 0x18) = 1;", "A32(pRecord, 0x18) = 0;"),
]


TRACKING_MUTATIONS = [
    ("selection count guard", "if (M32(M_PLAYER_COUNT) == 0)", "if (0)"),
    ("selection mode guard", "M32(M_PLAYER_MODE) != 0 ? multip_find_player", "1 ? multip_find_player"),
    ("view blocked word width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x5dc) != 0", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x5dc) != 0"),
    ("local model visibility width", "pbyGlobal[0x609]", "*(uint32_t *)(void *)(pbyGlobal + 0x609)"),
    ("local model enabled flag", "A32(pbyLocal, 0x34) = 1;", "A32(pbyLocal, 0x34) = 0;"),
    ("following position source", "pbyGlobal + 0xd74, pbyGlobal + 0x560, 24", "pbyGlobal + 0xd74, pbyGlobal + 0x588, 24"),
    ("track duration multiplier", "M32(0xa10) * 18", "M32(0xa10) * 17"),
    ("track callback window", "if (uiArgument == *(uint32_t *)", "if (uiArgument != *(uint32_t *)"),
    ("track callback leftover return", "uiResult = uiArgument;", "uiResult = 0;"),
    ("track clear retains registration", "M32(0xf1c) = 0;", "M32(0xf1c) = 0; M32(0xf20) = 0;"),
    ("selection clear word width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;"),
]


MESSAGE_MUTATIONS = [
    ("aircraft title tag", "auiPacket[0] = 0x100e;", "auiPacket[0] = 0x9999;"),
    ("aircraft title terminator length", "multip_strlen_fn)(acTitle) + 1", "multip_strlen_fn)(acTitle)"),
    ("aircraft title visibility", "auiPacket[2] = *(uint8_t *)", "auiPacket[2] = 0 * *(uint8_t *)"),
    ("request reliable flag", "multip_send(0, auiPacket, sizeof(auiPacket), 1)", "multip_send(0, auiPacket, sizeof(auiPacket), 0)"),
    ("request pending state", "M32(0x984) = 1;", "M32(0x984) = 0;"),
    ("request timestamp", "M32(0xadc) = HOST_FN(0x38, multip_ticks_fn)();", "M32(0xadc) = 0;"),
    ("value payload length", "{uiTag, 4, uiValue}", "{uiTag, 8, uiValue}"),
    ("allocated packet initialization", "(1, 8, (void **)&puiPacket, 1)", "(1, 8, (void **)&puiPacket, 0)"),
    ("allocated packet cleanup", "return HOST_FN(0x214, multip_free_fn)((void **)&puiPacket) != 0 ? -1 : 0;", "return 0;"),
]
for message_tag, call in (
    (0x1001, "multip_message_request(0x1001)"),
    (0x1008, "multip_message_request(0x1008)"),
    (0x1002, "multip_message_request(0x1002)"),
    (0x1009, "multip_message_request(0x1009)"),
    (0x1003, "multip_message_value(uiId, 0x1003, uiValue)"),
    (0x1004, "multip_message_value(uiId, 0x1004, uiValue)"),
    (0x1005, "multip_message_empty(uiId, 0x1005)"),
    (0x1006, "multip_message_empty(uiId, 0x1006)"),
    (0x100f, "multip_message_empty(uiId, 0x100f)"),
    (0x1010, "multip_message_empty(uiId, 0x1010)"),
    (0x1011, "multip_message_allocated(uiId, 0x1011)"),
    (0x100a, "multip_message_allocated(0, 0x100a)"),
    (0x100c, "multip_message_allocated(uiId, 0x100c)"),
):
    MESSAGE_MUTATIONS.append((f"wire tag {message_tag:#x}", call,
                              call.replace(f"{message_tag:#x}", "0x9999")))


PLAYER_MUTATIONS = [
    ("duplicate player ID", "if (multip_find_player(uiId) != NULL)", "if (0)"),
    ("player name allocation initialization", "(1, 21, (void **)pbyPlayer, 1)", "(1, 21, (void **)pbyPlayer, 0)"),
    ("player label model size", "s_abyNametag, sizeof(s_abyNametag))", "s_abyNametag, sizeof(s_abyNametag) - 1)"),
    ("player display name alias", "A32(pbyPlayer, 0xd4) = A32(pbyPlayer, 0);", "A32(pbyPlayer, 0xd4) = 0;"),
    ("player initial type", "A32(pPlayer, 8) = 4;", "A32(pPlayer, 8) = 3;"),
    ("player count publication", "M32(0xef8) = M32(M_PLAYER_COUNT);", "M32(0xef8) = M32(M_PLAYER_COUNT) + 1;"),
    ("player observer count decrement", "if (uiType == 2) M32(0x9a4)--;", "if (uiType == 2) M32(0x9a0)--;"),
    ("player active selection reset", "M32(M_PLAYER_MODE) != 0 && A32(pbyPlayer, 4)", "0 && A32(pbyPlayer, 4)"),
    ("player append local pointer", "pRecord = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660));", "pRecord = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660)); M32(M_LOCAL_PLAYER) = 0;"),
]


def check_variant(directory, source, slice_name):
    source_path = directory / "multip.c"
    source_path.write_text(source)
    command = [
        "i686-w64-mingw32-gcc", "-std=c99", "-Os", "-Wall", "-Wextra",
        "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src/multip"), "-ffreestanding", "-fno-builtin",
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
        ["wine", "multipdiff.exe", "1200"], cwd=directory,
        env={**os.environ, "WINEDEBUG": "-all"}, capture_output=True,
        text=True, timeout=60,
    )
    label = {"send": "send-one/drain/flush", "receive": "receive-one/dequeue",
             "workers": "worker lifecycles",
             "directplay": "DirectPlay loader/lifetime",
             "profile": "profile load/save", "groups": "session/group primitives",
             "metadata": "session descriptors/name metadata",
             "aircraft": "aircraft select/release",
             "records": "aircraft copy/session browser records",
             "tracking": "track-name display", "messages": "application message constructors",
             "players": "player lifecycle helpers"}[slice_name]
    prefix = f"MULTIP {label}: 1200 scenarios, "
    lines = [line for line in result.stdout.splitlines() if line.startswith(prefix)]
    if len(lines) != 1 or result.returncode not in (0, 1):
        raise RuntimeError("Harness did not finish normally:\n" + result.stdout + result.stderr)
    differences = int(lines[0][len(prefix):].split()[0])
    if (differences != 0) != (result.returncode == 1):
        raise RuntimeError("Harness failure outside selected comparison:\n" + result.stdout)
    return differences


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slice", choices=("send", "receive", "workers", "directplay", "profile", "groups", "metadata", "aircraft", "records", "tracking", "messages", "players", "all"), default="all")
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
    for slice_name, mutations in (("profile", PROFILE_MUTATIONS),
                                  ("groups", GROUP_MUTATIONS),
                                  ("metadata", METADATA_MUTATIONS),
                                  ("aircraft", AIRCRAFT_MUTATIONS),
                                  ("records", RECORD_MUTATIONS),
                                  ("tracking", TRACKING_MUTATIONS),
                                  ("messages", MESSAGE_MUTATIONS),
                                  ("players", PLAYER_MUTATIONS)):
        if args.slice in (slice_name, "all"):
            variants.extend((slice_name, *mutation) for mutation in mutations)
    source = (ROOT / "src/multip/multip.c").read_text()
    for include in ("multip_profile.inc", "multip_session.inc",
                    "multip_aircraft.inc", "multip_records.inc", "multip_tracking.inc", "multip_messages.inc", "multip_players.inc"):
        source = source.replace('#include "' + include + '"',
                                (ROOT / "src/multip" / include).read_text())
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
