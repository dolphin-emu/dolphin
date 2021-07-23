// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QWidget>

#include "Common/CommonTypes.h"

namespace HW::GBA
{
class Core;
}  // namespace HW::GBA

class QCloseEvent;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QPaintEvent;

class GBAWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GBAWidget(std::weak_ptr<HW::GBA::Core> core, int device_number,
                     std::string_view game_title, int width, int height, QWidget* parent = nullptr,
                     Qt::WindowFlags flags = {});
  ~GBAWidget();

  void GameChanged(std::string_view game_title, int width, int height);
  void SetVideoBuffer(std::vector<u32> video_buffer);

  void SetVolume(int volume);
  void VolumeDown();
  void VolumeUp();
  bool IsMuted();
  void ToggleMute();
  void ToggleDisconnect();

  void LoadROM();
  void UnloadROM();
  void PromptForEReaderCards();
  void ResetCore();
  void DoState(bool export_state);
  void Resize(int scale);

private:
  void UpdateTitle();
  void UpdateVolume();

  void LoadGeometry();
  void SaveGeometry();

  bool CanControlCore();
  bool CanResetCore();

  void closeEvent(QCloseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  std::weak_ptr<HW::GBA::Core> m_core;
  std::vector<u32> m_video_buffer;
  int m_device_number;
  int m_local_pad;
  std::string m_game_title;
  int m_width;
  int m_height;
  std::string m_netplayer_name;
  bool m_is_local_pad;
  int m_volume;
  bool m_muted;
  bool m_force_disconnect;
};

class GBAWidgetController : public QObject
{
  Q_OBJECT
public:
  explicit GBAWidgetController() = default;
  ~GBAWidgetController();

  void Create(std::weak_ptr<HW::GBA::Core> core, int device_number, std::string_view game_title,
              int width, int height);
  void GameChanged(std::string_view game_title, int width, int height);
  void FrameEnded(std::vector<u32> video_buffer);

private:
  GBAWidget* m_widget{};
};
