# Review: currency of the 2026-11-15 spine amendments

- **Reviewed:** the amendments to `ARCHITECTURE-SPINE.md` in today's diff (layered data root, pressings, the `fs98win32` WS2_32 and DPLAY rule, the CI row, the install-folder diagram, and the deferred items)
- **Lens:** check each newly committed technical claim against current web sources, not memory
- **Date:** 2026-11-15

## Verdict

The amendments hold up in substance, but the DPLAY sentence claims more than the spec supports and states the ports slightly wrong. The CI row is true today, but GitHub has announced an end date for Intel macOS runners that the spine does not record. The DirectPlay optional feature on Windows 10/11 is still there, so recording retail sessions as ground truth is viable.

## Findings

### H-1 (high). "native builds and retail FS98 share LAN sessions" is only documented for peers on a DirectX 6 or later runtime

- [MC-DPL4CS] documents only these dialects: DX6VERSION (9), DX61VERSION (10), DX61AVERSION (11), DX71VERSION (12), DX8VERSION (13) and DX9VERSION (14). DX6VERSION is described as "First version documented". The dialects are stated to be backward compatible with one another.
- Appendix A note <1> says the protocol "is available only in Windows operating systems that have the DirectX 6 runtime ... or later version of the runtime installed".
- Appendix A note <2> settles the IDirectPlay3A question. The interfaces (IDirectPlay4, IDirectPlay3, and so on) "do not affect the wire protocol". Every interface speaks the dialect of the installed runtime.
- **Consequence 1, good.** Retail FS98 running on any DirectX 6+ runtime speaks a documented dialect, even though it calls IDirectPlay3A, IDirectPlay2A and IDirectPlayLobbyA. That covers Windows 98 with DirectX 6+, every NT-line Windows, and Windows 10/11 (DX9VERSION). FS98 shipped DirectX 5.0, and DirectPlay 5.0 for NT 4.0 (KB Q178415), but the shipped runtime does not matter on such a system.
- **Consequence 2, the gap.** A peer that runs only the DirectX 5 runtime FS98 installed (Windows 95/98/NT 4 without later updates) speaks a pre-DX6 dialect with a version below 9, and the spec does not describe it. Under "Capability negotiation", each system player reports its dialect, and "the host cannot allow the connection of a client that does not have the capabilities of interoperating". Whether a DX5 peer is accepted, or what it sends, is unknown.
- **Fix:** scope the sentence to "retail FS98 on a DirectX 6 or later runtime (DX6VERSION–DX9VERSION)". Say which dialect our DPLAY advertises; DX9VERSION (14) matches Windows 10/11 peers. Put DX5-runtime peers out of scope, or under the ground-truth item: they need a recorded capture before anyone claims them.
- **Related.** The DirectPlay 4 Reliable Protocol is a separate spec, [MC-DPL4R]. Whether DPSP_MSG_HEADER's size/token/SockAddr fields are present depends on it. FS98 is a DX5-era title and probably does not request it (`DPSESSION_DIRECTPLAYPROTOCOL`, a DX6 flag), but nobody has verified that. Name [MC-DPL4R] as out of scope unless a capture shows it in use.

### M-1 (medium). The port description says "UDP 47624", but the host also listens on TCP 47624

- [MC-DPL4CS] §2.1 Transport: messages use UDP when broadcast and TCP when unicast. Clients "MUST use TCP and UDP port numbers in the range from 2300 to 2400". "Enumeration messages transmitted to the DirectPlay host computer MUST be transmitted to port 47624". Broadcasts go to UDP 255.255.255.255.
- Microsoft KB 240429 (archived), "Using DirectX 7 or an earlier version": initial **TCP** connection on 47624, outbound from clients and inbound on hosts, then TCP and UDP on 2300–2400. KB 273874 gives the same ports. The Wireshark DPLAY page also lists 47624 under TCP.
- So broadcast enumeration arrives on UDP 47624, and a directed enumeration or initial contact to a known host IP is a TCP connection to 47624. A host that listens only on UDP 47624 will miss peers that are given its address directly.
- **Fix:** write "enumeration to port 47624 (UDP broadcast, and TCP to a named host), sessions on TCP and UDP 2300–2400". Note that the DirectX 8+ ports (UDP 6073, 2302–2400) belong to DirectPlay 8, a different protocol that FS98 does not use, so no one adds them.

### M-2 (medium). The CI row's Intel macOS runners have a published end date

