# Neo1 Inline Documentation Cleanup Plan

Date: 2026-08-23

Status: In progress; phases 1-6 and Phase 7 checkpoint 1 complete

Purpose: make the current code legible and truthful before portable-core
extraction without changing behavior, blessing temporary architecture, or
creating broad comment churn in code that is about to move.

## Decision

A focused inline-documentation pass is worthwhile before portable-core work.
It should correct misleading ownership and behavioral claims at extraction
boundaries first. It should not attempt to make every file equally verbose or
polish temporary implementations as though they were the intended design.

The prerequisite pass ends when a contributor can identify the current owner,
observable contract, and known target deviation for the shared machine, both
CPU paths, the physical bus, and each 6502-visible device. Lower-risk cleanup in
6502 payloads and support files may follow as separate commits but should not
delay the first portable-core extraction.

## Research basis

This plan was built from the current code, `docs/architecture.md`,
`docs/current-state.md`, and `docs/portable-core-baseline.md`. Comments were
inventoried only in Neo1-owned source and build files. Vendored submodules were
excluded.

The main findings are:

| Surface | Evidence | Risk |
| --- | --- | --- |
| Shared machine | `src/systems/neo1.h` calls itself a minimal Neo6502 runtime, describes only the `$E000` ROM layout, and directly includes Pico-owned storage headers even though SDL consumes it | High: obscures the boundary the portable-core work must establish |
| CPU selection | `neo1_cpu_backend.h` describes the SDL backend as a future milestone; `soft65C02cpu.h` has almost no contract commentary; `wdc65C02cpu.h` still contains `TODO: docs` | High: reset, cycle, and ownership semantics differ materially by runner |
| Physical bus | GPIO, latch output-enable order, active-low control lines, delays, and bus direction are expressed mostly by code or redundant pin-name comments | High: cleanup must preserve verified ordering and explain why it exists |
| Pico runner | Its file header says Neo1-23 and calls the file a thin orchestrator, while it supports both profiles and still owns RAM-tool installation, physical timing, terminal wiring, USB polling, and pacing | High: overstates the present separation |
| MSC and VCFFA1 | Public headers contain milestone language such as “Phase 2,” “M1,” and “incremental bring-up”; one MSC implementation comment still says debug defaults on; SDL implements divergent protocols under the same function names with only a short “M1” header | High: these are 6502-visible contracts and known portability gaps |
| Terminal, video, and input | Pico headers are comparatively descriptive; SDL has a second terminal model and mixed platform API with almost no module or API commentary | Medium-high: duplication and behavioral differences should be visible before extraction |
| Build files | Some comments explain stable policy; others preserve temporary wording or anecdotes, such as “for now,” “Serial-only,” and a claimed power-cycle issue around generated RAM images | Medium: distinguish supported configuration from historical rationale |
| 6502 payloads | VACI and the VCFFA1 utility have hundreds of useful assembly comments, but some safety, deferred-feature, and range claims require verification against current code and known defects | Medium: valuable comments must be retained while stale claims are corrected |
| Orphan/legacy-looking files | `src/hid_app.c` and `src/msc_app.c` describe active host-app responsibilities but are not in either current target's CMake source list | Medium: comments can mislead readers about the active input/storage path |
| Imported and generated code | `chips_common.h`, `clk.h`, `mem.h`, and `wdc65C02cpu.h` retain derived-code notices; ROM/RAM image headers are generated; `fake65c02.h` has unresolved provenance | High if edited indiscriminately: attribution, license notices, and generated content must be preserved |

Comment volume is also uneven. The Pico runner and public Pico headers are
heavily narrated, while the SDL runner, SDL platform API, software CPU adapter,
and SDL storage bridge have few comments despite owning important behavior.
The goal is better contract coverage, not equal comment counts.

## Scope and guardrails

### In scope

- File headers and public API comments in Neo1-owned C, header, assembly,
  Python, and CMake files.
- Comments that state ownership, address ranges, read/write semantics, status
  and error meanings, interrupt behavior, bus timing, buffer ownership,
  concurrency, or target differences.
