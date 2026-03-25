# VACI Milestone Plan (Neo1-23 → Neo1-50 Ready)

Date: 2026-03-22
Completed: 2026-03-24
Status: Complete
Scope owner: Neo1 solo mainline (no branch required)

## 1) Branching Policy for This Phase

Because development is currently solo and VACI is intended as the next direct milestone, this phase can proceed on `main`.

Guardrails while staying on `main`:
- Keep commits small and reversible (one behavior change per commit).
- Keep `1810R` (VCFFA1 utility) bootable and usable after each VACI commit.
- Treat VCFFA1 as feature-frozen except for bugfixes and break/fix compatibility updates.
- Defer any repo-wide rename sweep for "Neo1-50" until VACI reaches a usable checkpoint.

## 2) Architectural Goal

Build a monitor-first Virtual Apple Cassette Interface (VACI) at `C100R` that reproduces Apple-1 ACI-style operator flow while using RP2040-backed USB storage.

VACI model:
- `R`/`W`/`Q` mini-menu after `C100R`
- `00-99` cassette index selection
- explicit memory address/range prompts
- display synthesized ACI command before transfer
- transfer begins on `CR`

## 3) Device-Level Contract (6502-visible)

VACI remains a memory-mapped peripheral from the 65C02 perspective.
Each operation is still bus cycles over addresses (`addr + R/W + data`).

Proposed contract for this phase:
- Reuse existing MSC backend registers where possible (`$D014`-`$D01A`) to avoid backend churn.
- Add only the minimum new control/status signals required for ACI-style read/write sessions.
- Keep all higher-level file list/index resolution in the RAM utility at `C100` entry path.

## 4) Milestones

### V0 — Freeze Gate + Baseline Capture (1 short session)
Objective: lock known-good starting state before VACI changes.

Deliverables:
- VCFFA1 smoke matrix run and logged:
  - catalog, load by index, write by name, delete by index, overwrite existing
- capture one known-good monitor transcript for `1810R`
- identify and record any intentionally deferred VCFFA1 items
- create/update `docs/vaci-v0-vcffa1-baseline.md` with the smoke matrix and transcript

Exit criteria:
- no VCFFA1 behavior regressions observed in the smoke matrix
- written baseline note available for future regression comparison

---

### V1 — Minimal VACI Read Flow (`R`) (first functional VACI)
Objective: load `.BIN` content by index into caller-selected RAM range, ACI-style.

Operator flow:
1. `C100R` prints `*` then menu (`R`/`W`/`Q`)
2. `R` shows indexed cassette list (`00-99` slots)
3. user selects two-digit index
4. prompt for destination start address
5. VACI prints synthesized command (example: `0300 . 0328R`)
6. user presses `CR` to execute transfer
7. return cleanly to WozMon on success

Implementation notes:
- Prefer reusing current catalog/index parsing routines from filer path.
- Keep transfer primitive byte-oriented; no auto-run.
- Preserve deterministic error text for empty index and range/parse errors.

Exit criteria:
- hardware-validated load from at least two different cassette files
- loaded bytes verify correctly in monitor memory view

---

### V2 — Minimal VACI Write Flow (`W`) (overwrite + create)
Objective: write a user-specified RAM range to selected or new cassette file.

Operator flow:
1. `W` shows indexed list and supports either:
   - select existing `00-99` slot (overwrite), or
   - `N` for new cassette filename entry (`.BIN` suffix required)
2. prompt for start address and byte length (hex)
3. display synthesized command (example: `0300 . 0328W`)
4. execute on `CR`
5. return to WozMon with clear status

Implementation notes:
- Keep write policy explicit and conservative.
- For overwrite, preserve existing index model behavior where possible.
- For `N`, normalize/validate filename and extension once, then call backend write path.

Exit criteria:
- overwrite path validated on hardware
- new file create path validated on hardware
- read-after-write verification from monitor confirms exact byte count

---

### V3 — UX Stabilization + Neo1-50 Readiness Checkpoint
Objective: freeze a historically-themed, robust VACI suitable for April/July demos.

Deliverables:
- prompt text and command echo polished for consistent Apple-1 feel
- deterministic, compact error strings
- concise usage note documenting `C100R` workflow
- test matrix for `R/W/Q`, empty slot, invalid index, short/long range

Exit criteria:
- repeated power-on tests pass without intervention
- operator can perform full load and save flow from monitor only
- system is ready to branch into Neo1-50 identity later (if desired)

Post-V3 transition target:
- Retire the legacy filer entry at `0400R` once VACI read/write flow fully covers its practical use.
- Keep any reused implementation pieces, but port/rename toward VACI ownership (`C100R` path).
- Remove or stub old `0400R` launch path only after one full VACI regression pass is clean.

## 5) Non-Goals During VACI Build

- No immediate split into separate Neo1-23 and Neo1-50 repos/branches.
- No broad file/symbol rename sweep tied to anniversary branding yet.
- No new ProDOS/CFFA1 features unless needed to keep existing behavior stable.

## 6) Practical Commit Cadence

Recommended commit rhythm on `main`:
- `vaci(v0): capture vcffa1 baseline smoke notes`
- `vaci(v1): add C100 menu and read-by-index flow`
- `vaci(v1): add ACI command echo + CR execute gate`
- `vaci(v2): add write overwrite path`
- `vaci(v2): add new-cassette create path`
- `vaci(v3): polish prompts/errors and add usage notes`

## 7) Immediate Next Actions

1. Execute `V0` smoke gate and record the baseline transcript.
2. Choose whether `C100R` image lives in `src/ram/` as a dedicated VACI artifact or evolves from current filer source directly.
3. Implement `V1` read-only flow end-to-end before touching write.
4. After `V3`, remove `0400R` entry and any remaining filer-only naming that no longer reflects architecture.

---

This plan intentionally keeps the machine monitor-first and 6502-visible, while preserving current VCFFA1 progress and deferring system divergence decisions until VACI is proven.