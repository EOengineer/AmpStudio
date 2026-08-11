# Champ 5F1 — physics-first model

White-box Fender Champ 5F1 (Volume only). Same discipline as the Tube Screamer:
**named parts → discrete network → small Newton islands → ports for drive/load.**

## Scope (v1 / v2)

| Layer | Status |
|-------|--------|
| V1A / Volume / V1B + NFB / 6V6 + OT | Implemented |
| Default load | Flat **8 Ω** via `cab::resolveLoadRlc` when chain end / unloaded |
| Cab Z(f) into OT secondary | Consumed when **Cab IR** follows (`SpeakerRlc` stamps) |
| Acoustic IR | Stays in Cab IR slot (not baked into amp) |
| Measured speaker Z tables | **Deferred** (`TODO(measured-z)`) |
| Nonlinear amp↔cab co-process | **Deferred** until port exchange proves insufficient |

This is intentionally **not** an Agoura / Fractal-complete cab interaction milestone.
Synthetic Z(f) presets feed the OT; measured tables and a joint solve wait.

## Signal path

```text
Input (1M + 68k + Miller LPF)
  → V1A 12AX7 (100k / 1.5k + 25µ bypass)     ChampTriodeStage
  → 0.02µ → Volume 1M                         CouplingHp + taper
  → V1B 12AX7 (100k / 1.5k, NFB 22k)          ChampTriodeStage
  → 0.02µ → 6V6 grid (220k leak)
  → 6V6 + OT → speaker Z                      ChampPowerStage
```

Nonlinear stages run inside `circuit::Oversampler` at **4×**.

## Key files

| File | Role |
|------|------|
| [`ChampComponents.h`](../Source/dsp/amps/champ/ChampComponents.h) | Named 5F1 parts + anchors |
| [`ChampTriodeStage.h`](../Source/dsp/amps/champ/ChampTriodeStage.h) | 12AX7 Newton island |
| [`ChampPowerStage.h`](../Source/dsp/amps/champ/ChampPowerStage.h) | 6V6 + AC OT + speaker RLC |
| [`ChampEngine.h`](../Source/dsp/amps/champ/ChampEngine.h) | Full path + `resolveLoadRlc` |
| [`Champ5F1.h`](../Source/dsp/amps/Champ5F1.h) | `Block` façade |
| [`TubeModel.h`](../Source/dsp/circuit/TubeModel.h) | Koren 12AX7 + beam 6V6 |
| [`SpeakerRlc.h`](../Source/dsp/cabs/SpeakerRlc.h) | Synthetic Z(f) presets |

## Verification

```bash
clang++ -std=c++17 -O2 -I Source tools/champ_verify_main.cpp -o tools/champ_verify
./tools/champ_verify
```

Checks: coupling corners, NFB ratio, OT n, tube idle currents, flat-8 / reactive Z Newton smoke, volume taper.

Debug plugin builds also `DBG` the report once from `Champ5F1::prepare`.

## Electrical ports

- **Zin** ≈ 1 MΩ (`getInputLoad`)
- **Zout** ≈ speaker nominal ohms (`getOutputPort`)
- **`loadContext`** → `cab::resolveLoadRlc` → OT secondary (not acoustic IR)
