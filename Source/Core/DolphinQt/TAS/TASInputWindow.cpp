// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASInputWindow.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
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
#include <QResizeEvent>
#include <QShortcut>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

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
constexpr const char* TAS_WINDOW_LAYOUT_SECTION = "TASWindowLayouts";
constexpr qint64 OPTIONS_MENU_DOUBLE_RIGHT_CLICK_MS = 450;
constexpr qint64 SAME_PHYSICAL_CLICK_SUPPRESSION_MS = 20;
constexpr int MAX_LAYOUT_SECTION_SIZE = 16384;
constexpr int LAYOUT_GRID_SIZE = 12;
constexpr int LAYOUT_EDGE_MARGIN = 8;
constexpr int LAYOUT_SECTION_SPACING = 6;

int SnapToLayoutGrid(int value)
{
  return ((value + LAYOUT_GRID_SIZE / 2) / LAYOUT_GRID_SIZE) * LAYOUT_GRID_SIZE;
}

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

class ProportionalTASLayout final : public QLayout
{
public:
  explicit ProportionalTASLayout(const QSize& baseline_size) : m_baseline_size(baseline_size)
  {
    setContentsMargins(0, 0, 0, 0);
  }

  ~ProportionalTASLayout() override
  {
    while (QLayoutItem* item = takeAt(0))
      delete item;
  }

  void AddWidget(QWidget* widget, const QRect& baseline_geometry)
  {
    addChildWidget(widget);
    m_items.push_back({new QWidgetItem(widget), widget, baseline_geometry});
  }

  void addItem(QLayoutItem* item) override
  {
    m_items.push_back({item, item->widget(), item->geometry()});
  }

  int count() const override { return static_cast<int>(m_items.size()); }

  QLayoutItem* itemAt(int index) const override
  {
    return index >= 0 && index < count() ? m_items[index].item : nullptr;
  }

  QLayoutItem* takeAt(int index) override
  {
    if (index < 0 || index >= count())
      return nullptr;
    QLayoutItem* item = m_items[index].item;
    m_items.erase(m_items.begin() + index);
    return item;
  }

  QSize sizeHint() const override { return m_baseline_size; }
  QSize minimumSize() const override { return {160, 120}; }
  Qt::Orientations expandingDirections() const override
  {
    return Qt::Horizontal | Qt::Vertical;
  }

  void setGeometry(const QRect& geometry) override
  {
    QLayout::setGeometry(geometry);
    const int baseline_width = std::max(1, m_baseline_size.width());
    const int baseline_height = std::max(1, m_baseline_size.height());
    const auto scale_x = [&](int value) {
      return geometry.x() + qRound(static_cast<double>(value) * geometry.width() / baseline_width);
    };
    const auto scale_y = [&](int value) {
      return geometry.y() +
             qRound(static_cast<double>(value) * geometry.height() / baseline_height);
    };

    for (const Item& item : m_items)
    {
      const int left = scale_x(item.baseline_geometry.left());
      const int top = scale_y(item.baseline_geometry.top());
      const int right = scale_x(item.baseline_geometry.right() + 1);
      const int bottom = scale_y(item.baseline_geometry.bottom() + 1);
      const QRect scaled_geometry(left, top, std::max(1, right - left),
                                  std::max(1, bottom - top));
      if (item.widget)
        item.widget->setGeometry(scaled_geometry);
      else
        item.item->setGeometry(scaled_geometry);
    }
  }

private:
  struct Item
  {
    QLayoutItem* item;
    QWidget* widget;
    QRect baseline_geometry;
  };

  QSize m_baseline_size;
  std::vector<Item> m_items;
};
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

  m_layout_save_timer = new QTimer(this);
  m_layout_save_timer->setSingleShot(true);
  m_layout_save_timer->setInterval(200);
  connect(m_layout_save_timer, &QTimer::timeout, this, &TASInputWindow::SaveLayoutState);

  InstallOptionsMenu(this);
}

