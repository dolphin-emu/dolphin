// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Infinity.h"

#include <array>
#include <bit>
#include <map>
#include <mutex>
#include <vector>

#include "Common/Crypto/SHA1.h"
#include "Common/Logging/Log.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
const std::map<const char*, const InfinityFigureInfo> list_infinity_figures = {
    {"The Incredibles - Mr. Incredible", {InfinityFigureType::Character, 0x4241}},
    {"Monsters University - Sulley", {InfinityFigureType::Character, 0x4242}},
    {"Pirates of the Caribbean - Jack Sparrow", {InfinityFigureType::Character, 0x4243}},
    {"The Lone Ranger - The Lone Ranger", {InfinityFigureType::Character, 0x4244}},
    {"The Lone Ranger - Tonto", {InfinityFigureType::Character, 0x4245}},
    {"Cars - Lightning McQueen", {InfinityFigureType::Character, 0x4246}},
    {"Cars - Holley Shiftwell", {InfinityFigureType::Character, 0x4247}},
    {"Toy Story - Buzz Lightyear", {InfinityFigureType::Character, 0x4248}},
    {"Toy Story - Jessie", {InfinityFigureType::Character, 0x4249}},
    {"Monsters University - Mike Wazowski", {InfinityFigureType::Character, 0x424A}},
    {"The Incredibles - Mrs. Incredible", {InfinityFigureType::Character, 0x424B}},
    {"Pirates of the Caribbean - Barbossa", {InfinityFigureType::Character, 0x424C}},
    {"Pirates of the Caribbean - Davy Jones", {InfinityFigureType::Character, 0x424D}},
    {"Monsters University - Randy", {InfinityFigureType::Character, 0x424E}},
    {"The Incredibles - Syndrome", {InfinityFigureType::Character, 0x424F}},
    {"Toy Story - Woody", {InfinityFigureType::Character, 0x4250}},
    {"Cars - Mater", {InfinityFigureType::Character, 0x4251}},
    {"The Incredibles - Dash", {InfinityFigureType::Character, 0x4252}},
    {"The Incredibles - Violet", {InfinityFigureType::Character, 0x4253}},
    {"Cars - Francesco Bernoulli", {InfinityFigureType::Character, 0x4254}},
    {"Fantasia - Sorcerer's Apprentice Mickey", {InfinityFigureType::Character, 0x4255}},
    {"The Nightmare Before Christmas - Jack Skellington", {InfinityFigureType::Character, 0x4256}},
    {"Tangled - Rapunzel", {InfinityFigureType::Character, 0x4257}},
    {"Frozen - Anna", {InfinityFigureType::Character, 0x4258}},
    {"Frozen - Elsa", {InfinityFigureType::Character, 0x4259}},
    {"Phineas and Ferb - Phineas Flynn", {InfinityFigureType::Character, 0x425A}},
    {"Phineas and Ferb - Agent P", {InfinityFigureType::Character, 0x425B}},
    {"Wreck-It Ralph - Wreck-It Ralph", {InfinityFigureType::Character, 0x425C}},
    {"Wreck-It Ralph - Vanellope von Schweetz", {InfinityFigureType::Character, 0x425D}},
    {"The Incredibles - Mr. Incredible (Crystal)", {InfinityFigureType::Character, 0x425E}},
    {"Pirates of the Caribbean - Jack Sparrow (Crystal)", {InfinityFigureType::Character, 0x425F}},
    {"Monsters University - Sulley (Crystal)", {InfinityFigureType::Character, 0x4260}},
    {"Cars - Lightning McQueen (Crystal)", {InfinityFigureType::Character, 0x4261}},
    {"The Lone Ranger - The Lone Ranger (Crystal)", {InfinityFigureType::Character, 0x4262}},
    {"Toy Story - Buzz Lightyear (Crystal)", {InfinityFigureType::Character, 0x4263}},
    {"Phineas and Ferb - Agent P (Crystal)", {InfinityFigureType::Character, 0x4264}},
    {"Fantasia - Sorcerer's Apprentice Mickey (Crystal)", {InfinityFigureType::Character, 0x4265}},
    {"Toy Story - Buzz Lightyear (Glowing)", {InfinityFigureType::Character, 0x4266}},
    {"The Incredibles - Pirates of the Caribbean - Monsters University Play Set",
     {InfinityFigureType::Playset, 0x8481}},
    {"The Lone Ranger Play Set", {InfinityFigureType::Playset, 0x8482}},
    {"Cars Play Set", {InfinityFigureType::Playset, 0x8483}},
    {"Toy Story in Space Play Set", {InfinityFigureType::Playset, 0x8484}},
    {"Mickey Mouse Universe - Mickey's Car - Toy (Vehicle)", {InfinityFigureType::Item, 0x0912}},
    {"Cinderella - Cinderella's Coach - Toy (Vehicle)", {InfinityFigureType::Item, 0x0913}},
    {"The Muppets - Electric Mayhem Bus - Toy (Vehicle)", {InfinityFigureType::Item, 0x0914}},
    {"101 Dalmatians - Cruella De Vil's Car - Toy (Vehicle)", {InfinityFigureType::Item, 0x0915}},
    {"Toy Story - Pizza Planet Delivery Truck - Toy (Vehicle)", {InfinityFigureType::Item, 0x0916}},
    {"Monsters, Inc. - Mike's New Car - Toy (Vehicle)", {InfinityFigureType::Item, 0x0917}},
    {"Disney Parks - Disney Parks Parking Lot Tram - Toy (Vehicle)",
     {InfinityFigureType::Item, 0x0919}},
    {"Peter Pan, Disney Parks - Jolly Roger - Toy (Aircraft)", {InfinityFigureType::Item, 0x091A}},
    {"Dumbo, Disney Parks - Dumbo the Flying Elephant - Toy (Aircraft)",
     {InfinityFigureType::Item, 0x091B}},
    {"Bolt - Calico Helicopter - Toy (Aircraft)", {InfinityFigureType::Item, 0x091C}},
    {"Tangled - Maximus - Toy (Mount)", {InfinityFigureType::Item, 0x091D}},
    {"Brave - Angus - Toy (Mount)", {InfinityFigureType::Item, 0x091E}},
    {"Aladdin - Abu the Elephant - Toy (Mount)", {InfinityFigureType::Item, 0x091F}},
    {"The Adventures of Ichabod and Mr. Toad - Headless Horseman's Horse - Toy (Mount)",
     {InfinityFigureType::Item, 0x0920}},
    {"Beauty and the Beast - Phillipe - Toy (Mount)", {InfinityFigureType::Item, 0x0921}},
    {"Mulan - Khan - Toy (Mount)", {InfinityFigureType::Item, 0x0922}},
    {"Tarzan - Tantor - Toy (Mount)", {InfinityFigureType::Item, 0x0923}},
    {"Mulan - Dragon Firework Cannon - Toy (Weapon)", {InfinityFigureType::Item, 0x0924}},
    {"Lilo & Stitch - Stitch's Blaster - Toy (Weapon)", {InfinityFigureType::Item, 0x0925}},
    {"Toy Story, Disney Parks - Toy Story Mania Blaster - Toy (Weapon)",
     {InfinityFigureType::Item, 0x0926}},
    {"Alice in Wonderland - Flamingo Croquet Mallet - Toy (Weapon)",
     {InfinityFigureType::Item, 0x0927}},
    {"Up - Carl Fredricksen's Cane - Toy (Weapon)", {InfinityFigureType::Item, 0x0928}},
    {"Lilo & Stitch - Hangin' Ten Stitch With Surfboard - Toy (Hoverboard)",
     {InfinityFigureType::Item, 0x0929}},
    {"Condorman - Condorman Glider - Toy (Glider)", {InfinityFigureType::Item, 0x092A}},
    {"WALL-E - WALL-E's Fire Extinguisher - Toy (Jetpack)", {InfinityFigureType::Item, 0x092B}},
    {"TRON - On the Grid - Customization (Terrain)", {InfinityFigureType::Item, 0x092C}},
    {"WALL-E - WALL-E's Collection - Customization (Terrain)", {InfinityFigureType::Item, 0x092D}},
    {"Wreck-It Ralph - King Candy's Dessert Toppings - Customization (Terrain)",
     {InfinityFigureType::Item, 0x092E}},
    {"Frankenweenie - Victor's Experiments - Customization (Terrain)",
     {InfinityFigureType::Item, 0x0930}},
    {"The Nightmare Before Christmas - Jack's Scary Decorations - Customization (Terrain)",
     {InfinityFigureType::Item, 0x0931}},
    {"Frozen - Frozen Flourish - Customization (Terrain)", {InfinityFigureType::Item, 0x0933}},
    {"Tangled - Rapunzel's Kingdom - Customization (Terrain)", {InfinityFigureType::Item, 0x0934}},
    {"TRON - TRON Interface - Customization (Skydome)", {InfinityFigureType::Item, 0x0935}},
    {"WALL-E - Buy N Large Atmosphere - Customization (Skydome)",
     {InfinityFigureType::Item, 0x0936}},
    {"Wreck-It Ralph - Sugar Rush Sky - Customization (Skydome)",
     {InfinityFigureType::Item, 0x0937}},
    {"The Nightmare Before Christmas - Halloween Town Sky - Customization (Skydome)",
     {InfinityFigureType::Item, 0x093A}},
    {"Frozen - Chill in the Air - Customization (Skydome)", {InfinityFigureType::Item, 0x093C}},
    {"Tangled - Rapunzel's Birthday Sky - Customization (Skydome)",
     {InfinityFigureType::Item, 0x093D}},
    {"Toy Story, Disney Parks - Astro Blasters Space Cruiser - Toy (Vehicle)",
     {InfinityFigureType::Item, 0x0940}},
    {"Finding Nemo - Marlin's Reef - Customization (Terrain)", {InfinityFigureType::Item, 0x0941}},
    {"Finding Nemo - Nemo's Seascape - Customization (Skydome)",
     {InfinityFigureType::Item, 0x0942}},
    {"Alice in Wonderland - Alice's Wonderland - Customization (Terrain)",
     {InfinityFigureType::Item, 0x0943}},
    {"Alice in Wonderland - Tulgey Wood - Customization (Skydome)",
     {InfinityFigureType::Item, 0x0944}},
    {"Phineas and Ferb - Tri-State Area Terrain - Customization (Terrain)",
     {InfinityFigureType::Item, 0x0945}},
    {"Phineas and Ferb - Danville Sky - Customization (Skydome)",
     {InfinityFigureType::Item, 0x0946}},
    {"Bolt - Bolt's Super Strength - Ability", {InfinityFigureType::Ability_A, 0xC6C3}},
    {"Wreck-it Ralph - Ralph's Power of Destruction - Ability",
     {InfinityFigureType::Ability_A, 0xC6C4}},
    {"Fantasia - Chernabog's Power - Ability", {InfinityFigureType::Ability_A, 0xC6C5}},
    {"Cars - C.H.R.O.M.E. Damage Increaser - Ability", {InfinityFigureType::Ability_A, 0xC6C6}},
    {"Phineas and Ferb - Dr. Doofenshmirtz's Damage-Inator! - Ability",
     {InfinityFigureType::Ability_A, 0xC6C7}},
    {"Frankenweenie - Electro-Charge - Ability", {InfinityFigureType::Ability_B, 0xC6C8}},
    {"Wreck-It Ralph - Fix-It Felix's Repair Power - Ability",
     {InfinityFigureType::Ability_B, 0xC6C9}},
    {"Tangled - Rapunzel's Healing - Ability", {InfinityFigureType::Ability_B, 0xC6CA}},
    {"Cars - C.H.R.O.M.E. Armor Shield - Ability", {InfinityFigureType::Ability_B, 0xC6CB}},
    {"Toy Story - Star Command Shield - Ability", {InfinityFigureType::Ability_B, 0xC6CC}},
    {"The Incredibles - Violet's Force Field - Ability", {InfinityFigureType::Ability_B, 0xC6CD}},
    {"Pirates of the Caribbean - Pieces of Eight - Ability",
     {InfinityFigureType::Ability_B, 0xC6CE}},
    {"DuckTales - Scrooge McDuck's Lucky Dime - Ability", {InfinityFigureType::Ability_B, 0xC6CF}},
    {"TRON - User Control Disc - Ability", {InfinityFigureType::Ability_B, 0xC6D0}},
    {"Fantasia - Mickey's Sorcerer Hat - Ability", {InfinityFigureType::Ability_B, 0xC6D1}},
    {"Toy Story - Emperor Zurg's Wrath - Ability", {InfinityFigureType::Ability_B, 0xC6FE}},
    {"The Sword in the Stone - Merlin's Summon - Ability",
     {InfinityFigureType::Ability_B, 0xC6FF}}};

