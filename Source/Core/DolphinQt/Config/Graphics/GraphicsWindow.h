// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QHash>

class AdvancedWidget;
class EnhancementsWidget;
class HacksWidget;
class GeneralWidget;
class MainWindow;
class QLabel;
class QTabWidget;
class QDialogButtonBox;
class SoftwareRendererWidget;

namespace X11Utils
{
class XRRConfiguration;
}

class GraphicsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit GraphicsWindow(X11Utils::XRRConfiguration* xrr_config, MainWindow* parent);

signals:
  void BackendChanged(const QString& backend);

private:
  void CreateMainLayout();
  void OnBackendChanged(const QString& backend);

  QTabWidget* m_tab_widget;
  QDialogButtonBox* m_button_box;

  AdvancedWidget* m_advanced_widget;
  EnhancementsWidget* m_enhancements_widget;
  HacksWidget* m_hacks_widget;
  GeneralWidget* m_general_widget;
  SoftwareRendererWidget* m_software_renderer;

  QWidget* m_wrapped_advanced;
  QWidget* m_wrapped_enhancements;
  QWidget* m_wrapped_hacks;
  QWidget* m_wrapped_general;
  QWidget* m_wrapped_software;

  X11Utils::XRRConfiguration* m_xrr_config;
};