- Removal or replacement of obsolete milestone labels, vague “currently” or
  “for now” language, and comments that merely restate the next line.
- Explicit classification of files that are retained but not linked into a
  current target.

### Out of scope

- Behavioral fixes, architectural extraction, symbol renaming, formatting
  sweeps, or deletion of unused-looking files.
- Vendored submodules under `lib/`, generated byte-array headers, binary image
  content, and license or attribution text.
- Rewriting `fake65c02.h` comments before its provenance and dependency decision
  is resolved.
- Treating SDL accommodations as authentic Apple-1 behavior or as the future
  platform API.
- Adding speculative Fruit Jam interfaces or universal-HAL terminology.

If research for a comment exposes a code defect, record it in
`docs/current-state.md` and stop at documentation. Fix the defect in a separate
behavioral change.

## Comment standard

Public module and API comments should answer only the questions relevant to
their surface:

1. What physical component or 6502-visible behavior does this represent?
2. Which layer owns the state, and which target or runner consumes it?
3. What are the inputs, outputs, side effects, and lifetime or buffer rules?
4. Which addresses, status bits, errors, or interrupts are observable?
5. Which ordering, timing, active-low polarity, or concurrency invariant must
   be preserved?
6. Is a target difference authentic behavior, a platform seam, or a temporary
   accommodation?

Implementation comments should explain why an operation or invariant exists.
They should not narrate straightforward assignments or preserve old milestone
history. Bare `TODO` markers are not acceptable: either replace them with a
verified contract, identify a concrete unresolved question, or remove them.

Stable contracts belong inline at their public interface and in
`docs/architecture.md`. Dated evidence and known deviations belong in
`docs/current-state.md`; inline comments may point readers there rather than
duplicating changing status.

## Acceptance criteria

The cleanup is complete when:

- The reset-to-WozMon path, one physical bus read, and one physical bus write
  can be followed through comments without relying on framework terminology.
- `src/systems/neo1.h` describes the code that exists, including both profiles,
  both CPU consumers, ROM protection, and its present target leakage, without
  claiming the desired extraction is already complete.
- The W65C02 wrapper documents latch ownership, bus direction, clock phase,
  active-low RESET/IRQ/NMI, and timing-sensitive ordering.
- The software CPU adapter documents its process-global bridge state,
  instruction-step semantics, reset/IRQ behavior, and why it is not machine
  state.
- Every Apple-1 or storage register interface states address ownership,
  read/write behavior, status/errors, interrupt behavior, and compatibility
  classification, or points to one authoritative nearby contract that does.
- Pico and SDL runner comments accurately identify their current ownership and
  known behavioral differences without normalizing them.
- No active-code comment uses obsolete M0/M1/phase language as present design.
- Files not linked by either target are explicitly identified as retained
  legacy/reference candidates only after build ownership is rechecked.
- License/provenance notices and generated-file warnings are unchanged.
- The final diff contains documentation-only edits; no symbol, constant,
  expression, string literal, assembly instruction, or build behavior changes.
- Both Pico personalities and the SDL-23 target still build, and generated VACI
  and VCFFA1 payload bytes remain identical.

## Execution plan

### Phase 1: Create a claim ledger and freeze behavior

Before editing each surface, list its behavioral claims beside the code that
proves them. Use `docs/architecture.md` as the stable contract and
`docs/current-state.md` for deviations, but prefer executable code when they
disagree.

Record these invariants before touching comments:

- reset vector and ROM-protection boundary for Neo1-23 and Neo1-50;
- keyboard and display register behavior at `$D010-$D013` and mirrors;
- MSC and VCFFA1 address decode, status, error, and interrupt behavior;
- W65C02 GPIO/latch/clock ordering;
- software CPU step and memory callback behavior;
- current Pico/SDL terminal and storage differences.

Deliverable: a short checklist in the working notes for each subsequent commit,
not a new permanent architecture document.

### Phase 2: Correct portable-core prerequisite comments

Files:

- `src/systems/neo1.h`
- `src/chips/neo1_cpu_backend.h`
- `src/chips/soft65C02cpu.h`
- `src/chips/wdc65C02cpu.h`

