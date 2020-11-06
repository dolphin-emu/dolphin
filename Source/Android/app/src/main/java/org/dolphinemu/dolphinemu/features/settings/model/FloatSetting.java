package org.dolphinemu.dolphinemu.features.settings.model;

public enum FloatSetting implements AbstractFloatSetting
{
  // These entries have the same names and order as in C++, just for consistency.

  MAIN_EMULATION_SPEED(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "EmulationSpeed", 1.0f),
  MAIN_OVERCLOCK(Settings.FILE_DOLPHIN, Settings.SECTION_INI_CORE, "Overclock", 1.0f),

  // GameCube Default Overlay

  MAIN_BUTTON_TYPE_0_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonA_X", 0f),
  MAIN_BUTTON_TYPE_0_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonA_Y", 0f),
  MAIN_BUTTON_TYPE_1_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonB_X", 0f),
  MAIN_BUTTON_TYPE_1_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonB_Y", 0f),
  MAIN_BUTTON_TYPE_2_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonStart_X", 0f),
  MAIN_BUTTON_TYPE_2_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonStart_Y", 0f),
  MAIN_BUTTON_TYPE_3_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonX_X", 0f),
  MAIN_BUTTON_TYPE_3_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonX_Y", 0f),
  MAIN_BUTTON_TYPE_4_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonY_X", 0f),
  MAIN_BUTTON_TYPE_4_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonY_Y", 0f),
  MAIN_BUTTON_TYPE_5_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonZ_X", 0f),
  MAIN_BUTTON_TYPE_5_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonZ_Y", 0f),
  MAIN_BUTTON_TYPE_6_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonUp_X", 0f),
  MAIN_BUTTON_TYPE_6_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonUp_Y", 0f),
  MAIN_BUTTON_TYPE_10_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeStickMain_X", 0f),
  MAIN_BUTTON_TYPE_10_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeStickMain_Y", 0f),
  MAIN_BUTTON_TYPE_15_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeStickC_X", 0f),
  MAIN_BUTTON_TYPE_15_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeStickC_Y", 0f),
  MAIN_BUTTON_TYPE_20_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeTriggerL_X", 0f),
  MAIN_BUTTON_TYPE_20_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeTriggerL_Y", 0f),
  MAIN_BUTTON_TYPE_21_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeTriggerR_X", 0f),
  MAIN_BUTTON_TYPE_21_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeTriggerR_Y", 0f),

  // GameCube Portrait Default Overlay

  MAIN_BUTTON_TYPE_0_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonA_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_0_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonA_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_1_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonB_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_1_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonB_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_2_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonStart_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_2_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonStart_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_3_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonX_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_3_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonX_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_4_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonY_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_4_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonY_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_5_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonZ_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_5_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonZ_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_6_PORTRAIT_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonUp_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_6_PORTRAIT_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeButtonUp_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_10_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeStickMain_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_10_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeStickMain_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_15_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeStickC_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_15_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeStickC_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_20_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeTriggerL_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_20_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeTriggerL_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_21_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeTriggerR_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_21_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeTriggerR_Portrait_Y", 0f),

  // Wii Default Overlay

  MAIN_BUTTON_TYPE_100_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonA_X", 0f),
  MAIN_BUTTON_TYPE_100_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonA_Y", 0f),
  MAIN_BUTTON_TYPE_101_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonB_X", 0f),
  MAIN_BUTTON_TYPE_101_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonB_Y", 0f),
  MAIN_BUTTON_TYPE_102_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonMinus_X", 0f),
  MAIN_BUTTON_TYPE_102_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonMinus_Y", 0f),
  MAIN_BUTTON_TYPE_103_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonPlus_X", 0f),
  MAIN_BUTTON_TYPE_103_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonPlus_Y", 0f),
  MAIN_BUTTON_TYPE_104_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonHome_X", 0f),
  MAIN_BUTTON_TYPE_104_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonHome_Y", 0f),
  MAIN_BUTTON_TYPE_105_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton1_X", 0f),
  MAIN_BUTTON_TYPE_105_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton1_Y", 0f),
  MAIN_BUTTON_TYPE_106_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton2_X", 0f),
  MAIN_BUTTON_TYPE_106_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton2_Y", 0f),
  MAIN_BUTTON_TYPE_107_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteUp_X", 0f),
  MAIN_BUTTON_TYPE_107_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteUp_Y", 0f),
  MAIN_BUTTON_TYPE_200_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukButtonC_X", 0f),
  MAIN_BUTTON_TYPE_200_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukButtonC_Y", 0f),
  MAIN_BUTTON_TYPE_201_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukButtonZ_X", 0f),
  MAIN_BUTTON_TYPE_201_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukButtonZ_Y", 0f),
  MAIN_BUTTON_TYPE_202_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukStick_X", 0f),
  MAIN_BUTTON_TYPE_202_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeNunchukStick_Y", 0f),

