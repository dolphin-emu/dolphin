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
#include "HW/Memmap.h"
#include "Movie.h"

static bool boolMatchStart = false;
static bool boolMatchEnd = false;
static bool wroteCodes = false;
static bool overwroteHomeCaptainPosTraining = false;
static bool customTrainingModeStart = false;
static std::vector<u8> training_mode_buffer;

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

void StateAuxillary::setMatchPlayerNames()
{
  bool leftTeamNetPlayFound = false;
  bool rightTeamNetPlayFound = false;

  int leftTeamNetPlayPort = -1;
  int rightTeamNetPlayPort = -1;

  std::string leftTeamNetPlayName = "";
  std::string rightTeamNetPlayName = "";

  if (NetPlay::IsNetPlayRunning())
  {
    int currentPortValue = Memory::Read_U16(Metadata::addressControllerPort1);
    if (currentPortValue == 0)
    {
      leftTeamNetPlayPort = 0;
      leftTeamNetPlayFound = true;
    }
    else if (currentPortValue == 1)
    {
      rightTeamNetPlayPort = 0;
      rightTeamNetPlayFound = true;
    }

    currentPortValue = Memory::Read_U16(Metadata::addressControllerPort2);
    if (currentPortValue == 0)
    {
      leftTeamNetPlayPort = 1;
      leftTeamNetPlayFound = true;
    }
    else if (currentPortValue == 1)
    {
      rightTeamNetPlayPort = 1;
      rightTeamNetPlayFound = true;
    }

    // just check this to save read from mem time
    if (!leftTeamNetPlayFound || !rightTeamNetPlayFound)
    {
      currentPortValue = Memory::Read_U16(Metadata::addressControllerPort3);
      if (currentPortValue == 0)
      {
        leftTeamNetPlayPort = 2;
        leftTeamNetPlayFound = true;
      }
      else if (currentPortValue == 1)
      {
        rightTeamNetPlayPort = 2;
        rightTeamNetPlayFound = true;
      }
    }

    // just check this to save read from mem time
    if (!leftTeamNetPlayFound || !rightTeamNetPlayFound)
    {
      currentPortValue = Memory::Read_U16(Metadata::addressControllerPort4);
      if (currentPortValue == 0)
      {
        leftTeamNetPlayPort = 3;
        leftTeamNetPlayFound = true;
      }
      else if (currentPortValue == 1)
      {
        rightTeamNetPlayPort = 3;
        rightTeamNetPlayFound = true;
      }
    }

    std::vector<const NetPlay::Player*> playerArray = Metadata::getPlayerArray();

    if (leftTeamNetPlayFound)
    {
      // netplay is running and we have at least one player on both sides
      // therefore, we can get their player ids from pad map now since we know the ports
      // using the id's we can get their player names

      // getting left team
      // so we're going to go to m_pad_map and find what player id is at port 0
      // then we're going to search for that player id in our player array and return their name
      NetPlay::PlayerId pad_map_player_id = netplayGCMap[leftTeamNetPlayPort];
      // now get the player name that matches the PID we just stored
      for (int j = 0; j < playerArray.size(); j++)
      {
        if (playerArray.at(j)->pid == pad_map_player_id)
        {
          leftTeamNetPlayName = playerArray.at(j)->name;
          break;
        }
      }

      // write the names to their spots in memory
      // need to format name to go .P.o.o.l.B.o.i...
      int index = 0;
      Memory::Write_U8(00, 0x80400018 + index);
      index++;
      for (char& c : leftTeamNetPlayName)
      {
        Memory::Write_U8(c, 0x80400018 + index);
        index++;
        Memory::Write_U8(00, 0x80400018 + index);
        index++;
      }
      for (int i = 0; i < 3; i++)
      {
        Memory::Write_U8(00, 0x80400018 + index);
        index++;
      }
      Memory::Write_U8(01, 0x80400014);
    }
    else
    {
      Memory::Write_U8(00, 0x80400014);
    }

    if (rightTeamNetPlayFound)
    {
      // getting right team
      NetPlay::PlayerId pad_map_player_id = netplayGCMap[rightTeamNetPlayPort];
      for (int j = 0; j < playerArray.size(); j++)
      {
        if (playerArray.at(j)->pid == pad_map_player_id)
        {
          rightTeamNetPlayName = playerArray.at(j)->name;
          break;
        }
      }

      // write the names to their spots in memory
      // need to format name to go .P.o.o.l.B.o.i...
      int index = 0;
      Memory::Write_U8(00, 0x80400040 + index);
      index++;
      for (char& c : rightTeamNetPlayName)
      {
        Memory::Write_U8(c, 0x80400040 + index);
        index++;
        Memory::Write_U8(00, 0x80400040 + index);
        index++;
      }
      for (int i = 0; i < 3; i++)
      {
        Memory::Write_U8(00, 0x80400040 + index);
        index++;
      }
      Memory::Write_U8(01, 0x80400015);
    }
    else
    {
      Memory::Write_U8(00, 0x80400015);
    }
  }
}

void StateAuxillary::saveState(const std::string& filename, bool wait) {
  std::thread t1(&State::SaveAs, filename, wait);
  //State::SaveAs(filename, wait);
  t1.detach();
}

void StateAuxillary::saveStateToTrainingBuffer()
{
  State::SaveToBuffer(training_mode_buffer);
}

void StateAuxillary::loadStateFromTrainingBuffer()
{
  State::LoadFromBuffer(training_mode_buffer);
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

// this should be updated to delete the files from where they came from which is not always the citrus replays dir
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

bool StateAuxillary::getOverwriteHomeCaptainPositionTrainingMode()
{
  return overwroteHomeCaptainPosTraining;
}

void StateAuxillary::setOverwriteHomeCaptainPositionTrainingMode(bool boolValue)
{
  overwroteHomeCaptainPosTraining = boolValue;
}

bool StateAuxillary::getCustomTrainingModeStart()
{
  return customTrainingModeStart;
}

void StateAuxillary::setCustomTrainingModeStart(bool boolValue)
{
  customTrainingModeStart = boolValue;
}
