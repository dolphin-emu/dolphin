/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#ifndef MPN_GAMESTATE_H
#define MPN_GAMESTATE_H

#include <stdint.h>
#include <string>
#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/InputConfig.h"
#include "VideoCommon/VideoConfig.h"

#define MPN_GAMEID_MP4 0x474D5045
#define MPN_GAMEID_MP5 0x47503545
#define MPN_GAMEID_MP6 0x47503645
#define MPN_GAMEID_MP7 0x47503745
#define MPN_GAMEID_MP8 0x524D3845
#define MPN_GAMEID_MP9 0x53535145

#define MPN_NEEDS_SAFE_TEX_CACHE (1 << 0)
#define MPN_NEEDS_NATIVE_RES (1 << 1)
#define MPN_NEEDS_EFB_TO_TEXTURE (1 << 2)
#define MPN_NEEDS_SIDEWAYS_WIIMOTE (1 << 3)
#define MPN_NEEDS_NOTHING 0xFF
#define NONE -1

#undef MPN_USE_LEADERBOARDS
#define MPN_USE_OSD

#include "Discord.h"

#ifdef MPN_USE_LEADERBOARDS
#include "Core/MarioPartyNetplay/Leaderboards.h"
#endif

#ifdef MPN_USE_OSD
#include "VideoCommon/OnScreenDisplay.h"
#define MPN_OSD_COLOR 0xFF4A4A94
#endif

typedef struct mpn_scene_t
{
  int16_t MiniGameId;
  int16_t SceneId;
  std::string Name;
  uint8_t Needs;
} mpn_scene_t;

typedef struct mpn_addresses_t
{
  /*
  uint32_t BlueSpaces;
  uint32_t RedSpaces;
  uint32_t HappeningSpaces;
  uint32_t ChanceSpaces;
  uint32_t BowserSpaces;
  uint32_t BattleSpaces;
  uint32_t ItemSpaces;
  uint32_t SpringSpaces;
  */
  uint32_t CurrentTurn;
  uint32_t TotalTurns;
  uint32_t MinigameIdAddress;
  uint32_t SceneIdAddress;
  uint32_t ControllerPortAddress1;
  uint32_t ControllerPortAddress2;
  uint32_t ControllerPortAddress3;
  uint32_t ControllerPortAddress4;
} mpn_addresses_t;

typedef struct mpn_player_t
{
  uint8_t BlueSpaces;
  uint16_t Coins;
  uint16_t CoinStar;
  uint16_t GameStar;
  uint8_t Stars;
} mpn_player_t;

typedef struct mpn_board_t
{
  int8_t BoardId;
  int16_t SceneId;
  std::string Name;
  std::string Icon;
} mpn_board_t;

typedef struct mpn_state_t
{
  bool IsMarioParty;

  int16_t CurrentSceneId;
  int16_t PreviousSceneId;

  const char* Title;
  const char* Image;

  const mpn_addresses_t* Addresses;
  const mpn_board_t* Board;
  const mpn_board_t* Boards;
  const mpn_scene_t* Scene;
  const mpn_scene_t* Scenes;
} mpn_state_t;

extern mpn_state_t CurrentState;

/* Function prototypes */
uint8_t mpn_get_needs(uint16_t StateId, bool IsSceneId = false);
void mpn_per_frame();
uint32_t mpn_read_value(uint32_t Address, uint8_t Size);
bool mpn_update_state();

/* ============================================================================
   Mario Party 4 metadata
============================================================================ */
const mpn_addresses_t MP4_ADDRESSES = {
    0x0018FCFC,  // Current Turns
    0x0018FCFD,  // Total Turns
    0x0018FD2C,  // Mini ID
    0x001D3CE2,  // Scene ID
    0x0018FC19,  // Controller Port A
    0x0018FC23,  // Controller Port B
    0x0018FC2D,  // Controller Port C
    0x0018FC37   // Controller Port D
};

const mpn_board_t MP4_BOARDS[] = {{1, 0x59, {"Toad's Midway Madness"}, {"mp4-toad"}},
                                  {2, 0x5A, {"Goomba's Greedy Gala"}, {"mp4-goomba"}},
                                  {3, 0x5B, {"Shy Guy's Jungle Jam"}, {"mp4-shyguy"}},
                                  {4, 0x5C, {"Boo's Haunted Bash"}, {"mp4-boo"}},
                                  {5, 0x5D, {"Koopa's Seaside Soiree"}, {"mp4-koopa"}},
                                  {6, 0x5E, {"Bowser's Gnarly Party"}, {"mp4-bowser"}},
                                  {7, 0x60, {"Mega Board Mayhem"}, {"mp4-mega"}},
                                  {8, 0x61, {"Mini Board Mad-Dash"}, {"mp4-mini"}},

                                  {NONE, NONE, {""}, ""}};

