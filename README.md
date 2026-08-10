# AmpStudio

JUCE audio plugin + standalone shell for tube amp / FX modeling experiments.

## What’s in this milestone

- AU / VST3 / Standalone (macOS)
- Fixed 6-slot signal chain with drag-reorder and nudge buttons
- Library tabs: **FX**, **Amps**, **Captures**
- Pass-through module stubs:
  - Tube Screamer (Drive / Tone / Level)
  - Champ 5F1 (Volume)
  - Neural Capture (Input / Output + placeholder load)
- Input trim (dB) + auto-calibrate to the modeling reference
- Input / output meters
- Master gain (dB)
- Electrical adjacency contract between chain slots (ports only; no Z DSP yet)

Modeling DSP is intentionally not implemented yet.

---

## Levels

AmpStudio treats **post-trim input level** as the modeling contract:

- **Reference:** `-18 dBFS` gated RMS during typical hard playing (open chords / aggressive strumming). White-box models and neural captures should be authored or trained assuming this level at the chain input.
- **Input Trim** (`±24 dB`) calibrates the guitar/interface to that reference. Use the **Calibrate** button, then play hard for ~4 seconds; the plugin measures gated RMS and sets trim so the input lands on `-18 dBFS`.
- **Module knobs** (Level / Volume / Capture Input–Output) are musical controls *after* the contract is met — not the guitar-to-guitar calibrator.
- **Master Gain** is output loudness only (dB).

Signal path: `Input → Input Trim → (meter) → Chain → Master Gain → (meter) → Output`.

---

## Electrical adjacency (impedance)

The Chain passes **electrical ports** between neighboring *active* slots (empty / Bypass slots are skipped). Models own the physics; Chain only discovers adjacency.

- **`ElectricalPort`** ([`Source/dsp/ElectricalPort.h`](Source/dsp/ElectricalPort.h)): resistive ohms now; optional `ImpedanceResponse` for frequency-dependent Z (cab speaker curves).
- **Defaults:** buffered source ≈ **100 Ω**, high-Z input / unloaded ≈ **1 MΩ**. Chain input is treated as an interface (buffered). Levels (−18 dBFS) stay a separate contract.
- **Per block:** `getOutputPort()` / `getInputLoad()` publish ports; Chain calls `setDriveContext` / `setLoadContext` before `process`.
- **FX → amp (e.g. Tube Screamer → Champ):** most of the boost is serial audio (level / mids / clip). Low-Z drive is available on the amp’s `driveContext` for a future input network.
- **Amp → cab:** a future `ModuleCategory::cab` module should publish speaker load Z(f) via `getInputLoad()`; the amp reads `loadContext` in the power section. No cab module yet — the API seam is in place.
- **Neural captures:** publish buffered ports and ignore contexts (loading is baked into the capture).

---

## Setup

### 1. Prerequisites

- **macOS** with **full Xcode** installed (not only Command Line Tools)
- **Git**

Point the active developer tools at Xcode (once per machine; persists until changed):

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
xcode-select -p
# expect: /Applications/Xcode.app/Contents/Developer
```

Confirm the toolchain works:

```bash
xcodebuild -version
```

### 2. Get the source (and JUCE)

JUCE lives in this repo as a **git submodule** at `libs/JUCE` (pinned to **8.0.8**).  
Module paths in `AmpStudio.jucer` are relative (`libs/JUCE/modules`), so you do **not** need a separate `/Applications/JUCE` install to **compile** the plugin.

**If you are cloning this repo for the first time** (new machine, CI, collaborator), pull the submodule in the same step:

```bash
git clone --recurse-submodules <repo-url>
cd AmpStudio
```

`--recurse-submodules` tells Git to also fetch the pinned JUCE commit into `libs/JUCE`. Without it, `libs/JUCE` will be empty and the build will fail.

**If you already cloned without submodules**, initialize them:

```bash
cd AmpStudio
git submodule update --init --recursive
```

**If you already have this repo locally** with `libs/JUCE` populated (e.g. you developed here before pushing), you do **not** need to re-clone. Just keep committing `.gitmodules` and the `libs/JUCE` gitlink with the rest of the project.

Verify the submodule is present:

```bash
ls libs/JUCE/modules/juce_core
git submodule status
# expect a commit hash and libs/JUCE (not a leading '-' which means not initialized)
```

### 3. Projucer (optional but useful)

You only need Projucer when you change `AmpStudio.jucer` or add/remove source files and need to regenerate the Xcode project.

Options:

- Use an existing Projucer binary (e.g. `/Applications/JUCE/Projucer.app`), **or**
- Build Projucer from the submodule: `libs/JUCE/extras/Projucer`

The **modules** used by the AmpStudio build still come from `libs/JUCE`, regardless of which Projucer binary you run.

### 4. Generate / refresh the Xcode project

After a fresh clone, or after editing the `.jucer` / file list:

```bash
# Example using a system Projucer install:
/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer --resave AmpStudio.jucer
```

This writes/updates `Builds/MacOSX/` (gitignored) and `JuceLibraryCode/` (gitignored).

If `Builds/MacOSX/AmpStudio.xcodeproj` already exists from a previous resave on this machine, you can skip this until the project definition changes.

### 5. Build

```bash
cd Builds/MacOSX
xcodebuild -project AmpStudio.xcodeproj \
  -scheme "AmpStudio - Standalone Plugin" \
  -configuration Debug \
  build
```

Or open the project in Xcode and run the **AmpStudio - Standalone Plugin** scheme:

```bash
open Builds/MacOSX/AmpStudio.xcodeproj
```

Other schemes: **AmpStudio - AU**, **AmpStudio - VST3**, **AmpStudio - All**.

### 6. Run

Standalone (Debug):

```bash
open Builds/MacOSX/build/Debug/AmpStudio.app
```

Debug plugin binaries (for DAW installs later) typically appear as:

- `Builds/MacOSX/build/Debug/AmpStudio.app` — Standalone
- `Builds/MacOSX/build/Debug/AmpStudio.component` — Audio Unit
- `Builds/MacOSX/build/Debug/AmpStudio.vst3` — VST3

---

## Usage (in the app)

1. Click a chain slot to select it
2. Double-click a library item (or use **Load into selected slot**)
3. Drag slots to reorder, or use `<` / `>`
4. Turn stub knobs — they store state but do not color the sound yet
5. Adjust **Input Trim** or press **Calibrate** (play hard ~4 s) so the input meter sits near the reference tick
6. **Master** gain controls output loudness in dB

---

## Repo layout notes

| Path | Role |
|------|------|
| `Source/` | App / plugin code |
| `AmpStudio.jucer` | Projucer project definition |
| `libs/JUCE` | JUCE submodule (pinned version) |
| `Builds/` | Generated Xcode project + binaries (**not committed**) |
| `JuceLibraryCode/` | Generated JUCE glue (**not committed**) |

Do not commit secrets, signing certs, or local `.env` files. Current `.gitignore` already excludes build products and generated JUCE glue.
