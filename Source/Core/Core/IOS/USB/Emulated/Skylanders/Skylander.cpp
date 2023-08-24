// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"

#include <map>
#include <mutex>
#include <vector>

#include "AudioCommon/AudioCommon.h"
#include "Common/Logging/Log.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderCrypto.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
const std::map<const std::pair<const u16, const u16>, SkyData> list_skylanders = {
    {{0, 0x0000}, {"Whirlwind", Game::SpyrosAdv, Element::Air, Type::Skylander}},
    {{0, 0x1801}, {"Whirlwind (S2)", Game::Giants, Element::Air, Type::Skylander}},
    {{0, 0x1C02}, {"Whirlwind (Polar)", Game::Giants, Element::Air, Type::Skylander}},
    {{0, 0x2805}, {"Whirlwind (Horn Blast)", Game::SwapForce, Element::Air, Type::Skylander}},
    {{0, 0x3810}, {"Whirlwind (Eon's Elite)", Game::TrapTeam, Element::Air, Type::Skylander}},
    {{1, 0x0000}, {"Sonic Boom", Game::SpyrosAdv, Element::Air, Type::Skylander}},
    {{1, 0x1801}, {"Sonic Boom (S2)", Game::Giants, Element::Air, Type::Skylander}},
    {{1, 0x1811}, {"Sonic Boom (Glow In The Dark)", Game::Giants, Element::Air, Type::Skylander}},
    {{1, 0x1813}, {"Sonic Boom (Sparkle)", Game::Giants, Element::Air, Type::Skylander}},
    {{2, 0x0000}, {"Warnado", Game::SpyrosAdv, Element::Air, Type::Skylander}},
    {{2, 0x2206}, {"Warnado (Lightcore)", Game::SwapForce, Element::Air, Type::Skylander}},
    {{3, 0x0000}, {"Lightning Rod", Game::SpyrosAdv, Element::Air, Type::Skylander}},
    {{3, 0x1801}, {"Lightning Rod (S2)", Game::Giants, Element::Air, Type::Skylander}},
    {{4, 0x0000}, {"Bash", Game::SpyrosAdv, Element::Earth, Type::Skylander}},
    {{4, 0x1801}, {"Bash (S2)", Game::Giants, Element::Earth, Type::Skylander}},
    {{5, 0x0000}, {"Terrafin", Game::SpyrosAdv, Element::Earth, Type::Skylander}},
    {{5, 0x1801}, {"Terrafin (S2)", Game::Giants, Element::Earth, Type::Skylander}},
    {{5, 0x2805}, {"Terrafin (Knockout)", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{5, 0x3810}, {"Terrafin (Eon's Elite)", Game::TrapTeam, Element::Earth, Type::Skylander}},
    {{6, 0x0000}, {"Dino-Rang", Game::SpyrosAdv, Element::Earth, Type::Skylander}},
    {{6, 0x4810},
     {"Dino-Rang (Eon's Elite)", Game::Superchargers, Element::Earth, Type::Skylander}},
    {{7, 0x0000}, {"Prism Break", Game::SpyrosAdv, Element::Earth, Type::Skylander}},
    {{7, 0x1206}, {"Prism Break (Lightcore)", Game::Giants, Element::Earth, Type::Skylander}},
    {{7, 0x1801}, {"Prism Break (S2)", Game::Giants, Element::Earth, Type::Skylander}},
    {{7, 0x2805}, {"Prism Break (Hyper Beam)", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{8, 0x0000}, {"Sunburn", Game::SpyrosAdv, Element::Fire, Type::Skylander}},
    {{9, 0x0000}, {"Eruptor", Game::SpyrosAdv, Element::Fire, Type::Skylander}},
    {{9, 0x1206}, {"Eruptor (Lightcore)", Game::Giants, Element::Fire, Type::Skylander}},
    {{9, 0x1801}, {"Eruptor (S2)", Game::Giants, Element::Fire, Type::Skylander}},
    {{9, 0x2805}, {"Eruptor (Lava Barf)", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{9, 0x2C02}, {"Eruptor (Volcanic)", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{9, 0x3810}, {"Eruptor (Eon's Elite)", Game::TrapTeam, Element::Fire, Type::Skylander}},
    {{10, 0x0000}, {"Ignitor", Game::SpyrosAdv, Element::Fire, Type::Skylander}},
    {{10, 0x1801}, {"Ignitor (S2)", Game::Giants, Element::Fire, Type::Skylander}},
    {{10, 0x1C03}, {"Ignitor (Legendary)", Game::Giants, Element::Fire, Type::Skylander}},
    {{11, 0x0000}, {"Flameslinger", Game::SpyrosAdv, Element::Fire, Type::Skylander}},
    {{11, 0x1801}, {"Flameslinger (S2)", Game::Giants, Element::Fire, Type::Skylander}},
    {{11, 0x1802}, {"Flameslinger (Golden)", Game::Giants, Element::Fire, Type::Skylander}},
    {{12, 0x0000}, {"Zap", Game::SpyrosAdv, Element::Water, Type::Skylander}},
    {{12, 0x1801}, {"Zap (S2)", Game::Giants, Element::Water, Type::Skylander}},
    {{13, 0x0000}, {"Wham-Shell", Game::SpyrosAdv, Element::Water, Type::Skylander}},
    {{13, 0x2206}, {"Wham-Shell (Lightcore)", Game::SwapForce, Element::Water, Type::Skylander}},
    {{14, 0x0000}, {"Gill Grunt", Game::SpyrosAdv, Element::Water, Type::Skylander}},
    {{14, 0x1801}, {"Gill Grunt (S2)", Game::Giants, Element::Water, Type::Skylander}},
    {{14, 0x1817}, {"Gill Grunt (Metallic)", Game::Giants, Element::Water, Type::Skylander}},
    {{14, 0x2805}, {"Gill Grunt (Anchors Away)", Game::SwapForce, Element::Water, Type::Skylander}},
    {{14, 0x3809}, {"Gill Grunt (Tidal Wave)", Game::TrapTeam, Element::Water, Type::Skylander}},
    {{14, 0x3810}, {"Gill Grunt (Eon's Elite)", Game::TrapTeam, Element::Water, Type::Skylander}},
    {{15, 0x0000}, {"Slam Bam", Game::SpyrosAdv, Element::Water, Type::Skylander}},
    {{15, 0x1801}, {"Slam Bam (S2)", Game::Giants, Element::Water, Type::Skylander}},
    {{15, 0x1C03}, {"Slam Bam (Legendary)", Game::Giants, Element::Water, Type::Skylander}},
    {{15, 0x3810},
     {"Slam Bam (Eon's Elite)", Game::Superchargers, Element::Water, Type::Skylander}},
    {{16, 0x0000}, {"Spyro", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{16, 0x1801}, {"Spyro (S2)", Game::Giants, Element::Magic, Type::Skylander}},
    {{16, 0x2805}, {"Spyro (Mega Ram)", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{16, 0x2C02}, {"Spyro (Dark Mega Ram)", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{16, 0x3810}, {"Spyro (Eon's Elite)", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{17, 0x0000}, {"Voodood", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{17, 0x3810}, {"Voodood (Eon's Elite)", Game::Superchargers, Element::Magic, Type::Skylander}},
    {{18, 0x0000}, {"Double Trouble", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{18, 0x1801}, {"Double Trouble (S2)", Game::Giants, Element::Magic, Type::Skylander}},
    {{18, 0x1C02}, {"Double Trouble (Royal)", Game::Giants, Element::Magic, Type::Skylander}},
    {{19, 0x0000}, {"Trigger Happy", Game::SpyrosAdv, Element::Tech, Type::Skylander}},
    {{19, 0x1801}, {"Trigger Happy (S2)", Game::Giants, Element::Tech, Type::Skylander}},
    {{19, 0x2805}, {"Trigger Happy (Big Bang)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{19, 0x2C02}, {"Trigger Happy (Springtime)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{19, 0x3810}, {"Trigger Happy (Eon's Elite)", Game::TrapTeam, Element::Tech, Type::Skylander}},
    {{20, 0x0000}, {"Drobot", Game::SpyrosAdv, Element::Tech, Type::Skylander}},
    {{20, 0x1206}, {"Drobot (Lightcore)", Game::Giants, Element::Tech, Type::Skylander}},
    {{20, 0x1801}, {"Drobot (S2)", Game::Giants, Element::Tech, Type::Skylander}},
    {{21, 0x0000}, {"Drill Sergeant", Game::SpyrosAdv, Element::Tech, Type::Skylander}},
    {{21, 0x1801}, {"Drill Sergeant (S2)", Game::Giants, Element::Tech, Type::Skylander}},
    {{22, 0x0000}, {"Boomer", Game::SpyrosAdv, Element::Tech, Type::Skylander}},
    {{22, 0x4810}, {"Boomer (Eon's Elite)", Game::Superchargers, Element::Tech, Type::Skylander}},
    {{23, 0x0000}, {"Wrecking Ball", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{23, 0x1801}, {"Wrecking Ball (S2)", Game::Giants, Element::Magic, Type::Skylander}},
    {{24, 0x0000}, {"Camo", Game::SpyrosAdv, Element::Life, Type::Skylander}},
    {{24, 0x2805}, {"Camo (Thorn Horn)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{25, 0x0000}, {"Zook", Game::SpyrosAdv, Element::Life, Type::Skylander}},
    {{25, 0x1801}, {"Zook (S2)", Game::Giants, Element::Life, Type::Skylander}},
    {{25, 0x3810}, {"Zook (Eon's Elite)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{26, 0x0000}, {"Stealth Elf", Game::SpyrosAdv, Element::Life, Type::Skylander}},
    {{26, 0x1801}, {"Stealth Elf (S2)", Game::Giants, Element::Life, Type::Skylander}},
    {{26, 0x1C03}, {"Stealth Elf (Legendary)", Game::Giants, Element::Life, Type::Skylander}},
    {{26, 0x2805}, {"Stealth Elf (Ninja)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{26, 0x2C02}, {"Stealth Elf (Dark)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{26, 0x3810}, {"Stealth Elf (Eon's Elite)", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{27, 0x0000}, {"Stump Smash", Game::SpyrosAdv, Element::Life, Type::Skylander}},
    {{27, 0x1801}, {"Stump Smash (S2)", Game::Giants, Element::Life, Type::Skylander}},
    {{28, 0x0000}, {"Spyro (Dark)", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{29, 0x0000}, {"Hex", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{29, 0x1206}, {"Hex (Lightcore)", Game::Giants, Element::Undead, Type::Skylander}},
    {{29, 0x1801}, {"Hex (S2)", Game::Giants, Element::Undead, Type::Skylander}},
    {{30, 0x0000}, {"Chop Chop", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{30, 0x1801}, {"Chop Chop (S2)", Game::Giants, Element::Undead, Type::Skylander}},
    {{30, 0x2805}, {"Chop Chop (Twin Blade)", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{30, 0x2816},
     {"Chop Chop (Green Twin Blade)", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{30, 0x3810}, {"Chop Chop (Eon's Elite)", Game::TrapTeam, Element::Undead, Type::Skylander}},
    {{31, 0x0000}, {"Ghost Roaster", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{31, 0x4810},
     {"Ghost Roaster (Eon's Elite)", Game::Superchargers, Element::Undead, Type::Skylander}},
    {{32, 0x0000}, {"Cynder", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{32, 0x1801}, {"Cynder (S2)", Game::Giants, Element::Undead, Type::Skylander}},
    {{32, 0x1811}, {"Cynder (Glow In The Dark)", Game::Giants, Element::Undead, Type::Skylander}},
    {{32, 0x2805}, {"Cynder (Phantom)", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{32, 0x301D}, {"Cynder (Clear)", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{100, 0x1000}, {"Jet-Vac", Game::Giants, Element::Air, Type::Skylander}},
    {{100, 0x1206}, {"Jet-Vac (Lightcore)", Game::Giants, Element::Air, Type::Skylander}},
    {{100, 0x1403}, {"Jet-Vac (Legendary)", Game::Giants, Element::Air, Type::Skylander}},
    {{100, 0x2805}, {"Jet Vac (Turbo)", Game::SwapForce, Element::Air, Type::Skylander}},
    {{100, 0x3805}, {"Jet-Vac (Full Blast)", Game::TrapTeam, Element::Air, Type::Skylander}},
    {{101, 0x1206}, {"Swarm", Game::Giants, Element::Air, Type::Giant}},
    {{102, 0x1206}, {"Crusher", Game::Giants, Element::Earth, Type::Giant}},
    {{102, 0x1602}, {"Crusher (Granite)", Game::Giants, Element::Earth, Type::Giant}},
    {{103, 0x1000}, {"Flashwing", Game::Giants, Element::Earth, Type::Skylander}},
    {{103, 0x1402}, {"Flashwing (Jade)", Game::Giants, Element::Earth, Type::Skylander}},
    {{103, 0x2206}, {"Flashwing (Lightcore)", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{104, 0x1206}, {"Hot Head", Game::Giants, Element::Fire, Type::Giant}},
    {{104, 0x1213}, {"Hot Head (Sparkle)", Game::Giants, Element::Fire, Type::Skylander}},
    {{105, 0x1000}, {"Hot Dog", Game::Giants, Element::Fire, Type::Skylander}},
    {{105, 0x1402}, {"Hot Dog (Molten)", Game::Giants, Element::Fire, Type::Skylander}},
    {{105, 0x2805}, {"Hot Dog (Fire Bone)", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{106, 0x1000}, {"Chill", Game::Giants, Element::Water, Type::Skylander}},
    {{106, 0x1206}, {"Chill (Lightcore)", Game::Giants, Element::Water, Type::Skylander}},
    {{106, 0x1603}, {"Chill (Legendary)", Game::Giants, Element::Water, Type::Skylander}},
    {{106, 0x2805}, {"Chill (Blizzard)", Game::SwapForce, Element::Water, Type::Skylander}},
    {{107, 0x1206}, {"Thumpback", Game::Giants, Element::Water, Type::Giant}},
    {{108, 0x1000}, {"Pop Fizz", Game::Giants, Element::Magic, Type::Skylander}},
    {{108, 0x1206}, {"Pop Fizz (Lightcore)", Game::Giants, Element::Magic, Type::Skylander}},
    {{108, 0x1402}, {"Pop Fizz (Punch)", Game::Giants, Element::Magic, Type::Skylander}},
    {{108, 0x2805}, {"Pop Fizz (Super Gulp)", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{108, 0x3805}, {"Pop Fizz (Fizzy Frenzy)", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{108, 0x3C02}, {"Pop Fizz (Love Potion)", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{109, 0x1206}, {"Ninjini", Game::Giants, Element::Magic, Type::Giant}},
    {{109, 0x1602}, {"Ninjini (Scarlet)", Game::Giants, Element::Magic, Type::Giant}},
    {{110, 0x1206}, {"Bouncer", Game::Giants, Element::Tech, Type::Giant}},
    {{110, 0x1603}, {"Bouncer (Legendary)", Game::Giants, Element::Tech, Type::Giant}},
    {{111, 0x1000}, {"Sprocket", Game::Giants, Element::Tech, Type::Skylander}},
    {{111, 0x2805}, {"Sprocket (Heavy Duty)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{111, 0x2819}, {"Sprocket (Heavy Metal)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{112, 0x1206}, {"Tree Rex", Game::Giants, Element::Life, Type::Giant}},
    {{112, 0x1602}, {"Tree Rex (Gnarly)", Game::Giants, Element::Life, Type::Giant}},
    {{113, 0x1000}, {"Shroomboom", Game::Giants, Element::Life, Type::Skylander}},
    {{113, 0x1206}, {"Shroomboom  (Lightcore)", Game::Giants, Element::Life, Type::Skylander}},
    {{113, 0x3801}, {"Shroomboom (Sure Shot)", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{114, 0x1206}, {"Eye-Brawl", Game::Giants, Element::Undead, Type::Giant}},
    {{114, 0x1215}, {"Eye-Brawl (Pumpkin)", Game::Giants, Element::Undead, Type::Giant}},
    {{115, 0x1000}, {"Fright Rider", Game::Giants, Element::Undead, Type::Skylander}},
    {{115, 0x1011}, {"Fright Rider (Halloween)", Game::Giants, Element::Undead, Type::Skylander}},
    {{115, 0x1811},
     {"Fright Rider (Glow In The Dark)", Game::Giants, Element::Undead, Type::Skylander}},
    {{404, 0x0000}, {"Bash (Legendary)", Game::SpyrosAdv, Element::Earth, Type::Skylander}},
    {{416, 0x0000}, {"Spyro (Legendary)", Game::SpyrosAdv, Element::Magic, Type::Skylander}},
    {{419, 0x0000}, {"Trigger Happy (Legendary)", Game::SpyrosAdv, Element::Tech, Type::Skylander}},
    {{430, 0x0000}, {"Chop Chop (Legendary)", Game::SpyrosAdv, Element::Undead, Type::Skylander}},
    {{450, 0x3000}, {"Gusto", Game::TrapTeam, Element::Air, Type::TrapMaster}},
    {{451, 0x3000}, {"Thunderbolt", Game::TrapTeam, Element::Air, Type::TrapMaster}},
    {{451, 0x301D}, {"Thunderbolt (Clear)", Game::TrapTeam, Element::Air, Type::TrapMaster}},
    {{452, 0x3000}, {"Fling Kong", Game::TrapTeam, Element::Air, Type::Skylander}},
    {{453, 0x3000}, {"Blades", Game::TrapTeam, Element::Air, Type::Skylander}},
    {{453, 0x3403}, {"Blades (Legendary)", Game::TrapTeam, Element::Air, Type::Skylander}},
    {{454, 0x3000}, {"Wallop", Game::TrapTeam, Element::Earth, Type::TrapMaster}},
    {{455, 0x3000}, {"Head Rush", Game::TrapTeam, Element::Earth, Type::TrapMaster}},
    {{455, 0x3402}, {"Head Rush (Nitro)", Game::TrapTeam, Element::Earth, Type::Skylander}},
    {{456, 0x3000}, {"Fist Bump", Game::TrapTeam, Element::Earth, Type::Skylander}},
    {{457, 0x3000}, {"Rocky Roll", Game::TrapTeam, Element::Earth, Type::Skylander}},
    {{458, 0x3000}, {"Wildfire", Game::TrapTeam, Element::Fire, Type::TrapMaster}},
    {{458, 0x3402}, {"Wildfire (Dark)", Game::TrapTeam, Element::Fire, Type::TrapMaster}},
    {{459, 0x3000}, {"Kaboom", Game::TrapTeam, Element::Fire, Type::TrapMaster}},
    {{460, 0x3000}, {"Trail Blazer", Game::TrapTeam, Element::Fire, Type::Skylander}},
    {{461, 0x3000}, {"Torch", Game::TrapTeam, Element::Fire, Type::Skylander}},
    {{462, 0x3000}, {"Snap Shot", Game::TrapTeam, Element::Water, Type::TrapMaster}},
    {{462, 0x450F}, {"Snap Shot (Virtual)", Game::TrapTeam, Element::Water, Type::TrapMaster}},
    {{462, 0x3402}, {"Snap Shot (Dark)", Game::TrapTeam, Element::Water, Type::TrapMaster}},
    {{463, 0x3000}, {"Lob Star", Game::TrapTeam, Element::Water, Type::TrapMaster}},
    {{463, 0x3402}, {"Lob Star (Winterfest)", Game::TrapTeam, Element::Water, Type::TrapMaster}},
    {{464, 0x3000}, {"Flip Wreck", Game::TrapTeam, Element::Water, Type::Skylander}},
    {{465, 0x3000}, {"Echo", Game::TrapTeam, Element::Water, Type::Skylander}},
    {{466, 0x3000}, {"Blastermind", Game::TrapTeam, Element::Magic, Type::TrapMaster}},
    {{467, 0x3000}, {"Enigma", Game::TrapTeam, Element::Magic, Type::TrapMaster}},
    {{468, 0x3000}, {"Deja Vu", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{468, 0x3403}, {"Deja Vu (Legendary)", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{469, 0x3000}, {"Cobra Cadabra", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{469, 0x3402}, {"Cobra Cadabra (King)", Game::TrapTeam, Element::Magic, Type::Skylander}},
    {{470, 0x3000}, {"Jawbreaker", Game::TrapTeam, Element::Tech, Type::TrapMaster}},
    {{470, 0x3403}, {"Jawbreaker (Legendary)", Game::TrapTeam, Element::Tech, Type::TrapMaster}},
    {{471, 0x3000}, {"Gearshift", Game::TrapTeam, Element::Tech, Type::TrapMaster}},
    {{472, 0x3000}, {"Chopper", Game::TrapTeam, Element::Tech, Type::Skylander}},
    {{473, 0x3000}, {"Tread Head", Game::TrapTeam, Element::Tech, Type::Skylander}},
    {{474, 0x3000}, {"Bushwhack", Game::TrapTeam, Element::Life, Type::TrapMaster}},
    {{474, 0x3403}, {"Bushwhack (Legendary)", Game::TrapTeam, Element::Life, Type::TrapMaster}},
    {{475, 0x3000}, {"Tuff Luck", Game::TrapTeam, Element::Life, Type::TrapMaster}},
    {{475, 0x301D}, {"Tuff Luck (Clear)", Game::TrapTeam, Element::Life, Type::TrapMaster}},
    {{476, 0x3000}, {"Food Fight", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{476, 0x3402}, {"Food Fight (Dark)", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{476, 0x450F}, {"Food Fight (Virtual)", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{477, 0x3000}, {"High Five", Game::TrapTeam, Element::Life, Type::Skylander}},
    {{478, 0x3000}, {"Krypt King", Game::TrapTeam, Element::Undead, Type::TrapMaster}},
    {{478, 0x3402}, {"Krypt King (Nitro)", Game::TrapTeam, Element::Undead, Type::TrapMaster}},
    {{479, 0x3000}, {"Short Cut", Game::TrapTeam, Element::Undead, Type::TrapMaster}},
    {{480, 0x3000}, {"Bat Spin", Game::TrapTeam, Element::Undead, Type::Skylander}},
    {{481, 0x3000}, {"Funny Bone", Game::TrapTeam, Element::Undead, Type::Skylander}},
    {{482, 0x3000}, {"Knight Light", Game::TrapTeam, Element::Light, Type::TrapMaster}},
    {{483, 0x3000}, {"Spot Light", Game::TrapTeam, Element::Light, Type::Skylander}},
    {{484, 0x3000}, {"Knight Mare", Game::TrapTeam, Element::Dark, Type::TrapMaster}},
    {{485, 0x3000}, {"Blackout", Game::TrapTeam, Element::Dark, Type::Skylander}},
    {{502, 0x3000}, {"Bop", Game::TrapTeam, Element::Earth, Type::Mini}},
    {{503, 0x3000}, {"Spry", Game::TrapTeam, Element::Magic, Type::Mini}},
    {{504, 0x3000}, {"Hijinx", Game::TrapTeam, Element::Undead, Type::Mini}},
    {{505, 0x3000}, {"Terrabite", Game::TrapTeam, Element::Earth, Type::Mini}},
    {{506, 0x3000}, {"Breeze", Game::TrapTeam, Element::Air, Type::Mini}},
    {{507, 0x3000}, {"Weeruptor", Game::TrapTeam, Element::Fire, Type::Mini}},
    {{507, 0x3402}, {"Weeruptor (Eggsellent)", Game::TrapTeam, Element::Fire, Type::Mini}},
    {{508, 0x3000}, {"Pet Vac", Game::TrapTeam, Element::Air, Type::Mini}},
    {{508, 0x3402}, {"Pet Vac (Power Punch)", Game::TrapTeam, Element::Air, Type::Mini}},
    {{509, 0x3000}, {"Small Fry", Game::TrapTeam, Element::Fire, Type::Mini}},
    {{510, 0x3000}, {"Drobit", Game::TrapTeam, Element::Tech, Type::Mini}},
    {{514, 0x3000}, {"Gill Runt", Game::TrapTeam, Element::Water, Type::Mini}},
    {{519, 0x3000}, {"Trigger Snappy", Game::TrapTeam, Element::Tech, Type::Mini}},
    {{526, 0x3000}, {"Whisper Elf", Game::TrapTeam, Element::Life, Type::Mini}},
    {{540, 0x1000}, {"Barkley (Sidekick)", Game::Giants, Element::Life, Type::Mini}},
    {{540, 0x3000}, {"Barkley", Game::TrapTeam, Element::Life, Type::Mini}},
    {{540, 0x3402}, {"Barkley (Gnarly)", Game::TrapTeam, Element::Life, Type::Mini}},
    {{541, 0x1000}, {"Thumpling (Sidekick)", Game::Giants, Element::Water, Type::Mini}},
    {{541, 0x3000}, {"Thumpling", Game::TrapTeam, Element::Water, Type::Mini}},
    {{542, 0x1000}, {"Mini Jini (Sidekick)", Game::Giants, Element::Magic, Type::Mini}},
    {{542, 0x3000}, {"Mini Jini", Game::TrapTeam, Element::Magic, Type::Mini}},
    {{543, 0x1000}, {"Eye-Small (Sidekick)", Game::Giants, Element::Undead, Type::Mini}},
    {{543, 0x3000}, {"Eye-Small", Game::TrapTeam, Element::Undead, Type::Mini}},
    {{1000, 0x2000}, {"Boom Jet (Bottom)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{1001, 0x2000}, {"Free Ranger (Bottom)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{1001, 0x2403},
     {"Free Ranger (Legendary) (Bottom)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{1002, 0x2000}, {"Rubble Rouser (Bottom)", Game::SwapForce, Element::Earth, Type::Swapper}},
    {{1003, 0x2000}, {"Doom Stone (Bottom)", Game::SwapForce, Element::Earth, Type::Swapper}},
    {{1004, 0x2000}, {"Blast Zone (Bottom)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{1004, 0x2402}, {"Blast Zone (Dark) (Bottom)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{1005, 0x2000}, {"Fire Kraken (Bottom)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{1005, 0x2402},
     {"Fire Kraken (Jade) (Bottom)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{1006, 0x2000}, {"Stink Bomb (Bottom)", Game::SwapForce, Element::Life, Type::Swapper}},
    {{1007, 0x2000}, {"Grilla Drilla (Bottom)", Game::SwapForce, Element::Life, Type::Swapper}},
    {{1008, 0x2000}, {"Hoot Loop (Bottom)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{1008, 0x2402},
     {"Hoot Loop (Enchanted) (Bottom)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{1009, 0x2000}, {"Trap Shadow (Bottom)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{1010, 0x2000}, {"Magna Charge (Bottom)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{1010, 0x2402},
     {"Magna Charge (Nitro) (Bottom)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{1011, 0x2000}, {"Spy Rise (Bottom)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{1012, 0x2000}, {"Night Shift (Bottom)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{1012, 0x2403},
     {"Night Shift (Legendary) (Bottom)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{1013, 0x2000}, {"Rattle Shake (Bottom)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{1013, 0x2402},
     {"Rattle Shake (Quickdraw) (Bottom)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{1014, 0x2000}, {"Freeze Blade (Bottom)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{1014, 0x2402},
     {"Freeze Blade (Nitro) (Bottom)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{1015, 0x2000}, {"Wash Buckler (Bottom)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{1015, 0x2402},
     {"Wash Buckler (Dark) (Bottom)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{2000, 0x2000}, {"Boom Jet (Top)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{2001, 0x2000}, {"Free Ranger (Top)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{2001, 0x2403},
     {"Free Ranger (Legendary) (Top)", Game::SwapForce, Element::Air, Type::Swapper}},
    {{2002, 0x2000}, {"Rubble Rouser (Top)", Game::SwapForce, Element::Earth, Type::Swapper}},
    {{2003, 0x2000}, {"Doom Stone (Top)", Game::SwapForce, Element::Earth, Type::Swapper}},
    {{2004, 0x2000}, {"Blast Zone (Top)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{2004, 0x2402}, {"Blast Zone (Dark) (Top)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{2005, 0x2000}, {"Fire Kraken (Top)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{2005, 0x2402}, {"Fire Kraken (Jade) (Top)", Game::SwapForce, Element::Fire, Type::Swapper}},
    {{2006, 0x2000}, {"Stink Bomb (Top)", Game::SwapForce, Element::Life, Type::Swapper}},
    {{2007, 0x2000}, {"Grilla Drilla (Top)", Game::SwapForce, Element::Life, Type::Swapper}},
    {{2008, 0x2000}, {"Hoot Loop (Top)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{2008, 0x2402},
     {"Hoot Loop (Enchanted) (Top)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{2009, 0x2000}, {"Trap Shadow (Top)", Game::SwapForce, Element::Magic, Type::Swapper}},
    {{2010, 0x2000}, {"Magna Charge (Top)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{2010, 0x2402}, {"Magna Charge (Nitro) (Top)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{2011, 0x2000}, {"Spy Rise (Top)", Game::SwapForce, Element::Tech, Type::Swapper}},
    {{2012, 0x2000}, {"Night Shift (Top)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{2012, 0x2403},
     {"Night Shift (Legendary) (Top)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{2013, 0x2000}, {"Rattle Shake (Top)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{2013, 0x2402},
     {"Rattle Shake (Quickdraw) (Top)", Game::SwapForce, Element::Undead, Type::Swapper}},
    {{2014, 0x2000}, {"Freeze Blade (Top)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{2014, 0x2402},
     {"Freeze Blade (Nitro) (Top)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{2015, 0x2000}, {"Wash Buckler (Top)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{2015, 0x2402}, {"Wash Buckler (Dark) (Top)", Game::SwapForce, Element::Water, Type::Swapper}},
    {{3000, 0x2000}, {"Scratch", Game::SwapForce, Element::Air, Type::Skylander}},
    {{3001, 0x2000}, {"Pop Thorn", Game::SwapForce, Element::Air, Type::Skylander}},
    {{3002, 0x2000}, {"Slobber Tooth", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{3002, 0x2402}, {"Slobber Tooth (Dark)", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{3003, 0x2000}, {"Scorp", Game::SwapForce, Element::Earth, Type::Skylander}},
    {{3004, 0x2000}, {"Fryno", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{3004, 0x3801}, {"Fryno (Hog Wild)", Game::TrapTeam, Element::Fire, Type::Skylander}},
    {{3005, 0x2000}, {"Smolderdash", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{3005, 0x2206}, {"Smolderdash (Lightcore)", Game::SwapForce, Element::Fire, Type::Skylander}},
    {{3006, 0x2000}, {"Bumble Blast", Game::SwapForce, Element::Life, Type::Skylander}},
    {{3006, 0x2206}, {"Bumble Blast (Lightcore)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{3006, 0x2402}, {"Bumble Blast (Jolly)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{3007, 0x2000}, {"Zoo Lou", Game::SwapForce, Element::Life, Type::Skylander}},
    {{3007, 0x2403}, {"Zoo Lou (Legendary)", Game::SwapForce, Element::Life, Type::Skylander}},
    {{3008, 0x2000}, {"Dune Bug", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{3009, 0x2000}, {"Star Strike", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{3009, 0x2206}, {"Star Strike (Lightcore)", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{3009, 0x2602},
     {"Star Strike (Lightcore Enchanted)", Game::SwapForce, Element::Magic, Type::Skylander}},
    {{3010, 0x2000}, {"Countdown", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{3010, 0x2206}, {"Countdown (Lightcore)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{3010, 0x2402}, {"Countdown (Kickoff)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{3011, 0x2000}, {"Wind Up", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{3011, 0x2404},
     {"Wind Up (Gear Head Vicarious Visions)", Game::SwapForce, Element::Tech, Type::Skylander}},
    {{3012, 0x2000}, {"Roller Brawl", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{3013, 0x2000}, {"Grim Creeper", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{3013, 0x2206},
     {"Grim Creeper (Lightcore)", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{3013, 0x2603},
     {"Grim Creeper (Legendary) (Lightcore)", Game::SwapForce, Element::Undead, Type::Skylander}},
    {{3014, 0x2000}, {"Rip Tide", Game::SwapForce, Element::Water, Type::Skylander}},
    {{3015, 0x2000}, {"Punk Shock", Game::SwapForce, Element::Water, Type::Skylander}},
    {{3400, 0x4100}, {"Fiesta", Game::Superchargers, Element::Undead, Type::Skylander}},
    {{3400, 0x4515}, {"Fiesta (Frightful)", Game::Superchargers, Element::Undead, Type::Skylander}},
    {{3401, 0x4100}, {"High Volt", Game::Superchargers, Element::Tech, Type::Skylander}},
    {{3402, 0x4100}, {"Splat", Game::Superchargers, Element::Magic, Type::Skylander}},
    {{3402, 0x4502}, {"Splat (Power Blue)", Game::Superchargers, Element::Magic, Type::Skylander}},
    {{3406, 0x4100}, {"Stormblade", Game::Superchargers, Element::Air, Type::Skylander}},
    {{3406, 0x4502}, {"Stormblade (Dark)", Game::Superchargers, Element::Air, Type::Skylander}},
    {{3406, 0x4503}, {"Stormblade (Dark)", Game::Superchargers, Element::Air, Type::Skylander}},
    {{3411, 0x4100}, {"Smash Hit", Game::Superchargers, Element::Earth, Type::Skylander}},
    {{3411, 0x4502},
     {"Smash Hit (Steel Plated)", Game::Superchargers, Element::Earth, Type::Skylander}},
    {{3412, 0x4100}, {"Spitfire", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3412, 0x4502}, {"Spitfire (Dark)", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3412, 0x450F}, {"Spitfire (Instant)", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3413, 0x4100}, {"Jet-Vac (Hurricane)", Game::Superchargers, Element::Air, Type::Skylander}},
    {{3413, 0x4503},
     {"Jet-Vac (Legendary Hurricane)", Game::Superchargers, Element::Air, Type::Skylander}},
    {{3414, 0x4100},
     {"Trigger Happy (Double Dare)", Game::Superchargers, Element::Tech, Type::Skylander}},
    {{3414, 0x4502},
     {"Trigger Happy (Power Blue)", Game::Superchargers, Element::Tech, Type::Skylander}},
    {{3415, 0x4100},
     {"Stealth Elf (Super Shot)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3415, 0x4502},
     {"Stealth Elf (Dark Super Shot)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3415, 0x450F},
     {"Stealth Elf (Instant)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3416, 0x4100},
     {"Terrafin (Shark Shooter)", Game::Superchargers, Element::Earth, Type::Skylander}},
    {{3417, 0x4100},
     {"Roller Brawl (Bone Bash)", Game::Superchargers, Element::Undead, Type::Skylander}},
    {{3417, 0x4503},
     {"Roller Brawl (Legendary Bone Bash)", Game::Superchargers, Element::Undead, Type::Skylander}},
    {{3420, 0x4100},
     {"Pop Fizz (Big Bubble)", Game::Superchargers, Element::Magic, Type::Skylander}},
    {{3420, 0x450E},
     {"Pop Fizz (Birthday Bash Big Bubble)", Game::Superchargers, Element::Magic, Type::Skylander}},
    {{3421, 0x4100}, {"Eruptor (Lava Lance)", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3422, 0x4100},
     {"Gill Grunt (Deep Dive)", Game::Superchargers, Element::Water, Type::Skylander}},
    {{3423, 0x4100},
     {"Donkey Kong (Turbo Charge)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3423, 0x4502},
     {"Donkey Kong (Dark Turbo Charge)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3424, 0x4100}, {"Bowser (Hammer Slam)", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3424, 0x4502},
     {"Bowser (Dark Hammer Slam)", Game::Superchargers, Element::Fire, Type::Skylander}},
    {{3425, 0x4100}, {"Dive-Clops", Game::Superchargers, Element::Water, Type::Skylander}},
    {{3425, 0x450E},
     {"Dive-Clops (Missile-Tow)", Game::Superchargers, Element::Water, Type::Skylander}},
    {{3425, 0x450F},
     {"Dive-Clops (Instant)", Game::Superchargers, Element::Water, Type::Skylander}},
    {{3426, 0x4100}, {"Astroblast", Game::Superchargers, Element::Tech, Type::Skylander}},
    {{3426, 0x4503},
     {"Astroblast (Legendary)", Game::Superchargers, Element::Light, Type::Skylander}},
    {{3427, 0x4100}, {"Nightfall", Game::Superchargers, Element::Dark, Type::Skylander}},
    {{3428, 0x4100}, {"Thrillipede", Game::Superchargers, Element::Life, Type::Skylander}},
    {{3428, 0x450D},
     {"Thrillipede (Eggcited)", Game::Superchargers, Element::Life, Type::Skylander}},
    {{200, 0x2000}, {"Anvil Rain", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{201, 0x2000}, {"Hidden Treasure", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{202, 0x2000}, {"Healing Elixer", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{203, 0x2000}, {"Ghost Pirate Swords", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{204, 0x2000}, {"Time Twister Hourglass", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{205, 0x2000}, {"Sky-Iron Shield", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{206, 0x2000}, {"Winged Boots", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{207, 0x2000}, {"Sparx Dragonfly", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{208, 0x1206}, {"Dragonfire Cannon", Game::Giants, Element::Other, Type::Item}},
    {{208, 0x1602}, {"Golden Dragonfire Cannon", Game::Giants, Element::Other, Type::Item}},
    {{209, 0x1206}, {"Scorpion Striker Catapult", Game::Giants, Element::Other, Type::Item}},
    {{230, 0x0000}, {"Hand Of Fate", Game::TrapTeam, Element::Other, Type::Item}},
    {{230, 0x3403}, {"Hand Of Fate (Legendary)", Game::TrapTeam, Element::Other, Type::Item}},
    {{231, 0x0000}, {"Piggy Bank", Game::TrapTeam, Element::Other, Type::Item}},
    {{232, 0x0000}, {"Rocket Ram", Game::TrapTeam, Element::Other, Type::Item}},
    {{233, 0x0000}, {"Tiki Speaky", Game::TrapTeam, Element::Other, Type::Item}},
    {{300, 0x0000}, {"Dragon's Peak", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{301, 0x2000}, {"Empire Of Ice", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{302, 0x2000}, {"Pirate Ship", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{303, 0x2000}, {"Darklight Crypt", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{304, 0x2000}, {"Volcanic Vault", Game::SpyrosAdv, Element::Other, Type::Item}},
    {{305, 0x3000}, {"Mirror Of Mystery", Game::TrapTeam, Element::Other, Type::Item}},
    {{306, 0x3000}, {"Nightmare Express", Game::TrapTeam, Element::Other, Type::Item}},
    {{307, 0x3206}, {"Sunscraper Spire", Game::TrapTeam, Element::Other, Type::Item}},
    {{308, 0x3206}, {"Midnight Museum", Game::TrapTeam, Element::Other, Type::Item}},
    {{3200, 0x2000}, {"Battle Hammer", Game::SwapForce, Element::Other, Type::Item}},
    {{3201, 0x2000}, {"Sky Diamond", Game::SwapForce, Element::Other, Type::Item}},
    {{3202, 0x2000}, {"Platinum Sheep", Game::SwapForce, Element::Other, Type::Item}},
    {{3203, 0x2000}, {"Groove Machine", Game::SwapForce, Element::Other, Type::Item}},
    {{3204, 0x2000}, {"Ufo Hat", Game::SwapForce, Element::Other, Type::Item}},
    {{3300, 0x2000}, {"Sheep Wreck Island", Game::SwapForce, Element::Other, Type::Item}},
    {{3301, 0x2000}, {"Tower Of Time", Game::SwapForce, Element::Other, Type::Item}},
    {{3302, 0x2206}, {"Fiery Forge", Game::SwapForce, Element::Other, Type::Item}},
    {{3303, 0x2206}, {"Arkeyan Crossbow", Game::SwapForce, Element::Other, Type::Item}},
    {{210, 0x3001}, {"Magic Log Holder", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{210, 0x3008}, {"Magic Skull", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{210, 0x300B}, {"Magic Axe", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{210, 0x300E}, {"Magic Hourglass", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{210, 0x3012}, {"Magic Totem", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{210, 0x3015}, {"Magic Rocket", Game::TrapTeam, Element::Magic, Type::Trap}},
    {{211, 0x3001}, {"Water Tiki", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x3002}, {"Water Log Holder", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x3006}, {"Water Jughead", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x3007}, {"Water Angel", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x300B}, {"Water Axe", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x3016}, {"Water Flying Helmet", Game::TrapTeam, Element::Water, Type::Trap}},
    {{211, 0x3406}, {"Water Jughead (Legendary)", Game::TrapTeam, Element::Water, Type::Trap}},
    {{212, 0x3003}, {"Air Toucan", Game::TrapTeam, Element::Air, Type::Trap}},
    {{212, 0x3006}, {"Air Jughead", Game::TrapTeam, Element::Air, Type::Trap}},
    {{212, 0x300E}, {"Air Hourglass", Game::TrapTeam, Element::Air, Type::Trap}},
    {{212, 0x3010}, {"Air Snake", Game::TrapTeam, Element::Air, Type::Trap}},
    {{212, 0x3011}, {"Air Screamer", Game::TrapTeam, Element::Air, Type::Trap}},
    {{212, 0x3018}, {"Air Sword", Game::TrapTeam, Element::Air, Type::Trap}},
    {{213, 0x3004}, {"Undead Orb", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x3008}, {"Undead Skull", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x300B}, {"Undead Axe", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x300C}, {"Undead Hand", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x3010}, {"Undead Snake", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x3017}, {"Undead Captain's Hat", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x3404}, {"Undead Orb (Legendary)", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{213, 0x3408}, {"Undead Skull (Legendary)", Game::TrapTeam, Element::Undead, Type::Trap}},
    {{214, 0x3001}, {"Tech Tiki", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{214, 0x3007}, {"Tech Angel", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{214, 0x3009}, {"Tech Scepter", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{214, 0x300C}, {"Tech Hand", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{214, 0x3016}, {"Tech Flying Helmet", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{214, 0x301A}, {"Tech Handstand", Game::TrapTeam, Element::Tech, Type::Trap}},
    {{215, 0x3001}, {"Fire Flower", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x3005}, {"Fire Torch", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x3009}, {"Fire Flower (NEW)", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x3011}, {"Fire Screamer", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x3012}, {"Fire Totem", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x3017}, {"Fire Captain's Hat", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{215, 0x301B}, {"Fire Yawn", Game::TrapTeam, Element::Fire, Type::Trap}},
    {{216, 0x3003}, {"Earth Toucan", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{216, 0x3004}, {"Earth Orb", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{216, 0x300A}, {"Earth Hammer", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{216, 0x300E}, {"Earth Hourglass", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{216, 0x3012}, {"Earth Totem", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{216, 0x301A}, {"Earth Handstand", Game::TrapTeam, Element::Earth, Type::Trap}},
    {{217, 0x3001}, {"Life Toucan", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x3003}, {"Life Toucan (NEW)", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x3005}, {"Life Torch", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x300A}, {"Life Hammer", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x3010}, {"Life Snake", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x3018}, {"Life Sword", Game::TrapTeam, Element::Life, Type::Trap}},
    {{217, 0x301B}, {"Life Yawn", Game::TrapTeam, Element::Life, Type::Trap}},
    {{218, 0x3014}, {"Dark Spider", Game::TrapTeam, Element::Dark, Type::Trap}},
    {{218, 0x3018}, {"Dark Sword", Game::TrapTeam, Element::Dark, Type::Trap}},
    {{218, 0x301A}, {"Dark Handstand", Game::TrapTeam, Element::Dark, Type::Trap}},
    {{219, 0x300F}, {"Light Owl", Game::TrapTeam, Element::Light, Type::Trap}},
    {{219, 0x3015}, {"Light Rocket", Game::TrapTeam, Element::Light, Type::Trap}},
    {{219, 0x301B}, {"Light Yawn", Game::TrapTeam, Element::Light, Type::Trap}},
    {{220, 0x301E}, {"Kaos", Game::TrapTeam, Element::Other, Type::Trap}},
    {{220, 0x351F}, {"Kaos (Ultimate)", Game::TrapTeam, Element::Other, Type::Trap}},
    {{3500, 0x4000}, {"Sky Trophy", Game::Superchargers, Element::Air, Type::Trophy}},
    {{3500, 0x4403}, {"Sky Trophy (Legendary)", Game::Superchargers, Element::Air, Type::Trophy}},
    {{3501, 0x4000}, {"Land Trophy", Game::Superchargers, Element::Earth, Type::Trophy}},
    {{3502, 0x4000}, {"Sea Trophy", Game::Superchargers, Element::Water, Type::Trophy}},
    {{3503, 0x4000}, {"Kaos Trophy", Game::Superchargers, Element::Other, Type::Trophy}},
    {{3220, 0x4000}, {"Jet Stream", Game::Superchargers, Element::Air, Type::Vehicle}},
    {{3221, 0x4000}, {"Tomb Buggy", Game::Superchargers, Element::Undead, Type::Vehicle}},
    {{3222, 0x4000}, {"Reef Ripper", Game::Superchargers, Element::Water, Type::Vehicle}},
    {{3223, 0x4000}, {"Burn-Cycle", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3224, 0x4000}, {"Hot Streak", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3224, 0x4004}, {"Hot Streak (Mobile)", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3224, 0x4402}, {"Hot Streak (Dark)", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3224, 0x441E},
     {"Hot Streak (Golden) (E3)", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3224, 0x450F}, {"Hot Streak (Instant)", Game::Superchargers, Element::Fire, Type::Vehicle}},
    {{3225, 0x4000}, {"Shark Tank", Game::Superchargers, Element::Earth, Type::Vehicle}},
    {{3226, 0x4000}, {"Thump Truck", Game::Superchargers, Element::Earth, Type::Vehicle}},
    {{3227, 0x4000}, {"Crypt Crusher", Game::Superchargers, Element::Undead, Type::Vehicle}},
    {{3228, 0x4000}, {"Stealth Stinger", Game::Superchargers, Element::Life, Type::Vehicle}},
    {{3228, 0x4402},
     {"Stealth Stinger (Nitro)", Game::Superchargers, Element::Life, Type::Vehicle}},
    {{3228, 0x450F},
     {"Stealth Stinger (Instant)", Game::Superchargers, Element::Life, Type::Vehicle}},
    {{3231, 0x4000}, {"Dive Bomber", Game::Superchargers, Element::Water, Type::Vehicle}},
    {{3231, 0x4402},
     {"Dive Bomber (Spring Ahead)", Game::Superchargers, Element::Water, Type::Vehicle}},
    {{3231, 0x450F}, {"Dive Bomber (Instant)", Game::Superchargers, Element::Water, Type::Vehicle}},
    {{3232, 0x4000}, {"Sky Slicer", Game::Superchargers, Element::Air, Type::Vehicle}},
    {{3233, 0x4000}, {"Clown Cruiser", Game::Superchargers, Element::Air, Type::Vehicle}},
    {{3233, 0x4402}, {"Clown Cruiser (Dark)", Game::Superchargers, Element::Air, Type::Vehicle}},
    {{3234, 0x4000}, {"Gold Rusher", Game::Superchargers, Element::Tech, Type::Vehicle}},
    {{3234, 0x4402},
     {"Gold Rusher (Power Blue)", Game::Superchargers, Element::Tech, Type::Vehicle}},
    {{3235, 0x4000}, {"Shield Striker", Game::Superchargers, Element::Tech, Type::Vehicle}},
    {{3236, 0x4000}, {"Sun Runner", Game::Superchargers, Element::Light, Type::Vehicle}},
    {{3236, 0x4403},
     {"Sun Runner (Legendary)", Game::Superchargers, Element::Light, Type::Vehicle}},
    {{3237, 0x4000}, {"Sea Shadow", Game::Superchargers, Element::Dark, Type::Vehicle}},
    {{3237, 0x4402}, {"Sea Shadow (Dark)", Game::Superchargers, Element::Dark, Type::Vehicle}},
    {{3238, 0x4000}, {"Splatter Splasher", Game::Superchargers, Element::Magic, Type::Vehicle}},
    {{3238, 0x4402},
     {"Splatter Splasher (Power Blue)", Game::Superchargers, Element::Magic, Type::Vehicle}},
    {{3239, 0x4000}, {"Soda Skimmer", Game::Superchargers, Element::Magic, Type::Vehicle}},
    {{3239, 0x4402}, {"Soda Skimmer (Nitro)", Game::Superchargers, Element::Magic, Type::Vehicle}},
    {{3240, 0x4000}, {"Barrel Blaster", Game::Superchargers, Element::Tech, Type::Vehicle}},
    {{3240, 0x4402}, {"Barrel Blaster (Dark)", Game::Superchargers, Element::Tech, Type::Vehicle}},
    {{3241, 0x4000}, {"Buzz Wing", Game::Superchargers, Element::Life, Type::Vehicle}},
};

SkylanderUSB::SkylanderUSB(EmulationKernel& ios, const std::string& device_name) : m_ios(ios)
{
  m_vid = 0x1430;
  m_pid = 0x150;
  m_id = (static_cast<u64>(m_vid) << 32 | static_cast<u64>(m_pid) << 16 | static_cast<u64>(9) << 8 |
          static_cast<u64>(1));
  m_device_descriptor = DeviceDescriptor{0x12,   0x1,   0x200, 0x0, 0x0, 0x0, 0x40,
                                         0x1430, 0x150, 0x100, 0x1, 0x2, 0x0, 0x1};
  m_config_descriptor.emplace_back(ConfigDescriptor{0x9, 0x2, 0x29, 0x1, 0x1, 0x0, 0x80, 0xFA});
  m_interface_descriptor.emplace_back(
      InterfaceDescriptor{0x9, 0x4, 0x0, 0x0, 0x2, 0x3, 0x0, 0x0, 0x0});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x81, 0x3, 0x40, 0x1});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x2, 0x3, 0x40, 0x1});
}

SkylanderUSB::~SkylanderUSB() = default;

DeviceDescriptor SkylanderUSB::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> SkylanderUSB::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> SkylanderUSB::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> SkylanderUSB::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool SkylanderUSB::Attach()
{
  if (m_device_attached)
  {
    return true;
  }
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  m_device_attached = true;
  return true;
}

bool SkylanderUSB::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int SkylanderUSB::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int SkylanderUSB::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int SkylanderUSB::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int SkylanderUSB::SetAltSetting(u8 alt_setting)
{
  return 0;
}

// Skylander Portal control transfers are handled via this method, if a transfer requires a
// response, then we request that data from the "Portal" and queue it to be responded to via the
// Interrupt Response, references can be found via:
// https://marijnkneppers.dev/posts
// https://github.com/tresni/PoweredPortals/wiki/USB-Protocols
// https://pastebin.com/EqtTRzeF
int SkylanderUSB::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  // If not HID Host to Device type, return invalid
  if (cmd->request_type != 0x21)
    return IPC_EINVAL;

  // Data to be sent back via the control transfer immediately
  std::array<u8, 64> control_response = {};
  s32 expected_count = 0;
  u64 expected_time_us = 100;

  // Non 0x09 Requests are handled here - no portal data is requested
  if (cmd->request != 0x09)
  {
    switch (cmd->request)
    {
    // Get Interface
    case 0x0A:
      expected_count = 8;
      break;
    // Set Interface
    case 0x0B:
      expected_count = 8;
      break;
    default:
      ERROR_LOG_FMT(IOS_USB, "Unhandled Request {}", cmd->request);
      break;
    }
  }
  else
  {
    // Skylander Portal Requests
    auto& system = m_ios.GetSystem();
    auto& memory = system.GetMemory();
    u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
    if (cmd->length == 0 || buf == nullptr)
    {
      ERROR_LOG_FMT(IOS_USB, "Skylander command invalid");
      return IPC_EINVAL;
    }
    // Data to be queued to be sent back via the Interrupt Transfer (if needed)
    std::array<u8, 64> interrupt_response = {};

    // The first byte of the Control Request is always a char for Skylanders
    switch (buf[0])
    {
    case 'A':
    {
      // Activation
      // Command	{ 'A', (00 | 01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'A', (00 | 01),
      // ff, 77, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00 }
      // The 2nd byte of the command is whether to activate (0x01) or deactivate (0x00) the
      // portal. The response echos back the activation byte as the 2nd byte of the response. The
      // 3rd and 4th bytes of the response appear to vary from wired to wireless. On wired
      // portals, the bytes appear to always be ff 77. On wireless portals, during activation the
      // 3rd byte appears to count down from ff (possibly a battery power indication) and during
      // deactivation ed and eb responses have been observed. The 4th byte appears to always be 00
      // for wireless portals.

      // Wii U Wireless: 41 01 f4 00 41 00 ed 00 41 01 f4 00 41 00 eb 00 41 01 f3 00 41 00 ed 00
      if (cmd->length == 2)
      {
        control_response = {buf[0], buf[1]};
        interrupt_response = {0x41, buf[1], 0xFF, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        m_queries.push(interrupt_response);
        expected_count = 10;
        system.GetSkylanderPortal().Activate();
      }
      break;
    }
    case 'C':
    {
      // Color
      // Command	{ 'C', 12, 34, 56, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'C', 12, 34, 56, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // The 3 bytes {12, 34, 56} are RGB values.

      // This command should set the color of the LED in the portal, however this appears
      // deprecated in most of the recent portals. On portals that do not have LEDs, this command
      // is silently ignored and do not require a response.
      if (cmd->length == 4)
      {
        system.GetSkylanderPortal().SetLEDs(0x01, buf[1], buf[2], buf[3]);
        control_response = {0x43, buf[1], buf[2], buf[3]};
        expected_count = 12;
      }
      break;
    }
    case 'J':
    {
      // Sided color
      // The 2nd byte is the side
      // 0x00: right
      // 0x02: left

      // The 3rd, 4th and 5th bytes are red, green and blue

      // The 6th and 7th bytes form a little-endian short for how long the fade duration should be
      // in milliseconds.
      // For example, 500 milliseconds becomes 0xF4, 0x01
      if (cmd->length == 7)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]};
        expected_count = 15;
        interrupt_response = {buf[0]};
        m_queries.push(interrupt_response);
        system.GetSkylanderPortal().SetLEDs(buf[1], buf[2], buf[3], buf[4]);
      }
      break;
    }
    case 'L':
    {
      // Light
      // This command is used while playing audio through the portal

      // The 2nd bytes is the position
      // 0x00: right
      // 0x01: trap led
      // 0x02: left

      // The 3rd, 4th and 5th bytes are red, green and blue
      // the trap led is white-only
      // increasing or decreasing the values results in a brighter or dimmer light
      if (cmd->length == 5)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3], buf[4]};
        expected_count = 13;

        u8 side = buf[1];
        if (side == 0x02)
        {
          side = 0x04;
        }
        system.GetSkylanderPortal().SetLEDs(side, buf[2], buf[3], buf[4]);
      }
      break;
    }
    case 'M':
    {
      // Audio Firmware version
      // Respond with version obtained from Trap Team wired portal
      if (cmd->length == 2)
      {
        control_response = {buf[0], buf[1]};
        expected_count = 10;
        interrupt_response = {buf[0], buf[1], 0x00, 0x19};
        m_queries.push(interrupt_response);
      }
      break;
    }
      // Query
      // Command	{ 'Q', 10, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'Q', 10, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // In the command the 2nd byte indicates which Skylander to query data
      // from. Index starts at 0x10 for the 1st Skylander (as reported in the Status command.) The
      // 16th Skylander indexed would be 0x20. The 3rd byte indicate which block to read from.

      // A response with the 2nd byte of 0x01 indicates an error in the read. Otherwise, the
      // response indicates the Skylander's index in the 2nd byte, the block read in the 3rd byte,
      // data (16 bytes) is contained in bytes 4-19.

      // A Skylander has 64 blocks of data indexed from 0x00 to 0x3f. SwapForce characters have 2
      // character indexes, these may not be sequential.
    case 'Q':
    {
      if (cmd->length == 3)
      {
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        system.GetSkylanderPortal().QueryBlock(sky_num, block, interrupt_response.data());
        m_queries.push(interrupt_response);
        control_response = {buf[0], buf[1], buf[2]};
        expected_count = 11;
      }
      break;
    }
    case 'R':
    {
      // Ready
      // Command	{ 'R', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'R', 02, 0a, 03, 02, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // The 4 byte sequence after the R (0x52) is unknown, but appears consistent based on device
      // type.
      if (cmd->length == 2)
      {
        control_response = {0x52, 0x00};
        interrupt_response = {0x52, 0x02, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        m_queries.push(interrupt_response);
        expected_count = 10;
      }
      break;
    }
      // Status
      // Command	{ 'S', 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'S', 55, 00, 00, 55, 3e,
      // (00|01), 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00 }
      // Status is the default command. If you open the HID device and
      // activate the portal, you will get status outputs.

      // The 4 bytes {55, 00, 00, 55} are the status of characters on the portal. The 4 bytes are
      // treated as a 32-bit binary array. Each unique Skylander placed on a board is represented
      // by 2 bits starting with the first Skylander in the least significant bit. This bit is
      // present whenever the Skylandar is added or present on the portal. When the Skylander is
      // added to the board, both bits are set in the next status message as a one-time signal.
      // When a Skylander is removed from the board, only the most significant bit of the 2 bits
      // is set.

      // Different portals can track a different number of RFID tags. The Wii Wireless portal
      // tracks 4, the Wired portal can track 8. The maximum number of unique Skylanders tracked
      // at any time is 16, after which new Skylanders appear to cycle unused bits.

      // Certain Skylanders, e.g. SwapForce Skylanders, are represented as 2 ID in the bit array.
      // This may be due to the presence of 2 RFIDs, one for each half of the Skylander.

      // The 6th byte {3e} is a counter and increments by one. It will roll over when reaching
      // {ff}.

      // The purpose of the (00\|01) byte at the 7th position appear to indicate if the portal has
      // been activated: {01} when active and {00} when deactivated.
    case 'S':
    {
      if (cmd->length == 1)
      {
        // The Status interrupt responses are automatically handled via the GetStatus method
        control_response = {buf[0]};
        expected_count = 9;
      }
      break;
    }
    case 'V':
    {
      if (cmd->length == 4)
      {
        control_response = {buf[0], buf[1], buf[2], buf[3]};
        expected_count = 12;
      }
      break;
    }
      // Write
      // Command	{ 'W', 10, 00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
      // Response	{ 'W', 00, 00, 00, 00, 00,
      // 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      // 00, 00, 00, 00 }
      // In the command the 2nd byte indicates which Skylander to query data from. Index starts at
      // 0x10 for the 1st Skylander (as reported in the Status command.) The 16th Skylander
      // indexed would be 0x20.

      // 3rd byte is the block to write to.

      // Bytes 4 - 19 ({ 01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f }) are the
      // data to write.

      // The response does not appear to return the id of the Skylander being written, the 2nd
      // byte is 0x00; however, the 3rd byte echos the block that was written (0x00 in example
      // above.)

    case 'W':
    {
      if (cmd->length == 19)
      {
        const u8 sky_num = buf[1] & 0xF;
        const u8 block = buf[2];
        system.GetSkylanderPortal().WriteBlock(sky_num, block, &buf[3], interrupt_response.data());
        m_queries.push(interrupt_response);
        control_response = {buf[0],  buf[1],  buf[2],  buf[3],  buf[4],  buf[5],  buf[6],
                            buf[7],  buf[8],  buf[9],  buf[10], buf[11], buf[12], buf[13],
                            buf[14], buf[15], buf[16], buf[17], buf[18]};
        expected_count = 27;
      }
      break;
    }
    default:
      ERROR_LOG_FMT(IOS_USB, "Unhandled Skylander Portal Query: {}", buf[0]);
      break;
    }
  }

  if (expected_count == 0)
    return IPC_EINVAL;

  ScheduleTransfer(std::move(cmd), control_response, expected_count, expected_time_us);
  return 0;
}

int SkylanderUSB::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  return 0;
}

// When an Interrupt Message is received by the Skylander Portal from the console,
// it needs to respond with either the status of the console if there are no Control Messages that
// require a response, or with the relevant response data that is requested from the Control
// Message.
int SkylanderUSB::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={} endpoint={}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);

  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (cmd->length == 0 || buf == nullptr)
  {
    ERROR_LOG_FMT(IOS_USB, "Skylander command invalid");
    return IPC_EINVAL;
  }
  std::array<u8, 64> interrupt_response = {};
  s32 expected_count;
  u64 expected_time_us;
  // Audio requests are 64 bytes long, are the only Interrupt requests longer than 32 bytes,
  // echo the request as the response and respond immediately
  if (cmd->length > 32 && cmd->length <= 64)
  {
    // Play audio through Portal Mixer
    // Audio is unsigned 16 bit, supplied as 64 bytes which is 32 samples
    SoundStream* sound_stream = system.GetSoundStream();
    sound_stream->GetMixer()->PushSkylanderPortalSamples(buf, cmd->length / 2);

    std::array<u8, 64> audio_interrupt_response = {};
    u8* audio_buf = audio_interrupt_response.data();
    memcpy(audio_buf, buf, cmd->length);
    expected_time_us = 0;
    expected_count = cmd->length;
    ScheduleTransfer(std::move(cmd), audio_interrupt_response, expected_count, expected_time_us);

    return 0;
  }
  // If some data was requested from the Control Message, then the Interrupt message needs to
  // respond with that data. Check if the queries queue is empty
  if (!m_queries.empty())
  {
    interrupt_response = m_queries.front();
    m_queries.pop();
    // This needs to happen after ~22 milliseconds
    expected_time_us = 22000;
  }
  // If there is no relevant data to respond with, respond with the currentstatus of the Portal
  else
  {
    interrupt_response = system.GetSkylanderPortal().GetStatus();
    expected_time_us = 2000;
  }
  expected_count = 32;
  ScheduleTransfer(std::move(cmd), interrupt_response, expected_count, expected_time_us);
  return 0;
}

int SkylanderUSB::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Isochronous: length={} endpoint={} num_packets={}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  return 0;
}

void SkylanderUSB::ScheduleTransfer(std::unique_ptr<TransferCommand> command,
                                    const std::array<u8, 64>& data, s32 expected_count,
                                    u64 expected_time_us)
{
  command->FillBuffer(data.data(), expected_count);
  command->ScheduleTransferCompletion(expected_count, expected_time_us);
}

void SkylanderPortal::Activate()
{
  std::lock_guard lock(sky_mutex);
  if (m_activated)
  {
    // If the portal was already active no change is needed
    return;
  }

  // If not we need to advertise change to all the figures present on the portal
  for (auto& s : skylanders)
  {
    if (s.status & 1)
    {
      s.queued_status.push(Skylander::ADDED);
      s.queued_status.push(Skylander::READY);
    }
  }

  m_activated = true;
}

void SkylanderPortal::Deactivate()
{
  std::lock_guard lock(sky_mutex);

  for (auto& s : skylanders)
  {
    // check if at the end of the updates there would be a figure on the portal
    if (!s.queued_status.empty())
    {
      s.status = s.queued_status.back();
      s.queued_status = std::queue<u8>();
    }

    s.status &= 1;
  }

  m_activated = false;
}

bool SkylanderPortal::IsActivated()
{
  std::lock_guard lock(sky_mutex);

  return m_activated;
}

void SkylanderPortal::UpdateStatus()
{
  std::lock_guard lock(sky_mutex);

  if (!m_status_updated)
  {
    for (auto& s : skylanders)
    {
      if (s.status & 1)
      {
        s.queued_status.push(Skylander::REMOVED);
        s.queued_status.push(Skylander::ADDED);
        s.queued_status.push(Skylander::READY);
      }
    }
    m_status_updated = true;
  }
}

// Side:
// 0x00 = right
// 0x01 = left and right
// 0x02 = left
// 0x03 = trap
void SkylanderPortal::SetLEDs(u8 side, u8 red, u8 green, u8 blue)
{
  std::lock_guard lock(sky_mutex);
  if (side == 0x00)
  {
    m_color_right.red = red;
    m_color_right.green = green;
    m_color_right.blue = blue;
  }
  else if (side == 0x01)
  {
    m_color_right.red = red;
    m_color_right.green = green;
    m_color_right.blue = blue;

    m_color_left.red = red;
    m_color_left.green = green;
    m_color_left.blue = blue;
  }
  else if (side == 0x02)
  {
    m_color_left.red = red;
    m_color_left.green = green;
    m_color_left.blue = blue;
  }
  else if (side == 0x03)
  {
    m_color_trap.red = red;
    m_color_trap.green = green;
    m_color_trap.blue = blue;
  }
}

std::array<u8, 64> SkylanderPortal::GetStatus()
{
  std::lock_guard lock(sky_mutex);

  u32 status = 0;
  u8 active = 0x00;

  if (m_activated)
  {
    active = 0x01;
  }

  for (int i = MAX_SKYLANDERS - 1; i >= 0; i--)
  {
    auto& s = skylanders[i];

    if (!s.queued_status.empty())
    {
      s.status = s.queued_status.front();
      s.queued_status.pop();
    }
    status <<= 2;
    status |= s.status;
  }

  std::array<u8, 64> interrupt_response = {0x53,   0x00, 0x00, 0x00, 0x00, m_interrupt_counter++,
                                           active, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00,   0x00};
  memcpy(&interrupt_response[1], &status, sizeof(status));
  return interrupt_response;
}

void SkylanderPortal::QueryBlock(u8 sky_num, u8 block, u8* reply_buf)
{
  if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
    return;

  std::lock_guard lock(sky_mutex);

  const auto& skylander = skylanders[sky_num];

  reply_buf[0] = 'Q';
  reply_buf[2] = block;
  if (skylander.status & Skylander::READY)
  {
    reply_buf[1] = (0x10 | sky_num);
    skylander.figure->GetBlock(block, reply_buf + 3);
  }
  else
  {
    reply_buf[1] = 0x01;
  }
}

void SkylanderPortal::WriteBlock(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf)
{
  if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
    return;

  std::lock_guard lock(sky_mutex);

  auto& skylander = skylanders[sky_num];

  reply_buf[0] = 'W';
  reply_buf[2] = block;

  if (skylander.status & 1)
  {
    reply_buf[1] = (0x10 | sky_num);
    skylander.figure->SetBlock(block, to_write_buf);
    skylander.figure->Save();
  }
  else
  {
    reply_buf[1] = 0x01;
  }
}

bool SkylanderPortal::RemoveSkylander(u8 sky_num)
{
  if (!IsSkylanderNumberValid(sky_num))
    return false;

  DEBUG_LOG_FMT(IOS_USB, "Cleared Skylander from slot {}", sky_num);
  std::lock_guard lock(sky_mutex);
  auto& skylander = skylanders[sky_num];

  if (skylander.figure->FileIsOpen())
  {
    skylander.figure->Save();
    skylander.figure->Close();
  }

  if (skylander.status & Skylander::READY)
  {
    skylander.status = Skylander::REMOVING;
    skylander.queued_status.push(Skylander::REMOVING);
    skylander.queued_status.push(Skylander::REMOVED);
    return true;
  }

  return false;
}

u8 SkylanderPortal::LoadSkylander(std::unique_ptr<SkylanderFigure> figure)
{
  std::lock_guard lock(sky_mutex);

  u32 sky_serial = 0;
  std::array<u8, 0x10> block = {};
  figure->GetBlock(0, block.data());
  for (int i = 3; i > -1; i--)
  {
    sky_serial <<= 8;
    sky_serial |= block[i];
  }
  u8 found_slot = 0xFF;

  // mimics spot retaining on the portal
  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    if ((skylanders[i].status & 1) == 0)
    {
      if (skylanders[i].last_id == sky_serial)
      {
        DEBUG_LOG_FMT(IOS_USB, "Last Id: {}", skylanders[i].last_id);

        found_slot = i;
        break;
      }

      if (i < found_slot)
      {
        DEBUG_LOG_FMT(IOS_USB, "Last Id: {}", skylanders[i].last_id);
        found_slot = i;
      }
    }
  }

  if (found_slot != 0xFF)
  {
    auto& skylander = skylanders[found_slot];
    skylander.figure = std::move(figure);
    skylander.status = Skylander::ADDED;
    skylander.queued_status.push(Skylander::ADDED);
    skylander.queued_status.push(Skylander::READY);
    skylander.last_id = sky_serial;
  }
  return found_slot;
}

bool SkylanderPortal::IsSkylanderNumberValid(u8 sky_num)
{
  return sky_num < MAX_SKYLANDERS;
}

bool SkylanderPortal::IsBlockNumberValid(u8 block)
{
  return block < 64;
}

std::pair<u16, u16> SkylanderPortal::CalculateIDs(const std::array<u8, 0x40 * 0x10>& file_data)
{
  u16 m_sky_id = file_data[0x11];
  u16 m_sky_var = file_data[0x1D];
  m_sky_id <<= 8;
  m_sky_var <<= 8;
  m_sky_id |= file_data[0x10];
  m_sky_var |= file_data[0x1C];
  return std::make_pair(m_sky_id, m_sky_var);
}

Skylander* SkylanderPortal::GetSkylander(u8 slot)
{
  return &skylanders[slot];
}

Type NormalizeSkylanderType(Type type)
{
  switch (type)
  {
  case Type::Skylander:
  case Type::Giant:
  case Type::Swapper:
  case Type::TrapMaster:
  case Type::Mini:
    return Type::Skylander;
  case Type::Trophy:
    return Type::Trophy;
  default:
  case Type::Item:
  case Type::Trap:
  case Type::Vehicle:
  case Type::Unknown:
    // until these get seperate data logic (except unknown and item since items don't save data and
    // unknown is unknown)
    return Type::Unknown;
  }
}

}  // namespace IOS::HLE::USB
