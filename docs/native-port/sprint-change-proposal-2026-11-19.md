# Sprint Change Proposal: earlier first release

Date: 2026-11-19 · Requested by Steve · Mode: batch · Scope: **Moderate** (backlog reorganisation, one SPEC section)

## 1. Issue summary

**What changes.** As planned, players get nothing until Story 8.5. Install from CD (Epic 8) waits behind the guest interpreter (Epic 6) and the add-ons and EFB (Epic 7). Steve has chosen an earlier first release:

- **Release 1** comes after Epic 5: a Linux x86-64 tarball and a Windows x64 zip. It has first-run CD setup, all 74 flights and every retail dialog, with no wine.
- **Later releases** add third-party aircraft and gauges (the guest), native add-ons and the EFB, Mac apps, and then multiplayer.

**How it came up.** A planning review on 2026-11-19, while Story 1.3 was in progress. No implementation finding triggered it. It's a strategic resequencing: category *strategic pivot*, not a technical failure.

**Evidence that the reorder is safe:**
- Release 1 doesn't need the guest. Retail's own aircraft fly on our decompiled gauges, and Story 3.6 already logs "guest not yet available" for a PE with code.
- The SPEC already accepts staged releases. Multiplayer is a non-goal for "the first native releases".
- There's one real dependency to cut: setup (8.4) is "built on `dialog.h`", which is Story 7.3.

## 2. Impact analysis

### Checklist

| Item | Status | Finding |
| --- | --- | --- |
| 1.1 Trigger | [x] | No story triggered this; it's a planning decision made during Story 1.3 |
| 1.2 Problem | [x] | Strategic pivot: the first release needs to come sooner |
| 1.3 Evidence | [x] | See above |
| 2.1 Current epic (1) | [x] | Unaffected; 1.3 continues |
| 2.2 Epic changes | [!] | Epic 8 moves up to become Epic 6 and gets the native half of `dialog.h`; Story 7.3 is split |
| 2.3 Future epics | [!] | Guest becomes Epic 7 and add-ons become Epic 8. Epics 9 and 10 keep their numbers; their references to moved stories change |
| 2.4 Obsolete or new epics | [N/A] | None removed or added |
| 2.5 Order | [!] | New order: 1–5, install and Release 1, guest, add-ons, Mac, multiplayer |
| 3.1 SPEC (in place of a PRD) | [!] | The success signal splits into Release 1 and later releases; Assumptions renumbered |
| 3.2 Architecture spine | [x] | No change. AD-8 stays, and Story 5.2 is brought in line with it |
| 3.3 UX | [N/A] | No UX document; the only UX change is in the release notes (Story 6.6) |
| 3.4 Other artifacts | [!] | `sprint-status.yaml` (keys change, 3 action items added); BMAD config `planning_artifacts` |
| 4.1 Direct adjustment | Viable | Low effort, low risk |
| 4.2 Rollback | Not viable | Nothing completed is affected |
| 4.3 MVP review | Viable, as part of a hybrid | The success signal is restated in two stages, and nothing is dropped |

**Epics affected.** Epics 6, 7 and 8 are renumbered, and Epics 9 and 10 change only in their cross-references. Every affected story is still `backlog`, so no progress is lost.

**Technical impact.** None in code. Nothing touches `src/` or the wine prefix, so Story 1.3's test runs aren't disturbed.

## 3. Recommended approach

**A hybrid of direct adjustment and an MVP restatement.** Renumber and re-link the epics, split Story 7.3, and restate the SPEC success signal as two releases. All capabilities stay in scope; only the release boundary moves.

- **Effort:** low (docs only).
- **Risk:** low.
- **Timeline:** players get a build three epics sooner. The guest and add-on work is unchanged, only later.

**Trade-off.** Release 1's first reports will include "my 1998 add-on aircraft has no panel". The release notes say so up front (Story 6.6).

**Alternative considered.** Keep the epic numbers and change only the dependencies. Rejected because the sprint tool orders epics by number and would keep recommending the guest first.

## 4. Detailed change proposals

### 4.0 BMAD config

`_bmad/custom/config.toml`, both the `bmm` and `gds` tables:

```
OLD: planning_artifacts = "{project-root}/OpenFS98/docs/planning"
NEW: planning_artifacts = "{project-root}/OpenFS98/docs/native-port"
```

Rationale: `docs/planning` doesn't exist, and every planning skill was looking in the wrong place.

### 4.1 Epic renumbering (`epics.md`)

