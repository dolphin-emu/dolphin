

#include "Common/IniFile.h"
#include "Core/LocalPlayers.h"
#include "Common/FileUtil.h"

namespace LocalPlayers
{
// gets a vector of all the local players
std::vector<LocalPlayers::Player> LocalPlayers::GetPlayers(const IniFile& localIni)
{
  std::vector<LocalPlayers::Player> players;
  LocalPlayers::Player defaultPlayer{"No Player Selected", "0"};
  players.push_back(defaultPlayer);

  for (const IniFile* ini : {&localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Local_Players_List", &lines, false);

    for (auto& line : lines)
    {
      LocalPlayers::Player player = toLocalPlayer(line);
       if (!player.username.empty() && player.username != defaultPlayer.username && player.userid != defaultPlayer.userid)
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
// NOTE: reads from the file
std::map<int, LocalPlayers::Player> LocalPlayers::GetPortPlayers()
{
  std::map<int, LocalPlayers::Player> LocalPortPlayers;

  IniFile local_players_ini;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  const std::vector<LocalPlayers::Player> LocalPlayersList = GetPlayers(local_players_ini); // list of all available local players

  IniFile::Section* localplayers = local_players_ini.GetOrCreateSection("Local Players");
  const IniFile::Section::SectionMap portmap = localplayers->GetValues();

  u8 portnum = 1;
  for (const auto& name : portmap)
  {
    std::string playerStr = name.second;

    LocalPortPlayers[portnum] = toLocalPlayer(playerStr);
    portnum++;
  }

  return LocalPortPlayers;
}


// converts a string to a LocalPlayers object
LocalPlayers::Player LocalPlayers::toLocalPlayer(std::string playerStr)
{
  LocalPlayers::Player player;

  std::istringstream ss(playerStr);
  ss.imbue(std::locale::classic());

  switch ((playerStr)[0])
  {
  case '+':
    ss.seekg(1, std::ios_base::cur);

    // read the username
    std::getline(ss, player.username,
                 '[');  // stop at [ character (beginning of uid)

    // read the uid
    std::getline(ss, player.userid, ']');
    break;
    break;
  }

  return player;
}

// converts local players to a str
std::string LocalPlayers::Player::LocalPlayerToStr()
{
  std::string title = '+' + this->username + "[" + this->userid + "]";
  return title;
}

std::vector<std::string> LocalPlayers::Player::GetUserInfo(std::string playerStr)
{
  std::string a_username;
  std::string a_userid;
  std::vector<std::string> a_userInfo;

  std::istringstream ss(playerStr);
  ss.imbue(std::locale::classic());

  switch ((playerStr)[0])
  {
  case '+':
    // player = LocalPlayers();
    ss.seekg(1, std::ios_base::cur);

    // read the username
    std::getline(ss, a_username,
                 '[');  // stop at [ character (beginning of uid)

    // read the uid
    std::getline(ss, a_userid, ']');
    break;
    break;
  }
  a_userInfo.push_back(a_username);
  a_userInfo.push_back(a_userid);
  return a_userInfo;
}

std::string LocalPlayers::Player::GetUsername()
{
  return GetUserInfo(this->LocalPlayerToStr())[0];
}

std::string LocalPlayers::Player::GetUserID()
{
  return GetUserInfo(this->LocalPlayerToStr())[1];
}

void LocalPlayers::Player::SetUserInfo(LocalPlayers::Player player)
{
  this->username = player.GetUsername();
  this->userid = player.GetUserID();
} 

}  // namespace LocalPlayers
