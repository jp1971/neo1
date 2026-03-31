# VCFFA1 V0 Baseline — Smoke Matrix

Date: 2026-03-22
Owner: Neo1 solo mainline
Purpose: freeze known-good VCFFA1 behavior before the next storage/runtime changes.

## Scope

This V0 gate validates that current VCFFA1 behavior remains stable as the reference baseline.

In scope:
- `1810R` mini-menu and catalog output
- load by index (`L`)
- write by name (`W`)
- delete by index (`D`)
- overwrite existing file path

Out of scope:
- new VCFFA1 feature work
- VACI feature implementation details
- broad UX redesign

## Pre-Run Conditions

- Firmware builds cleanly.
- Firmware flashes cleanly.
- USB storage mounted and contains known test files.
- WozMon reachable and responsive.

## Smoke Matrix

Mark each row PASS/FAIL and capture one short note.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| S1 | Enter `1810R` | VCFFA1 banner/menu appears; no hang | PASS | banner + sig/status OK printed |
| S2 | `C` catalog | header + entries render with blank line after header | PASS | all 4 entries listed with proper spacing |
| S3 | `L` valid index | prompts for addr; loads bytes; success message | PASS | indices 00-03 all load successfully; `00 SUCCESS` |
| S4 | `L` empty index | deterministic `LOAD ERR:EMPTY IDX` | PASS | index 99 returns exact error text |
| S5 | `W` overwrite existing by name | write succeeds and persists | DEFERRED | known issue, deferring to post-V0 |
| S6 | `D` valid index | entry removed; recatalog reflects deletion | PASS | deletion works; catalog reflects removal |
| S7 | `D` empty index | deterministic `DELETE ERR:EMPTY IDX` | PASS | empty slot returns exact error text |
| S8 | `Q` exit | returns cleanly to WozMon | PASS | returns to `\` prompt |

## Transcript Template

Paste one full monitor transcript from reset through at least S1-S8:

```
<reset>
\
@E000R
...
@1810R
...
<CATALOG / LOAD / WRITE / DELETE / QUIT interactions>
...
```

## Baseline Snapshot (Known from last validated checkpoint)

The following was already validated in-session before V0 execution:
- hybrid menu model active (`L`/`D` by index, `W` by filename)
- index parser/state regressions fixed
- catalog spacing polished (blank line after `CATALOG BLK 0002`)
- deterministic empty-slot errors:
  - `LOAD ERR:EMPTY IDX`
  - `DELETE ERR:EMPTY IDX`

V0 run result: **PASS (with write deferred)**

Regression fixed:
- MenuLoadByIndex was clobbering parsed index value via PrintCR (A register)
- Fixed by saving index before PrintCR call; also corrected RAM linker config (`$1800` vs `$0400`)
- Hardware-tested: all valid load indices now work correctly

## Baseline Decision

✅ PASS — proceed to V1 (`C100R` VACI read flow)

S1-S8 status:
- [x] S1-S4: Load path working
- [x] S6-S8: Delete and menu navigation working
- [⊗] S5: Write/overwrite deferred; root cause not yet diagnosed
- [→] Defer W path improvements until the next storage milestone work resumes

## Naming Convention Note

Doc naming and wording use **VCFFA1** (virtual CFFA1) as the feature name.
Code symbols currently remain `neo1_cffa1_*` for compatibility and to avoid churn.
A dedicated rename pass can be done later when active VCFFA1 development resumes.

### Deferred Rename Checklist (when VCFFA1 work resumes)

- Pick a single canonical prefix (`vcffa1_` vs `neo1_cffa1_`) and apply it consistently.
- Rename module files together in one pass (header, implementation, generated artifacts if any).
- Rename public symbols and compile-time flags in the same pass.
- Update include paths and any RAM/tool references that embed old names.
- Run full compile/flash/smoke matrix after rename before functional changes.
