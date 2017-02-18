// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is the main Wii IPC file that handles all incoming IPC requests and directs them
// to the right function.
//
// IPC basics (IOS' usage):
// All IPC request handlers will write a return value to 0x04.
//   Open: Device file descriptor or error code
//   Close: IPC_SUCCESS
//   Read: Bytes read
//   Write: Bytes written
//   Seek: Seek position
//   Ioctl: Depends on the handler
//   Ioctlv: Depends on the handler
// Replies may be sent immediately or asynchronously for ioctls and ioctlvs.

#include "Core/IOS/IPC.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/DeviceStub.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FS.h"
#include "Core/IOS/FS/FileIO.h"
#include "Core/IOS/MIOS.h"
#include "Core/IOS/Network/IP/Top.h"
#include "Core/IOS/Network/KD/NetKDRequest.h"
#include "Core/IOS/Network/KD/NetKDTime.h"
#include "Core/IOS/Network/NCD/Manage.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/IOS/Network/WD/Command.h"
#include "Core/IOS/SDIO/SDIOSlot0.h"
#include "Core/IOS/STM/STM.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/IOS/USB/OH0/OH0.h"
#include "Core/IOS/USB/OH0/OH0Device.h"
#include "Core/IOS/USB/USB_HID/HIDv4.h"
#include "Core/IOS/USB/USB_KBD.h"
#include "Core/IOS/USB/USB_VEN/VEN.h"
#include "Core/IOS/WFS/WFSI.h"
#include "Core/IOS/WFS/WFSSRV.h"

namespace CoreTiming
{
struct EventType;
}  // namespace CoreTiming

