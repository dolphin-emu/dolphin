// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <thread>

#include <QMouseEvent>
#include <QRegExp>
#include <QString>

#include "DolphinQt2/Config/Mapping/MappingButton.h"

#include "Common/Thread.h"
#include "DolphinQt2/Config/Mapping/IOWindow.h"
#include "DolphinQt2/Config/Mapping/MappingCommon.h"
#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

MappingButton::MappingButton(MappingWidget* widget, ControlReference* ref)
    : ElidedButton(QString::fromStdString(ref->expression)), m_parent(widget), m_reference(ref)
{
  Connect();
}

void MappingButton::Connect()
{
  connect(this, &MappingButton::clicked, this, &MappingButton::OnButtonPressed);
}

void MappingButton::OnButtonPressed()
{
  if (m_block || m_parent->GetDevice() == nullptr || !m_reference->IsInput())
    return;

  // Make sure that we don't block event handling
  std::thread([this] {
    const auto dev = m_parent->GetDevice();

    setText(QStringLiteral("..."));

    SetBlockInputs(true);

    // Avoid that the button press itself is registered as an event
    Common::SleepCurrentThread(100);

    const auto expr = MappingCommon::DetectExpression(m_reference, dev.get(),
                                                      m_parent->GetParent()->GetDeviceQualifier(),
                                                      m_parent->GetController()->default_device);

    SetBlockInputs(false);
    if (!expr.isEmpty())
    {
      m_reference->expression = expr.toStdString();
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
  setText(QString::fromStdString(m_reference->expression));
}

void MappingButton::Clear()
{
  m_parent->Update();
  m_reference->expression.clear();
  Update();
}

void MappingButton::Update()
{
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  m_reference->UpdateReference(g_controller_interface, m_parent->GetParent()->GetDeviceQualifier());
  setText(QString::fromStdString(m_reference->expression));
  m_parent->SaveSettings();
}

void MappingButton::SetBlockInputs(const bool block)
{
  m_parent->SetBlockInputs(block);
  m_block = block;
}

bool MappingButton::event(QEvent* event)
{
  return !m_block ? QPushButton::event(event) : true;
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
  case Qt::MouseButton::MiddleButton:
    Clear();
    return;
  case Qt::MouseButton::RightButton:
    emit AdvancedPressed();
    return;
  default:
    return;
  }
}
