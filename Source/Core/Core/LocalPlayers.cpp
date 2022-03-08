

#include "Common/IniFile.h"
#include "Core/LocalPlayers.h"
#include "Common/FileUtil.h"

namespace LocalPlayers
{
// gets a vector of all the local players
std::vector<LocalPlayers> LocalPlayers::GetPlayers(const IniFile& localIni)
{
  std::vector<LocalPlayers> players;

  for (const IniFile* ini : {&localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Local_Players_List", &lines, false);

    LocalPlayers player;

    for (auto& line : lines)
    {
      player = toLocalPlayer(line);
       if (!player.username.empty())
        players.push_back(player);
    }

    // do i need this?
    // add the last username
    //if (!player.username.empty())
    //{
    //  players.push_back(player);
    //}

  }
  return players;
}


// returns map of local players set to each port 
std::map<int, LocalPlayers> LocalPlayers::GetPortPlayers()
{
  std::map<int, LocalPlayers> LocalPortPlayers;

  IniFile local_players_ini;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  const std::vector<LocalPlayers> LocalPlayersList = GetPlayers(local_players_ini); // list of all available local players

  IniFile::Section* localplayers = local_players_ini.GetOrCreateSection("Local Players");
  const IniFile::Section::SectionMap portmap = localplayers->GetValues();

  u8 portnum = 1;
  for (const auto& name : portmap)
  {
    LocalPlayers player;
    std::string playerStr = name.second;

    LocalPortPlayers[portnum] = toLocalPlayer(playerStr);
    portnum++;
  }

  return LocalPortPlayers;
}


// converts a string to a LocalPlayers object
LocalPlayers LocalPlayers::toLocalPlayer(std::string playerStr)
{
  LocalPlayers player;

  std::istringstream ss(playerStr);
  ss.imbue(std::locale::classic());

  switch ((playerStr)[0])
  {
  case '+':
    //if (!player.username.empty())
    //  players.push_back(player);

    player = LocalPlayers();
    ss.seekg(1, std::ios_base::cur);
    // read the username
    std::getline(ss, player.username,
                 '[');  // stop at [ character (beginning of uid)
    player.username = StripSpaces(player.username);
    // read the uid
    std::getline(ss, player.userid, ']');
    break;
    break;
  }

  return player;
}

// converts local players to a str
std::string LocalPlayers::LocalPlayerToStr()
{
  std::string title = '+' + this->username + "[" + this->userid + "]";
  return title;
}

}  // namespace LocalPlayers