| Old | New | Title |
| --- | --- | --- |
| Epic 8 (8.1–8.5) | **Epic 6** (6.1–6.6) | Install from a CD image, and Release 1 |
| Epic 6 (6.1–6.7) | **Epic 7** (7.1–7.7) | Third-party aircraft and gauges |
| Epic 7 (7.1–7.5) | **Epic 8** (8.1–8.5) | Add-ons and new dialogs |

Story map:
- **Install and Release 1:** 8.1→6.1, 8.2→6.2, 8.3→6.3, *new* 6.4, 8.4→6.5, 8.5→6.6.
- **Guest:** 6.n→7.n.
- **Add-ons:** 7.n→8.n.

Every "Given Story X" and cross-reference follows the map, including:
- 9.3: "Story 6.1's instruction suite" → 7.1.
- 10.2: "Story 7.4's WS2_32 subset" → 8.4.
- The 7.2 to 7.7 internal links.

The Overview's FR Coverage Map and Epic List are updated:
- FR12 → Epic 6.
- FR7 → Epic 7.
- FR6, FR8 and FR9 → Epic 8.
- The Epic List is reordered.

### 4.2 Epic 6 header (old Epic 8)

```
OLD: ## Epic 8: Install from a CD image
     A fresh release archive plus the user's FS98 CD, any known pressing, reaches a flight.
     Covers FR12, NFR1 (runtime), AD-10, AD-12, AD-13, AD-26.
NEW: ## Epic 6: Install from a CD image, and Release 1
     A fresh release archive plus the user's FS98 CD, any known pressing, reaches a flight,
     and Release 1 ships for Linux x86-64 and Windows x64. Third-party binaries that need
     the guest come in a later release (Epic 7). Covers FR12, NFR1 (runtime), AD-10, AD-12,
     AD-13, AD-25 (native half), AD-26.
```

### 4.3 Story 6.1 (old 8.1): dependency

```
OLD: **Given** Epic 7 and the ISOs collected so far
NEW: **Given** Epic 5 and the ISOs collected so far (catalogued with the action-item script)
```

### 4.4 New Story 6.4: the dialog API, natively (the native half of old 7.3)

```
### Story 6.4: The dialog API, natively

As the developer,
I want `include/fs98/dialog.h` mapped onto `fs98mfc`, with dialog resources shipped as a
`.res` blob in `fs98_resources`,
So that setup, and later the add-ons, show FS98-style dialogs from one API.

**Acceptance Criteria:**

**Given** Epic 5
**When** a test dialog is built from `dialog.h` with its resources in a blob
**Then** it opens and works natively on `linux-x86_64` and `windows-x64` through `fs98mfc`
(AD-25), and the resource reader serves the blob (AD-26)
**And** it matches the retail dialogs beside it: SIMSUI controls, fonts and chrome
**And** a FRAMELOCK script drives it in a test
```

Story 6.5 (old 8.4) then reads **Given** Story 6.3 and Story 6.4.

### 4.5 Story 6.6 (old 8.5): Release 1

```
OLD: ### Story 8.5: Release archives
     ...
     **Then** a check finds no CD file, retail resource or raw dump in either archive
     **And** a fresh extract plus an ISO completes setup and starts a flight on each platform
     (FR12), with the data installed beside the binaries
NEW: ### Story 6.6: Release 1 archives
     ...
     **Then** a check finds no CD file, retail resource or raw dump in either archive
     **And** a fresh extract plus an ISO completes setup and starts a flight on each platform
     (FR12), with the data installed beside the binaries
     **And** the release notes say plainly that third-party aircraft and gauges built as
     Windows binaries need a later release, and that until then they are skipped with a
     "guest not yet available" log line (Story 3.6)
```

### 4.6 Story 8.3 (old 7.3): only the EFB dialog and the retail mapping remain

```
OLD: ### Story 7.3: The dialog API, with the EFB settings dialog
     I want `include/fs98/dialog.h` and resources shipped as a `.res` blob in `fs98_resources`,
     ...
     **Given** Story 7.2
     ...
     **Then** `dialog.h` maps onto retail MFC42 ordinals in `win32-retail` and onto `fs98mfc`
     natively (AD-25), and the resource reader serves the blob (AD-26)
NEW: ### Story 8.3: The EFB settings dialog, in both builds
     I want `dialog.h` (Story 6.4) also mapped onto retail MFC42 ordinals in `win32-retail`,
     ...
     **Given** Story 8.2 and Story 6.4
     ...
     **Then** `dialog.h` maps onto retail MFC42 ordinals in `win32-retail` (AD-25), and retail
     FS98 reads the same resource blob
```

The other ACs stay: the dialog opens and works natively and in retail FS98 under wine (FR6), and it matches the retail dialogs beside it.

### 4.7 Story 7.1 (old 6.1): dependency