const mpn_scene_t MP4_GAMESTATES[] = {{NONE, 0x01, {"Title Screen"}, 0},
                                      {0x00, 0x09, {"Manta Rings"}, 0},
                                      {0x01, 0x0A, {"Slime Time"}, 0},
                                      {0x02, 0x0B, {"Booksquirm"}, 0},
                                      {0x03, 0x0C, {"Trace Race"}, MPN_NEEDS_SAFE_TEX_CACHE},
                                      {0x04, 0x0D, {"Mario Medley"}, 0},
                                      {0x05, 0x0E, {"Avalanche!"}, 0},
                                      {0x06, 0x0F, {"Domination"}, 0},
                                      {0x07, 0x10, {"Paratrooper Plunge"}, 0},
                                      {0x08, 0x11, {"Toad's Quick Draw"}, 0},
                                      {0x09, 0x12, {"Three Throw"}, 0},
                                      {0x0A, 0x13, {"Photo Finish"}, 0},
                                      {0x0B, 0x14, {"Mr. Blizzard's Brigade"}, 0},
                                      {0x0C, 0x15, {"Bob-omb Breakers"}, 0},
                                      {0x0D, 0x16, {"Long Claw of the Law"}, 0},
                                      {0x0E, 0x17, {"Stamp Out!"}, 0},
                                      {0x0F, 0x18, {"Candlelight Fright"}, 0},
                                      {0x10, 0x19, {"Makin' Waves"}, 0},
                                      {0x11, 0x1A, {"Hide and Go BOOM!"}, 0},
                                      {0x12, 0x1B, {"Tree Stomp"}, 0},
                                      {0x13, 0x1C, {"Fish n' Drips"}, 0},
                                      {0x14, 0x1D, {"Hop or Pop"}, 0},
                                      {0x15, 0x1E, {"Money Belts"}, 0},
                                      {0x16, 0x1F, {"GOOOOOOOAL!!"}, 0},
                                      {0x17, 0x20, {"Blame it on the Crane"}, 0},
                                      {0x18, 0x21, {"The Great Deflate"}, 0},
                                      {0x19, 0x22, {"Revers-a-Bomb"}, 0},
                                      {0x1A, 0x23, {"Right Oar Left?"}, 0},
                                      {0x1B, 0x24, {"Cliffhangers"}, 0},
                                      {0x1C, 0x25, {"Team Treasure Trek"}, 0},
                                      {0x1D, 0x26, {"Pair-a-sailing"}, 0},
                                      {0x1E, 0x27, {"Order Up"}, 0},
                                      {0x1F, 0x28, {"Dungeon Duos"}, 0},
                                      {0x20, 0x29, {"Beach Volley Folley"}, 0},
                                      {0x21, 0x2A, {"Cheep Cheep Sweep"}, 0},
                                      {0x22, 0x2B, {"Darts of Doom"}, 0},
                                      {0x23, 0x2C, {"Fruits of Doom"}, 0},
                                      {0x24, 0x2D, {"Balloon of Doom"}, 0},
                                      {0x25, 0x2E, {"Chain Chomp Fever"}, 0},
                                      {0x26, 0x2F, {"Paths of Peril"}, MPN_NEEDS_EFB_TO_TEXTURE},
                                      {0x27, 0x30, {"Bowser's Bigger Blast"}, 0},
                                      {0x28, 0x31, {"Butterfly Blitz"}, 0},
                                      {0x29, 0x32, {"Barrel Baron"}, 0},
                                      {0x2A, 0x33, {"Mario Speedwagons"}, 0},
                                      /* 2B? */
                                      {0x2C, 0x35, {"Bowser Bop"}, 0},
                                      {0x2D, 0x36, {"Mystic Match 'Em"}, 0},
                                      {0x2E, 0x37, {"Archaeologuess"}, 0},
                                      {0x2F, 0x38, {"Goomba's Chip Flip"}, 0},
                                      {0x30, 0x39, {"Kareening Koopas"}, 0},
                                      {0x31, 0x3A, {"The Final Battle!"}, 0},
                                      {NONE, 0x3B, {"Jigsaw Jitters"}, 0},
                                      {NONE, 0x3C, {"Challenge Booksquirm"}, 0},
                                      {NONE, 0x3D, {"Rumble Fishing"}, 0},
                                      {NONE, 0x3E, {"Take a Breather"}, 0},
                                      {NONE, 0x3F, {"Bowser Wrestling"}, 0},
                                      {NONE, 0x41, {"Mushroom Medic"}, 0},
                                      {NONE, 0x42, {"Doors of Doom"}, 0},
                                      {NONE, 0x43, {"Bob-omb X-ing"}, 0},
                                      {NONE, 0x44, {"Goomba Stomp"}, 0},
                                      {NONE, 0x45, {"Panel Panic"}, 0},
                                      {NONE, 0x46, {"Party Mode Menu"}, 0},
                                      {NONE, 0x48, {"Mini-Game Mode Menu"}, 0},
                                      {NONE, 0x4A, {"Main Menu"}, 0},
                                      {NONE, 0x4B, {"Extra Room"}, 0},
                                      {NONE, 0x52, {"Option Room"}, 0},
                                      {NONE, 0x53, {"Present Room"}, 0},
                                      {NONE, 0x59, {"Toad's Midway Madness"}, 0},
                                      {NONE, 0x5A, {"Goomba's Greedy Gala"}, 0},
                                      {NONE, 0x5B, {"Shy Guy's Jungle Jam"}, 0},
                                      {NONE, 0x5C, {"Boo's Haunted Bash"}, 0},
                                      {NONE, 0x5D, {"Koopa's Seaside Soiree"}, 0},
                                      {NONE, 0x5E, {"Bowser's Gnarly Party"}, 0},
                                      {NONE, 0x5F, {"Board Map Rules"}, 0},
                                      {NONE, 0x60, {"Mega Board Mayhem"}, 0},
                                      {NONE, 0x61, {"Mini Board Mad-Dash"}, 0},
                                      {NONE, 0x62, {"Beach Volley Folley Menu"}, 0},

                                      {NONE, NONE, {""}}};

/* ============================================================================
   Mario Party 5 metadata
============================================================================ */

