// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/GraphicsWindow.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/Graphics/AdvancedWidget.h"
#include "DolphinQt/Config/Graphics/EnhancementsWidget.h"
#include "DolphinQt/Config/Graphics/GeneralWidget.h"
#include "DolphinQt/Config/Graphics/HacksWidget.h"
#include "DolphinQt/Config/Graphics/PrimeWidget.h"
#include "DolphinQt/Config/Graphics/SoftwareRendererWidget.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "Core/ConfigManager.h"


GraphicsWindow::GraphicsWindow(X11Utils::XRRConfiguration* xrr_config, MainWindow* parent)
    : QDialog(parent), m_xrr_config(xrr_config)
{
  CreateMainLayout();

  setWindowTitle(tr("Graphics"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  OnBackendChanged(QString::fromStdString(Config::Get(Config::MAIN_GFX_BACKEND)));
}

void GraphicsWindow::CreateMainLayout()
{
  auto* main_layout = new QVBoxLayout();
  m_tab_widget = new QTabWidget();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  main_layout->addWidget(m_tab_widget);
  main_layout->addWidget(m_button_box);

  m_general_widget = new GeneralWidget(m_xrr_config, this);
  m_enhancements_widget = new EnhancementsWidget(this);
  m_hacks_widget = new HacksWidget(this);
  m_prime_widget = new PrimeWidget(this);
  m_advanced_widget = new AdvancedWidget(this);
  m_software_renderer = new SoftwareRendererWidget(this);

  connect(m_general_widget, &GeneralWidget::BackendChanged, this,
          &GraphicsWindow::OnBackendChanged);
  connect(m_software_renderer, &SoftwareRendererWidget::BackendChanged, this,
          &GraphicsWindow::OnBackendChanged);

  m_wrapped_general = GetWrappedWidget(m_general_widget, this, 50, 100);
  m_wrapped_enhancements = GetWrappedWidget(m_enhancements_widget, this, 50, 100);
  m_wrapped_hacks = GetWrappedWidget(m_hacks_widget, this, 50, 100);
  m_wrapped_advanced = GetWrappedWidget(m_advanced_widget, this, 50, 100);
  m_wrapped_software = GetWrappedWidget(m_software_renderer, this, 50, 100);

  m_wrapped_prime = GetWrappedWidget(m_prime_widget, this, 50, 305);
  if (Config::Get(Config::MAIN_GFX_BACKEND) != "Software Renderer")
  {
    m_tab_widget->addTab(m_wrapped_general, tr("General"));
    m_tab_widget->addTab(m_wrapped_enhancements, tr("Enhancements"));
    m_tab_widget->addTab(m_wrapped_hacks, tr("Hacks"));
    m_tab_widget->addTab(m_wrapped_advanced, tr("Advanced"));
    m_tab_widget->addTab(m_wrapped_prime, tr("PrimeHack GFX"));
  }
  else
  {
    m_tab_widget->addTab(m_wrapped_software, tr("Software Renderer"));
  }

  setLayout(main_layout);
}

void GraphicsWindow::OnBackendChanged(const QString& backend_name)
{
  Config::SetBase(Config::MAIN_GFX_BACKEND, backend_name.toStdString());
  VideoBackendBase::PopulateBackendInfoFromUI();

  setWindowTitle(
      tr("%1 Graphics Configuration").arg(tr(g_video_backend->GetDisplayName().c_str())));
  if (backend_name == QStringLiteral("Software Renderer") && m_tab_widget->count() > 1)
  {
    m_tab_widget->clear();
    m_tab_widget->addTab(m_wrapped_software, tr("Software Renderer"));
  }

  if (backend_name != QStringLiteral("Software Renderer") && m_tab_widget->count() == 1)
  {
    m_tab_widget->clear();
    m_tab_widget->addTab(m_wrapped_general, tr("General"));
    m_tab_widget->addTab(m_wrapped_enhancements, tr("Enhancements"));
    m_tab_widget->addTab(m_wrapped_hacks, tr("Hacks"));
    m_tab_widget->addTab(m_wrapped_advanced, tr("Advanced"));
    m_tab_widget->addTab(m_wrapped_prime, tr("PrimeHack Misc"));
  }

  emit BackendChanged(backend_name);
}