Replace the Neo6502-only and future-SDL framing with accurate present-tense
ownership. Document the physical and software CPU semantics without defending
the Reload-style macro surface as the desired architecture. Preserve the
derived-code license blocks exactly.

Commit boundary: `docs(core): correct machine and CPU contracts`

Gate: complete this phase before portable-core extraction begins.

### Phase 3: Correct 6502-visible storage comments

Files:

- `systems/neo1-pico/src/neo1_msc.[ch]`
- `systems/neo1-pico/src/neo1_cffa1.[ch]`
- `systems/neo1-sdl/src/neo1_storage_stub.c`
- storage decode comments in `src/systems/neo1.h`

Make one authoritative register contract per device. Describe synchronous
completion, DATA streaming, buffer reset/commit points, FatFs error mapping,
write protection, and lack of IRQ/NMI. Clearly label VACI's MSC interface as a
Neo1 extension and VCFFA1 as Replica 1 compatibility. Describe SDL's no-op file
commands and raw-image mapping as deviations, not equivalents.

Commit boundary: `docs(storage): document visible register contracts`

Gate: complete this phase before extracting either storage state machine.

### Phase 4: Correct runner, terminal, video, and input comments

Files:

- `systems/neo1-pico/src/neo1.c`
- `systems/neo1-pico/src/neo1_terminal.[ch]`
- `systems/neo1-pico/src/neo1_video.[ch]`
- `systems/neo1-pico/src/neo1_usb.[ch]`
- `systems/neo1-sdl/src/main.c`
- `systems/neo1-sdl/src/neo1_platform.h`
- `systems/neo1-sdl/src/neo1_platform_sdl.c`

Document actual runner ownership and the terminal/input differences already
identified in the portability baseline. Preserve comments that explain DVI
cross-core buffer publication, frame-boundary swaps, and physical timing.
Remove narration that claims the Pico runner is already thin or that the SDL
mixed interface is the future shared API.

Commit boundary: `docs(runners): clarify target ownership and seams`

Gate: complete the relevant file comments immediately before extracting the
shared terminal, so comments do not bless the duplicate SDL implementation.

### Phase 5: Audit build and retained support-source comments

Files:

- top-level and target `CMakeLists.txt` files
- `src/hid_app.c`
- `src/msc_app.c`
- generator scripts and their small README files

Keep comments that explain stable profile policy and external tool
requirements. Replace anecdotes and temporary labels with testable rationale.
Confirm through CMake source lists that `hid_app.c` and `msc_app.c` are not
linked, then mark their status without deleting or repurposing them.

Commit boundary: `docs(build): clarify active sources and generated artifacts`

This phase is useful but is not a blocker for the first portable-core
extraction.

### Phase 6: Audit 6502 payload comments separately

Files:

- `src/ram/neo1_vaci_v1.s`
- `src/ram/neo1_cffa1_m2_blockdrv.s`
- their linker configurations and generator documentation

Preserve routine-level explanations and add or correct inputs, outputs,
clobbers, inclusive/exclusive ranges, zero-page ownership, and protocol polling
where the code proves them. Reconcile VACI comments with the known BASIC-load
defect and verify claims such as page-2 safety. Replace historical milestone
labels in the VCFFA1 program with present capability or explicit limitation.

Commit boundary: one commit per payload, never combined with regenerated-byte
changes or behavior fixes.

This phase is intentionally independent of the portable C core and should not
delay its first extraction.

### Phase 7: Remediate issues exposed by the audit

Treat the defects recorded during phases 1-6 as separate behavioral work, not
as one cleanup commit. Preserve both targets at each completed checkpoint and
update `docs/current-state.md` only after evidence changes.

#### Deferral decision

VCFFA1 reliability work is explicitly deferred until after the next
portable-core checkpoint. This includes generic `.po` discovery, transactional
create/delete, existing-file metadata updates, filesystem bounds, load-range
validation, and bounded DRQ polling. VCFFA1 remains available as a Replica 1
compatibility feature, but it must not define or block the shared Apple-1 core;
VACI remains the preferred storage interface. Until remediation resumes, limit
normal VCFFA1 use to the verified catalog/load/block-inspection workflow and
use only disposable images for `W` or `D`.

