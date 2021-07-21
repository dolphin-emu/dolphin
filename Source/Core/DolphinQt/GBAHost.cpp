// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GBAHost.h"

#include <QApplication>

#include "Core/HW/GBACore.h"
#include "DolphinQt/GBAWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

GBAHost::GBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  m_widget_controller = new GBAWidgetController();
  m_widget_controller->moveToThread(qApp->thread());
  m_core = std::move(core);
  auto core_ptr = m_core.lock();

  int device_number = core_ptr->GetDeviceNumber();
  std::string game_title = core_ptr->GetGameTitle();
  u32 width, height;
  core_ptr->GetVideoDimensions(&width, &height);

  QueueOnObject(m_widget_controller, [widget_controller = m_widget_controller, core = m_core,
                                      device_number, game_title, width, height] {
    widget_controller->Create(core, device_number, game_title, width, height);
  });
}

GBAHost::~GBAHost()
{
  m_widget_controller->deleteLater();
}

void GBAHost::GameChanged()
{
  auto core_ptr = m_core.lock();
  if (!core_ptr || !core_ptr->IsStarted())
    return;

  std::string game_title = core_ptr->GetGameTitle();
  u32 width, height;
  core_ptr->GetVideoDimensions(&width, &height);

  QueueOnObject(m_widget_controller,
                [widget_controller = m_widget_controller, game_title, width, height] {
                  widget_controller->GameChanged(game_title, width, height);
                });
}

void GBAHost::FrameEnded(const std::vector<u32>& video_buffer)
{
  QueueOnObject(m_widget_controller, [widget_controller = m_widget_controller, video_buffer] {
    widget_controller->FrameEnded(video_buffer);
  });
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return std::make_unique<GBAHost>(core);
}
