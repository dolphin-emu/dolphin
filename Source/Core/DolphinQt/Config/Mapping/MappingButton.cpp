// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingButton.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QString>

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingCommon.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
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

bool MappingButton::IsInput() const
{
  return m_reference->IsInput();
}

MappingButton::MappingButton(MappingWidget* parent, ControlReference* ref, bool indicator)
    : ElidedButton(RefToDisplayString(ref)), m_parent(parent), m_reference(ref)
{
  if (IsInput())
  {
    setToolTip(
        tr("Left-click to detect input.\nMiddle-click to clear.\nRight-click for more options."));
  }
  else
  {
    setToolTip(tr("Left/Right-click to configure output.\nMiddle-click to clear."));
  }

  connect(this, &MappingButton::clicked, this, &MappingButton::Clicked);

  if (indicator)
    connect(parent, &MappingWidget::Update, this, &MappingButton::UpdateIndicator);

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingButton::ConfigChanged);
}

void MappingButton::AdvancedPressed()
{
  IOWindow io(m_parent, m_parent->GetController(), m_reference,
              m_reference->IsInput() ? IOWindow::Type::Input : IOWindow::Type::Output);
  SetQWidgetWindowDecorations(&io);
  io.exec();

  ConfigChanged();
  m_parent->SaveSettings();
}

void MappingButton::Clicked()
{
  if (!m_reference->IsInput())
  {
    AdvancedPressed();
    return;
  }

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
  m_parent->GetController()->UpdateSingleControlReference(g_controller_interface, m_reference);

  ConfigChanged();
  m_parent->SaveSettings();
}

void MappingButton::Clear()
{
  m_reference->range = 100.0 / SLIDER_TICK_COUNT;

  m_reference->SetExpression("");
  m_parent->GetController()->UpdateSingleControlReference(g_controller_interface, m_reference);

  m_parent->SaveSettings();
  ConfigChanged();
}

void MappingButton::UpdateIndicator()
{
  if (!isActiveWindow())
    return;

  QFont f = m_parent->font();

  // If the input state is "true" (we can't know the state of outputs), show it in bold.
  if (m_reference->IsInput() && m_reference->GetState<bool>())
    f.setBold(true);
  // If the expression has failed to parse, show it in italic.
  // Some expressions still work even the failed to parse so don't prevent the GetState() above.
  if (m_reference->GetParseStatus() == ciface::ExpressionParser::ParseStatus::SyntaxError)
    f.setItalic(true);

  setFont(f);
}

void MappingButton::ConfigChanged()
{
  setText(RefToDisplayString(m_reference));
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
