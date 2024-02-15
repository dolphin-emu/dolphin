// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCKeyboardEmu.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/GCKeyboard.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/KeyboardStatus.h"

static const u16 keys0_bitmasks[] = {KEYMASK_HOME,       KEYMASK_END, KEYMASK_PGUP, KEYMASK_PGDN,
                                     KEYMASK_SCROLLLOCK, KEYMASK_A,   KEYMASK_B,    KEYMASK_C,
                                     KEYMASK_D,          KEYMASK_E,   KEYMASK_F,    KEYMASK_G,
                                     KEYMASK_H,          KEYMASK_I,   KEYMASK_J,    KEYMASK_K};
static const u16 keys1_bitmasks[] = {
    KEYMASK_L, KEYMASK_M, KEYMASK_N, KEYMASK_O, KEYMASK_P, KEYMASK_Q, KEYMASK_R, KEYMASK_S,
    KEYMASK_T, KEYMASK_U, KEYMASK_V, KEYMASK_W, KEYMASK_X, KEYMASK_Y, KEYMASK_Z, KEYMASK_1};
static const u16 keys2_bitmasks[] = {
    KEYMASK_2,          KEYMASK_3,           KEYMASK_4,     KEYMASK_5,
    KEYMASK_6,          KEYMASK_7,           KEYMASK_8,     KEYMASK_9,
    KEYMASK_0,          KEYMASK_MINUS,       KEYMASK_PLUS,  KEYMASK_PRINTSCR,
    KEYMASK_BRACE_OPEN, KEYMASK_BRACE_CLOSE, KEYMASK_COLON, KEYMASK_QUOTE};
static const u16 keys3_bitmasks[] = {
    KEYMASK_HASH, KEYMASK_COMMA, KEYMASK_PERIOD, KEYMASK_QUESTIONMARK, KEYMASK_INTERNATIONAL1,
    KEYMASK_F1,   KEYMASK_F2,    KEYMASK_F3,     KEYMASK_F4,           KEYMASK_F5,
    KEYMASK_F6,   KEYMASK_F7,    KEYMASK_F8,     KEYMASK_F9,           KEYMASK_F10,
    KEYMASK_F11};
static const u16 keys4_bitmasks[] = {
    KEYMASK_F12,         KEYMASK_ESC,        KEYMASK_INSERT,       KEYMASK_DELETE,
    KEYMASK_TILDE,       KEYMASK_BACKSPACE,  KEYMASK_TAB,          KEYMASK_CAPSLOCK,
    KEYMASK_LEFTSHIFT,   KEYMASK_RIGHTSHIFT, KEYMASK_LEFTCONTROL,  KEYMASK_RIGHTALT,
    KEYMASK_LEFTWINDOWS, KEYMASK_SPACE,      KEYMASK_RIGHTWINDOWS, KEYMASK_MENU};
static const u16 keys5_bitmasks[] = {KEYMASK_LEFTARROW, KEYMASK_DOWNARROW, KEYMASK_UPARROW,
                                     KEYMASK_RIGHTARROW, KEYMASK_ENTER};

static const char* const named_keys0[] = {"HOME", "END", "PGUP", "PGDN", "SCR LK", "A", "B", "C",
                                          "D",    "E",   "F",    "G",    "H",      "I", "J", "K"};
static const char* const named_keys1[] = {"L", "M", "N", "O", "P", "Q", "R", "S",
                                          "T", "U", "V", "W", "X", "Y", "Z", "1"};
static const char* const named_keys2[] = {"2", "3", "4", "5",      "6", "7", "8",      "9",
                                          "0", "-", "`", "PRT SC", "'", "[", "EQUALS", "*"};
static const char* const named_keys3[] = {"]",  ",",  ".",  "/",  "\\", "F1", "F2",  "F3",
                                          "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11"};
static const char* const named_keys4[] = {
    "F12",     "ESC",     "INSERT", "DELETE", ";",     "BACKSPACE", "TAB",   "CAPS LOCK",
    "L SHIFT", "R SHIFT", "L CTRL", "R ALT",  "L WIN", "SPACE",     "R WIN", "MENU"};
static const char* const named_keys5[] = {"LEFT", "DOWN", "UP", "RIGHT", "ENTER"};

GCKeyboard::GCKeyboard(const unsigned int index) : m_index(index)
{
  using Translatability = ControllerEmu::Translatability;

  // buttons
  groups.emplace_back(m_keys0x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys0)
    m_keys0x->AddInput(Translatability::DoNotTranslate, key);

  groups.emplace_back(m_keys1x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys1)
    m_keys1x->AddInput(Translatability::DoNotTranslate, key);

  groups.emplace_back(m_keys2x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys2)
    m_keys2x->AddInput(Translatability::DoNotTranslate, key);

  groups.emplace_back(m_keys3x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys3)
    m_keys3x->AddInput(Translatability::DoNotTranslate, key);

  groups.emplace_back(m_keys4x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys4)
    m_keys4x->AddInput(Translatability::DoNotTranslate, key);

  groups.emplace_back(m_keys5x = new ControllerEmu::Buttons(_trans("Keys")));
  for (const char* key : named_keys5)
    m_keys5x->AddInput(Translatability::DoNotTranslate, key);
}

