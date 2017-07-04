// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QHash>
#include <wobjectdefs.h>

class AdvancedWidget;
class EnhancementsWidget;
class HacksWidget;
class GeneralWidget;
class GraphicsWidget;
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
  W_OBJECT(GraphicsWindow)
public:
  explicit GraphicsWindow(X11Utils::XRRConfiguration* xrr_config, MainWindow* parent);

  void RegisterWidget(GraphicsWidget* widget);
  bool eventFilter(QObject* object, QEvent* event) override;

  void BackendChanged(const QString& backend) W_SIGNAL(BackendChanged, (const QString&), backend);
  void EmulationStarted() W_SIGNAL(EmulationStarted);
  void EmulationStopped() W_SIGNAL(EmulationStopped);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void OnBackendChanged(const QString& backend);
  void OnDescriptionAdded(QWidget* widget, const char* description);

  QTabWidget* m_tab_widget;
  QLabel* m_description;
  QDialogButtonBox* m_button_box;

  AdvancedWidget* m_advanced_widget;
  EnhancementsWidget* m_enhancements_widget;
  HacksWidget* m_hacks_widget;
  GeneralWidget* m_general_widget;
  SoftwareRendererWidget* m_software_renderer;

  X11Utils::XRRConfiguration* m_xrr_config;

  QHash<QObject*, const char*> m_widget_descriptions;
};
