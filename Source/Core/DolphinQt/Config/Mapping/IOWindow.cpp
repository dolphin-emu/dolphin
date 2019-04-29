// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/IOWindow.h"

#include <thread>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

constexpr int SLIDER_TICK_COUNT = 100;

IOWindow::IOWindow(QWidget* parent, ControllerEmu::EmulatedController* controller,
                   ControlReference* ref, IOWindow::Type type)
    : QDialog(parent), m_reference(ref), m_controller(controller), m_type(type)
{
  CreateMainLayout();
  ConnectWidgets();

  setWindowTitle(type == IOWindow::Type::Input ? tr("Configure Input") : tr("Configure Output"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ConfigChanged();
}

void IOWindow::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();

  m_devices_combo = new QComboBox();
  m_option_list = new QListWidget();
  m_select_button = new QPushButton(tr("Select"));
  m_detect_button = new QPushButton(tr("Detect"));
  m_or_button = new QPushButton(tr("| OR"));
  m_and_button = new QPushButton(tr("&& AND"));
  m_add_button = new QPushButton(tr("+ ADD"));
  m_not_button = new QPushButton(tr("! NOT"));
  m_test_button = new QPushButton(tr("Test"));
  m_expression_text = new QPlainTextEdit();
  m_button_box = new QDialogButtonBox();
  m_clear_button = new QPushButton(tr("Clear"));
  m_apply_button = new QPushButton(tr("Apply"));
  m_range_slider = new QSlider(Qt::Horizontal);
  m_range_spinbox = new QSpinBox();

  // Devices
  m_main_layout->addWidget(m_devices_combo);

  // Range
  auto* range_hbox = new QHBoxLayout();
  range_hbox->addWidget(new QLabel(tr("Range")));
  range_hbox->addWidget(m_range_slider);
  range_hbox->addWidget(m_range_spinbox);
  m_range_slider->setMinimum(-500);
  m_range_slider->setMaximum(500);
  m_range_spinbox->setMinimum(-500);
  m_range_spinbox->setMaximum(500);
  m_main_layout->addLayout(range_hbox);

  // Options (Buttons, Outputs) and action buttons
  // macOS style doesn't support expanding buttons
#ifndef __APPLE__
  for (QPushButton* button : {m_select_button, m_detect_button, m_or_button, m_and_button,
                              m_add_button, m_not_button, m_test_button})
  {
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }
#endif

  auto* hbox = new QHBoxLayout();
  auto* button_vbox = new QVBoxLayout();
  hbox->addWidget(m_option_list, 8);
  hbox->addLayout(button_vbox, 1);

  button_vbox->addWidget(m_select_button);
  button_vbox->addWidget(m_type == Type::Input ? m_detect_button : m_test_button);
  button_vbox->addWidget(m_or_button);

  if (m_type == Type::Input)
  {
    button_vbox->addWidget(m_and_button);
    button_vbox->addWidget(m_add_button);
    button_vbox->addWidget(m_not_button);
  }

  m_main_layout->addLayout(hbox, 2);
  m_main_layout->addWidget(m_expression_text, 1);

  // Button Box
  m_main_layout->addWidget(m_button_box);
  m_button_box->addButton(m_clear_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(m_apply_button, QDialogButtonBox::ActionRole);
  m_button_box->addButton(QDialogButtonBox::Ok);

  setLayout(m_main_layout);
}

void IOWindow::ConfigChanged()
{
  const QSignalBlocker blocker(this);

  m_expression_text->setPlainText(QString::fromStdString(m_reference->GetExpression()));
  m_expression_text->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
  m_range_spinbox->setValue(m_reference->range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_reference->range * SLIDER_TICK_COUNT);

  m_devq = m_controller->GetDefaultDevice();

  UpdateDeviceList();
  UpdateOptionList();
}

void IOWindow::ConnectWidgets()
{
  connect(m_select_button, &QPushButton::clicked, [this] { AppendSelectedOption(""); });
  connect(m_add_button, &QPushButton::clicked, [this] { AppendSelectedOption(" + "); });
  connect(m_and_button, &QPushButton::clicked, [this] { AppendSelectedOption(" & "); });
  connect(m_or_button, &QPushButton::clicked, [this] { AppendSelectedOption(" | "); });
  connect(m_not_button, &QPushButton::clicked, [this] { AppendSelectedOption("!"); });

  connect(m_detect_button, &QPushButton::clicked, this, &IOWindow::OnDetectButtonPressed);
  connect(m_test_button, &QPushButton::clicked, this, &IOWindow::OnTestButtonPressed);

  connect(m_button_box, &QDialogButtonBox::clicked, this, &IOWindow::OnDialogButtonPressed);
  connect(m_devices_combo, &QComboBox::currentTextChanged, this, &IOWindow::OnDeviceChanged);
  connect(m_range_spinbox, static_cast<void (QSpinBox::*)(int value)>(&QSpinBox::valueChanged),
          this, &IOWindow::OnRangeChanged);
  connect(m_range_slider, static_cast<void (QSlider::*)(int value)>(&QSlider::valueChanged), this,
          &IOWindow::OnRangeChanged);
}

void IOWindow::AppendSelectedOption(const std::string& prefix)
{
  if (m_option_list->currentItem() == nullptr)
    return;

  m_expression_text->insertPlainText(
      QString::fromStdString(prefix) +
      MappingCommon::GetExpressionForControl(m_option_list->currentItem()->text(), m_devq,
                                             m_controller->GetDefaultDevice()));
}

void IOWindow::OnDeviceChanged(const QString& device)
{
  m_devq.FromString(device.toStdString());
  UpdateOptionList();
}

void IOWindow::OnDialogButtonPressed(QAbstractButton* button)
{
  if (button == m_clear_button)
  {
    m_expression_text->clear();
    return;
  }

  m_reference->SetExpression(m_expression_text->toPlainText().toStdString());
  m_controller->UpdateReferences(g_controller_interface);

  if (button != m_apply_button)
    accept();
}

void IOWindow::OnDetectButtonPressed()
{
  const auto expression =
      MappingCommon::DetectExpression(m_detect_button, g_controller_interface, {m_devq.ToString()},
                                      m_devq, MappingCommon::Quote::Off);

  if (expression.isEmpty())
    return;

  const auto list = m_option_list->findItems(expression, Qt::MatchFixedString);

  if (!list.empty())
    m_option_list->setCurrentItem(list[0]);
}

void IOWindow::OnTestButtonPressed()
{
  MappingCommon::TestOutput(m_test_button, static_cast<OutputReference*>(m_reference));
}

void IOWindow::OnRangeChanged(int value)
{
  m_reference->range = static_cast<double>(value) / SLIDER_TICK_COUNT;
  m_range_spinbox->setValue(m_reference->range * SLIDER_TICK_COUNT);
  m_range_slider->setValue(m_reference->range * SLIDER_TICK_COUNT);
}

void IOWindow::UpdateOptionList()
{
  m_option_list->clear();

  const auto device = g_controller_interface.FindDevice(m_devq);

  if (device == nullptr)
    return;

  if (m_reference->IsInput())
  {
    for (const auto* input : device->Inputs())
    {
      m_option_list->addItem(QString::fromStdString(input->GetName()));
    }
  }
  else
  {
    for (const auto* output : device->Outputs())
    {
      m_option_list->addItem(QString::fromStdString(output->GetName()));
    }
  }
}

void IOWindow::UpdateDeviceList()
{
  m_devices_combo->clear();

  for (const auto& name : g_controller_interface.GetAllDeviceStrings())
    m_devices_combo->addItem(QString::fromStdString(name));

  m_devices_combo->setCurrentText(
      QString::fromStdString(m_controller->GetDefaultDevice().ToString()));
}
