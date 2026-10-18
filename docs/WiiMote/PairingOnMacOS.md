# Pairing a real Wii Remote on macOS

Dolphin's macOS HID backend can use a Wii Remote after macOS has paired it and
created its Bluetooth HID device. Pairing and selecting **Real Wii Remote** in
Dolphin are separate steps.

This guide covers the Mac's Bluetooth connection with **Emulate the Wii's
Bluetooth adapter** selected. For a USB adapter managed directly by Dolphin,
see [Bluetooth Passthrough](https://wiki.dolphin-emu.org/index.php?title=Bluetooth_Passthrough).

## First-time pairing

On macOS 12 or later, use the Dolphin project's
[WiimotePair utility](https://github.com/dolphin-emu/WiimotePair):

1. Quit Dolphin and other Wii Remote utilities while pairing.
2. Open WiimotePair and allow Bluetooth access when macOS asks for it.
3. Press the red **SYNC** button inside the Wii Remote's battery compartment.
   Use this button rather than 1+2 for WiimotePair's first-time pairing procedure.
4. Wait for WiimotePair to report successful pairing, then quit the utility.
5. Open Dolphin's **Controllers** settings. Select **Emulate the Wii's Bluetooth
   adapter** and set a Wii Remote slot to **Real Wii Remote**.
6. Enable **Continuous Scanning**, or click **Refresh**, and press a normal button
   on the remote, such as **A**. A connected remote should vibrate and select its
   player LED.

For later connections to the same Mac, open Dolphin and press a normal button.
Use SYNC again when establishing a new pairing, such as after moving the remote
between a Wii and the Mac.

A sensor bar is needed for infrared pointer input, but it is not needed to pair
the remote or connect it over Bluetooth.

## Troubleshooting

### Bluetooth permission

Review **System Settings → Privacy & Security → Bluetooth** if macOS denied
Bluetooth access to WiimotePair or Dolphin. Quit the affected app and reopen it
after allowing access.

Do not try arbitrary text PINs in the macOS Bluetooth settings. WiimotePair's
SYNC procedure supplies the Wii Remote's six-byte binary PIN.

### WiimotePair fails with `(os/kern) default set` on Tahoe

An original `Nintendo RVL-CNT-01` was observed to fail on macOS 26.5 before
WiimotePair received its PIN-request callback. The macOS Classic pairing agent
selected Just Works and accepted it without the required binary PIN. The
resulting Bluetooth status `0x1F` was displayed by older WiimotePair builds as an
unrelated Mach kernel error.

See the [WiimotePair issue](https://github.com/dolphin-emu/WiimotePair/issues/17)
and [proposed legacy PIN fix](https://github.com/dolphin-emu/WiimotePair/pull/20).
Check their status and the utility's available builds; an older download may not
contain the fix. WiimotePair's README includes source-build instructions.

A local prototype of that fix successfully paired an original RVL-CNT-01 on
macOS 26.5 (25F71), exposed its physical Bluetooth HID device, and connected it
in Dolphin 2609a. That result does not establish compatibility with other remote
models or all macOS releases.

### Pairing succeeds but Dolphin does not connect

Quit the pairing utility, confirm that the intended slot is set to **Real Wii
Remote**, and try **Refresh** or **Continuous Scanning** while pressing a normal
button. A successful pairing message alone does not show that Dolphin has opened
the remote.

See also the [controller configuration guide](https://dolphin-emu.org/docs/guides/configuring-controllers/).
