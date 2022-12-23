#include <windows.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/windows.graphics.display.core.h>

#include <Gamingdeviceinformation.h>

#include <Common/WindowSystemInfo.h>
#include <UICommon/UICommon.h>

#include <Core/Boot/Boot.h>
#include <Core/BootManager.h>
#include <Core/Core.h>
#include <Core/HW/ProcessorInterface.h>
#include <Core/Host.h>
#include <Core/IOS/STM/STM.h>

#define SDL_MAIN_HANDLED

using namespace winrt;

using namespace Windows;
using namespace Windows::Storage;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Composition;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;
using namespace Windows::Graphics::Display::Core;

using winrt::Windows::UI::Core::BackRequestedEventArgs;
using winrt::Windows::UI::Core::CoreProcessEventsOption;
using winrt::Windows::UI::Core::CoreWindow;

Common::Flag m_running{true};
Common::Flag m_shutdown_requested{false};
Common::Flag m_tried_graceful_shutdown{false};
winrt::hstring m_launchOnExit;

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
  IFrameworkView CreateView() { return *this; }

  void Initialize(CoreApplicationView const& v)
  {
    v.Activated({this, &App::OnActivate});
    CoreApplication::EnteredBackground({this, &App::EnteredBackground});
    CoreApplication::Suspending({this, &App::Suspending});
  }

  void Load(hstring const&) {}

  void Uninitialize() {}

  void Run()
  {
    while (m_running.IsSet())
    {
      if (m_shutdown_requested.TestAndClear())
      {
        const auto ios = IOS::HLE::GetIOS();
        const auto stm = ios ? ios->GetDeviceByName("/dev/stm/eventhook") : nullptr;
        if (!m_tried_graceful_shutdown.IsSet() && stm &&
            std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->HasHookInstalled())
        {
          ProcessorInterface::PowerButton_Tap();
          m_tried_graceful_shutdown.Set();
        }
        else
        {
          m_running.Clear();
        }
      }

      ::Core::HostDispatchJobs();
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  winrt::fire_and_forget InitializeDolphinFromFile(std::string path)
  {
    if (path.empty())
    {
      FileOpenPicker openPicker;
      openPicker.ViewMode(PickerViewMode::List);
      openPicker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
      openPicker.FileTypeFilter().Append(L".iso");
      openPicker.FileTypeFilter().Append(L".ciso");
      openPicker.FileTypeFilter().Append(L".rvz");
      openPicker.FileTypeFilter().Append(L".wbfs");
      openPicker.FileTypeFilter().Append(L".gcm");
      openPicker.FileTypeFilter().Append(L".gcz");
      openPicker.FileTypeFilter().Append(L".json");

      auto file = co_await openPicker.PickSingleFileAsync();
      if (file)
      {
        path = winrt::to_string(file.Path().data());
      }
    }

    CoreWindow window = CoreWindow::GetForCurrentThread();
    void* abi = winrt::get_abi(window);

    WindowSystemInfo wsi;
    wsi.type = WindowSystemType::UWP;
    wsi.render_window = abi;
    wsi.render_surface = abi;
    wsi.render_width = window.Bounds().Width;
    wsi.render_height = window.Bounds().Height;

    auto navigation = winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView();

    // UWP on Xbox One triggers a back request whenever the B button is pressed
    // which can result in the app being suspended if unhandled
    navigation.BackRequested([](const winrt::Windows::Foundation::IInspectable&,
                                const BackRequestedEventArgs& args) { args.Handled(true); });

    GAMING_DEVICE_MODEL_INFORMATION info = {};
    GetGamingDeviceModelInformation(&info);
    if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT)
    {
      HdmiDisplayInformation hdi = HdmiDisplayInformation::GetForCurrentView();
      if (hdi)
      {
        wsi.render_width = hdi.GetCurrentDisplayMode().ResolutionWidthInRawPixels();
        wsi.render_height = hdi.GetCurrentDisplayMode().ResolutionHeightInRawPixels();
      }
    }

    std::unique_ptr<BootParameters> boot =
        BootParameters::GenerateFromFile(path, BootSessionData("", DeleteSavestateAfterBoot::No));

    UICommon::SetUserDirectory(winrt::to_string(ApplicationData::Current().LocalFolder().Path()));
    UICommon::CreateDirectories();
    UICommon::Init();
    UICommon::InitControllers(wsi);

    if (!BootManager::BootCore(std::move(boot), wsi))
    {
      fprintf(stderr, "Could not boot the specified file\n");
    }
  }

  void SetWindow(CoreWindow const& w) { w.Closed({this, &App::OnClosed}); }

  void OnClosed(const IInspectable&, const winrt::Windows::UI::Core::CoreWindowEventArgs& args)
  {
    m_shutdown_requested.Set();
  }

  void OnActivate(const winrt::Windows::ApplicationModel::Core::CoreApplicationView&,
                  const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& args)
  {
    std::stringstream filePath;

    if (args.Kind() == Windows::ApplicationModel::Activation::ActivationKind::Protocol)
    {
      auto protocolActivatedEventArgs{
          args.as<Windows::ApplicationModel::Activation::ProtocolActivatedEventArgs>()};
      auto query = protocolActivatedEventArgs.Uri().QueryParsed();

      for (uint32_t i = 0; i < query.Size(); i++)
      {
        auto arg = query.GetAt(i);

        // parse command line string
        if (arg.Name() == winrt::hstring(L"cmd"))
        {
          std::string argVal = winrt::to_string(arg.Value());

          // Strip the executable from the cmd argument
          if (argVal.starts_with("dolphin.exe"))
          {
            argVal = argVal.substr(11, argVal.length());
          }

          std::istringstream iss(argVal);
          std::string s;

          // Maintain slashes while reading the quotes
          while (iss >> std::quoted(s, '"', (char)0))
          {
            filePath << s;
          }
        }
        else if (arg.Name() == winrt::hstring(L"launchOnExit"))
        {
          // For if we want to return to a frontend
          m_launchOnExit = arg.Value();
        }
      }
    }

    // Defaults to file picker if no path is present.
    InitializeDolphinFromFile(filePath.str());

    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();
  }

  void EnteredBackground(const IInspectable&,
                         const winrt::Windows::ApplicationModel::EnteredBackgroundEventArgs& args)
  {
  }

  void Suspending(const IInspectable&,
                  const winrt::Windows::ApplicationModel::SuspendingEventArgs& args)
  {
    m_shutdown_requested.Set();

    // The Series S/X quits fast, so let's immediately shutdown to ensure all the caches save.
    Core::Stop();
    Core::Shutdown();
    UICommon::Shutdown();

    if (!m_launchOnExit.empty())
    {
      winrt::Windows::Foundation::Uri m_uri{m_launchOnExit};
      auto asyncOperation = winrt::Windows::System::Launcher::LaunchUriAsync(m_uri);
      asyncOperation.Completed([](winrt::Windows::Foundation::IAsyncOperation<bool> const& sender,
                                  winrt::Windows::Foundation::AsyncStatus const asyncStatus) {
        CoreApplication::Exit();
      });
    }
  }
};

int WINAPIV WinMain()
{
  winrt::init_apartment();

  CoreApplication::Run(make<App>());

  winrt::uninit_apartment();

  return 0;
}

std::vector<std::string> Host_GetPreferredLocales()
{
  return {};
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

void Host_Message(HostMessageID id)
{
}

void Host_UpdateTitle(const std::string& title)
{
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererHasFullFocus()
{
  // Mouse capturing isn't implemented
  return Host_RendererHasFocus();
}

bool Host_RendererIsFullscreen()
{
  return true;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

void Host_UpdateDiscordClientID(const std::string& client_id)
{
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
  return false;
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}
