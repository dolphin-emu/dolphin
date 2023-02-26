#include "StateAuxillary.h"
#include "Core/State.h"
#include <thread>
#include <Core/HW/SI/SI_Device.h>
#include <Core/Config/MainSettings.h>
#include <Core/Config/WiimoteSettings.h>
#include <Core/HW/Wiimote.h>
#include <Core/Core.h>
#include <Core/Metadata.h>
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

static bool boolMatchStart = false;
static bool boolMatchEnd = false;
static bool wroteCodes = false;

static NetPlay::PadMappingArray netplayGCMap;
// even if the player is using multiple netplay ports to play, say 1 and 3, the game only needs the first one to do proper playback
// therefore, we can use a single int instead of an array
static int ourNetPlayPort;
static std::vector<int> ourNetPlayPortsVector(4);
static SerialInterface::SIDevices preMoviePort0;
static SerialInterface::SIDevices preMoviePort1;
static SerialInterface::SIDevices preMoviePort2;
static SerialInterface::SIDevices preMoviePort3;

bool StateAuxillary::getBoolMatchStart()
{
  return boolMatchStart;
}

bool StateAuxillary::getBoolMatchEnd()
{
  return boolMatchEnd;
}

bool StateAuxillary::getBoolWroteCodes()
{
  return wroteCodes;
}

void StateAuxillary::setBoolMatchStart(bool boolValue)
{
  boolMatchStart = boolValue;
}

void StateAuxillary::setBoolMatchEnd(bool boolValue)
{
  boolMatchEnd = boolValue;
}

void StateAuxillary::setBoolWroteCodes(bool boolValue)
{
  wroteCodes = boolValue;
}

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
  std::thread t3(&Metadata::writeJSON, jsonString, true);
  t3.detach();
}

void StateAuxillary::endPlayback()
{
  /*
  PWSTR path;
  SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &path);
  std::wstring strpath(path);
  CoTaskMemFree(path);
  std::string documents_file_path(strpath.begin(), strpath.end());

  std::string replays_path = documents_file_path;
  replays_path += "\\Citrus Replays\\";
  */
  std::string replays_path = File::GetUserPath(D_CITRUSREPLAYS_IDX);
  std::string fileArr[4] = {"output.dtm", "output.dtm.sav", "output.json", "diffFile.patch"};
  for (int i = 0; i < 4; i++)
  {
    std::string innerFileName = replays_path + fileArr[i];
    if (File::Exists(innerFileName))
    {
      std::filesystem::remove(innerFileName);
    }
  }
}

void StateAuxillary::setNetPlayControllers(NetPlay::PadMappingArray m_pad_map, NetPlay::PlayerId m_pid)
{
  netplayGCMap = m_pad_map;
  for (int i = 0; i < 4; i++)
  {
    if (m_pad_map[i] == m_pid)
    {
      ourNetPlayPortsVector.at(i) = 1;
    }
    else
    {
      ourNetPlayPortsVector.at(i) = 0;
    }
  }
}

// old and should not be used
int StateAuxillary::getOurNetPlayPort()
{
  return ourNetPlayPort;
}

std::vector<int> StateAuxillary::getOurNetPlayPorts()
{
  return ourNetPlayPortsVector;
}

bool StateAuxillary::isSpectator()
{
  if (!NetPlay::IsNetPlayRunning())
  {
    return false;
  }
  for (int i = 0; i < 4; i++)
  {
    if (ourNetPlayPortsVector.at(i) == 1)
    {
      return false;
    }
  }
  return true;
}

void StateAuxillary::setPrePort(SerialInterface::SIDevices currentPort0,
                                SerialInterface::SIDevices currentPort1,
                                SerialInterface::SIDevices currentPort2,
                                SerialInterface::SIDevices currentPort3)
{
  preMoviePort0 = currentPort0;
  preMoviePort1 = currentPort1;
  preMoviePort2 = currentPort2;
  preMoviePort3 = currentPort3;
}

void StateAuxillary::setPostPort()
{
  Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(static_cast<int>(0)), preMoviePort0);
  Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(static_cast<int>(1)), preMoviePort1);
  Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(static_cast<int>(2)), preMoviePort2);
  Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(static_cast<int>(3)), preMoviePort3);
  SConfig::GetInstance().SaveSettings();
}
