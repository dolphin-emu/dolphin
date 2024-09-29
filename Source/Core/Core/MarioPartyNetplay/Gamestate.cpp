#include "Gamestate.h"
#include "Core/System.h"

mpn_state_t CurrentState;


bool mpn_init_state()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  if (!memory.IsInitialized())
    return false;

  switch (mpn_read_value(0x00000000, 4))
  {
  case MPN_GAMEID_MP4:
    CurrentState.Addresses = &MP4_ADDRESSES;
    CurrentState.Boards = MP4_BOARDS;
    CurrentState.Image = "box-mp4";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP4_GAMESTATES;
    CurrentState.Title = "Mario Party 4";
    break;
  case MPN_GAMEID_MP5:
    CurrentState.Addresses = &MP5_ADDRESSES;
    CurrentState.Boards = MP5_BOARDS;
    CurrentState.Image = "box-mp5";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP5_GAMESTATES;
    CurrentState.Title = "Mario Party 5";
    break;
  case MPN_GAMEID_MP6:
    CurrentState.Addresses = &MP6_ADDRESSES;
    CurrentState.Boards = MP6_BOARDS;
    CurrentState.Image = "box-mp6";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP6_GAMESTATES;
    CurrentState.Title = "Mario Party 6";
    break;
  case MPN_GAMEID_MP7:
    CurrentState.Addresses    = &MP7_ADDRESSES;
    CurrentState.Boards       = MP7_BOARDS;
    CurrentState.Image = "box-mp7";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP7_GAMESTATES;
    CurrentState.Title = "Mario Party 7";
    break;
  case MPN_GAMEID_MP8:
    CurrentState.Addresses = &MP8_ADDRESSES;
    CurrentState.Boards = MP8_BOARDS;
    CurrentState.Image = "box-mp8";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP8_GAMESTATES;
    CurrentState.Title = "Mario Party 8";
    break;
  case MPN_GAMEID_MP9: /* TODO */
  default:
    CurrentState.Addresses = NULL;
    CurrentState.Boards = NULL;
    CurrentState.Image = "box-mp9";
    CurrentState.IsMarioParty = false;
    CurrentState.Scenes = NULL;
  }

  return CurrentState.Scenes != NULL;
}

bool mpn_update_board()
{
  uint8_t i;

  if (CurrentState.Boards == NULL)
    CurrentState.Board = NULL;
  else if (CurrentState.CurrentSceneId != CurrentState.PreviousSceneId)
  {
    for (i = 0;; i++)
    {
      /* Unknown scene ID */
      if (CurrentState.Boards[i].SceneId == NONE)
        break;
      if (CurrentState.Boards[i].SceneId == CurrentState.CurrentSceneId)
      {
        CurrentState.Board = &CurrentState.Boards[i];
        return true;
      }
    }
  }

  return false;
}

uint8_t mpn_get_needs(uint16_t StateId, bool IsSceneId)
{
  uint16_t i;

  if (CurrentState.Scenes == NULL)
    return MPN_NEEDS_NOTHING;
  else if (CurrentState.CurrentSceneId != CurrentState.PreviousSceneId)
  {
    for (i = 0;; i++)
    {
      /* Unknown scene ID */
      if (CurrentState.Scenes[i].SceneId == NONE)
        return MPN_NEEDS_NOTHING;

      /* Scene ID found in array */
      if ((IsSceneId && StateId == CurrentState.Scenes[i].SceneId) ||
          (StateId == CurrentState.Scenes[i].MiniGameId))
        return CurrentState.Scenes[i].Needs;
    }
  }

  return MPN_NEEDS_NOTHING;
}

void mpn_push_osd_message(const std::string& Message)
{
#ifdef MPN_USE_OSD
  OSD::AddMessage(Message, OSD::Duration::SHORT, MPN_OSD_COLOR);
#endif
}

bool mpn_update_state()
{
  if (CurrentState.Scenes == NULL && !mpn_init_state())
    return false;
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  if (!memory.IsInitialized())
    return false;

  CurrentState.PreviousSceneId = CurrentState.CurrentSceneId;
  CurrentState.CurrentSceneId = mpn_read_value(CurrentState.Addresses->SceneIdAddress, 2);

  for (uint16_t i = 0;; i++)
  {
    if (CurrentState.Scenes[i].SceneId == NONE)
      break;
    if (CurrentState.CurrentSceneId == CurrentState.Scenes[i].SceneId)
    {
      CurrentState.Scene = &CurrentState.Scenes[i];
      return true;
    }
  }

  return false;
}

#define OSD_PUSH(a) mpn_push_osd_message("Adjusting #a for " + CurrentState.Scene->Name);
void mpn_per_frame()
{
  uint8_t Needs = 0;

  if (!mpn_update_state() || CurrentState.PreviousSceneId == CurrentState.CurrentSceneId)
    return;

  mpn_update_board();
  mpn_update_discord();

  Needs = mpn_get_needs(mpn_read_value(CurrentState.Addresses->SceneIdAddress, 2), true);

  if (Needs != MPN_NEEDS_NOTHING)
  {
    if (Needs & MPN_NEEDS_SAFE_TEX_CACHE)
    {
      OSD_PUSH(GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES)
      Config::SetCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 0);
    }
    else
      Config::SetCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 128);

    if (Needs & MPN_NEEDS_NATIVE_RES)
    {
      OSD_PUSH(GFX_EFB_SCALE)
      Config::SetCurrent(Config::GFX_EFB_SCALE, 1);
    }
    else
      Config::SetCurrent(Config::GFX_EFB_SCALE, Config::GetBase(Config::GFX_EFB_SCALE));

    if (Needs & MPN_NEEDS_EFB_TO_TEXTURE)
    {
      OSD_PUSH(GFX_HACK_SKIP_EFB_COPY_TO_RAM)
      Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, false);
    }
    else
      Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, true);

    UpdateActiveConfig();
  }
}

uint32_t mpn_read_value(uint32_t Address, uint8_t Size)
{
  uint32_t Value = 0;
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  for (int8_t i = 0; i < Size; i++)
    Value += memory.GetRAM()[Address + i] * pow(256, Size - i - 1);

  return Value;
}
