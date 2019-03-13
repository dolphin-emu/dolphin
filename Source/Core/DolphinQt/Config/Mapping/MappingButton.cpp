// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingButton.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QRegExp>
#include <QString>
#include <QTimer>

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
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

constexpr int SLIDER_TICK_COUNT = 100;

// Escape ampersands and remove ticks
static QString ToDisplayString(QString&& string)
{
  return string.replace(QStringLiteral("&"), QStringLiteral("&&"))
      .replace(QStringLiteral("`"), QStringLiteral(""));
}

bool MappingButton::IsInput() const
{
  return m_reference->IsInput();
}

MappingButton::MappingButton(MappingWidget* widget, ControlReference* ref, bool indicator)
    : ElidedButton(ToDisplayString(QString::fromStdString(ref->GetExpression()))), m_parent(widget),
      m_reference(ref)
{
  // Force all mapping buttons to stay at a minimal height.
  setFixedHeight(minimumSizeHint().height());

  // Make sure that long entries don't throw our layout out of whack.
  setFixedWidth(112);

  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

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

    if (state > ControllerEmu::Buttons::ACTIVATION_THRESHOLD)
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

  const auto default_device_qualifier = m_parent->GetController()->GetDefaultDevice();

  QString expression;

  if (m_parent->GetParent()->IsMappingAllDevices())
  {
    expression = MappingCommon::DetectExpression(this, g_controller_interface,
                                                 g_controller_interface.GetAllDeviceStrings(),
                                                 default_device_qualifier);
  }
  else
  {
    expression = MappingCommon::DetectExpression(this, g_controller_interface,
                                                 {default_device_qualifier.ToString()},
                                                 default_device_qualifier);
  }

  if (expression.isEmpty())
    return;

  m_reference->SetExpression(expression.toStdString());
  m_parent->SaveSettings();
  Update();
  m_parent->GetController()->UpdateReferences(g_controller_interface);

  if (m_parent->IsIterativeInput())
    m_parent->NextButton(this);
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
  setText(ToDisplayString(QString::fromStdString(m_reference->GetExpression())));
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