- They are available today. The `actions/runner-images` README lists `macos-15-intel` and `macos-15-large` (macOS 15, x64), and `macos-26-intel`, `macos-26-large` and `macos-latest-large` (macOS 26, x64). The ARM labels are `macos-latest`, `macos-26`, `macos-26-xlarge`, `macos-15` and `macos-15-xlarge`. macOS 14 images retired from GitHub Actions on 2026-11-02.
- The macOS 26 GA changelog (2026-02-26) names `macos-26-intel` as "Standard macOS runner for x64".
- The end date: the macOS 13 closure notice (2025-09-19) says "GitHub will no longer support this architecture on macOS after the macOS 15 runner image is retired in Fall 2027". The later `macos-26-intel` label leaves it unclear whether that date still holds. No newer statement was found, and the README gives no end date for either Intel image.
- **Fix:** keep the row, and add "(Intel x64: `macos-15-intel` / `macos-26-intel`; GitHub has announced the end of Intel macOS runners around Fall 2027, so CAP-3 then needs self-hosted Intel hardware or must drop to build-only)". Add it to the deferred decisions too.

### L-1 (low). Recording retail FS98 sessions on Windows 10/11 is viable

- DirectPlay is still an optional feature: Windows Features > Legacy Components > DirectPlay, or `DISM /Online /Enable-Feature /FeatureName:DirectPlay /All` (Microsoft Q&A, 2024; Winaero).
- Neither Microsoft's deprecated-features list (updated 2026-11-11) nor its removed-features list (updated 2026-08-25) mentions DirectPlay or Legacy Components.
- [MC-DPL4CS] Appendix A (updated 2024-10-30) lists Windows 10, Windows 11 and Windows Server 2025 as applicable products.
- **Caveat:** a Windows 10/11 capture shows DX9VERSION traffic only. It says nothing about DX5-runtime peers (H-1). If those matter, capture on a Windows 98 VM with only DirectX 5.
- **Fix:** none required. Optionally name this as the ground-truth source in the spine, together with the dialect it yields.

## Other amended claims

| Claim | Result |
| --- | --- |
| `SDL_GetPrefPath` gives the writable per-user root | **Current.** It is an SDL3 API. No change. |
| WS2_32 subset (UDP and TCP sockets) | No currency issue. H-1 and M-1 describe what DPLAY needs from it: UDP broadcast send and receive, and TCP listen and connect. |
| Pressings normalised through `.reloc` against reference checksums | This is internal. Nothing on the web can check it. |
| "Multiplayer beyond the LAN ... after CAP-11" | Consistent. KB 240429 shows that DirectPlay 4 over NAT needs port forwarding, so deferring internet play is reasonable. |
| Data root "beside the binaries when that folder is writable" | No external claim to check. |

## Sources

- [MC-DPL4CS] Versioning and Capability Negotiation: https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/962011a7-984b-46c0-a187-41939fa1b174
- [MC-DPL4CS] DPLAYI_SUPERPACKEDPLAYER (version values): https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/59101f7c-5cee-4490-891c-ec799089eb55
- [MC-DPL4CS] DPSP_MSG_HEADER: https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/caf0ddbe-d56d-474f-9c2a-f47c84cc0da9
- [MC-DPL4CS] Transport: https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/e61a374b-be48-42a1-9fd4-c7a6efbd8b4b
- [MC-DPL4CS] Appendix A, Product Behavior: https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/b2858b2d-52e7-4c05-87b7-820e2b9d56ff
- KB 240429, DirectX ports (archived): https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/240/429.HTM
- KB 273874, Game Voice ports (archived): https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/273/874.HTM
- KB Q178415, FS98 setup installs DirectX 5.0 / DirectPlay 5.0: https://jeffpar.github.io/kbarchive/kb/178/Q178415
- Wireshark DPLAY: https://wiki.wireshark.org/DPlay
- Windows deprecated features: https://learn.microsoft.com/en-us/windows/whats-new/deprecated-features
- Windows removed features: https://learn.microsoft.com/en-us/windows/whats-new/removed-features
- Microsoft Q&A, enabling DirectPlay on Windows 11: https://learn.microsoft.com/en-us/answers/questions/1822524/how-to-enable-directplay-on-win-11-xp3-if-i-can-fi
- Winaero, installing DirectPlay on Windows 10/11: https://winaero.com/how-to-install-directplay-in-windows-11-and-windows-10
- actions/runner-images README: https://github.com/actions/runner-images
- GitHub changelog, macOS 26 GA: https://github.blog/changelog/2026-02-26-macos-26-is-now-generally-available-for-github-hosted-runners/
- GitHub changelog, macOS 13 closing down (Intel end, Fall 2027): https://github.blog/changelog/2025-09-19-github-actions-macos-13-runner-image-is-closing-down/
- GitHub changelog, macOS 14 retirement: https://github.blog/changelog/2026-10-01-github-actions-macos-14-runner-image-retirement
