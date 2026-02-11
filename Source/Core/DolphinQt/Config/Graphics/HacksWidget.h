// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigSlider;
class ConfigSliderLabel;
class GraphicsPane;

namespace Config
{
class Layer;
}  // namespace Config

class HacksWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit HacksWidget(GraphicsPane* gfx_pane);

private:
  void OnBackendChanged(const QString& backend_name);

  // EFB
  ConfigBool* m_skip_efb_cpu;
  ConfigBool* m_ignore_format_changes;
  ConfigBool* m_store_efb_copies;
  ConfigBool* m_defer_efb_copies;
  ConfigBool* m_defer_efb_access_invalidation;

  // Texture Cache
  ConfigSliderLabel* m_texture_accuracy_label;
  ConfigSlider* m_texture_accuracy;

  // External Framebuffer
  ConfigBool* m_store_xfb_copies;
  ConfigBool* m_immediate_xfb;

  // Other
  ConfigBool* m_fast_depth_calculation;
  ConfigBool* m_fast_texture_sampling;
  ConfigBool* m_disable_bounding_box;

  // Enhancements
  ConfigBool* m_widescreen_hack;
  ConfigBool* m_disable_fog;

  Config::Layer* m_game_layer = nullptr;

  void CreateWidgets();
  void ConnectWidgets(GraphicsPane* gfx_pane);
  void AddDescriptions();

  void UpdateSkipEFBCPUEnabled();
  void UpdateDeferEFBCopiesEnabled();
};