namespace IOS
{
namespace HLE
{
static std::map<u32, std::shared_ptr<Device::Device>> s_device_map;
static std::mutex s_device_map_mutex;

// STATE_TO_SAVE
constexpr u8 IPC_MAX_FDS = 0x18;
constexpr u8 ES_MAX_COUNT = 3;
static std::shared_ptr<Device::Device> s_fdmap[IPC_MAX_FDS];
static std::shared_ptr<Device::ES> s_es_handles[ES_MAX_COUNT];

using IPCMsgQueue = std::deque<u32>;
static IPCMsgQueue s_request_queue;  // ppc -> arm
static IPCMsgQueue s_reply_queue;    // arm -> ppc
static IPCMsgQueue s_ack_queue;      // arm -> ppc

static CoreTiming::EventType* s_event_enqueue;
static CoreTiming::EventType* s_event_sdio_notify;

static u64 s_last_reply_time;

static u64 s_active_title_id;

static constexpr u64 ENQUEUE_REQUEST_FLAG = 0x100000000ULL;
static constexpr u64 ENQUEUE_ACKNOWLEDGEMENT_FLAG = 0x200000000ULL;

struct IosMemoryValues
{
  u16 ios_number;
  u32 ios_version;
  u32 ios_date;
  u32 mem1_physical_size;
  u32 mem1_simulated_size;
  u32 mem1_end;
  u32 mem1_arena_begin;
  u32 mem1_arena_end;
  u32 mem2_physical_size;
  u32 mem2_simulated_size;
  u32 mem2_end;
  u32 mem2_arena_begin;
  u32 mem2_arena_end;
  u32 ipc_buffer_begin;
  u32 ipc_buffer_end;
  u32 hollywood_revision;
  u32 ram_vendor;
  u32 unknown_begin;
  u32 unknown_end;
  u32 sysmenu_sync;
};

constexpr u32 ADDR_MEM1_SIZE = 0x3100;
constexpr u32 ADDR_MEM1_SIM_SIZE = 0x3104;
constexpr u32 ADDR_MEM1_END = 0x3108;
constexpr u32 ADDR_MEM1_ARENA_BEGIN = 0x310c;
constexpr u32 ADDR_MEM1_ARENA_END = 0x3110;
constexpr u32 ADDR_PH1 = 0x3114;
constexpr u32 ADDR_MEM2_SIZE = 0x3118;
constexpr u32 ADDR_MEM2_SIM_SIZE = 0x311c;
constexpr u32 ADDR_MEM2_END = 0x3120;
constexpr u32 ADDR_MEM2_ARENA_BEGIN = 0x3124;
constexpr u32 ADDR_MEM2_ARENA_END = 0x3128;
constexpr u32 ADDR_PH2 = 0x312c;
constexpr u32 ADDR_IPC_BUFFER_BEGIN = 0x3130;
constexpr u32 ADDR_IPC_BUFFER_END = 0x3134;
constexpr u32 ADDR_HOLLYWOOD_REVISION = 0x3138;
constexpr u32 ADDR_PH3 = 0x313c;
constexpr u32 ADDR_IOS_VERSION = 0x3140;
constexpr u32 ADDR_IOS_DATE = 0x3144;
constexpr u32 ADDR_UNKNOWN_BEGIN = 0x3148;
constexpr u32 ADDR_UNKNOWN_END = 0x314c;
constexpr u32 ADDR_PH4 = 0x3150;
constexpr u32 ADDR_PH5 = 0x3154;
constexpr u32 ADDR_RAM_VENDOR = 0x3158;
constexpr u32 ADDR_BOOT_FLAG = 0x315c;
constexpr u32 ADDR_APPLOADER_FLAG = 0x315d;
constexpr u32 ADDR_DEVKIT_BOOT_PROGRAM_VERSION = 0x315e;
constexpr u32 ADDR_SYSMENU_SYNC = 0x3160;

constexpr u32 MEM1_SIZE = 0x01800000;
constexpr u32 MEM1_END = 0x81800000;
constexpr u32 MEM1_ARENA_BEGIN = 0x00000000;
constexpr u32 MEM1_ARENA_END = 0x81800000;
constexpr u32 MEM2_SIZE = 0x4000000;
constexpr u32 MEM2_ARENA_BEGIN = 0x90000800;
constexpr u32 HOLLYWOOD_REVISION = 0x00000011;
constexpr u32 PLACEHOLDER = 0xDEADBEEF;
constexpr u32 RAM_VENDOR = 0x0000FF01;
constexpr u32 RAM_VENDOR_MIOS = 0xCAFEBABE;

// These values were manually extracted from the relevant IOS binaries.
// The writes are usually contained in a single function that
// mostly writes raw literals to the relevant locations.
// e.g. IOS9, version 1034, content id 0x00000006, function at 0xffff6884
constexpr std::array<IosMemoryValues, 40> ios_memory_values = {
    {{
         9,          0x9040a,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         11,         0xb000a,     0x102506,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         12,         0xc020e,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         13,         0xd0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         14,         0xe0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         15,         0xf0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         17,         0x110408,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         20,         0x14000c,    0x102506,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         21,         0x15040f,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         22,         0x16050e,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         28,         0x1c070f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93800000,       MEM2_ARENA_BEGIN,
         0x937E0000, 0x937E0000, 0x93800000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93800000, 0x93820000,       0,
     },
     {
         30,         0x1e0a10,   0x40308,          MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         31,         0x1f0e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         33,         0x210e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         34,         0x220e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         35,         0x230e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         36,         0x240e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         37,         0x25161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         38,         0x26101c,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         40,         0x280911,   0x022308,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         41,         0x290e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         43,         0x2b0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         45,         0x2d0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         46,         0x2e0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         48,         0x30101c,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         50,         0x321319,   0x101008,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         51,         0x331219,   0x071108,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         52,         0x34161d,   0x101008,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         53,         0x35161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         55,         0x37161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         56,         0x38161e,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         57,         0x39171f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         58,         0x3a1820,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         59,         0x3b1c21,   0x101811,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         60,         0x3c181e,   0x112408,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         61,         0x3d161e,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         62,         0x3e191e,   0x022712,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         70,         0x461a1f,   0x060309,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         80,         0x501b20,   0x030310,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         257,
         0x707,
         0x82209,
         MEM1_SIZE,
         MEM1_SIZE,
         MEM1_END,
         MEM1_ARENA_BEGIN,
         MEM1_ARENA_END,
         MEM2_SIZE,
         MEM2_SIZE,
         0x93600000,
         MEM2_ARENA_BEGIN,
         0x935E0000,
         0x935E0000,
         0x93600000,
         HOLLYWOOD_REVISION,
         RAM_VENDOR_MIOS,
         PLACEHOLDER,
         PLACEHOLDER,
         PLACEHOLDER,
     }}};

static void EnqueueEvent(u64 userdata, s64 cycles_late = 0)
{
  if (userdata & ENQUEUE_ACKNOWLEDGEMENT_FLAG)
  {
    s_ack_queue.push_back(static_cast<u32>(userdata));
  }
  else if (userdata & ENQUEUE_REQUEST_FLAG)
  {
    s_request_queue.push_back(static_cast<u32>(userdata));
  }
  else
  {
    s_reply_queue.push_back(static_cast<u32>(userdata));
  }
  Update();
}

static void SDIO_EventNotify_CPUThread(u64 userdata, s64 cycles_late)
{
  auto device = static_cast<Device::SDIOSlot0*>(GetDeviceByName("/dev/sdio/slot0").get());
  if (device)
    device->EventNotify();
}

// The title ID is a u64 where the first 32 bits are used for the title type.
// For IOS title IDs, the type will always be 00000001 (system), and the lower 32 bits
// are used for the IOS major version -- which is what we want here.
u32 GetVersion()
{
  return static_cast<u32>(s_active_title_id);
}

static bool SetupMemory(u64 ios_title_id)
{
  auto target_imv = std::find_if(
      ios_memory_values.begin(), ios_memory_values.end(),
      [&](const IosMemoryValues& imv) { return imv.ios_number == (ios_title_id & 0xffff); });

  if (target_imv == ios_memory_values.end())
  {
    ERROR_LOG(IOS, "Unknown IOS version: %016" PRIx64, ios_title_id);
    return false;
  }

  Memory::Write_U32(target_imv->mem1_physical_size, ADDR_MEM1_SIZE);
  Memory::Write_U32(target_imv->mem1_simulated_size, ADDR_MEM1_SIM_SIZE);
  Memory::Write_U32(target_imv->mem1_end, ADDR_MEM1_END);
  Memory::Write_U32(target_imv->mem1_arena_begin, ADDR_MEM1_ARENA_BEGIN);
  Memory::Write_U32(target_imv->mem1_arena_end, ADDR_MEM1_ARENA_END);
  Memory::Write_U32(PLACEHOLDER, ADDR_PH1);
  Memory::Write_U32(target_imv->mem2_physical_size, ADDR_MEM2_SIZE);
  Memory::Write_U32(target_imv->mem2_simulated_size, ADDR_MEM2_SIM_SIZE);
  Memory::Write_U32(target_imv->mem2_end, ADDR_MEM2_END);
  Memory::Write_U32(target_imv->mem2_arena_begin, ADDR_MEM2_ARENA_BEGIN);
  Memory::Write_U32(target_imv->mem2_arena_end, ADDR_MEM2_ARENA_END);
  Memory::Write_U32(PLACEHOLDER, ADDR_PH2);
  Memory::Write_U32(target_imv->ipc_buffer_begin, ADDR_IPC_BUFFER_BEGIN);
  Memory::Write_U32(target_imv->ipc_buffer_end, ADDR_IPC_BUFFER_END);
  Memory::Write_U32(target_imv->hollywood_revision, ADDR_HOLLYWOOD_REVISION);
  Memory::Write_U32(PLACEHOLDER, ADDR_PH3);
  Memory::Write_U32(target_imv->ios_version, ADDR_IOS_VERSION);
  Memory::Write_U32(target_imv->ios_date, ADDR_IOS_DATE);
  Memory::Write_U32(target_imv->unknown_begin, ADDR_UNKNOWN_BEGIN);
  Memory::Write_U32(target_imv->unknown_end, ADDR_UNKNOWN_END);
  Memory::Write_U32(PLACEHOLDER, ADDR_PH4);
  Memory::Write_U32(PLACEHOLDER, ADDR_PH5);
  Memory::Write_U32(target_imv->ram_vendor, ADDR_RAM_VENDOR);
  Memory::Write_U8(0xDE, ADDR_BOOT_FLAG);
  Memory::Write_U8(0xAD, ADDR_APPLOADER_FLAG);
  Memory::Write_U16(0xBEEF, ADDR_DEVKIT_BOOT_PROGRAM_VERSION);
  Memory::Write_U32(target_imv->sysmenu_sync, ADDR_SYSMENU_SYNC);
  return true;
}

static u32 num_devices;

template <typename T>
std::shared_ptr<T> AddDevice(const char* device_name)
{
  auto device = std::make_shared<T>(num_devices, device_name);
  _assert_(device->GetDeviceType() == Device::Device::DeviceType::Static);
  s_device_map[num_devices] = device;
  num_devices++;
  return device;
}

static void AddStaticDevices()
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  _assert_msg_(IOS, s_device_map.empty(), "Reinit called while already initialized");
  Device::ES::m_ContentFile = "";

