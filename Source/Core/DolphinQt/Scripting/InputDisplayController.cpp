// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayController.h"

#include <algorithm>

#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/SI/SI_Device.h"
#include "DolphinQt/Scripting/InputDisplayDumper.h"
#include "DolphinQt/Scripting/InputDisplayWidget.h"

namespace
{
QString SysInputDisplayDir()
{
  return QString::fromStdString(File::GetSysDirectory() + "InputDisplay");
}

// Reads the configured device, so the display works even before a game boots.
SerialInterface::SIDevices ConfiguredDevice(int port)
{
  return Config::Get(Config::GetInfoForSIDevice(port));
}

QString SkinNameForDevice(SerialInterface::SIDevices type)
{
  if (type == SerialInterface::SIDEVICE_GC_GBA_EMULATED ||
      type == SerialInterface::SIDEVICE_GC_GBA)
  {
    return QStringLiteral("gba");
  }
  return QStringLiteral("gamecube");
}
}  // namespace

InputDisplayController::InputDisplayController(QObject* parent) : QObject(parent)
{
}

InputDisplayController::~InputDisplayController()
{
  if (m_config_callback.id != Config::ConfigChangedCallbackID{}.id)
    Config::RemoveConfigChangedCallback(m_config_callback);
}

void InputDisplayController::Show()
{
  if (m_active)
    return;
  m_active = true;

  // Reconcile windows whenever controller config changes. The callback may fire on a non-GUI
  // thread, so marshal back onto this object's thread before touching widgets.
  if (m_config_callback.id == Config::ConfigChangedCallbackID{}.id)
  {
    m_config_callback = Config::AddConfigChangedCallback([this] {
      QMetaObject::invokeMethod(this, [this] { SyncWindows(); }, Qt::QueuedConnection);
    });
  }

  SyncWindows();

  if (!IsShown())
  {
    m_active = false;
    emit closed();
  }
}

void InputDisplayController::Hide()
{
  StopDump();
  m_active = false;
  for (int port = 0; port < 4; ++port)
    ClosePort(port);
  m_skins.clear();
}

void InputDisplayController::SyncWindows()
{
  if (!m_active)
    return;

  int configured = 0;
  for (int port = 0; port < 4; ++port)
  {
    const SerialInterface::SIDevices type = ConfiguredDevice(port);
    if (type == SerialInterface::SIDEVICE_NONE)
    {
      ClosePort(port);
      continue;
    }

    ++configured;
    const QString skin_name = SkinNameForDevice(type);
    // Recreate the window if the device type (and thus skin) changed under it.
    if (m_widgets[port] && m_widget_skins[port] != skin_name)
      ClosePort(port);
    if (!m_widgets[port])
      OpenPort(port, skin_name);
  }

  // Nothing configured (e.g. fresh setup): fall back to a single GC window on port 1.
  if (configured == 0 && !m_widgets[0])
    OpenPort(0, QStringLiteral("gamecube"));

  if (!IsShown())
  {
    m_active = false;
    emit closed();
  }
}

void InputDisplayController::OpenPort(int port, const QString& skin_name)
{
  if (!m_skins[skin_name].IsValid())
    m_skins[skin_name] = GCSkin::Load(SysInputDisplayDir(), skin_name);
  const GCSkin& skin = m_skins[skin_name];
  if (!skin.IsValid())
    return;

  auto* widget = new InputDisplayWidget(skin, port);
  connect(widget, &InputDisplayWidget::closed, this, [this, port] { OnWidgetClosed(port); });
  connect(widget, &InputDisplayWidget::dumpControllerInputsChanged, this,
          [this](int port, bool enabled) { SetPortDumping(port, enabled); });
  widget->show();
  m_widgets[port] = widget;
  m_widget_skins[port] = skin_name;

  if (m_dump_all_ports && !m_dumpers[port])
  {
    m_dumpers[port] = std::make_unique<InputDisplayDumper>(skin, port);
    m_dumpers[port]->Start();
    widget->SetDumping(true);
  }
}

void InputDisplayController::ClosePort(int port)
{
  if (m_dumpers[port])
  {
    m_dumpers[port]->Stop();
    m_dumpers[port].reset();
  }
  if (m_widgets[port])
  {
    InputDisplayWidget* w = m_widgets[port];
    m_widgets[port] = nullptr;
    m_widget_skins[port].clear();
    w->close();
    w->deleteLater();
  }
}

bool InputDisplayController::IsShown() const
{
  for (const auto* w : m_widgets)
  {
    if (w)
      return true;
  }
  return false;
}

void InputDisplayController::StartDump()
{
  Show();
  m_dump_all_ports = true;
  m_dumping = true;
  for (int port = 0; port < 4; ++port)
  {
    if (!m_widgets[port] || m_dumpers[port])
      continue;
    const QString& skin_name = m_widget_skins[port];
    if (m_skins.contains(skin_name))
    {
      m_dumpers[port] = std::make_unique<InputDisplayDumper>(m_skins[skin_name], port);
      m_dumpers[port]->Start();
      m_widgets[port]->SetDumping(true);
    }
  }
}

void InputDisplayController::StopDump()
{
  for (auto& dumper : m_dumpers)
  {
    if (dumper)
      dumper->Stop();
    dumper.reset();
  }
  for (InputDisplayWidget* widget : m_widgets)
  {
    if (widget)
      widget->SetDumping(false);
  }
  m_dumping = false;
  m_dump_all_ports = false;
}

bool InputDisplayController::IsDumping() const
{
  return m_dumping;
}

bool InputDisplayController::AnyPortDumping() const
{
  return std::ranges::any_of(m_dumpers, [](const auto& dumper) { return dumper != nullptr; });
}

void InputDisplayController::SetPortDumping(int port, bool dumping)
{
  if (port < 0 || port >= static_cast<int>(m_dumpers.size()))
    return;

  if (dumping)
  {
    Show();
    if (!m_widgets[port] || m_dumpers[port])
      return;

    const QString& skin_name = m_widget_skins[port];
    if (!m_skins.contains(skin_name))
      return;

    m_dumpers[port] = std::make_unique<InputDisplayDumper>(m_skins[skin_name], port);
    m_dumpers[port]->Start();
  }
  else if (m_dumpers[port])
  {
    m_dumpers[port]->Stop();
    m_dumpers[port].reset();
  }

  if (m_widgets[port])
    m_widgets[port]->SetDumping(m_dumpers[port] != nullptr);

  m_dump_all_ports = false;
  m_dumping = AnyPortDumping();
}

void InputDisplayController::OnWidgetClosed(int port)
{
  m_widgets[port] = nullptr;
  m_widget_skins[port].clear();
  if (m_dumpers[port])
  {
    m_dumpers[port]->Stop();
    m_dumpers[port].reset();
  }
  m_dumping = AnyPortDumping();
  if (!IsShown())
  {
    m_active = false;
    emit closed();
  }
}