  // Wii Only Default Overlay

  MAIN_BUTTON_TYPE_100_H_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonA_H_X", 0f),
  MAIN_BUTTON_TYPE_100_H_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonA_H_Y", 0f),
  MAIN_BUTTON_TYPE_101_H_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonB_H_X", 0f),
  MAIN_BUTTON_TYPE_101_H_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButtonB_H_Y", 0f),
  MAIN_BUTTON_TYPE_105_H_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton1_H_X", 0f),
  MAIN_BUTTON_TYPE_105_H_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton1_H_Y", 0f),
  MAIN_BUTTON_TYPE_106_H_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton2_H_X", 0f),
  MAIN_BUTTON_TYPE_106_H_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteButton2_H_Y", 0f),
  MAIN_BUTTON_TYPE_107_O_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteUp_O_X", 0f),
  MAIN_BUTTON_TYPE_107_O_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteUp_O_Y", 0f),
  MAIN_BUTTON_TYPE_110_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteRight_X", 0f),
  MAIN_BUTTON_TYPE_110_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeWiimoteRight_Y", 0f),

  // Wii Portrait Default Overlay

  MAIN_BUTTON_TYPE_100_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonA_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_100_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonA_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_101_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonB_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_101_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonB_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_102_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonMinus_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_102_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonMinus_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_103_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonPlus_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_103_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonPlus_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_104_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonHome_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_104_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonHome_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_105_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton1_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_105_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton1_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_106_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton2_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_106_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton2_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_107_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteUp_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_107_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteUp_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_200_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukButtonC_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_200_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukButtonC_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_201_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukButtonZ_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_201_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukButtonZ_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_202_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukStick_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_202_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeNunchukStick_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_110_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteRight_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_110_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteRight_Portrait_Y", 0f),

  // Wii Only Portrait Default Overlay

  MAIN_BUTTON_TYPE_100_H_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonA_H_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_100_H_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonA_H_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_101_H_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonB_H_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_101_H_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButtonB_H_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_105_H_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton1_H_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_105_H_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton1_H_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_106_H_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton2_H_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_106_H_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteButton2_H_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_107_O_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteUp_O_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_107_O_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeWiimoteUp_O_Portrait_Y", 0f),

  // Wii Classic Default Overlay

