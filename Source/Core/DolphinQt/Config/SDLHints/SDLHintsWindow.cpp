// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SDLHints/SDLHintsWindow.h"

#include "Common/Config/Config.h"

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

SDLHintsWindow::SDLHintsWindow(QWidget* parent) : QDialog(parent)
{
  CreateMainLayout();

  setWindowTitle(tr("SDL Controller Settings"));
}

QSize SDLHintsWindow::sizeHint() const
{
  return {450, 0};
}

void SDLHintsWindow::CreateMainLayout()
{
  setMinimumWidth(300);
  setMinimumHeight(270);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &SDLHintsWindow::OnClose);

  // Create hints table
  m_hints_table = new QTableWidget(0, 2);
  QHeaderView* const hints_table_header = m_hints_table->horizontalHeader();
  m_hints_table->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
  m_hints_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_hints_table->setSelectionMode(QTableWidget::SingleSelection);
  hints_table_header->setSectionResizeMode(0, QHeaderView::Interactive);
  hints_table_header->setSectionResizeMode(1, QHeaderView::Fixed);
  hints_table_header->setMinimumSectionSize(60);
  m_hints_table->verticalHeader()->setVisible(false);
  hints_table_header->installEventFilter(this);
  QObject::connect(hints_table_header, &QHeaderView::sectionResized, this,
                   &SDLHintsWindow::SectionResized);

  PopulateTable();

  // Create table buttons
  auto* const add_row_btn = new NonDefaultQPushButton(tr("Add"));
  connect(add_row_btn, &QPushButton::pressed, this, &SDLHintsWindow::AddRow);

  m_rem_row_btn = new NonDefaultQPushButton(tr("Remove"));
  m_rem_row_btn->setEnabled(false);
  connect(m_rem_row_btn, &QPushButton::pressed, this, &SDLHintsWindow::RemoveRow);
  connect(m_hints_table, &QTableWidget::itemSelectionChanged, this,
          &SDLHintsWindow::SelectionChanged);

  auto* const btns_layout = new QDialogButtonBox;
  btns_layout->setContentsMargins(0, 0, 5, 5);
  btns_layout->addButton(add_row_btn, QDialogButtonBox::ActionRole);
  btns_layout->addButton(m_rem_row_btn, QDialogButtonBox::ActionRole);

  // Create advanced tab
  auto* advanced_layout = new QVBoxLayout();
  advanced_layout->addWidget(m_hints_table);
  advanced_layout->addWidget(btns_layout);

  auto* advanced_frame = new QFrame();
  advanced_frame->setLayout(advanced_layout);

  // Create default tab
  m_directinput_detection = new ToolTipCheckBox(tr("Enable DirectInput Detection"));
  m_directinput_detection->SetDescription(
      tr("Controls whether SDL should use DirectInput for detecting controllers. Enabling this "
         "fixes hotplug detection issues with DualSense controllers but causes Dolphin to hang up "
         "on shutdown when using certain 8BitDo controllers.<br><br><dolphin_emphasis>If unsure, "
         "leave this checked.</dolphin_emphasis>"));
  connect(m_directinput_detection, &ToolTipCheckBox::toggled, [](bool checked) {
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_DIRECTINPUT, checked ? "1" : "0");
  });

  m_combine_joy_cons = new ToolTipCheckBox(tr("Use Joy-Con Pairs as a Single Controller"));
  m_combine_joy_cons->SetDescription(
      tr("Controls whether SDL should treat a pair of Joy-Con as a single controller or as two "
         "separate controllers.<br><br><dolphin_emphasis>If unsure, leave this "
         "checked.</dolphin_emphasis>"));
  connect(m_combine_joy_cons, &ToolTipCheckBox::toggled, [](bool checked) {
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, checked ? "1" : "0");
  });

  m_horizontal_joy_cons = new ToolTipCheckBox(tr("Sideways Joy-Con"));
  m_horizontal_joy_cons->SetDescription(
      tr("Defines the default orientation for individual Joy-Con. This setting has no effect when "
         "Use Joy-Con Pairs as a Single Controller is "
         "enabled.<br><br><dolphin_emphasis>If unsure, "
         "leave this checked.</dolphin_emphasis>"));
  connect(m_horizontal_joy_cons, &ToolTipCheckBox::toggled, [](bool checked) {
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, checked ? "0" : "1");
  });

  m_dualsense_player_led = new ToolTipCheckBox(tr("Enable DualSense Player LEDs"));
  m_dualsense_player_led->SetDescription(
      tr("Controls whether the player LEDs should be lit to indicate which player is associated "
         "with a DualSense controller.<br><br><dolphin_emphasis>If unsure, leave this "
         "unchecked.</dolphin_emphasis>"));
  connect(m_dualsense_player_led, &ToolTipCheckBox::toggled, [](bool checked) {
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, checked ? "1" : "0");
  });

  auto* const default_layout = new QVBoxLayout();
  default_layout->setContentsMargins(10, 10, 10, 10);
  default_layout->addWidget(m_directinput_detection);
  default_layout->addWidget(m_combine_joy_cons);
  default_layout->addWidget(m_horizontal_joy_cons);
  default_layout->addWidget(m_dualsense_player_led);
  default_layout->addStretch(1);

  auto* const default_frame = new QFrame();
  default_frame->setLayout(default_layout);

  PopulateChecklist();

  // Create the tab widget
  m_tab_widget = new QTabWidget();
  m_tab_widget->addTab(default_frame, tr("Main"));
  m_tab_widget->addTab(advanced_frame, tr("Advanced"));

  m_current_tab_index = 0;
  m_tab_widget->setCurrentIndex(m_current_tab_index);

  connect(m_tab_widget, &QTabWidget::currentChanged, this, &SDLHintsWindow::TabChanged);

  auto* const warning_text =
      new QLabel(tr("Dolphin must be restarted for these changes to take effect."));
  warning_text->setWordWrap(true);

  // Create main layout
  auto* const main_layout = new QVBoxLayout();
  main_layout->addWidget(
      QtUtils::CreateIconWarning(this, QStyle::SP_MessageBoxWarning, warning_text), 0);
  main_layout->addWidget(m_tab_widget, 1);
  main_layout->addWidget(m_button_box, 0, Qt::AlignBottom | Qt::AlignRight);
  setLayout(main_layout);
}

