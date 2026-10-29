#!/usr/bin/env python3
"""Exercise the installed World.DLL through the retail game's real owners."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "bin/world-diff/game-integration"
PREFIX = ROOT / "bin/world-diff/wineprefix"
HELPER = ROOT / "bin/world-diff/worldgameui.exe"
GAME_SOURCE = Path(os.environ.get("FS98_DIR", ROOT / "game")).resolve()
GAME = OUT / "game-copy"
CFG = GAME / "FLTSIM98.CFG"
WORLD_FILE = GAME / "Scenery/world.VIS"
WORLD_DLL = GAME / "English/World.DLL"
BASE_DLL = ROOT / "bin/WORLD.DLL"
RETAIL = ROOT / "orig"
ENV = dict(os.environ, WINEPREFIX=str(PREFIX), WINEARCH="win32", WINEDEBUG="-all")
TIMEOUT = 20
REPORT: dict[str, object] = {"game": str(GAME), "phases": [], "steps": [], "world_module_paths_seen": []}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def action(kind: str, *args: object) -> str:
    result = subprocess.run(
        ["wine", str(HELPER), kind, *(str(arg) for arg in args)],
        cwd=ROOT,
        env=ENV,
        capture_output=True,
        text=True,
        timeout=TIMEOUT,
    )
    if result.returncode:
        raise RuntimeError(f"UI helper {kind} {args} returned {result.returncode}: {result.stderr.strip()}")
    return result.stdout


def snapshot() -> dict:
    return json.loads(action("snapshot"))


def wait_for(predicate, description: str, seconds: int = 45):
    deadline = time.monotonic() + seconds
    latest = None
    last_error = None
    while time.monotonic() < deadline:
        try:
            latest = snapshot()
            value = predicate(latest)
            if value:
                return value
            last_error = None
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            last_error = str(error)
        time.sleep(0.25)
    if latest:
        details = {
            "pid": latest.get("pid"),
            "gauges": sum(module["name"].lower().endswith(".gau") for module in latest.get("modules", [])),
            "windows": [
                {
                    "class": window["window"]["class"],
                    "title": window["window"]["text"],
                    "enabled": window["window"]["enabled"],
                    "controls": [
                        {key: control.get(key) for key in ("id", "class", "text", "count", "selection", "enabled")}
                        for control in window["controls"]
                        if control["visible"]
                    ],
                }
                for window in latest.get("windows", [])
            ],
        }
    else:
        details = last_error
    raise RuntimeError(f"timed out waiting for {description}: {details}")


def main_window(state: dict | None = None) -> dict:
    state = state or snapshot()
    return next(item for item in state["windows"] if item["window"]["class"] == "FS98MAIN")


def modal_windows(state: dict | None = None) -> list[dict]:
    state = state or snapshot()
    return [item for item in state["windows"] if item["window"]["class"] != "FS98MAIN"]


def control(window: dict, control_id: int) -> dict:
    match = next((item for item in window["controls"] if item["id"] == control_id and item["visible"]), None)
    if match is None:
        raise AssertionError(f"visible control {control_id:#x} missing from {window['window']}")
    return match


def command(window: dict, control_id: int) -> None:
    action("command", window["window"]["hwnd"], control_id)


def click(item: dict) -> None:
    action("click", item["hwnd"])


def set_text(item: dict, value: str) -> None:
    action("text", item["hwnd"], value)


def select(item: dict, index: int) -> None:
    action("select", item["hwnd"], index)


def save_step(name: str) -> dict:
    state = snapshot()
    for module in state["modules"]:
        if module["name"].lower() == "world.dll" and module["path"] not in REPORT["world_module_paths_seen"]:
            REPORT["world_module_paths_seen"].append(module["path"])
    (OUT / f"{name}.json").write_text(json.dumps(state, indent=2, ensure_ascii=False) + "\n")
    REPORT["steps"].append(name)
    return state


def open_dialog(menu_id: int) -> dict:
    command(main_window(), menu_id)
    return wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"]), None), f"menu {menu_id:#x} dialog")


def wait_closed(window: dict) -> None:
    hwnd = window["window"]["hwnd"]
    wait_for(lambda state: not any(item["window"]["hwnd"] == hwnd for item in modal_windows(state)), f"window {hwnd} to close")


def accept(window: dict) -> None:
    command(window, 1)
    wait_closed(window)


def cancel(window: dict) -> None:
    command(window, 2)
    wait_closed(window)


def assert_world_loaded() -> None:
    paths = [path.replace("/", "\\").lower() for path in REPORT["world_module_paths_seen"]]
    if "c:\\fs98\\english\\world.dll" not in paths:
        raise AssertionError(f"game did not load World.DLL from its installed English directory: {paths}")


def basic_owner_flows() -> None:
    # Basic Weather: the MAIN owner applies accepted values to live GLOBAL.
    window = open_dialog(0x1306)
    original = [control(window, item)["selection"] for item in (0x1abe, 0x1ac0, 0x1ac2)]
    changed = [(original[0] + 1) % 5, (original[1] + 1) % 5, (original[2] + 1) % 4]
    for item, value in zip((0x1abe, 0x1ac0, 0x1ac2), changed):
        select(control(window, item), value)
    accept(window)
    window = open_dialog(0x1306)
    observed = [control(window, item)["selection"] for item in (0x1abe, 0x1ac0, 0x1ac2)]
    if observed != changed:
        raise AssertionError(f"MAIN did not retain Basic Weather {changed}: {observed}")
    save_step("owner-basic-weather-reopened")
    for item, value in zip((0x1abe, 0x1ac0, 0x1ac2), original):
        select(control(window, item), value)
    accept(window)
    REPORT["steps"].append("owner-basic-weather-apply-reopen-restore")

    # Time & Season: choose exact time so the edit is enabled, then apply and restore.
    window = open_dialog(0x1301)
    old_minute = control(window, 0x1af9)["text"]
    new_minute = str((int(old_minute) + 7) % 60)
    click(control(window, 0x1b1c))
    click(control(window, 0x1af9))
    set_text(control(window, 0x1af9), new_minute)
    accept(window)
    window = open_dialog(0x1301)
    if control(window, 0x1af9)["text"].strip() != new_minute:
        raise AssertionError("MAIN did not retain exact GMT minute")
    save_step("owner-time-reopened")
    click(control(window, 0x1b1c))
    click(control(window, 0x1af9))
    set_text(control(window, 0x1af9), old_minute)
    accept(window)
    REPORT["steps"].append("owner-time-apply-reopen-restore")

    # Exact aircraft location and heading are applied through the real MAIN owner.
    window = open_dialog(0x1303)
    heading = control(window, 0x119f)["text"].strip()
    changed_heading = "011" if heading != "011" else "012"
    click(control(window, 0x119f))
    set_text(control(window, 0x119f), changed_heading)
    accept(window)
    window = open_dialog(0x1303)
    if control(window, 0x119f)["text"].strip() != changed_heading:
        raise AssertionError("MAIN did not retain exact-position heading")
    save_step("owner-position-reopened")
    click(control(window, 0x119f))
    set_text(control(window, 0x119f), heading)
    accept(window)
    REPORT["steps"].append("owner-position-apply-reopen-restore")

    # The Airport search and its nested directory are driven by MAIN and FACIL.
    window = open_dialog(0x1304)
    search_mode = control(window, 0x1bde)
    click(search_mode)
    window = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["hwnd"] == window["window"]["hwnd"] and any(c["id"] == 0x1bee and c["visible"] for c in item["controls"])), None), "airport ID search controls")
    set_text(control(window, 0x1bee), "KORD")
    state = wait_for(lambda item: next((item for item in modal_windows(item) if any(c["id"] == 0x1bef and c.get("count", 0) > 0 for c in item["controls"])), None), "airport search results")
    window = state
    if control(window, 0x1bef)["count"] != 16:
        raise AssertionError("KORD results list did not populate from the real FACIL database")
    save_step("owner-airport-kord-search")
    command(window, 0x1bd9)
    nested = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and item["window"]["hwnd"] != window["window"]["hwnd"]), None), "nested Airport/Facility Directory")
    save_step("owner-airport-facility-directory")
    cancel(nested)
    cancel(window)
    REPORT["steps"].append("owner-airport-search-and-nested-directory")

    # MAIN-backed live settings: change, reopen, and restore each value.
    for menu_id, control_id, name, count in ((0x1308, 0x1ad4, "complexity", 5), (0x1309, 0x1ad9, "dynamic", 5)):
        window = open_dialog(menu_id)
        original_value = control(window, control_id)["selection"]
        changed_value = (original_value + 1) % count
        select(control(window, control_id), changed_value)
        accept(window)
        window = open_dialog(menu_id)
        if control(window, control_id)["selection"] != changed_value:
            raise AssertionError(f"MAIN did not retain {name} selection")
        save_step(f"owner-{name}-reopened")
        select(control(window, control_id), original_value)
        accept(window)
    REPORT["steps"].append("owner-complexity-dynamic-apply-reopen-restore")

    # Weather area creation, copy callback, confirmation, and removal through MAIN.
    window = open_dialog(0x1307)
    original_count = control(window, 0x1b14)["count"]
    command(window, 0x1a93)
    state = wait_for(lambda item: next((item for item in modal_windows(item) if any(c["id"] == 0x1b14 and c.get("count", 0) == original_count + 1 for c in item["controls"])), None), "new weather area")
    window = state
    set_text(control(window, 0x1b0b), "OpenFS98 integration")
    command(window, 0x1a95)
    copy_dialog = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and item["window"]["hwnd"] != window["window"]["hwnd"]), None), "Copy Weather child dialog")
    save_step("owner-weather-copy-dialog")
    accept(copy_dialog)
    # Child close can make the parent's accepted notification asynchronous.
    window = wait_for(lambda state: next((item for item in modal_windows(state) if any(c["id"] == 0x1b14 for c in item["controls"])), None), "weather area parent")
    accept(window)
    window = open_dialog(0x1307)
    if control(window, 0x1b14)["count"] != original_count + 1:
        raise AssertionError("MAIN did not retain the added weather area")
    select(control(window, 0x1b14), original_count)
    state = wait_for(lambda item: next((item for item in modal_windows(item) if any(c["id"] == 0x1b0b and c["text"] == "OpenFS98 integration" for c in item["controls"])), None), "selected named weather area")
    window = state
    save_step("owner-weather-area-reopened")
    command(window, 0x1aa2)
    alert = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x7d4 for c in item["controls"])), None), "weather-area deletion confirmation")
    if "remove this weather area" not in " ".join(c["text"] for c in alert["controls"]).lower():
        raise AssertionError("unexpected weather-area confirmation text")
    command(alert, 0x7d4)
    wait_for(lambda state: next((item for item in modal_windows(state) if any(c["id"] == 0x1b14 and c.get("count") == original_count for c in item["controls"])), None), "weather-area deletion")
    window = next(item for item in modal_windows() if any(c["id"] == 0x1b14 for c in item["controls"]))
    accept(window)
    REPORT["steps"].append("owner-weather-add-copy-reopen-confirm-delete")


def scenery_file_owner_flow() -> None:
    original_hash = sha256(WORLD_FILE)
    open_dialog(0x130a)
    save_step("owner-scenery-library-warning")
    warning = next(item for item in modal_windows() if item["window"]["enabled"])
    command(warning, 0x7d4)
    world_files = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aab for c in item["controls"])), None), "Scenery Library world-files container")
    save_step("owner-scenery-world-files")
    command(world_files, 0x1aab)
    area_list = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aa6 for c in item["controls"])), None), "scenery-area list")
    save_step("owner-scenery-area-list")
    command(area_list, 0x1aa6)
    editor = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1b07 for c in item["controls"])), None), "scenery-area editor")
    title = control(editor, 0x1b07)["text"]
    new_title = "OpenFS98 integration"
    if title == new_title:
        new_title = "OpenFS98 integration 2"
    path = control(editor, 0x1b05)["text"]
    set_text(control(editor, 0x1b07), new_title)
    save_step("owner-scenery-area-editor-updated")
    updated_editor = next(item for item in modal_windows() if item["window"]["hwnd"] == editor["window"]["hwnd"])
    if control(updated_editor, 0x1b07)["text"].strip() != new_title:
        raise AssertionError("WM_SETTEXT did not update the scenery title control")
    click(control(editor, 1))
    wait_closed(editor)
    # FE writes the selected world after the containing area list is accepted.
    area_list = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aa6 for c in item["controls"])), None), "scenery-area list after edit")
    click(control(area_list, 1))
    wait_closed(area_list)
    world_files = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aab for c in item["controls"])), None), "Scenery Library container after edit")
    title_in_file = new_title.encode("ascii") in WORLD_FILE.read_bytes()
    if not path or not WORLD_FILE.exists():
        raise AssertionError("the active scenery record/path was not available")
    REPORT["world_file_write"] = {"before_sha256": original_hash, "during_sha256": sha256(WORLD_FILE), "title": new_title, "title_present": title_in_file, "path": path}
    # Reopen through FE's real list/editor callbacks and verify serialized data.
    command(world_files, 0x1aab)
    area_list = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aa6 for c in item["controls"])), None), "scenery-area list after file write")
    command(area_list, 0x1aa6)
    editor = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1b07 for c in item["controls"])), None), "reopened scenery-area editor")
    if control(editor, 0x1b07)["text"].strip() != new_title:
        raise AssertionError("FE did not reload the accepted scenery title")
    save_step("owner-scenery-title-reopened")
    if not title_in_file:
        raise AssertionError("FE retained the edited title in the owner state but did not serialize it to world.vis")
    # Restore the exact title through the UI; final cleanup restores original bytes.
    set_text(control(editor, 0x1b07), title)
    click(control(editor, 1))
    wait_closed(editor)
    area_list = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aa6 for c in item["controls"])), None), "scenery-area list after UI restore")
    click(control(area_list, 1))
    wait_closed(area_list)
    wait_for(lambda _state: WORLD_FILE.exists(), "restored scenery file")
    world_files = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == 0x1aac for c in item["controls"])), None), "Scenery Library container after UI restore")
    REPORT["steps"].append("owner-fe-world-vis-edit-write-reopen-restore")

    # Also open, then cancel, each FE-owned nested list without changing data.
    for button_id, child_id, name in ((0x1aac, 0x1b10, "scenery-cache-list"),):
        command(world_files, button_id)
        child = wait_for(lambda state: next((item for item in modal_windows(state) if item["window"]["enabled"] and any(c["id"] == child_id for c in item["controls"])), None), name)
        save_step(f"owner-{name}")
        cancel(child)
        cancel(world_files)
    REPORT["steps"].append("owner-scenery-cache-list-open-cancel")


def advanced_owner_flow() -> None:
    # Advanced Weather embeds five native page objects in a single MAIN-owned dialog.
    window = next(item for item in modal_windows() if item["window"]["enabled"])
    expected = (0x1a96, 0x1a9c, 0x1a99, 0x1a9b, 0x1a98)
    ids = {item["id"] for item in window["controls"]}
    missing = [item for item in expected if item not in ids]
    if missing:
        save_step("owner-advanced-weather-page-discovery")
        raise AssertionError(f"Advanced Weather owner is missing page controls {missing}; observed {sorted(ids)}")
    save_step("owner-advanced-weather-parent")
    # Each tab is an ordinary dialog button; exercise all five owned page graphs.
    for tab_id in expected:
        click(control(window, tab_id))
        time.sleep(0.15)
        state = save_step(f"owner-advanced-weather-page-{tab_id:04x}")
        window = next(item for item in modal_windows(state) if item["window"]["hwnd"] == window["window"]["hwnd"])
    accept(window)
    REPORT["steps"].append("owner-advanced-weather-parent-and-five-pages")


def run_phase(advanced: int, scope: str = "full") -> None:
    phase = {"advanced_weather": advanced}
    REPORT["phases"].append(phase)
    trace = OUT / f"game-trace-advanced-{advanced}.log"
    phase["trace"] = str(trace.relative_to(ROOT))
    trace_file = trace.open("w")
    process = subprocess.Popen(
        ["wine", "explorer", "/desktop=WorldIntegration,1024x768", r"C:\FS98\FLTSIM98.EXE"],
        cwd=GAME,
        env=dict(ENV, WINEDEBUG="+loaddll"),
        stdout=trace_file,
        stderr=subprocess.STDOUT,
    )
    try:
        state = wait_for(
            lambda item: item
            if any(window["window"]["class"] == "FS98MAIN" for window in item["windows"])
            and sum(module["name"].lower().endswith(".gau") for module in item["modules"]) >= 10
            else None,
            "retail game cockpit and gauges startup",
            120,
        )
        phase["pid"] = state["pid"]
        main_window(state)
        gauges = [module["name"] for module in state["modules"] if module["name"].lower().endswith(".gau")]
        if len(gauges) < 10:
            raise AssertionError(f"game reached the main window with only {len(gauges)} gauges")
        phase["initial_gauges"] = len(gauges)
        REPORT["steps"].append(f"game-start-advanced-{advanced}-cockpit-{len(gauges)}-gauges")
        if advanced:
            window = open_dialog(0x1306)
            if not any(c["id"] in (0x1a96, 0x1a9c, 0x1a99, 0x1a9b, 0x1a98) for c in window["controls"]):
                raise AssertionError("advanced-weather menu selected a non-Advanced dialog")
            advanced_owner_flow()
        elif scope == "scenery-only":
            scenery_file_owner_flow()
        else:
            basic_owner_flows()
            scenery_file_owner_flow()
        state = snapshot()
        phase["world_loaded_paths"] = list(REPORT["world_module_paths_seen"])
        assert_world_loaded()
        command(main_window(state), 0x110a)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline and process.poll() is None:
            time.sleep(0.25)
        if process.poll() is None:
            subprocess.run(["wineserver", "-k"], cwd=ROOT, env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
        phase["exit_code"] = process.poll()
    finally:
        if process.poll() is None:
            subprocess.run(["wineserver", "-k"], cwd=ROOT, env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
        trace_file.close()
    if phase["exit_code"] != 0:
        raise AssertionError(f"game phase exited with status {phase['exit_code']}")
    trace_lines = trace.read_text(encoding="utf-8", errors="replace").splitlines()
    errors = [line for line in trace_lines if re.search(r"err:|unhandled exception", line, re.IGNORECASE) and not re.search(r"winediag|RandR", line, re.IGNORECASE)]
    phase["runtime_errors"] = len(errors)
    if errors:
        raise AssertionError("unexpected Wine/game errors: " + "\n".join(errors[:8]))


def set_config(original: bytes, advanced: int) -> bytes:
    changed, count = re.subn(rb"(?im)^(ADVANCED_WEATHER=)[^\r\n]*", rb"\g<1>" + str(advanced).encode("ascii"), original)
    if count != 1:
        raise AssertionError(f"expected one ADVANCED_WEATHER setting, found {count}")
    changed, count = re.subn(rb"(?im)^(SPLASH_SCREEN_OFF=)[^\r\n]*", rb"\g<1>1", changed)
    if count != 1:
        raise AssertionError(f"expected one SPLASH_SCREEN_OFF setting, found {count}")
    changed, count = re.subn(rb"(?im)^(PROMPT_ON_EXIT=)[^\r\n]*", rb"\g<1>0", changed)
    if count != 1:
        raise AssertionError(f"expected one PROMPT_ON_EXIT setting, found {count}")
    return changed


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    source_world_dll = GAME_SOURCE / "English/World.DLL"
    if not HELPER.is_file() or not source_world_dll.is_file():
        raise RuntimeError("build the helper and install World.DLL first (make world-game-ui)")
    accepted_hash = sha256(BASE_DLL)
    installed_hash = sha256(source_world_dll)
    REPORT.update({"installed_game": str(GAME_SOURCE), "accepted_world_sha256": accepted_hash, "installed_world_sha256": installed_hash})
    if accepted_hash != installed_hash:
        raise RuntimeError(f"installed World.DLL {installed_hash} does not match accepted build {accepted_hash}")
    host_hash = sha256(GAME_SOURCE / "FLTSIM98.EXE")
    main_hash = sha256(GAME_SOURCE / "Modules/MAIN.DLL")
    if host_hash != sha256(RETAIL / "FLTSIM98.EXE") or main_hash != sha256(RETAIL / "Modules/MAIN.DLL"):
        raise RuntimeError("installed host or MAIN differs from the retail layout used by the read-only menu reader")
    if GAME.is_symlink():
        raise RuntimeError(f"refusing to replace unexpected game-copy symlink {GAME}")
    if GAME.exists():
        shutil.rmtree(GAME)
    shutil.copytree(GAME_SOURCE, GAME)
    copied_world_hash = sha256(WORLD_DLL)
    if copied_world_hash != accepted_hash:
        raise RuntimeError(f"copied game World.DLL {copied_world_hash} does not match accepted build {accepted_hash}")
    lowercase_world = GAME / "Scenery/world.vis"
    uppercase_world = GAME / "Scenery/world.VIS"
    if lowercase_world.exists() and uppercase_world.exists():
        raise RuntimeError("source game already contains both world.vis and world.VIS")
    if lowercase_world.exists():
        lowercase_world.rename(uppercase_world)
    if not uppercase_world.exists():
        raise RuntimeError(f"game copy has no scenery world file under {GAME / 'Scenery'}")
    REPORT["game_copy"] = str(GAME)
    REPORT["world_filename_in_copy"] = uppercase_world.name
    cfg_before = CFG.read_bytes()
    world_before = WORLD_FILE.read_bytes()
    cfg_backup = OUT / "FLTSIM98.CFG.before"
    world_backup = OUT / "world.vis.before"
    cfg_backup.write_bytes(cfg_before)
    world_backup.write_bytes(world_before)
    scenery = GAME / "Scenery"
    vis_before = {
        path.relative_to(scenery).as_posix(): path.read_bytes()
        for path in scenery.rglob("*")
        if path.is_file() and path.suffix.lower() == ".vis"
    }
    prefix_game = PREFIX / "drive_c/FS98"
    old_target = os.readlink(prefix_game) if prefix_game.is_symlink() else None
    old_is_dir = prefix_game.exists() and not prefix_game.is_symlink()
    if old_is_dir:
        raise RuntimeError(f"refusing to replace non-symlink test-prefix game directory {prefix_game}")
    if prefix_game.exists() and not prefix_game.is_symlink():
        raise RuntimeError(f"unexpected test-prefix path {prefix_game}")
    if prefix_game.is_symlink():
        prefix_game.unlink()
    prefix_game.symlink_to(GAME, target_is_directory=True)
    try:
        scope = "scenery-only" if "--scenery-only" in sys.argv[1:] else "full"
        REPORT["scope"] = scope
        for advanced in ((0,) if scope == "scenery-only" else (0, 1)):
            CFG.write_bytes(set_config(cfg_before, advanced))
            run_phase(advanced, scope)
    finally:
        # The serializer runs in a disposable game copy; restore its fixture even
        # if a UI assertion fails.
        CFG.write_bytes(cfg_before)
        WORLD_FILE.write_bytes(world_before)
        if prefix_game.is_symlink():
            prefix_game.unlink()
        if old_target is not None:
            prefix_game.symlink_to(old_target, target_is_directory=True)
        REPORT["restoration"] = {
            "cfg_sha256": sha256(CFG),
            "cfg_matches_backup": sha256(CFG) == sha256(cfg_backup),
            "world_vis_sha256": sha256(WORLD_FILE),
            "world_vis_matches_backup": sha256(WORLD_FILE) == sha256(world_backup),
            "scenery_vis_inventory_restored": True,
            "prefix_link_restored": (os.readlink(prefix_game) if prefix_game.is_symlink() else None) == old_target,
        }
        scenery = GAME / "Scenery"
        for path in scenery.rglob("*"):
            if path.is_file() and path.suffix.lower() == ".vis" and path.relative_to(scenery).as_posix() not in vis_before:
                path.unlink()
        for relative, data in vis_before.items():
            (scenery / relative).write_bytes(data)
        REPORT["restoration"]["scenery_vis_inventory_restored"] = {
            path.relative_to(scenery).as_posix(): path.read_bytes()
            for path in scenery.rglob("*")
            if path.is_file() and path.suffix.lower() == ".vis"
        } == vis_before
        REPORT["installed_game_unchanged"] = {
            "cfg_sha256": sha256(GAME_SOURCE / "FLTSIM98.CFG"),
            "world_sha256": sha256(GAME_SOURCE / "Scenery/world.vis"),
            "cfg_matches_copy_source": sha256(GAME_SOURCE / "FLTSIM98.CFG") == hashlib.sha256(cfg_before).hexdigest(),
            "world_matches_copy_source": sha256(GAME_SOURCE / "Scenery/world.vis") == hashlib.sha256(world_before).hexdigest(),
        }
    passed = all(REPORT["restoration"][key] for key in ("cfg_matches_backup", "world_vis_matches_backup", "scenery_vis_inventory_restored", "prefix_link_restored")) and all(REPORT["installed_game_unchanged"].values())
    REPORT["result"] = "PASS" if passed else "FAIL"
    report_path = OUT / "acceptance.json"
    report_path.write_text(json.dumps(REPORT, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps(REPORT, indent=2, ensure_ascii=False))
    if not passed:
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        REPORT["result"] = "FAIL"
        REPORT["error"] = str(error)
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "acceptance.json").write_text(json.dumps(REPORT, indent=2, ensure_ascii=False) + "\n")
        print(f"FAIL: {error}", file=sys.stderr)
        raise
