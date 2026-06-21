// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASInputWindow.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QAction>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QRect>
#include <QShortcut>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/SectionResizer.h"
#include "DolphinQt/TAS/StickWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSlider.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "Scripting/ScriptList.h"

namespace
{
constexpr const char* TAS_WINDOW_VISIBILITY_SECTION = "TASWindowVisibility";
constexpr const char* TAS_WINDOW_OPTIONS_SECTION = "TASWindowOptions";
constexpr qint64 OPTIONS_MENU_DOUBLE_RIGHT_CLICK_MS = 450;
constexpr qint64 SAME_PHYSICAL_CLICK_SUPPRESSION_MS = 20;

std::string GetDolphinIniPath()
{
  return File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini";
}

bool IsInteractiveOptionsMenuWidget(const QObject* object)
{
  return qobject_cast<const QAbstractButton*>(object) ||
         qobject_cast<const QAbstractSlider*>(object) ||
         qobject_cast<const QAbstractSpinBox*>(object) ||
         qobject_cast<const StickWidget*>(object) || qobject_cast<const IRWidget*>(object);
}
}  // namespace

void InputOverrider::AddFunction(std::string_view group_name, std::string_view control_name,
                                 OverrideFunction function)
{
  m_functions[std::make_pair(group_name, control_name)] = std::move(function);
}

ControllerEmu::InputOverrideFunction InputOverrider::GetInputOverrideFunction() const
{
  return [this](std::string_view group_name, std::string_view control_name,
                ControlState controller_state) {
    const auto it = m_functions.find(std::make_pair(group_name, control_name));
    return it != m_functions.end() ? it->second(controller_state) : std::nullopt;
  };
}

TASInputWindow::TASInputWindow(QWidget* parent) : QDialog(parent)
{
  m_default_window_flags = windowFlags();
  setWindowIcon(Resources::GetAppIcon());

  QGridLayout* settings_layout = new QGridLayout;

  m_use_controller = new QCheckBox(tr("Enable Controller Inpu&t"));
  m_use_controller->setToolTip(tr("Warning: Analog inputs may reset to controller values at "
                                  "random. In some cases this can be fixed by adding a deadzone."));
  settings_layout->addWidget(m_use_controller, 0, 0, 1, 2);

  m_toggle_lines = new QCheckBox(tr("Enable Axis Lines"));
  m_toggle_lines->setChecked(true);
  settings_layout->addWidget(m_toggle_lines, 1, 0, 1, 2);

  auto* turbo_box = new QGroupBox(tr("Turbo"));
  settings_layout->addWidget(turbo_box, 2, 0, 1, 2);

  auto* turbo_layout = new QGridLayout;
  auto* turbo_press_label = new QLabel(tr("Press:"));
  m_turbo_press_frames = new TASSpinBox(turbo_box);
  m_turbo_press_frames->setMinimum(1);
  m_turbo_press_frames->setValue(2);
  turbo_layout->addWidget(turbo_press_label, 0, 0);
  turbo_layout->addWidget(m_turbo_press_frames, 0, 1);

  auto* turbo_release_label = new QLabel(tr("Release:"));
  m_turbo_release_frames = new TASSpinBox(turbo_box);
  m_turbo_release_frames->setMinimum(1);
  m_turbo_release_frames->setValue(2);
  turbo_layout->addWidget(turbo_release_label, 1, 0);
  turbo_layout->addWidget(m_turbo_release_frames, 1, 1);
  turbo_box->setLayout(turbo_layout);

  m_settings_box = new QGroupBox(tr("Settings"));
  m_settings_box->setLayout(settings_layout);

  m_view_inputs_timer = new QTimer(this);
  m_view_inputs_timer->setInterval(16);
  connect(m_view_inputs_timer, &QTimer::timeout, this, &TASInputWindow::PollViewInputs);
  m_view_inputs_timer->start();

  InstallOptionsMenu(this);
}

int TASInputWindow::GetTurboPressFrames() const
{
  return m_turbo_press_frames->value();
}

int TASInputWindow::GetTurboReleaseFrames() const
{
  return m_turbo_release_frames->value();
}

TASCheckBox* TASInputWindow::CreateButton(const QString& text, std::string_view group_name,
                                          std::string_view control_name, InputOverrider* overrider)
{
  TASCheckBox* checkbox = new TASCheckBox(text, this);

  overrider->AddFunction(group_name, control_name, [this, checkbox](ControlState controller_state) {
    return GetButton(checkbox, controller_state);
  });

  return checkbox;
}