static constexpr std::array<u8, 32> SHA1_CONSTANT = {
    0xAF, 0x62, 0xD2, 0xEC, 0x04, 0x91, 0x96, 0x8C, 0xC5, 0x2A, 0x1A, 0x71, 0x65, 0xF8, 0x65, 0xFE,
    0x28, 0x63, 0x29, 0x20, 0x44, 0x69, 0x73, 0x6e, 0x65, 0x79, 0x20, 0x32, 0x30, 0x31, 0x33};

static constexpr std::array<u8, 16> BLANK_BLOCK = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

InfinityUSB::InfinityUSB(Kernel& ios, const std::string& device_name) : m_ios(ios)
{
  m_vid = 0x0E6F;
  m_pid = 0x0129;
  m_id = (u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1));
  m_device_descriptor = DeviceDescriptor{0x12,   0x1,    0x200, 0x0, 0x0, 0x0, 0x20,
                                         0x0E6F, 0x0129, 0x200, 0x1, 0x2, 0x3, 0x1};
  m_config_descriptor.emplace_back(ConfigDescriptor{0x9, 0x2, 0x29, 0x1, 0x1, 0x0, 0x80, 0xFA});
  m_interface_descriptor.emplace_back(
      InterfaceDescriptor{0x9, 0x4, 0x0, 0x0, 0x2, 0x3, 0x0, 0x0, 0x0});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x81, 0x3, 0x20, 0x1});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x1, 0x3, 0x20, 0x1});
}