std::string GCKeyboard::GetName() const
{
  return std::string("GCKeyboard") + char('1' + m_index);
}

InputConfig* GCKeyboard::GetConfig() const
{
  return Keyboard::GetConfig();
}

ControllerEmu::ControlGroup* GCKeyboard::GetGroup(KeyboardGroup group)
{
  switch (group)
  {
  case KeyboardGroup::Kb0x:
    return m_keys0x;
  case KeyboardGroup::Kb1x:
    return m_keys1x;
  case KeyboardGroup::Kb2x:
    return m_keys2x;
  case KeyboardGroup::Kb3x:
    return m_keys3x;
  case KeyboardGroup::Kb4x:
    return m_keys4x;
  case KeyboardGroup::Kb5x:
    return m_keys5x;
  default:
    return nullptr;
  }
}

KeyboardStatus GCKeyboard::GetInput() const
{
  const auto lock = GetStateLock();

  KeyboardStatus kb = {};

  m_keys0x->GetState(&kb.key0x, keys0_bitmasks);
  m_keys1x->GetState(&kb.key1x, keys1_bitmasks);
  m_keys2x->GetState(&kb.key2x, keys2_bitmasks);
  m_keys3x->GetState(&kb.key3x, keys3_bitmasks);
  m_keys4x->GetState(&kb.key4x, keys4_bitmasks);
  m_keys5x->GetState(&kb.key5x, keys5_bitmasks);

  return kb;
}

void GCKeyboard::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

  // Buttons
  m_keys0x->SetControlExpression(5, "A");
  m_keys0x->SetControlExpression(6, "B");
  m_keys0x->SetControlExpression(7, "C");
  m_keys0x->SetControlExpression(8, "D");
  m_keys0x->SetControlExpression(9, "E");
  m_keys0x->SetControlExpression(10, "F");
  m_keys0x->SetControlExpression(11, "G");
  m_keys0x->SetControlExpression(12, "H");
  m_keys0x->SetControlExpression(13, "I");
  m_keys0x->SetControlExpression(14, "J");
  m_keys0x->SetControlExpression(15, "K");
  m_keys1x->SetControlExpression(0, "L");
  m_keys1x->SetControlExpression(1, "M");
  m_keys1x->SetControlExpression(2, "N");
  m_keys1x->SetControlExpression(3, "O");
  m_keys1x->SetControlExpression(4, "P");
  m_keys1x->SetControlExpression(5, "Q");
  m_keys1x->SetControlExpression(6, "R");
  m_keys1x->SetControlExpression(7, "S");
  m_keys1x->SetControlExpression(8, "T");
  m_keys1x->SetControlExpression(9, "U");
  m_keys1x->SetControlExpression(10, "V");
  m_keys1x->SetControlExpression(11, "W");
  m_keys1x->SetControlExpression(12, "X");
  m_keys1x->SetControlExpression(13, "Y");
  m_keys1x->SetControlExpression(14, "Z");

  m_keys1x->SetControlExpression(15, "`1`");
  m_keys2x->SetControlExpression(0, "`2`");
  m_keys2x->SetControlExpression(1, "`3`");
  m_keys2x->SetControlExpression(2, "`4`");
  m_keys2x->SetControlExpression(3, "`5`");
  m_keys2x->SetControlExpression(4, "`6`");
  m_keys2x->SetControlExpression(5, "`7`");
  m_keys2x->SetControlExpression(6, "`8`");
  m_keys2x->SetControlExpression(7, "`9`");
  m_keys2x->SetControlExpression(8, "`0`");

  m_keys3x->SetControlExpression(5, "F1");
  m_keys3x->SetControlExpression(6, "F2");
  m_keys3x->SetControlExpression(7, "F3");
  m_keys3x->SetControlExpression(8, "F4");
  m_keys3x->SetControlExpression(9, "F5");
  m_keys3x->SetControlExpression(10, "F6");
  m_keys3x->SetControlExpression(11, "F7");
  m_keys3x->SetControlExpression(12, "F8");
  m_keys3x->SetControlExpression(13, "F9");
  m_keys3x->SetControlExpression(14, "F10");
  m_keys3x->SetControlExpression(15, "F11");
  m_keys4x->SetControlExpression(0, "F12");

