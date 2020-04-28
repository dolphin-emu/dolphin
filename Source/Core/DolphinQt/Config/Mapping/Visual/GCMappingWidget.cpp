// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/Visual/GCMappingWidget.h"

#include <QApplication>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "Common/FileUtil.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

VisualGCMappingWidget::VisualGCMappingWidget(MappingWindow* parent, int port)
    : QWidget(parent), m_parent(parent), m_port(port)
{
  CreateWidgets();

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &VisualGCMappingWidget::RefreshControls);
  m_timer->start(1000 / INDICATOR_UPDATE_FREQ);
}

void VisualGCMappingWidget::CreateWidgets()
{
  auto* layout = new QVBoxLayout;

  m_widget = new QQuickWidget;
  m_widget->setSource(QUrl(QStringLiteral("qrc:/GCPad/GCPadView.qml")));
  m_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_widget->setClearColor(Qt::transparent);
  m_widget->setAttribute(Qt::WA_TranslucentBackground);
  m_widget->setAttribute(Qt::WA_AlwaysStackOnTop);

  layout->addWidget(m_widget);

  // We need to use the old signal / slot syntax here in order to access the QML signal
  connect(GetQMLController(), SIGNAL(detectInput(QString, QString)), this,
          SLOT(DetectInput(QString, QString)), Qt::QueuedConnection);

  setLayout(layout);
}

void VisualGCMappingWidget::DetectInput(QString group, QString name)
{
  auto* buttons = Pad::GetGroup(m_port, PadGroup::Buttons);

  ControllerEmu::Control* map_control = nullptr;

  if (group == QStringLiteral("Button"))
  {
    for (auto& control : buttons->controls)
    {
      if (control->name == name.toStdString())
      {
        map_control = control.get();
        break;
      }
    }
  }
  else if (group == QStringLiteral("DPad"))
  {
    auto* dpad = Pad::GetGroup(m_port, PadGroup::DPad);
    for (auto& control : dpad->controls)
    {
      if (control->name == name.toStdString())
      {
        map_control = control.get();
        break;
      }
    }
  }
  else if (group == QStringLiteral("Trigger"))
  {
    auto* triggers = Pad::GetGroup(m_port, PadGroup::Triggers);
    for (auto& control : triggers->controls)
    {
      if (control->name == name.toStdString())
      {
        map_control = control.get();
        break;
      }
    }
  }
  else if (group == QStringLiteral("Stick_Main"))
  {
    auto* main_stick = Pad::GetGroup(m_port, PadGroup::MainStick);
    for (auto& control : main_stick->controls)
    {
      if (control->name == name.toStdString())
      {
        map_control = control.get();
        break;
      }
    }
  }
  else if (group == QStringLiteral("Stick_C"))
  {
    auto* c_stick = Pad::GetGroup(m_port, PadGroup::CStick);
    for (auto& control : c_stick->controls)
    {
      if (control->name == name.toStdString())
      {
        map_control = control.get();
        break;
      }
    }
  }

  if (map_control == nullptr)
    return;

  const auto default_device_qualifier = m_parent->GetController()->GetDefaultDevice();

  QString expression;

  auto device_strings = m_parent->IsMappingAllDevices() ?
                            g_controller_interface.GetAllDeviceStrings() :
                            std::vector<std::string>{default_device_qualifier.ToString()};

  expression = MappingCommon::DetectExpression(this, g_controller_interface, device_strings,
                                               default_device_qualifier);

  QMetaObject::invokeMethod(GetQMLController(), "detectedInput", Qt::QueuedConnection,
                            Q_ARG(QString, group), Q_ARG(QString, name),
                            Q_ARG(QString, expression));

  if (expression.isEmpty())
    return;

  map_control->control_ref->SetExpression(expression.toStdString());
  m_parent->GetController()->UpdateReferences(g_controller_interface);

  Pad::GetConfig()->SaveConfig();
}

QObject* VisualGCMappingWidget::GetQMLController()
{
  return m_widget->rootObject()->findChild<QObject*>(QStringLiteral("gcpad"));
}

void VisualGCMappingWidget::RefreshControls()
{
  if (!isActiveWindow())
    return;

  auto* buttons = Pad::GetGroup(m_port, PadGroup::Buttons);

  QObject* object = GetQMLController();

  const auto lock = m_parent->GetController()->GetStateLock();

  auto update_control = [this](std::string group, std::string name, std::string expression,
                               double value) {
    QMetaObject::invokeMethod(
        GetQMLController(), "updateControl", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(group)), Q_ARG(QString, QString::fromStdString(name)),
        Q_ARG(QString, QString::fromStdString(expression)), Q_ARG(double, value));
  };

  for (auto& control : buttons->controls)
  {
    const bool activated = control->GetState<bool>();
    const std::string expression = control->control_ref->GetExpression();

    update_control("Button", control->name, expression, activated);
  }

  auto* dpad = Pad::GetGroup(m_port, PadGroup::DPad);

  for (auto& control : dpad->controls)
  {
    const bool activated = control->GetState<bool>();
    const std::string expression = control->control_ref->GetExpression();

    update_control("DPad", control->name, expression, activated);
  }

  auto* triggers = Pad::GetGroup(m_port, PadGroup::Triggers);

  for (size_t i = 0; i < 2; i++)
  {
    auto& control = triggers->controls[i];

    const auto name = control->name;
    const std::string expression = control->control_ref->GetExpression();
    const double value = std::max(std::min(control->GetState<double>(), 1.0), 0.0);

    update_control("Trigger", control->name, expression, value);
  }

  auto* main_stick =
      static_cast<ControllerEmu::ReshapableInput*>(Pad::GetGroup(m_port, PadGroup::MainStick));

  auto main_state = main_stick->GetReshapableState(true);

  QMetaObject::invokeMethod(object, "updateStick", Q_ARG(QString, QString::fromStdString("Main")),
                            Q_ARG(double, main_state.x), Q_ARG(double, main_state.y));

  auto* main_stick_group = Pad::GetGroup(m_port, PadGroup::MainStick);

  for (auto& control : main_stick_group->controls)
  {
    const std::string expression = control->control_ref->GetExpression();
    update_control("Stick_Main", control->name, expression, 0);
  }

  auto* c_stick =
      static_cast<ControllerEmu::ReshapableInput*>(Pad::GetGroup(m_port, PadGroup::CStick));

  auto c_state = c_stick->GetReshapableState(true);

  QMetaObject::invokeMethod(object, "updateStick", Q_ARG(QString, QString::fromStdString("C")),
                            Q_ARG(double, c_state.x), Q_ARG(double, c_state.y));

  auto* c_stick_group = Pad::GetGroup(m_port, PadGroup::CStick);

  for (auto& control : c_stick_group->controls)
  {
    const std::string expression = control->control_ref->GetExpression();
    update_control("Stick_C", control->name, expression, 0);
  }
}
