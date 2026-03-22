# V0 Baseline — VCFFA1 Smoke Matrix

Date: 2026-03-22
Owner: Neo1 solo mainline
Purpose: freeze known-good VCFFA1 behavior before VACI implementation changes begin.

## Scope

This V0 gate validates that current VCFFA1 behavior remains stable while VACI work starts on `main`.

In scope:
- `1810R` mini-menu and catalog output
- load by index (`L`)
- write by name (`W`)
- delete by index (`D`)
- overwrite existing file path

Out of scope:
- new VCFFA1 feature work
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
| S1 | Enter `1810R` | VCFFA1 banner/menu appears; no hang | PENDING | |
| S2 | `C` catalog | header + entries render with blank line after header | PENDING | |
| S3 | `L` valid index | prompts for addr; loads bytes; success message | PENDING | |
| S4 | `L` empty index | deterministic `LOAD ERR:EMPTY IDX` | PENDING | |
| S5 | `W` overwrite existing by name | write succeeds and persists | PENDING | |
| S6 | `D` valid index | entry removed; recatalog reflects deletion | PENDING | |
| S7 | `D` empty index | deterministic `DELETE ERR:EMPTY IDX` | PENDING | |
| S8 | `Q` exit | returns cleanly to WozMon | PENDING | |

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

V0 run result: PENDING HARDWARE PASS

## Exit Decision

V0 is complete when all S1-S8 are PASS and one transcript is attached.

Decision:
- [ ] PASS — proceed to V1 (`C100R` VACI read flow)
- [ ] HOLD — fix regressions before VACI implementation
