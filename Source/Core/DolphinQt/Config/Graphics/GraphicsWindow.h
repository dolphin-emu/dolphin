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

class GraphicsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit GraphicsWindow(MainWindow* parent);

signals:
  void BackendChanged(const QString& backend);

private:
  void CreateMainLayout();
  void OnBackendChanged(const QString& backend);
};
