// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QSpinBox;

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

  QComboBox* m_backend_combo;
  QCheckBox* m_bypass_xfb;
  QCheckBox* m_enable_statistics;
  QCheckBox* m_dump_textures;
  QCheckBox* m_dump_objects;
  QCheckBox* m_dump_tev_stages;
  QCheckBox* m_dump_tev_fetches;

  QSpinBox* m_object_range_min;
  QSpinBox* m_object_range_max;
};
