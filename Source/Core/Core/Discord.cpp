#include "Discord.h"

bool mpn_update_discord()
{
  if (!Memory::IsInitialized())
    return false;

  DiscordRichPresence RichPresence = {};

  RichPresence.largeImageKey = CurrentState.Image ? CurrentState.Image : "default-pj64";
  RichPresence.largeImageText = CurrentState.Title ? CurrentState.Title : "In-Game";

  if (CurrentState.Scenes != NULL && CurrentState.Scene != NULL)
    RichPresence.state = CurrentState.Scene->Name.c_str();

  if (CurrentState.Addresses != NULL)
  {
    char Details[128] = "";

    if (CurrentState.Boards && CurrentState.Board)
    {
      snprintf(Details, sizeof(Details), "Turn: %d/%d",
               mpn_read_value(CurrentState.Addresses->CurrentTurn, 1),
               mpn_read_value(CurrentState.Addresses->TotalTurns, 1));
      RichPresence.smallImageKey = CurrentState.Board->Icon.c_str();
      RichPresence.smallImageText = CurrentState.Board->Name.c_str();
    }
    else
    {
      RichPresence.smallImageKey = "";
      RichPresence.smallImageText = "";
    }
    RichPresence.details = Details;
  }
  Discord_UpdatePresence(&RichPresence);

  return true;
}