  num_devices = 0;

  // Build hardware devices
  if (!SConfig::GetInstance().m_bt_passthrough_enabled)
    AddDevice<Device::BluetoothEmu>("/dev/usb/oh1/57e/305");
  else
    AddDevice<Device::BluetoothReal>("/dev/usb/oh1/57e/305");

  AddDevice<Device::STMImmediate>("/dev/stm/immediate");
  AddDevice<Device::STMEventHook>("/dev/stm/eventhook");
  AddDevice<Device::FS>("/dev/fs");

  // IOS allows three ES devices at a time
  for (auto& es_device : s_es_handles)
    es_device = AddDevice<Device::ES>("/dev/es");

  AddDevice<Device::DI>("/dev/di");
  AddDevice<Device::NetKDRequest>("/dev/net/kd/request");
  AddDevice<Device::NetKDTime>("/dev/net/kd/time");
  AddDevice<Device::NetNCDManage>("/dev/net/ncd/manage");
  AddDevice<Device::NetWDCommand>("/dev/net/wd/command");
  AddDevice<Device::NetIPTop>("/dev/net/ip/top");
  AddDevice<Device::NetSSL>("/dev/net/ssl");
  AddDevice<Device::USB_KBD>("/dev/usb/kbd");
  AddDevice<Device::SDIOSlot0>("/dev/sdio/slot0");
  AddDevice<Device::Stub>("/dev/sdio/slot1");
  AddDevice<Device::USB_HIDv4>("/dev/usb/hid");
  AddDevice<Device::OH0>("/dev/usb/oh0");
  AddDevice<Device::Stub>("/dev/usb/oh1");
  AddDevice<Device::USB_VEN>("/dev/usb/ven");
  AddDevice<Device::WFSSRV>("/dev/usb/wfssrv");
  AddDevice<Device::WFSI>("/dev/wfsi");
}

void Init()
{
  s_event_enqueue = CoreTiming::RegisterEvent("IPCEvent", EnqueueEvent);
  s_event_sdio_notify = CoreTiming::RegisterEvent("SDIO_EventNotify", SDIO_EventNotify_CPUThread);
}

void Reset(const bool clear_devices)
{
  CoreTiming::RemoveAllEvents(s_event_enqueue);

  // Close all devices that were opened and delete their resources
  for (auto& device : s_fdmap)
  {
    if (!device)
      continue;
    device->Close();
    device.reset();
  }

  if (clear_devices)
  {
    std::lock_guard<std::mutex> lock(s_device_map_mutex);
    s_device_map.clear();
  }

  s_request_queue.clear();
  s_reply_queue.clear();

  s_last_reply_time = 0;
}

void Shutdown()
{
  Reset(true);
}

constexpr u64 BC_TITLE_ID = 0x0000000100000100;
constexpr u64 MIOS_TITLE_ID = 0x0000000100000101;

bool Reload(const u64 ios_title_id)
{
  // A real Wii goes through several steps before getting to MIOS.
  //
  // * The System Menu detects a GameCube disc and launches BC (1-100) instead of the game.
  // * BC (similar to boot1) lowers the clock speed to the Flipper's and then launches boot2.
  // * boot2 sees the lowered clock speed and launches MIOS (1-101) instead of the System Menu.
  //
  // Because we currently don't have boot1 and boot2, and BC is only ever used to launch MIOS
  // (indirectly via boot2), we can just launch MIOS when BC is launched.
  if (ios_title_id == BC_TITLE_ID)
    return Reload(MIOS_TITLE_ID);

  if (!SetupMemory(ios_title_id))
    return false;

  s_active_title_id = ios_title_id;
  Reset(true);

  if (ios_title_id == MIOS_TITLE_ID)
    return MIOS::Load();

  AddStaticDevices();
  return true;
}

void SetDefaultContentFile(const std::string& file_name)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  for (const auto& es : s_es_handles)
    es->LoadWAD(file_name);
}

