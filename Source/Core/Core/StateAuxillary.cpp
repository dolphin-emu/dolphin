#include "StateAuxillary.h"
#include "Core/State.h"
#include <thread>
#include <Core/HW/SI/SI_Device.h>
#include <Core/Config/MainSettings.h>
#include <Core/Config/WiimoteSettings.h>
#include <Core/HW/Wiimote.h>
#include <Core/Core.h>
#include <Core/Metadata.h>
#include <ShlObj_core.h>

static NetPlay::PadMappingArray netplayGCMap;

void StateAuxillary::saveState(const std::string& filename, bool wait) {
  std::thread t1(&State::SaveAs, filename, wait);
  //State::SaveAs(filename, wait);
  t1.detach();
}

void StateAuxillary::startRecording()
{
  Movie::SetReadOnly(false);
  Movie::ControllerTypeArray controllers{};
  Movie::WiimoteEnabledArray wiimotes{};
  // this is how they're set up in mainwindow.cpp

  if (NetPlay::IsNetPlayRunning())
  {
    for (unsigned int i = 0; i < 4; ++i)
    {
      if (netplayGCMap[i] > 0)
      {
        controllers[i] = Movie::ControllerType::GC;
      }
      else
      {
        controllers[i] = Movie::ControllerType::None;
      }
    }
  }
  else
  {
    for (int i = 0; i < 4; i++)
    {
      const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
      if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
        controllers[i] = Movie::ControllerType::GBA;
      else if (SerialInterface::SIDevice_IsGCController(si_device))
        controllers[i] = Movie::ControllerType::GC;
      else
        controllers[i] = Movie::ControllerType::None;
      wiimotes[i] = Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::None;
    }
  }
  std::thread t2(&Movie::BeginRecordingInput, controllers, wiimotes);
  t2.detach();
}

void StateAuxillary::stopRecording(const std::string replay_path, tm* matchDateTimeParam)
{
  // not moving this to its own thread as of now
  if (Movie::IsRecordingInput())
    Core::RunAsCPUThread([=] {
      Movie::SaveRecording(replay_path);
    });
  if (Movie::IsMovieActive())
  {
    Movie::EndPlayInput(false);
  }
  Metadata::setMatchMetadata(matchDateTimeParam);
  std::string jsonString = Metadata::getJSONString();
  Metadata::writeJSON(jsonString, true);
}

void StateAuxillary::endPlayback()
{
  PWSTR path;
  SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &path);
  std::wstring strpath(path);
  CoTaskMemFree(path);
  std::string documents_file_path(strpath.begin(), strpath.end());
  std::string replays_path = documents_file_path;
  replays_path += "\\Citrus Replays\\";
  std::string fileArr[3] = {"output.dtm", "output.dtm.sav", "output.json"};
  for (int i = 0; i < 3; i++)
  {
    std::string innerFileName = replays_path + fileArr[i];
    std::filesystem::remove(innerFileName);
  }
}

void StateAuxillary::setNetPlayControllers(NetPlay::PadMappingArray m_pad_map)
{
  netplayGCMap = m_pad_map;
}
