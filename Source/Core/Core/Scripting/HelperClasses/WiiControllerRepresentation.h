#ifndef WIIMOTE_ENUMS
#define WIIMOTE_ENUMS
#include "Common/CommonTypes.h"

namespace WiiControllerEnums
{
/*
struct AccelerationRepresentation
{
  u16 x;
  u16 y;
  u16 z;
};
*/
enum class AccelerationEnum
{
  X,
  Y,
  Z,
  Unknown
};

/*
struct IRRepresentation
{
  u16 x;
  u16 y;
};
*/

enum class IREnum
{
  X,
  Y,
  Unknown
};

/*
struct WiimoteRepresentation
{
  bool DPadLeft;
  bool DPadRight;
  bool DPadUp;
  bool DPadDown;
  bool A;
  bool B;
  bool Plus;
  bool Minus;
  bool One;
  bool Two;
  bool Home;
  bool has_accel;
  AccelerationRepresentation acceleration;
  IREnum ir[4];
};
*/

enum class WiimoteButtonsEnum
{
  DPadLeft,
  DPadRight,
  DPadUp,
  DPadDown,
  A,
  B,
  Plus,
  Minus,
  One,
  Two,
  Home,
  Acceleration,
  Unknown
};

/*
struct NunchuckRepresentation
{
  bool C;
  bool Z;
  u8 control_stick_x;
  u8 control_stick_y;
  bool has_accel;
  AccelerationRepresentation acceleration;
};
*/

enum class NunchuckButtonsEnum
{
  C,
  Z,
  ControlStickX,
  ControlStickY,
  Acceleration,
  Unknown
};

/*
struct ClassicControllerRepresentation
{
  bool Left;
  bool Right;
  bool Up;
  bool Down;
  bool A;
  bool B;
  bool X;
  bool Y;
  bool YL;
  bool ZR;
  bool Plus;
  bool Minus;
  bool Home;
  u8 L_Trigger;
  u8 R_Trigger;
  u8 Left_Stick_X;
  u8 Left_Stick_Y;
  u8 Right_Stick_X;
  u8 Right_Stick_Y;
};
*/

enum class ClassicControllerButtonsEnum
{
  DPadLeft,
  DPadRight,
  DPadUp,
  DPadDown,
  A,
  B,
  X,
  Y,
  YL,
  ZR,
  Plus,
  Minus,
  Home,
  TriggerL,
  TriggerR,
  LeftStickX,
  LeftStickY,
  RightStickX,
  RightStickY,
  Unknown
};

/*
struct WiiSlot
{
  bool is_wiimote_connected;
  bool has_core;
  bool has_ir;
  bool has_ext;
  bool has_nunhuck_attached;
  bool has_classic_controller_attached;
  WiimoteRepresentation wiimote;
  NunchuckRepresentation nunuchk;
  ClassicControllerRepresentation classic_controller;
};
*/
extern WiimoteButtonsEnum ParseWiimoteButton(const char* button_name);
extern const char* ConvertWiimoteButtonToString(WiimoteButtonsEnum button);

extern AccelerationEnum ParseAccelerationInput(const char* input_name);
extern const char* ConvertAccelerationInputToString(AccelerationEnum input);

extern IREnum ParseIRInput(const char* input_name);
extern const char* ConvertIRInputToString(IREnum input);

extern NunchuckButtonsEnum ParseNunchuckButton(const char* button_name);
extern const char* ConvertNunchuckButtonToString(NunchuckButtonsEnum button);

extern ClassicControllerButtonsEnum ParseClassicControllerButton(const char* button_name);
extern const char* ConvertClassicControllerButtonToString(ClassicControllerButtonsEnum button);
}  // namespace WiiControllerEnums
#endif