void ES_DIVerify(const std::vector<u8>& tmd)
{
  Device::ES::ES_DIVerify(tmd);
}

void SDIO_EventNotify()
{
  // TODO: Potential race condition: If IsRunning() becomes false after
  // it's checked, an event may be scheduled after CoreTiming shuts down.
  if (SConfig::GetInstance().bWii && Core::IsRunning())
    CoreTiming::ScheduleEvent(0, s_event_sdio_notify, 0, CoreTiming::FromThread::NON_CPU);
}

static int GetFreeDeviceID()
{
  for (u32 i = 0; i < IPC_MAX_FDS; i++)
  {
    if (s_fdmap[i] == nullptr)
    {
      return i;
    }
  }

  return -1;
}

std::shared_ptr<Device::Device> GetDeviceByName(const std::string& device_name)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  for (const auto& entry : s_device_map)
  {
    if (entry.second && entry.second->GetDeviceName() == device_name)
    {
      return entry.second;
    }
  }

  return nullptr;
}

std::shared_ptr<Device::Device> AccessDeviceByID(u32 id)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  if (s_device_map.find(id) != s_device_map.end())
  {
    return s_device_map[id];
  }

  return nullptr;
}

void DoState(PointerWrap& p)
{
  p.Do(s_request_queue);
  p.Do(s_reply_queue);
  p.Do(s_last_reply_time);
  p.Do(s_active_title_id);

  if (s_active_title_id == MIOS_TITLE_ID)
    return;

  // We need to make sure all file handles are closed so IOS::HLE::Device::FS::DoState can
  // successfully save or re-create /tmp
  for (auto& descriptor : s_fdmap)
  {
    if (descriptor)
      descriptor->PrepareForState(p.GetMode());
  }

  for (const auto& entry : s_device_map)
    entry.second->DoState(p);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (u32 i = 0; i < IPC_MAX_FDS; i++)
    {
      u32 exists = 0;
      p.Do(exists);
      if (exists)
      {
        auto device_type = Device::Device::DeviceType::Static;
        p.Do(device_type);
        switch (device_type)
        {
        case Device::Device::DeviceType::Static:
        {
          u32 device_id = 0;
          p.Do(device_id);
          s_fdmap[i] = AccessDeviceByID(device_id);
          break;
        }
        case Device::Device::DeviceType::FileIO:
          s_fdmap[i] = std::make_shared<Device::FileIO>(i, "");
          s_fdmap[i]->DoState(p);
          break;
        case Device::Device::DeviceType::OH0:
          s_fdmap[i] = std::make_shared<Device::OH0Device>(i, "");
          s_fdmap[i]->DoState(p);
          break;
        }
      }
    }

    for (auto& es_device : s_es_handles)
    {
      const u32 handle_id = es_device->GetDeviceID();
      p.Do(handle_id);
      es_device = std::static_pointer_cast<Device::ES>(AccessDeviceByID(handle_id));
    }
  }
  else
  {
    for (auto& descriptor : s_fdmap)
    {
      u32 exists = descriptor ? 1 : 0;
      p.Do(exists);
      if (exists)
      {
        auto device_type = descriptor->GetDeviceType();
        p.Do(device_type);
        if (device_type == Device::Device::DeviceType::Static)
        {
          u32 hwId = descriptor->GetDeviceID();
          p.Do(hwId);
        }
        else
        {
          descriptor->DoState(p);
        }
      }
    }

    for (const auto& es_device : s_es_handles)
    {
      const u32 handle_id = es_device->GetDeviceID();
      p.Do(handle_id);
    }
  }
}