InfinityUSB::~InfinityUSB() = default;

DeviceDescriptor InfinityUSB::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> InfinityUSB::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> InfinityUSB::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> InfinityUSB::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool InfinityUSB::Attach()
{
  if (m_device_attached)
  {
    return true;
  }
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  m_device_attached = true;
  return true;
}

bool InfinityUSB::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int InfinityUSB::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int InfinityUSB::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int InfinityUSB::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int InfinityUSB::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int InfinityUSB::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);
  return 0;
}
int InfinityUSB::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={:04x} endpoint={:02x}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  return 0;
}
int InfinityUSB::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={:04x} endpoint={:02x}", m_vid,
                m_pid, m_active_interface, cmd->length, cmd->endpoint);

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  auto& infinity_base = system.GetInfinityBase();
  u8* buf = memory.GetPointerForRange(cmd->data_address, cmd->length);

  std::array<u8, 32> data = {};
  std::array<u8, 32> response_data = {};

  // First call - constant device response
  if (buf[0] == 0x00)
  {
    m_response_list.push(std::move(cmd));
  }
  // Respond with data requested from FF command
  else if (buf[0] == 0xAA || buf[0] == 0xAB)
  {
    // If a new figure has been added or removed, get the updated figure response
    if (infinity_base.HasFigureBeenAddedRemoved())
    {
      ScheduleTransfer(std::move(cmd), infinity_base.GetAddedRemovedResponse(), 32, 1000);
    }
    else if (m_queries.empty())
    {
      m_response_list.push(std::move(cmd));
    }
    else
    {
      ScheduleTransfer(std::move(cmd), m_queries.front(), 32, 1000);
      m_queries.pop();
    }
  }
  else if (buf[0] == 0xFF)
  {
    // FF <length of packet data> <packet data> <checksum>
    // <checksum> is the sum of all the bytes preceding it (including the FF and <packet length>).
    //
    //<packet data> consists of:
    //
    //<command> <sequence> <optional attached data>
    //<sequence> is an auto-incrementing sequence, so that the game can match up the command packet
    // with the response it goes with.
    // <length of packet data> includes <command>, <sequence> and <optional attached data>, but not
    // FF or <checksum>

    u8 command = buf[2];
    u8 sequence = buf[3];

    switch (command)
    {
    case 0x80:
    {
      // Activate Base, constant response (might be specific based on device type)
      response_data = {0xaa, 0x15, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x03, 0x02, 0x09, 0x09, 0x43,
                       0x20, 0x32, 0x62, 0x36, 0x36, 0x4b, 0x34, 0x99, 0x67, 0x31, 0x93, 0x8c};
      break;
    }
    case 0x81:
    {
      // Initiate random number generation, combine the 8 big endian aligned bytes sent in the
      // command in to a u64, and descramble this number, which is then used to generate
      // the seed for the random number generator to align with the game. Finally, respond with a
      // blank packet as this command doesn't need a response.
      u64 value = u64(buf[4]) << 56 | u64(buf[5]) << 48 | u64(buf[6]) << 40 | u64(buf[7]) << 32 |
                  u64(buf[8]) << 24 | u64(buf[9]) << 16 | u64(buf[10]) << 8 | u64(buf[11]);
      u32 seed = infinity_base.Descramble(value);
      infinity_base.GenerateSeed(seed);
      infinity_base.GetBlankResponse(sequence, response_data.data());
      break;
    }
    case 0x83:
    {
      // Respond with random generated numbers based on the seed from the 0x81 command. The next
      // random number is 4 bytes, which then needs to be scrambled in to an 8 byte unsigned
      // integer, and then passed back as 8 big endian aligned bytes.
      u32 next_random = infinity_base.GetNext();
      u64 scrambled_next_random = infinity_base.Scramble(next_random, 0);
      response_data = {0xAA, 0x09, sequence};
      response_data[3] = u8((scrambled_next_random >> 56) & 0xFF);
      response_data[4] = u8((scrambled_next_random >> 48) & 0xFF);
      response_data[5] = u8((scrambled_next_random >> 40) & 0xFF);
      response_data[6] = u8((scrambled_next_random >> 32) & 0xFF);
      response_data[7] = u8((scrambled_next_random >> 24) & 0xFF);
      response_data[8] = u8((scrambled_next_random >> 16) & 0xFF);
      response_data[9] = u8((scrambled_next_random >> 8) & 0xFF);
      response_data[10] = u8(scrambled_next_random & 0xFF);
      response_data[11] = infinity_base.GenerateChecksum(response_data.data(), 11);
      break;
    }

    case 0x90:
    case 0x92:
    case 0x93:
    case 0x95:
    case 0x96:
    {
      // Set Colors. Needs further input to actually emulate the 'colors', but none of these
      // commands expect a response. Potential commands could include turning the lights on
      // individual sections of the base, flashing lights, setting colors, initiating a color
      // sequence etc.
      infinity_base.GetBlankResponse(sequence, response_data.data());
      break;
    }

    case 0xA1:
    {
      // A1 - Get Presence Status:
      // Returns what figures, if any, are present (and possibly the order they were placed, or
      // something). For each figure on the infinity base, 2 blocks are sent in the response:
      // <index> <unknown>
      // <index> is 20 for player 1, 30 for player 2, and 10 for the hexagonal position.
      // This is also added with the order the figure was added, so 22 would indicate player one,
      // which was added 3rd to the base.
      // <unknown> is (for me) always 09. If nothing is on the
      // infinity base, no data is sent in the response.
      infinity_base.GetPresentFigures(sequence, response_data.data());
      break;
    }

    case 0xA2:
    {
      // A2 - Read Figure Data:
      // This reads a block of 16 bytes from a figure on the infinity base.
      // Attached data: II BB UU
      // II is the order the figure was added (00 for first, 01 for second and 03 for third).
      // BB is the block number. 00 is block 1, 01 is block 4, 02 is block 8, and 03 is block 12).
      // UU is either 00 or 01 -- not exactly sure what it indicates.
      infinity_base.QueryBlock(buf[4], buf[5], response_data.data(), sequence);
      break;
    }

    case 0xA3:
    {
      // A3 - Write Figure Data:
      // This writes a block of 16 bytes to a figure on the infinity base.
      // Attached data: II BB UU <16 bytes>
      // II is the order the figure was added (00 for first, 01 for second and 03 for third).
      // BB is the block number. 00 is block 1, 01 is block 4, 02 is block 8, and 03 is block 12).
      // UU is either 00 or 01 -- not exactly sure what it indicates.
      // <16 bytes> is the raw (encrypted) 16 bytes to be sent to the figure.
      infinity_base.WriteBlock(buf[4], buf[5], &buf[7], response_data.data(), sequence);
      break;
    }

    case 0xB4:
    {
      // B4 - Get Tag ID:
      // This gets the tag's unique ID for a figure on the infinity base.
      // The first byte is the order the figure was added (00 for first, 01 for second and 03 for
      // third). The infinity base will respond with 7 bytes (the tag ID).
      infinity_base.GetFigureIdentifier(buf[4], sequence, response_data.data());
      break;
    }

    case 0xB5:
    {
      // Get status?
      infinity_base.GetBlankResponse(sequence, response_data.data());
      break;
    }

    default:
      break;
    }
    memcpy(data.data(), buf, 32);
    ScheduleTransfer(std::move(cmd), data, 32, 500);
    if (m_response_list.empty())
    {
      m_queries.push(response_data);
    }
    else
    {
      ScheduleTransfer(std::move(m_response_list.front()), response_data, 32, 1000);
      m_response_list.pop();
    }
  }
  return 0;
}
int InfinityUSB::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Isochronous: length={:04x} endpoint={:02x} num_packets={:02x}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  return 0;
}
void InfinityUSB::ScheduleTransfer(std::unique_ptr<TransferCommand> command,
                                   const std::array<u8, 32>& data, s32 expected_count,
                                   u64 expected_time_us)
{
  command->FillBuffer(data.data(), expected_count);
  command->ScheduleTransferCompletion(expected_count, expected_time_us);
}