const mpn_addresses_t MP5_ADDRESSES = {
    0x0022A494,  // Current Turns
    0x0022A495,  // Total Turns
    0x0022A4C4,  // Mini ID
    0x00288862,  // Scene ID
    0x0022A05B,  // Controller Port A
    0x0022A051,  // Controller Port B
    0x0022A065,  // Controller Port C
    0x0022A06F   // Controller Port D
};

const mpn_board_t MP5_BOARDS[] = {{1, 0x76, {"Toy Dream"}, {"mp5-toy"}},
                                  {2, 0x78, {"Rainbow Dream"}, {"mp5-rainbow"}},
                                  {3, 0x7A, {"Pirate Dream"}, {"mp5-pirate"}},
                                  {4, 0x7C, {"Undersea Dream"}, {"mp5-undersea"}},
                                  {5, 0x7E, {"Future Dream"}, {"mp5-future"}},
                                  {6, 0x80, {"Sweet Dream"}, {"mp5-sweet"}},
                                  {7, 0x82, {"Bowser Nightmare"}, {"mp5-bowser"}},


                                  {NONE, NONE, {""}, ""}};

const mpn_scene_t MP5_GAMESTATES[] = {{NONE, 0x01, {"Title Screen"}, 0},
                                      {NONE, 0x02, {"Card Party"}, 0},
                                      {NONE, 0x06, {"Save-File Screen"}, 0},
                                      //{NONE, 0x07, {"Mini-game Explanation"}, 0},
                                      {0x4D, 0x0B, {"Beach Volleyball"}, 0},
                                      {0x00, 0x0F, {"Coney Island"}, 0},
                                      {0x01, 0x10, {"Ground Pound Down"}, 0},
                                      {0x02, 0x11, {"Chimp Chase"}, 0},
                                      {0x03, 0x12, {"Chomp Romp"}, 0},
                                      {0x04, 0x13, {"Pushy Penguins"}, MPN_NEEDS_EFB_TO_TEXTURE},
                                      {0x05, 0x14, {"Leaf Leap"}, 0},
                                      {0x06, 0x15, {"Night Light Fright"}, 0},
                                      {0x07, 0x16, {"Pop-Star Piranhas"}, 0},
                                      {0x08, 0x17, {"Mazed & Confused"}, 0},
                                      {0x09, 0x18, {"Dinger Derby"}, 0},
                                      {0x0A, 0x19, {"Hydrostars"}, 0},
                                      {0x0B, 0x1A, {"Later Skater"}, 0},
                                      {0x0C, 0x1B, {"Will Flower"}, 0},
                                      {0x0D, 0x1C, {"Triple Jump"}, 0},
                                      {0x0E, 0x1D, {"Hotel Goomba"}, 0},
                                      {0x0F, 0x1E, {"Coin Cache"}, 0},
                                      {0x10, 0x1F, {"Flatiator"}, 0},
                                      {0x11, 0x20, {"Squared Away"}, 0},
                                      {0x12, 0x21, {"Mario Mechs"}, 0},
                                      {0x13, 0x22, {"Revolving Fire"}, 0},
                                      {0x14, 0x23, {"Clock Stoppers"}, 0},
                                      {0x15, 0x24, {"Heat Stroke"}, 0},
                                      {0x16, 0x25, {"Beam Team"}, 0},
                                      {0x17, 0x26, {"Vicious Vending"}, 0},
                                      {0x18, 0x27, {"Big Top Drop"}, 0},
                                      {0x19, 0x28, {"Defuse or Lose"}, 0},
                                      {0x1A, 0x29, {"ID UFO"}, 0},
                                      {0x1B, 0x2A, {"Mario Can-Can"}, 0},
                                      {0x1C, 0x2B, {"Handy Hoppers"}, 0},
                                      {0x1D, 0x2C, {"Berry Basket"}, 0},
                                      {0x1E, 0x2D, {"Bus Buffer"}, 0},
                                      {0x1F, 0x2E, {"Rumble Ready"}, 0},
                                      {0x20, 0x2F, {"Submarathon"}, 0},
                                      {0x21, 0x30, {"Manic Mallets"}, 0},
                                      {0x22, 0x31, {"Astro-Logical"}, 0},
                                      {0x23, 0x32, {"Bill Blasters"}, 0},
                                      {0x24, 0x33, {"Tug-o-Dorrie"}, 0},
                                      {0x25, 0x34, {"Twist 'n' Out"}, 0},
                                      {0x26, 0x35, {"Lucky Lineup"}, 0},
                                      {0x27, 0x36, {"Random Ride"}, 0},
                                      {0x28, 0x37, {"Shock Absorbers"}, 0},
                                      {0x29, 0x38, {"Countdown Pound"}, 0},
                                      {0x2A, 0x39, {"Whomp Maze"}, 0},
                                      {0x2B, 0x3A, {"Shy Guy Showdown"}, 0},
                                      {0x2C, 0x3B, {"Button Mashers"}, 0},
                                      {0x2D, 0x3C, {"Get a Rope"}, 0},
                                      {0x2E, 0x3D, {"Pump 'n' Jump"}, 0},
                                      {0x2F, 0x3E, {"Head Waiter"}, 0},
                                      {0x30, 0x3F, {"Blown Away"}, 0},
                                      {0x31, 0x40, {"Merry Poppings"}, 0},
                                      {0x32, 0x41, {"Pound Peril"}, 0},
                                      {0x33, 0x42, {"Piece Out"}, 0},
                                      {0x34, 0x43, {"Bound of Music"}, 0},
                                      {0x35, 0x44, {"Wind Wavers"}, 0},
                                      {0x36, 0x45, {"Sky Survivor"}, 0},
                                      {0x3A, 0x46, {"Rain of Fire"}, 0},
                                      {0x3B, 0x47, {"Cage-in Cookin'"}, 0},
                                      {0x3C, 0x48, {"Scaldin' Cauldron"}, 0},
                                      {0x3D, 0x49, {"Frightmare"}, 0},
                                      {0x3E, 0x4A, {"Flower Shower"}, 0},
                                      {0x3F, 0x4B, {"Dodge Bomb"}, 0},
                                      {0x40, 0x4C, {"Fish Upon a Star"}, 0},
                                      {0x41, 0x4D, {"Rumble Fumble"}, 0},
                                      {0x42, 0x4E, {"Quilt for Speed"}, 0},
                                      {0x43, 0x4F, {"Tube It or Lose It"}, 0},
                                      {0x44, 0x50, {"Mathletes"}, 0},
                                      {0x45, 0x51, {"Fight Cards"}, 0},
                                      {0x46, 0x52, {"Banana Punch"}, 0},
                                      {0x47, 0x53, {"Da Vine Climb"}, 0},
                                      {0x48, 0x54, {"Mass A-peel"}, 0},
                                      {0x49, 0x55, {"Panic Pinball"}, 0},
                                      {0x4A, 0x56, {"Banking Coins"}, 0},
                                      {0x4B, 0x57, {"Frozen Frenzy"}, 0},
                                      {0x4C, 0x58, {"Curvy Curbs"}, 0},
                                      {0x4E, 0x59, {"Fish Sticks"}, 0},
                                      {0x4F, 0x5A, {"Ice Hockey"}, 0},
                                      {NONE, 0x5C, {"Card Party Menu"}, 0},
                                      {NONE, 0x5E, {"Bonus Mode Menu"}, 0},
                                      {NONE, 0x60, {"Party Mode Menu"}, 0},
                                      {NONE, 0x61, {"Main Menu"}, 0},
                                      {NONE, 0x62, {"Party Mode Menu"}, 0},
                                      {NONE, 0x64, {"Free Play"}, 0},
                                      {NONE, 0x6F, {"Super Duel Mode Menu"}, 0},
                                      {NONE, 0x76, {"Toy Dream"}, 0},
                                      {NONE, 0x78, {"Rainbow Dream"}, 0},
                                      {NONE, 0x7A, {"Pirate Dream"}, 0},
                                      {NONE, 0x7C, {"Undersea Dream"}, 0},
                                      {NONE, 0x7E, {"Future Dream"}, 0},
                                      {NONE, 0x80, {"Sweet Dream"}, 0},
                                      {NONE, 0x82, {"Bowser Nightmare"}, 0},

                                      {NONE, NONE, {""}}};

