// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLook2DWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/FreeLookSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Graphics/GraphicsChoice.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/Settings.h"

FreeLook2DWidget::FreeLook2DWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void FreeLook2DWidget::CreateLayout()
{
  auto* layout = new QVBoxLayout();
  layout->setStretch(0, 0);

  m_enable_freelook = new ToolTipCheckBox(tr("Enable"));
  m_enable_freelook->setChecked(Config::Get(Config::FREE_LOOK_2D_ENABLED));
  m_enable_freelook->SetDescription(tr("Allows manipulation of the in-game camera for 2d "
                                       "elements.<br><br><dolphin_emphasis>If unsure, "
                                       "leave this unchecked.</dolphin_emphasis>"));
  m_freelook_controller_configure_button = new QPushButton(tr("Configure Controller"));

  auto* hlayout = new QHBoxLayout();
  hlayout->addWidget(new QLabel(tr("Camera 1")));
  hlayout->addWidget(m_freelook_controller_configure_button);

  layout->addWidget(m_enable_freelook);
  layout->addLayout(hlayout);
  layout->insertStretch(-1, 1);

  setLayout(layout);
}

void FreeLook2DWidget::ConnectWidgets()
{
  connect(m_freelook_controller_configure_button, &QPushButton::clicked, this,
          &FreeLook2DWidget::OnFreeLookControllerConfigured);
  connect(m_enable_freelook, &QCheckBox::clicked, this, &FreeLook2DWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    const QSignalBlocker blocker(this);
    LoadSettings();
  });
}

void FreeLook2DWidget::OnFreeLookControllerConfigured()
{
  if (m_freelook_controller_configure_button != QObject::sender())
    return;
  const int index = 0;
  MappingWindow* window = new MappingWindow(this, MappingWindow::Type::MAPPING_FREELOOK_2D, index);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void FreeLook2DWidget::LoadSettings()
{
  const bool checked = Config::Get(Config::FREE_LOOK_2D_ENABLED);
  m_enable_freelook->setChecked(checked);
  m_freelook_controller_configure_button->setEnabled(checked);
}

void FreeLook2DWidget::SaveSettings()
{
  const bool checked = m_enable_freelook->isChecked();
  Config::SetBaseOrCurrent(Config::FREE_LOOK_2D_ENABLED, checked);
  m_freelook_controller_configure_button->setEnabled(checked);
}
