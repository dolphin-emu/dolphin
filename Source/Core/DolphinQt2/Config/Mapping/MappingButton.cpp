// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QMouseEvent>
#include <QRegExp>
#include <QString>
#include <thread>

#include "DolphinQt2/Config/Mapping/MappingButton.h"

#include "Common/Thread.h"
#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingButton::MappingButton(MappingWidget* widget, ControlReference* ref)
    : ElidedButton(QString::fromStdString(ref->expression)), m_parent(widget), m_reference(ref)
{
  Connect();
}

void MappingButton::Connect()
{
  connect(this, &MappingButton::clicked, this, &MappingButton::OnButtonPressed);
}

static QString
GetExpressionForControl(const QString& control_name,
                        const ciface::Core::DeviceQualifier* control_device = nullptr,
                        const ciface::Core::DeviceQualifier* default_device = nullptr)
{
  QString expr;

  // non-default device
  if (control_device && default_device && !(*control_device == *default_device))
  {
    expr += QString::fromStdString(control_device->ToString());
    expr += QStringLiteral(":");
  }

  // append the control name
  expr += control_name;

  QRegExp reg(QStringLiteral("[a-zA-Z0-9_]*"));
  if (!reg.exactMatch(expr))
    expr = QStringLiteral("`%1`").arg(expr);

  return expr;
}

void MappingButton::OnButtonPressed()
{
  if (m_block || m_parent->GetDevice() == nullptr)
    return;

  // Make sure that we don't block event handling
  std::thread([this] {
    if (m_reference->IsInput())
    {
      const auto dev = m_parent->GetDevice();

      setText(QStringLiteral("..."));

      Common::SleepCurrentThread(100);

      SetBlockInputs(true);

      if (m_parent->GetFirstButtonPress())
        m_reference->Detect(10, dev.get());

      // Avoid that the button press itself is registered as an event
      Common::SleepCurrentThread(100);

      ciface::Core::Device::Control* const ctrl = m_reference->Detect(5000, dev.get());

      SetBlockInputs(false);
      if (ctrl)
      {
        m_reference->expression =
            GetExpressionForControl(QString::fromStdString(ctrl->GetName())).toStdString();
        Update();
      }
      else
      {
        OnButtonTimeout();
      }
    }
    else
    {
      // TODO: Implement Output
    }
  }).detach();
}

void MappingButton::OnButtonTimeout()
{
  setText(QStringLiteral(""));
}

void MappingButton::Clear()
{
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

void MappingWindow::OnDefaultFieldsPressed()
{
  if (m_controller == nullptr)
    return;

  m_controller->LoadDefaults(g_controller_interface);
  m_controller->UpdateReferences(g_controller_interface);
  emit Update();
}

bool MappingButton::event(QEvent* event)
{
  return !m_block ? QPushButton::event(event) : true;
}

void MappingButton::mouseReleaseEvent(QMouseEvent* event)
{
  if (m_reference->IsInput())
  {
    switch (event->button())
    {
    case Qt::MouseButton::LeftButton:
      QPushButton::mouseReleaseEvent(event);
      break;
    case Qt::MouseButton::MiddleButton:
      Clear();
      break;
    case Qt::MouseButton::RightButton:
      // TODO Open advanced dialog
      break;
    default:
      break;
    }
  }
  else
  {
    // TODO Open output dialog
  }
}
