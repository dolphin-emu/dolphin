// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLookOptionsWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

#include "Common/Config/Config.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ToolTipControls/ToolTipDoubleSpinBox.h"
#include "DolphinQt/Settings.h"

FreeLookOptionsWidget::FreeLookOptionsWidget(QWidget* parent,
                                             const ::Config::Info<float>& fov_horizontal,
                                             const ::Config::Info<float>& fov_vertical)
    : QWidget(parent), m_fov_horizontal_config(fov_horizontal), m_fov_vertical_config(fov_vertical)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void FreeLookOptionsWidget::CreateLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(CreateFieldOfViewGroup());
  setLayout(layout);
}

void FreeLookOptionsWidget::ConnectWidgets()
{
  connect(m_fov_horizontal, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &FreeLookOptionsWidget::SaveSettings);
  connect(m_fov_vertical, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &FreeLookOptionsWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    const QSignalBlocker blocker(this);
    LoadSettings();
  });
}

void FreeLookOptionsWidget::LoadSettings()
{
  m_fov_horizontal->setValue(::Config::Get(m_fov_horizontal_config));
  m_fov_vertical->setValue(::Config::Get(m_fov_vertical_config));
}

void FreeLookOptionsWidget::SaveSettings()
{
  ::Config::SetBaseOrCurrent(m_fov_horizontal_config,
                             static_cast<float>(m_fov_horizontal->value()));
  ::Config::SetBaseOrCurrent(m_fov_vertical_config, static_cast<float>(m_fov_vertical->value()));
}

QGroupBox* FreeLookOptionsWidget::CreateFieldOfViewGroup()
{
  QGroupBox* group_box = new QGroupBox(tr("Field of View"));
  QFormLayout* form_layout = new QFormLayout();

  group_box->setLayout(form_layout);

  m_fov_horizontal = new ToolTipDoubleSpinBox;
  m_fov_horizontal->setMinimum(0.025);
  m_fov_horizontal->setSingleStep(0.025);
  m_fov_horizontal->setDecimals(3);
  m_fov_horizontal->SetTitle(tr("Horizontal Multiplier"));
  m_fov_horizontal->SetDescription(
      tr("Multiplier for the field of view in the horizontal direction.<br><br>"));

  m_fov_vertical = new ToolTipDoubleSpinBox;
  m_fov_vertical->setMinimum(0.025);
  m_fov_vertical->setSingleStep(0.025);
  m_fov_vertical->setDecimals(3);
  m_fov_vertical->SetTitle(tr("Vertical Multiplier"));
  m_fov_vertical->SetDescription(
      tr("Multiplier for the field of view in the vertical direction.<br><br>"));

  form_layout->addRow(tr("Horizontal Multiplier:"), m_fov_horizontal);
  form_layout->addRow(tr("Vertical Multiplier:"), m_fov_vertical);

  return group_box;
}
