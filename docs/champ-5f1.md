# Champ 5F1 — physics-first model

White-box Fender Champ 5F1 (Volume on the primary strip; NFB Stock/Off in Deep settings). Same discipline as the Tube Screamer:
**named parts → discrete network → small Newton islands → ports for drive/load.**

Canonical print: [Fender Champ-Amp schematic K-EE / layout K-8E](https://cdn.shopify.com/s/files/1/0604/9615/0624/files/fender_champ_5f1.pdf?v=1758721069) (Hi jack path). Some K-EE drawings omit the V1A 25 µF cathode bypass; that is widely treated as a drafting error, and vintage amps usually have it — we keep the 25 µF.

## Scope (v1 / v2)

| Layer | Status |
|-------|--------|
| V1A / Volume / V1B + NFB / 6V6 + OT | Implemented (NFB Stock 22k; Off = lifted resistor) |
| Default load | Flat **8 Ω** when unloaded |
| Cab Z(f) into OT secondary | **Implemented** — `loadContext` → `cab::resolveLoadRlc` → OT/NFB |
| Acoustic IR | Stays in Cab IR slot (not baked into amp) |
| Measured speaker Z tables | **Deferred** (`TODO(measured-z)`) |
| Nonlinear amp↔cab co-process | **Deferred** until port exchange proves insufficient |

This is intentionally **not** an Agoura / Fractal-complete cab interaction milestone.
Synthetic Z(f) presets feed the OT; measured tables and a joint solve wait.

Stripped-down vs the full amp: Hi jack only, no 5Y3 sag, **base-rate** (no JUCE oversampling — that path muted in-host). Unloaded OT is flat 8 Ω; with a Cab IR in the next slot, the OT and NFB see the selected Z curve (synthetic RLC). NFB is **Stock** (22k) by default; Deep settings **Off** is the classic lifted-resistor mod. Not every future amp will expose this control.

## Named parts (K-EE)

| Stage | Parts |
|-------|--------|
| Input | 1 MΩ grid leak, 68 kΩ stopper |
| V1A | 100 kΩ plate, 1.5 kΩ + 25 µF cathode, 0.02 µF into 1 MΩ volume |
| V1B | 100 kΩ plate, 1.5 kΩ unbypassed cathode, 22 kΩ NFB from speaker |
| 6V6 | 0.02 µF coupling, 220 kΩ grid leak, 470 Ω + 25 µF cathode, ~5 kΩ : 8 Ω OT |

## Signal path

```text
Input (1M + 68k + Miller LPF)
  → V1A 12AX7 (100k / 1.5k + 25µ bypass)     ChampTriodeStage
  → 0.02µ → Volume 1M                         CouplingHp + taper
  → V1B 12AX7 (100k / 1.5k, NFB from speaker) ChampTriodeStage
  → 0.02µ → 6V6 grid (220k leak)
  → 6V6 + OT → speaker Z                      ChampPowerStage
                    └─ 22k NFB ───────────────┘  (Deep: Stock / Off)
                       Z = Cab IR curve, else flat 8 Ω
```

Speaker volts into V1B cathode are **inverted** (V1B and 6V6 each invert; raw OT secondary would be positive feedback). The NFB tap is **Re + Zmech** (voice-coil Le omitted) so discrete Le + a 1-sample delay cannot motorboat at Nyquist. Off disconnects that path (`gnfb = 0`) without resettling the island. Tube Newton always sees Re (the stable flat-8 path); motional Z is a linear filter on OT current. Speaker volts also get a lossy Le (`sL || Reddy`, eddy pole ~8 kHz) so the coil rise is audible without the 2L/T companion crackle.

Coupling caps are seeded at **equilibrium** after each triode settles (`vC = Vp_idle`) so idle plate DC is not dumped onto the next grid.

Tube islands run at **base sample rate**. 4× `circuit::Oversampler` is deferred: the same stages pass `champ_verify`, but JUCE half-band OS has zeroed this amp in-host. Re-enable OS only after the base-rate plugin path is audible. Debug builds `DBG` in/out peaks on the first few blocks.

## Key files

| File | Role |
|------|------|
| [`ChampComponents.h`](../Source/dsp/amps/champ/ChampComponents.h) | Named 5F1 parts + anchors |
| [`ChampTriodeStage.h`](../Source/dsp/amps/champ/ChampTriodeStage.h) | 12AX7 Newton island + CouplingHp |
| [`ChampPowerStage.h`](../Source/dsp/amps/champ/ChampPowerStage.h) | 6V6 + AC OT + speaker RLC |
| [`ChampEngine.h`](../Source/dsp/amps/champ/ChampEngine.h) | Base-rate path; OT load from `resolveLoadRlc` |
| [`Champ5F1.h`](../Source/dsp/amps/Champ5F1.h) | `Block` façade |
| [`TubeModel.h`](../Source/dsp/circuit/TubeModel.h) | Koren 12AX7 + beam 6V6 |
| [`SpeakerRlc.h`](../Source/dsp/cabs/SpeakerRlc.h) | Synthetic Z(f) presets |

## Verification

```bash
clang++ -std=c++17 -O2 -I Source tools/champ_verify_main.cpp -o tools/champ_verify
./tools/champ_verify
```

Checks: coupling corners, NFB ratio, OT n, solved 12AX7/6V6 idle, flat-8 / reactive Z Newton smoke, volume taper, an **end-to-end base-rate audio probe** with NFB Off (seeded coupling, all stages finite, non-zero AC RMS into resistive 8 Ω), **NFB Stock quieter than Off**, and the same NFB check into **Fender Dlx 1x12** and **Mesa 4x12** Z(f).

Debug plugin builds also `DBG` the report once from `Champ5F1::prepare`.

## Electrical ports

- **Zin** ≈ 1 MΩ (`getInputLoad`)
- **Zout** ≈ speaker nominal ohms (`getOutputPort`)
- **`loadContext`** → `cab::resolveLoadRlc` → OT secondary (not acoustic IR). Unloaded / high-Z → flat 8 Ω. Cab IR `SpeakerImpedance` → selected Z curve. RLC is cached so trap rebuilds happen only on change.

## Deep settings

Per-module, opt-in. Champ registers NFB (`Off` / `Stock` pill) as a deep spec; TS and Cab have none, so the Deep button is hidden. Sag is a later Deep control, not this milestone.
