// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylander.h"

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
#include "Core/System.h"

namespace IOS::HLE::USB
{
const std::map<const std::pair<const u16, const u16>, const char*> list_skylanders = {
    {{0, 0x0000}, "Whirlwind"},
    {{0, 0x1801}, "Series 2 Whirlwind"},
    {{0, 0x1C02}, "Polar Whirlwind"},
    {{0, 0x2805}, "Horn Blast Whirlwind"},
    {{0, 0x3810}, "Eon's Elite Whirlwind"},
    {{1, 0x0000}, "Sonic Boom"},
    {{1, 0x1801}, "Series 2 Sonic Boom"},
    {{2, 0x0000}, "Warnado"},
    {{2, 0x2206}, "LightCore Warnado"},
    {{3, 0x0000}, "Lightning Rod"},
    {{3, 0x1801}, "Series 2 Lightning Rod"},
    {{4, 0x0000}, "Bash"},
    {{4, 0x1801}, "Series 2 Bash"},
    {{5, 0x0000}, "Terrafin"},
    {{5, 0x1801}, "Series 2 Terrafin"},
    {{5, 0x2805}, "Knockout Terrafin"},
    {{5, 0x3810}, "Eon's Elite Terrafin"},
    {{6, 0x0000}, "Dino Rang"},
    {{6, 0x4810}, "Eon's Elite Dino Rang"},
    {{7, 0x0000}, "Prism Break"},
    {{7, 0x1801}, "Series 2 Prism Break"},
    {{7, 0x2805}, "Hyper Beam Prism Break"},
    {{7, 0x1206}, "LightCore Prism Break"},
    {{8, 0x0000}, "Sunburn"},
    {{9, 0x0000}, "Eruptor"},
    {{9, 0x1801}, "Series 2 Eruptor"},
    {{9, 0x2C02}, "Volcanic Eruptor"},
    {{9, 0x2805}, "Lava Barf Eruptor"},
    {{9, 0x1206}, "LightCore Eruptor"},
    {{9, 0x3810}, "Eon's Elite Eruptor"},
    {{10, 0x0000}, "Ignitor"},
    {{10, 0x1801}, "Series 2 Ignitor"},
    {{10, 0x1C03}, "Legendary Ignitor"},
    {{11, 0x0000}, "Flameslinger"},
    {{11, 0x1801}, "Series 2 Flameslinger"},
    {{12, 0x0000}, "Zap"},
    {{12, 0x1801}, "Series 2 Zap"},
    {{13, 0x0000}, "Wham Shell"},
    {{13, 0x2206}, "LightCore Wham Shell"},
    {{14, 0x0000}, "Gill Grunt"},
    {{14, 0x1801}, "Series 2 Gill Grunt"},
    {{14, 0x2805}, "Anchors Away Gill Grunt"},
    {{14, 0x3805}, "Tidal Wave Gill Grunt"},
    {{14, 0x3810}, "Eon's Elite Gill Grunt"},
    {{15, 0x0000}, "Slam Bam"},
    {{15, 0x1801}, "Series 2 Slam Bam"},
    {{15, 0x1C03}, "Legendary Slam Bam"},
    {{15, 0x4810}, "Eon's Elite Slam Bam"},
    {{16, 0x0000}, "Spyro"},
    {{16, 0x1801}, "Series 2 Spyro"},
    {{16, 0x2C02}, "Dark Mega Ram Spyro"},
    {{16, 0x2805}, "Mega Ram Spyro"},
    {{16, 0x3810}, "Eon's Elite Spyro"},
    {{17, 0x0000}, "Voodood"},
    {{17, 0x4810}, "Eon's Elite Voodood"},
    {{18, 0x0000}, "Double Trouble"},
    {{18, 0x1801}, "Series 2 Double Trouble"},
    {{18, 0x1C02}, "Royal Double Trouble"},
    {{19, 0x0000}, "Trigger Happy"},
    {{19, 0x1801}, "Series 2 Trigger Happy"},
    {{19, 0x2C02}, "Springtime Trigger Happy"},
    {{19, 0x2805}, "Big Bang Trigger Happy"},
    {{19, 0x3810}, "Eon's Elite Trigger Happy"},
    {{20, 0x0000}, "Drobot"},
    {{20, 0x1801}, "Series 2 Drobot"},
    {{20, 0x1206}, "LightCore Drobot"},
    {{21, 0x0000}, "Drill Seargeant"},
    {{21, 0x1801}, "Series 2 Drill Seargeant"},
    {{22, 0x0000}, "Boomer"},
    {{22, 0x4810}, "Eon's Elite Boomer"},
    {{23, 0x0000}, "Wrecking Ball"},
    {{23, 0x1801}, "Series 2 Wrecking Ball"},
    {{24, 0x0000}, "Camo"},
    {{24, 0x2805}, "Thorn Horn Camo"},
    {{25, 0x0000}, "Zook"},
    {{25, 0x1801}, "Series 2 Zook"},
    {{25, 0x4810}, "Eon's Elite Zook"},
    {{26, 0x0000}, "Stealth Elf"},
    {{26, 0x1801}, "Series 2 Stealth Elf"},
    {{26, 0x2C02}, "Dark Stealth Elf"},
    {{26, 0x1C03}, "Legendary Stealth Elf"},
    {{26, 0x2805}, "Ninja Stealth Elf"},
    {{26, 0x3810}, "Eon's Elite Stealth Elf"},
    {{27, 0x0000}, "Stump Smash"},
    {{27, 0x1801}, "Series 2 Stump Smash"},
    {{28, 0x0000}, "Dark Spyro"},
    {{29, 0x0000}, "Hex"},
    {{29, 0x1801}, "Series 2 Hex"},
    {{29, 0x1206}, "LightCore Hex"},
    {{30, 0x0000}, "Chop Chop"},
    {{30, 0x1801}, "Series 2 Chop Chop"},
    {{30, 0x2805}, "Twin Blade Chop Chop"},
    {{30, 0x3810}, "Eon's Elite Chop Chop"},
    {{31, 0x0000}, "Ghost Roaster"},
    {{31, 0x4810}, "Eon's Elite Ghost Roaster"},
    {{32, 0x0000}, "Cynder"},
    {{32, 0x1801}, "Series 2 Cynder"},
    {{32, 0x2805}, "Phantom Cynder"},
    {{100, 0x0000}, "Jet Vac"},
    {{100, 0x1403}, "Legendary Jet Vac"},
    {{100, 0x2805}, "Turbo Jet Vac"},
    {{100, 0x3805}, "Full Blast Jet Vac"},
    {{100, 0x1206}, "LightCore Jet Vac"},
    {{101, 0x0000}, "Swarm"},
    {{102, 0x0000}, "Crusher"},
    {{102, 0x1602}, "Granite Crusher"},
    {{103, 0x0000}, "Flashwing"},
    {{103, 0x1402}, "Jade Flash Wing"},
    {{103, 0x2206}, "LightCore Flashwing"},
    {{104, 0x0000}, "Hot Head"},
    {{105, 0x0000}, "Hot Dog"},
    {{105, 0x1402}, "Molten Hot Dog"},
    {{105, 0x2805}, "Fire Bone Hot Dog"},
    {{106, 0x0000}, "Chill"},
    {{106, 0x1603}, "Legendary Chill"},
    {{106, 0x2805}, "Blizzard Chill"},
    {{106, 0x1206}, "LightCore Chill"},
    {{107, 0x0000}, "Thumpback"},
    {{108, 0x0000}, "Pop Fizz"},
    {{108, 0x1402}, "Punch Pop Fizz"},
    {{108, 0x3C02}, "Love Potion Pop Fizz"},
    {{108, 0x2805}, "Super Gulp Pop Fizz"},
    {{108, 0x3805}, "Fizzy Frenzy Pop Fizz"},
    {{108, 0x1206}, "LightCore Pop Fizz"},
    {{109, 0x0000}, "Ninjini"},
    {{109, 0x1602}, "Scarlet Ninjini"},
    {{110, 0x0000}, "Bouncer"},
    {{110, 0x1603}, "Legendary Bouncer"},
    {{111, 0x0000}, "Sprocket"},
    {{111, 0x2805}, "Heavy Duty Sprocket"},
    {{112, 0x0000}, "Tree Rex"},
    {{112, 0x1602}, "Gnarly Tree Rex"},
    {{113, 0x0000}, "Shroomboom"},
    {{113, 0x3805}, "Sure Shot Shroomboom"},
    {{113, 0x1206}, "LightCore Shroomboom"},
    {{114, 0x0000}, "Eye Brawl"},
    {{115, 0x0000}, "Fright Rider"},
    {{200, 0x0000}, "Anvil Rain"},
    {{201, 0x0000}, "Hidden Treasure"},
    {{201, 0x2000}, "Platinum Hidden Treasure"},
    {{202, 0x0000}, "Healing Elixir"},
    {{203, 0x0000}, "Ghost Pirate Swords"},
    {{204, 0x0000}, "Time Twist Hourglass"},
    {{205, 0x0000}, "Sky Iron Shield"},
    {{206, 0x0000}, "Winged Boots"},
    {{207, 0x0000}, "Sparx the Dragonfly"},
    {{208, 0x0000}, "Dragonfire Cannon"},
    {{208, 0x1602}, "Golden Dragonfire Cannon"},
    {{209, 0x0000}, "Scorpion Striker"},
    {{210, 0x3002}, "Biter's Bane"},
    {{210, 0x3008}, "Sorcerous Skull"},
    {{210, 0x300B}, "Axe of Illusion"},
    {{210, 0x300E}, "Arcane Hourglass"},
    {{210, 0x3012}, "Spell Slapper"},
    {{210, 0x3014}, "Rune Rocket"},
    {{211, 0x3001}, "Tidal Tiki"},
    {{211, 0x3002}, "Wet Walter"},
    {{211, 0x3006}, "Flood Flask"},
    {{211, 0x3406}, "Legendary Flood Flask"},
    {{211, 0x3007}, "Soaking Staff"},
    {{211, 0x300B}, "Aqua Axe"},
    {{211, 0x3016}, "Frost Helm"},
    {{212, 0x3003}, "Breezy Bird"},
    {{212, 0x3006}, "Drafty Decanter"},
    {{212, 0x300D}, "Tempest Timer"},
    {{212, 0x3010}, "Cloudy Cobra"},
    {{212, 0x3011}, "Storm Warning"},
    {{212, 0x3018}, "Cyclone Saber"},
    {{213, 0x3004}, "Spirit Sphere"},
    {{213, 0x3404}, "Legendary Spirit Sphere"},
    {{213, 0x3008}, "Spectral Skull"},
    {{213, 0x3408}, "Legendary Spectral Skull"},
    {{213, 0x300B}, "Haunted Hatchet"},
    {{213, 0x300C}, "Grim Gripper"},
    {{213, 0x3010}, "Spooky Snake"},
    {{213, 0x3017}, "Dream Piercer"},
    {{214, 0x3000}, "Tech Totem"},
    {{214, 0x3007}, "Automatic Angel"},
    {{214, 0x3009}, "Factory Flower"},
    {{214, 0x300C}, "Grabbing Gadget"},
    {{214, 0x3016}, "Makers Mana"},
    {{214, 0x301A}, "Topsy Techy"},
    {{215, 0x3005}, "Eternal Flame"},
    {{215, 0x3009}, "Fire Flower"},
    {{215, 0x3011}, "Scorching Stopper"},
    {{215, 0x3012}, "Searing Spinner"},
    {{215, 0x3017}, "Spark Spear"},
    {{215, 0x301B}, "Blazing Belch"},
    {{216, 0x3000}, "Banded Boulder"},
    {{216, 0x3003}, "Rock Hawk"},
    {{216, 0x300A}, "Slag Hammer"},
    {{216, 0x300E}, "Dust Of Time"},
    {{216, 0x3013}, "Spinning Sandstorm"},
    {{216, 0x301A}, "Rubble Trouble"},
    {{217, 0x3003}, "Oak Eagle"},
    {{217, 0x3005}, "Emerald Energy"},
    {{217, 0x300A}, "Weed Whacker"},
    {{217, 0x3010}, "Seed Serpent"},
    {{217, 0x3018}, "Jade Blade"},
    {{217, 0x301B}, "Shrub Shrieker"},
    {{218, 0x3000}, "Dark Dagger"},
    {{218, 0x3014}, "Shadow Spider"},
    {{218, 0x301A}, "Ghastly Grimace"},
    {{219, 0x3000}, "Shining Ship"},
    {{219, 0x300F}, "Heavenly Hawk"},
    {{219, 0x301B}, "Beam Scream"},
    {{220, 0x301E}, "Kaos Trap"},
    {{220, 0x351F}, "Ultimate Kaos Trap"},
    {{230, 0x0000}, "Hand of Fate"},
    {{230, 0x3403}, "Legendary Hand of Fate"},
    {{231, 0x0000}, "Piggy Bank"},
    {{232, 0x0000}, "Rocket Ram"},
    {{233, 0x0000}, "Tiki Speaky"},
    {{300, 0x0000}, "Dragon’s Peak"},
    {{301, 0x0000}, "Empire of Ice"},
    {{302, 0x0000}, "Pirate Seas"},
    {{303, 0x0000}, "Darklight Crypt"},
    {{304, 0x0000}, "Volcanic Vault"},
    {{305, 0x0000}, "Mirror of Mystery"},
    {{306, 0x0000}, "Nightmare Express"},
    {{307, 0x0000}, "Sunscraper Spire"},
    {{308, 0x0000}, "Midnight Museum"},
    {{404, 0x0000}, "Legendary Bash"},
    {{416, 0x0000}, "Legendary Spyro"},
    {{419, 0x0000}, "Legendary Trigger Happy"},
    {{430, 0x0000}, "Legendary Chop Chop"},
    {{450, 0x0000}, "Gusto"},
    {{451, 0x0000}, "Thunderbolt"},
    {{452, 0x0000}, "Fling Kong"},
    {{453, 0x0000}, "Blades"},
    {{453, 0x3403}, "Legendary Blades"},
    {{454, 0x0000}, "Wallop"},
    {{455, 0x0000}, "Head Rush"},
    {{455, 0x3402}, "Nitro Head Rush"},
    {{456, 0x0000}, "Fist Bump"},
    {{457, 0x0000}, "Rocky Roll"},
    {{458, 0x0000}, "Wildfire"},
    {{458, 0x3402}, "Dark Wildfire"},
    {{459, 0x0000}, "Ka Boom"},
    {{460, 0x0000}, "Trail Blazer"},
    {{461, 0x0000}, "Torch"},
    {{462, 0x0000}, "Snap Shot"},
    {{462, 0x3402}, "Dark Snap Shot"},
    {{463, 0x0000}, "Lob Star"},
    {{463, 0x3402}, "Winterfest Lob-Star"},
    {{464, 0x0000}, "Flip Wreck"},
    {{465, 0x0000}, "Echo"},
    {{466, 0x0000}, "Blastermind"},
    {{467, 0x0000}, "Enigma"},
    {{468, 0x0000}, "Deja Vu"},
    {{468, 0x3403}, "Legendary Deja Vu"},
    {{469, 0x0000}, "Cobra Candabra"},
    {{469, 0x3402}, "King Cobra Cadabra"},
    {{470, 0x0000}, "Jawbreaker"},
    {{470, 0x3403}, "Legendary Jawbreaker"},
    {{471, 0x0000}, "Gearshift"},
    {{472, 0x0000}, "Chopper"},
    {{473, 0x0000}, "Tread Head"},
    {{474, 0x0000}, "Bushwack"},
    {{474, 0x3403}, "Legendary Bushwack"},
    {{475, 0x0000}, "Tuff Luck"},
    {{476, 0x0000}, "Food Fight"},
    {{476, 0x3402}, "Dark Food Fight"},
    {{477, 0x0000}, "High Five"},
    {{478, 0x0000}, "Krypt King"},
    {{478, 0x3402}, "Nitro Krypt King"},
    {{479, 0x0000}, "Short Cut"},
    {{480, 0x0000}, "Bat Spin"},
    {{481, 0x0000}, "Funny Bone"},
    {{482, 0x0000}, "Knight Light"},
    {{483, 0x0000}, "Spotlight"},
    {{484, 0x0000}, "Knight Mare"},
    {{485, 0x0000}, "Blackout"},
    {{502, 0x0000}, "Bop"},
    {{505, 0x0000}, "Terrabite"},
    {{506, 0x0000}, "Breeze"},
    {{508, 0x0000}, "Pet Vac"},
    {{508, 0x3402}, "Power Punch Pet Vac"},
    {{507, 0x0000}, "Weeruptor"},
    {{507, 0x3402}, "Eggcellent Weeruptor"},
    {{509, 0x0000}, "Small Fry"},
    {{510, 0x0000}, "Drobit"},
    {{519, 0x0000}, "Trigger Snappy"},
    {{526, 0x0000}, "Whisper Elf"},
    {{540, 0x0000}, "Barkley"},
    {{540, 0x3402}, "Gnarly Barkley"},
    {{541, 0x0000}, "Thumpling"},
    {{514, 0x0000}, "Gill Runt"},
    {{542, 0x0000}, "Mini-Jini"},
    {{503, 0x0000}, "Spry"},
    {{504, 0x0000}, "Hijinx"},
    {{543, 0x0000}, "Eye Small"},
    {{1000, 0x0000}, "Boom Jet (Bottom)"},
    {{1001, 0x0000}, "Free Ranger (Bottom)"},
    {{1001, 0x2403}, "Legendary Free Ranger (Bottom)"},
    {{1002, 0x0000}, "Rubble Rouser (Bottom)"},
    {{1003, 0x0000}, "Doom Stone (Bottom)"},
    {{1004, 0x0000}, "Blast Zone (Bottom)"},
    {{1004, 0x2402}, "Dark Blast Zone (Bottom)"},
    {{1005, 0x0000}, "Fire Kraken (Bottom)"},
    {{1005, 0x2402}, "Jade Fire Kraken (Bottom)"},
    {{1006, 0x0000}, "Stink Bomb (Bottom)"},
    {{1007, 0x0000}, "Grilla Drilla (Bottom)"},
    {{1008, 0x0000}, "Hoot Loop (Bottom)"},
    {{1008, 0x2402}, "Enchanted Hoot Loop (Bottom)"},
    {{1009, 0x0000}, "Trap Shadow (Bottom)"},
    {{1010, 0x0000}, "Magna Charge (Bottom)"},
    {{1010, 0x2402}, "Nitro Magna Charge (Bottom)"},
    {{1011, 0x0000}, "Spy Rise (Bottom)"},
    {{1012, 0x0000}, "Night Shift (Bottom)"},
    {{1012, 0x2403}, "Legendary Night Shift (Bottom)"},
    {{1013, 0x0000}, "Rattle Shake (Bottom)"},
    {{1013, 0x2402}, "Quick Draw Rattle Shake (Bottom)"},
    {{1014, 0x0000}, "Freeze Blade (Bottom)"},
    {{1014, 0x2402}, "Nitro Freeze Blade (Bottom)"},
    {{1015, 0x0000}, "Wash Buckler (Bottom)"},
    {{1015, 0x2402}, "Dark Wash Buckler (Bottom)"},
    {{2000, 0x0000}, "Boom Jet (Top)"},
    {{2001, 0x0000}, "Free Ranger (Top)"},
    {{2001, 0x2403}, "Legendary Free Ranger (Top)"},
    {{2002, 0x0000}, "Rubble Rouser (Top)"},
    {{2003, 0x0000}, "Doom Stone (Top)"},
    {{2004, 0x0000}, "Blast Zone (Top)"},
    {{2004, 0x2402}, "Dark Blast Zone (Top)"},
    {{2005, 0x0000}, "Fire Kraken (Top)"},
    {{2005, 0x2402}, "Jade Fire Kraken (Top)"},
    {{2006, 0x0000}, "Stink Bomb (Top)"},
    {{2007, 0x0000}, "Grilla Drilla (Top)"},
    {{2008, 0x0000}, "Hoot Loop (Top)"},
    {{2008, 0x2402}, "Enchanted Hoot Loop (Top)"},
    {{2009, 0x0000}, "Trap Shadow (Top)"},
    {{2010, 0x0000}, "Magna Charge (Top)"},
    {{2010, 0x2402}, "Nitro Magna Charge (Top)"},
    {{2011, 0x0000}, "Spy Rise (Top)"},
    {{2012, 0x0000}, "Night Shift (Top)"},
    {{2012, 0x2403}, "Legendary Night Shift (Top)"},
    {{2013, 0x0000}, "Rattle Shake (Top)"},
    {{2013, 0x2402}, "Quick Draw Rattle Shake (Top)"},
    {{2014, 0x0000}, "Freeze Blade (Top)"},
    {{2014, 0x2402}, "Nitro Freeze Blade (Top)"},
    {{2015, 0x0000}, "Wash Buckler (Top)"},
    {{2015, 0x2402}, "Dark Wash Buckler (Top)"},
    {{3000, 0x0000}, "Scratch"},
    {{3001, 0x0000}, "Pop Thorn"},
    {{3002, 0x0000}, "Slobber Tooth"},
    {{3002, 0x2402}, "Dark Slobber Tooth"},
    {{3003, 0x0000}, "Scorp"},
    {{3004, 0x0000}, "Fryno"},
    {{3004, 0x3805}, "Hog Wild Fryno"},
    {{3005, 0x0000}, "Smolderdash"},
    {{3005, 0x2206}, "LightCore Smolderdash"},
    {{3006, 0x0000}, "Bumble Blast"},
    {{3006, 0x2402}, "Jolly Bumble Blast"},
    {{3006, 0x2206}, "LightCore Bumble Blast"},
    {{3007, 0x0000}, "Zoo Lou"},
    {{3007, 0x2403}, "Legendary Zoo Lou"},
    {{3008, 0x0000}, "Dune Bug"},
    {{3009, 0x0000}, "Star Strike"},
    {{3009, 0x2602}, "Enchanted Star Strike"},
    {{3009, 0x2206}, "LightCore Star Strike"},
    {{3010, 0x0000}, "Countdown"},
    {{3010, 0x2402}, "Kickoff Countdown"},
    {{3010, 0x2206}, "LightCore Countdown"},
    {{3011, 0x0000}, "Wind Up"},
    {{3011, 0x2404}, "Gear Head VVind Up"},
    {{3012, 0x0000}, "Roller Brawl"},
    {{3013, 0x0000}, "Grim Creeper"},
    {{3013, 0x2603}, "Legendary Grim Creeper"},
    {{3013, 0x2206}, "LightCore Grim Creeper"},
    {{3014, 0x0000}, "Rip Tide"},
    {{3015, 0x0000}, "Punk Shock"},
    {{3200, 0x0000}, "Battle Hammer"},
    {{3201, 0x0000}, "Sky Diamond"},
    {{3202, 0x0000}, "Platinum Sheep"},
    {{3203, 0x0000}, "Groove Machine"},
    {{3204, 0x0000}, "UFO Hat"},
    {{3300, 0x0000}, "Sheep Wreck Island"},
    {{3301, 0x0000}, "Tower of Time"},
    {{3302, 0x0000}, "Fiery Forge"},
    {{3303, 0x0000}, "Arkeyan Crossbow"},
    {{3220, 0x0000}, "Jet Stream"},
    {{3221, 0x0000}, "Tomb Buggy"},
    {{3222, 0x0000}, "Reef Ripper"},
    {{3223, 0x0000}, "Burn Cycle"},
    {{3224, 0x0000}, "Hot Streak"},
    {{3224, 0x4402}, "Dark Hot Streak"},
    {{3224, 0x4004}, "E3 Hot Streak"},
    {{3224, 0x441E}, "Golden Hot Streak"},
    {{3225, 0x0000}, "Shark Tank"},
    {{3226, 0x0000}, "Thump Truck"},
    {{3227, 0x0000}, "Crypt Crusher"},
    {{3228, 0x0000}, "Stealth Stinger"},
    {{3228, 0x4402}, "Nitro Stealth Stinger"},
    {{3231, 0x0000}, "Dive Bomber"},
    {{3231, 0x4402}, "Spring Ahead Dive Bomber"},
    {{3232, 0x0000}, "Sky Slicer"},
    {{3233, 0x0000}, "Clown Cruiser (Nintendo Only)"},
    {{3233, 0x4402}, "Dark Clown Cruiser (Nintendo Only)"},
    {{3234, 0x0000}, "Gold Rusher"},
    {{3234, 0x4402}, "Power Blue Gold Rusher"},
    {{3235, 0x0000}, "Shield Striker"},
    {{3236, 0x0000}, "Sun Runner"},
    {{3236, 0x4403}, "Legendary Sun Runner"},
    {{3237, 0x0000}, "Sea Shadow"},
    {{3237, 0x4402}, "Dark Sea Shadow"},
    {{3238, 0x0000}, "Splatter Splasher"},
    {{3238, 0x4402}, "Power Blue Splatter Splasher"},
    {{3239, 0x0000}, "Soda Skimmer"},
    {{3240, 0x0000}, "Barrel Blaster (Nintendo Only)"},
    {{3240, 0x4402}, "Dark Barrel Blaster (Nintendo Only)"},
    {{3239, 0x4402}, "Nitro Soda Skimmer"},
    {{3241, 0x0000}, "Buzz Wing"},
    {{3400, 0x0000}, "Fiesta"},
    {{3400, 0x4515}, "Frightful Fiesta"},
    {{3401, 0x0000}, "High Volt"},
    {{3402, 0x0000}, "Splat"},
    {{3402, 0x4502}, "Power Blue Splat"},
    {{3406, 0x0000}, "Stormblade"},
    {{3411, 0x0000}, "Smash Hit"},
    {{3411, 0x4502}, "Steel Plated Smash Hit"},
    {{3412, 0x0000}, "Spitfire"},
    {{3412, 0x4502}, "Dark Spitfire"},
    {{3413, 0x0000}, "Hurricane Jet Vac"},
    {{3413, 0x4503}, "Legendary Hurricane Jet Vac"},
    {{3414, 0x0000}, "Double Dare Trigger Happy"},
    {{3414, 0x4502}, "Power Blue Double Dare Trigger Happy"},
    {{3415, 0x0000}, "Super Shot Stealth Elf"},
    {{3415, 0x4502}, "Dark Super Shot Stealth Elf"},
    {{3416, 0x0000}, "Shark Shooter Terrafin"},
    {{3417, 0x0000}, "Bone Bash Roller Brawl"},
    {{3417, 0x4503}, "Legendary Bone Bash Roller Brawl"},
    {{3420, 0x0000}, "Big Bubble Pop Fizz"},
    {{3420, 0x450E}, "Birthday Bash Big Bubble Pop Fizz"},
    {{3421, 0x0000}, "Lava Lance Eruptor"},
    {{3422, 0x0000}, "Deep Dive Gill Grunt"},
    {{3423, 0x0000}, "Turbo Charge Donkey Kong (Nintendo Only)"},
    {{3423, 0x4502}, "Dark Turbo Charge Donkey Kong (Nintendo Only)"},
    {{3424, 0x0000}, "Hammer Slam Bowser (Nintendo Only)"},
    {{3424, 0x4502}, "Dark Hammer Slam Bowser (Nintendo Only)"},
    {{3425, 0x0000}, "Dive-Clops"},
    {{3425, 0x450E}, "Missile-Tow Dive-Clops"},
    {{3426, 0x0000}, "Astroblast"},
    {{3426, 0x4503}, "Legendary Astroblast"},
    {{3427, 0x0000}, "Nightfall"},
    {{3428, 0x0000}, "Thrillipede"},
    {{3428, 0x450D}, "Eggcited Thrillipede"},
    {{3500, 0x0000}, "Sky Trophy"},
    {{3501, 0x0000}, "Land Trophy"},
    {{3502, 0x0000}, "Sea Trophy"},
    {{3503, 0x0000}, "Kaos Trophy"},
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
      // 0x01: left and right
      // 0x02: left

      // The 3rd, 4th and 5th bytes are red, green and blue

      // The 6th byte is unknown. Observed values are 0x00, 0x0D and 0xF4

      // The 7th byte is the fade duration. Exact value-time corrolation unknown. Observed values
      // are 0x00, 0x01 and 0x07. Custom commands show that the higher this value the longer the
      // duration.

      // Empty J response is sent after the fade is completed.
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

void Skylander::Save()
{
  if (!sky_file)
    return;

  sky_file.Seek(0, File::SeekOrigin::Begin);
  sky_file.WriteBytes(data.data(), 0x40 * 0x10);
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
  if (skylander.status & 1)
  {
    reply_buf[1] = (0x10 | sky_num);
    memcpy(reply_buf + 3, skylander.data.data() + (16 * block), 16);
  }
  else
  {
    reply_buf[1] = sky_num;
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
    memcpy(skylander.data.data() + (block * 16), to_write_buf, 16);
    skylander.Save();
  }
  else
  {
    reply_buf[1] = sky_num;
  }
}

static u16 SkylanderCRC16(u16 init_value, const u8* buffer, u32 size)
{
  static constexpr std::array<u16, 256> CRC_CCITT_TABLE{
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A,
      0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294,
      0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462,
      0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509,
      0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695,
      0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5,
      0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948,
      0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
      0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
      0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B,
      0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F,
      0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB,
      0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046,
      0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
      0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E,
      0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
      0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691,
      0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
      0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D,
      0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16,
      0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8,
      0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E,
      0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93,
      0x3EB2, 0x0ED1, 0x1EF0};

  u16 crc = init_value;

  for (u32 i = 0; i < size; i++)
  {
    const u16 tmp = (crc >> 8) ^ buffer[i];
    crc = (crc << 8) ^ CRC_CCITT_TABLE[tmp];
  }

  return crc;
}

bool SkylanderPortal::CreateSkylander(const std::string& file_path, u16 sky_id, u16 sky_var)
{
  File::IOFile sky_file(file_path, "w+b");
  if (!sky_file)
  {
    return false;
  }

  std::array<u8, 0x40 * 0x10> buf{};
  const auto file_data = buf.data();
  // Set the block permissions
  u32 first_block = 0x690F0F0F;
  u32 other_blocks = 0x69080F7F;
  memcpy(&file_data[0x36], &first_block, sizeof(first_block));
  for (u32 index = 1; index < 0x10; index++)
  {
    memcpy(&file_data[(index * 0x40) + 0x36], &other_blocks, sizeof(other_blocks));
  }

  // Set the NUID of the figure
  Common::Random::Generate(&file_data[0], 4);

  // The BCC (Block Check Character)
  file_data[4] = file_data[0] ^ file_data[1] ^ file_data[2] ^ file_data[3];

  // ATQA
  file_data[5] = 0x81;
  file_data[6] = 0x01;

  // SAK
  file_data[7] = 0x0F;

  // Set the skylander info
  memcpy(&file_data[0x10], &sky_id, sizeof(sky_id));
  memcpy(&file_data[0x1C], &sky_var, sizeof(sky_var));

  // Set checksum
  u16 checksum = SkylanderCRC16(0xFFFF, file_data, 0x1E);
  memcpy(&file_data[0x1E], &checksum, sizeof(checksum));

  sky_file.WriteBytes(buf.data(), buf.size());
  return true;
}

bool SkylanderPortal::RemoveSkylander(u8 sky_num)
{
  if (!IsSkylanderNumberValid(sky_num))
    return false;

  DEBUG_LOG_FMT(IOS_USB, "Cleared Skylander from slot {}", sky_num);
  std::lock_guard lock(sky_mutex);
  auto& skylander = skylanders[sky_num];

  if (skylander.sky_file.IsOpen())
  {
    skylander.Save();
    skylander.sky_file.Close();
  }

  if (skylander.status & 1)
  {
    skylander.status = Skylander::REMOVING;
    skylander.queued_status.push(Skylander::REMOVING);
    skylander.queued_status.push(Skylander::REMOVED);
    return true;
  }

  return false;
}

u8 SkylanderPortal::LoadSkylander(u8* buf, File::IOFile in_file)
{
  std::lock_guard lock(sky_mutex);

  u32 sky_serial = 0;
  for (int i = 3; i > -1; i--)
  {
    sky_serial <<= 8;
    sky_serial |= buf[i];
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
    memcpy(skylander.data.data(), buf, skylander.data.size());
    DEBUG_LOG_FMT(IOS_USB, "Skylander Data: \n{}",
                  HexDump(skylander.data.data(), skylander.data.size()));
    skylander.sky_file = std::move(in_file);
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
  u16 sky_id = file_data[0x11];
  u16 sky_var = file_data[0x1D];
  sky_id <<= 8;
  sky_var <<= 8;
  sky_id |= file_data[0x10];
  sky_var |= file_data[0x1C];
  return std::make_pair(sky_id, sky_var);
}

}  // namespace IOS::HLE::USB