bool InfinityBase::HasFigureBeenAddedRemoved() const
{
  return !m_figure_added_removed_response.empty();
}

std::array<u8, 32> InfinityBase::GetAddedRemovedResponse()
{
  std::array<u8, 32> response = m_figure_added_removed_response.front();
  m_figure_added_removed_response.pop();
  return response;
}

u8 InfinityBase::GenerateChecksum(const u8* data, int num_of_bytes) const
{
  int checksum = 0;
  for (int i = 0; i < num_of_bytes; i++)
  {
    checksum += data[i];
  }
  return (checksum & 0xFF);
}

void InfinityBase::GetBlankResponse(u8 sequence, u8* reply_buf)
{
  reply_buf[0] = 0xaa;
  reply_buf[1] = 0x01;
  reply_buf[2] = sequence;
  reply_buf[3] = GenerateChecksum(reply_buf, 3);
}

void InfinityBase::GetPresentFigures(u8 sequence, u8* reply_buf)
{
  int x = 3;
  if (m_player_one_ability_one.present)
  {
    reply_buf[x] = 0x20 + m_player_one_ability_one.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_player_one_ability_two.present)
  {
    reply_buf[x] = 0x20 + m_player_one_ability_two.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_player_one.present)
  {
    reply_buf[x] = 0x20 + m_player_one.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_player_two_ability_one.present)
  {
    reply_buf[x] = 0x30 + m_player_two_ability_one.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_player_two_ability_two.present)
  {
    reply_buf[x] = 0x30 + m_player_two_ability_two.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_player_two.present)
  {
    reply_buf[x] = 0x30 + m_player_two.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  if (m_hexagon.present)
  {
    reply_buf[x] = 0x10 + m_hexagon.order_added;
    reply_buf[x + 1] = 0x09;
    x = x + 2;
  }
  reply_buf[0] = 0xaa;
  reply_buf[1] = x - 2;
  reply_buf[2] = sequence;
  reply_buf[x] = GenerateChecksum(reply_buf, x);
}

void InfinityBase::GetFigureIdentifier(u8 fig_num, u8 sequence, u8* reply_buf)
{
  std::lock_guard lock(m_infinity_mutex);

  InfinityFigure& figure =
      fig_num == m_player_one.order_added             ? m_player_one :
      fig_num == m_player_two.order_added             ? m_player_two :
      fig_num == m_player_one_ability_one.order_added ? m_player_one_ability_one :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_one :
      fig_num == m_player_one_ability_two.order_added ? m_player_one_ability_two :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_two :
                                                        m_hexagon;

  reply_buf[0] = 0xaa;
  reply_buf[1] = 0x09;
  reply_buf[2] = sequence;
  reply_buf[3] = 0x00;

  if (figure.present)
  {
    memcpy(reply_buf + 4, figure.data.data(), 7);
  }
  reply_buf[11] = GenerateChecksum(reply_buf, 11);
}

void InfinityBase::QueryBlock(u8 fig_num, u8 block, u8* reply_buf, u8 sequence)
{
  std::lock_guard lock(m_infinity_mutex);

  InfinityFigure& figure =
      fig_num == m_player_one.order_added             ? m_player_one :
      fig_num == m_player_two.order_added             ? m_player_two :
      fig_num == m_player_one_ability_one.order_added ? m_player_one_ability_one :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_one :
      fig_num == m_player_one_ability_two.order_added ? m_player_one_ability_two :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_two :
                                                        m_hexagon;

  reply_buf[0] = 0xaa;
  reply_buf[1] = 0x12;
  reply_buf[2] = sequence;
  reply_buf[3] = 0x00;
  u8 file_block = 0;
  if (block == 0)
  {
    file_block = 1;
  }
  else
  {
    file_block = block * 4;
  }
  if (figure.present)
  {
    memcpy(reply_buf + 4, figure.data.data() + (16 * (file_block)), 16);
  }
  reply_buf[20] = GenerateChecksum(reply_buf, 20);
}

void InfinityBase::WriteBlock(u8 fig_num, u8 block, const u8* to_write_buf, u8* reply_buf,
                              u8 sequence)
{
  std::lock_guard lock(m_infinity_mutex);

  InfinityFigure& figure =
      fig_num == m_player_one.order_added             ? m_player_one :
      fig_num == m_player_two.order_added             ? m_player_two :
      fig_num == m_player_one_ability_one.order_added ? m_player_one_ability_one :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_one :
      fig_num == m_player_one_ability_two.order_added ? m_player_one_ability_two :
      fig_num == m_player_two_ability_two.order_added ? m_player_two_ability_two :
                                                        m_hexagon;

  reply_buf[0] = 0xaa;
  reply_buf[1] = 0x02;
  reply_buf[2] = sequence;
  reply_buf[3] = 0x00;
  u8 file_block = 0;
  if (block == 0)
  {
    file_block = 1;
  }
  else
  {
    file_block = block * 4;
  }
  if (figure.present)
  {
    memcpy(figure.data.data() + ((file_block)*16), to_write_buf, 16);
    figure.Save();
  }
  reply_buf[4] = GenerateChecksum(reply_buf, 4);
}

u32 InfinityCRC32(const u8* buffer, u32 size)
{
  static const u32 CRC32_TABLE[256] = {
      0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
      0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
      0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
      0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
      0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
      0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
      0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
      0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
      0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
      0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
      0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
      0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
      0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
      0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
      0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
      0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
      0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
      0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
      0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
      0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
      0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
      0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
      0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
      0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
      0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
      0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
      0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
      0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
      0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
      0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
      0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
      0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
      0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
      0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
      0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
      0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
      0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

  // Calculate it
  u32 ret = 0x00000000;
  for (u32 i = 0; i < size; ++i)
  {
    u8 index = u8(((ret & 0xFF) ^ buffer[i]));
    ret = u32(((ret >> 8) ^ CRC32_TABLE[index]));
  }

  return ret;
}

std::string InfinityBase::LoadFigure(u8* buf, File::IOFile in_file, u8 position)
{
  std::lock_guard lock(m_infinity_mutex);
  u8 order_added;

  std::vector<u8> sha1_calc = {SHA1_CONSTANT.begin(), SHA1_CONSTANT.end() - 1};
  for (int i = 0; i < 7; i++)
  {
    sha1_calc.push_back(buf[i]);
  }

  Common::SHA1::Digest digest = Common::SHA1::CalculateDigest(sha1_calc);
  std::vector<u8> infinity_aes_key = {};
  for (int i = 0; i < 4; i++)
  {
    for (int x = 3; x >= 0; x--)
    {
      infinity_aes_key.push_back(digest[x + (i * 4)]);
    }
  }
  std::unique_ptr<Common::AES::Context> ctx =
      Common::AES::CreateContextDecrypt(infinity_aes_key.data());
  std::array<u8, 16> infinity_decrypted_block = {};
  ctx->CryptIvZero(&buf[16], infinity_decrypted_block.data(), 16);

  u16 number = u16(infinity_decrypted_block[2]) << 8 | u16(infinity_decrypted_block[3]);
  DEBUG_LOG_FMT(IOS_USB, "Toy Number: {}", number);

  InfinityFigure& figure = position == 0x01 ? m_hexagon :
                           position == 0x02 ? m_player_one :
                           position == 0x03 ? m_player_two :
                           position == 0x04 ? m_player_one_ability_one :
                           position == 0x05 ? m_player_two_ability_one :
                           position == 0x06 ? m_player_one_ability_two :
                                              m_player_two_ability_two;

  figure.inf_file = std::move(in_file);
  memcpy(figure.data.data(), buf, figure.data.size());
  figure.present = true;
  if (figure.order_added == 255)
  {
    figure.order_added = m_figure_order;
    m_figure_order++;
  }
  order_added = figure.order_added;

  while (position > 3)
    position = position - 2;

  std::array<u8, 32> figure_change_response = {0xab, 0x04, position, 0x09, order_added, 0x00};
  figure_change_response[6] = GenerateChecksum(figure_change_response.data(), 6);
  m_figure_added_removed_response.push(figure_change_response);

  return FindFigure(number);
}

void InfinityBase::RemoveFigure(u8 position)
{
  std::lock_guard lock(m_infinity_mutex);
  InfinityFigure& figure = position == 0x01 ? m_hexagon :
                           position == 0x02 ? m_player_one :
                           position == 0x03 ? m_player_two :
                           position == 0x04 ? m_player_one_ability_one :
                           position == 0x05 ? m_player_two_ability_one :
                           position == 0x06 ? m_player_one_ability_two :
                                              m_player_two_ability_two;
  figure.present = false;

  while (position > 3)
    position = position - 2;

  std::array<u8, 32> figure_change_response = {0xab, 0x04, position, 0x09, figure.order_added,
                                               0x01};
  figure_change_response[6] = GenerateChecksum(figure_change_response.data(), 6);
  m_figure_added_removed_response.push(figure_change_response);
}

bool InfinityBase::CreateFigure(const std::string& file_path, u16 figure_num)
{
  File::IOFile inf_file(file_path, "w+b");
  if (!inf_file)
  {
    return false;
  }
  InfinityFigureInfo figure = {InfinityFigureType::Character, 0};
  for (auto it = list_infinity_figures.begin(); it != list_infinity_figures.end(); it++)
  {
    if (it->second.figure_number == figure_num)
    {
      figure = it->second;
      break;
    }
  }
  if (figure.figure_number == 0)
  {
    return false;
  }
  std::array<u8, INFINITY_NUM_BLOCKS * INFINITY_BLOCK_SIZE> file_data{};
  u32 first_block = 0x17878E;
  u32 other_blocks = 0x778788;
  for (s8 i = 2; i >= 0; i--)
  {
    file_data[0x38 - i] = u8((first_block >> i * 8) & 0xFF);
  }
  for (u32 index = 1; index < 0x05; index++)
  {
    for (s8 i = 2; i >= 0; i--)
    {
      file_data[((index * 0x40) + 0x38) - i] = u8((other_blocks >> i * 8) & 0xFF);
    }
  }
  std::vector<u8> sha1_calc = {SHA1_CONSTANT.begin(), SHA1_CONSTANT.end() - 1};
  std::array<u8, 16> uid_data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x44, 0x00, 0xC2};
  Common::Random::Generate(&uid_data[0], 7);
  for (s8 i = 0; i < 7; i++)
  {
    sha1_calc.push_back(uid_data[i]);
  }
  std::array<u8, 16> figure_data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x01, 0xD1, 0x1F};
  if (figure.type == InfinityFigureType::Character)
  {
    figure_data[1] = 0x0F;
    figure_data[4] = 0x10;
    figure_data[5] = 0x0C;
    figure_data[6] = 0x1E;
  }
  else if (figure.type == InfinityFigureType::Playset)
  {
    figure_data[1] = 0x1E;
    figure_data[4] = 0x11;
    figure_data[5] = 0x04;
    figure_data[6] = 0x06;
  }
  else if (figure.type == InfinityFigureType::Ability_A)
  {
    figure_data[1] = 0x2D;
    figure_data[4] = 0x11;
    figure_data[5] = 0x04;
    figure_data[6] = 0x16;
  }
  else if (figure.type == InfinityFigureType::Ability_B)
  {
    figure_data[1] = 0x2D;
    figure_data[4] = 0x11;
    figure_data[5] = 0x04;
    figure_data[6] = 0x08;
  }
  else if (figure.type == InfinityFigureType::Item)
  {
    figure_data[1] = 0x3D;
    figure_data[4] = 0x12;
    figure_data[5] = 0x0A;
    figure_data[6] = 0x15;
  }
  figure_data[2] = u8((figure_num >> 8) & 0xFF);
  figure_data[3] = u8(figure_num & 0xFF);
  u32 checksum = InfinityCRC32(figure_data.data(), 12);
  for (s8 i = 3; i >= 0; i--)
  {
    figure_data[15 - i] = u8((checksum >> i * 8) & 0xFF);
  }
  DEBUG_LOG_FMT(IOS_USB, "Block 1: \n{}", HexDump(figure_data.data(), figure_data.size()));
  Common::SHA1::Digest digest = Common::SHA1::CalculateDigest(sha1_calc);
  // Infinity AES keys are the first 16 bytes of the SHA1 Digest, every set of 4 bytes need to be
  // reversed due to endianness
  std::vector<u8> infinity_aes_key = {};
  for (int i = 0; i < 4; i++)
  {
    for (int x = 3; x >= 0; x--)
    {
      infinity_aes_key.push_back(digest[x + (i * 4)]);
    }
  }

  // Create AES Encrypt context based on AES key, use this to encrypt the character data and 4 blank
  // blocks
  std::unique_ptr<Common::AES::Context> ctx =
      Common::AES::CreateContextEncrypt(infinity_aes_key.data());
  std::array<u8, 16> encrypted_block = {};
  std::array<u8, 16> encrypted_blank = {};

  ctx->CryptIvZero(figure_data.data(), encrypted_block.data(), figure_data.size());
  ctx->CryptIvZero(BLANK_BLOCK.data(), encrypted_blank.data(), BLANK_BLOCK.size());

  memcpy(&file_data[0], uid_data.data(), uid_data.size());
  memcpy(&file_data[16], encrypted_block.data(), encrypted_block.size());
  memcpy(&file_data[16 * 0x04], encrypted_blank.data(), encrypted_blank.size());
  memcpy(&file_data[16 * 0x08], encrypted_blank.data(), encrypted_blank.size());
  memcpy(&file_data[16 * 0x0C], encrypted_blank.data(), encrypted_blank.size());
  memcpy(&file_data[16 * 0x0D], encrypted_blank.data(), encrypted_blank.size());

  DEBUG_LOG_FMT(IOS_USB, "File Data: \n{}", HexDump(file_data.data(), file_data.size()));

  inf_file.WriteBytes(file_data.data(), file_data.size());

  return true;
}

