# CE-2 BBD-on THD gate — evidence record — 2026-11-11

STATUS: **IN PROGRESS** (this file is updated after every step; sections marked
PENDING are not yet established). Worktree branch `agents/luthier/dsp/ce2-bbd-thd`,
base `ca14934e`.

Gate: `RefPedalCe2.bbdOnOriginalThdGate` (`Source/Tests/ReferencePedals_boss_ce2Tests.cpp:65`):
1 kHz sine, −12 dBFS, depth 0, BBD SIM on, noise 0; output THD (harmonics 2..9 over
the fundamental) must be 0.5..3 % at 48/44.1/88.2/96/192 kHz. Spec
`spec/reference-pedalboard/specs/pedals/boss-ce2.md` §9, tagged [E] (estimated).
The test, its fixture and its thresholds are not touched by this work.

## 1. What was already known (repo history)

- The model has no BBD transfer non-linearity: `BbdCore::process` only shapes above
  `clipOnset` 0.5 (CE-2), and a −12 dBFS sine peaks at 0.251. Clocked mode is a
  zero-order hold at the BBD clock (about 150.6 kHz at 3.4 ms).
- `4dd4adc2` added `x += even*(x^2-<x^2>) - odd*x^3` (even .02, odd .03): output THD
  0.90..1.08 %. `6e2cb7f7` reverted it; the stated reason was that the fit needed
  "about 9x the datasheet typical" (0.5 % typ at 0.78 Vrms scaled to about 0.1 % at
  0.18 Vrms). Recorded in `PEDALBOARD-2026-10-28-CONTINUATION.md`, owner decision #2.

## 2. Comb-notch analysis (analytic; measured numbers in section 4)

The gate is measured at the pedal OUTPUT = dry + 0.944·wet. With depth 0 the wet
path is a static 3.4 ms delay plus the phase of the 4th-order Butterworth pre (6.5 kHz)
and post (5.5 kHz) low-passes. Analog prototype at 1 kHz
(`python3` evaluation of the Butterworth pole pairs used in `BbdCore::setFilters`):

| Quantity | Value |
|---|---|
| Filter phase delay at 1 kHz | 140.2 µs (total wet delay 3.540 ms) |
| Wet/dry phase at 1 kHz | 165.5° lag past 3 full cycles, i.e. 14.5° from exact cancellation |
| Nearest comb minimum (filtered comb) | 988.7 Hz, gain 0.056 (pure 3.4 ms comb: 1029.4 Hz) |
| Output fundamental / input at 1 kHz | 0.2509 (−12.0 dB) |
| Wet harmonics reach the output at | 0.944 × post-LPF gain (0.9998 at 2 kHz, 0.996 at 3 kHz) |
| **Output THD / wet-path THD** | **0.944·\|W\| / \|1 + 0.944·W\| = 3.76** |

Harmonics are generated only in the wet path, the dry path adds none, and the
fundamental is the near-cancelled sum. So the output THD is about 3.8 times the
wet-path THD at this one frequency. The gate window 0.5..3 % at the output
corresponds to about **0.13..0.80 % in the wet path**.

Consequence for the earlier "9x" argument: the reverted fit's wet-path distortion at
−12 dBFS was HD2 = 0.02·0.251/2 = 0.251 % and HD3 = 0.03·0.251²/4 = 0.047 %
(about 0.255 % THD). The 0.90..1.08 % that was compared with the datasheet-scaled
0.1 % is the output-referred number. The like-for-like ratio was about 2.5x the
linearly scaled typical, not 9x.

## 3. Physical reference: MN3007 datasheet — what was and was not verified

Network: `WebFetch` of two datasheet mirrors failed with `getaddrinfo ENOTFOUND`
(`www.littlediode.com`, `ic-phoenix.com`: outside this session's allowed network
domains). No datasheet PDF or page was read in this session. Fetching was stopped
after the second identical failure. `WebSearch` works; the values below are
**search-result text only (tag [S], not page-read)** or values already recorded in
the repository (tag [R], secondary).

