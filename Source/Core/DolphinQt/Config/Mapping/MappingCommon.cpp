// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <tuple>
#include <vector>

#include <QApplication>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QTimer>

#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "InputCommon/ControlReference/ControlReference.h"

#include "Common/Thread.h"

namespace MappingCommon
{
constexpr auto INPUT_DETECT_INITIAL_TIME = std::chrono::seconds(3);
constexpr auto INPUT_DETECT_CONFIRMATION_TIME = std::chrono::milliseconds(0);
constexpr auto INPUT_DETECT_MAXIMUM_TIME = std::chrono::seconds(5);

constexpr auto OUTPUT_TEST_TIME = std::chrono::seconds(2);

// Pressing inputs at the same time will result in the & operator vs a hotkey expression.
constexpr auto HOTKEY_VS_CONJUNCION_THRESHOLD = std::chrono::milliseconds(50);

// Some devices (e.g. DS4) provide an analog and digital input for the trigger.
// We prefer just the analog input for simultaneous digital+analog input detections.
constexpr auto SPURIOUS_TRIGGER_COMBO_THRESHOLD = std::chrono::milliseconds(150);

QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  QString expr;

  // non-default device
  if (control_device != default_device)
  {
    expr += QString::fromStdString(control_device.ToString());
    expr += QLatin1Char{':'};
  }

  // append the control name
  expr += control_name;

  if (quote == Quote::On)
  {
    // If our expression contains any non-alpha characters
    // we should quote it
    const QRegularExpression reg(QStringLiteral("[^a-zA-Z]"));
    if (reg.match(expr).hasMatch())
      expr = QStringLiteral("`%1`").arg(expr);
  }

  return expr;
}

QString DetectExpression(QPushButton* button, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  const auto filter = new BlockUserInputFilter(button);

  button->installEventFilter(filter);
  button->grabKeyboard();
  button->grabMouse();

  const auto old_text = button->text();
  button->setText(QStringLiteral("..."));

  // The button text won't be updated if we don't process events here
  QApplication::processEvents();

  // Avoid that the button press itself is registered as an event
  Common::SleepCurrentThread(50);

  auto detections =
      device_container.DetectInput(device_strings, INPUT_DETECT_INITIAL_TIME,
                                   INPUT_DETECT_CONFIRMATION_TIME, INPUT_DETECT_MAXIMUM_TIME);

  RemoveSpuriousTriggerCombinations(&detections);

  const auto timer = new QTimer(button);

  timer->setSingleShot(true);

  button->connect(timer, &QTimer::timeout, [button, filter] {
    button->releaseMouse();
    button->releaseKeyboard();
    button->removeEventFilter(filter);
  });

  // Prevent mappings of "space", "return", or mouse clicks from re-activating detection.
  timer->start(500);

  button->setText(old_text);

  return BuildExpression(detections, default_device, quote);
}

void TestOutput(QPushButton* button, OutputReference* reference)
{
  const auto old_text = button->text();
  button->setText(QStringLiteral("..."));

  // The button text won't be updated if we don't process events here
  QApplication::processEvents();

  reference->State(1.0);
  std::this_thread::sleep_for(OUTPUT_TEST_TIME);
  reference->State(0.0);

  button->setText(old_text);
}

void RemoveSpuriousTriggerCombinations(
    std::vector<ciface::Core::DeviceContainer::InputDetection>* detections)
{
  const auto is_spurious = [&](auto& detection) {
    return std::any_of(detections->begin(), detections->end(), [&](auto& d) {
      // This is a suprious digital detection if a "smooth" (analog) detection is temporally near.
      return &d != &detection && d.smoothness > 1 &&
             abs(d.press_time - detection.press_time) < SPURIOUS_TRIGGER_COMBO_THRESHOLD;
    });
  };

  detections->erase(std::remove_if(detections->begin(), detections->end(), is_spurious),
                    detections->end());
}

QString
BuildExpression(const std::vector<ciface::Core::DeviceContainer::InputDetection>& detections,
                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  std::vector<const ciface::Core::DeviceContainer::InputDetection*> pressed_inputs;

  QStringList alternations;

  const auto get_control_expression = [&](auto& detection) {
    // Return the parent-most name if there is one for better hotkey strings.
    // Detection of L/R_Ctrl will be changed to just Ctrl.
    // Users can manually map L_Ctrl if they so desire.
    const auto input = (quote == Quote::On) ?
                           detection.device->GetParentMostInput(detection.input) :
                           detection.input;

    ciface::Core::DeviceQualifier device_qualifier;
    device_qualifier.FromDevice(detection.device.get());

    return MappingCommon::GetExpressionForControl(QString::fromStdString(input->GetName()),
                                                  device_qualifier, default_device, quote);
  };

  bool new_alternation = false;

  const auto handle_press = [&](auto& detection) {
    pressed_inputs.emplace_back(&detection);
    new_alternation = true;
  };

  const auto handle_release = [&]() {
    if (!new_alternation)
      return;

    new_alternation = false;

    QStringList alternation;
    for (auto* input : pressed_inputs)
      alternation.push_back(get_control_expression(*input));

    const bool is_hotkey = pressed_inputs.size() >= 2 &&
                           (pressed_inputs[1]->press_time - pressed_inputs[0]->press_time) >
                               HOTKEY_VS_CONJUNCION_THRESHOLD;

    if (is_hotkey)
    {
      alternations.push_back(QStringLiteral("@(%1)").arg(alternation.join(QLatin1Char('+'))));
    }
    else
    {
      alternation.sort();
      alternations.push_back(alternation.join(QLatin1Char('&')));
    }
  };

  for (auto& detection : detections)
  {
    // Remove since released inputs.
    for (auto it = pressed_inputs.begin(); it != pressed_inputs.end();)
    {
      if (!((*it)->release_time > detection.press_time))
      {
        handle_release();
        it = pressed_inputs.erase(it);
      }
      else
        ++it;
    }

    handle_press(detection);
  }

  handle_release();

  alternations.removeDuplicates();
  return alternations.join(QLatin1Char('|'));
}

}  // namespace MappingCommon
