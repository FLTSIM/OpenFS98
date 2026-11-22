# Rubric review 2: 2026-11-15 spine amendments

Scope: the amended parts of `ARCHITECTURE-SPINE.md`: AD-10 (data root default), AD-12 (pressings), AD-24 (WS2_32 and the DirectPlay 4 wire protocol), the Stack CI row, the seed diagram, and the Deferred changes. Checked against the spine as a whole, the spec memlog tail (DirectPlay compatibility with retail, every bug fixed, the EFB add-on, the data root default, pressings, CI) and `docs/building.md` (the known pressings).

**Verdict: not ready.** The amendments go the right way. But AD-24 adds a network stack without deciding its threading or its socket semantics, and AD-9 still describes the old design. The pressing rule promises "own tables" that AD-3 cannot hold. The data root choice has no stored location. CI cannot run the checks the Mac release story now closes on.

## Findings

### F1 (high): AD-24's network stack has no threading rule, and AD-9 is stale

AD-9 still says "MULTIP's network workers talk to the sim thread only through queues." After the amendment, MULTIP has no workers. The transport is `fs98win32`'s DPLAY on its WS2_32 subset. The spine doesn't say:
- whether DPLAY's protocol engine (enumeration replies when we host, guaranteed-message acks and retries, keepalives, the TCP accept on 2300–2400) runs on a platform worker or on the sim thread;
- which clock that engine uses.

If it runs on the sim thread, a long load or a modal dialog (MPDIALOG) stalls acks and keepalives, and retail peers drop the session. If it reads `fs98win32`'s clock, FRAMELOCK's virtual time leaks into wire timeouts. Two builders could each pick a different answer.

**Fix:** amend AD-9's Network bullet as follows. DPLAY's protocol engine and all socket I/O run on one `fs98win32` network worker using platform sockets (inside `src/platform/`) and real monotonic time. Only application-level DirectPlay messages, session and player events, and `EnumSessions` results cross to the sim thread, through queues drained at the start of a pass. `EnumSessions` callbacks and `Receive` run on the sim thread. Add `fs98win32` DPLAY and add-ons to AD-9's Binds.

### F2 (high): "WS2_32 subset" is undefined, so DPLAY and the EFB add-on can diverge

AD-24 names the DLL but no surface. Several things are undecided:
- the function list;
- blocking or non-blocking sockets (a blocking `recvfrom` on the sim thread freezes the sim, and AD-9 forbids add-on threads);
- whether `WSAAsyncSelect` or `select` is offered;
- the type and layout of `SOCKET`, which is `UINT_PTR` on Win64 and so conflicts with AD-14 and AD-6;
- which header carries it (AD-6 allows only `include/fs98/win32/windows.h`);
- whether DPLAY uses this arena-ABI surface or the platform sockets directly.

**Fix:** list the subset in AD-24: `WSAStartup`/`WSACleanup`, `socket`, `bind`, `connect`, `listen`, `accept`, `send`/`recv`, `sendto`/`recvfrom`, `select`, `ioctlsocket(FIONBIO)`, `setsockopt` (`SO_BROADCAST`, `SO_REUSEADDR`), `closesocket`, `gethostname`, `gethostbyname`, `inet_addr`/`inet_ntoa`, `htons`/`ntohs`, `WSAGetLastError`.
- Calls never block the sim thread: a blocking call made there returns `WSAEWOULDBLOCK` after a zero-timeout poll, or the rule requires `FIONBIO`.
- `SOCKET` is a `uint32_t` handle value, never reused (AD-6).
- `include/fs98/win32/winsock2.h` is the one allowed header, at AD-14 layouts.
- DPLAY sits on the platform sockets, not on the arena surface. That makes F1's worker possible.

### F3 (high): mixed sessions have no rule pinning MULTIP's payloads or the protocol version

Wire compatibility with retail needs two things the spine doesn't fix.

First, MULTIP's application messages must stay byte-identical to retail. The spec now says every retail bug is fixed and that divergence in mixed sessions is accepted. Nothing stops a "fix" that changes a MULTIP payload layout and silently breaks joining.

