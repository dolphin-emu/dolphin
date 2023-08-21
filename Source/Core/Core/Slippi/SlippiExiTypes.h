#pragma once

#include <Common/CommonTypes.h>
#include <Common/Swap.h>

#define REPORT_PLAYER_COUNT 4

namespace SlippiExiTypes
{
// Using pragma pack here will remove any structure padding which is what EXI comms expect
// https://www.geeksforgeeks.org/how-to-avoid-structure-padding-in-c/
// Note that none of these classes should be used outside of the handler function, pragma pack
// is supposedly not very efficient?
#pragma pack(1)

struct ReportGameQueryPlayer
{
  u8 slot_type;
  u8 stocks_remaining;
  float damage_done;
  u8 synced_stocks_remaining;
  u16 synced_current_health;
};

struct ReportGameQuery
{
  u8 command;
  u8 mode;
  u32 frame_length;
  u32 game_idx;
  u32 tiebreak_idx;
  s8 winner_idx;
  u8 game_end_method;
  s8 lras_initiator;
  u32 synced_timer;
  ReportGameQueryPlayer players[REPORT_PLAYER_COUNT];
  u8 game_info_block[312];
};

struct ReportSetCompletionQuery
{
  u8 command;
  u8 end_mode;
};

struct GpCompleteStepQuery
{
  u8 command;
  u8 step_idx;
  u8 char_selection;
  u8 char_color_selection;
  u8 stage_selections[2];
};

struct GpFetchStepQuery
{
  u8 command;
  u8 step_idx;
};

struct GpFetchStepResponse
{
  u8 is_found;
  u8 is_skip;
  u8 char_selection;
  u8 char_color_selection;
  u8 stage_selections[2];
};

struct OverwriteCharSelections
{
  u8 is_set;
  u8 char_id;
  u8 char_color_id;
};
struct OverwriteSelectionsQuery
{
  u8 command;
  u16 stage_id;
  OverwriteCharSelections chars[4];
};

struct PlayerSettings
{
  char chat_messages[16][51];
};
struct GetPlayerSettingsResponse
{
  PlayerSettings settings[4];
};

struct PlayMusicQuery
{
  u8 command;
  u32 offset;
  u32 size;
};

struct ChangeMusicVolumeQuery
{
  u8 command;
  u8 volume;
};

// Not sure if resetting is strictly needed, might be contained to the file
#pragma pack()

template <typename T>
inline T Convert(u8* payload)
{
  return *reinterpret_cast<T*>(payload);
}

// Here we define custom convert functions for any type that larger than u8 sized fields to convert
// from big-endian

template <>
inline ReportGameQuery Convert(u8* payload)
{
  auto q = *reinterpret_cast<ReportGameQuery*>(payload);
  q.frame_length = Common::FromBigEndian(q.frame_length);
  q.game_idx = Common::FromBigEndian(q.game_idx);
  q.tiebreak_idx = Common::FromBigEndian(q.tiebreak_idx);
  q.synced_timer = Common::FromBigEndian(q.synced_timer);
  for (int i = 0; i < REPORT_PLAYER_COUNT; i++)
  {
    auto* p = &q.players[i];
    p->damage_done = Common::FromBigEndian(p->damage_done);
    p->synced_current_health = Common::FromBigEndian(p->synced_current_health);
  }
  return q;
}

template <>
inline OverwriteSelectionsQuery Convert(u8* payload)
{
  auto q = *reinterpret_cast<OverwriteSelectionsQuery*>(payload);
  q.stage_id = Common::FromBigEndian(q.stage_id);
  return q;
}

template <>
inline PlayMusicQuery Convert(u8* payload)
{
  auto q = *reinterpret_cast<PlayMusicQuery*>(payload);
  q.offset = Common::FromBigEndian(q.offset);
  q.size = Common::FromBigEndian(q.size);
  return q;
}
};  // namespace SlippiExiTypes
