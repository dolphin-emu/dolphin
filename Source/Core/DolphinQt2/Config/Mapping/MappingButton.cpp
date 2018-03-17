// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <thread>

#include <QMouseEvent>
#include <QRegExp>
#include <QString>
#include <QTimer>

#include "DolphinQt2/Config/Mapping/MappingButton.h"

#include "Common/Thread.h"
#include "DolphinQt2/Config/Mapping/IOWindow.h"
#include "DolphinQt2/Config/Mapping/MappingCommon.h"
#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"
#include "DolphinQt2/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt2/Settings.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

static QString EscapeAmpersand(QString&& string)
{
  return string.replace(QStringLiteral("&"), QStringLiteral("&&"));
}

MappingButton::MappingButton(MappingWidget* widget, ControlReference* ref, bool indicator)
    : ElidedButton(EscapeAmpersand(QString::fromStdString(ref->GetExpression()))), m_parent(widget),
      m_reference(ref)
{
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

  setMaximumHeight(24);
  setMaximumWidth(200);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MappingButton::Connect()
{
  connect(this, &MappingButton::clicked, this, &MappingButton::OnButtonPressed);
}

void MappingButton::OnButtonPressed()
{
  if (m_parent->GetDevice() == nullptr || !m_reference->IsInput())
    return;

  installEventFilter(BlockUserInputFilter::Instance());
  grabKeyboard();
  grabMouse();

  // Make sure that we don't block event handling
  std::thread([this] {
    const auto dev = m_parent->GetDevice();

    setText(QStringLiteral("..."));

    // Avoid that the button press itself is registered as an event
    Common::SleepCurrentThread(100);

    const auto expr = MappingCommon::DetectExpression(
        m_reference, dev.get(), m_parent->GetController()->GetDefaultDevice());

    releaseMouse();
    releaseKeyboard();
    removeEventFilter(BlockUserInputFilter::Instance());

    if (!expr.isEmpty())
    {
      m_reference->SetExpression(expr.toStdString());
      m_parent->SaveSettings();
      Update();
    }
    else
    {
      OnButtonTimeout();
    }
  }).detach();
}

void MappingButton::OnButtonTimeout()
{
  setText(EscapeAmpersand(QString::fromStdString(m_reference->GetExpression())));
}

void MappingButton::Clear()
{
  m_reference->SetExpression("");
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
