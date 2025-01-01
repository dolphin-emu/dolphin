// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigSlider;
class ConfigSliderLabel;
class GameConfigWidget;
class GraphicsWindow;
class QLabel;
class ToolTipSlider;

namespace Config
{
class Layer;
}  // namespace Config

class HacksWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit HacksWidget(GraphicsWindow* parent);
  HacksWidget(GameConfigWidget* parent, Config::Layer* layer);

private:
  void OnBackendChanged(const QString& backend_name);

  // EFB
  ConfigBool* m_skip_efb_cpu;
  ConfigBool* m_ignore_format_changes;
  ConfigBool* m_store_efb_copies;
  ConfigBool* m_defer_efb_copies;

  // Texture Cache
  ConfigSliderLabel* m_accuracy_label;
  ConfigSlider* m_accuracy;
  ConfigBool* m_gpu_texture_decoding;

  // External Framebuffer
  ConfigBool* m_store_xfb_copies;
  ConfigBool* m_immediate_xfb;
  ConfigBool* m_skip_duplicate_xfbs;

  // Other
  ConfigBool* m_fast_depth_calculation;
  ConfigBool* m_disable_bounding_box;
  ConfigBool* m_vertex_rounding;
  ConfigBool* m_vi_skip;
  ConfigBool* m_save_texture_cache_state;

  Config::Layer* m_game_layer = nullptr;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void UpdateDeferEFBCopiesEnabled();
  void UpdateSkipPresentingDuplicateFramesEnabled();
};