| Value | Tag | Attributed to |
|---|---|---|
| THD = 0.5 % typ at Vi = 0.78 Vrms ("Low distortion: THD = 0.5% typ. (Vi = 0.78Vrms)") | [S] | Panasonic MN3007 sheet copy, `http://kazojc.com/elementy_czynne/IC/MN3007.pdf` |
| Supply "VDD, VGG: −15, VDD+1 V"; insertion loss 0 dB typ | [S] | same |
| THD 0.5 % typ / 2.5 % max; input signal swing 1.5 Vrms; drain supply −14/−15 V, gate VDD+1 V | [S] | Xvive MN3007 listing, `https://www.cedist.com/products/integrated-circuit-xvive-mn3007-bbd-1024-stage` (one search summary printed the 2.5 % under a "min" label; treated as the max column) |
| Input signal swing (Vi) 1.5 Vrms | [S] | `https://www.diystompboxes.com/smfforum/index.php?topic=86680.0` |
| THD 0.5 % typ / 2.5 % max at Vi = 0.78 Vrms | [R] | `PEDALBOARD-2026-10-28-CONTINUATION.md` (forum-quoted; "the Panasonic PDF was not read") |
| Test conditions fcp = 40 kHz, fi = 1 kHz for the THD row | [S] | search summary of the kazojc.com copy |

NOT verified: the datasheet's THD-versus-input-level curve, any THD figure at a 9 V
supply (the CE-2 runs the MN3007 from 9 V; the sheet characterises −15 V), the exact
condition attached to the 1.5 Vrms input-swing row.

## 3a. Level mapping (how the model's dBFS relates to the sheet's test point)

Model scale: spec §3 fixes 0 dBFS = 1.0 V peak at the BBD input [E]; the E51 limiter
is linear to −6 dBFS (0.5) and has a ±1.0 ceiling. So −12 dBFS = 0.2512 V pk =
0.1776 Vrms. The sheet's test point is 0.78 Vrms (1.103 V pk) at VDD = −15 V. The
CE-2 runs the device from 9 V (spec §3: "9V single-supply bias 4.5V").

A single-ended charge-transfer chain distorts in even order at low level, so the
low-level law is HD2 = a2·A/2 (A = peak): distortion proportional to level. (Same
scaling the 2026-10-28 note used.) Three ways to place the sheet's point on the model
scale, each giving a "typical" a2 and the wet-path HD2 at −12 dBFS:

| Mapping | Level scale k | a2 = 2·0.005/(1.103·k) | Wet HD2 typ at −12 dBFS | max (5x) |
|---|---|---|---|---|
| A: sheet at face value (15 V figures used unchanged at 9 V) | 1.0 | 0.00907 | 0.114 % | 0.569 % |
| B1: supply-ratiometric (all device voltages scale with VDD, 9/15) | 0.6 | 0.01511 | 0.190 % | 0.949 % |
| B2: Roland "max input 0 dBm at 100 Hz" [S, spec §1] taken as the 9 V equivalent of the sheet's 1.5 Vrms swing [S] | 0.517 | 0.01754 | 0.220 % | 1.10 % |

Mapping A is a lower bound: a 9 V supply can only reduce headroom. B1 is the
first-order physical estimate and the least distortion the 9 V argument supports
(device thresholds do not scale with the supply, so the real k is at most 0.6).
B2 rests on two unverified [S] figures and is listed only as a cross-check.

## 4. Measurements

All C++ numbers come from a scratch probe test (not committed) run through
`ltest.sh` on this worktree: same coherent fixture as the gate (2 s, last second,
harmonics 2..9). The wet path is taken as (output − dry)/0.944 with the dry path
rebuilt from the 15 Hz input high-pass; the rebuilt sum matches the pedal output
exactly (error 0.0). A Python port of the wet chain (`port.py`, scratch) was used
for design exploration and agrees with the C++ baseline to four digits.

### 4a. Baseline (unmodified model at `ca14934e`), BBD SIM on

Gate before: `ltest.sh /home/user/lw/ce2 RefPedalCe2` = 1 of 13 tests failed (5 of 637 checks):
0.0017 / 0.0097 / 0.0075 / 0.0008 / 0.0003 % at 48 / 44.1 / 88.2 / 96 / 192 kHz.

