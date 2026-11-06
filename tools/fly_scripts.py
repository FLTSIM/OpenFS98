#!/usr/bin/env python3
"""Write the FRAMELOCK_SCRIPT files for flying the game (tools/hg2d_frames.sh, frames_many.sh).

    tools/fly_scripts.py DIRECTORY         one <scenario>.txt per scenario below
    tools/fly_scripts.py --list            the scenarios
    tools/fly_scripts.py --drop SCENARIO N  that scenario's script without step N, on stdout
    tools/fly_scripts.py --steps SCENARIO   its steps, numbered

A script is "pass message wparam lparam" in hex.  A command is posted as WM_COMMAND with
its 0x1xxxx id, as MAIN's menus post them (H_POST), so shifted keys work without a real
keyboard state; the ids and names are CONTROLS' table.  A few use WM_KEYDOWN so that the
key tables are on the path too.  Each step below is (pass, name), or (pass, name, count)
for a command repeated on each of `count` passes, like a held key.  The first pass is
after the flight has loaded and is drawing: the frame logs show that by about pass 30.
"""
import pathlib
import re
import sys

WM_KEYDOWN = 0x100
WM_KEYUP = 0x101
WM_COMMAND = 0x111

ROOT = pathlib.Path(__file__).resolve().parent.parent

# A scenario flies one thing long enough for it to reach the picture.  None needs a
# particular flight; tools/frames_many.sh runs them on several.
SCENARIOS = {
    # Drive the aircraft into the ground; acceptance requires its crash MDL.
    "crash_models": [
        (32, "THROTTLE_FULL"),
        (40, "NUMPAD8", 240),
        (320, "NUMPAD8", 120),
    ],
    # Change the active view while exercising the aircraft's model state.
    "models": [
        (40, "VIEW_MODE_REV"),
        (60, "THROTTLE_FULL"),
        (80, "FLAPS_DOWN"),
        (140, "GEAR_TOGGLE"),
        (200, "GEAR_TOGGLE"),
        (240, "FLAPS_UP"),
        (280, "NUMPAD4", 12),
        (340, "NUMPAD6", 24),
        (400, "ALL_LIGHTS_TOGGLE"),
        (440, "LANDING_LIGHTS_TOGGLE"),
        (480, "RUDDER_LEFT", 10),
        (520, "RUDDER_RIGHT", 10),
    ],
    # Throttle, pitch and roll: the flight model, then the terrain moving under it.
    "controls": [
        (60, "THROTTLE_FULL"),
        (120, "NUMPAD2", 30),       # elevator up
        (240, "NUMPAD4", 12),       # aileron left
        (330, "NUMPAD6", 24),       # aileron right
        (430, "NUMPAD4", 12),
        (500, "NUMPAD8", 30),       # elevator down
        (600, "THROTTLE_DECR", 10),
        (640, "RUDDER_LEFT", 10),
        (680, "RUDDER_RIGHT", 10),
        (720, "THROTTLE_CUT"),
    ],
    # Flaps, gear, brakes, spoilers, lights, smoke: panel and flight model together.
    "systems": [
        (60, "FLAPS_INCR", 2),
        (110, "FLAPS_DOWN"),
        (160, "GEAR_TOGGLE"),
        (220, "GEAR_TOGGLE"),
        (280, "FLAPS_UP"),
        (320, "SPOILERS_TOGGLE"),
        (360, "SPOILERS_TOGGLE"),
        (400, "BRAKES", 5),
        (440, "PARKING_BRAKES"),
        (480, "ALL_LIGHTS_TOGGLE"),
        (520, "LANDING_LIGHTS_TOGGLE"),
        (560, "STROBES_TOGGLE"),
        (600, "SMOKE_TOGGLE"),
        (660, "ANTI_ICE_TOGGLE"),
        (700, "PITOT_HEAT_TOGGLE"),
        (740, "SMOKE_TOGGLE"),
    ],
    # Every view direction, zoom, the view modes, a second view and a map.
    "views": [
        (60, "VIEW_RIGHT"),
        (90, "VIEW_REAR"),
        (120, "VIEW_LEFT"),
        (150, "VIEW_DOWN"),
        (180, "VIEW_FORWARD"),
        (210, "ZOOM_PLUS", 8),
        (260, "ZOOM_MINUS", 8),
        (300, "ZOOM_1X"),
        (330, "VIEW_MODE"),
        (390, "VIEW_MODE"),
        (450, "VIEW_MODE"),
        (510, "VIEW_MODE_REV"),
        (540, "PAN_LEFT", 10),
        (570, "PAN_UP", 10),
        (600, "PAN_RESET"),
        (630, "NEW_VIEW"),
        (680, "NEXT_VIEW"),
        (710, "CLOSE_VIEW"),
        (740, "NEW_MAP"),
        (800, "CLOSE_VIEW"),
        (830, "PANEL_TOGGLE"),
        (870, "PANEL_TOGGLE"),
    ],
    # The radios digit by digit, then the autopilot's holds flying the aircraft.
    "avionics": [
        (60, "COM_RADIO"), (64, "PLUS", 3), (70, "MINUS"),
        (90, "NAV_RADIO"), (94, "PLUS", 3), (100, "MINUS"),
        (120, "VOR_OBS"), (124, "PLUS", 20),
        (160, "ADF"), (164, "PLUS", 3),
        (190, "XPNDR"), (194, "PLUS", 2),
        (220, "BAROMETRIC"), (224, "MINUS", 3),
        (250, "RADIO_VOR1_IDENT_TOGGLE"),
        (280, "THROTTLE_FULL"),
        (300, "AP_MASTER"),
        (340, "AP_HDG_HOLD"),
        (400, "AP_ALT_HOLD"),
        (460, "AP_WING_LEVELER"),
        (520, "AP_NAV1_HOLD"),
        (580, "AP_ATT_HOLD"),
        (640, "AP_AIRSPEED_HOLD"),
        (700, "AUTO_THROTTLE_ARM"),
        (760, "AP_MASTER"),
    ],
    # Slew moves the aircraft across scenery fast, which is what loads new terrain.
    "slew": [
        (60, "SLEW_TOGGLE"),
        (80, "SLEW_AHEAD_PLUS", 8),
        (180, "SLEW_RIGHT", 6),
        (260, "SLEW_ALTIT_UP_FAST"),
        (320, "SLEW_ALTIT_FREEZE"),
        (340, "SLEW_RESET"),
        (360, "SLEW_HEADING_PLUS", 6),
        (440, "SLEW_RESET"),
        (460, "SLEW_AHEAD_MINUS", 8),
        (560, "SLEW_RESET"),
        (580, "SLEW_PITCH_PLUS", 4),
        (620, "SLEW_LEFT", 4),
        (680, "SLEW_RESET"),
        (700, "SLEW_TOGGLE"),
    ],
    # The toggles a pilot reaches for: pause, sim rate, sound, full window, readouts.
    "toggles": [
        (60, "PAUSE_TOGGLE"),
        (100, "PAUSE_TOGGLE"),
        (140, "SIM_RATE"), (144, "PLUS", 2),
        (240, "SIM_RATE"), (244, "MINUS", 2),
        (300, "SOUND_TOGGLE"),
        (340, "SOUND_TOGGLE"),
        (380, "FULL_WINDOW_TOGGLE"),
        (440, "FULL_WINDOW_TOGGLE"),
        (500, "READOUTS_FLIGHT"),
        (560, "READOUTS_FLIGHT"),
        (620, "LANDME_TOGGLE"),
        (680, "LANDME_TOGGLE"),
        (740, "SITUATION_RESET"),
    ],
    # The same as "controls" but through real key messages and the key tables.
    "keys": [
        (60, "key:F4"),
        (120, "key:DOWN", 30),
        (240, "key:LEFT", 12),
        (330, "key:RIGHT", 24),
        (430, "key:LEFT", 12),
        (500, "key:UP", 30),
        (600, "key:F2", 10),
        (640, "key:G"),
        (700, "key:S"),
        (760, "key:S"),
        (820, "key:F1"),
    ],
}