QGroupBox* TASInputWindow::CreateStickInputs(const QString& text, std::string_view group_name,
                                             InputOverrider* overrider, int min_x, int min_y,
                                             int max_x, int max_y, Qt::Key x_shortcut_key,
                                             Qt::Key y_shortcut_key, TASSpinBox** x_value_out,
                                             TASSpinBox** y_value_out,
                                             StickWidget** stick_widget_out)
{
  const QKeySequence x_shortcut_key_sequence = QKeySequence(Qt::ALT | x_shortcut_key);
  const QKeySequence y_shortcut_key_sequence = QKeySequence(Qt::ALT | y_shortcut_key);

  auto* box =
      new QGroupBox(QStringLiteral("%1 (%2/%3)")
                        .arg(text, x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                             y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  const int x_default = static_cast<int>(std::round(max_x / 2.));
  const int y_default = static_cast<int>(std::round(max_y / 2.));

  auto* x_layout = new QHBoxLayout;
  TASSpinBox* x_value = CreateSliderValuePair(x_layout, x_default, max_x, x_shortcut_key_sequence,
                                              Qt::Horizontal, box);

  auto* y_layout = new QVBoxLayout;
  TASSpinBox* y_value =
      CreateSliderValuePair(y_layout, y_default, max_y, y_shortcut_key_sequence, Qt::Vertical, box);
  y_value->setMaximumWidth(60);

  auto* visual = new StickWidget(this, max_x, max_y);
  visual->SetX(x_default);
  visual->SetY(y_default);

  connect(x_value, &QSpinBox::valueChanged, visual, &StickWidget::SetX);
  connect(y_value, &QSpinBox::valueChanged, visual, &StickWidget::SetY);
  connect(visual, &StickWidget::ChangedX, x_value, &QSpinBox::setValue);
  connect(visual, &StickWidget::ChangedY, y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, max_x, max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(x_layout);
  layout->addLayout(visual_layout);
  box->setLayout(layout);

  if (x_value_out)
    *x_value_out = x_value;
  if (y_value_out)
    *y_value_out = y_value;
  if (stick_widget_out)
    *stick_widget_out = visual;

  overrider->AddFunction(group_name, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                         [this, x_value, x_default, min_x, max_x](ControlState controller_state) {
                           return GetSpinBox(x_value, x_default, min_x, max_x, controller_state);
                         });

  overrider->AddFunction(group_name, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                         [this, y_value, y_default, min_y, max_y](ControlState controller_state) {
                           return GetSpinBox(y_value, y_default, min_y, max_y, controller_state);
                         });

  return box;
}

QBoxLayout* TASInputWindow::CreateSliderValuePairLayout(
    const QString& text, std::string_view group_name, std::string_view control_name,
    InputOverrider* overrider, int zero, int default_, int min, int max, Qt::Key shortcut_key,
    QWidget* shortcut_widget, std::optional<ControlState> scale, TASSpinBox** value_out)
{
  const QKeySequence shortcut_key_sequence = QKeySequence(Qt::ALT | shortcut_key);

  auto* label = new QLabel(QStringLiteral("%1 (%2)").arg(
      text, shortcut_key_sequence.toString(QKeySequence::NativeText)));

  QBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(label);

  auto* value = CreateSliderValuePair(group_name, control_name, overrider, layout, zero, default_,
                                      min, max, shortcut_key_sequence, Qt::Horizontal,
                                      shortcut_widget, scale);
  if (value_out)
    *value_out = value;

  return layout;
}

TASSpinBox* TASInputWindow::CreateSliderValuePair(
    std::string_view group_name, std::string_view control_name, InputOverrider* overrider,
    QBoxLayout* layout, int zero, int default_, int min, int max,
    QKeySequence shortcut_key_sequence, Qt::Orientation orientation, QWidget* shortcut_widget,
    std::optional<ControlState> scale)
{
  TASSpinBox* value = CreateSliderValuePair(layout, default_, max, shortcut_key_sequence,
                                            orientation, shortcut_widget);

  InputOverrider::OverrideFunction func;
  if (scale)
  {
    func = [this, value, zero, scale](ControlState controller_state) {
      return GetSpinBox(value, zero, controller_state, *scale);
    };
  }
  else
  {
    func = [this, value, zero, min, max](ControlState controller_state) {
      return GetSpinBox(value, zero, min, max, controller_state);
    };
  }

  overrider->AddFunction(group_name, control_name, std::move(func));

  return value;
}

// The shortcut_widget argument needs to specify the container widget that will be hidden/shown.
// This is done to avoid ambiguous shortcuts
TASSpinBox* TASInputWindow::CreateSliderValuePair(QBoxLayout* layout, int default_, int max,
                                                  QKeySequence shortcut_key_sequence,
                                                  Qt::Orientation orientation,
                                                  QWidget* shortcut_widget)
{
  auto* value = new TASSpinBox();
  value->setRange(0, 99999);
  value->setValue(default_);
  connect(value, &QSpinBox::valueChanged, [value, max](int i) {
    if (i > max)
      value->setValue(max);
  });
  auto* slider = new TASSlider(default_, orientation);
  slider->setRange(0, max);
  slider->setValue(default_);
  slider->setFocusPolicy(Qt::ClickFocus);

  connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
  connect(value, &QSpinBox::valueChanged, slider, &QSlider::setValue);

  auto* shortcut = new QShortcut(shortcut_key_sequence, shortcut_widget);
  connect(shortcut, &QShortcut::activated, [value] {
    value->setFocus();
    value->selectAll();
  });

  layout->addWidget(slider);
  layout->addWidget(value);
  if (orientation == Qt::Vertical)
    layout->setAlignment(slider, Qt::AlignRight);

  return value;
}

void TASInputWindow::SetResizableContentLayout(QLayout* content_layout)
{
  content_layout->setSizeConstraint(QLayout::SetNoConstraint);
  setLayout(content_layout);
  setMinimumSize(160, 120);
  resize(sizeHint());
}

void TASInputWindow::MakeSectionResizable(const std::string& key, QWidget* widget)
{
  if (!widget)
    return;

  auto* resizer = new SectionResizer(widget, [this] { RelayoutSections(); }, this);
  m_resizable_sections.push_back({key, widget, resizer});
}

void TASInputWindow::RelayoutSections()
{
  if (QLayout* content_layout = layout())
  {
    content_layout->invalidate();
    content_layout->activate();
  }
}

std::map<std::string, int> TASInputWindow::GetSectionWidths() const
{
  std::map<std::string, int> widths;
  for (const ResizableSection& section : m_resizable_sections)
  {
    if (section.resizer->HasCustomWidth())
      widths[section.key] = section.resizer->CustomWidth();
  }
  return widths;
}

void TASInputWindow::ApplySectionWidths(const std::map<std::string, int>& widths)
{
  for (const ResizableSection& section : m_resizable_sections)
  {
    const auto it = widths.find(section.key);
    if (it != widths.end())
      section.resizer->SetCustomWidth(it->second);
    else
      section.resizer->ClearCustomWidth();
  }
}

void TASInputWindow::RegisterVisibilitySection(const QString& label, const std::string& key,
                                               QWidget* widget)
{
  RegisterVisibilitySection(label, key, std::vector<QWidget*>{widget});
}

void TASInputWindow::RegisterVisibilitySection(const QString& label, const std::string& key,
                                               std::vector<QWidget*> widgets)
{
  widgets.erase(std::remove(widgets.begin(), widgets.end(), nullptr), widgets.end());
  if (widgets.empty())
    return;

  m_visibility_sections.push_back({label, key, std::move(widgets)});
}

void TASInputWindow::SetAlwaysOnTopConfigKey(std::string key)
{
  m_always_on_top_config_key = std::move(key);
  ApplyAlwaysOnTopWindowFlags(IsAlwaysOnTopEnabled());
}

void TASInputWindow::FinalizeVisibilitySections()
{
  for (std::size_t i = 0; i < m_visibility_sections.size(); ++i)
  {
    for (QWidget* widget : m_visibility_sections[i].widgets)
      InstallOptionsMenu(widget);
  }

  ApplyVisibilitySettings();
}

void TASInputWindow::ApplyVisibilitySettings()
{
  for (const auto& section : m_visibility_sections)
  {
    const bool visible = LoadVisibilitySectionVisible(section.key);
    for (QWidget* widget : section.widgets)
      widget->setVisible(visible);
  }
}

bool TASInputWindow::IsVisibilitySectionUserVisible(const std::string& key) const
{
  return LoadVisibilitySectionVisible(key);
}

bool TASInputWindow::IsVisibilitySectionAvailable(const std::string& key) const
{
  (void)key;
  return true;
}

void TASInputWindow::InstallOptionsMenu(QWidget* widget)
{
  const auto install_filter = [this](QWidget* target) {
    if (target->property("tas_options_menu_filter_installed").toBool())
      return;

    target->installEventFilter(this);
    target->setProperty("tas_options_menu_filter_installed", true);
  };

  install_filter(widget);
  const auto children = widget->findChildren<QWidget*>(QString{}, Qt::FindChildrenRecursively);
  for (QWidget* child : children)
    install_filter(child);
}

bool TASInputWindow::eventFilter(QObject* watched, QEvent* event)
{
  QPoint global_pos;
  if (IsOptionsMenuTarget(watched) && ShouldOpenOptionsMenu(event, &global_pos))
  {
    ShowOptionsMenu(global_pos);
    return true;
  }

  return QDialog::eventFilter(watched, event);
}

bool TASInputWindow::IsOptionsMenuTarget(QObject* watched) const
{
  for (const QObject* object = watched; object != nullptr && object != this;
       object = object->parent())
  {
    if (IsInteractiveOptionsMenuWidget(object))
      return false;
  }

  return qobject_cast<QWidget*>(watched) != nullptr;
}

bool TASInputWindow::ShouldOpenOptionsMenu(QEvent* event, QPoint* global_pos)
{
  if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick)
    return false;

  auto* mouse_event = static_cast<QMouseEvent*>(event);
  if (mouse_event->button() != Qt::RightButton)
    return false;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPoint click_pos = mouse_event->globalPosition().toPoint();
#else
  const QPoint click_pos = mouse_event->globalPos();
#endif

  if (m_has_pending_options_menu_click && m_options_menu_click_timer.isValid() &&
      m_options_menu_click_timer.elapsed() <= SAME_PHYSICAL_CLICK_SUPPRESSION_MS &&
      (click_pos - m_last_options_menu_click_pos).manhattanLength() <= 1)
  {
    return false;
  }

  const bool is_double_right_click =
      m_has_pending_options_menu_click && m_options_menu_click_timer.isValid() &&
      m_options_menu_click_timer.elapsed() <= OPTIONS_MENU_DOUBLE_RIGHT_CLICK_MS &&
      (click_pos - m_last_options_menu_click_pos).manhattanLength() <=
          QApplication::startDragDistance();

  m_last_options_menu_click_pos = click_pos;
  m_options_menu_click_timer.restart();
  m_has_pending_options_menu_click = !is_double_right_click;

  if (!is_double_right_click)
    return false;

  *global_pos = click_pos;
  return true;
}

void TASInputWindow::ShowOptionsMenu(const QPoint& global_pos)
{
  QMenu menu(this);

  auto* turbo_visualizer = menu.addAction(tr("Turbo Visualizer"));
  turbo_visualizer->setCheckable(true);
  turbo_visualizer->setChecked(Config::Get(Config::MAIN_MOVIE_TURBO_VISUALIZER));
  connect(turbo_visualizer, &QAction::toggled, this, [](bool value) {
    Config::SetBaseOrCurrent(Config::MAIN_MOVIE_TURBO_VISUALIZER, value);
  });

  auto* always_on_top = menu.addAction(tr("Always on top"));
  always_on_top->setCheckable(true);
  always_on_top->setChecked(IsAlwaysOnTopEnabled());
  connect(always_on_top, &QAction::toggled, this, &TASInputWindow::SetAlwaysOnTopEnabled);

  if (!m_visibility_sections.empty())
    menu.addSeparator();

  for (std::size_t i = 0; i < m_visibility_sections.size(); ++i)
  {
    const auto& section = m_visibility_sections[i];
    if (!IsVisibilitySectionAvailable(section.key))
      continue;

    auto* action = menu.addAction(section.label);
    action->setCheckable(true);
    action->setChecked(LoadVisibilitySectionVisible(section.key));
    connect(action, &QAction::toggled, this,
            [this, i](bool value) { SetVisibilitySectionVisible(i, value); });
  }

  menu.exec(global_pos);
}

bool TASInputWindow::IsAlwaysOnTopEnabled() const
{
  bool enabled = false;
  Common::IniFile ini;
  ini.Load(GetDolphinIniPath());
  ini.GetIfExists(TAS_WINDOW_OPTIONS_SECTION, GetAlwaysOnTopConfigKey(), &enabled);
  return enabled;
}

void TASInputWindow::SetAlwaysOnTopEnabled(bool enabled)
{
  Common::IniFile ini;
  const std::string ini_path = GetDolphinIniPath();
  ini.Load(ini_path);
  ini.GetOrCreateSection(TAS_WINDOW_OPTIONS_SECTION)->Set(GetAlwaysOnTopConfigKey(), enabled,
                                                          false);
  ini.Save(ini_path);

  ApplyAlwaysOnTopWindowFlags(enabled);
}

void TASInputWindow::ApplyAlwaysOnTopWindowFlags(bool enabled)
{
  Qt::WindowFlags flags = m_default_window_flags;
  if (enabled)
    flags |= Qt::WindowStaysOnTopHint | Qt::WindowMinimizeButtonHint;

  if (flags == windowFlags())
    return;

  const bool was_visible = isVisible();
  const QRect previous_geometry = geometry();
  const Qt::WindowStates previous_state = windowState();

  setWindowFlags(flags);
  setGeometry(previous_geometry);
  setWindowState(previous_state);

  if (was_visible)
  {
    show();
    if (enabled)
    {
      raise();
      activateWindow();
    }
  }
}

std::string TASInputWindow::GetAlwaysOnTopConfigKey() const
{
  if (!m_always_on_top_config_key.empty())
    return m_always_on_top_config_key;

  return std::string(metaObject()->className()) + ".AlwaysOnTop";
}

void TASInputWindow::SetVisibilitySectionVisible(std::size_t section_index, bool visible)
{
  if (section_index >= m_visibility_sections.size())
    return;

  auto& section = m_visibility_sections[section_index];
  SaveVisibilitySectionVisible(section.key, visible);
  ApplyVisibilitySettings();
  if (layout())
  {
    layout()->invalidate();
    layout()->activate();
  }
}

bool TASInputWindow::LoadVisibilitySectionVisible(const std::string& key) const
{
  bool visible = true;
  Common::IniFile ini;
  ini.Load(GetDolphinIniPath());
  ini.GetIfExists(TAS_WINDOW_VISIBILITY_SECTION, key, &visible);
  return visible;
}

void TASInputWindow::SaveVisibilitySectionVisible(const std::string& key, bool visible) const
{
  Common::IniFile ini;
  const std::string ini_path = GetDolphinIniPath();
  ini.Load(ini_path);
  ini.GetOrCreateSection(TAS_WINDOW_VISIBILITY_SECTION)->Set(key, visible, true);
  ini.Save(ini_path);
}

std::optional<ControlState> TASInputWindow::GetButton(TASCheckBox* checkbox,
                                                      ControlState controller_state)
{
  const bool pressed = std::llround(controller_state) > 0;
  if (m_use_controller->isChecked() && !ShouldViewMovieInputs())
    checkbox->OnControllerValueChanged(pressed);

  return checkbox->GetValue() ? 1.0 : 0.0;
}

std::optional<ControlState> TASInputWindow::GetSpinBox(TASSpinBox* spin, int zero, int min, int max,
                                                       ControlState controller_state)
{
  const int controller_value = ControllerEmu::MapFloat<int>(controller_state, zero, 0, max);

  if (m_use_controller->isChecked() && !ShouldViewMovieInputs())
    spin->OnControllerValueChanged(controller_value);

  return ControllerEmu::MapToFloat<ControlState, int>(spin->GetValue(), zero, min, max);
}

std::optional<ControlState> TASInputWindow::GetSpinBox(TASSpinBox* spin, int zero,
                                                       ControlState controller_state,
                                                       ControlState scale)
{
  const int controller_value = static_cast<int>(std::llround(controller_state * scale + zero));

  if (m_use_controller->isChecked() && !ShouldViewMovieInputs())
    spin->OnControllerValueChanged(controller_value);

  return (spin->GetValue() - zero) / scale;
}

void TASInputWindow::changeEvent(QEvent* const event)
{
  if (event->type() == QEvent::ActivationChange)
  {
    const bool active_window_is_tas_input =
        qobject_cast<TASInputWindow*>(QApplication::activeWindow()) != nullptr;

    // Switching between TAS Input windows will call SetTASInputFocus(true) twice, but that's fine.
    Host::GetInstance()->SetTASInputFocus(active_window_is_tas_input);
  }
  QDialog::changeEvent(event);
}

void TASInputWindow::PollViewInputs()
{
  if (!ShouldViewMovieInputs())
    return;

  UpdateLiveInputDisplay();
}

bool TASInputWindow::ShouldViewMovieInputs() const
{
  if (!isVisible() || !Config::Get(Config::MAIN_MOVIE_VIEW_TAS_INPUTS))
    return false;

  const auto state = Core::GetState(Core::System::GetInstance());
  if (state != Core::State::Running && state != Core::State::Paused)
    return false;

  const auto& movie = Core::System::GetInstance().GetMovie();
  const bool script_active =
      std::any_of(Scripts::g_scripts.begin(), Scripts::g_scripts.end(),
                  [](const auto& script) { return script.second != nullptr; });
  return movie.IsPlayingInput() || script_active;
}
