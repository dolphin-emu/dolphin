// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayController.h"

#include "Common/FileUtil.h"
#include "DolphinQt/Scripting/InputDisplayDumper.h"
#include "DolphinQt/Scripting/InputDisplayWidget.h"

namespace
{
QString SysInputDisplayDir()
{
  return QString::fromStdString(File::GetSysDirectory() + "InputDisplay");
}
}  // namespace

InputDisplayController::InputDisplayController(QObject* parent) : QObject(parent)
{
}

InputDisplayController::~InputDisplayController() = default;

void InputDisplayController::Show()
{
  if (m_widget)
    return;
  if (!m_skin.IsValid())
    m_skin = GCSkin::Load(SysInputDisplayDir());
  m_widget = new InputDisplayWidget(m_skin);
  connect(m_widget, &InputDisplayWidget::closed, this, &InputDisplayController::closed);
  m_widget->show();
}

void InputDisplayController::Hide()
{
  StopDump();
  if (m_widget)
  {
    InputDisplayWidget* w = m_widget;
    m_widget = nullptr;
    w->close();
    w->deleteLater();
  }
}

bool InputDisplayController::IsShown() const
{
  return m_widget != nullptr;
}

void InputDisplayController::StartDump()
{
  Show();
  if (!m_dumper)
    m_dumper = std::make_unique<InputDisplayDumper>(m_skin);
  m_dumper->Start();
  m_dumping = true;
}

void InputDisplayController::StopDump()
{
  if (m_dumper)
    m_dumper->Stop();
  m_dumping = false;
}

bool InputDisplayController::IsDumping() const
{
  return m_dumping;
}