/* ============================================================================
   Mario Party 6 metadata
============================================================================ */

const mpn_addresses_t MP6_ADDRESSES = {
    0x00265B74,  // Current Turns
    0x00265B75,  // Total Turns
    0x00265BA8,  // Mini ID
    0x002C0256,  // Scene ID
    0x00265745,  // Controller Port A
    0x00265731,  // Controller Port B
    0x0026574F,  // Controller Port C
    0x0026573B}; // Controller Port D

const mpn_board_t MP6_BOARDS[] = {{1, 0x7B, {"Towering Treetop"}, {"mp6-treetop"}},
                                  {2, 0x7C, {"E. Gadd's Garage"}, {"mp6-garage"}},
                                  {3, 0x7D, {"Faire Square"}, {"mp6-square"}},
                                  {4, 0x7E, {"Snowflake Lake"}, {"mp6-lake"}},
                                  {5, 0x7F, {"Castaway Bay"}, {"mp6-bay"}},
                                  {6, 0x80, {"Clockwork Castle"}, {"mp6-castle"}},
                                  {7, 0x72, {"Thirsty Gultch"}, {"mp6-gultch"}},
                                  {8, 0x73, {"Astro Avenue"}, {"mp6-avenue"}},
                                  {9, 0x74, {"Infernal Tower"}, {"mp6-tower"}},

                                  {NONE, NONE, {""}, ""}};