void SDLHintsWindow::PopulateTable()
{
  m_hints_table->setRowCount(0);

  // Loop through all the values in the SDL_Hints settings section and load them into the table
  std::shared_ptr<Config::Layer> layer = Config::GetLayer(Config::LayerType::Base);
  const Config::Section& section = layer->GetSection(Config::System::Main, "SDL_Hints");
  for (auto& row_data : section)
  {
    const Config::Location& location = row_data.first;
    const std::optional<std::string>& value = row_data.second;

    if (value)
    {
      m_hints_table->insertRow(m_hints_table->rowCount());
      m_hints_table->setItem(m_hints_table->rowCount() - 1, 0,
                             new QTableWidgetItem(QString::fromStdString(location.key)));
      m_hints_table->setItem(m_hints_table->rowCount() - 1, 1,
                             new QTableWidgetItem(QString::fromStdString(*value)));
    }
  }
}

void SDLHintsWindow::SaveTable()
{
  // Clear all the old values from the SDL_Hints section
  std::shared_ptr<Config::Layer> layer = Config::GetLayer(Config::LayerType::Base);
  Config::Section section = layer->GetSection(Config::System::Main, "SDL_Hints");

  for (auto& row_data : section)
    row_data.second.reset();

  // Add each item still in the table to the config file
  for (int row = 0; row < m_hints_table->rowCount(); ++row)
  {
    QTableWidgetItem* hint_name_item = m_hints_table->item(row, 0);
    QTableWidgetItem* hint_value_item = m_hints_table->item(row, 1);

    if (hint_name_item != nullptr && hint_value_item != nullptr)
    {
      const QString& hint_name = hint_name_item->text().trimmed();
      const QString& hint_value = hint_value_item->text().trimmed();

      if (!hint_name.isEmpty() && !hint_value.isEmpty())
      {
        const Config::Info<std::string> setting{
            {Config::System::Main, "SDL_Hints", hint_name.toStdString()}, ""};
        Config::SetBase(setting, hint_value.toStdString());
      }
    }
  }
}

void SDLHintsWindow::PopulateChecklist()
{
  // Populate the checklist and default to the SDL default for an invalid value

  // Default to checked if incorrectly set
  SignalBlocking(m_directinput_detection)
      ->setChecked(Config::GetBase(Config::MAIN_SDL_HINT_JOYSTICK_DIRECTINPUT) != "0");

  // Default to checked if incorrectly set
  SignalBlocking(m_combine_joy_cons)
      ->setChecked(Config::GetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS) != "0");

  // Default to checked if incorrectly set
  SignalBlocking(m_horizontal_joy_cons)
      ->setChecked(Config::GetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS) != "1");

  // Default to checked if incorrectly set
  SignalBlocking(m_dualsense_player_led)
      ->setChecked(Config::GetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED) != "0");
}

void SDLHintsWindow::AddRow()
{
  m_hints_table->insertRow(m_hints_table->rowCount());
  m_hints_table->scrollToBottom();
}

void SDLHintsWindow::RemoveRow()
{
  QModelIndex index = m_hints_table->selectionModel()->currentIndex();
  m_hints_table->removeRow(index.row());
}

void SDLHintsWindow::SelectionChanged()
{
  m_rem_row_btn->setEnabled(m_hints_table->selectionModel()->hasSelection());
}

void SDLHintsWindow::OnClose()
{
  TabChanged(-1);  // Pass -1 to indicate exit

  reject();
}

void SDLHintsWindow::TabChanged(int new_index)
{
  // Check which tab we're coming from, cur_tab_idx has not been updated yet
  switch (m_current_tab_index)
  {
  case 1:  // Coming from the advanced tab
    SaveTable();
    break;

  default:
    break;
  }

  // Check which tab we're going to
  switch (new_index)
  {
  case 0:  // Going to the main tab
    PopulateChecklist();
    break;

  case 1:  // Going to the advanced tab
    PopulateTable();
    break;

  default:
    break;
  }

  m_current_tab_index = new_index;
}

void SDLHintsWindow::SectionResized(int logical_index, int old_size, int new_size)
{
  if (logical_index == 0 && old_size != new_size)
  {
    QHeaderView* const header = m_hints_table->horizontalHeader();
    header->setMaximumSectionSize(header->size().width() - header->minimumSectionSize());
    header->resizeSection(1, header->size().width() - new_size);
  }
}

bool SDLHintsWindow::eventFilter(QObject* obj, QEvent* event)
{
  auto* const table_widget = qobject_cast<QHeaderView*>(obj);
  if (table_widget)
  {
    if (event->type() == QEvent::Resize)
    {
      auto* const resize_event = static_cast<QResizeEvent*>(event);
      QHeaderView* header = m_hints_table->horizontalHeader();
      header->setMaximumSectionSize(resize_event->size().width() - header->minimumSectionSize());
      header->resizeSection(0, resize_event->size().width() - header->sectionSize(1));
    }
  }

  return QDialog::eventFilter(obj, event);
}