Second, the spine doesn't say which DirectPlay version the protocol speaks. Retail FS98 runs on whatever `dplayx.dll` the host has (DX5 on Win98, the DX9-era one on modern Windows), and MC-DPL4CS messages carry a version field.

The port list also leaves out a detail. The DirectPlay server (`DPLAYSVR`) uses TCP 47624 as well as UDP. One instance per machine owns 47624, so a native build beside retail FS98 on one Windows box would clash.

**Fix:** add to AD-24:
- MULTIP's payloads and DirectPlay message framing stay retail's on every build. A retail-bug fix that would change one is not made, and it is logged in `retail-bugs.md`.
- DPLAY sends version N and accepts versions N..M. Name the versions, and the reference partner: retail FS98 on Windows with the DirectPlay feature, not wine's `dplayx`.
- Port 47624 is TCP and UDP. If 47624 is already bound, DPLAY doesn't share it; it logs and falls back to client-only.

### F4 (high): the AD-12 "own tables" branch for pressings cannot work under AD-3

`docs/building.md`'s only known other pressing (November 1997) rebuilt `FLTSIM98.EXE` and ten modules as different programs, not rebased copies. Our native code is decompiled from the reference's `.data` layout, so a rebuilt file's initial image can't feed it whatever tables are added. AD-3's `Fs98Image` also names one retail file section, with no pressing key.

Two more things are unstated:
- whether setup writes the normalised bytes into `Data/Retail/` (so everything else sees one pressing), or whether they are normalised at load;
- where the reference checksums live.

**Fix:** state that `Data/Retail/` always holds files byte-identical to the reference after `.reloc` normalisation, checked per section against checksums committed in `docs/` (digests, not Microsoft bytes). Any pressing that can't meet that is refused, with the error naming its files. Decide the November-1997 pressing now: refuse it, or take data files from it and ask for the reference CD for `Data/Retail/`. Drop "its own tables", or open a separate AD for a per-pressing descriptor key.

### F5 (high): the data root choice is stored nowhere

AD-10 lets the data root move: beside the binaries if that folder is writable, else in the user root, and the user can change it. Three units must agree on where it is on later launches: setup, the `fs98win32` path layer and the host's "first launch" test. Nothing says where the choice is recorded or how first launch is detected.

That leaves cases open:
- A second OS user gets a fresh user root while data already sits beside the binaries. Do they run setup again?
- The install folder is moved.
- The folder is writable at setup but not later.

**Fix:** record the data root as an absolute host path in a fixed key in the user root's registry file. The path layer reads only that key. First launch means the key is missing, or `Data/Retail/` fails its checksums. Before copying, setup looks for an already-installed data root beside the binaries and offers to reuse it.

### F6 (medium): a read-only install folder leaves add-on modules and layered lookup unsettled

AD-10 now admits read-only installs (Program Files, the macOS app bundle, packages) as the reason the data root may live in the user root. But AD-2 and AD-13 scan only `Modules/` in the install folder, so a user with such an install can't add a CAP-8 native add-on or a retail-PE add-on (and CAP-9's EFB add-on is one).

AD-10 also doesn't say whether the user root overlays the data root. When third-party `Gauges/` or `Aircraft/` in the user root share a name with a data-root entry, which wins? Does `FindFirstFileA` merge the two roots? The lookup rule settles only case collisions.

**Fix:**
- AD-2/AD-13: the host also scans `<user root>/Modules/`. Its entries sort after the install's, and clashes follow AD-13.
- AD-10: the user root overlays the data root. A user-root entry wins. Listings merge and drop duplicates, then sort in ASCII-uppercase order.

### F7 (medium): CI cannot run the checks the Mac release story now closes on

The spec says the Mac release story closes on CI results. The tier-2 tolerance checks and the 74-flight report need the user's CD data (AD-12) and the reference logs, which live outside git. Neither may appear in a public GitHub Actions repo, its logs or its artifacts.

The spine also doesn't name the runner labels. GitHub's Intel macOS images are being retired: `macos-15-intel` is the last Intel image, announced to go away in 2027. That puts a date on the `macos-x86_64` target's CI.

