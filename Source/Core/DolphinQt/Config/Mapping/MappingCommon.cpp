// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <algorithm>
#include <deque>
#include <memory>

#include <QTimer>

#include "DolphinQt/Config/Mapping/MappingButton.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/BlockUserInputFilter.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"
#include "InputCommon/InputConfig.h"

namespace MappingCommon
{
constexpr auto INPUT_DETECT_INITIAL_TIME = std::chrono::seconds(3);
constexpr auto INPUT_DETECT_CONFIRMATION_TIME = std::chrono::milliseconds(750);
constexpr auto INPUT_DETECT_MAXIMUM_TIME = std::chrono::seconds(5);
// Ignore the mouse-click when queuing more buttons with "alternate mappings" enabled.
constexpr auto INPUT_DETECT_ENDING_IGNORE_TIME = std::chrono::milliseconds(50);

class MappingProcessor : public QObject
{
public:
  MappingProcessor(MappingWindow* parent) : QObject{parent}, m_parent{parent}
  {
    using MW = MappingWindow;
    using MP = MappingProcessor;

    connect(parent, &MW::Update, this, &MP::ProcessMappingButtons);
    connect(parent, &MW::ConfigChanged, this, &MP::CancelMapping);

    connect(parent, &MW::UnQueueInputDetection, this, &MP::UnQueueInputDetection);
    connect(parent, &MW::QueueInputDetection, this, &MP::QueueInputDetection);
    connect(parent, &MW::CancelMapping, this, &MP::CancelMapping);

    m_input_detection_start_timer = new QTimer(this);
    m_input_detection_start_timer->setSingleShot(true);
    connect(m_input_detection_start_timer, &QTimer::timeout, this, &MP::StartInputDetection);
  }

  void StartInputDetection()
  {
    const auto& default_device = m_parent->GetController()->GetDefaultDevice();
    auto& button = m_clicked_mapping_buttons.front();

    // Focus just makes it more clear which button is currently being mapped.
    button->setFocus();
    button->setText(tr("[ Press Now ]"));
    QtUtils::InstallKeyboardBlocker(button, button, &MappingButton::ConfigChanged);

    std::vector device_strings{default_device.ToString()};
    if (m_parent->IsCreateOtherDeviceMappingsEnabled())
      device_strings = g_controller_interface.GetAllDeviceStrings();

    m_input_detector = std::make_unique<ciface::Core::InputDetector>();
    const auto lock = m_parent->GetController()->GetStateLock();
    m_input_detector->Start(g_controller_interface, device_strings);
  }

  void ProcessMappingButtons()
  {
    if (!m_input_detector)
      return;

    const auto confirmation_time =
        INPUT_DETECT_CONFIRMATION_TIME * (m_parent->IsWaitForAlternateMappingsEnabled() ? 1 : 0);

    m_input_detector->Update(INPUT_DETECT_INITIAL_TIME, confirmation_time,
                             INPUT_DETECT_MAXIMUM_TIME);

    if (!m_input_detector->IsComplete())
      return;

    auto* const button = m_clicked_mapping_buttons.front();

    auto results = m_input_detector->TakeResults();
    if (!FinalizeMapping(&results))
    {
      // No inputs detected. Cancel this and any other queued mappings.
      CancelMapping();
    }
    else if (m_parent->IsIterativeMappingEnabled() && m_clicked_mapping_buttons.empty())
    {
      button->QueueNextButtonMapping();

      if (m_clicked_mapping_buttons.empty())
        return;

      // Skip "Modifier" mappings when using analog inputs.
      auto* next_button = m_clicked_mapping_buttons.front();
      if (next_button->GetControlType() == MappingButton::ControlType::ModifierInput &&
          std::ranges::any_of(results, &ciface::Core::InputDetector::Detection::IsAnalogPress))
      {
        // Clear "Modifier" mapping and queue the next button.
        SetButtonExpression(next_button, "");
        UnQueueInputDetection(next_button);
        next_button->QueueNextButtonMapping();
      }
    }
  }

  bool FinalizeMapping(ciface::Core::InputDetector::Results* detections_ptr)
  {
    auto& detections = *detections_ptr;
    if (!ciface::MappingCommon::ContainsCompleteDetection(detections))
      return false;

    ciface::MappingCommon::RemoveSpuriousTriggerCombinations(&detections);

    const auto& default_device = m_parent->GetController()->GetDefaultDevice();
    auto& button = m_clicked_mapping_buttons.front();
    SetButtonExpression(
        button, BuildExpression(detections, default_device, ciface::MappingCommon::Quote::On));

    UnQueueInputDetection(button);
    return true;
  }

  void SetButtonExpression(MappingButton* button, const std::string& expression)
  {
    auto* const control_reference = button->GetControlReference();
    control_reference->SetExpression(expression);
    m_parent->Save();
    m_parent->GetController()->UpdateSingleControlReference(g_controller_interface,
                                                            control_reference);
    m_parent->GetController()->GetConfig()->GenerateControllerTextures();
  }

  void UpdateInputDetectionStartTimer()
  {
    m_input_detector.reset();

    if (m_clicked_mapping_buttons.empty())
      m_input_detection_start_timer->stop();
    else
      m_input_detection_start_timer->start(INPUT_DETECT_INITIAL_DELAY);
  }

  bool UnQueueInputDetection(MappingButton* button)
  {
    button->ConfigChanged();
    if (!std::erase(m_clicked_mapping_buttons, button))
      return false;
    UpdateInputDetectionStartTimer();
    return true;
  }

  void QueueInputDetection(MappingButton* button)
  {
    // Just UnQueue if already queued.
    if (UnQueueInputDetection(button))
      return;

    button->setText(QStringLiteral("[ ... ]"));
    m_clicked_mapping_buttons.push_back(button);

    if (m_input_detector)
    {
      // Ignore the mouse-click that queued this new detection and finalize the current mapping.
      auto results = m_input_detector->TakeResults();
      ciface::MappingCommon::RemoveDetectionsAfterTimePoint(
          &results, Clock::now() - INPUT_DETECT_ENDING_IGNORE_TIME);
      FinalizeMapping(&results);
    }
    UpdateInputDetectionStartTimer();
  }

  void CancelMapping()
  {
    // Signal buttons to take on their proper input expression text.
    for (auto* button : m_clicked_mapping_buttons)
      button->ConfigChanged();

    m_clicked_mapping_buttons = {};
    UpdateInputDetectionStartTimer();
  }

private:
  std::deque<MappingButton*> m_clicked_mapping_buttons;
  std::unique_ptr<ciface::Core::InputDetector> m_input_detector;
  QTimer* m_input_detection_start_timer;
  MappingWindow* const m_parent;
};

void CreateMappingProcessor(MappingWindow* window)
{
  new MappingProcessor{window};
}

}  // namespace MappingCommon