const std::map<const char*, const InfinityFigureInfo>& InfinityBase::GetFigureList()
{
  return list_infinity_figures;
}

std::string InfinityBase::FindFigure(u16 number) const
{
  for (auto it = list_infinity_figures.begin(); it != list_infinity_figures.end(); it++)
  {
    if (it->second.figure_number == number)
    {
      return it->first;
    }
  }
  return "";
}

void InfinityBase::GenerateSeed(u32 seed)
{
  m_random_a = 0xF1EA5EED;
  m_random_b = seed;
  m_random_c = seed;
  m_random_d = seed;

  for (int i = 0; i < 23; i++)
  {
    GetNext();
  }
}

u32 InfinityBase::GetNext()
{
  u32 a = m_random_a;
  u32 b = m_random_b;
  u32 c = m_random_c;
  u32 ret = std::rotl(m_random_b, 27);

  u32 temp = (a + ((ret ^ 0xFFFFFFFF) + 1)) & 0xFFFFFFFF;
  b = (b ^ std::rotl(c, 17)) & 0xFFFFFFFF;
  a = m_random_d;
  c = (c + a) & 0xFFFFFFFF;
  ret = (b + temp) & 0xFFFFFFFF;
  a = (a + temp) & 0xFFFFFFFF;

  m_random_c = a;
  m_random_a = b;
  m_random_b = c;
  m_random_d = ret;

  return ret;
}