static std::shared_ptr<Device::Device> GetUnusedESDevice()
{
  const auto iterator = std::find_if(std::begin(s_es_handles), std::end(s_es_handles),
                                     [](const auto& es_device) { return !es_device->IsOpened(); });
  return (iterator != std::end(s_es_handles)) ? *iterator : nullptr;
}

// Returns the FD for the newly opened device (on success) or an error code.
static s32 OpenDevice(const OpenRequest& request)
{
  const s32 new_fd = GetFreeDeviceID();
  INFO_LOG(IOS, "Opening %s (mode %d, fd %d)", request.path.c_str(), request.flags, new_fd);
  if (new_fd < 0 || new_fd >= IPC_MAX_FDS)
  {
    ERROR_LOG(IOS, "Couldn't get a free fd, too many open files");
    return FS_EFDEXHAUSTED;
  }

  std::shared_ptr<Device::Device> device;
  if (request.path == "/dev/es")
  {
    device = GetUnusedESDevice();
    if (!device)
      return IPC_EESEXHAUSTED;
  }
  else if (request.path.find("/dev/usb/oh0/") == 0 && !GetDeviceByName(request.path))
  {
    device = std::make_shared<Device::OH0Device>(new_fd, request.path);
  }
  else if (request.path.find("/dev/") == 0)
  {
    device = GetDeviceByName(request.path);
  }
  else if (request.path.find('/') == 0)
  {
    device = std::make_shared<Device::FileIO>(new_fd, request.path);
  }

  if (!device)
  {
    ERROR_LOG(IOS, "Unknown device: %s", request.path.c_str());
    return IPC_ENOENT;
  }

  const ReturnCode code = device->Open(request);
  if (code < IPC_SUCCESS)
    return code;
  s_fdmap[new_fd] = device;
  return new_fd;
}

