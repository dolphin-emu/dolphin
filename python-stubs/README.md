# Python scripting API stubs

Type stubs for Dolphin's embedded Python scripting API. They give editors
autocomplete and type-checking for the modules that TAS scripts call, and serve
as a concise human-readable reference for the API surface.

## Modules

Each `.pyi` under `dolphin/` mirrors the public surface of the matching
`Source/Core/Scripting/Python/Modules/<mod>module.cpp` and carries terse notes on
non-obvious behavior (override clear-on-frame, dict key names, value ranges):

| Stub | Import | Purpose |
|------|--------|---------|
| `dolphin/memory.pyi` | `from dolphin import memory` | read/write emulated memory |
| `dolphin/registers.pyi` | `from dolphin import registers` | PowerPC GPR/FPR access |
| `dolphin/savestate.pyi` | `from dolphin import savestate` | save/load state (slot, file, bytes) |
| `dolphin/event.pyi` | `from dolphin import event` | frame/breakpoint/savestate hooks |
| `dolphin/controller.pyi` | `from dolphin import controller` | override GC/Wii/Nunchuk input |
| `dolphin/gui.pyi` | `from dolphin import gui` | overlay + window widgets, OSD, drawing |
| `dolphin/debug.pyi` | `from dolphin import debug` | code/memory breakpoints |
| `dolphin/util.pyi` | `from dolphin import utils` | game id, dump toggles, screenshots |
| `dolphin/dtm.pyi` | `import dolphin_dtm as dtm` | inspect/edit the live DTM movie |

`from dolphin import event, memory, gui` resolves through `dolphin/__init__.pyi`.

## Editor setup

`pyrightconfig.json` at the repo root sets `stubPath = python-stubs` and
`pythonVersion = 3.8`. Open the repo root as the workspace (VS Code with
Pylight/Pylance, or any Pyright-based editor) and the stubs resolve automatically;
e.g. `gui.window(...).text(..., text_color=0xFF0000FF)` completes and unknown
kwargs are flagged.

## Maintenance

The stubs are hand-written against this fork's bindings, which have diverged from
upstream. When a module's `pycode[]` block or method table changes, update the
matching `.pyi`. Embedded Python is 3.8, so use `Optional[...]`, not `X | None`.
