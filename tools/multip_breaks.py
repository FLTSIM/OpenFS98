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
    ("descriptor probe exact HRESULT", "NULL, &size) != 0x8877001eu)", "NULL, &size) == 0)"),
    ("descriptor allocation initialization", "size, description_out, 0)", "size, description_out, 1)"),
    ("capacity byte order", "(counts[2] & 255) << 16", "(counts[1] & 255) << 16"),
    ("capacity NOTIMPL handling", "result != 0 && result != 0x80004001u", "result != 0"),
    ("name invalid type host update", "name_text[0] = host != 0 ? 'H' : 'C';", "if (type == 0 || type == 1 || type == 2 || type == 4) name_text[0] = host != 0 ? 'H' : 'C';"),
    ("name decoder unknown type", "case 'U': *type = 4;", "case 'U': *type = 3;"),
    ("player name publication flags", "name, 2) != 0 ? -1 : 0", "name, 0) != 0 ? -1 : 0"),
    ("collision countdown width", "M32(0x160) << 16", "M32(0x160) << 15"),
    ("player count includes local", 'int32_t multip_player_count_types(void)\n{\n    uint32_t count = M32(M_PLAYER_COUNT);', 'int32_t multip_player_count_types(void)\n{\n    uint32_t count = M32(M_PLAYER_COUNT); if (count) count--;'),
]


AIRCRAFT_MUTATIONS = [
    ("model selection kind", "(aircraft, 1) == 0", "(aircraft, 2) == 0"),
    ("model display state", "A32(model, 0x68) = 2;\n    if", "A32(model, 0x68) = 1;\n    if"),
    ("model release kind", "if (A32(aircraft, 0xc) != 0) {", "if (A32(aircraft, 0xc) == 1) {"),
    ("model release failure", "if (AC_FN(0x24, multip_aircraft_release_fn)(aircraft) == 0)", "if (0)"),
]

RECORD_MUTATIONS = [
    ("received aircraft header initialization", "(1, 0x24, (void **)aircraft, 1)", "(1, 0x24, (void **)aircraft, 0)"),
    ("received aircraft extra state", "A32(aircraft, 0x28) = A32(source, 0x28);", "A32(aircraft, 0x28) = 0;"),
    ("received aircraft title copy", "title);\n    A32(model, 0x68)", "title + 1);\n    A32(model, 0x68)"),
    ("browser session GUID", "(const uint8_t *)description + 8, 16)", "(const uint8_t *)description + 12, 16)"),
    ("browser name list stride", "A32(record, 0x44) = 16;", "A32(record, 0x44) = 32;"),
    ("browser player identity", "A32(record, 0) = id;", "A32(record, 0) = type;"),
    ("browser invalid final record", "(uint32_t *)(void *)(record + 8)) == 0", "(uint32_t *)(void *)(record + 8)) != 0"),
    ("browser publication ready", "A32(record, 0x18) = 1;", "A32(record, 0x18) = 0;"),
]


TRACKING_MUTATIONS = [
    ("selection count guard", "if (M32(M_PLAYER_COUNT) == 0)", "if (0)"),
    ("selection mode guard", "M32(M_PLAYER_MODE) != 0 ? multip_find_player", "1 ? multip_find_player"),
    ("view blocked word width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x5dc) != 0", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x5dc) != 0"),
    ("local model visibility width", "global[0x609]", "*(uint32_t *)(void *)(global + 0x609)"),
    ("local model enabled flag", "A32(local, 0x34) = 1;", "A32(local, 0x34) = 0;"),
    ("following position source", "global + 0xd74, global + 0x560, 24", "global + 0xd74, global + 0x588, 24"),
    ("track duration multiplier", "M32(0xa10) * 18", "M32(0xa10) * 17"),
    ("track callback window", "if (argument == *(uint32_t *)", "if (argument != *(uint32_t *)"),
    ("track callback leftover return", "result = argument;", "result = 0;"),
    ("track clear retains registration", "M32(0xf1c) = 0;", "M32(0xf1c) = 0; M32(0xf20) = 0;"),
    ("selection clear word width", 'int32_t multip_selection_reset(void)\n{\n    M32(M_PLAYER_MODE) = 0;\n    M32(M_ACTIVE_PLAYER_ID) = 0;\n    *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;', 'int32_t multip_selection_reset(void)\n{\n    M32(M_PLAYER_MODE) = 0;\n    M32(M_ACTIVE_PLAYER_ID) = 0;\n    *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;'),
]


MESSAGE_MUTATIONS = [
    ("aircraft title tag", "packet[0] = 0x100e;", "packet[0] = 0x9999;"),
    ("aircraft title terminator length", "size = HOST_FN(0x2b8, multip_strlen_fn)(title) + 1;\n    packet[1] = size;", "size = HOST_FN(0x2b8, multip_strlen_fn)(title);\n    packet[1] = size;"),
    ("aircraft title visibility", "packet[2] = *(uint8_t *)", "packet[2] = 0 * *(uint8_t *)"),
    ("request reliable flag", "multip_send(0, packet, sizeof(packet), 1)", "multip_send(0, packet, sizeof(packet), 0)"),
    ("request pending state", "M32(0x984) = 1;\n    M32(0xadc)", "M32(0x984) = 0;\n    M32(0xadc)"),
    ("request timestamp", "M32(0xadc) = HOST_FN(0x38, multip_ticks_fn)();", "M32(0xadc) = 0;"),
    ("value payload length", "{tag, 4, value}", "{tag, 8, value}"),
    ("allocated packet initialization", "(1, 8, (void **)&packet, 1)", "(1, 8, (void **)&packet, 0)"),
    ("allocated packet cleanup", "if (multip_send(id, packet, 8, 1) != 0)\n        return -1;\n    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;", "if (multip_send(id, packet, 8, 1) != 0)\n        return -1;\n    return 0;"),
]
for message_tag, call in (
    (0x1001, "multip_message_request(0x1001)"),
    (0x1008, "multip_message_request(0x1008)"),
    (0x1002, "multip_message_request(0x1002)"),
    (0x1009, "multip_message_request(0x1009)"),
    (0x1003, "multip_message_value(id, 0x1003, value)"),
    (0x1004, "multip_message_value(id, 0x1004, value)"),
    (0x1005, "multip_message_empty(id, 0x1005)"),
    (0x1006, "multip_message_empty(id, 0x1006)"),
    (0x100f, "multip_message_empty(id, 0x100f)"),
    (0x1010, "multip_message_empty(id, 0x1010)"),
    (0x1011, "multip_message_allocated(id, 0x1011)"),
    (0x100a, "multip_message_allocated(0, 0x100a)"),
    (0x100c, "multip_message_allocated(id, 0x100c)"),
):
    MESSAGE_MUTATIONS.append((f"wire tag {message_tag:#x}", call,
                              call.replace(f"{message_tag:#x}", "0x9999")))