const mpn_scene_t MP6_GAMESTATES[] = {{NONE, 0x01, {"Title Screen"}, 0},
                                      {NONE, 0x03, {"File-selection Screen"}, 0},
                                      //{NONE, 0x04, {"Mini-game Explanation"}, 0},
                                      {0x00, 0x06, {"Smashdance"}, 0},
                                      {0x01, 0x07, {"Odd Card Out"}, 0},
                                      {0x02, 0x08, {"Freeze Frame"}, 0},
                                      {0x03, 0x09, {"What Goes Up..."}, 0},
                                      {0x04, 0x0A, {"Granite Getaway"}, 0},
                                      {0x05, 0x0B, {"Circuit Maximus"}, 0},
                                      {0x06, 0x0C, {"Catch You Letter"}, 0},
                                      {0x07, 0x0D, {"Snow Whirled"}, 0},
                                      {0x08, 0x0E, {"Daft Rafts"}, 0},
                                      {0x09, 0x0F, {"Tricky Tires"}, 0},
                                      {0x0A, 0x10, {"Treasure Trawlers"}, 0},
                                      {0x0B, 0x11, {"Memory Lane"}, 0},
                                      {0x0C, 0x12, {"Mowtown"}, MPN_NEEDS_SAFE_TEX_CACHE},
                                      {0x0D, 0x13, {"Cannonball Fun"}, 0},
                                      {0x0E, 0x14, {"Note to Self"}, 0},
                                      {0x0F, 0x15, {"Same is Lame"}, 0},
                                      {0x10, 0x16, {"Light Up My Night"}, 0},
                                      {0x11, 0x17, {"Lift Leapers"}, 0},
                                      {0x12, 0x18, {"Blooper Scooper"}, 0},
                                      {0x13, 0x19, {"Trap Ease Artist"}, 0},
                                      {0x14, 0x1A, {"Pokey Punch-out"}, 0},
                                      {0x15, 0x1B, {"Money Belt"}, 0},
                                      {0x16, 0x1C, {"Cash Flow"}, 0},
                                      {0x17, 0x1D, {"Cog Jog"}, 0},
                                      {0x18, 0x1E, {"Sink or Swim"}, 0},
                                      {0x19, 0x1F, {"Snow Brawl"}, 0},
                                      {0x1A, 0x20, {"Ball Dozers"}, 0},
                                      {0x1B, 0x21, {"Surge and Destroy"}, 0},
                                      {0x1C, 0x22, {"Pop Star"}, 0},
                                      {0x1D, 0x23, {"Stage Fright"}, 0},
                                      {0x1E, 0x24, {"Conveyor Bolt"}, 0},
                                      {0x1F, 0x25, {"Crate and Peril"}, 0},
                                      {0x20, 0x26, {"Ray of Fright"}, 0},
                                      {0x21, 0x27, {"Dust 'til Dawn"}, 0},
                                      {0x22, 0x28, {"Garden Grab"}, 0},
                                      {0x23, 0x29, {"Pixel Perfect"}, 0},
                                      {0x24, 0x2A, {"Slot Trot"}, 0},
                                      {0x25, 0x2B, {"Gondola Glide"}, 0},
                                      {0x26, 0x2C, {"Light Breeze"}, 0},
                                      {0x27, 0x2D, {"Body Builder"}, 0},
                                      {0x28, 0x2E, {"Mole-it!"}, 0},
                                      {0x29, 0x2F, {"Cashapult"}, 0},
                                      {0x2A, 0x30, {"Jump the Gun"}, 0},
                                      {0x2B, 0x31, {"Rocky Road"}, 0},
                                      {0x2C, 0x32, {"Clean Team"}, 0},
                                      {0x2D, 0x33, {"Hyper Sniper"}, 0},
                                      {0x2E, 0x34, {"Insectiride"}, 0},
                                      {0x2F, 0x35, {"Sunday Drivers"}, 0},
                                      {0x30, 0x36, {"Stamp By Me"}, 0},
                                      {0x31, 0x37, {"Throw Me a Bone"}, 0},
                                      {0x32, 0x38, {"Black Hole Boogie"}, 0},
                                      {0x33, 0x39, {"Full Tilt"}, 0},
                                      {0x34, 0x3A, {"Sumo of Doom-o"}, 0},
                                      {0x35, 0x3B, {"O-Zone"}, 0},
                                      {0x36, 0x3C, {"Pitifall"}, 0},
                                      {0x37, 0x3D, {"Mass Meteor"}, 0},
                                      {0x38, 0x3E, {"Lunar-tics"}, 0},
                                      {0x39, 0x3F, {"T Minus Five"}, 0},
                                      {0x3A, 0x40, {"Asteroad Rage"}, 0},
                                      {0x3B, 0x41, {"Boo'd Off the Stage"}, 0},
                                      {0x3C, 0x42, {"Boonanza!"}, 0},
                                      {0x3D, 0x43, {"Trick or Tree"}, 0},
                                      {0x3E, 0x44, {"Something's Amist"}, 0},
                                      {0x3F, 0x45, {"Wrasslin' Rapids"}, 0},
                                      {0x43, 0x46, {"Burnstile"}, 0},
                                      {0x44, 0x47, {"Word Herd"}, 0},
                                      {0x45, 0x48, {"Fruit Talktail"}, 0},
                                      {0x46, 0x4C, {"Pit Boss"}, 0},
                                      {0x47, 0x4D, {"Dizzy Rotisserie"}, 0},
                                      {0x48, 0x4E, {"Dark 'n Crispy"}, 0},
                                      {0x49, 0x4F, {"Tally Me Banana"}, 0},
                                      {0x4A, 0x50, {"Banana Shake"}, 0},
                                      {0x4B, 0x51, {"Pier Factor"}, 0},
                                      {0x4C, 0x52, {"Seer Terror"}, 0},
                                      {0x4D, 0x53, {"Block Star"}, 0},
                                      {0x4E, 0x54, {"Lab Brats"}, 0},
                                      {0x4F, 0x55, {"Strawberry Shortfuse"}, 0},
                                      {0x50, 0x56, {"Control Shtick"}, 0},
                                      {0x51, 0x57, {"Dunk Bros."}, 0},
                                      {NONE, 0x58, {"Star Bank"}, 0},
                                      {NONE, 0x5A, {"Mini-game Mode"}, 0},
                                      {NONE, 0x5B, {"Party Mode Menu"}, 0},
                                      {NONE, 0x5C, {"Final Results"}, 0},
                                      {NONE, 0x5D, {"Main Menu"}, 0},
                                      {NONE, 0x5E, {"Solo Mode Menu"}, 0},
                                      {NONE, 0x60, {"Battle Bridge"}, 0},
                                      {NONE, 0x61, {"Treetop Bingo"}, 0},
                                      {NONE, 0x62, {"Mount Duel"}, 0},
                                      {NONE, 0x63, {"Mini-game Tour"}, MPN_NEEDS_EFB_TO_TEXTURE},
                                      {NONE, 0x63, {"Decathlon Park"}, 0},
                                      {NONE, 0x64, {"Endurance Alley"}, 0},
                                      {NONE, 0x6F, {"Option Mode"}, 0},
                                      {NONE, 0x71, {"Mini-game Results"}, 0},
                                      {NONE, 0x72, {"Thirsty Gultch"}, 0},
                                      {NONE, 0x73, {"Astro Avenue"}, 0},
                                      {NONE, 0x74, {"Infernal Tower"}, 0},
                                      {NONE, 0x7B, {"Towering Treetop"}, 0},
                                      {NONE, 0x7C, {"E. Gadd's Garage"}, 0},
                                      {NONE, 0x7D, {"Faire Square"}, 0},
                                      {NONE, 0x7E, {"Snowflake Lake"}, 0},
                                      {NONE, 0x7F, {"Castaway Bay"}, 0},
                                      {NONE, 0x80, {"Clockwork Castle"}, 0},

                                      {NONE, NONE, {""}}};

