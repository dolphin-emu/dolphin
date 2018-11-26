// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <future>
#include <utility>

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QRegExp>
#include <QString>
#include <QTimer>

#include "DolphinQt/Config/Mapping/MappingButton.h"

#include "Common/Thread.h"
#include "Core/Core.h"

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

constexpr int SLIDER_TICK_COUNT = 100;
constexpr int VERTICAL_PADDING = 2;

static QString EscapeAmpersand(QString&& string)
{
  return string.replace(QStringLiteral("&"), QStringLiteral("&&"));
}

bool MappingButton::IsInput() const
{
  return m_reference->IsInput();
}

MappingButton::MappingButton(MappingWidget* widget, ControlReference* ref, bool indicator)
    : ElidedButton(EscapeAmpersand(QString::fromStdString(ref->GetExpression()))), m_parent(widget),
      m_reference(ref)
{
  // Force all mapping buttons to use stay at a minimal height
  int height = QFontMetrics(qApp->font()).height() + 2 * VERTICAL_PADDING;

  setMinimumHeight(height);

  // macOS needs some wiggle room to always get round buttons
  setMaximumHeight(height + 8);

  // Make sure that long entries don't throw our layout out of whack
  setMaximumWidth(115);

  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  Connect();
  setToolTip(
      tr("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
  if (!m_reference->IsInput() || !indicator)
    return;

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, [this] {
    if (!isActiveWindow())
      return;

    Settings::Instance().SetControllerStateNeeded(true);

    if (Core::GetState() == Core::State::Uninitialized || Core::GetState() == Core::State::Paused)
      g_controller_interface.UpdateInput();

    auto state = m_reference->State();

    QFont f = m_parent->font();
    QPalette p = m_parent->palette();

    if (state != 0)
    {
      f.setBold(true);
      p.setColor(QPalette::ButtonText, Qt::red);
    }

    setFont(f);
    setPalette(p);

    Settings::Instance().SetControllerStateNeeded(false);
  });

  m_timer->start(1000 / 30);
}

void MappingButton::Connect()
{
  connect(this, &MappingButton::pressed, this, &MappingButton::Detect);
}

void MappingButton::Detect()
{
  if (m_parent->GetDevice() == nullptr || !m_reference->IsInput())
    return;

  installEventFilter(BlockUserInputFilter::Instance());
  grabKeyboard();

  // Make sure that we don't block event handling
  QueueOnObject(this, [this] {
    setText(QStringLiteral("..."));

    // The button text won't be updated if we don't process events here
    QApplication::processEvents();

    // Avoid that the button press itself is registered as an event
    Common::SleepCurrentThread(100);

    QString expr;

    if (m_parent->GetParent()->IsMappingAllDevices())
    {
      std::vector<std::pair<QString, std::shared_ptr<ciface::Core::Device>>> devices;
      for (const std::string& device_str : g_controller_interface.GetAllDeviceStrings())
      {
        ciface::Core::DeviceQualifier devq;
        devq.FromString(device_str);

        auto dev = g_controller_interface.FindDevice(devq);
        if (!dev)
          continue;

        devices.emplace_back(QString::fromStdString(device_str), dev);
      }

      // Wait up to 5 seconds for any controller input.
      const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (expr.isEmpty() && std::chrono::steady_clock::now() < end)
      {
        for (const auto& dev : devices)
        {
          expr = MappingCommon::DetectExpression(m_reference, dev.second.get(),
                                                 ciface::Core::DeviceQualifier(),
                                                 MappingCommon::Quote::On, 10);
          if (expr.isEmpty())
            continue;

          expr = QStringLiteral("`%1:%2`").arg(dev.first).arg(
              expr.startsWith(QLatin1Char('`')) ? expr.mid(1) : expr);
          break;
        }
      }
    }
    else
    {
      const auto dev = m_parent->GetDevice();
      if (dev)
      {
        expr = MappingCommon::DetectExpression(m_reference, dev.get(),
                                               m_parent->GetController()->GetDefaultDevice());
      }
    }

    releaseKeyboard();
    removeEventFilter(BlockUserInputFilter::Instance());

    if (!expr.isEmpty())
    {
      m_reference->SetExpression(expr.toStdString());
      m_parent->SaveSettings();
      Update();
      m_parent->GetController()->UpdateReferences(g_controller_interface);

      if (m_parent->IsIterativeInput())
        m_parent->NextButton(this);
    }
    else
    {
      OnButtonTimeout();
    }
  });
}

void MappingButton::OnButtonTimeout()
{
  setText(EscapeAmpersand(QString::fromStdString(m_reference->GetExpression())));
}

void MappingButton::Clear()
{
  m_reference->SetExpression("");
  m_reference->range = 100.0 / SLIDER_TICK_COUNT;
  m_parent->SaveSettings();
  Update();
}

void MappingButton::Update()
{
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  m_reference->UpdateReference(g_controller_interface,
                               m_parent->GetController()->GetDefaultDevice());
  setText(EscapeAmpersand(QString::fromStdString(m_reference->GetExpression())));
}

void MappingButton::mouseReleaseEvent(QMouseEvent* event)
{
  switch (event->button())
  {
  case Qt::MouseButton::LeftButton:
    if (m_reference->IsInput())
      QPushButton::mouseReleaseEvent(event);
    else
      emit AdvancedPressed();
    return;
  case Qt::MouseButton::MidButton:
    Clear();
    return;
  case Qt::MouseButton::RightButton:
    emit AdvancedPressed();
    return;
  default:
    return;
  }
}
