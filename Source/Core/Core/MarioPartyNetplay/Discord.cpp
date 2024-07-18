#include "Discord.h"
#include <Core/State.h>
#include "Core/Config/NetplaySettings.h"
#include "Core/Core.h"
#include "Core/IOS/DolphinDevice.h"
#include "Core/System.h"
#include "UICommon/DiscordPresence.h"

bool mpn_update_discord()
{
  // if (!Memory::IsInitialized())
  //   return false;

  DiscordRichPresence RichPresence = {};

  RichPresence.largeImageKey = CurrentState.Image ? CurrentState.Image : "default";
  RichPresence.largeImageText = CurrentState.Title ? CurrentState.Title : "In-Game";

  if (CurrentState.Scenes != NULL && CurrentState.Scene != NULL)
    RichPresence.state = CurrentState.Scene->Name.c_str();

  if (CurrentState.Addresses != NULL)
  {
    if (mpn_read_value(CurrentState.Addresses->CurrentTurn, 1) ==
        (mpn_read_value(CurrentState.Addresses->TotalTurns, 1) + 1))
    {
      State::Save(Core::System::GetInstance(), 1);
    }

    // Add controller port values
    int value1 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress1, 1);
    int value2 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress2, 1);
    int value3 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress3, 1);
    int value4 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress4, 1);

    // Modify values based on the given conditions
    value1 = (value1 == 0) ? 1 : 0;
    value2 = (value2 == 0) ? 1 : 0;
    value3 = (value3 == 0) ? 1 : 0;
    value4 = (value4 == 0) ? 1 : 0;

    int controller_port_sum = value1 + value2 + value3 + value4;

    char Details[128] = "";

    if (CurrentState.Boards && CurrentState.Board)
    {
      snprintf(Details, sizeof(Details), "Players: %d/4 Turn: %d/%d", controller_port_sum,
               mpn_read_value(CurrentState.Addresses->CurrentTurn, 1),
               mpn_read_value(CurrentState.Addresses->TotalTurns, 1));

      RichPresence.smallImageKey = CurrentState.Board->Icon.c_str();
      RichPresence.smallImageText = CurrentState.Board->Name.c_str();
    }
    else
    {
      snprintf(Details, sizeof(Details), "Players: %d/4", controller_port_sum);
      RichPresence.smallImageKey = "";
      RichPresence.smallImageText = "";
    }
    RichPresence.details = Details;
  }
  else
  {
    // Handle the case where CurrentState.Addresses is NULL
    RichPresence.details = "Invalid state: Addresses are NULL";
    RichPresence.smallImageKey = "";
    RichPresence.smallImageText = "";
  }

  RichPresence.startTimestamp = std::time(nullptr);
  Discord_UpdatePresence(&RichPresence);

  return true;
}