KEYS = {"F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73,
        "LEFT": 0x25, "UP": 0x26, "RIGHT": 0x27, "DOWN": 0x28}


def commands():
    table = {}
    text = (ROOT / "src/controls/controls_table.inc").read_text()
    for number, name in re.findall(r'0x(1[0-9a-f]{4}), 0x[0-9a-f]+, "(\w+)"', text):
        table[name] = int(number, 16)
    return table


def lines(steps, table):
    events = []
    for step in steps:
        at, name = step[0], step[1]
        count = step[2] if len(step) > 2 else 1
        for index in range(count):
            if name.startswith("key:"):
                key = name[4:]
                code = KEYS[key] if key in KEYS else ord(key)
                events.append((at + index, WM_KEYDOWN, code, 1))
                events.append((at + index, WM_KEYUP, code, 0xc0000001))
            else:
                events.append((at + index, WM_COMMAND, table[name], 0))
    events.sort(key=lambda event: event[0])
    return "".join("%x %x %x %x\n" % event for event in events)


def main():
    if sys.argv[1:] == ["--list"]:
        print("\n".join(SCENARIOS))
        return 0
    if sys.argv[1:2] == ["--steps"]:
        for index, step in enumerate(SCENARIOS[sys.argv[2]]):
            print(index, step[1], "at", step[0])
        return 0
    if sys.argv[1:2] == ["--drop"]:
        steps = list(SCENARIOS[sys.argv[2]])
        del steps[int(sys.argv[3])]
        sys.stdout.write(lines(steps, commands()))
        return 0
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    table = commands()
    for name, steps in SCENARIOS.items():
        (out / (name + ".txt")).write_text(lines(steps, table))
    return 0


if __name__ == "__main__":
    sys.exit(main())