```
OLD: **Given** Epic 5
NEW: **Given** Epic 6 (Release 1 is out) and the recorded results from the `-m32` recorder action item
```

### 4.8 Story 5.2: bring it in line with AD-8

```
OLD: **Then** all 74 flights are frame-identical in both
NEW: **Then** all 74 flights are frame-identical in both to an i386 run of the same commit
     (AD-8), and to Story 4.4's references
```

Rationale: AD-8 compares CAP-2 with a same-commit i386 run, while Story 5.2 compared it with the frozen references. Doing both catches drift both ways. Stories 5.3 and 5.5 still use Story 4.4's references, as AD-8 says.

### 4.9 SPEC (through `bmad-spec` update, since `.memlog.md` is canonical)

**Success signal:**

```
OLD: - On Linux x86-64 and Windows x64, a user with only the FS98 CD installs OpenFS98 through
       first-run setup, flies any of the 74 flights frame-identical to retail, uses every retail
       dialog, and loads a retail gauge binary and a native add-on, with no wine. Mac users
       download a release app for Intel or Apple Silicon, which installs from the CD the same way
       and stays within its documented tolerances.
NEW: - **Release 1:** on Linux x86-64 and Windows x64, a user with only the FS98 CD installs
       OpenFS98 through first-run setup, flies any of the 74 flights frame-identical to retail,
       and uses every retail dialog, with no wine.
     - **Later releases:** the same user also loads a retail gauge binary, a third-party aircraft
       and a native add-on. Mac users download a release app for Intel or Apple Silicon, which
       installs from the CD the same way and stays within its documented tolerances.
```

**Assumptions** (renumbered):
- "Story 6.7" → "Story 7.7".
- "Story 8.1" → "Story 6.1".
- "Story 10.1" is unchanged.

CAP-1 to CAP-12, the constraints and the non-goals are unchanged.

### 4.10 `sprint-status.yaml`

Regenerate from the new `epics.md`. Old keys for Epics 6–8 drop out as orphans; all of them are `backlog`, so nothing needs transplanting. Add these action items:

```yaml
action_items:
  - epic: 6
    action: "ISO pressing catalogue script for Story 6.1: records each pressing's game-file and resource checksums in docs/building.md's format and flags differences from the reference; tested here against the reference CD image; Steve runs it at home on his ISOs the week of 2026-11-23"
    owner: "Steve"
    status: open
  - epic: 10
    action: "DirectPlay capture checklist for Story 10.1: DirectPlay legacy component, capture UDP 47624 and TCP/UDP 2300-2400, session script (host, join, chat, fly, leave, then swap hosts), file names; Steve captures it at home on two Windows machines"
    owner: "Steve"
    status: open
  - epic: 7
    action: "-m32 instruction-result recorder for Story 7.1: standalone program outside src/, no wine; records integer, flag and x87 results once on a real CPU; deterministic (two runs, same bytes)"
    owner: "dev"
    status: open
```

The script and checklist are written now. Neither touches `src/` or the shared wine prefix while Story 1.3's runs are going.

### 4.11 Stories 1.6 and 3.6: an early headless smoke run (added at approval)

```
Story 1.6, new AC:
**And** one deterministic flight is also recorded with a GLOBAL dump each pass, for Story 3.6's
headless smoke run

Story 3.6, new AC:
**And** a headless smoke run flies Story 1.6's GLOBAL-dump flight with every module linked, and
compares GLOBAL (relocated dwords masked) after each pass with the reference. The result is
reported, not gating until Story 3.12; each difference is explained in the story before
Story 3.7 starts
```

Rationale: as planned, Epic 3 has no flight check until Story 3.12. This check catches sim-state drift while only the host and kernel layers exist.

## Approval

Approved by Steve on 2026-11-19, with 4.11 added. Applied the same day: 4.0, 4.1–4.8, 4.10 and 4.11. 4.9 (SPEC) goes through `bmad-spec`.

## 5. Implementation handoff

**Scope: Moderate.** This is a backlog reorganisation plus one SPEC section. Nothing escalates to the architecture.

| Who | Does |
| --- | --- |
| Developer (this session) | 4.0 config, 4.1–4.8 in `epics.md`, 4.10 regeneration and action items |
| `bmad-spec` (update) | 4.9, so that SPEC.md and its `.memlog.md` stay canonical |
| Steve | Reviews and commits; runs the ISO script and the capture the week of 2026-11-23 |

**Success criteria:**
- `epics.md` has 10 epics and 67 stories (66 plus 6.4).
- Every "Given" points backwards.
- `sprint_plan.py generate` reports no warnings beyond the two known non-epic headings.
- The three action items are open.
- SPEC's success signal names two releases.