PLAYER_MUTATIONS = [
    ("duplicate player ID", "if (multip_find_player(id) != NULL)", "if (0)"),
    ("player name allocation initialization", "(1, 21, (void **)player, 1)", "(1, 21, (void **)player, 0)"),
    ("player label model size", "s_nametag, sizeof(s_nametag))", "s_nametag, sizeof(s_nametag) - 1)"),
    ("player display name alias", "A32(player, 0xd4) = A32(player, 0);", "A32(player, 0xd4) = 0;"),
    ("player initial type", "A32(player, 8) = 4;\n    A32(player, 0x4c) = 0;", "A32(player, 8) = 3;\n    A32(player, 0x4c) = 0;"),
    ("player count publication", "M32(0xef8) = M32(M_PLAYER_COUNT);", "M32(0xef8) = M32(M_PLAYER_COUNT) + 1;"),
    ("player observer count decrement", "if (type == 2) M32(0x9a4)--;", "if (type == 2) M32(0x9a0)--;"),
    ("player active selection reset", "M32(M_PLAYER_MODE) != 0 && A32(player, 4)", "0 && A32(player, 4)"),
    ("player append local pointer", "record = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660));", "record = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660)); M32(M_LOCAL_PLAYER) = 0;"),
]


LOCAL_MUTATIONS = [
    ("local reserved name flags", "name_fields[1] = reserved;", "name_fields[1] = 0;"),
    ("local stack slot", "movl -0x400(%esp), %eax", "movl -0x3fc(%esp), %eax"),
    ("startup stack slot", "movl -0x404(%esp), %eax", "movl -0x400(%esp), %eax"),
    ("local name record size", "name_fields[0] = 16;", "name_fields[0] = 12;"),
    ("local receive event", "(HANDLE)(uintptr_t)M32(M_RECEIVE_EVENT), NULL, 0, 0)", "NULL, NULL, 0, 0)"),
    ("local flag after creation", "A32(local, 0xc) = 1;", "A32(local, 0xc) = 0;"),
    ("host player selection", "A32(local, 8) = M32(0x1a4);", "A32(local, 8) = 4;"),
    ("host player count", "if (type == 1) M32(0x9a0)++;", "if (type == 1) M32(0x9a4)++;"),
    ("join player request", "if (multip_message_1001() != 0) return -1;", "if (multip_message_1002() != 0) return -1;"),
    ("join observer request", "if (multip_message_1002() != 0) return -1;", "if (multip_message_1001() != 0) return -1;"),
    ("startup name publication", "if (multip_message_1002() != 0) return -1;\n        }\n    }\n    if (multip_player_publish_name() != 0)", "if (multip_message_1002() != 0) return -1;\n        }\n    }\n    if (0)"),
    ("startup capacity publication", "if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)\n        return -1;\n    local", "if (0)\n        return -1;\n    local"),
]


FRAME_MUTATIONS = [
    ("session frame input offset", "case_index = code - 0x21;", "case_index = code - 0x20;"),
    ("session frame outer return", "if (case_index >= sizeof(s_frame_actions))\n        return case_index;", "if (case_index >= sizeof(s_frame_actions))\n        return 0;"),
    ("session frame inner return", "    return case_index;\n}", "    return 0;\n}"),
    ("session frame aircraft refresh", "result = multip_player_refresh_local();", "result = 0;"),
    ("session frame collision reset", "return (uint32_t)multip_player_reset_collision((void *)(uintptr_t)M32(M_LOCAL_PLAYER));", "return 0;"),
    ("session frame view block", "case 3: M32(0xad4) = 1;", "case 3: M32(0xad4) = 0;"),
    ("session frame start flag", "M32(0x954) = 1;", "M32(0x954) = 0;"),
    ("session frame stop flag", "M32(0x954) = 0;", "M32(0x954) = 1;"),
]
PLAYER_MUTATIONS.extend([
    ("peer enumeration player filter", "type == 1 && multip_player_add_remote", "type == 3 && multip_player_add_remote"),
    ("peer enumeration callback stop", "if (type == 1 && multip_player_add_remote(name, id) != 0)\n        return 0;", "if (type == 1 && multip_player_add_remote(name, id) != 0)\n        return 1;"),
    ("peer enumeration raw HRESULT", "multip_player_enumerate_callback, NULL, 16);", "multip_player_enumerate_callback, NULL, 16) != 0 ? -1 : 0;"),
    ("receive option membership", "if (M32(0xac0) != 0)", "if (M32(0xac0) == 0)"),
    ("ping timestamp field", "A32(player, 0xfc) = HOST_FN", "A32(player, 0xf8) = HOST_FN"),
    ("ping reliable flag", "A32(player, 0xfc) = HOST_FN(0x38, multip_ticks_fn)();\n    return multip_send(id, packet, sizeof(packet), 0)", "A32(player, 0xfc) = HOST_FN(0x38, multip_ticks_fn)();\n    return multip_send(id, packet, sizeof(packet), 1)"),
])
MESSAGE_MUTATIONS.append(("host departure tag", "multip_message_allocated(0, 0x1012)", "multip_message_allocated(0, 0x9999)"))


DRAW_MUTATIONS = [
    ("drawing pending guard", "    void *state;\n\n    if (M32(0x984) != 0)", "    void *state;\n\n    if (0)"),
    ("drawing first interval", "M32(0x960) - 200;", "M32(0x960) - 199;"),
    ("drawing captured count", "M32(0x960) = HOST_FN(0x38, multip_ticks_fn)();", "M32(0x960) = HOST_FN(0x38, multip_ticks_fn)(); count = M32(M_PLAYER_COUNT);"),
    ("drawing local guard", "A32(player, 0xc) != 0 || state == NULL", "state == NULL"),
    ("drawing player type", "A32(player, 8) != 1 ||", "A32(player, 8) == 1 ||"),
    ("drawing visibility", "A32(player, 0x4c) == 0 ||", "A32(player, 0x4c) != 0 ||"),
    ("drawing model ownership", "if (A32(player, 0x1c) != 0)", "if (A32(player, 0x1c) == 0)"),
    ("drawing label state reload", "if (M32(0x188) != 0) {\n            state = *(void **)(void *)(player + 0x28);", "if (M32(0x188) != 0) {"),
    ("drawing label vector offset", "player + 0xd8, (uint8_t *)state + 4", "player + 0xdc, (uint8_t *)state + 4"),
    ("drawing label option", "if (M32(0x188) != 0) {", "if (M32(0x188) == 0) {"),
    ("drawing registration flag", "M32(0x958) = 1;", "M32(0x958) = 0;"),
    ("drawing removal flag", "M32(0x958) = 0;", "M32(0x958) = 1;"),
]


