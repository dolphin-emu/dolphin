// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// DolphinQt code copied and modified for Dolphin from the RPCS3 Qt utility for Creating, Loading
// and Clearing Skylanders

#include "DolphinQt/SkylanderPortal/SkylanderPortalWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include "Common/IOFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

// Qt is not guaranteed to keep track of file paths using native file pickers, so we use this
// static variable to ensure we open at the most recent Skylander file location
static QString s_last_skylander_path;

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
    {{601, 0x0000}, "King Pen"},
    {{602, 0x0000}, "Tri-Tip"},
    {{603, 0x0000}, "Chopscotch"},
    {{604, 0x0000}, "Boom Bloom"},
    {{605, 0x0000}, "Pit Boss"},
    {{606, 0x0000}, "Barbella"},
    {{607, 0x0000}, "Air Strike"},
    {{608, 0x0000}, "Ember"},
    {{609, 0x0000}, "Ambush"},
    {{610, 0x0000}, "Dr. Krankcase"},
    {{611, 0x0000}, "Hood Sickle"},
    {{612, 0x0000}, "Tae Kwon Crow"},
    {{613, 0x0000}, "Golden Queen"},
    {{614, 0x0000}, "Wolfgang"},
    {{615, 0x0000}, "Pain-Yatta"},
    {{616, 0x0000}, "Mysticat"},
    {{617, 0x0000}, "Starcast"},
    {{618, 0x0000}, "Buckshot"},
    {{619, 0x0000}, "Aurora"},
    {{620, 0x0000}, "Flare Wolf"},
    {{621, 0x0000}, "Chompy Mage"},
    {{622, 0x0000}, "Bad Juju"},
    {{623, 0x0000}, "Grave Clobber"},
    {{624, 0x0000}, "Blaster-Tron"},
    {{625, 0x0000}, "Ro-Bow"},
    {{626, 0x0000}, "Chain Reaction"},
    {{627, 0x0000}, "Kaos"},
    {{628, 0x0000}, "Wild Storm"},
    {{629, 0x0000}, "Tidepool"},
    {{630, 0x0000}, "Crash Bandicoot"},
    {{631, 0x0000}, "Dr. Neo Cortex"},
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

SkylanderPortalWindow::SkylanderPortalWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Skylanders Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QString::fromStdString("skylanders_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &SkylanderPortalWindow::OnEmulationStateChanged);

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState());
};

SkylanderPortalWindow::~SkylanderPortalWindow() = default;

void SkylanderPortalWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_checkbox = new QCheckBox(tr("Emulate Skylander Portal"), this);
  m_checkbox->setChecked(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  connect(m_checkbox, &QCheckBox::toggled, [=](bool checked) { EmulatePortal(checked); });
  checkbox_layout->addWidget(m_checkbox);
  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);

  auto add_line = [](QVBoxLayout* vbox) {
    auto* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);
  };

  m_group_skylanders = new QGroupBox(tr("Active Portal Skylanders:"));
  auto* vbox_group = new QVBoxLayout();
  auto* scroll_area = new QScrollArea();

  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    if (i != 0)
    {
      add_line(vbox_group);
    }

    auto* hbox_skylander = new QHBoxLayout();
    auto* label_skyname = new QLabel(QString(tr("Skylander %1")).arg(i + 1));
    m_edit_skylanders[i] = new QLineEdit();
    m_edit_skylanders[i]->setEnabled(false);

    auto* clear_btn = new QPushButton(tr("Clear"));
    auto* create_btn = new QPushButton(tr("Create"));
    auto* load_btn = new QPushButton(tr("Load"));

    connect(clear_btn, &QAbstractButton::clicked, this, [this, i]() { ClearSkylander(i); });
    connect(create_btn, &QAbstractButton::clicked, this, [this, i]() { CreateSkylander(i); });
    connect(load_btn, &QAbstractButton::clicked, this, [this, i]() { LoadSkylander(i); });

    hbox_skylander->addWidget(label_skyname);
    hbox_skylander->addWidget(m_edit_skylanders[i]);
    hbox_skylander->addWidget(clear_btn);
    hbox_skylander->addWidget(create_btn);
    hbox_skylander->addWidget(load_btn);

    vbox_group->addLayout(hbox_skylander);
  }

  m_group_skylanders->setLayout(vbox_group);
  scroll_area->setWidget(m_group_skylanders);
  scroll_area->setWidgetResizable(true);
  m_group_skylanders->setVisible(Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL));
  main_layout->addWidget(scroll_area);
  setLayout(main_layout);

  UpdateEdits();
}

void SkylanderPortalWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox->setEnabled(!running);
}

CreateSkylanderDialog::CreateSkylanderDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Skylander Creator"));
  setObjectName(QString::fromStdString("skylanders_creator"));
  setMinimumSize(QSize(500, 150));
  auto* layout = new QVBoxLayout;

  auto* combo_skylist = new QComboBox();
  QStringList filterlist;
  for (const auto& entry : list_skylanders)
  {
    const uint qvar = (entry.first.first << 16) | entry.first.second;
    combo_skylist->addItem(QString::fromStdString(entry.second), QVariant(qvar));
    filterlist << QString::fromStdString(entry.second);
  }
  combo_skylist->addItem(tr("--Unknown--"), QVariant(0xFFFFFFFF));
  combo_skylist->setEditable(true);
  combo_skylist->setInsertPolicy(QComboBox::NoInsert);

  auto* co_compl = new QCompleter(filterlist, this);
  co_compl->setCaseSensitivity(Qt::CaseInsensitive);
  co_compl->setCompletionMode(QCompleter::PopupCompletion);
  co_compl->setFilterMode(Qt::MatchContains);
  combo_skylist->setCompleter(co_compl);

  layout->addWidget(combo_skylist);

  auto* line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addWidget(line);

  auto* hbox_idvar = new QHBoxLayout();
  auto* label_id = new QLabel(tr("ID:"));
  auto* label_var = new QLabel(tr("Variant:"));
  auto* edit_id = new QLineEdit(tr("0"));
  auto* edit_var = new QLineEdit(tr("0"));
  auto* rxv =
      new QRegularExpressionValidator(QRegularExpression(QString::fromStdString("\\d*")), this);
  edit_id->setValidator(rxv);
  edit_var->setValidator(rxv);
  hbox_idvar->addWidget(label_id);
  hbox_idvar->addWidget(edit_id);
  hbox_idvar->addWidget(label_var);
  hbox_idvar->addWidget(edit_var);
  layout->addLayout(hbox_idvar);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
  layout->addWidget(buttons);

  setLayout(layout);

  connect(combo_skylist, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
    const u32 sky_info = combo_skylist->itemData(index).toUInt();
    if (sky_info != 0xFFFFFFFF)
    {
      const u16 sky_id = sky_info >> 16;
      const u16 sky_var = sky_info & 0xFFFF;

      edit_id->setText(QString::number(sky_id));
      edit_var->setText(QString::number(sky_var));
    }
  });

  connect(buttons, &QDialogButtonBox::accepted, this, [=, this]() {
    bool ok_id = false, ok_var = false;
    const u16 sky_id = edit_id->text().toUShort(&ok_id);
    if (!ok_id)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("ID entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }
    const u16 sky_var = edit_var->text().toUShort(&ok_var);
    if (!ok_var)
    {
      QMessageBox::warning(this, tr("Error converting value"), tr("Variant entered is invalid!"),
                           QMessageBox::Ok);
      return;
    }

    QString predef_name = s_last_skylander_path;
    const auto found_sky = list_skylanders.find(std::make_pair(sky_id, sky_var));
    if (found_sky != list_skylanders.end())
    {
      predef_name += QString::fromStdString(std::string(found_sky->second) + ".sky");
    }
    else
    {
      QString str = tr("Unknown(%1 %2).sky");
      predef_name += str.arg(sky_id, sky_var);
    }

    m_file_path = DolphinFileDialog::getSaveFileName(this, tr("Create Skylander File"), predef_name,
                                                     tr("Skylander Object (*.sky);;"));
    if (m_file_path.isEmpty())
    {
      return;
    }

    auto& system = Core::System::GetInstance();

    if (!system.GetSkylanderPortal().CreateSkylander(m_file_path.toStdString(), sky_id, sky_var))
    {
      QMessageBox::warning(this, tr("Failed to create Skylander file!"),
                           tr("Failed to create Skylander file:\n%1").arg(m_file_path),
                           QMessageBox::Ok);
      return;
    }
    s_last_skylander_path = QFileInfo(m_file_path).absolutePath() + QString::fromStdString("/");
    accept();
  });

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(co_compl, QOverload<const QString&>::of(&QCompleter::activated),
          [=](const QString& text) {
            combo_skylist->setCurrentText(text);
            combo_skylist->setCurrentIndex(combo_skylist->findText(text));
          });
}

QString CreateSkylanderDialog::GetFilePath() const
{
  return m_file_path;
}

