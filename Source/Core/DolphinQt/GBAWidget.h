// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QWidget>

#include "Common/CommonTypes.h"
#include "Core/HW/GBACore.h"

class QCloseEvent;
class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QPaintEvent;

class GBAWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GBAWidget(std::weak_ptr<HW::GBA::Core> core, const HW::GBA::CoreInfo& info,
                     QWidget* parent = nullptr, Qt::WindowFlags flags = {});
  ~GBAWidget();

  void GameChanged(const HW::GBA::CoreInfo& info);
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
  HW::GBA::CoreInfo m_core_info;
  std::vector<u32> m_video_buffer;
  int m_local_pad;
  bool m_is_local_pad;
  std::string m_netplayer_name;
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

  void Create(std::weak_ptr<HW::GBA::Core> core, const HW::GBA::CoreInfo& info);
  void GameChanged(const HW::GBA::CoreInfo& info);
  void FrameEnded(std::vector<u32> video_buffer);

private:
  GBAWidget* m_widget{};
};