  MAIN_BUTTON_TYPE_300_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonA_X", 0f),
  MAIN_BUTTON_TYPE_300_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonA_Y", 0f),
  MAIN_BUTTON_TYPE_301_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonB_X", 0f),
  MAIN_BUTTON_TYPE_301_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonB_Y", 0f),
  MAIN_BUTTON_TYPE_302_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonX_X", 0f),
  MAIN_BUTTON_TYPE_302_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonX_Y", 0f),
  MAIN_BUTTON_TYPE_303_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonY_X", 0f),
  MAIN_BUTTON_TYPE_303_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonY_Y", 0f),
  MAIN_BUTTON_TYPE_304_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonMinus_X", 0f),
  MAIN_BUTTON_TYPE_304_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonMinus_Y", 0f),
  MAIN_BUTTON_TYPE_305_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonPlus_X", 0f),
  MAIN_BUTTON_TYPE_305_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonPlus_Y", 0f),
  MAIN_BUTTON_TYPE_306_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonHome_X", 0f),
  MAIN_BUTTON_TYPE_306_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonHome_Y", 0f),
  MAIN_BUTTON_TYPE_307_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonZL_X", 0f),
  MAIN_BUTTON_TYPE_307_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonZL_Y", 0f),
  MAIN_BUTTON_TYPE_308_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonZR_X", 0f),
  MAIN_BUTTON_TYPE_308_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicButtonZR_Y", 0f),
  MAIN_BUTTON_TYPE_309_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicDpadUp_X", 0f),
  MAIN_BUTTON_TYPE_309_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicDpadUp_Y", 0f),
  MAIN_BUTTON_TYPE_313_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicStickLeft_X", 0f),
  MAIN_BUTTON_TYPE_313_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicStickLeft_Y", 0f),
  MAIN_BUTTON_TYPE_318_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicStickRight_X", 0f),
  MAIN_BUTTON_TYPE_318_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicStickRight_Y", 0f),
  MAIN_BUTTON_TYPE_323_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicTriggerL_X", 0f),
  MAIN_BUTTON_TYPE_323_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicTriggerL_Y", 0f),
  MAIN_BUTTON_TYPE_324_X(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicTriggerR_X", 0f),
  MAIN_BUTTON_TYPE_324_Y(Settings.FILE_DOLPHIN, Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS,
          "buttonTypeClassicTriggerR_Y", 0f),

  // Wii Classic Portrait Default Overlay

  MAIN_BUTTON_TYPE_300_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonA_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_300_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonA_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_301_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonB_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_301_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonB_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_302_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonX_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_302_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonX_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_303_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonY_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_303_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonY_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_304_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonMinus_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_304_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonMinus_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_305_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonPlus_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_305_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonPlus_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_306_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonHome_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_306_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonHome_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_307_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonZL_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_307_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonZL_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_308_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonZR_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_308_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicButtonZR_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_309_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicDpadUp_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_309_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicDpadUp_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_313_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicStickLeft_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_313_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicStickLeft_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_318_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicStickRight_Portrait_X",
          0f),
  MAIN_BUTTON_TYPE_318_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicStickRight_Portrait_Y",
          0f),
  MAIN_BUTTON_TYPE_323_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicTriggerL_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_323_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicTriggerL_Portrait_Y", 0f),
  MAIN_BUTTON_TYPE_324_PORTRAIT_X(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicTriggerR_Portrait_X", 0f),
  MAIN_BUTTON_TYPE_324_PORTRAIT_Y(Settings.FILE_DOLPHIN,
          Settings.SECTION_INI_ANDROID_OVERLAY_BUTTONS, "buttonTypeClassicTriggerR_Portrait_Y", 0f);

  private final String mFile;
  private final String mSection;
  private final String mKey;
  private final float mDefaultValue;

  FloatSetting(String file, String section, String key, float defaultValue)
  {
    mFile = file;
    mSection = section;
    mKey = key;
    mDefaultValue = defaultValue;
  }

  @Override
  public boolean isOverridden(Settings settings)
  {
    if (settings.isGameSpecific() && !NativeConfig.isSettingSaveable(mFile, mSection, mKey))
      return settings.getSection(mFile, mSection).exists(mKey);
    else
      return NativeConfig.isOverridden(mFile, mSection, mKey);
  }

  @Override
  public boolean isRuntimeEditable()
  {
    return NativeConfig.isSettingSaveable(mFile, mSection, mKey);
  }

  @Override
  public boolean delete(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.deleteKey(settings.getWriteLayer(), mFile, mSection, mKey);
    }
    else
    {
      return settings.getSection(mFile, mSection).delete(mKey);
    }
  }

  @Override
  public float getFloat(Settings settings)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      return NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
    }
    else
    {
      return settings.getSection(mFile, mSection).getFloat(mKey, mDefaultValue);
    }
  }

  @Override
  public void setFloat(Settings settings, float newValue)
  {
    if (NativeConfig.isSettingSaveable(mFile, mSection, mKey))
    {
      NativeConfig.setFloat(settings.getWriteLayer(), mFile, mSection, mKey, newValue);
    }
    else
    {
      settings.getSection(mFile, mSection).setFloat(mKey, newValue);
    }
  }

  public float getFloatGlobal()
  {
    return NativeConfig.getFloat(NativeConfig.LAYER_ACTIVE, mFile, mSection, mKey, mDefaultValue);
  }
}
