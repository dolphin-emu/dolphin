// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsBool;
class GraphicsWindow;
class QSpinBox;
class ToolTipComboBox;

class SoftwareRendererWidget final : public GraphicsWidget
{
  Q_OBJECT
public:
  explicit SoftwareRendererWidget(GraphicsWindow* parent);

signals:
  void BackendChanged(const QString& backend);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnEmulationStateChanged(bool running);

  ToolTipComboBox* m_backend_combo;
  GraphicsBool* m_show_statistics;
  GraphicsBool* m_dump_textures;
  GraphicsBool* m_dump_objects;
  GraphicsBool* m_dump_tev_stages;
  GraphicsBool* m_dump_tev_fetches;

  QSpinBox* m_object_range_min;
  QSpinBox* m_object_range_max;
};
