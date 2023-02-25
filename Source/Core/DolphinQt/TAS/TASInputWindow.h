// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <QDialog>

#include "Common/CommonTypes.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

class QBoxLayout;
class QCheckBox;
class QDialog;
class QGroupBox;
class QSpinBox;
class QString;
class TASCheckBox;

class InputOverrider final
{
public:
  using OverrideFunction = std::function<std::optional<ControlState>(ControlState)>;

  void AddFunction(std::string_view group_name, std::string_view control_name,
                   OverrideFunction function);

  ControllerEmu::InputOverrideFunction GetInputOverrideFunction() const;

private:
  std::map<std::pair<std::string_view, std::string_view>, OverrideFunction> m_functions;
};

class TASInputWindow : public QDialog
{
  Q_OBJECT
public:
  explicit TASInputWindow(QWidget* parent);

  int GetTurboPressFrames() const;
  int GetTurboReleaseFrames() const;

protected:
  TASCheckBox* CreateButton(const QString& text, std::string_view group_name,
                            std::string_view control_name, InputOverrider* overrider);
  QGroupBox* CreateStickInputs(const QString& text, std::string_view group_name,
                               InputOverrider* overrider, QSpinBox*& x_value, QSpinBox*& y_value,
                               u16 min_x, u16 min_y, u16 max_x, u16 max_y, Qt::Key x_shortcut_key,
                               Qt::Key y_shortcut_key);
  QBoxLayout* CreateSliderValuePairLayout(const QString& text, std::string_view group_name,
                                          std::string_view control_name, InputOverrider* overrider,
                                          QSpinBox*& value, u16 zero, int default_, u16 min,
                                          u16 max, Qt::Key shortcut_key, QWidget* shortcut_widget,
                                          std::optional<ControlState> scale = {});
  QSpinBox* CreateSliderValuePair(std::string_view group_name, std::string_view control_name,
                                  InputOverrider* overrider, QBoxLayout* layout, u16 zero,
                                  int default_, u16 min, u16 max,
                                  QKeySequence shortcut_key_sequence, Qt::Orientation orientation,
                                  QWidget* shortcut_widget, std::optional<ControlState> scale = {});
  QSpinBox* CreateSliderValuePair(QBoxLayout* layout, int default_, u16 max,
                                  QKeySequence shortcut_key_sequence, Qt::Orientation orientation,
                                  QWidget* shortcut_widget);

  QGroupBox* m_settings_box;
  QCheckBox* m_use_controller;
  QSpinBox* m_turbo_press_frames;
  QSpinBox* m_turbo_release_frames;

private:
  std::optional<ControlState> GetButton(TASCheckBox* checkbox, ControlState controller_state);
  std::optional<ControlState> GetSpinBox(QSpinBox* spin, u16 zero, u16 min, u16 max,
                                         ControlState controller_state);
  std::optional<ControlState> GetSpinBox(QSpinBox* spin, u16 zero, ControlState controller_state,
                                         ControlState scale);

  std::map<TASCheckBox*, bool> m_checkbox_set_by_controller;
  std::map<QSpinBox*, u16> m_spinbox_most_recent_values;
};
