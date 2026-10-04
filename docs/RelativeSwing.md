# Relative input for Swing

In the emulated Wii Remote configuration, open **Motion Simulation** and click
**Use Mouse Controlled Swing** in **Swing**. This assigns the direction bindings,
Motion Hold and Recenter and enables **Relative Input**. No separate Mouse Motion
group or manual direction binding is needed. Relative Input is off by default;
ordinary Swing profiles keep their existing position-based behavior.

Move the mouse normally to point. Hold either side mouse button to swing: away
from yourself moves upward, toward yourself moves downward, and left/right moves
sideways. Faster mouse movement produces faster rotation and stronger acceleration.
Release the modifier to return smoothly to pointing. Hold the middle mouse button
or R to recenter. Normal game button bindings and the existing MotionPlus
attachment setting still apply.

Motion Hold and Recenter are editable, ordinary bindings, and all six Swing
directions accept ordinary expressions. Relative Input works with other devices,
including gamepad axes and buttons: each input value represents a displacement
for that update. An empty Motion Hold binding allows continuous motion. The
mouse preset is a Qt convenience, not a restriction in controller emulation.
It requires relative mouse inputs, currently provided by Windows DInput and X11
XInput2. macOS and Wayland mouse presets are not supported.

**Motion Sensitivity**, **Horizontal Sensitivity** and **Vertical Sensitivity**
(default 100%) control the relative bindings, not the pointer or absolute Swing
mode. The default mouse conversion remains 0.2 degrees per count: 400 counts
for 80 degrees, about 13 mm at 800 DPI. Swing's **Distance** is the arm length;
**Angle** bounds rotation, capped at 85 degrees per axis to avoid inversion;
**Speed** caps movement and **Return Speed** controls return/recenter speed.
Forward/Backward bindings move the arm along its forward axis. The mouse preset
leaves these two bindings empty because the mouse supplies only two axes.

## Integration

- Relative mouse axes return counts divided by eight on both DInput and XInput2.
  Ordinary smoothed Axis and absolute Cursor inputs are unchanged. XInput2 uses
  the existing per-input-channel RelativeInputState, like DInput, so GUI polling
  does not consume an emulation displacement. The backends expose a consistent
  Mouse Side Button alias; emulation code never checks backend or button names.
- Qt's preset assigns public Swing bindings. Force exposes the raw displacements
  without the absolute position gate or unit-radius clamp, with generic axis
  sensitivity settings. It performs no device discovery or backend scaling.
- EmulateSwing selects displacement or the unchanged absolute-position model.
  Both use the existing MotionState, accelerometer and MotionPlus report paths.
  Relative input integrates two fixed bounded axes with 20 ms rate smoothing,
  discards overflow at the limits and reacts immediately to direction reversal.
  Gyro comes from the actual quaternion difference. Arm acceleration includes
  tangential and centripetal terms. No second Mouse Motion state or report path
  remains. Nunchuk's shared Swing uses the same orientation in relative mode.
- Wii Remote pointing and relative Swing share one pose, including the return
  after releasing the modifier. Ordinary pointing avoids full-arm acceleration;
  the Point target is frozen while Motion Hold is pressed. Input gating and a
  250 ms update gap discard stale displacement after focus loss or a pause.
- Profile persistence uses existing Swing config keys/settings. Prototype Mouse
  Motion profile settings are no longer read: use the new preset and reapply any
  customized sensitivity under Swing. The savestate version changes because
  MotionState's layout changes; use an in-game save, not an older savestate.

A two-axis mouse cannot independently measure roll or full 3D translation.
There is no game detection, report-format change or calibration/protocol change.

## Validation and manual regression

The previous prototype was manually tested by the PR author in Wii Sports Resort
Golf: a complete mouse swing successfully hit the ball. The automated prototype
trajectories now run through public Force direction bindings and EmulateSwing.
They cover motion strength/direction, reversals, fixed-axis bounds, closed paths,
gyro consistency, pointing stability and modifier transitions. Additional tests
cover analog bindings, forward/backward, settings persistence and default
absolute Swing returning to neutral. These tests cannot establish game acceptance
of the reworked build; it requires a fresh manual regression test.

For the reworked Windows build:

1. Start Wii Sports Resort Golf from an in-game save, with MotionPlus attached;
   follow the game's instructions about disconnecting an extension.
2. Select Use Mouse Controlled Swing; initially use default sensitivity (100%),
   Distance (50 cm), Angle (90 degrees), Speed (16 m/s) and Return Speed (2 m/s).
3. Check pointer/menu stability without a side button and calibration at rest.
4. Hold a side button and the game's required button(s), make a slow backswing
   away from yourself, pause and stroke faster toward yourself. Verify a ball hit.
5. Repeat with slower/faster strokes and left/right movements, including an
   immediate reversal. Verify direction, different shot strengths and no drift.
6. Release the side button and check return to pointing; check middle button/R
   recentering and a pause/focus change without an unexpected swing on return.
7. Save/reload the profile, then disable Relative Input and check normal controls.
