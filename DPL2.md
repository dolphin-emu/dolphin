# Function Documentation

## Overview
Processes a linked list of **Parameter Blocks (PBs)**, each representing a single audio voice.  
Each voice can route audio to **3 busses** (Main, AuxA, AuxB), and each bus has **3 channels** (Left, Right, Surround).  
This yields **9 total output buffers**:

- Main.L, Main.R, Main.S
- AuxA.L, AuxA.R, AuxA.S
- AuxB.L, AuxB.R, AuxB.S

There can be up to **55 PBs** in the linked list, though typically only a few are active at any time.  
A silence is represented as a PB with all volumes set to **0**.

---

## Processing
For each PB, audio data is read, decoded, processed in **5 ms chunks**, and routed to one or more output buffers.  
Optional **Dolby Pro Logic II (DPL2) encoding** is applied after all voices have been mixed.

### PB List Processing
- Audio is processed in **5 ms frames at 32 kHz** (32 samples per ms).
- For each voice:
    - Updates are applied at the start of each millisecond.
    - Voice samples are processed and mixed to output buffers:
        - Sample decoding and resampling (if needed).
        - Applying mixer gains to route audio to the appropriate buffer (via `pb.mixer_control`).
        - Volume ramping and effects.
    - Buffer pointers advance after each ms of processing.
- After voices are processed, **DPL2 encoding** must be performed if it was required (`pb.mixer_control` set to `0x0010` or `0x0018`).
- Not all voices require DPL2 processing, so the **DPL2 processing** is skipped if it is not required.

---

## Parameter Block (PB) Fields
- `next_pb`: Pointer to next PB in the linked list (32-bit address).
- `running`: Flag indicating if voice is active.
- `is_stream`: Indicates if audio data is streaming or one-shot.
- `mixer_control`: Bitmask defining which busses/channels are active.
- `sample_format`: Audio format (4/8/16-bit PCM or ADPCM).
- `loop_addr`: Start address for looped playback.
- `end_addr`: End address of audio data.
- `cur_addr`: Current playback position.
- `audio_addr`: Buffer addresses and format metadata.
- `vol_env`: Volume envelope parameters (current, target, delta).
- `mixer`: Per-bus mixing parameters (volumes, gains, etc).
- `initial_time_delay`: Stereo positioning (ITD).
- `updates`: Number of parameter updates per millisecond.
    - `updates.num_updates`: Number of updates in the current frame.
    - `updates.data`: Address of update data in memory.
- `dpop`: State for per-channel volume ramping and effects.

---

## Dolby Pro Logic II
The **Dolby Pro Logic II (DPL2)** matrix is a **5:2:5 system**:  
It encodes 5 channels (Left, Right, Center, Rear Left, Rear Right) into 2 channels, and decodes them back to 5.

### Encoding requirements:
- Apply a **90° Phase Shift** (implemented using wideband quadrature all-pass networks; in DSP, `hilbert()` is a broadband approximation).
- Use the correct matrix:

|              | Left | Right | Center | Rear Left | Rear Right |
|--------------|------|-------|--------|-----------|------------|
| **Left Total**  | 1    | 0    | √½     | j·(√3/2)  | j·(½)       |
| **Right Total** | 0    | 1    | √½     | k·(½)     | k·(√3/2)    |

Where:
- `j` = +90° phase shift (imaginary unit `+1j`).
- `k` = –90° phase shift (imaginary unit `–1j`).

### Derived Center

```
C = 0.707 * (L + R)
```

### Expanded Equations
```
Left_Total  = 1.000*L + 0.707*C + (+0.866 · Q+90(RL)) + (+0.500 · Q+90(RR))
Right_Total = 1.000*R + 0.707*C + (-0.500 · Q-90(RL)) + (-0.866 · Q-90(RR))
```

Where:
Q+90(x) = signal shifted by +90° (Hilbert transform).
Q-90(x) = signal shifted by –90° (or -Q+90(x) since Hilbert is odd).

More info: [Dolby Pro Logic II matrix (Wikipedia)](https://en.wikipedia.org/wiki/Matrix_decoder#Dolby_Pro_Logic_II_matrix_(5:2:5))

---

## Game Usage

### Rogue Squadron 2 (GSWE64)
- Tested during **Tie Fighter Speaker Test** in **Stereo Mode** (DPL2 enabled).
- Each PB list has **2 PBs**.
- Mixer control behavior:
    - **0x0000 (20.6%)**: Standard stereo mixing (no DPL2).
    - **0x0008 (9.8%)**: Transition/ramping state.
    - **0x0010 (6.5%)**: AuxB bus active → surround/DPL2 processing.
    - **0x0018 (63.1%)**: AuxB bus active + DPL2 + volume ramping.

Additional notes:
- **Initial Time Delay (pb.ITD)** is only active in stereo mode (Main.L + Main.R), used in **X-Wing intro**.
- ITD is not applied during Tie Fighter Sound Test, regardless of bus selection.

**Example logs:**
```
mixer_ctrl:0x0000 | Main.L:7968 Main.R:288 Main.S:0 | AuxA.L:0 AuxA.R:0 AuxA.S:0 | AuxB.L:0 AuxB.R:0 AuxB.S:0 | ITD.offL: 0, ITD.offR: 32, ITD.targL: 0, ITD.targR: 32, ITD.On: 0
mixer_ctrl:0x0018 | Main.L:7968 Main.R:288 Main.S:0 | AuxA.L:0 AuxA.R:0 AuxA.S:0 | AuxB.L:0 AuxB.R:0 AuxB.S:0 | ITD.offL: 0, ITD.offR: 32, ITD.targL: 0, ITD.targR: 32, ITD.On: 0
mixer_ctrl:0x0018 | Main.L:7904 Main.R:192 Main.S:0 | AuxA.L:0 AuxA.R:0 AuxA.S:0 | AuxB.L:352 AuxB.R:192 AuxB.S:0 | ITD.offL: 0, ITD.offR: 32, ITD.targL: 0, ITD.targR: 32, ITD.On: 0
```
### Metroid Prime (GSWE64)
- In-game analysis after enabling **Surround Mode** (e.g., near a gas pipe leak).
- Uses **AuxA (L, R, S)** and **AuxB (L, R)**.

### Baten Kaitos (GKBEAF)
- **Main.Surround** and **AuxA.Surround** used when moving arrows in menus.
- **AuxB.L and AuxB.R** active when selecting **SURROUND** in sound options.

---

## Parameters
- **`pb_addr`**: Memory address of the first Parameter Buffer (PB) in the linked list.  



