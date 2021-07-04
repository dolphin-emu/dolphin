// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GBAHost.h"

#include <QApplication>

#include "Core/HW/GBACore.h"
#include "DolphinQt/GBAWidget.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::weak_ptr<HW::GBA::Core>)
Q_DECLARE_METATYPE(std::vector<u32>)

GBAHost::GBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  qRegisterMetaType<std::string>();
  qRegisterMetaType<std::weak_ptr<HW::GBA::Core>>();
  qRegisterMetaType<std::vector<u32>>();

  m_widget_controller = new GBAWidgetController();
  m_widget_controller->moveToThread(qApp->thread());
  m_core = std::move(core);
  auto core_ptr = m_core.lock();

  u32 width, height;
  core_ptr->GetVideoDimensions(&width, &height);
  QMetaObject::invokeMethod(
      m_widget_controller, "Create", Q_ARG(std::weak_ptr<HW::GBA::Core>, m_core),
      Q_ARG(int, core_ptr->GetDeviceNumber()), Q_ARG(std::string, core_ptr->GetGameTitle()),
      Q_ARG(int, static_cast<int>(width)), Q_ARG(int, static_cast<int>(height)));
}

GBAHost::~GBAHost()
{
  m_widget_controller->deleteLater();
}

void GBAHost::GameChanged()
{
  auto core_ptr = m_core.lock();
  if (!core_ptr->IsStarted())
    return;

  u32 width, height;
  core_ptr->GetVideoDimensions(&width, &height);
  QMetaObject::invokeMethod(
      m_widget_controller, "GameChanged", Q_ARG(std::string, core_ptr->GetGameTitle()),
      Q_ARG(int, static_cast<int>(width)), Q_ARG(int, static_cast<int>(height)));
}

void GBAHost::FrameEnded(const std::vector<u32>& video_buffer)
{
  QMetaObject::invokeMethod(m_widget_controller, "FrameEnded",
                            Q_ARG(std::vector<u32>, video_buffer));
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return std::make_unique<GBAHost>(core);
}
