// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingButton.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QString>

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

constexpr int SLIDER_TICK_COUNT = 100;

// Escape ampersands and simplify the text for a short preview
static QString RefToDisplayString(ControlReference* ref)
{
  const bool expression_valid =
      ref->GetParseStatus() != ciface::ExpressionParser::ParseStatus::SyntaxError;
  QString expression;
  {
    const auto lock = ControllerEmu::EmulatedController::GetStateLock();
    expression = QString::fromStdString(ref->GetExpression());
  }

  // Split by "`" so that we can give a better preview of control,
  // without including their device in front of them, which is usually
  // too long to actually see the control.
  QStringList controls = expression.split(QLatin1Char{'`'});
  // Do try to simplify controls if the parsing had failed, as it might create false positives.
  if (expression_valid)
  {
    for (int i = 0; i < controls.size(); i++)
    {
      // We have two ` for control so make sure to only consider the odd ones.
      if (i % 2)
      {
        // Use the code from the ControlQualifier instead of duplicating it.
        ciface::ExpressionParser::ControlQualifier qualifier;
        qualifier.FromString(controls[i].toStdString());
        // If the control has got a device specifier/path, add ":" in front of it, to make it clear.
        controls[i] = qualifier.has_device ? QStringLiteral(":") : QString();
        controls[i].append(QString::fromStdString(qualifier.control_name));
      }
      else
      {
        controls[i].remove(QLatin1Char{' '});
      }
    }
  }
  // Do not re-add "`" to the final string, we don't need to see it.
  expression = controls.join(QStringLiteral(""));

  expression.remove(QLatin1Char{'\t'});
  expression.remove(QLatin1Char{'\n'});
  expression.remove(QLatin1Char{'\r'});
  expression.replace(QLatin1Char{'&'}, QStringLiteral("&&"));

  return expression;
}

MappingButton::MappingButton(MappingWidget* parent, ControlReference* ref, ControlType control_type)
    : ElidedButton{RefToDisplayString(ref)}, m_mapping_window{parent->GetParent()},
      m_reference{ref}, m_control_type{control_type}
{
  if (m_reference->IsInput())
  {
    setToolTip(
        tr("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
  }
  else
  {
    setToolTip(tr("Left/Right-click to configure output.\nMiddle-click to clear."));
  }

  connect(this, &MappingButton::clicked, this, &MappingButton::Clicked);

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingButton::ConfigChanged);
  connect(this, &MappingButton::ConfigChanged,
          [this] { setText(RefToDisplayString(m_reference)); });
}

void MappingButton::AdvancedPressed()
{
  m_mapping_window->CancelMapping();

  IOWindow io(m_mapping_window, m_mapping_window->GetController(), m_reference,
              m_reference->IsInput() ? IOWindow::Type::Input : IOWindow::Type::Output);
  io.exec();

  ConfigChanged();
  m_mapping_window->Save();
}

void MappingButton::Clicked()
{
  if (!m_reference->IsInput())
  {
    AdvancedPressed();
    return;
  }

  m_mapping_window->QueueInputDetection(this);
}

void MappingButton::Clear()
{
  m_reference->range = 100.0 / SLIDER_TICK_COUNT;

  m_reference->SetExpression("");
  m_mapping_window->GetController()->UpdateSingleControlReference(g_controller_interface,
                                                                  m_reference);

  m_mapping_window->Save();

  m_mapping_window->UnQueueInputDetection(this);
}

void MappingButton::mouseReleaseEvent(QMouseEvent* event)
{
  switch (event->button())
  {
  case Qt::MouseButton::MiddleButton:
    Clear();
    return;
  case Qt::MouseButton::RightButton:
    AdvancedPressed();
    return;
  default:
    QPushButton::mouseReleaseEvent(event);
    return;
  }
}

ControlReference* MappingButton::GetControlReference()
{
  return m_reference;
}

auto MappingButton::GetControlType() const -> ControlType
{
  return m_control_type;
}