MOTION_MUTATIONS = [
    ("prediction elapsed timestamp", "timestamp = A32(player, 0x94)", "timestamp = A32(player, 0x90)"),
    ("prediction elapsed scale", "s_multip_rdata + 0x38", "s_multip_rdata + 0x30"),
    ("prediction feedback gate", "if (A32(player, 0x98) != 0)", "if (0)"),
    ("prediction feedback correction", '"m"(MD(0x130))', '"m"(MD(0x118))'),
    ("prediction heading lower edge", "A32(state, 0x10) >= 0x4000", "A32(state, 0x10) > 0x4000"),
    ("prediction heading upper edge", "A32(state, 0x10) <= 0xc000", "A32(state, 0x10) < 0xc000"),
    ("prediction heading format", "unsigned_heading ? 0xbc : 0xc4", "0xc4"),
    ("prediction orientation copy", "(uint8_t *)player + 0x6c, 12", "(uint8_t *)player + 0x6c, 8"),
    ("prediction feedback clear", "A32(player, 0x98) = 0;", "A32(player, 0x98) = 1;"),
    ("prediction first interval", "M32(0x978) = M32(0x974) - 200", "M32(0x978) = M32(0x974) - 199"),
    ("selection frame registration", "M32(0x968) = 1;", "M32(0x968) = 0;"),
    ("selection frame removal", "M32(0x968) = 0;", "M32(0x968) = 1;"),
]
# The three feedback branches have identical predicates: select the first.
MOTION_MUTATIONS[2] = ("prediction feedback gate", "if (A32(player, 0x98) != 0) {\n        UTIL_FN(0xc8, multip_fixed_to_double_fn)(&anchor, (uint8_t *)player + 0x54);", "if (0) {\n        UTIL_FN(0xc8, multip_fixed_to_double_fn)(&anchor, (uint8_t *)player + 0x54);")