#ifdef _WIN32
  m_keys0x->SetControlExpression(0, "HOME");
  m_keys0x->SetControlExpression(1, "END");
  m_keys0x->SetControlExpression(2, "PRIOR");
  m_keys0x->SetControlExpression(3, "NEXT");
  m_keys0x->SetControlExpression(4, "SCROLL");

  m_keys2x->SetControlExpression(9, "MINUS");
  m_keys2x->SetControlExpression(10, "GRAVE");
  m_keys2x->SetControlExpression(11, "SYSRQ");
  m_keys2x->SetControlExpression(12, "APOSTROPHE");
  m_keys2x->SetControlExpression(13, "LBRACKET");
  m_keys2x->SetControlExpression(14, "EQUALS");
  m_keys2x->SetControlExpression(15, "MULTIPLY");
  m_keys3x->SetControlExpression(0, "RBRACKET");
  m_keys3x->SetControlExpression(1, "COMMA");
  m_keys3x->SetControlExpression(2, "PERIOD");
  m_keys3x->SetControlExpression(3, "SLASH");
  m_keys3x->SetControlExpression(4, "BACKSLASH");

  m_keys4x->SetControlExpression(1, "ESCAPE");
  m_keys4x->SetControlExpression(2, "INSERT");
  m_keys4x->SetControlExpression(3, "DELETE");
  m_keys4x->SetControlExpression(4, "SEMICOLON");
  m_keys4x->SetControlExpression(5, "BACK");
  m_keys4x->SetControlExpression(6, "TAB");
  m_keys4x->SetControlExpression(7, "CAPITAL");
  m_keys4x->SetControlExpression(8, "LSHIFT");
  m_keys4x->SetControlExpression(9, "RSHIFT");
  m_keys4x->SetControlExpression(10, "LCONTROL");
  m_keys4x->SetControlExpression(11, "RMENU");
  m_keys4x->SetControlExpression(12, "LWIN");
  m_keys4x->SetControlExpression(13, "SPACE");
  m_keys4x->SetControlExpression(14, "RWIN");
  m_keys4x->SetControlExpression(15, "MENU");

  m_keys5x->SetControlExpression(0, "LEFT");
  m_keys5x->SetControlExpression(1, "DOWN");
  m_keys5x->SetControlExpression(2, "UP");
  m_keys5x->SetControlExpression(3, "RIGHT");
  m_keys5x->SetControlExpression(4, "RETURN");
#elif __APPLE__
  m_keys0x->SetControlExpression(0, "Home");
  m_keys0x->SetControlExpression(1, "End");
  m_keys0x->SetControlExpression(2, "`Page Up`");
  m_keys0x->SetControlExpression(3, "`Page Down`");
  m_keys0x->SetControlExpression(4, "");  // Scroll lock

  m_keys2x->SetControlExpression(9, "`-`");
  m_keys2x->SetControlExpression(10, "Paragraph");
  m_keys2x->SetControlExpression(11, "");  // Print Scr
  m_keys2x->SetControlExpression(12, "`'`");
  m_keys2x->SetControlExpression(13, "`[`");
  m_keys2x->SetControlExpression(14, "`=`");
  m_keys2x->SetControlExpression(15, "`Keypad *`");
  m_keys3x->SetControlExpression(0, "`]`");
  m_keys3x->SetControlExpression(1, "`,`");
  m_keys3x->SetControlExpression(2, "`.`");
  m_keys3x->SetControlExpression(3, "`/`");
  m_keys3x->SetControlExpression(4, "`\\`");

  m_keys4x->SetControlExpression(1, "Escape");
  m_keys4x->SetControlExpression(2, "Insert");
  m_keys4x->SetControlExpression(3, "Delete");
  m_keys4x->SetControlExpression(4, "`;`");
  m_keys4x->SetControlExpression(5, "Backspace");
  m_keys4x->SetControlExpression(6, "Tab");
  m_keys4x->SetControlExpression(7, "`Caps Lock`");
  m_keys4x->SetControlExpression(8, "`Left Shift`");
  m_keys4x->SetControlExpression(9, "`Right Shift`");
  m_keys4x->SetControlExpression(10, "`Left Control`");
  m_keys4x->SetControlExpression(11, "`Right Alt`");
  m_keys4x->SetControlExpression(12, "`Left Command`");
  m_keys4x->SetControlExpression(13, "Space");
  m_keys4x->SetControlExpression(14, "`Right Command`");
  m_keys4x->SetControlExpression(15, "");  // Menu

  m_keys5x->SetControlExpression(0, "`Left Arrow`");
  m_keys5x->SetControlExpression(1, "`Down Arrow`");
  m_keys5x->SetControlExpression(2, "`Up Arrow`");
  m_keys5x->SetControlExpression(3, "`Right Arrow`");
  m_keys5x->SetControlExpression(4, "Return");