TASInputWindow::~TASInputWindow()
{
  SaveLayoutState();
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

  auto* value =
      CreateSliderValuePair(group_name, control_name, overrider, layout, zero, default_, min, max,
                            shortcut_key_sequence, Qt::Horizontal, shortcut_widget, scale);
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

void TASInputWindow::SetDefaultContentLayoutBuilder(std::function<QLayout*()> builder)
{
  m_default_layout_builder = std::move(builder);
  ReplaceContentLayout(m_default_layout_builder());
  resize(sizeHint());
}

void TASInputWindow::ReplaceContentLayout(QLayout* content_layout)
{
  if (!content_layout)
    return;

  delete layout();
  content_layout->setSizeConstraint(QLayout::SetNoConstraint);
  setLayout(content_layout);
  setMinimumSize(160, 120);
  RelayoutSections();
}

void TASInputWindow::MakeSectionResizable(const std::string& key, QWidget* widget)
{
  if (!widget)
    return;

  if (FindResizableSection(key))
    return;

  auto* resizer = new SectionResizer(
      widget, [this] { RelayoutSections(); }, [this] { SaveLayoutState(); },
      [this](QWidget* moved, const QRect& geometry, SectionGeometryOperation operation,
             Qt::Edges resize_edges, bool commit) {
        return HandleSectionGeometry(moved, geometry, operation, resize_edges, commit);
      },
      this);
  m_resizable_sections.push_back({key, widget, resizer});
}

TASInputWindow::ResizableSection* TASInputWindow::FindResizableSection(std::string_view key)
{
  const auto it =
      std::find_if(m_resizable_sections.begin(), m_resizable_sections.end(),
                   [key](const ResizableSection& section) { return section.key == key; });
  return it == m_resizable_sections.end() ? nullptr : &*it;
}

const TASInputWindow::ResizableSection*
TASInputWindow::FindResizableSection(std::string_view key) const
{
  const auto it =
      std::find_if(m_resizable_sections.begin(), m_resizable_sections.end(),
                   [key](const ResizableSection& section) { return section.key == key; });
  return it == m_resizable_sections.end() ? nullptr : &*it;
}

void TASInputWindow::RelayoutSections()
{
  if (QLayout* content_layout = layout())
  {
    content_layout->invalidate();
    content_layout->activate();
  }
  updateGeometry();
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
  SaveLayoutState();
}

std::string TASInputWindow::GetLayoutState() const
{
  if (m_has_custom_layout)
  {
    std::string state = "scaled:" + std::to_string(width()) + ':' + std::to_string(height()) + ';';
    bool first = true;
    for (const LayoutPlacement& placement : CaptureCurrentLayout())
    {
      if (!first)
        state += '|';
      first = false;
      state += placement.key + ':' + std::to_string(placement.x) + ':' +
               std::to_string(placement.y) + ':' + std::to_string(placement.width) + ':' +
               std::to_string(placement.height);
    }
    return state;
  }

  std::string state = "default;";
  bool first = true;
  for (const ResizableSection& section : m_resizable_sections)
  {
    if (!section.resizer->HasCustomWidth() && !section.resizer->HasCustomHeight())
    {
      continue;
    }

    if (!first)
      state += '|';
    first = false;
    state += section.key + ':' + std::to_string(section.resizer->CustomWidth()) + ':' +
             std::to_string(section.resizer->CustomHeight());
  }
  return state;
}

void TASInputWindow::ApplyLayoutState(std::string_view state, bool restore_window_size)
{
  const std::size_t separator = state.find(';');
  if (separator == std::string_view::npos)
    return;

  const std::string_view mode_descriptor = state.substr(0, separator);
  const std::vector<std::string> mode_fields = SplitString(std::string(mode_descriptor), ':');
  if (mode_fields.empty())
    return;
  const std::string_view mode = mode_fields[0];
  const bool use_default = mode == "default";
  const bool use_freeform = mode == "free";
  const bool use_scaled = mode == "scaled";
  const bool use_obsolete_layout = mode == "grid" || mode == "flow";
  if (!use_default && !use_freeform && !use_scaled && !use_obsolete_layout)
    return;

  int baseline_width = width();
  int baseline_height = height();
  if (use_scaled &&
      (mode_fields.size() != 3 || !TryParse(mode_fields[1], &baseline_width) ||
       !TryParse(mode_fields[2], &baseline_height) || baseline_width <= 0 ||
       baseline_height <= 0 || baseline_width > MAX_LAYOUT_SECTION_SIZE ||
       baseline_height > MAX_LAYOUT_SECTION_SIZE))
  {
    return;
  }
  const QSize baseline_size(baseline_width, baseline_height);

  std::vector<LayoutPlacement> parsed_sections;
  std::set<std::string> seen_keys;
  std::string payload(state.substr(separator + 1));
  std::size_t start = 0;
  while (!use_obsolete_layout && start <= payload.size())
  {
    const std::size_t end = payload.find('|', start);
    const std::string entry = payload.substr(start, end - start);
    const std::vector<std::string> fields = SplitString(entry, ':');
    const bool has_expected_fields =
        ((use_freeform || use_scaled) && fields.size() == 5) ||
        (use_default && fields.size() == 3);
    if (has_expected_fields && FindResizableSection(fields[0]))
    {
      int x = 0;
      int y = 0;
      int width = 0;
      int height = 0;
      const bool parsed = use_freeform || use_scaled ?
                              TryParse(fields[1], &x) && TryParse(fields[2], &y) &&
                                  TryParse(fields[3], &width) && TryParse(fields[4], &height) :
                              TryParse(fields[1], &width) && TryParse(fields[2], &height);
      const bool valid_size = use_freeform || use_scaled ? width > 0 && height > 0 :
                                                          width >= 0 && height >= 0;
      if (parsed && x >= 0 && y >= 0 && x <= MAX_LAYOUT_SECTION_SIZE &&
          y <= MAX_LAYOUT_SECTION_SIZE && valid_size &&
          width <= MAX_LAYOUT_SECTION_SIZE && height <= MAX_LAYOUT_SECTION_SIZE &&
          seen_keys.insert(fields[0]).second)
      {
        parsed_sections.push_back({fields[0], x, y, width, height});
      }
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }

  for (ResizableSection& section : m_resizable_sections)
  {
    section.resizer->SetRearrangeEnabled(false);
    section.resizer->ClearCustomSize();
  }
  m_rearrange_enabled = false;

  if (use_default)
  {
    for (const LayoutPlacement& parsed : parsed_sections)
    {
      if (ResizableSection* section = FindResizableSection(parsed.key))
        section->resizer->SetCustomSize(parsed.width, parsed.height);
    }
  }

  if ((use_freeform || use_scaled) && !parsed_sections.empty())
  {
    if (restore_window_size && use_scaled)
      resize(baseline_size);
    BuildResponsiveLayout(parsed_sections, baseline_size);
    m_has_custom_layout = true;
  }
  else
  {
    m_has_custom_layout = false;
    if (m_default_layout_builder)
      ReplaceContentLayout(m_default_layout_builder());
  }

  ApplyVisibilitySettings();
  if (m_has_custom_layout && !layout())
    UpdateFreeformMinimumSize();
  RelayoutSections();
  SaveLayoutState();
}

std::vector<TASInputWindow::LayoutPlacement> TASInputWindow::CaptureCurrentLayout() const
{
  std::vector<LayoutPlacement> placements;
  placements.reserve(m_resizable_sections.size());
  for (const ResizableSection& section : m_resizable_sections)
  {
    const QRect geometry = section.widget->geometry();
    placements.push_back(
        {section.key, geometry.x(), geometry.y(), geometry.width(), geometry.height()});
  }
  return placements;
}

void TASInputWindow::EnterFreeformLayout(const std::vector<LayoutPlacement>& placements)
{
  const QSize current_size = size();
  delete layout();

  std::set<std::string> added;
  const auto apply_placement = [&](const LayoutPlacement& placement) {
    ResizableSection* section = FindResizableSection(placement.key);
    if (!section || !added.insert(placement.key).second)
      return;
    section->resizer->SetCustomSizeExact(placement.width, placement.height);
    section->widget->setGeometry(placement.x, placement.y, placement.width, placement.height);
  };

  for (const LayoutPlacement& placement : placements)
    apply_placement(placement);
  for (ResizableSection& section : m_resizable_sections)
  {
    if (!added.contains(section.key))
    {
      const QRect geometry = section.widget->geometry();
      apply_placement(
          {section.key, geometry.x(), geometry.y(), geometry.width(), geometry.height()});
    }
  }

  UpdateFreeformMinimumSize();
  resize(current_size.expandedTo(minimumSize()));
}

void TASInputWindow::BuildResponsiveLayout(const std::vector<LayoutPlacement>& placements,
                                           const QSize& baseline_size)
{
  const QSize current_size = size();
  std::vector<LayoutPlacement> valid_placements;
  valid_placements.reserve(placements.size());
  std::set<std::string> added;

  for (const LayoutPlacement& placement : placements)
  {
    if (!FindResizableSection(placement.key) || placement.width <= 0 || placement.height <= 0 ||
        !added.insert(placement.key).second)
      continue;
    valid_placements.push_back(placement);
  }
  for (const ResizableSection& section : m_resizable_sections)
  {
    if (added.contains(section.key))
      continue;
    const QRect geometry = section.widget->geometry();
    valid_placements.push_back(
        {section.key, geometry.x(), geometry.y(), geometry.width(), geometry.height()});
  }

  if (valid_placements.empty())
  {
    if (m_default_layout_builder)
      ReplaceContentLayout(m_default_layout_builder());
    return;
  }

  for (ResizableSection& section : m_resizable_sections)
    section.resizer->UseResponsiveSize();

  auto* responsive_layout = new ProportionalTASLayout(baseline_size);
  for (const LayoutPlacement& placement : valid_placements)
  {
    ResizableSection* section = FindResizableSection(placement.key);
    responsive_layout->AddWidget(
        section->widget,
        QRect(placement.x, placement.y, placement.width, placement.height));
  }

  ReplaceContentLayout(responsive_layout);
  resize(current_size.expandedTo(minimumSize()));
}

std::optional<QRect> TASInputWindow::HandleSectionGeometry(QWidget* widget, const QRect& geometry,
                                                           SectionGeometryOperation operation,
                                                           Qt::Edges resize_edges, bool commit)
{
  if (!m_rearrange_enabled || !widget || geometry.width() <= 0 || geometry.height() <= 0)
    return std::nullopt;

  const auto active_section_it = std::ranges::find_if(
      m_resizable_sections,
      [widget](const ResizableSection& section) { return section.widget == widget; });
  if (active_section_it == m_resizable_sections.end())
    return std::nullopt;
  ResizableSection* active_section = &*active_section_it;

  const QRect current_geometry = widget->geometry();
  const bool resizing = operation == SectionGeometryOperation::Resize;
  const bool resizing_horizontally =
      resizing && (resize_edges.testFlag(Qt::LeftEdge) || resize_edges.testFlag(Qt::RightEdge));
  const bool resizing_vertically =
      resizing && (resize_edges.testFlag(Qt::TopEdge) || resize_edges.testFlag(Qt::BottomEdge));
  QRect adjusted = geometry;

  if (resizing_horizontally || resizing_vertically)
  {
    if (resize_edges.testFlag(Qt::LeftEdge))
    {
      const int maximum_left = adjusted.right() - active_section->resizer->MinimumWidth() + 1;
      adjusted.setLeft(std::min(maximum_left, SnapToLayoutGrid(adjusted.left())));
    }
    else if (resize_edges.testFlag(Qt::RightEdge))
    {
      const int minimum_right = adjusted.left() + active_section->resizer->MinimumWidth() - 1;
      adjusted.setRight(std::max(minimum_right, SnapToLayoutGrid(adjusted.right() + 1) - 1));
    }
    if (resize_edges.testFlag(Qt::TopEdge))
    {
      const int maximum_top = adjusted.bottom() - active_section->resizer->MinimumHeight() + 1;
      adjusted.setTop(std::min(maximum_top, SnapToLayoutGrid(adjusted.top())));
    }
    else if (resize_edges.testFlag(Qt::BottomEdge))
    {
      const int minimum_bottom = adjusted.top() + active_section->resizer->MinimumHeight() - 1;
      adjusted.setBottom(std::max(minimum_bottom, SnapToLayoutGrid(adjusted.bottom() + 1) - 1));
    }
  }
  else
  {
    adjusted.moveLeft(SnapToLayoutGrid(adjusted.left()));
    adjusted.moveTop(SnapToLayoutGrid(adjusted.top()));
  }

  const QRect available = rect().adjusted(LAYOUT_EDGE_MARGIN, LAYOUT_EDGE_MARGIN,
                                          -LAYOUT_EDGE_MARGIN, -LAYOUT_EDGE_MARGIN);
  if (resize_edges.testFlag(Qt::LeftEdge))
    adjusted.setLeft(std::max(adjusted.left(), available.left()));
  else if (resize_edges.testFlag(Qt::RightEdge))
    adjusted.setRight(std::min(adjusted.right(), available.right()));
  else
    adjusted.moveLeft(std::clamp(
        adjusted.left(), available.left(),
        std::max(available.left(), available.right() - adjusted.width() + 1)));
  if (resize_edges.testFlag(Qt::TopEdge))
    adjusted.setTop(std::max(adjusted.top(), available.top()));
  else if (resize_edges.testFlag(Qt::BottomEdge))
    adjusted.setBottom(std::min(adjusted.bottom(), available.bottom()));
  else
    adjusted.moveTop(std::clamp(
        adjusted.top(), available.top(),
        std::max(available.top(), available.bottom() - adjusted.height() + 1)));

  if (!resizing_horizontally && !resizing_vertically)
  {
    ResizableSection* swap_section = nullptr;
    qsizetype largest_overlap = 0;
    for (ResizableSection& section : m_resizable_sections)
    {
      if (section.widget == widget || !section.widget->isVisible())
        continue;
      const QRect intersection = adjusted.intersected(section.widget->geometry());
      const qsizetype overlap = static_cast<qsizetype>(intersection.width()) * intersection.height();
      if (section.widget->geometry().contains(adjusted.center()) && overlap > largest_overlap)
      {
        swap_section = &section;
        largest_overlap = overlap;
      }
    }

    if (swap_section)
    {
      const QRect destination = swap_section->widget->geometry();
      if (commit)
      {
        swap_section->resizer->SetCustomSizeExact(current_geometry.width(),
                                                  current_geometry.height());
        swap_section->widget->setGeometry(current_geometry);
        widget->setGeometry(destination);
      }
      return destination;
    }
  }

  for (const ResizableSection& section : m_resizable_sections)
  {
    if (section.widget == widget || !section.widget->isVisible())
      continue;

    const QRect other = section.widget->geometry();
    if (resize_edges.testFlag(Qt::RightEdge) && adjusted.top() <= other.bottom() &&
        adjusted.bottom() >= other.top() && current_geometry.right() < other.left() &&
        adjusted.right() + LAYOUT_SECTION_SPACING >= other.left())
    {
      adjusted.setRight(other.left() - LAYOUT_SECTION_SPACING - 1);
    }
    if (resize_edges.testFlag(Qt::LeftEdge) && adjusted.top() <= other.bottom() &&
        adjusted.bottom() >= other.top() && current_geometry.left() > other.right() &&
        adjusted.left() - LAYOUT_SECTION_SPACING <= other.right())
    {
      adjusted.setLeft(other.right() + LAYOUT_SECTION_SPACING + 1);
    }
    if (resize_edges.testFlag(Qt::BottomEdge) && adjusted.left() <= other.right() &&
        adjusted.right() >= other.left() && current_geometry.bottom() < other.top() &&
        adjusted.bottom() + LAYOUT_SECTION_SPACING >= other.top())
    {
      adjusted.setBottom(other.top() - LAYOUT_SECTION_SPACING - 1);
    }
    if (resize_edges.testFlag(Qt::TopEdge) && adjusted.left() <= other.right() &&
        adjusted.right() >= other.left() && current_geometry.top() > other.bottom() &&
        adjusted.top() - LAYOUT_SECTION_SPACING <= other.bottom())
    {
      adjusted.setTop(other.bottom() + LAYOUT_SECTION_SPACING + 1);
    }
  }

  if (adjusted.width() < active_section->resizer->MinimumWidth() ||
      adjusted.height() < active_section->resizer->MinimumHeight())
  {
    return std::nullopt;
  }

  const QRect collision_geometry = adjusted.adjusted(
      -LAYOUT_SECTION_SPACING, -LAYOUT_SECTION_SPACING, LAYOUT_SECTION_SPACING,
      LAYOUT_SECTION_SPACING);
  std::vector<std::pair<ResizableSection*, QRect>> neighbor_adjustments;
  std::vector<ResizableSection*> displaced_sections;
  for (ResizableSection& section : m_resizable_sections)
  {
    if (section.widget == widget || !section.widget->isVisible() ||
        !collision_geometry.intersects(section.widget->geometry()))
    {
      continue;
    }

    if (resizing_horizontally || resizing_vertically)
      return std::nullopt;

    const QRect other = section.widget->geometry();
    const int minimum_width = section.resizer->MinimumWidth();
    const int minimum_height = section.resizer->MinimumHeight();
    std::vector<QRect> candidates;
    candidates.emplace_back(other.left(), other.top(),
                            adjusted.left() - LAYOUT_SECTION_SPACING - other.left(),
                            other.height());
    candidates.emplace_back(adjusted.right() + LAYOUT_SECTION_SPACING + 1, other.top(),
                            other.right() - adjusted.right() - LAYOUT_SECTION_SPACING,
                            other.height());
    candidates.emplace_back(other.left(), other.top(), other.width(),
                            adjusted.top() - LAYOUT_SECTION_SPACING - other.top());
    candidates.emplace_back(other.left(), adjusted.bottom() + LAYOUT_SECTION_SPACING + 1,
                            other.width(),
                            other.bottom() - adjusted.bottom() - LAYOUT_SECTION_SPACING);

    const QRect* best_candidate = nullptr;
    int best_area = 0;
    for (const QRect& candidate : candidates)
    {
      if (candidate.width() < minimum_width || candidate.height() < minimum_height)
        continue;
      const int area = candidate.width() * candidate.height();
      if (area > best_area)
      {
        best_candidate = &candidate;
        best_area = area;
      }
    }
    if (best_candidate)
    {
      neighbor_adjustments.emplace_back(&section, *best_candidate);
      continue;
    }

    displaced_sections.push_back(&section);
  }

  if (!displaced_sections.empty())
  {
    if (collision_geometry.intersects(current_geometry))
      return std::nullopt;

    const auto pack_displaced = [&](bool horizontally)
        -> std::optional<std::vector<QRect>> {
      const int count = static_cast<int>(displaced_sections.size());
      const int total_spacing = LAYOUT_SECTION_SPACING * (count - 1);
      int required_primary = total_spacing;
      for (const ResizableSection* section : displaced_sections)
      {
        required_primary += horizontally ? section->resizer->MinimumWidth() :
                                           section->resizer->MinimumHeight();
        const int cross_minimum = horizontally ? section->resizer->MinimumHeight() :
                                                 section->resizer->MinimumWidth();
        const int cross_available =
            horizontally ? current_geometry.height() : current_geometry.width();
        if (cross_available < cross_minimum)
          return std::nullopt;
      }

      const int primary_available =
          horizontally ? current_geometry.width() : current_geometry.height();
      if (primary_available < required_primary)
        return std::nullopt;

      int cursor = horizontally ? current_geometry.left() : current_geometry.top();
      int remaining_extra = primary_available - required_primary;
      std::vector<QRect> packed;
      packed.reserve(displaced_sections.size());
      for (int i = 0; i < count; ++i)
      {
        const ResizableSection* section = displaced_sections[i];
        const int minimum = horizontally ? section->resizer->MinimumWidth() :
                                           section->resizer->MinimumHeight();
        const int extra = remaining_extra / (count - i);
        const int extent = minimum + extra;
        remaining_extra -= extra;
        if (horizontally)
          packed.emplace_back(cursor, current_geometry.top(), extent, current_geometry.height());
        else
          packed.emplace_back(current_geometry.left(), cursor, current_geometry.width(), extent);
        cursor += extent + LAYOUT_SECTION_SPACING;
      }
      return packed;
    };

    const bool prefer_horizontal = current_geometry.width() >= current_geometry.height();
    std::optional<std::vector<QRect>> packed = pack_displaced(prefer_horizontal);
    if (!packed)
      packed = pack_displaced(!prefer_horizontal);
    if (!packed)
      return std::nullopt;
    for (std::size_t i = 0; i < displaced_sections.size(); ++i)
      neighbor_adjustments.emplace_back(displaced_sections[i], (*packed)[i]);
  }

  if (commit)
  {
    for (const auto& [section, new_geometry] : neighbor_adjustments)
    {
      section->resizer->SetCustomSize(new_geometry.width(), new_geometry.height());
      section->widget->setGeometry(new_geometry);
    }
    widget->setGeometry(adjusted);
  }
  return adjusted;
}

void TASInputWindow::UpdateFreeformMinimumSize()
{
  int required_width = 160;
  int required_height = 120;
  for (const ResizableSection& section : m_resizable_sections)
  {
    if (!section.widget->isVisible())
      continue;
    required_width =
        std::max(required_width, section.widget->geometry().right() + LAYOUT_EDGE_MARGIN + 1);
    required_height =
        std::max(required_height, section.widget->geometry().bottom() + LAYOUT_EDGE_MARGIN + 1);
  }
  setMinimumSize(required_width, required_height);
}

void TASInputWindow::SetRearrangeEnabled(bool enabled)
{
  if (enabled == m_rearrange_enabled)
    return;

  if (enabled)
  {
    if (layout())
      EnterFreeformLayout(CaptureCurrentLayout());
    m_has_custom_layout = true;
    ApplyVisibilitySettings();
    UpdateFreeformMinimumSize();
  }
  else
  {
    const std::vector<LayoutPlacement> placements = CaptureCurrentLayout();
    BuildResponsiveLayout(placements, size());
    ApplyVisibilitySettings();
  }

  m_rearrange_enabled = enabled;
  for (ResizableSection& section : m_resizable_sections)
    section.resizer->SetRearrangeEnabled(enabled);
  SaveLayoutState();
}

void TASInputWindow::ResetLayout()
{
  m_rearrange_enabled = false;
  m_has_custom_layout = false;
  for (ResizableSection& section : m_resizable_sections)
  {
    section.resizer->SetRearrangeEnabled(false);
    section.resizer->ClearCustomSize();
  }
  if (m_default_layout_builder)
    ReplaceContentLayout(m_default_layout_builder());
  ApplyVisibilitySettings();

  if (!m_layout_config_key.empty())
  {
    Common::IniFile ini;
    const std::string ini_path = GetDolphinIniPath();
    ini.Load(ini_path);
    ini.DeleteKey(TAS_WINDOW_LAYOUT_SECTION, m_layout_config_key);
    ini.Save(ini_path);
  }
  resize(sizeHint());
}

void TASInputWindow::SaveLayoutState() const
{
  if (!m_layout_finalized || m_layout_config_key.empty())
    return;

  Common::IniFile ini;
  const std::string ini_path = GetDolphinIniPath();
  ini.Load(ini_path);
  const std::string state = GetLayoutState();
  if (state == "default;")
    ini.DeleteKey(TAS_WINDOW_LAYOUT_SECTION, m_layout_config_key);
  else
    ini.GetOrCreateSection(TAS_WINDOW_LAYOUT_SECTION)->Set(m_layout_config_key, state);
  ini.Save(ini_path);
}

std::string TASInputWindow::LoadLayoutState() const
{
  if (m_layout_config_key.empty())
    return {};

  std::string state;
  Common::IniFile ini;
  ini.Load(GetDolphinIniPath());
  ini.GetIfExists(TAS_WINDOW_LAYOUT_SECTION, m_layout_config_key, &state);
  return state;
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

void TASInputWindow::SetLayoutConfigKey(std::string key)
{
  m_layout_config_key = std::move(key);
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

void TASInputWindow::FinalizeLayoutSections()
{
  const std::string state = LoadLayoutState();
  if (!state.empty())
    ApplyLayoutState(state, true);
  m_layout_finalized = true;
  if (!state.empty())
    SaveLayoutState();
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

  auto* view_movie_inputs = menu.addAction(tr("View Movie Inputs"));
  view_movie_inputs->setCheckable(true);
  view_movie_inputs->setChecked(Config::Get(Config::MAIN_MOVIE_VIEW_TAS_INPUTS));
  connect(view_movie_inputs, &QAction::toggled, this, [](bool value) {
    Config::SetBaseOrCurrent(Config::MAIN_MOVIE_VIEW_TAS_INPUTS, value);
  });

  auto* turbo_visualizer = menu.addAction(tr("Turbo Visualizer"));
  turbo_visualizer->setCheckable(true);
  turbo_visualizer->setChecked(Config::Get(Config::MAIN_MOVIE_TURBO_VISUALIZER));
  connect(turbo_visualizer, &QAction::toggled, this,
          [](bool value) { Config::SetBaseOrCurrent(Config::MAIN_MOVIE_TURBO_VISUALIZER, value); });

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

  if (!m_resizable_sections.empty())
  {
    menu.addSeparator();
    auto* rearrange = menu.addAction(tr("Rearrange"));
    rearrange->setCheckable(true);
    rearrange->setChecked(m_rearrange_enabled);
    connect(rearrange, &QAction::toggled, this, &TASInputWindow::SetRearrangeEnabled);

    auto* reset_layout = menu.addAction(tr("Reset Layout"));
    connect(reset_layout, &QAction::triggered, this, &TASInputWindow::ResetLayout);
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
  ini.GetOrCreateSection(TAS_WINDOW_OPTIONS_SECTION)
      ->Set(GetAlwaysOnTopConfigKey(), enabled, false);
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
  else if (m_has_custom_layout)
  {
    UpdateFreeformMinimumSize();
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

void TASInputWindow::resizeEvent(QResizeEvent* event)
{
  QDialog::resizeEvent(event);
  if (m_layout_finalized && m_has_custom_layout && m_layout_save_timer)
    m_layout_save_timer->start();
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