COMMAND_MUTATIONS = [
    ("observer view mode guard", "M32(M_LOCAL_PLAYER), 8) == 2", "M32(M_LOCAL_PLAYER), 8) == 1"),
    ("observer view enabled flag", "M32(0xa58) = 1;", "M32(0xa58) = 2;"),
    ("cycle first candidate", "        index++;\n        if (index >= count)", "        index += 2;\n        if (index >= count)"),
    ("cycle local eligibility", "A32(player, 0xc) == 1)", "A32(player, 0xc) == 0)"),
    ("cycle view word width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 1;", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 1;"),
    ("observer confirmation result", "& 0x40) == 0", "& 0x20) == 0"),
    ("observer pending flag", "M32(0xa64) = 1;", "M32(0xa64) = 0;"),
    ("observer capacity increment", "if (M32(0x9a4) == M32(0x99c)) M32(0x99c)++;", "M32(0x99c)++;"),
    ("observer player decrement", "        M32(0x9a0)--;\n        M32(0x9a4)++;", "        M32(0x9a0)++;\n        M32(0x9a4)++;"),
    ("follow stack latitude padding", '"movzwl -0x1c(%esp), %edx\\n\\tmovzwl -0x24(%esp), %eax', '"movzwl -0x1c(%esp), %edx\\n\\tmovzwl -0x20(%esp), %eax'),
    ("follow heading copy", "position + 32,\n        (void *)(uintptr_t)(A32(player, 0x28) + 0x24), 4", "position + 32,\n        (void *)(uintptr_t)(A32(player, 0x28) + 0x20), 4"),
    ("follow radius source", "peer_radius = A32(player, 0x30);", "peer_radius = A32(player, 0x2c);"),
    ("commands registration flag", "M32(0x96c) = 1;", "M32(0x96c) = 0;"),
    ("commands removal flag", "M32(0x96c) = 0;", "M32(0x96c) = 1;"),
]


WEATHER_MUTATIONS = [
    ("weather payload length", "A32(packet, 4) = 0x254;", "A32(packet, 4) = 0x250;"),
    ("weather trailing padding", "A32(packet, 0x258) = padding;", "A32(packet, 0x258) = 0;"),
    ("weather stack slot", '"movl -0xa8(%esp), %eax', '"movl -0xa4(%esp), %eax'),
    ("weather first source", "(const void *)(uintptr_t)(M32(IMPORT_GLOBAL) + field->source)", "(const void *)(uintptr_t)(M32(IMPORT_GLOBAL) + field->source + 1)"),
    ("weather request tag", "multip_message_allocated(0, 0x1013)", "multip_message_allocated(0, 0x1012)"),
    ("sync message wire tag", "packet[0] = 0x1017;", "packet[0] = 0x1016;"),
    ("sync message reliability", "multip_send(A32(player, 4), packet, 20, 0)", "multip_send(A32(player, 4), packet, 20, 1)"),
    ("sync peer field", "packet[3] = A32(player, 0x40);", "packet[3] = A32(player, 0x3c);"),
]
WEATHER_MUTATIONS.extend([
    ("weather receive source", "(const uint8_t *)packet + field->dest, field->size", "(const uint8_t *)packet + field->dest + 1, field->size"),
    ("weather MAIN notification", "1, *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x23e)", "0, *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x23e)"),
    ("weather host reply guard", "if (M32(0xaa8) != 0 && multip_message_1014_impl(id, padding) != 0)", "if (M32(0xaa8) == 0 && multip_message_1014_impl(id, padding) != 0)"),
    ("weather request stack slot", '"movl -0xb0(%esp), %eax\\n\\tpushl %eax\\n\\tpushl 8(%esp)', '"movl -0xac(%esp), %eax\\n\\tpushl %eax\\n\\tpushl 8(%esp)'),
])

TICK_MUTATIONS = [
    ("tick sync zero incoming register", "if (M32(0x164) == 0) return incoming;", "if (M32(0x164) == 0) return 0;"),
    ("tick sync strict period", "if (result <= M32(0x164)) return result;", "if (result < M32(0x164)) return result;"),
    ("tick sync counter reset", "M32(0x944) = 0;", "M32(0x944) = 1;"),
    ("tick weather pending clear", "M32(0xa68) = 0;", "M32(0xa68) = 1;"),
    ("tick track countdown", "if (M32(0xf1c) != 0) M32(0xf1c)--;", "M32(0xf1c)--;"),
    ("tick accumulator clamp", "MD(0xa50) = 100.0;", "MD(0xa50) = 99.0;"),
    ("tick accumulator return", "return (incoming & 0xffff0000u) | status;", "return (incoming & 0xffff0000u);"),
    ("guidance disable condition", "if (A32(global, 0x84c) != 0) M32(0xab8) = 0;", "if (A32(global, 0x84c) == 0) M32(0xab8) = 0;"),
    ("guidance panel bit", "global[0x389] |= 0x40;", "global[0x389] |= 0x20;"),
    ("guidance ADF bearing subtraction", "(uint16_t)(bearing - own_heading)", "(uint16_t)(bearing + own_heading)"),
    ("guidance autopilot bearing width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x7cc)", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x7cc)"),
    ("guidance panel enable value", "A32(global, 0x384), 8) + 0x18) = 1;", "A32(global, 0x384), 8) + 0x18) = 2;"),
    ("tick registration flag", "M32(0x940) = 1;", "M32(0x940) = 0;"),
    ("tick removal flag", "M32(0x940) = 0;", "M32(0x940) = 1;"),
]


SYSTEM_MUTATIONS = [
    ("system created player filter", "    if (type == 1) {\n        if (multip_player_add_remote", "    if (type == 1 && id == 0) {\n        if (multip_player_add_remote"),
    ("system created peer ping", "if (multip_message_1007(id) != 0) return -1;", "if (multip_message_1005(id) != 0) return -1;"),
    ("system created type query", "if (multip_message_100f(id) != 0) return -1;", "if (multip_message_1006(id) != 0) return -1;"),
    ("system destroyed failure result", "        multip_player_remove(A32(message, 8));\n    return -1;", "        multip_player_remove(A32(message, 8));\n    return 0;"),
    ("system destroyed local guard", "if (A32(message, 4) == 1 && A32(message, 8) !=\n        A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4))", "if (A32(message, 4) == 1)"),
    ("system data unknown peer", "multip_find_player(A32(message, 8)) == NULL", "multip_find_player(A32(message, 8)) != NULL"),
    ("system host transition", "    M32(0xaa8) = 1;\n    if (multip_session_get_description", "    M32(0xaa8) = 0;\n    if (multip_session_get_description"),
    ("system host notification", "(2, 1, 0xf536, 0xf537, 0, 0, 0, 0)", "(2, 1, 0xf537, 0xf536, 0, 0, 0, 0)"),
]


APP_PLAYER_MUTATIONS = [
    ("type request capacity equality", "M32(count_offset) == M32(limit_offset)", "M32(count_offset) >= M32(limit_offset)"),
    ("type query existing member", "already_member = A32(player, 8) == type;", "already_member = 0;"),
    ("type request denied reply", "type == 1 ? multip_message_1005(id) : multip_message_1006(id)", "type == 1 ? multip_message_1006(id) : multip_message_1005(id)"),
    ("type grant target", "multip_find_player(A32(packet, 8));", "multip_find_player(A32(packet, 12));"),
    ("type grant sender fallback", "if (A32(player, 4) == id) {", "if (A32(player, 4) == A32(packet, 8)) {"),
    ("type grant ignored publication", "multip_player_publish_name(); /* This fallback ignores publication failure. */", "if (multip_player_publish_name() != 0) return -1;"),
    ("observer grant pending clear", "    M32(0xa64) = 0;\n    if (multip_message_1004", "    M32(0xa64) = 1;\n    if (multip_message_1004"),
    ("observer host player count", "if (multip_player_publish_name() != 0) return -1;\n    M32(0x9a0)--;", "if (multip_player_publish_name() != 0) return -1;\n    M32(0x9a0)++;"),
    ("observer rejected wire tag", "multip_message_allocated(id, 0x100d)", "multip_message_allocated(id, 0x100c)"),
    ("type query local ID", "multip_message_1003(id, A32(local, 4))", "multip_message_1003(id, id)"),
    ("collision message word width", "*(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x832) = 0x10;", "*(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x832) = 0x10;"),
    ("observer rejection notification", "(2, 1, 0xf535, 0, 0, 0, 0, 0)", "(2, 1, 0xf534, 0, 0, 0, 0, 0)"),
]


UPDATE_MUTATIONS = [
    ("full-state payload source", "(const uint8_t *)packet + 0x14, A32(packet, 0x10) - 0x6c", "(const uint8_t *)packet + 0x18, A32(packet, 0x10) - 0x6c"),
    ("full-state mismatched-size sequence", "}\n        A32(player, 0x50) = A32(packet, 0xc);", "}\n        if (A32(player, 0x2c) == A32(packet, 0x10)) A32(player, 0x50) = A32(packet, 0xc);"),
    ("motion initialization residual", "AD(player, 0xb4) = 0;", "AD(player, 0xb4) = 1;"),
    ("compact missing peer", "if (player == NULL) return 0;\n    if (HOST_FN(0x218, multip_memset_fn)(position", "if (player == NULL) return -1;\n    if (HOST_FN(0x218, multip_memset_fn)(position"),
    ("compact angle unpack", "A32(position, 24) = packed & 0xffc00000u;", "A32(position, 24) = packed & 0xff800000u;"),
    ("compact zero anchor copy", "if (A32(packet, 0xc) > A32(player, 0x50)) {\n        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x7c, player + 0x54, 0)", "if (A32(packet, 0xc) > A32(player, 0x50)) {\n        if (HOST_FN(0x21c, multip_copy_fn)(player + 0x7c, player + 0x54, 24)"),
    ("velocity stack altitude", "movzwl -0x14(%eax), %edx", "movzwl -0x18(%eax), %edx"),
    ("velocity scaling", '"m"(*(double *)(void *)(s_multip_rdata + 8))', '"m"(*(double *)(void *)(s_multip_rdata + 0))'),
    ("velocity feedback pending", "A32(player, 0x98) = 1;", "A32(player, 0x98) = 0;"),
    ("timing count signedness", "uint64_t old_count = A32(player, 0x48), new_count;", "int64_t old_count = (int32_t)A32(player, 0x48), new_count;"),
    ("timing half rounding", "int32_t half = (int32_t)delta / 2;", "int32_t half = (int32_t)delta >> 1;"),
    ("timing receive timestamp", "A32(player, 0x40) = timestamp;", "A32(player, 0x40) = 0;"),
    ("timing sender timestamp", "A32(player, 0x3c) = A32(packet, 0x10);", "A32(player, 0x3c) = A32(packet, 0xc);"),
]


TEXTURE_MUTATIONS = [
    ("texture trim spaces", "while (*end == ' ') *end-- = 0;", "while (*end == ' ') { *end-- = 0; break; }"),
    ("texture extension dot", "multip_strcpy_fn)(extension, dot);", "multip_strcpy_fn)(extension, dot + 1);"),
    ("texture directory resource", "multip_string_resource_fn)(0x9006, directory", "multip_string_resource_fn)(0x9007, directory"),
    ("texture name comparison width", "multip_compare_fn)(name, names + --count * 15, 14)", "multip_compare_fn)(name, names + --count * 15, 13)"),
    ("texture exists sense", "multip_file_exists_fn)(path, 0) == 0 ? 1 : 0", "multip_file_exists_fn)(path, 0) != 0 ? 1 : 0"),
    ("texture catalogue opcode", "*(const uint16_t *)(const void *)record == 0x18", "*(const uint16_t *)(const void *)record == 0x17"),
    ("texture catalogue zeroing", "multip_memset_fn)(entry, 0, 15)", "multip_memset_fn)(entry, 1, 15)"),
    ("texture list wire stride", "bytes = count * 15", "bytes = count * 14"),
    ("texture read zeroing", "(1, length, data, 1)", "(1, length, data, 0)"),
    ("texture short read", "if (length != read) return -1;", "if (0) return -1;"),
    ("texture short write", "if (size != written) return -1;", "if (0) return -1;"),
    ("texture write registry width", "multip_copy_fn)(entry, name, 15)", "multip_copy_fn)(entry, name, 14)"),
    ("texture cleanup extension", "multip_file_exists_fn)(path, 0) != 0", "multip_file_exists_fn)(path, 1) != 0"),
    ("texture failure pending flag", "if (multip_message_empty(id, 0x1016) != 0) return -1;\n    M32(0x984) = 1;", "if (multip_message_empty(id, 0x1016) != 0) return -1;\n    M32(0x984) = 0;"),
]


TEXTURE_RECEIVE_MUTATIONS = [
    ("receive owner duplicate", "owner == NULL) {\n        if (HOST_FN(0x20c, multip_alloc_fn)(1, 12", "1) {\n        if (HOST_FN(0x20c, multip_alloc_fn)(1, 12"),
    ("receive buffer zeroing", "(1, size, (void **)((uint8_t *)file + 0x18), 1)", "(1, size, (void **)((uint8_t *)file + 0x18), 0)"),
    ("receive texture name width", "multip_copy_fn)((uint8_t *)file + 4, name, 15)", "multip_copy_fn)((uint8_t *)file + 4, name, 14)"),
    ("receive segment count", "A32(file, 0x1c) = segments;", "A32(file, 0x1c) = segments + 1;"),
    ("receive outer head bug", "M32(0xdec) = A32(owner, 8);", "if (M32(0xdec) == ADDR(owner)) M32(0xdec) = A32(owner, 8); else { void *previous = (void *)(uintptr_t)M32(0xdec); while (previous && A32(previous, 8) != ADDR(owner)) previous = (void *)(uintptr_t)A32(previous, 8); if (previous) A32(previous, 8) = A32(owner, 8); }"),
    ("receive missing file return", "if (file == NULL) return -1;\n    if (HOST_FN", "if (file == NULL) return 0;\n    if (HOST_FN"),
    ("receive chunk offset", "A32(file, 0x18) + A32(packet, 0x10)", "A32(file, 0x18) + 0"),
    ("receive chunk decrement", "A32(file, 0x1c)--;", "A32(file, 0x1c)++;"),
    ("receive completed size", "(const void *)(uintptr_t)A32(file, 0x18), A32(file, 0x14)", "(const void *)(uintptr_t)A32(file, 0x18), 0"),
    ("receive unknown owner cancellation", "if (owner == NULL) return -1;\n    return multip_texture_receive_remove", "if (owner == NULL) return 0;\n    return multip_texture_receive_remove"),
]


TRANSFER_MESSAGE_MUTATIONS = [
    ("model header wire tag", "A32(packet, 0) = 0x101a;", "A32(packet, 0) = 0x101b;"),
    ("model header current total", "A32(packet, 8) = A32(aircraft, 8) + A32(aircraft, 0x18);", "A32(packet, 8) = total;"),
    ("model header record width", "M32(0xa3c) + 0x10, 44)", "M32(0xa3c) + 0x10, 43)"),
    ("model header title length", "A32(packet, 0x3c) = HOST_FN(0x2b8, multip_strlen_fn)(title) + 1;", "A32(packet, 0x3c) = HOST_FN(0x2b8, multip_strlen_fn)(title);"),
    ("model header segment count", "A32(packet, 0xc) = segments;", "A32(packet, 0xc) = segments + 1;"),
    ("model cancellation tag", "multip_message_allocated(id, 0x101c)", "multip_message_allocated(id, 0x101f)"),
    ("model chunk wire tag", "A32(packet, 0) = 0x101b;", "A32(packet, 0) = 0x101e;"),
    ("model chunk offset", "A32(packet, 0xc) = offset;", "A32(packet, 0xc) = 0;"),
    ("texture chunk ID", "A32(packet, 8) = texture;", "A32(packet, 8) = texture + 1;"),
    ("texture chunk offset", "A32(packet, 0x10) = offset;", "A32(packet, 0x10) = 0;"),
    ("texture header raw name", "multip_copy_fn)(packet + 8, name, 15)", "multip_copy_fn)(packet + 8, name, 14)"),
    ("texture header size", "A32(packet, 0x18) = size;", "A32(packet, 0x18) = size + 1;"),
]


TEXTURE_SEND_MUTATIONS = [
    ("sender name comparison width", "multip_compare_fn)(name, file, 15) == 0) break;\n        file =", "multip_compare_fn)(name, file, 14) == 0) break;\n        file ="),
    ("sender segment rounding", "if (size % M32(0x16c) != 0) A32(file, 0x14)++;", "if (0) A32(file, 0x14)++;"),
    ("sender texture ID increment", "A32(file, 0x10) = M32(0xe40); M32(0xe40)++;", "A32(file, 0x10) = M32(0xe40); M32(0xe40)--;"),
    ("sender duplicate recipient", "if (recipient != NULL) return -1;", "if (recipient != NULL) return 0;"),
    ("sender missing recipient", "if (recipient == NULL) return -1;", "if (recipient == NULL) return 0;"),
    ("sender timer rate", "s_multip_rdata + 0x198", "s_multip_rdata + 0x188"),
    ("sender CHAIN start code", "(2, multip_texture_send_callback) != 0) return -1;\n        M32(0xe38) = 1;", "(3, multip_texture_send_callback) != 0) return -1;\n        M32(0xe38) = 1;"),
    ("sender CHAIN stop flag", "M32(0xe38) = 0;\n    }\n    return 0;", "M32(0xe38) = 1;\n    }\n    return 0;"),
    ("sender chunk cap", "if (size > M32(0x16c)) size = M32(0x16c);", "if (size > M32(0x16c)) size = 0;"),
    ("sender threshold comparison", "if (status & 0x100) return result;\n    MD(0xe50)", "if (status & 0x4100) return result;\n    MD(0xe50)"),
    ("sender request enable", "if (M32(0xac4) != 0) {", "if (M32(0xac4) == 0) {"),
    ("sender request reverse index", "packet + 12 + A32(packet, 8) * 15", "packet + 12"),
]


MODEL_SEND_MUTATIONS = [
    ("model send total size", "M32(0xe18) = A32(local, 0x24) + A32(local, 0x2c);", "M32(0xe18) = A32(local, 0x24);"),
    ("model send timer rate", "s_multip_rdata + 0x178", "s_multip_rdata + 0x188"),
    ("model send segment rounding", "if (M32(0xe18) % M32(0x16c) != 0) M32(0xe1c)++;", "if (0) M32(0xe1c)++;"),
    ("model send missing ID", "if (recipient == NULL) return 0;", "if (recipient == NULL) return -1;"),
    ("model send initial data zeroing", "M32(0xe18), (void **)(uintptr_t)(D + 0xe14), 1)", "M32(0xe18), (void **)(uintptr_t)(D + 0xe14), 0)"),
    ("model send chunk cap", "if (size >= M32(0x16c)) size = M32(0x16c);", "if (size >= M32(0x16c)) size = 0;"),
    ("model send elapsed reset", "MD(0xe30) = -1.0;", "MD(0xe30) = 0.0;"),
    ("model send remove stop", "if (M32(0xe10) == 0 && multip_model_send_stop() != 0)", "if (0 && multip_model_send_stop() != 0)"),
    ("model send request enable", "if (M32(0xacc) != 0) {", "if (M32(0xacc) == 0) {"),
    ("model send callback registration", "(2, multip_model_send_callback) != 0) return -1;\n        M32(0xe20) = 1;", "(3, multip_model_send_callback) != 0) return -1;\n        M32(0xe20) = 1;"),
]


MODEL_CACHE_MUTATIONS = [
    ("cache scalar order", "{0x20, 0x28, 0x14, 0x1c}", "{0x28, 0x20, 0x14, 0x1c}"),
    ("cache model folder ID", "(0x9006, folder, 260);\n    if (HOST_FN", "(0x9007, folder, 260);\n    if (HOST_FN"),
    ("cache mkdir skipped", "multip_directory_create_fn)(path) != 0) return -1;", "multip_directory_create_fn)(path) != 0) return 0;"),
    ("cache write scalar short count", "if (written != 4) return -1;", "if (written != 4) return 1;"),
    ("cache read scalar short count", "if (read != 4) return -1;", "if (read != 4) return 1;"),
    ("cache header short read", "if (read != A32(aircraft, 0x14)) return -1;", "if (read != A32(aircraft, 0x14)) return 1;"),
    ("cache model short write", "if (written != A32(aircraft, 0x1c)) return -1;", "if (written != A32(aircraft, 0x1c)) return 1;"),
    ("cache title allocation zeroing", "(void **)(uintptr_t)(A32(aircraft, 0x18) + 0x64), 0)", "(void **)(uintptr_t)(A32(aircraft, 0x18) + 0x64), 1)"),
]


MODEL_RECEIVE_MUTATIONS = [
    ("model receive duplicate", "if (transfer != NULL) return -1;", "if (transfer != NULL) return 0;"),
    ("model receive metadata width", "(const uint8_t *)packet + 0x10, 44)", "(const uint8_t *)packet + 0x10, 43)"),
    ("model receive title source", "(const char *)packet + 0x40)", "(const char *)packet + 0x41)"),
    ("model receive buffer zeroing", "(void **)((uint8_t *)transfer + 0x34), 1)", "(void **)((uint8_t *)transfer + 0x34), 0)"),
    ("model receive split offset", "A32(transfer, 0x34) + A32(transfer, 0x1c)", "A32(transfer, 0x34) + A32(transfer, 0x1c) + 1"),
    ("model receive missing transfer", "if (transfer == NULL) return -1;", "if (transfer == NULL) return 0;"),
    ("model receive chunk offset", "A32(transfer, 0x34) + A32(packet, 0xc)", "A32(transfer, 0x34) + 0"),
    ("model receive segment decrement", "A32(transfer, 4)--;", "A32(transfer, 4)++;"),
    ("model receive absent peer cleanup", "            multip_model_receive_remove(id);", "            /* skip cleanup */"),
    ("model receive saved position width", "(const uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4, 36)", "(const uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4, 35)"),
    ("model receive restored position width", "(uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4,\n        saved, 36)", "(uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4,\n        saved, 35)"),
    ("model receive temporary path cleanup", "if (HOST_FN(0x214, multip_free_fn)((void **)(uintptr_t)(A32(temporary, 0x18) + 0x64)) != 0)", "if (0)"),
]


TITLE_RECEIVE_MUTATIONS = [
    ("title cache raw return", "return UTIL_FN(0xa0, multip_aircraft_cache_exists_fn)(path);", "return UTIL_FN(0xa0, multip_aircraft_cache_exists_fn)(path) != 0;"),
    ("title visibility source", "A32(player, 0x38) = A32(packet, 8);", "A32(player, 0x38) = A32(packet, 4);"),
    ("title temporary zero width", "multip_memset_fn)(temporary, 0, 12)", "multip_memset_fn)(temporary, 0, 11)"),
    ("title local aircraft selection", "multip_aircraft_select_fn)(temporary, title) != 0", "multip_aircraft_select_fn)(temporary, title) == 0"),
    ("title saved position width", "(const uint8_t *)(uintptr_t)A32(player, 0x28) + 4, 36)", "(const uint8_t *)(uintptr_t)A32(player, 0x28) + 4, 35)"),
    ("title restored position width", "(uint8_t *)(uintptr_t)A32(player, 0x28) + 4,\n            saved, 36)", "(uint8_t *)(uintptr_t)A32(player, 0x28) + 4,\n            saved, 35)"),
    ("title model request setting", "else if (M32(0xad0) != 0)", "else if (M32(0xad0) == 0)"),
]


STATE_MESSAGE_MUTATIONS = [
    ("state blocked word width", "*(const uint16_t *)(const void *)(global + 0x5dc)", "*(const uint32_t *)(const void *)(global + 0x5dc)"),
    ("state full trailing bytes", "packet[index] = stack[index];", "packet[index] = stack[index] ^ 1;"),
    ("state full sent size", "multip_send(id, packet, size + 20, 0)", "multip_send(id, packet, size + 19, 0)"),
    ("state compact angle mask", "(A32(global, 0x578) & 0xffc00000u)", "(A32(global, 0x578) & 0xff800000u)"),
    ("state compact padding slot", "movzwl -0x3c2(%esp), %eax", "movzwl -0x3c4(%esp), %eax"),
    ("state velocity padding slot", "movzwl -2(%esp), %eax", "movzwl -4(%esp), %eax"),
    ("state velocity scaling", "s_multip_rdata + 0x170", "s_multip_rdata + 0x188"),
    ("state velocity count", "A32(packet, 8) = M32(0xe08); M32(0xe08)++;", "A32(packet, 8) = M32(0xe08); M32(0xe08)--;"),
    ("state velocity extra field", "A32(packet, 0x1c) = A32(global, 0x2b4);", "A32(packet, 0x1c) = A32(global, 0x2b0);"),
    ("state velocity blocked guard", "if (multip_motion_is_blocked()) {", "if (!multip_motion_is_blocked()) {"),
]


CONTROL_UPDATE_MUTATIONS = [
    ("control initialization flag", "M32(0x948) = state; M32(0x94c) = A32(global, 0x268); M32(0xfc) = 0;", "M32(0x948) = state; M32(0x94c) = A32(global, 0x268); M32(0xfc) = 1;"),
    ("control cached word compare", "if (M32(0x948) != state)", "if (M32(0x948) == state)"),
    ("control local timer clamp", "A32(local, 0xcc) = (int32_t)timer < 0 ? 0 : timer;", "A32(local, 0xcc) = timer;"),
    ("control remote timer clamp", "A32(player, 0xcc) = (int32_t)timer < 0 ? 0 : timer;", "A32(player, 0xcc) = timer;"),
    ("control collision peer filter", "if (A32(player, 0xc) != 0 || A32(player, 8) != 1)", "if (A32(player, 0xc) == 0 || A32(player, 8) != 1)"),
    ("control collision distance boundary", "if (separation > 0) continue;", "if (separation >= 0) continue;"),
    ("control collision notification ID", "multip_message_1011(A32(player, 4))", "multip_message_1011(0)"),
    ("publish timer threshold", "if (status & 0x100) return 0;", "if (status & 0x4100) return 0;"),
    ("publish checksum saved byte", "M8(0xf28) = sum;", "M8(0xf28) = sum ^ 1;"),
    ("publish checksum start", "A32(global, 0x11e4) + 0x28;", "A32(global, 0x11e4) + 0x29;"),
    ("publish compact padding", "stack + 0x26)", "stack + 0x24)"),
    ("publish velocity padding", "stack + 0x3e6)", "stack + 0x3e4)"),
]


CONTROL_CALLBACK_MUTATIONS = [
    ('control callback timestamp', 'movl %eax, _s_multip_image+0xad8', 'movl $0, _s_multip_image+0xad8'),
    ('control callback signed timestep', 'movswl 0x2aa(%eax), %ecx', 'movzwl 0x2aa(%eax), %ecx'),
    ('control callback rate factor', 'fmull _s_multip_rdata+0x18', 'fmull _s_multip_rdata+0x170'),
    ('control callback rate difference', 'fsubl _s_multip_image+0xa78', 'faddl _s_multip_image+0xa78'),
    ('control callback velocity destination', 'pushl $_s_multip_image+0xa98', 'pushl $_s_multip_image+0xa90'),
    ('control callback pending guard', 'cmpl $0, _s_multip_image+0x984', 'cmpl $1, _s_multip_image+0x984'),
    ('control callback retry period', 'cmpl _s_multip_image+0x194, %eax', 'cmpl $0, %eax'),
    ('control callback player retry', '2: call _multip_message_1008', '2: call _multip_message_1001'),
    ('control callback observer retry', 'call _multip_message_1009', 'call _multip_message_1002'),
    ('control callback rate failure', 'testl %eax, %eax\\n\\tjnz 8f\\n\\tmovl _s_multip_image+0xc', 'testl %eax, %eax\\n\\tjz 8f\\n\\tmovl _s_multip_image+0xc'),
    ('control callback velocity failure', 'call *0x30(%edx)\\n\\ttestl %eax, %eax\\n\\tjnz 8f', 'call *0x30(%edx)\\n\\ttestl %eax, %eax\\n\\tjz 8f'),
    ('control callback wrapped elapsed', 'subl _s_multip_image+0xadc, %eax', 'addl _s_multip_image+0xadc, %eax'),
    ('control callback blocked rate', 'movl %eax, _s_multip_image+0xa7c', 'movl $0, _s_multip_image+0xa7c'),
]


LIFECYCLE_MUTATIONS = [
    ("completion callee-left startup word", "call _multip_player_start\\n\\ttestl %eax, %eax", "movl $0x6739c15a, -0x408(%esp)\\n\\tcall _multip_player_start\\n\\ttestl %eax, %eax"),
    ("completion startup stack", "call _multip_player_start\\n\\ttestl %eax, %eax", "subl $4, %esp\\n\\tcall _multip_player_start\\n\\taddl $4, %esp\\n\\ttestl %eax, %eax"),
    ("system lost disconnect", "case 0x31: return multip_system_lost(packet, size);", "case 0x31: return 0;"),

    ("disconnect notification", "(2, 1, 0xf538, 0, 0, 0, 0, 0);", "(2, 1, 0xf533, 0, 0, 0, 0, 0);"),

    ("receive registration order", "CHAIN_FN(0x18, multip_chain_register_fn)(1, multip_receive_callback)", "CHAIN_FN(0x18, multip_chain_register_fn)(9, multip_receive_callback)"),
    ("receive removal flag", "        M32(0x950) = 0;", "        M32(0x950) = 1;"),
    ("disconnect already closed", "if (M32(M_CONNECTED) == 0) return 0;\n    CHAIN_FN", "if (M32(M_CONNECTED) == 0) return -1;\n    CHAIN_FN"),
    ("disconnect departure", "multip_message_1012() != 0) return -1;", "multip_message_1013() != 0) return -1;"),
    ("disconnect elapsed wait", "multip_ticks_fn)() - start + 3000", "multip_ticks_fn)() - start - 3000"),
    ("disconnect close result", "    return result;\n}\n\n/* .text+0x9c70", "    return result != 0 ? -1 : 0;\n}\n\n/* .text+0x9c70"),
    ("disconnect player destroy", "if (M32(0xa40) != 0)\n        DP_FN", "if (M32(0xa40) == 0)\n        DP_FN"),
    ("disconnect cleanup counter", "M32(0x9a0) = 0; M32(0x9a4) = 0;", "M32(0x9a0) = 0; M32(0x9a4) = 1;"),
    ("completion local flag", "movl $1, _s_multip_image+0xa40", "movl $0, _s_multip_image+0xa40"),
    ("completion ping target", "jnz 1f\\n\\tpushl $0\\n\\tcall _multip_message_1007@4", "jnz 1f\\n\\tpushl $1\\n\\tcall _multip_message_1007@4"),
    ("join rejection connected flag", "M32(0x984) = 0; M32(M_CONNECTED) = 0;", "M32(0x984) = 0; M32(M_CONNECTED) = 1;"),
    ("unknown application return", "xorl %eax, %eax\\n\\taddl $12, %esp\\n\\tret $16", "movl $-1, %eax\\n\\taddl $12, %esp\\n\\tret $16"),
    ("receive expiry boundary", "A32(player, 0xfc) > M32(0x17c)", "A32(player, 0xfc) >= M32(0x17c)"),
    ("receive expiry type", "if (A32(player, 8) == 4 && HOST_FN", "if (A32(player, 8) == 1 && HOST_FN"),
    ("receive raw failure", "3: popl %esi\\n\\taddl $8, %esp", "3: negl %eax\\n\\tsbbl %eax, %eax\\n\\tpopl %esi\\n\\taddl $8, %esp"),
]

DISPATCH_MUTATIONS = [
    ("system created route", "case 3: return multip_system_created(packet, size);", "case 3: return multip_system_destroyed(packet, size);"),
    ("system destroyed raw failure", "case 5: return multip_system_destroyed(packet, size);", "case 5: return 0;"),
    ("system player data timestamp", "case 0x102: return multip_system_player_data(packet, size, timestamp);", "case 0x102: return 0;"),
    ("application position dispatch frame", "movl 8(%esp), %eax\\n\\tsubl $12, %esp", "movl 8(%esp), %eax\\n\\tsubl $12, %esp\\n\\tmovw $0, -0x34(%esp)"),
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
        ["wine", "multipdiff.exe", "1200"] + ([slice_name] if slice_name in ("local", "frame", "players", "messages", "draw", "motion", "commands", "weather", "tick", "system", "app_players", "updates", "textures", "texture_receive", "transfer_messages", "texture_send", "model_send", "model_cache", "model_receive", "title_receive", "state_messages", "control_updates", "control_callback", "dispatch", "lifecycle") else []), cwd=directory,
        env={**os.environ, "WINEDEBUG": "-all"}, capture_output=True,
        text=True, timeout=180 if slice_name == "send" else 60,
    )
    label = {"send": "send-one/drain/flush", "receive": "receive-one/dequeue",
             "workers": "worker lifecycles",
             "directplay": "DirectPlay loader/lifetime",
             "profile": "profile load/save", "groups": "session/group primitives",
             "metadata": "session descriptors/name metadata",
             "aircraft": "aircraft select/release",
             "records": "aircraft copy/session browser records",
             "tracking": "track-name display", "messages": "application message constructors",
             "players": "player lifecycle helpers", "local": "local player creation/start", "frame": "session CHAIN 0x14", "draw": "player drawing/CHAIN 0x10", "motion": "aircraft extrapolation/CHAIN 0x13", "commands": "view commands/CHAIN 0x0c", "weather": "weather/sync messages", "tick": "tick callbacks/CHAIN 0x02", "system": "DirectPlay system notifications", "app_players": "application player types", "updates": "aircraft state receive/timing", "textures": "texture catalogue/file services", "texture_receive": "multipart texture receive", "transfer_messages": "multipart transfer messages", "texture_send": "multipart texture sender", "model_send": "multipart model sender", "model_cache": "aircraft cache files", "model_receive": "multipart model receive", "title_receive": "aircraft title receive", "state_messages": "aircraft state message constructors", "control_updates": "control/collision/state publishing", "control_callback": "control CHAIN 0x01", "dispatch": "application receive dispatch", "lifecycle": "receive/session lifecycle"}[slice_name]
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
    parser.add_argument("--slice", choices=("send", "receive", "workers", "directplay", "profile", "groups", "metadata", "aircraft", "records", "tracking", "messages", "players", "local", "frame", "draw", "motion", "commands", "weather", "tick", "system", "app_players", "updates", "textures", "texture_receive", "transfer_messages", "texture_send", "model_send", "model_cache", "model_receive", "title_receive", "state_messages", "control_updates", "control_callback", "dispatch", "lifecycle", "all"), default="all")
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
                                  ("players", PLAYER_MUTATIONS),
                                  ("local", LOCAL_MUTATIONS),
                                  ("frame", FRAME_MUTATIONS),
                                  ("draw", DRAW_MUTATIONS),
                                  ("motion", MOTION_MUTATIONS),
                                  ("commands", COMMAND_MUTATIONS),
                                  ("weather", WEATHER_MUTATIONS),
                                  ("tick", TICK_MUTATIONS),
                                  ("system", SYSTEM_MUTATIONS),
                                  ("app_players", APP_PLAYER_MUTATIONS),
                                  ("updates", UPDATE_MUTATIONS),
                                  ("textures", TEXTURE_MUTATIONS),
                                  ("texture_receive", TEXTURE_RECEIVE_MUTATIONS),
                                  ("transfer_messages", TRANSFER_MESSAGE_MUTATIONS),
                                  ("texture_send", TEXTURE_SEND_MUTATIONS),
                                  ("model_send", MODEL_SEND_MUTATIONS),
                                  ("model_cache", MODEL_CACHE_MUTATIONS),
                                  ("model_receive", MODEL_RECEIVE_MUTATIONS),
                                  ("title_receive", TITLE_RECEIVE_MUTATIONS),
                                  ("state_messages", STATE_MESSAGE_MUTATIONS),
                                  ("control_updates", CONTROL_UPDATE_MUTATIONS),
                                  ("control_callback", CONTROL_CALLBACK_MUTATIONS),
                                  ("dispatch", DISPATCH_MUTATIONS),
                                  ("lifecycle", LIFECYCLE_MUTATIONS)):
        if args.slice in (slice_name, "all"):
            variants.extend((slice_name, *mutation) for mutation in mutations)
    source = (ROOT / "src/multip/multip.c").read_text()
    for include in ("multip_profile.inc", "multip_session.inc",
                    "multip_aircraft.inc", "multip_records.inc", "multip_tracking.inc", "multip_messages.inc", "multip_players.inc", "multip_callbacks.inc", "multip_motion.inc", "multip_commands.inc", "multip_weather.inc", "multip_tick.inc", "multip_system.inc", "multip_app_players.inc", "multip_updates.inc", "multip_textures.inc", "multip_texture_receive.inc", "multip_transfer_messages.inc", "multip_texture_send.inc", "multip_model_send.inc", "multip_model_cache.inc", "multip_model_receive.inc", "multip_title_receive.inc", "multip_state_messages.inc", "multip_control_updates.inc", "multip_lifecycle.inc", "multip_dispatch.inc", "multip_dispatch_cases.inc"):
        source = source.replace('#include "' + include + '"',
                                (ROOT / "src/multip" / include).read_text())
    for slice_name, name, before, after in variants:
        if source.count(before) != 1:
            raise RuntimeError(f"Mutation {name!r} matches {source.count(before)} times; expected one")
    with tempfile.TemporaryDirectory(prefix="multip-breaks-", dir="/tmp") as temp:
        directory = Path(temp)
        for name in ("multipdiff.exe", "MULTIP_retail.dll", "UTIL_retail.dll", "G3D_retail.dll"):
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