#elif ANDROID
  m_keys0x->SetControlExpression(0, "`Move Home`");
  m_keys0x->SetControlExpression(1, "`Move End`");
  m_keys0x->SetControlExpression(2, "`Page Up`");
  m_keys0x->SetControlExpression(3, "`Page Down`");
  m_keys0x->SetControlExpression(4, "`Scroll Lock`");

  m_keys2x->SetControlExpression(9, "`Minus`");
  m_keys2x->SetControlExpression(10, "`Grave`");
  m_keys2x->SetControlExpression(11, "`PrtSc SysRq`");
  m_keys2x->SetControlExpression(12, "`Apostrophe`");
  m_keys2x->SetControlExpression(13, "`Left Bracket`");
  m_keys2x->SetControlExpression(14, "``Equals``");
  m_keys2x->SetControlExpression(15, "`Numpad Multiply`");
  m_keys3x->SetControlExpression(0, "`Right Bracket`");
  m_keys3x->SetControlExpression(1, "`Comma`");
  m_keys3x->SetControlExpression(2, "`Period`");
  m_keys3x->SetControlExpression(3, "`Slash`");
  m_keys3x->SetControlExpression(4, "`Backslash`");

  m_keys4x->SetControlExpression(1, "`Escape`");
  m_keys4x->SetControlExpression(2, "`Insert`");
  m_keys4x->SetControlExpression(3, "`Delete`");
  m_keys4x->SetControlExpression(4, "`Semicolon`");
  m_keys4x->SetControlExpression(5, "`Backspace`");
  m_keys4x->SetControlExpression(6, "`Tab`");
  m_keys4x->SetControlExpression(7, "`Caps Lock`");
  m_keys4x->SetControlExpression(8, "`Left Shift`");
  m_keys4x->SetControlExpression(9, "`Right Shift`");
  m_keys4x->SetControlExpression(10, "`Left Ctrl`");
  m_keys4x->SetControlExpression(11, "`Right Alt`");
  m_keys4x->SetControlExpression(12, "`Left Meta`");
  m_keys4x->SetControlExpression(13, "`Space`");
  m_keys4x->SetControlExpression(14, "`Right Meta`");
  m_keys4x->SetControlExpression(15, "`Menu`");

  m_keys5x->SetControlExpression(0, "`Left`");
  m_keys5x->SetControlExpression(1, "`Down`");
  m_keys5x->SetControlExpression(2, "`Up`");
  m_keys5x->SetControlExpression(3, "`Right`");
  m_keys5x->SetControlExpression(4, "`Enter`");
#else  // linux
  m_keys0x->SetControlExpression(0, "Home");
  m_keys0x->SetControlExpression(1, "End");
  m_keys0x->SetControlExpression(2, "Prior");
  m_keys0x->SetControlExpression(3, "Next");
  m_keys0x->SetControlExpression(4, "Scroll_Lock");

  m_keys2x->SetControlExpression(9, "minus");
  m_keys2x->SetControlExpression(10, "grave");
  m_keys2x->SetControlExpression(11, "Print");
  m_keys2x->SetControlExpression(12, "apostrophe");
  m_keys2x->SetControlExpression(13, "bracketleft");
  m_keys2x->SetControlExpression(14, "equal");
  m_keys2x->SetControlExpression(15, "KP_Multiply");
  m_keys3x->SetControlExpression(0, "bracketright");
  m_keys3x->SetControlExpression(1, "comma");
  m_keys3x->SetControlExpression(2, "period");
  m_keys3x->SetControlExpression(3, "slash");
  m_keys3x->SetControlExpression(4, "backslash");

  m_keys4x->SetControlExpression(1, "Escape");
  m_keys4x->SetControlExpression(2, "Insert");
  m_keys4x->SetControlExpression(3, "Delete");
  m_keys4x->SetControlExpression(4, "semicolon");
  m_keys4x->SetControlExpression(5, "BackSpace");
  m_keys4x->SetControlExpression(6, "Tab");
  m_keys4x->SetControlExpression(7, "Caps_Lock");
  m_keys4x->SetControlExpression(8, "Shift_L");
  m_keys4x->SetControlExpression(9, "Shift_R");
  m_keys4x->SetControlExpression(10, "Control_L");
  m_keys4x->SetControlExpression(11, "Alt_R");
  m_keys4x->SetControlExpression(12, "Super_L");
  m_keys4x->SetControlExpression(13, "space");
  m_keys4x->SetControlExpression(14, "Super_R");
  m_keys4x->SetControlExpression(15, "Menu");

  m_keys5x->SetControlExpression(0, "Left");
  m_keys5x->SetControlExpression(1, "Down");
  m_keys5x->SetControlExpression(2, "Up");
  m_keys5x->SetControlExpression(3, "Right");
  m_keys5x->SetControlExpression(4, "Return");
#endif
}
