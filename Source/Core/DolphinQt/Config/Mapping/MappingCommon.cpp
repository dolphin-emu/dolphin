// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <chrono>

#include <QApplication>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QTimer>

#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"

#include "Common/Thread.h"

namespace MappingCommon
{
constexpr auto INPUT_DETECT_INITIAL_TIME = std::chrono::seconds(3);
constexpr auto INPUT_DETECT_CONFIRMATION_TIME = std::chrono::milliseconds(0);
constexpr auto INPUT_DETECT_MAXIMUM_TIME = std::chrono::seconds(5);

constexpr auto OUTPUT_TEST_TIME = std::chrono::seconds(2);

QString DetectExpression(QPushButton* button, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device,
                         ciface::MappingCommon::Quote quote)
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

  ciface::MappingCommon::RemoveSpuriousTriggerCombinations(&detections);

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

  return QString::fromStdString(BuildExpression(detections, default_device, quote));
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

}  // namespace MappingCommon
