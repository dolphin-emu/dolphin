// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QHash>

class GraphicsWidget;
class MainWindow;
class QLabel;
class QTabWidget;
class QDialogButtonBox;

namespace X11Utils
{
class XRRConfiguration;
}

class GraphicsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit GraphicsWindow(X11Utils::XRRConfiguration* xrr_config, MainWindow* parent);

  void RegisterWidget(GraphicsWidget* widget);
  bool eventFilter(QObject* object, QEvent* event) override;
signals:
  void BackendChanged(const QString& backend);
  void EmulationStarted();
  void EmulationStopped();

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void OnBackendChanged(const QString& backend);
  void OnDescriptionAdded(QWidget* widget, const char* description);

  QTabWidget* m_tab_widget;
  QLabel* m_description;
  QDialogButtonBox* m_button_box;

  X11Utils::XRRConfiguration* m_xrr_config;

  QHash<QObject*, const char*> m_widget_descriptions;
};
