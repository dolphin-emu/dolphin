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

  X11Utils::XRRConfiguration* m_xrr_config;
};
