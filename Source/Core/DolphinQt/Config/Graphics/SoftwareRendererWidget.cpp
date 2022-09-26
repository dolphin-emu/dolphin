// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/SoftwareRendererWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"
#include "DolphinQt/Settings.h"

#include "UICommon/VideoUtils.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

SoftwareRendererWidget::SoftwareRendererWidget(GraphicsWindow* parent)
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
  AddDescriptions();
  emit BackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));

  connect(parent, &GraphicsWindow::BackendChanged, [this] { LoadSettings(); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
  OnEmulationStateChanged(Core::GetState() != Core::State::Uninitialized);
}

void SoftwareRendererWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  auto* rendering_box = new QGroupBox(tr("Rendering"));
  auto* rendering_layout = new QGridLayout();
  m_backend_combo = new ToolTipComboBox();

  rendering_box->setLayout(rendering_layout);
  rendering_layout->addWidget(new QLabel(tr("Backend:")), 1, 1);
  rendering_layout->addWidget(m_backend_combo, 1, 2);

  for (const auto& backend : VideoBackendBase::GetAvailableBackends())
    m_backend_combo->addItem(tr(backend->GetDisplayName().c_str()));

  main_layout->addWidget(rendering_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void SoftwareRendererWidget::ConnectWidgets()
{
  connect(m_backend_combo, qOverload<int>(&QComboBox::currentIndexChanged),
          [this](int) { SaveSettings(); });
}

void SoftwareRendererWidget::LoadSettings()
{
  for (const auto& backend : VideoBackendBase::GetAvailableBackends())
  {
    if (backend->GetName() == Config::Get(Config::MAIN_GFX_BACKEND))
    {
      m_backend_combo->setCurrentIndex(
          m_backend_combo->findText(tr(backend->GetDisplayName().c_str())));
    }
  }
}

void SoftwareRendererWidget::SaveSettings()
{
  for (const auto& backend : VideoBackendBase::GetAvailableBackends())
  {
    if (tr(backend->GetDisplayName().c_str()) == m_backend_combo->currentText())
    {
      const auto backend_name = backend->GetName();
      if (backend_name != Config::Get(Config::MAIN_GFX_BACKEND))
        emit BackendChanged(QString::fromStdString(backend_name));
      break;
    }
  }
}

void SoftwareRendererWidget::AddDescriptions()
{
  static const char TR_BACKEND_DESCRIPTION[] = QT_TR_NOOP(
      "Selects what graphics API to use internally.<br>The software renderer is extremely "
      "slow and only useful for debugging, so you'll want to use either Direct3D or "
      "OpenGL. Different games and different GPUs will behave differently on each "
      "backend, so for the best emulation experience it's recommended to try both and "
      "choose the one that's less problematic.<br><br><dolphin_emphasis>If unsure, select "
      "OpenGL.</dolphin_emphasis>");

  m_backend_combo->SetTitle(tr("Backend"));
  m_backend_combo->SetDescription(tr(TR_BACKEND_DESCRIPTION));
}

void SoftwareRendererWidget::OnEmulationStateChanged(bool running)
{
  m_backend_combo->setEnabled(!running);
}
