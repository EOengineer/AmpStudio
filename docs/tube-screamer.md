# Tube Screamer — physics-first model

Learning map from R.G. Keen’s [Technology of the Tube Screamer](http://www.geofex.com/article_folders/tstech/tsxtech.htm) to AmpStudio code.

## Method

| Region | Approach | Code |
|--------|----------|------|
| Shared R/C, diodes, Newton | Trapezoidal companions, Shockley I–V, dense NR | [`Source/dsp/circuit/`](../Source/dsp/circuit/) |
| Oversampled nonlinear island | 4× `juce::dsp::Oversampling` around clipper + post-LP | [`Oversampler.h`](../Source/dsp/circuit/Oversampler.h), [`TsEngine.h`](../Source/dsp/fx/ts/TsEngine.h) |
| Linear tone / level / output | Component-parameterized filters + load divider | [`TsToneLevelStage.h`](../Source/dsp/fx/ts/TsToneLevelStage.h) |
| Chain façade | `Block` + `ElectricalPort` | [`TubeScreamer.h`](../Source/dsp/fx/TubeScreamer.h) |

This is the same discipline we will reuse for Champ: **named parts → discrete network → small Newton island → ports for drive/load**. Not a waveshaper preset.

## Geofex block ↔ stage ↔ code

```text
Input buffer          → unity (Zin ≈ 510k published)     TsEngine / getInputLoad
Clipping amp          → MNA + Newton @ N·fs              TsClippingAmp
Post-clip LPF         → 1k + 0.22µ @ N·fs                OnePoleLpRc in TsEngine
Tone + Level          → shelf from 20k / 220 / 0.22µ     TsToneLevelStage
Output buffer         → series/shunt into loadContext    TsOutputBuffer
Bypass JFETs          → deferred (chain bypass)          —
```

| Geofex anchor | Expected | Where verified |
|---------------|----------|----------------|
| Zi 4.7k / 0.047µ corner | ~720 Hz | `TsGeofexVerify` |
| Drive HF gain | ~12 … ~118 | `idealHfGain` |
| Post-clip 1k / 0.22µ | ~723 Hz | `TsGeofexVerify` |
| Tone 220 / 0.22µ | ~3.2 kHz | `TsGeofexVerify` |
| 808 vs 9 into 1 MΩ | 0.990 / 0.995 | `TsOutputBuffer::dividerGain` |

## Oversampling / aliasing

Diode clipping is processed at **4×** base rate. Upsample → clipper → post-clip LPF → downsample → tone/level/output. Stock circuit HF limiting (51 pF, ~723 Hz LPF) is modeled as physics; it does **not** replace oversampling.

Latency: `TsEngine::getLatencySamples()` from the JUCE oversampler.

## Impedance

- **Zin** ≈ 510 kΩ (`getInputLoad`)
- **Zout** ≈ series output R (`getOutputPort`) — 100 Ω (808) or 470 Ω (9)
- **`loadContext`** sets the divider load on the output stage (amp input ~1 MΩ when unloaded)

## Mods (component-backed)

| Param | Values | Component change |
|-------|--------|------------------|
| Output | 808 / 9 | series/shunt R |
| Bass | stock / more | Zi C 0.047µ → 0.1µ |
| Diodes | Si/Si, asym, Ge/Si, LED | diode model params in the same MNA island |

## Verification

```bash
clang++ -std=c++17 -O2 -I Source tools/ts_geofex_verify_main.cpp -o tools/ts_geofex_verify
./tools/ts_geofex_verify
```

Debug plugin builds also `DBG` the report once from `TubeScreamer::prepare` via `TsVerifyBootstrap`.

## Champ reuse

Bring forward:

1. `circuit::Oversampler` around tube nonlinearities  
2. `circuit::NewtonSolver` / diode (later tube) device stamps  
3. `ElectricalPort` drive/load already on `Block`  
4. Named `ComponentSet` pattern per amp schematic  
