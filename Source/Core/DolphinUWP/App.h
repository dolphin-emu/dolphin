#pragma once

#include <UICommon/UICommon.h>
#include <Common/WindowSystemInfo.h>

#include <Core/BootManager.h>
#include <Core/Boot/Boot.h>
#include <Core/Host.h>
#include <Core/Core.h>
#include <Core/IOS/STM/STM.h>
#include <Core/HW/ProcessorInterface.h>

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.ViewManagement.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/base.h>

class App
    : public winrt::implements<App, winrt::Windows::ApplicationModel::Core::IFrameworkViewSource,
                               winrt::Windows::ApplicationModel::Core::IFrameworkView>
{
public:
  winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView();
  void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView&);
  void Load(const winrt::hstring&);
  void Uninitialize();
  void Run();
  void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window);
  void UpdateRunningFlag();
  void OnActivated(CoreApplicationView const& /*applicationView*/,
              winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs const& /*args*/);

private:
  Common::Flag m_running{true};
  Common::Flag m_shutdown_requested{false};
  Common::Flag m_tried_graceful_shutdown{false};
  winrt::Windows::UI::Core::CoreDispatcher m_dispatcher{nullptr};
};
