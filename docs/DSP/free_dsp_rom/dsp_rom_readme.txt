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