void SkylanderPortalWindow::EmulatePortal(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_SKYLANDER_PORTAL, emulate);
  m_group_skylanders->setVisible(emulate);
}

void SkylanderPortalWindow::CreateSkylander(u8 slot)
{
  CreateSkylanderDialog create_dlg(this);
  if (create_dlg.exec() == CreateSkylanderDialog::Accepted)
  {
    LoadSkylanderPath(slot, create_dlg.GetFilePath());
  }
}

void SkylanderPortalWindow::LoadSkylander(u8 slot)
{
  const QString file_path =
      DolphinFileDialog::getOpenFileName(this, tr("Select Skylander File"), s_last_skylander_path,
                                         QString::fromStdString("Skylander (*.sky);;"));
  if (file_path.isEmpty())
  {
    return;
  }
  s_last_skylander_path = QFileInfo(file_path).absolutePath() + QString::fromStdString("/");

  LoadSkylanderPath(slot, file_path);
}

void SkylanderPortalWindow::LoadSkylanderPath(u8 slot, const QString& path)
{
  File::IOFile sky_file(path.toStdString(), "r+b");
  if (!sky_file)
  {
    QMessageBox::warning(
        this, tr("Failed to open the Skylander file!"),
        tr("Failed to open the Skylander file(%1)!\nFile may already be in use on the portal.")
            .arg(path),
        QMessageBox::Ok);
    return;
  }
  std::array<u8, 0x40 * 0x10> file_data;
  if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
  {
    QMessageBox::warning(
        this, tr("Failed to read the Skylander file!"),
        tr("Failed to read the Skylander file(%1)!\nFile was too small.").arg(path),
        QMessageBox::Ok);
    return;
  }

  ClearSkylander(slot);

  u16 sky_id = file_data[0x11];
  u16 sky_var = file_data[0x1D];
  sky_id <<= 8;
  sky_var <<= 8;
  sky_id |= file_data[0x10];
  sky_var |= file_data[0x1C];

  DEBUG_LOG_FMT(IOS_USB, "Sky Id: {}, 0x10: {} 0x11: {}", sky_id, file_data[0x10], file_data[0x11]);
  DEBUG_LOG_FMT(IOS_USB, "Sky Var: {}, 0x1D: {} 0x1C: {}", sky_var, file_data[0x1D],
                file_data[0x1C]);

  auto& system = Core::System::GetInstance();

  u8 portal_slot = system.GetSkylanderPortal().LoadSkylander(file_data.data(), std::move(sky_file));
  if (portal_slot == 0xFF)
  {
    QMessageBox::warning(this, tr("Failed to load the Skylander file!"),
                         tr("Failed to load the Skylander file(%1)!\n").arg(path), QMessageBox::Ok);
    return;
  }
  m_sky_slots[slot] = {portal_slot, sky_id, sky_var};
  UpdateEdits();
}

void SkylanderPortalWindow::ClearSkylander(u8 slot)
{
  auto& system = Core::System::GetInstance();
  if (auto slot_infos = m_sky_slots[slot])
  {
    if (!system.GetSkylanderPortal().RemoveSkylander(slot_infos->portal_slot))
    {
      QMessageBox::warning(this, tr("Failed to clear Skylander!"),
                           tr("Failed to clear the Skylander from slot(%1)!\n").arg(slot),
                           QMessageBox::Ok);
      return;
    }
    system.GetSkylanderPortal().RemoveSkylander(slot_infos->portal_slot);
    m_sky_slots[slot].reset();
    UpdateEdits();
  }
}

void SkylanderPortalWindow::UpdateEdits()
{
  for (auto i = 0; i < MAX_SKYLANDERS; i++)
  {
    QString display_string;
    if (auto sd = m_sky_slots[i])
    {
      auto found_sky = list_skylanders.find(std::make_pair(sd->sky_id, sd->sky_var));
      if (found_sky != list_skylanders.end())
      {
        display_string = QString::fromStdString(found_sky->second);
      }
      else
      {
        display_string = QString(tr("Unknown (Id:%1 Var:%2)")).arg(sd->sky_id).arg(sd->sky_var);
      }
    }
    else
    {
      display_string = tr("None");
    }

    m_edit_skylanders[i]->setText(display_string);
  }
}

bool SkylanderPortalWindow::eventFilter(QObject* object, QEvent* event)
{
  // Close when escape is pressed
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}

void SkylanderPortalWindow::closeEvent(QCloseEvent* event)
{
  hide();
}
