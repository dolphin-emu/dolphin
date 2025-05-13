// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class MainWindow;

namespace Config
{
class Layer;
}  // namespace Config

class GraphicsPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GraphicsPane(MainWindow* main_window, Config::Layer* config_layer);

  Config::Layer* GetConfigLayer();

signals:
  void BackendChanged(const QString& backend);
  void UseFastTextureSamplingChanged();
  void UseGPUTextureDecodingChanged();

private:
  void CreateMainLayout();
  void OnBackendChanged(const QString& backend);

  MainWindow* const m_main_window;
  Config::Layer* const m_config_layer;
};
