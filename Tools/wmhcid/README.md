# wmhcid

A small user-space Bluetooth host that lets Dolphin use real Wii Remotes through a
USB Bluetooth adapter, without the operating system's Bluetooth stack.

It drives a USB HCI dongle directly through libusb (minimal HCI + L2CAP), connects
to Wii Remotes and exposes each connected remote as a Unix socket in `/tmp/wmhci/`.
Dolphin's `wmhci` Wii Remote backend picks these sockets up and treats them like
any other real Wii Remote, so IR, motion, rumble, speaker and extensions are
handled by Dolphin as usual.

This was written for macOS, where third-party ("clone") Wii Remotes cannot be used
through the system Bluetooth stack. It should also build on Linux.

## Build

Requires libusb 1.0.

```
clang -O2 $(pkg-config --cflags --libs libusb-1.0) wmhcid.c -o wmhcid
```

## Usage

1. Plug in a supported adapter. It must not be in use by anything else
   (disable Bluetooth passthrough in Dolphin).
2. Run `./wmhcid`. Optionally pass known remotes as `AA:BB:CC:DD:EE:FF[=linkkeyhex]`.
3. Press SYNC on a Wii Remote (pairs it with the adapter), or any button on a remote
   that is already paired with the adapter.
4. In Dolphin, set the Wii Remote slots to "Real Wii Remote".

Set `WMHCI_DEBUG=1` to log HCI traffic and per-remote report statistics.

## Files

- Link keys of paired remotes: `~/.config/wmhci/keys`
- Sockets: `/tmp/wmhci/<address>.sock`, one per connected remote. Frames are one
  length byte followed by the raw HID report including its header byte
  (`0xA1` from the remote, `0xA2` to the remote).

## Supported adapters

Adapters that speak plain HCI over USB. The adapter is selected by USB ID:

- `0a12:0001` (CSR8510 A10, e.g. TP-Link UB400). This is the only one tested.
- `0a5c:21e8` (Broadcom BCM20702)
- `0b05:190e` (ASUS USB-BT500)