/* ============================================================================
   Mario Party 7 metadata
============================================================================ */

const mpn_addresses_t MP7_ADDRESSES = {
  0x0029151C,   // Current Turns
  0x0029151D,   // Total Turns
  0x00291558,   // Mini ID
  0x002F2F3E,   // Scene ID
  0x00290C51,   // Controller Port A
  0x00290C5B,   // Controller Port B
  0x00290C65,   // Controller Port C
  0x00290C6F    // Controller Port D
  //0x00290C79, // Controller Port E
  //0x00290C83, // Controller Port F
  //0x00290C8D, // Controller Port G
  //0x00290C97  // Controller Port H
};  

const mpn_board_t MP7_BOARDS[] = {{1, 0x7A, {"Grand Canal"}, {"mp7-canal"}},
                                  {2, 0x7B, {"Pagoda Peak"}, {"mp7-peak"}},
                                  {3, 0x7C, {"Pyramid Park"}, {"mp7-park"}},
                                  {4, 0x7D, {"Neon Heights"}, {"mp7-heights"}},
                                  {5, 0x7E, {"Windmillville"}, {"mp7-windmillville"}},
                                  {6, 0x7F, {"Bowser's Enchanted Inferno!"}, {"mp7-inferno"}},

                                  {NONE, NONE, "", ""}};