The deferral is an ordering decision, not closure: defects 5 and 14-17 remain
open in `docs/current-state.md`. A newly observed regression in the verified
read-oriented workflow may be fixed sooner as a narrowly scoped preservation
change.

Proceed with the active work in this order:

1. **Completed 2026-08-23 — Make VACI storage tests repeatable.** The SDL host
   configuration now compiles the production Pico MSC register implementation
   against a test-only in-memory FatFs backend. The focused CTest covers
   missing/read-only media, invalid commands and seeks, directory filtering and
   indexed open, short reads and writes, delete, multi-sector write, and exact
   truncating overwrite. It deliberately does not claim execution coverage for
   the 6502 VACI payload.
2. **Repair VACI BASIC save/load as one format decision.** Move or preserve
   scratch state so `$004A-$00FF` is captured and restored faithfully, restore
   the missing `$0800-$0949` bytes, and prove that save then load reproduces all
   2230 bytes. Decide whether existing malformed snapshots need compatibility
   handling before changing the generated payload.
3. **Harden ordinary VACI transfers.** Close successful reads, reject or safely
   handle 64 KB length wrap and protected/overlapping ranges, accept only
   defined status values, and add a bounded failure path. Tighten the linker
   ceiling so payload growth cannot enter device or profile-ROM space.
4. **Resolve cross-target deviations found earlier in the pass.** Remove or
   retire unusable CPU backend value 2, make MSC decode enablement explicit,
   synchronize Pico terminal publication, and give the SDL software runner
   elapsed-time/cycle pacing. Keep architectural extraction separate from each
   observable behavior fix.

After the portable-core checkpoint, resume the deferred VCFFA1 track in this
order:

1. Add disposable-image tests with injected failures between ProDOS allocation,
   directory, index, overwrite, and delete writes.
2. Correct generic `.po` matching and make create/delete updates recoverable or
   correctly ordered, including propagation of every metadata-write failure.
3. Update existing-entry EOF and related fields, validate bitmap/directory
   bounds, and either support or explicitly reject volumes and storage types
   outside the utility's limits.
4. Check busy/error state while waiting for DRQ, provide a timeout result, and
   reject load destinations that wrap or overwrite utility, staging, I/O, or
   ROM regions.

Each payload behavior change must regenerate its checked-in header in the same
commit and prove the source/header pair agrees. Shared-machine changes require
focused tests, SDL and both Pico-profile builds, affected-address reporting, and
an exact Neo6502 smoke procedure. Do not claim physical validation until the
user supplies it.

Commit boundaries: one defect or tightly coupled invariant per commit. Never
combine generated payload changes, C architecture extraction, broad renaming,
and unrelated behavioral fixes.

### Phase 8: Consistency sweep and closeout

Search Neo1-owned active source for bare or stale markers such as `TODO: docs`,
`Phase`, `M0`, `M1`, `for now`, `currently`, `thin runner`, and `emulator`.
Review each match rather than mechanically deleting it. Confirm terminology
against `docs/architecture.md` and update `docs/current-state.md` only when the
audit discovers a new verified deviation.

Closeout evidence:

- `git diff --check`;
- staged-diff review proving comment-only changes;
- configure/build `neo1-pico-23-full` and `neo1-pico-50-full`;
- configure/build `neo1-sdl-23-full`;
- `python3 src/ram/build_vaci.py --check`;
- assemble the VCFFA1 payload before and after its comment pass and compare the
  resulting binary byte-for-byte;
- state that no physical hardware test is required for a comment-only diff. If
  any executable token changes, move that change out of this plan and apply the
  normal behavior-change validation, including hardware evidence where
  required.

## Recommended stopping point before portable-core work

Complete Phases 1 through 3 first. Then perform Phase 4 only for the terminal
surface selected as the first extraction. Begin portable-core implementation
after those gates rather than waiting for low-risk build-support and assembly
comment cleanup. This gives the architectural work truthful guideposts without
spending time polishing code that the extraction will soon move or replace.
