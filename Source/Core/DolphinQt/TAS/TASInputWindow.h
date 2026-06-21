// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QDialog>
#include <QElapsedTimer>
#include <QString>

#include "Common/CommonTypes.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

class QBoxLayout;
class QCheckBox;
class QDialog;
class QEvent;
class QGroupBox;
class QLayout;
class QPoint;
class SectionResizer;
class StickWidget;
class TASCheckBox;
class TASSpinBox;
class QTimer;
class QWidget;

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

  // Custom widths of drag-resized bottom-row sections, keyed by section. Persisted in window presets.
  std::map<std::string, int> GetSectionWidths() const;
  void ApplySectionWidths(const std::map<std::string, int>& widths);

protected:
  virtual void UpdateLiveInputDisplay() = 0;

  TASCheckBox* CreateButton(const QString& text, std::string_view group_name,
                            std::string_view control_name, InputOverrider* overrider);
  QGroupBox* CreateStickInputs(const QString& text, std::string_view group_name,
                               InputOverrider* overrider, int min_x, int min_y, int max_x,
                               int max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key,
                               TASSpinBox** x_value_out = nullptr,
                               TASSpinBox** y_value_out = nullptr,
                               StickWidget** stick_widget_out = nullptr);
  QBoxLayout* CreateSliderValuePairLayout(const QString& text, std::string_view group_name,
                                          std::string_view control_name, InputOverrider* overrider,
                                          int zero, int default_, int min, int max,
                                          Qt::Key shortcut_key, QWidget* shortcut_widget,
                                          std::optional<ControlState> scale = {},
                                          TASSpinBox** value_out = nullptr);
  TASSpinBox* CreateSliderValuePair(std::string_view group_name, std::string_view control_name,
                                    InputOverrider* overrider, QBoxLayout* layout, int zero,
                                    int default_, int min, int max,
                                    QKeySequence shortcut_key_sequence, Qt::Orientation orientation,
                                    QWidget* shortcut_widget,
                                    std::optional<ControlState> scale = {});
  TASSpinBox* CreateSliderValuePair(QBoxLayout* layout, int default_, int max,
                                    QKeySequence shortcut_key_sequence, Qt::Orientation orientation,
                                    QWidget* shortcut_widget);
  void SetResizableContentLayout(QLayout* content_layout);
  void MakeSectionResizable(const std::string& key, QWidget* widget);
  void RegisterVisibilitySection(const QString& label, const std::string& key, QWidget* widget);
  void RegisterVisibilitySection(const QString& label, const std::string& key,
                                 std::vector<QWidget*> widgets);
  void SetAlwaysOnTopConfigKey(std::string key);
  void FinalizeVisibilitySections();
  virtual void ApplyVisibilitySettings();
  bool IsVisibilitySectionUserVisible(const std::string& key) const;
  virtual bool IsVisibilitySectionAvailable(const std::string& key) const;

  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  QGroupBox* m_settings_box;
  QCheckBox* m_use_controller;
  QCheckBox* m_toggle_lines = nullptr;
  TASSpinBox* m_turbo_press_frames = nullptr;
  TASSpinBox* m_turbo_release_frames = nullptr;

private:
  struct VisibilitySection
  {
    QString label;
    std::string key;
    std::vector<QWidget*> widgets;
  };

  struct ResizableSection
  {
    std::string key;
    QWidget* widget;
    SectionResizer* resizer;
  };

  void RelayoutSections();
  bool ShouldViewMovieInputs() const;
  void PollViewInputs();
  void InstallOptionsMenu(QWidget* widget);
  bool IsOptionsMenuTarget(QObject* watched) const;
  bool ShouldOpenOptionsMenu(QEvent* event, QPoint* global_pos);
  void ShowOptionsMenu(const QPoint& global_pos);
  bool IsAlwaysOnTopEnabled() const;
  void SetAlwaysOnTopEnabled(bool enabled);
  void ApplyAlwaysOnTopWindowFlags(bool enabled);
  std::string GetAlwaysOnTopConfigKey() const;
  void SetVisibilitySectionVisible(std::size_t section_index, bool visible);
  bool LoadVisibilitySectionVisible(const std::string& key) const;
  void SaveVisibilitySectionVisible(const std::string& key, bool visible) const;
  std::optional<ControlState> GetButton(TASCheckBox* checkbox, ControlState controller_state);
  std::optional<ControlState> GetSpinBox(TASSpinBox* spin, int zero, int min, int max,
                                         ControlState controller_state);
  std::optional<ControlState> GetSpinBox(TASSpinBox* spin, int zero, ControlState controller_state,
                                         ControlState scale);
  std::vector<VisibilitySection> m_visibility_sections;
  std::vector<ResizableSection> m_resizable_sections;
  Qt::WindowFlags m_default_window_flags;
  std::string m_always_on_top_config_key;
  QElapsedTimer m_options_menu_click_timer;
  QPoint m_last_options_menu_click_pos;
  bool m_has_pending_options_menu_click = false;
  QTimer* m_view_inputs_timer = nullptr;
};
