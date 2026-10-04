# Mouse Motion for an emulated Wii Remote

In the emulated Wii Remote configuration, open **Motion Simulation**, then enable
**Mouse Motion**. The mode is disabled by default and saved in controller profiles.
It currently uses the existing relative mouse inputs on Windows (DInput) and X11
(XInput2). macOS and Wayland backends are not supported by this mode.

Ordinary mouse movement controls the pointer. **Hold either side mouse button**
to move the Wii Remote: away from yourself for an upward backswing, toward
yourself for a downward stroke, and left/right for sideways swings. Faster motion
produces faster rotation and stronger acceleration. Release the side button to
return to pointing. This separates menu navigation from swings without detecting
which game is running.

**Motion Hold** is automatically bound to both side buttons (DInput Click 3/4,
X11 Click 8/9). **Recenter** is automatically bound to the middle mouse button and
**R**. Both bindings appear in the group and can be changed; existing profile
bindings are preserved. Hold Recenter to return to neutral. **Motion Sensitivity**
adjusts the travel needed for all swings. **Horizontal Sensitivity** and
**Vertical Sensitivity** independently multiply it for each axis. All three
default to 100%; higher values need less mouse movement. These settings affect
motion while Motion Hold is pressed, not pointer movement. They are saved in the
controller profile; the existing `Sensitivity` config key is preserved.
Mouse DPI affects sensitivity. The default remains 0.2 degrees
per mouse count: 400 counts for 80 degrees, or about 13 mm at 800 DPI.
No Swing, Tilt, or Shake direction bindings are required. Existing button and
pointer bindings remain usable. MotionPlus uses the existing **Attach MotionPlus**
setting, which is already enabled in the default configuration.

## Integration

- DInput already supplies raw, per-input-channel relative mouse counts. XInput2
  supplies relative axes scaled by eight; the adapter restores their count scale.
- A small control group in `WiimoteEmu.cpp` binds these inputs automatically when
  Dolphin refreshes devices. Internal `InputReference`s reuse input gating and
  device lifetime handling. Two ordinary controls provide the automatically
  assigned motion modifier and recenter bindings, using existing config/UI code.
- `StepDynamics()` advances a small `MouseMotionState` built on the existing
  `PositionalState` and `Common::Quaternion`, using `EmulateMouseMotion()` in
  `Dynamics.cpp`. Mouse counts determine rates on two fixed axes, smoothed over
  20 ms, with total speed capped at 20 radians/second. The pose is reconstructed
  from these axes rather than accumulating local rotations. Closed mouse paths
  therefore do not accumulate roll or change the meaning of left/right/up/down.
  Each axis is limited to 85 degrees so the hand cannot flip behind the user.
  Overflow is discarded at the limits, and reversing input responds immediately
  rather than first consuming excess travel or continuing the old direction.
- A 0.5-meter virtual arm converts the pose into position. Angular velocity and
  acceleration provide consistent tangential and centripetal acceleration.
  The usual Wii Remote path combines this with gravity and converts it to
  accelerometer reports. Device angular velocity is passed through the existing
  MotionPlus path. Gyro is calculated from the actual pose difference, including
  limits, recentering and transitions to pointing. No report, calibration, or
  MotionPlus protocol code changes are needed.
- IR and the motion sensors share the same pose. Outside a gesture it follows
  Dolphin's usual Point simulation without a second smoothing filter. Returning
  from a gesture is rate-limited, with gyro describing the return, so IR and gyro
  cannot disagree about the controller's orientation. Ordinary pointing does not
  generate the acceleration of a full arm swing. The Point target is frozen while
  the modifier is held. Other simulated motions still compose as before. Mouse
  pose and activation state are included in save states.
  The savestate version is incremented because the controller-state layout changed;
  older savestates are rejected instead of being read using the wrong layout.
- Input gating and a 250 ms gap check discard movement accumulated during focus
  loss or pauses while preserving the pose. DInput clears relative samples when
  mouse acquisition fails, rather than replaying the last movement.

A two-axis mouse cannot measure independent roll or three-dimensional translation;
this mode infers a consistent arm movement rather than reconstructing six degrees
of freedom. It has no game-specific detection or special cases.

## Validation

`MouseMotionTest` covers motion generation, fixed-axis direction, closed paths,
reversals at the limits, pointer/swing separation, and consistent gyro during
transitions. Integration tests also compare generated camera and accelerometer
reports with standard Dolphin pointing, check a stationary pointer for drift, and
verify that holding Motion Hold freezes the pointer target. These tests do not
establish that a particular game accepts a gesture.

For Wii Sports Resort Golf, retain normal button bindings, enable Mouse Motion,
and keep MotionPlus attached. Follow the game's instructions, including any
request to disconnect an extension. Check calibration at rest, then perform a
slow movement away from yourself, pause, and a faster stroke toward yourself,
while holding a side mouse button and the game's required button(s).
Verify an actual ball hit, different shot strengths, repeated swings, and pointer
use in menus.

The PR author manually tested the current Windows build in Wii Sports Resort Golf with Mouse Motion enabled and confirmed a complete mouse swing and a
successful ball hit. This passes the primary acceptance test; it does not
establish compatibility with every Wii game.