| Level | Rate | Output THD % | Wet THD % | Output fundamental / input | Wet/dry magnitude, phase | Output fundamental / wet fundamental | B = output THD / wet THD |
|---|---|---|---|---|---|---|---|
| −12 dBFS | 48 kHz | 0.001711 | 0.000416 | 0.22911 | 0.99882, 166.877° | 0.22941 | 4.1152 |
| −12 dBFS | 44.1 kHz | 0.009750 | 0.002285 | 0.22095 | 0.99873, 167.379° | 0.22126 | 4.2668 |
| −12 dBFS | 88.2 kHz | 0.007500 | 0.002052 | 0.25818 | 0.99949, 165.096° | 0.25834 | 3.6543 |
| −12 dBFS | 96 kHz | 0.000795 | 0.000219 | 0.26049 | 0.99953, 164.955° | 0.26064 | 3.6220 |
| −12 dBFS | 192 kHz | 0.000312 | 0.000089 | 0.26827 | 0.99984, 164.480° | 0.26834 | 3.5182 |
| −3 dBFS | 48 kHz | 2.328432 | 0.572222 | 0.23009 | 0.99186, 166.877° | 0.23201 | 4.0691 |
| −3 dBFS | 44.1 kHz | 2.418094 | 0.573479 | 0.22203 | 0.99177, 167.379° | 0.22389 | 4.2165 |
| −3 dBFS | 88.2 kHz | 2.089046 | 0.577160 | 0.25884 | 0.99252, 165.096° | 0.26082 | 3.6195 |
| −3 dBFS | 96 kHz | 2.066597 | 0.575977 | 0.26113 | 0.99256, 164.955° | 0.26312 | 3.5880 |
| −3 dBFS | 192 kHz | 2.014987 | 0.577962 | 0.26883 | 0.99288, 164.480° | 0.27079 | 3.4864 |

The tiny baseline "THD" is clock-hold intermodulation landing on harmonic bins, not
a transfer non-linearity. B = 0.944 / (output fundamental / wet fundamental).

**Wet-path THD the output gate actually demands** (0.5..3 % divided by B):

| Rate | Wet THD for 0.5 % at the output | Wet THD for 3 % at the output |
|---|---|---|
| 48 kHz | 0.1215 % | 0.729 % |
| 44.1 kHz | 0.1172 % | 0.703 % |
| 88.2 kHz | 0.1368 % | 0.821 % |
| 96 kHz | 0.1380 % | 0.828 % |
| 192 kHz | 0.1421 % | 0.853 % |

All five rates together: **0.142 % ≤ wet THD ≤ 0.703 %** at −12 dBFS.

BBD SIM off (unchanged reference): −12 dBFS 0.000000 % at all rates; −3 dBFS
2.583 / 2.683 / 2.268 / 2.250 / 2.179 % (48 / 44.1 / 88.2 / 96 / 192 kHz).

### 4b. Candidates (Python port; C++ after-numbers follow in 4c)

Output THD at −12 dBFS, 48 / 44.1 / 88.2 / 96 / 192 kHz:

| a2 | Mapping | Output THD % | Gate |
|---|---|---|---|
| 0.00907 | A typical | 0.468 / 0.482 / 0.417 / 0.411 / 0.400 | below the floor at all rates |
| 0.01511 | B1 typical | 0.779 / 0.805 / 0.694 / 0.686 / 0.667 | inside |

Companion gate at −3 dBFS (BBD on, 2..6 %, h3 > h2), a2 = 0.01511, by where the
quadratic sits relative to the E51 limiter (h3/h2 at the output):

| Composition | THD % | h3/h2 |
|---|---|---|
| after the limiter, c + a2·c² | 2.72..3.27 | 0.998..1.004 (fails at 48 kHz) |
| inside the limiter, clip(v + a2·v²) | 2.60..3.12 | 1.124..1.130 |
| inside the limiter, clip(v + a2·clip(v)²) | 2.58..3.10 | 1.152..1.158 |

PENDING: implementation, C++ after-numbers (4c), CPU, decision text.
