Legal GC/WII DSP IROM replacement (v0.3.1)
-------------------------------------------------------

- irom: When running from the ROM entrypoint, skip the bootucode_ax branch
  of the bootucode procedure. Since the ROM doesn't set any of the AX
  registers, it could cause bad DMA transfers and crashes.

ligfx
10/aug/2017

Legal GC/WII DSP IROM replacement (v0.3)
-------------------------------------------------------

- coef: Explicitly set 23 different values that are used by GBA UCode, and
  tweaked overall parameters to more closely match those 23 values.
- irom: Moved a few functions to their proper places, updated BootUCode to
  configure DMA transfers using AX registers as well as IX registers (the GBA
  UCode uses this to do two sequential transfers in one call), and added
  partial functions used by GBA UCode.

ligfx
2/june/2017

Legal GC/WII DSP IROM replacement (v0.2.1)
-------------------------------------------------------

- coef: 4-tap polyphase FIR filters
- irom: unchanged

Coefficients are roughly equivalent to those in the official DROM.
Improves resampling quality greatly over linear interpolation.
See generate_coefs.py for details.

stgn
29/june/2015

Legal GC/WII DSP IROM replacement (v0.2)
-------------------------------------------------------

- coef: crafted to use a linear interpolation when resampling (instead of
  having a real 4 TAP FIR filter)
- irom: added all the mixing functions, some functions not used by AX/Zelda are
  still missing

Should work with all AX, AXWii and Zelda UCode games. Card/IPL/GBA are most
likely still broken with it and require a real DSP ROM.

delroth
16/march/2013

Legal GC/WII DSP IROM replacement (v0.1)
-------------------------------------------------------
- coef: fake (zeroes)
- irom: reversed and rewrote ucode loading/reset part, everything else is missing

Good enough for Zelda ucode games (and maybe some AX too):
- WII: SMG 1/2, Pikmin 1/2 WII, Zelda TP WII, Donkey Kong Jungle Beat (WII), ... 
- GC: Mario Kart Double Dash, Luigi Mansion, Super Mario Sunshine, Pikmin 1/2, Zelda WW, Zelda TP, ...

Basically... If game is not using coef and irom mixing functions it will work ok.
Dolphin emulator will report wrong CRCs, but it will work ok with mentioned games.

LM
31/july/2011
