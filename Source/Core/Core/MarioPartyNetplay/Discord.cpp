#include "Discord.h"
#include "UICommon/DiscordPresence.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Core.h"
#include "Core/IOS/DolphinDevice.h"
#include <Core/State.h>
#include "Core/System.h"

bool mpn_update_discord()
{
  //if (!Memory::IsInitialized())
  //  return false;

  DiscordRichPresence RichPresence = {};

  RichPresence.largeImageKey = CurrentState.Image ? CurrentState.Image : "default";
  RichPresence.largeImageText = CurrentState.Title ? CurrentState.Title : "In-Game";

  if (CurrentState.Scenes != NULL && CurrentState.Scene != NULL)
    RichPresence.state = CurrentState.Scene->Name.c_str();

  if (mpn_read_value(CurrentState.Addresses->CurrentTurn, 1) == (mpn_read_value(CurrentState.Addresses->TotalTurns, 1) + 1))
  {
    State::Save(Core::System::GetInstance(), 1);
  }
  if (CurrentState.Addresses != NULL)
  {
    char Details[128] = "";

    if (CurrentState.Boards && CurrentState.Board)
    {
      DiscordRichPresence discord_presence = {};
      
      snprintf(Details, sizeof(Details), "Players: 1/4 Turn: %d/%d",
               mpn_read_value(CurrentState.Addresses->CurrentTurn, 1),
               mpn_read_value(CurrentState.Addresses->TotalTurns, 1));

      RichPresence.smallImageKey = CurrentState.Board->Icon.c_str();
      RichPresence.smallImageText = CurrentState.Board->Name.c_str();
    }
    else
    {
      snprintf(Details, sizeof(Details), "Players: 1/4");
      RichPresence.smallImageKey = "";
      RichPresence.smallImageText = "";
    }
    RichPresence.details = Details;
  }

  RichPresence.startTimestamp = std::time(nullptr);
  Discord_UpdatePresence(&RichPresence);

  return true;
}
