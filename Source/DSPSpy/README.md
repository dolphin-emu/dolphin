# DSPSpy

DSPSpy is a homebrew tool for experimenting with the GameCube/Wii DSP.  It can also be used to dump the DSP ROMs.

## Building

DSPSpy is built using [devkitPPC](https://wiibrew.org/wiki/DevkitPPC); see the [devkitPro getting started page](https://devkitpro.org/wiki/Getting_Started) for more information.  DSPSpy also requires DSPTool to be built.

First, run DSPTool to generate `dsp_code.h`, for instance from `tests/dsp_test.ds`.  The following commands assume an x64 Windows setup running in the DSPSpy directory:

```
../../Binary/x64/DSPTool.exe -h dsp_code tests/dsp_test.ds
```

To use the ROM-dumping code, run this:

```
../../Binary/x64/DSPTool.exe -h dsp_code util/dump_roms.ds
```

DSPTool can also generate a header for multiple DSP programs at the same time.  First, create a file (in this example, it was named `file_list.txt`) with the following contents:

```
tests/dsp_test.ds
tests/mul_test.ds
tests/neg_test.ds
```

Then run:

```
../../Binary/x64/DSPTool.exe -h dsp_code -m file_list.txt
```

After `dsp_code.h` has been generated, simply run `make` to generate `dspspy_wii.dol`, which can be loaded by normal means.

## Dumping DSP ROMs

Build DSPSpy with `util/dump_roms.ds`.  When launched, DSPSpy will automatically create files `dsp_rom.bin` and `dsp_coef.bin` on the SD card (only SD cards are supported); DSPSpy can be exited immediately afterwards.