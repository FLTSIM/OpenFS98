# Adversarial review 2: the 2026-11-15 amendments to ARCHITECTURE-SPINE.md

Reviewed 2026-11-15. Scope: the four amended decisions in the diff, which are AD-10 (data root location), AD-12 (pressings), AD-24 (WS2_32, the DPLAY wire protocol and ports) and Deferred (LAN multiplayer). I also read the ADs they lean on: AD-9, AD-16, AD-19 and AD-22. I checked them against `src/multip/multip.c`, `src/booster/booster.c`, `docs/modules/multip.md` and `docs/building.md` ("Which CD").

Method: as in `review-adversary.md`. For each hole I build two units one level down. Each obeys every AD to the letter, and the two still build incompatibly. Each hole ends with the rule text that closes it.

**Verdict.** The amendments add three new owners: of the data-root location, of pressing normalisation, and of sockets. None of them says who owns the thing, when it is decided, or which thread it runs on. One amendment also rests on a false premise. The only known non-reference pressing is a rebuilt program, not a rebase. Separately, AD-9 as written is already contradicted by decompiled code in the tree. MULTIP's network workers are module code started with `CreateThread`. The WS2_32 amendment widens that hole to every add-on.

---

## Critical

### C1. AD-9 forbids the threads MULTIP and BOOSTER already create, and WS2_32 invites more
- **Pair:** MULTIP as decompiled ↔ `fs98win32` built from AD-9, AD-16, AD-19 and AD-22.
- **Evidence:** `src/multip/multip.c:441-466` starts `multip_receive_thread` and `multip_send_thread` with `CreateThread`, as retail does (`docs/modules/multip.md`, table at line 655). Those threads run decompiled module code. They walk the send and receive queues in MULTIP's image, call `IDirectPlay3A::Receive` and `Send`, and allocate. `src/booster/booster.c:299` starts `booster_thread` the same way.
- **Both comply:** AD-9 says "module and guest code runs only on the sim thread". It also says "MULTIP's network workers talk to the sim thread only through queues". The second sentence reads as permission for the threads that the first sentence forbids. An `fs98win32` implementer can therefore build `CreateThread` as a real OS thread, which obeys the "network workers" clause. Or they can make it fail, which obeys the "sim thread only" clause.
- **Clash:**
  - **Real thread:** the worker calls `fs98win32` exports that take pointers. Those need `FS98_FRAME_BEGIN` (AD-16), but the arena stack is "one LIFO stack on the sim thread", so the worker has none. It also has no SEH list (AD-19 defines one, the sim thread's). It allocates from AD-22's single deterministic heap at the same time as the sim thread, so heap placement depends on scheduling and CAP-1 comparison breaks. AD-20's control-word copy is per transition, not per thread.
  - **Failed `CreateThread`:** MULTIP never receives, and CAP-11 is dead.
- **What the amendment adds:** the EFB add-on is one source for two builds. Its retail DLL build may well run a blocking `recvfrom` loop on its own thread, which is the normal Win98 pattern. Natively, the same source either breaks AD-9 or hangs the frame. A guest gauge that does the same has no rule at all.
- **Rule (replace AD-9's Network bullet; add a Threads bullet):**
  - **Threads:** "`CreateThread` from module or guest code creates a *sim fiber*: a cooperative context on the sim thread. Each fiber has its own arena-stack region (AD-16), its own arena TIB and SEH list (AD-19), and its own guest `esp`. Fibers switch only inside `fs98win32` wait points. Those are the waits (`WaitFor*`, `MsgWaitForMultipleObjects`), `Sleep`, blocking socket calls, `IDirectPlay*::Receive` with no message ready, and the main loop's clock read. Between those points they run in creation order. No module or guest code ever runs on an OS thread other than the sim thread."
  - **`fs98win32`'s own threads:** "`fs98win32` may own OS threads: the DPLAY transport, a socket poller, and audio. They never touch the arena. They pass results to the sim thread through `fs98win32`-internal queues. Those queues are drained at every wait point and at the clock read, in arrival order, and the drain is logged under FRAMELOCK."

### C2. The layered root has no merge rule, and its default puts the lower layer inside the upper one
- **Pair:** the Flights save path (MAIN or Flights.DLL calls `CreateFileA("C:\FLTSIM98\FLIGHTS\...\FOO.FLT", GENERIC_WRITE)` over a stock flight) ↔ `fs98win32`'s `FindFirstFileA` and lookup.
- **Both comply:** AD-10 says "writes go to the user root". It also says lookup ignores case, and that among several matches "the first in ASCII-uppercase order wins". The stock file in the data root and the user's saved copy in the user root are equal in uppercase, so the order rule has a tie it doesn't break. One implementer resolves the open by layer order (data root first, as in "a read-only data root under a writable per-user root"). Another resolves the listing by directory-walk order. The user's save is then listed but the stock flight loads, or the other way round.
- **Further splits the rule leaves open:**
  - **Opening for write:** an existing data-root file opened read/write is either copied up first or created empty in the user root. Both are "writes go to the user root".
  - **Delete:** the Flights delete dialog (`flights-play` exercises it) runs `DeleteFileA` on a stock flight. It either fails, where retail succeeds, or "succeeds" while the file still lists.
  - **Default overlap:** AD-10's new default puts the data root "in the user root" when the binaries' folder is read-only. Without a named subfolder, the lower layer sits inside the upper one. `C:\FLTSIM98\` then shows the data root twice, once merged and once as a subdirectory, or the user root's files show up as data.
- **Rule (add to AD-10 Paths):** "The user root and the data root are disjoint directories: `<pref>/user/` and, by default, `<pref>/data/`. Setup refuses a chosen data root that contains, or sits inside, either the user root or the install folder's `Modules/`. A DOS path resolves in the user root first, then the data root. Within each layer, case variants resolve by ASCII-uppercase order. `FindFirstFileA` lists the union, deduplicated by uppercase name, with the user root winning. An open with write access copies a data-root file up before the first write. `DeleteFileA` or `RemoveDirectoryA` on a data-root entry records a whiteout in `<pref>/user/.fs98-whiteout`, which hides it from lookup and listing. `fs98win32` opens the data root read-only even when the OS would allow writes. `docs/` has a test for each of these."

## High

### H1. Nobody owns the data-root location after setup, so a folder that turns read-only, or a new release, moves it
- **Pair:** setup, which probes "is the binaries' folder writable?" and writes data there ↔ `fs98win32`'s path layer, which on a later launch reads AD-10 literally ("defaults to beside the binaries when that folder is writable, else it lives in the user root") and probes again.
- **Both comply:** AD-10 says when the default applies but not when it is evaluated or where the decision is stored. It names neither the registry file, nor `FLTSIM98.CFG`, nor a pointer file.
- **Clash:**
  - **The folder turns read-only:** an admin `chmod`s it, the install moves to `/opt` or `Program Files`, or a package manager takes it over. The path layer then re-probes, gets "else the user root", finds it empty, and either reruns setup or runs with no scenery.
  - **Upgrade:** the new tarball extracts to a new folder, which is writable and empty. The data is still in the old one.
  - **Two owners:** a portable install beside the binaries and a second version sharing one `SDL_GetPrefPath` share one user root. If setup stores the choice in the registry file but the host reads it before `fs98win32` mounts the registry, there are two owners of one value. A path the user typed into setup also has no named owner for persisting it.
- **Rule (add to AD-10):** "Only setup probes writability, once. It writes the data root's absolute host path to `<pref>/dataroot` (UTF-8, one line), and `fs98win32`'s path layer is the only reader. The path layer never re-probes. If the file is missing, or the path lacks the `Data/Retail/` manifest, setup runs with the last path pre-filled and an 'I moved it' option. It does not silently re-pick the default. The data root carries a manifest (pressing id and per-file checksums, see H2). A release that needs files the manifest lacks runs setup's copy step again, against the same root."

### H2. Pressing normalisation has three possible owners, and the known pressing isn't a rebase
- **Pair:** setup, which rebases files in `Data/Retail/` to the reference base (AD-12) ↔ the `Fs98Image` loader (AD-3), which copies "a section of a retail file in `Data/Retail/`" and could equally apply the `.reloc` delta itself when it sees `ImageBase` differ. The build generators in `tools/gen_*_data.py` are a third owner: they read `orig/` and today stop with `image base moved`.
- **Both comply:** AD-12 says the files "are normalised" without saying by whom or into what file form. AD-3 doesn't forbid the loader from rebasing.
- **Clash:**
  - **Double rebase:** if both setup and the loader rebase, the image is relocated twice. If neither does, the image keeps the wrong base.
  - **Checksums:** "must then match the reference checksums" fails at whole-file granularity. `.reloc` fixes relocated dwords, but not the PE header's `ImageBase`, timestamp or checksum. One implementer compares sections and accepts; another compares files and refuses every rebased pressing.
  - **The false premise:** `docs/building.md` says the only known non-reference pressing (ISO dated 10 November 1997) rebuilt `FLTSIM98.EXE` and ten modules, MULTIP and MAIN among them. They are "different programs" (CONTROLS has 493 commands to 383), so `.reloc` normalisation never applies to them. Its other 17 modules are byte-identical already.
  - **"Its own tables":** this would put two descriptors in one module, which breaks AD-3's "exactly one data symbol". Alternatively, a mixed install (17 reference files plus 10 foreign ones) gets half-accepted.
- **Rule (replace AD-12 Pressings):** "`Data/Retail/` holds only reference-form files. Setup's normaliser is the only code that changes retail bytes, and it is the same library the build generators link. Section bytes and the PE header fields `ImageBase`, `TimeDateStamp` and `CheckSum` are rewritten to the reference. The per-section reference checksums and the pressing id are written to `Data/Retail/manifest`. The loader checks the section checksum on every load, refuses a mismatch with a log line, and never rebases. A pressing is accepted only if every file normalises to the reference. Otherwise setup refuses it and names it. There are no per-pressing `Fs98Image` tables. The November 1997 pressing is refused until a decision in `docs/building.md` says otherwise."

### H3. The WS2_32 subset has no surface and no names, so the dual-built EFB compiles against two different APIs
- **Pair:** the EFB add-on, retail DLL build (real `WSOCK32`/`WS2_32`, `SOCKET` from the OS, `setsockopt(SO_BROADCAST)` for GDL 90 broadcast on UDP 4000, `ioctlsocket(FIONBIO)`, `select`) ↔ the same source, native build against `fs98win32`'s "WS2_32 subset (UDP and TCP sockets)".
- **Both comply:** AD-24 says "subset" without listing it. AD-6 allows only `include/fs98/win32/windows.h` and names no `winsock2.h`. AD-14 forbids `UINT_PTR`, yet `SOCKET` and `fd_set.fd_array` are pointer-width on Win64. AD-7 binds a missing guest import to a handle that raises an access violation when called, while a native module just fails to link.
- **Clash:**
  - **Missing calls:** if the subset lacks `SO_BROADCAST` or `select`, the native build fails to link, or runs and silently sends nothing.
  - **Handle values:** one implementer makes `SOCKET` the OS fd. Fds get reused, which breaks AD-6's "kernel handle values are never reused".
  - **Old imports:** FS98-era guest add-ons import `WSOCK32.DLL`, not `WS2_32`. If only `WS2_32` is served, they load (AD-7) and crash on their first call.
- **Rule (add to AD-24):** "`include/fs98/win32/winsock2.h` is the only socket header. It lists every served function and option: at least `WSAStartup`, `WSACleanup`, `WSAGetLastError`, `socket`, `bind`, `connect`, `listen`, `accept`, `send`, `sendto`, `recv`, `recvfrom`, `select`, `closesocket`, `ioctlsocket(FIONBIO)`, `setsockopt`/`getsockopt` with `SO_BROADCAST`, `SO_REUSEADDR` and `SO_RCVBUF`, `getsockname`, `gethostname`, `gethostbyname`, `inet_addr`, `inet_ntoa`, `htons`/`ntohs`/`htonl`/`ntohl` and `WSAAsyncSelect`. Each one is served under both `WSOCK32.DLL` and `WS2_32.DLL` for guests. `SOCKET` is a 32-bit `fs98win32` handle that is never reused. It is not the OS descriptor. `fd_set` keeps its Win32 layout (`u_int fd_count`, 64 × 32-bit). Blocking calls are sim-fiber wait points (C1). `WSAAsyncSelect` notifications are posted to the window's queue and delivered at message retrieval (AD-21)."

### H4. Ports: DPLAY's fixed ports have no owner per machine, and add-ons can take them
- **Pair:** `fs98win32` DPLAY, which binds UDP 47624 for enumeration and the first free ports in 2300–2400 ↔ any WS2_32 user: the EFB, a second OpenFS98 instance, or retail FS98 under wine on the same host for interop testing.
- **Both comply:** AD-24 fixes the ports but not how they are shared. On real Windows, `dplaysvr.exe` owns 47624 for every DirectPlay application on the machine, and AD-24 describes no equivalent. The WS2_32 rule lets an add-on bind anything.
- **Clash:**
  - **Two instances:** the second process can't bind 47624, so it can neither host nor answer enumeration. Which instance works depends on start order. The native↔retail interop test, the very thing the amendment promises, can't run on one machine.
  - **An add-on in DPLAY's range:** an add-on that binds 2300 first (a GDL 90 variant, or any "port 2300" multiplayer tool) makes DPLAY's session port depend on load order.
  - **The EFB's own port:** GDL 90 on 4000 doesn't collide with DPLAY, but with no rule nothing guarantees it. An EFB that `bind`s 4000 to listen collides with an EFB app on the same machine.
- **Rule (add to AD-24):** "Inside one process, WS2_32 `bind` from module or guest code to UDP 47624 or TCP/UDP 2300–2400 always fails with `WSAEADDRINUSE`, whether or not DPLAY is active. DPLAY binds 47624 with `SO_REUSEADDR`, and on macOS also `SO_REUSEPORT`, so several instances on one host can each answer enumeration. Each instance takes the first free session port in 2300–2400. Add-ons send from ephemeral ports (`bind` to port 0). An add-on's fixed port, such as the EFB's 4000, is a documented default the user can change, and it never falls in the DPLAY ranges."

### H5. Session interop: the protocol version, the clock and the wire bytes are unowned
- **Pair:** a native host's DPLAY ↔ a retail FS98 peer on Win98 with the DirectX it shipped against (MULTIP uses `IDirectPlay3A`, which is DirectX 5).
- **Both comply:** AD-24 names "[MC-DPL4CS]", the DirectPlay 4 document, but not which protocol version DPLAY emits or accepts. AD-9 says time comes from one clock that FRAMELOCK can make virtual.
- **Clash:**
  - **Version:** a DPLAY that emits DirectPlay 6 or 7 version fields is compliant, but a DirectX 5 peer may drop it. A DPLAY that speaks only the DirectX 5 version is also compliant, but can't talk to retail peers that have newer DirectX installed.
  - **Clock:** if DPLAY's keepalives and timeouts use the virtual clock under FRAMELOCK, or under a long modal loop, a real peer times the session out.
  - **Names:** player and session names in `DPNAME` are ANSI. One unit sends the UTF-8 that SDL text input gives. Another sends CP1252, which is what AD-10 maps only for file names.
  - **Other pressings:** the November pressing's rebuilt MULTIP may lay out its application messages differently. That isn't decided anywhere, and H2 refuses that pressing for installs but can't refuse it as a network peer.
  - **Untrusted input:** a LAN peer's packets reach decompiled MULTIP, which parses them unchecked, as retail does. Kept retail bugs become remote memory writes into the arena.
- **Rule (add to AD-24, and an AD-27 bullet):**
  - **Version:** "DPLAY emits the DirectX 5 protocol version and accepts every version [MC-DPL4CS] documents up to 7."
  - **Clock:** "Protocol timers run on DPLAY's own thread against real time. Only messages handed to the sim are stamped with the `fs98win32` clock. While FRAMELOCK is active, DPLAY is replaced by a recorded transport, and the real provider is never opened."
  - **Wire bytes:** "Strings on the wire are CP1252. Application-message payloads are retail bytes at retail layout, never arena offsets, with padding zeroed. MULTIP talks only to peers whose session GUID matches the reference pressing's."
  - **Untrusted packets:** "DPLAY validates framing and sizes before queueing. A kept retail bug reachable from a peer's packet is listed in `docs/retail-bugs.md` with the tag `network-reachable`, and is fixed at porting time regardless of CAP-1."
  - **Testing (AD-27):** "Native↔retail interop is tested against captured DirectPlay traffic from real Win98 or Windows. Wine's `dplayx` isn't the reference."

## Medium

- **M1. Two installs share one user root.** A portable install and a system install, or two versions, write the same `FLTSIM98.CFG` and registry file. An older version then reads keys a newer one wrote. **Rule:** the user root is per data-root manifest id (`<pref>/user-<id>/`), or AD-10 states that it is shared on purpose and that config readers ignore unknown keys.
- **M2. The CI row adds macOS runners, but AD-27 has no network test.** Multiplayer (CAP-11) has no case in the per-target report. **Rule:** AD-27's report includes a two-instance loopback session on each target, which uses H4's `SO_REUSEADDR` rule.

## Summary

| Id | Amends | Severity |
| --- | --- | --- |
| C1 | AD-9: module threads become sim fibers; `fs98win32` threads never touch the arena | critical |
| C2 | AD-10: disjoint roots, resolution order, copy-up, whiteouts | critical |
| H1 | AD-10: data-root pointer file written once by setup | high |
| H2 | AD-12, AD-3: one normaliser, reference-form `Data/Retail`, refuse the Nov-1997 pressing | high |
| H3 | AD-24, AD-6, AD-14: enumerated `winsock2.h`, WSOCK32 alias, 32-bit `SOCKET` | high |
| H4 | AD-24: reserved DPLAY ports, `SO_REUSEADDR`, ephemeral add-on ports | high |
| H5 | AD-24, AD-27: protocol version, real-time timers, CP1252, untrusted packets | high |
| M1 | AD-10: user root per manifest | medium |
| M2 | AD-27: loopback MP test | medium |
