# DirectPlay capture checklist (for Story 10.1)

Capture retail FS98 multiplayer over a real LAN, so Story 10.1 has its ground truth and Stories 10.2 and 10.4 have something to replay. Two Windows machines, one evening. Machines **A** and **B**.

## Before you start (both machines)

1. **FS98 is installed** from the reference CD (`docs/building.md`, "Which CD"), and a free flight starts.
2. **DirectPlay is on.** Go to Control Panel → Programs → Turn Windows features on or off → Legacy Components → **DirectPlay**, then reboot.
3. **Wireshark is installed**, with Npcap.
4. **Note down, in `notes.txt`:**
   - the Windows version (`winver`);
   - the version of `C:\Windows\SysWOW64\dplayx.dll` (Properties → Details);
   - each machine's IPv4 address (`ipconfig`);
   - which pressing is installed.
5. **Firewall:** allow FS98 on private networks when Windows asks. If it doesn't ask, add inbound rules for UDP 47624 and TCP/UDP 2300–2400. Note which you did.
6. **Clocks:** sync both (Settings → Time → Sync now), so the two captures line up.

## Capture settings (both machines)

- **Interface:** the LAN adapter.
- **Capture filter:** `udp port 47624 or tcp port 47624 or portrange 2300-2400`
- **Format:** pcapng. Start capturing **before** opening FS98's multiplayer dialog, and stop **after** the last step.

## Session 1: A hosts, B joins

Write the clock time of each step into `notes.txt` as you do it.

1. Start both captures.
2. **A:** Flights → Multiplayer → Connect. Choose to host, with callsign `HOSTA`. Note the session name, the player limit and any other settings you pick.
3. **B:** Connect, then join, over TCP/IP, with callsign `JOINB`:
   - first with the host address field **blank** (the broadcast enumeration);
   - then cancel and join again **with A's IP typed in**.
4. Each machine sends two chat lines: one short, and one near the maximum length.
5. Both fly in sight of each other for about 2 minutes, with one turn and one change of altitude.
6. **B** leaves (disconnects) while A stays.
7. **B** joins again and flies for 30 seconds.
8. **A** leaves while B is connected (host-left). Note what B shows.
9. Stop both captures.

## Session 2: B hosts, A joins

Repeat Session 1 with the roles swapped (`HOSTB`, `JOINA`).

## Files

Put everything in one folder, `dplay-capture-YYYY-MM-DD/`:

| File | Contents |
| --- | --- |
| `s1-A-host.pcapng`, `s1-B-join.pcapng` | Session 1, from each machine |
| `s2-B-host.pcapng`, `s2-A-join.pcapng` | Session 2, from each machine |
| `notes.txt` | Versions, addresses, firewall rules, the step times, anything odd (dialogs, errors, delays) |
| `screens/` | Optional: screenshots of each multiplayer dialog page and of the host-left message |

Keep the folder outside git: it holds real addresses, and it's test data. It belongs next to the references in `bin/`. Story 10.1's write-up cites it by name.

## If something goes wrong

- **B can't find A's session with a blank address:** keep that capture, note it, and carry on with the typed IP. It's a finding, not a blocker.
- **FS98 can't start multiplayer at all:** check that `dplayx.dll` exists. Then capture what you can, and note the exact error.