const mpn_scene_t MP7_GAMESTATES[] = {{NONE, 0x03, {"Boot Logos"}, 0},
                                      {NONE, 0x05, {"File Select"}, 0},
                                      //{NONE, 0x06, {"Mini-Game Explanation"}, 0},
                                      {0x00, 0x07, {"Catchy Tunes"}, 0},
                                      {0x01, 0x08, {"Bubble Brawl"}, 0},
                                      {0x02, 0x09, {"Track & Yield"}, 0},
                                      {0x03, 0x0A, {"Fun Run"}, 0},
                                      {0x04, 0x0B, {"Cointagious"}, 0},
                                      {0x05, 0x0C, {"Snow Ride"}, 0},
                                      {0x07, 0x0E, {"Picture This"}, 0},
                                      {0x08, 0x0F, {"Ghost in the Hall"}, 0},
                                      {0x09, 0x10, {"Big Dripper"}, 0},
                                      {0x0A, 0x11, {"Target Tag"}, 0},
                                      {0x0B, 0x12, {"Pokey Pummel"}, 0},
                                      {0x0C, 0x13, {"Take Me Ohm"}, 0},
                                      {0x0D, 0x14, {"Kart Wheeled"}, 0},
                                      {0x0E, 0x15, {"Balloon Busters"}, 0},
                                      {0x0F, 0x16, {"Clock Watchers"}, 0},
                                      {0x10, 0x17, {"Dart Attack"}, 0},
                                      {0x12, 0x18, {"Oil Crisis"}, 0},
                                      {0x14, 0x1A, {"La Bomba"}, 0},
                                      {0x15, 0x1B, {"Spray Anything"}, 0},
                                      {0x16, 0x1C, {"Balloonatic"}, 0},
                                      {0x17, 0x1D, {"Spinner Cell"}, 0},
                                      {0x18, 0x1E, {"Think Tank"}, 0},
                                      {0x19, 0x1F, {"Flashfright"}, 0},
                                      {0x1A, 0x20, {"Coin-op Bop"}, 0},
                                      {0x1B, 0x21, {"Easy Pickings"}, 0},
                                      {0x1C, 0x22, {"Wheel of Woe"}, 0},
                                      {0x1D, 0x23, {"Boxing Day"}, 0},
                                      {0x1E, 0x24, {"Be My Chum!"}, 0},
                                      {0x1F, 0x25, {"StratosFEAR!"}, 0},
                                      {0x20, 0x26, {"Pogo-a-go-go"}, 0},
                                      {0x21, 0x27, {"Buzzstormer"}, 0},
                                      {0x22, 0x28, {"Tile and Error"}, 0},
                                      {0x23, 0x29, {"Battery Ram"}, 0},
                                      {0x24, 0x2A, {"Cardinal Rule"}, 0},
                                      {0x25, 0x2B, {"Ice Moves"}, 0},
                                      {0x26, 0x2C, {"Bumper Crop"}, 0},
                                      {0x27, 0x2D, {"Hop-O-Matic 4000"}, 0},
                                      {0x28, 0x2E, {"Wingin' It"}, 0},
                                      {0x29, 0x2F, {"Sphere Factor"}, 0},
                                      {0x2A, 0x30, {"Herbicidal Maniac"}, 0},
                                      {0x2B, 0x31, {"Pyramid Scheme"}, 0},
                                      {0x2C, 0x32, {"World Piece"}, 0},
                                      {0x2D, 0x33, {"Warp Pipe Dreams"}, 0},
                                      {0x2E, 0x34, {"Weight for It"}, 0},
                                      {0x2F, 0x35, {"Helipopper"}, 0},
                                      {0x30, 0x36, {"Monty's Revenge"}, 0},
                                      {0x31, 0x37, {"Deck Hands"}, 0},
                                      {0x32, 0x38, {"Mad Props"}, 0},
                                      {0x33, 0x39, {"Gimme a Sign"}, 0},
                                      {0x34, 0x3A, {"Bridge Work"}, 0},
                                      {0x35, 0x3B, {"Spin Doctor"}, 0},
                                      {0x36, 0x3C, {"Hip Hop Drop"}, 0},
                                      {0x37, 0x3D, {"Air Farce"}, 0},
                                      {0x38, 0x3E, {"The Final Countdown"}, 0},
                                      {0x39, 0x3F, {"Royal Rumpus"}, 0},
                                      {0x3A, 0x40, {"Light Speed"}, 0},
                                      {0x3B, 0x41, {"Apes of Wrath"}, 0},
                                      {0x3C, 0x42, {"Fish & Cheeps"}, 0},
                                      {0x3D, 0x43, {"Camp Ukiki"}, 0},
                                      {0x3E, 0x44, {"Funstacle Course!"}, 0},
                                      {0x3F, 0x45, {"Funderwall!"}, 0},
                                      {0x40, 0x46, {"Magmagical Journey!"}, 0},
                                      {0x41, 0x47, {"Tunnel of Lava!"}, 0},
                                      {0x42, 0x48, {"Treasure Dome!"}, 0},
                                      {0x43, 0x49, {"Slot-O-Whirl!"}, 0},
                                      {0x44, 0x4A, {"Peel Out"}, 0},
                                      {0x45, 0x4B, {"Bananas Faster"}, 0},
                                      {0x46, 0x4C, {"Stump Change"}, 0},
                                      {0x47, 0x4D, {"Jump, Man"}, 0},
                                      {0x48, 0x4E, {"Vine Country"}, 0},
                                      {0x49, 0x4F, {"A Bridge Too Short"}, 0},
                                      {0x4A, 0x50, {"Spider Stomp"}, 0},
                                      {0x4B, 0x51, {"Stick and Spin"}, 0},
                                      {0x55, 0x5B, {"Bowser's Lovely Lift!"}, 0},
                                      {0x57, 0x5D, {"Mathemortician"}, 0},
                                      {NONE, 0x61, {"Duty-Free Shop"}, 0},
                                      {NONE, 0x63, {"Deluxe Cruise"}, 0},
                                      {NONE, 0x64, {"Minigame Cruise"}, 0},
                                      {NONE, 0x65, {"Control Room"}, 0},
                                      {NONE, 0x67, {"Main Menu"}, 0},
                                      {NONE, 0x68, {"Solo Cruise"}, 0},
                                      {NONE, 0x6A, {"Decathlon Castle"}, 0},
                                      {NONE, 0x6B, {"Free-Play Sub"}, 0},
                                      {NONE, 0x6C, {"Waterfall Battle"}, 0},
                                      {NONE, 0x6E, {"Volcano Peril"}, 0},
                                      {NONE, 0x6F, {"Pearl Hunt"}, 0},
                                      {NONE, 0x70, {"King of the River"}, 0},
                                      {NONE, 0x66, {"Party Cruise"}, 0},
                                      {NONE, 0x7A, {"Grand Canal"}, 0},
                                      {NONE, 0x7B, {"Pagoda Peak"}, 0},
                                      {NONE, 0x7C, {"Pyramid Park"}, 0},
                                      {NONE, 0x7D, {"Neon Heights"}, 0},
                                      {NONE, 0x7E, {"Windmillville"}, 0},
                                      {NONE, 0x7F, {"Bowser's Enchanted Inferno!"}, 0},
                                      {NONE, 0x73, {"Mini-Game Results"}, 0},

                                      {NONE, NONE, {""}, 0}};

/* ============================================================================
   Mario Party 8 metadata
============================================================================ */

const mpn_addresses_t MP8_ADDRESSES = {0x00228764, 0x00228765, 0x002287CC, 0x002CD222};

const mpn_board_t MP8_BOARDS[] = {{1, 0x10, {"DK's Treetop Temple"}, {"mp8-dktt"}},
                                  {2, 0x11, {"Goomba's Booty Boardwalk"}, {"mp8-gbb"}},
                                  {3, 0x12, {"King Boo's Haunted Hideaway"}, {"mp8-kbhh"}},
                                  {4, 0x13, {"Shy Guy's Perplex Express"}, {"mp8-sgpe"}},
                                  {5, 0x14, {"Koopa's Tycoon Town"}, {"mp8-ktt"}},
                                  {6, 0x15, {"Bowser's Warped Orbit"}, {"mp8-bwo"}},

                                  {NONE, NONE, {""}, ""}};