**Fix:** add a Stack or AD-27 line naming the runners (`ubuntu-24.04`, `windows-2025`, `macos-15-intel`, `macos-15`) and noting the Intel image's end date as the revisit condition. Decide how CI gets the CD and references: an encrypted secret or private storage fetched at job start, never uploaded as an artifact, or a self-hosted runner. Otherwise CI only builds and runs CD-free tests, and the Mac story's closing condition changes.

### F8 (medium): AD-27 has no interop test for mixed sessions

CAP-11 now claims retail interoperability, but AD-27 has no test for it, and AD-8's wine baseline can't serve: wine's `dplayx` is its own partial implementation, not retail's.

**Fix:** add an AD-27 bullet. Run a scripted host/join between a native build and retail FS98 on Windows with real DirectPlay, in both directions (native hosts, retail hosts), and keep packet captures as references outside git.

### F9 (medium): Deferred and conventions still say "kept" retail bugs

The direction is now that every retail bug is fixed after CAP-1. Deferred's "Which kept retail bugs get fixed, and in what order" is therefore half stale: which ones is decided (all), and only the order is open. The Kept-retail-bugs convention should say what happens to the `RETAIL BUG (kept)` markers after CAP-1. F3's MULTIP exception is the one carve-out.

**Fix:** reword the Deferred item to "Order of the retail-bug fixes after CAP-1 (all are fixed; a player toggle is future-backlog)". Add to the convention: "after CAP-1 the marker becomes `RETAIL BUG (fixed): <id>`".

### F10 (low): stale wording in companions

`decisions.md` line 18 still says "Mixed multiplayer with retail FS98: Not a goal." `SPEC.md` (not yet re-rendered) still has CAP-11 "two native builds … over UDP", the non-goal "mixed sessions with retail FS98", and the "kept retail bugs" constraint. `epics.md` AR9 lists the system DLLs without WS2_32. AD-24's Binds lack add-ons and CAP-9. The CAP map row for CAP-9 lacks AD-24. Neither the spine nor these files mention the NFR6 exception (soft 80-bit x87 in the guest on non-x86); AD-20 should carry it.

**Fix:** update these when the SPEC is re-rendered. Add AD-24 to the CAP-9 row and "add-ons; CAP-9" to AD-24's Binds. Add one line to AD-20: "on non-x86 hosts `fs98guest` emulates x87 with soft 80-bit (NFR6 exception); native code may not."

### F11 (low): the seed diagram omits a user-root `Modules/` and the data root's own `Aircraft/` and `Gauges/`

The diagram shows the data root holding "CD data" and only the user root holding `Aircraft/` and `Gauges/`. Retail aircraft data (not code, per AD-13) lives in the data root. That is fine, but the diagram should show it, plus `Modules/` in the user root if F6 is adopted.

## Rubric

| Check | Result |
| --- | --- |
| Fixes the real divergence points, misses none | Partial. Network threading (F1), socket semantics (F2), payload and version pinning (F3), and the data root record (F5) are missed. |
| Each Rule enforceable and prevents its divergence | Partial. "WS2_32 subset" and "own tables" are not enforceable (F2, F4). The AD-10 default is enforceable once it is stored (F5). |
| Nothing in Deferred lets two units diverge | Mostly. "Multiplayer beyond the LAN" is safe. The removed "MULTIP transport details" was replaced by AD-24 but leaves the threading open (F1). The retail-bugs item is stale (F9). |
| Named tech current | MC-DPL4CS, its ports and GitHub Actions are current. The DirectPlay version (F3) and the Intel macOS runner end of life (F7) need naming. |
| Covers the spec's capabilities | CAP-11 interop has no test (F8). CAP-8/CAP-9 add-ons have no home on read-only installs (F6). The Mac release story depends on CI access to the CD (F7). |
| Every dimension decided, deferred or open | CI data access, pressing outcomes for the known November pressing, and the DPLAY version are none of these. |

## Cross-checks asked for

- **AD-9 and sockets:** contradicted. See F1.
- **AD-13 setup rules:** consistent. `Data/Retail/` copies aren't modules, and normalised files stay non-executed. The gap is the user-root add-on scan (F6).
- **CAP-11's old "not mixed sessions" wording:** gone from the spine. Still present in `SPEC.md` line 81 and `decisions.md` line 18 (F10).
- **AD-10 and AD-12 against each other:** consistent. The seed diagram matches AD-10.