u64 InfinityBase::Scramble(u32 num_to_scramble, u32 garbage)
{
  u64 mask = 0x8E55AA1B3999E8AA;
  u64 ret = 0;

  for (int i = 0; i < 64; i++)
  {
    ret = ret << 1;

    if ((mask & 0x01) > 0)
    {
      ret |= (num_to_scramble & 0x01);
      num_to_scramble = num_to_scramble >> 1;
    }
    else
    {
      ret |= (garbage & 0x01);
      garbage = garbage >> 1;
    }

    mask = mask >> 1;
  }

  return ret;
}

u32 InfinityBase::Descramble(u64 num_to_descramble)
{
  u64 mask = 0x8E55AA1B3999E8AA;
  u32 ret = 0;

  for (int i = 0; i < 64; i++)
  {
    if ((mask & 0x8000000000000000) > 0)
    {
      ret = ((ret << 1) & 0xFFFFFFFF) | (num_to_descramble & 0x01);
    }

    num_to_descramble = num_to_descramble >> 1;
    mask = mask << 1;
  }

  return ret;
}

void InfinityFigure::Save()
{
  if (!inf_file)
    return;

  inf_file.Seek(0, File::SeekOrigin::Begin);
  inf_file.WriteBytes(data.data(), 0x40 * 0x05);
}
}  // namespace IOS::HLE::USB