const mpn_scene_t MP8_GAMESTATES[] = {
    {NONE, 0x04, {"Main Menu"}, 0},
    {NONE, 0x08, {"Fun Bazaar"}, 0},
    {NONE, 0x0A, {"Free Play Arcade"}, 0},
    {NONE, 0x0B, {"Crown Showdown"}, 0},
    {NONE, 0x0C, {"Flip-Out Frenzy"}, 0},
    {NONE, 0x0D, {"Tic-Tac Drop"}, 0},
    {NONE, 0x0E, {"Test for the Best"}, 0},
    {NONE, 0x10, {"DK's Treetop Temple"}, 0},
    {NONE, 0x11, {"Goomba's Booty Boardwalk"}, 0},
    {NONE, 0x12, {"King Boo's Haunted Hideaway"}, 0},
    {NONE, 0x13, {"Shy Guy's Perplex Express"}, 0},
    {NONE, 0x14, {"Koopa's Tycoon Town"}, 0},
    {NONE, 0x15, {"Bowser's Warped Orbit"}, 0},
    //{NONE, 0x16, {"Mini-Game Explanation"}, 0},
    {0x00, 0x17, {"Speedy Graffiti"}, MPN_NEEDS_EFB_TO_TEXTURE},
    {0x01, 0x18, {"Swing Kings"}, 0},
    {0x02, 0x19, {"Water Ski Spree"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x03, 0x1A, {"Punch-a-Bunch"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x04, 0x1B, {"Crank to Rank"}, 0},
    {0x05, 0x1C, {"At the Chomp Wash"}, 0},
    {0x06, 0x1D, {"Mosh-Pit Playroom"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x07, 0x1E, {"Mario Matrix"}, 0},
    {0x08, 0x1F, {"??? - Hammer de Pokari"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x09, 0x20, {"Grabby Giridion"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x0A, 0x21, {"Lava or Leave 'Em"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x0B, 0x22, {"Kartastrophe"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x0C, 0x23, {"??? - Ribbon Game"}, 0},
    {0x0D, 0x24, {"Aim of the Game"}, 0},
    {0x0E, 0x25, {"Rudder Madness"}, 0},
    {0x0F, 0x26, {"Gun the Runner"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},  // 1P is sideways
    {0x10, 0x27, {"Grabbin' Gold"}, 0},
    {0x11, 0x28, {"Power Trip"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x12, 0x29, {"Bob-ombs Away"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x13, 0x2A, {"Swervin' Skies"}, 0},
    {0x14, 0x2B, {"Picture Perfect"}, 0},
    {0x15, 0x2C, {"Snow Way Out"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},  // 3P are sideways
    {0x16, 0x2D, {"Thrash 'n' Crash"}, 0},
    {0x17, 0x2E, {"Chump Rope"}, 0},
    {0x18, 0x2F, {"Sick and Twisted"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x19, 0x30, {"Bumper Balloons"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x1A, 0x31, {"Rowed to Victory"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x1B, 0x32, {"Winner or Dinner"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x1C, 0x33, {"Paint Misbehavin'"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x1D, 0x34, {"Sugar Rush"}, 0},
    {0x1E, 0x35, {"King of the Thrill"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x1F, 0x36, {"Shake It Up"}, 0},
    {0x20, 0x37, {"Lean, Mean Ravine"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x21, 0x38, {"Boo-ting Gallery"}, 0},
    {0x22, 0x39, {"Crops 'n' Robbers"}, 0},
    {0x23, 0x3A, {"In the Nick of Time"}, 0},
    {0x24, 0x3B, {"Cut from the Team"}, 0},
    {0x25, 0x3C, {"Snipe for the Picking"}, 0},
    {0x26, 0x3D, {"Saucer Swarm"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x27, 0x3E, {"Glacial Meltdown"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x28, 0x3F, {"Attention Grabber"}, 0},
    {0x29, 0x40, {"Blazing Lassos"}, 0},
    {0x2A, 0x41, {"Wing and a Scare"}, 0},
    {0x2B, 0x42, {"Lob to Rob"}, 0},
    {0x2C, 0x43, {"Pumper Cars"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x2D, 0x44, {"Cosmic Slalom"}, 0},
    {0x2E, 0x45, {"Lava Lobbers"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x2F, 0x46, {"Loco Motives"}, 0},
    {0x30, 0x47, {"Specter Inspector"}, 0},
    {0x31, 0x48, {"Frozen Assets"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x32, 0x49, {"Breakneck Building"}, MPN_NEEDS_NATIVE_RES},
    {0x33, 0x4A, {"Surf's Way Up"}, 0},
    {0x34, 0x4B, {"??? - Bull Riding"}, 0},
    {0x35, 0x4C, {"Balancing Act"}, 0},
    {0x36, 0x4D, {"Ion the Prize"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x37, 0x4E, {"You're the Bob-omb"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x38, 0x4F, {"Scooter Pursuit"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x39, 0x50, {"Cardiators"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x3A, 0x51, {"Rotation Station"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x3B, 0x52, {"Eyebrawl"}, 0},
    {0x3C, 0x53, {"Table Menace"}, 0},
    {0x3D, 0x54, {"Flagging Rights"}, 0},
    {0x3E, 0x55, {"Trial by Tile"}, 0},
    {0x3F, 0x56, {"Star Carnival Bowling"}, 0},
    {0x40, 0x57, {"Puzzle Pillars"}, 0},
    {0x41, 0x58, {"Canyon Cruisers"}, 0},
    {0x42, 0x59, {"??? - CRASH"}, 0},
    {0x43, 0x5A, {"Settle It in Court"}, 0},
    {0x44, 0x5B, {"Moped Mayhem"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},
    {0x45, 0x5C, {"Flip the Chimp"}, 0},
    {0x46, 0x5D, {"Pour to Score"}, 0},
    {0x47, 0x5E, {"Fruit Picker"}, 0},
    {0x48, 0x5F, {"Stampede"}, 0},
    {0x49, 0x60, {"Superstar Showdown"}, 0},
    {0x4A, 0x61, {"Alpine Assault"}, 0},
    {0x4B, 0x62, {"Treacherous Tightrope"}, MPN_NEEDS_SIDEWAYS_WIIMOTE},

    {NONE, NONE, {""}, 0}};

#endif