static IPCCommandResult HandleCommand(const Request& request)
{
  if (request.command == IPC_CMD_OPEN)
  {
    OpenRequest open_request{request.address};
    const s32 new_fd = OpenDevice(open_request);
    return Device::Device::GetDefaultReply(new_fd);
  }

  const auto device = (request.fd < IPC_MAX_FDS) ? s_fdmap[request.fd] : nullptr;
  if (!device)
    return Device::Device::GetDefaultReply(IPC_EINVAL);

  switch (request.command)
  {
  case IPC_CMD_CLOSE:
    s_fdmap[request.fd].reset();
    device->Close();
    return Device::Device::GetDefaultReply(IPC_SUCCESS);
  case IPC_CMD_READ:
    return device->Read(ReadWriteRequest{request.address});
  case IPC_CMD_WRITE:
    return device->Write(ReadWriteRequest{request.address});
  case IPC_CMD_SEEK:
    return device->Seek(SeekRequest{request.address});
  case IPC_CMD_IOCTL:
    return device->IOCtl(IOCtlRequest{request.address});
  case IPC_CMD_IOCTLV:
    return device->IOCtlV(IOCtlVRequest{request.address});
  default:
    _assert_msg_(IOS, false, "Unexpected command: %x", request.command);
    return Device::Device::GetDefaultReply(IPC_EINVAL);
  }
}

void ExecuteCommand(const u32 address)
{
  Request request{address};
  IPCCommandResult result = HandleCommand(request);

  // Ensure replies happen in order
  const s64 ticks_until_last_reply = s_last_reply_time - CoreTiming::GetTicks();
  if (ticks_until_last_reply > 0)
    result.reply_delay_ticks += ticks_until_last_reply;
  s_last_reply_time = CoreTiming::GetTicks() + result.reply_delay_ticks;

  if (result.send_reply)
    EnqueueReply(request, result.return_value, static_cast<int>(result.reply_delay_ticks));
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqueueRequest(u32 address)
{
  CoreTiming::ScheduleEvent(1000, s_event_enqueue, address | ENQUEUE_REQUEST_FLAG);
}

// Called to send a reply to an IOS syscall
void EnqueueReply(const Request& request, const s32 return_value, int cycles_in_future,
                  CoreTiming::FromThread from)
{
  Memory::Write_U32(static_cast<u32>(return_value), request.address + 4);
  // IOS writes back the command that was responded to in the FD field.
  Memory::Write_U32(request.command, request.address + 8);
  // IOS also overwrites the command type with the reply type.
  Memory::Write_U32(IPC_REPLY, request.address);
  CoreTiming::ScheduleEvent(cycles_in_future, s_event_enqueue, request.address, from);
}

void EnqueueCommandAcknowledgement(u32 address, int cycles_in_future)
{
  CoreTiming::ScheduleEvent(cycles_in_future, s_event_enqueue,
                            address | ENQUEUE_ACKNOWLEDGEMENT_FLAG);
}

// This is called every IPC_HLE_PERIOD from SystemTimers.cpp
// Takes care of routing ipc <-> ipc HLE
void Update()
{
  if (!IsReady())
    return;

  if (s_request_queue.size())
  {
    GenerateAck(s_request_queue.front());
    DEBUG_LOG(IOS, "||-- Acknowledge IPC Request @ 0x%08x", s_request_queue.front());
    u32 command = s_request_queue.front();
    s_request_queue.pop_front();
    ExecuteCommand(command);
    return;
  }

  if (s_reply_queue.size())
  {
    GenerateReply(s_reply_queue.front());
    DEBUG_LOG(IOS, "<<-- Reply to IPC Request @ 0x%08x", s_reply_queue.front());
    s_reply_queue.pop_front();
    return;
  }

  if (s_ack_queue.size())
  {
    GenerateAck(s_ack_queue.front());
    WARN_LOG(IOS, "<<-- Double-ack to IPC Request @ 0x%08x", s_ack_queue.front());
    s_ack_queue.pop_front();
    return;
  }
}

void UpdateDevices()
{
  // Check if a hardware device must be updated
  for (const auto& entry : s_device_map)
  {
    if (entry.second->IsOpened())
    {
      entry.second->Update();
    }
  }
}

void UpdateWantDeterminism(const bool new_want_determinism)
{
  WiiSockMan::GetInstance().UpdateWantDeterminism(new_want_determinism);
  for (const auto& device : s_device_map)
    device.second->UpdateWantDeterminism(new_want_determinism);
}
}  // namespace HLE
}  // namespace IOS
