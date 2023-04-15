// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace ExpansionInterface
{
// XXX: The BBA stores multi-byte elements as little endian.
// Multiple parts of this implementation depend on Dolphin
// being compiled for a little endian host.

CEXIETHERNET::CEXIETHERNET(Core::System& system, BBADeviceType type) : IEXIDevice(system)
{
  // Parse MAC address from config, and generate a new one if it doesn't
  // exist or can't be parsed.
  std::string mac_addr_setting = Config::Get(Config::MAIN_BBA_MAC);
  std::optional<Common::MACAddress> mac_addr = Common::StringToMacAddress(mac_addr_setting);

  Common::ToLower(&mac_addr_setting);

  if (!mac_addr)
  {
    mac_addr = Common::GenerateMacAddress(Common::MACConsumer::BBA);
    mac_addr_setting = Common::MacAddressToString(mac_addr.value());
    Config::SetBaseOrCurrent(Config::MAIN_BBA_MAC, mac_addr_setting);
    Config::Save();
  }

  switch (type)
  {
  case BBADeviceType::TAP:
    m_network_interface = std::make_unique<TAPNetworkInterface>(this);
    INFO_LOG_FMT(SP1, "Created TAP physical network interface.");
    break;
#if defined(__APPLE__)
  case BBADeviceType::TAPSERVER:
    m_network_interface = std::make_unique<TAPServerNetworkInterface>(this);
    INFO_LOG_FMT(SP1, "Created tapserver physical network interface.");
    break;
#endif
  case BBADeviceType::BuiltIn:
    m_network_interface = std::make_unique<BuiltInBBAInterface>(
        this, Config::Get(Config::MAIN_BBA_BUILTIN_DNS), Config::Get(Config::MAIN_BBA_BUILTIN_IP));
    INFO_LOG_FMT(SP1, "Created Built in network interface.");
    break;
  case BBADeviceType::XLINK:
    // TODO start BBA with network link down, bring it up after "connected" response from XLink

    // Perform sanity check on BBA MAC address, XLink requires the vendor OUI to be Nintendo's and
    // to be one of the two used for the GameCube.
    // Don't actually stop the BBA from initializing though
    if (!mac_addr_setting.starts_with("00:09:bf") && !mac_addr_setting.starts_with("00:17:ab"))
    {
      PanicAlertFmtT(
          "BBA MAC address {0} invalid for XLink Kai. A valid Nintendo GameCube MAC address "
          "must be used. Generate a new MAC address starting with 00:09:bf or 00:17:ab.",
          mac_addr_setting);
    }

    // m_client_mdentifier should be unique per connected emulator from the XLink kai client's
    // perspective so lets use "dolphin<bba mac>"
    m_network_interface =
        std::make_unique<XLinkNetworkInterface>(this, Config::Get(Config::MAIN_BBA_XLINK_IP), 34523,
                                                "dolphin" + Config::Get(Config::MAIN_BBA_MAC),
                                                Config::Get(Config::MAIN_BBA_XLINK_CHAT_OSD));
    INFO_LOG_FMT(SP1, "Created XLink Kai BBA network interface connection to {}:34523",
                 Config::Get(Config::MAIN_BBA_XLINK_IP));
    break;
  }

  tx_fifo = std::make_unique<u8[]>(BBA_TXFIFO_SIZE);
  mBbaMem = std::make_unique<u8[]>(BBA_MEM_SIZE);
  mRecvBuffer = std::make_unique<u8[]>(BBA_RECV_SIZE);

  MXHardReset();

  const auto& mac = mac_addr.value();
  memcpy(&mBbaMem[BBA_NAFR_PAR0], mac.data(), mac.size());

  // HACK: .. fully established 100BASE-T link
  mBbaMem[BBA_NWAYS] = NWAYS_LS100 | NWAYS_LPNWAY | NWAYS_100TXF | NWAYS_ANCLPT;
}

CEXIETHERNET::~CEXIETHERNET()
{
  m_network_interface->Deactivate();
}

void CEXIETHERNET::SetCS(int cs)
{
  if (cs)
  {
    // Invalidate the previous transfer
    transfer.valid = false;
  }
}

bool CEXIETHERNET::IsPresent() const
{
  return true;
}

bool CEXIETHERNET::IsInterruptSet()
{
  return !!(exi_status.interrupt & exi_status.interrupt_mask);
}

void CEXIETHERNET::ImmWrite(u32 data, u32 size)
{
  data >>= (4 - size) * 8;

  if (!transfer.valid)
  {
    transfer.valid = true;
    transfer.region = IsMXCommand(data) ? transfer.MX : transfer.EXI;
    if (transfer.region == transfer.EXI)
      transfer.address = ((data & ~0xc000) >> 8) & 0xff;
    else
      transfer.address = (data >> 8) & 0xffff;
    transfer.direction = IsWriteCommand(data) ? transfer.WRITE : transfer.READ;

    DEBUG_LOG_FMT(SP1, "{} {} {} {:x}", IsMXCommand(data) ? "mx " : "exi",
                  IsWriteCommand(data) ? "write" : "read ", GetRegisterName(), transfer.address);

    if (transfer.address == BBA_IOB && transfer.region == transfer.MX)
    {
      ERROR_LOG_FMT(SP1,
                    "Usage of BBA_IOB indicates that the rx packet descriptor has been corrupted. "
                    "Killing Dolphin...");
      std::exit(0);
    }

    // transfer has been setup
    return;
  }

  // Reach here if we're actually writing data to the EXI or MX region.

  DEBUG_LOG_FMT(SP1, "{} write {:x}", transfer.region == transfer.MX ? "mx " : "exi", data);

  if (transfer.region == transfer.EXI)
  {
    switch (transfer.address)
    {
    case INTERRUPT:
      exi_status.interrupt &= data ^ 0xff;
      // raise back if there is still data
      if (page_ptr(BBA_RRP) != page_ptr(BBA_RWP))
      {
        if (mBbaMem[BBA_IMR] & INT_R)
        {
          mBbaMem[BBA_IR] |= INT_R;

          exi_status.interrupt |= exi_status.TRANSFER;
        }
      }

      break;
    case INTERRUPT_MASK:
      exi_status.interrupt_mask = data;
      break;
    }
    m_system.GetExpansionInterface().UpdateInterrupts();
  }
  else
  {
    MXCommandHandler(data, size);
  }
}

u32 CEXIETHERNET::ImmRead(u32 size)
{
  u32 ret = 0;

  if (transfer.region == transfer.EXI)
  {
    switch (transfer.address)
    {
    case EXI_ID:
      ret = EXI_DEVTYPE_ETHER;
      break;
    case REVISION_ID:
      ret = exi_status.revision_id;
      break;
    case DEVICE_ID:
      ret = exi_status.device_id;
      break;
    case ACSTART:
      ret = exi_status.acstart;
      break;
    case INTERRUPT:
      ret = exi_status.interrupt;
      break;
    }

    transfer.address += size;
  }
  else
  {
    for (int i = size - 1; i >= 0; i--)
      ret |= mBbaMem[transfer.address++] << (i * 8);
  }

  DEBUG_LOG_FMT(SP1, "imm r{}: {:x}", size, ret);

  ret <<= (4 - size) * 8;

  return ret;
}

void CEXIETHERNET::DMAWrite(u32 addr, u32 size)
{
  DEBUG_LOG_FMT(SP1, "DMA write: {:08x} {:x}", addr, size);

  if (transfer.region == transfer.MX && transfer.direction == transfer.WRITE &&
      transfer.address == BBA_WRTXFIFOD)
  {
    auto& memory = m_system.GetMemory();
    DirectFIFOWrite(memory.GetPointer(addr), size);
  }
  else
  {
    ERROR_LOG_FMT(SP1, "DMA write in {} {} mode - not implemented",
                  transfer.region == transfer.EXI ? "exi" : "mx",
                  transfer.direction == transfer.READ ? "read" : "write");
  }
}

void CEXIETHERNET::DMARead(u32 addr, u32 size)
{
  DEBUG_LOG_FMT(SP1, "DMA read: {:08x} {:x}", addr, size);
  auto& memory = m_system.GetMemory();
  memory.CopyToEmu(addr, &mBbaMem[transfer.address], size);
  transfer.address += size;
}

void CEXIETHERNET::DoState(PointerWrap& p)
{
  p.DoArray(tx_fifo.get(), BBA_TXFIFO_SIZE);
  p.DoArray(mBbaMem.get(), BBA_MEM_SIZE);
}

bool CEXIETHERNET::IsMXCommand(u32 const data)
{
  return !!(data & (1 << 31));
}

bool CEXIETHERNET::IsWriteCommand(u32 const data)
{
  return IsMXCommand(data) ? !!(data & (1 << 30)) : !!(data & (1 << 14));
}

const char* CEXIETHERNET::GetRegisterName() const
{
#define STR_RETURN(x)                                                                              \
  case x:                                                                                          \
    return #x;

  if (transfer.region == transfer.EXI)
  {
    switch (transfer.address)
    {
      STR_RETURN(EXI_ID)
      STR_RETURN(REVISION_ID)
      STR_RETURN(INTERRUPT)
      STR_RETURN(INTERRUPT_MASK)
      STR_RETURN(DEVICE_ID)
      STR_RETURN(ACSTART)
      STR_RETURN(HASH_READ)
      STR_RETURN(HASH_WRITE)
      STR_RETURN(HASH_STATUS)
      STR_RETURN(RESET)
    default:
      return "unknown";
    }
  }
  else
  {
    switch (transfer.address)
    {
      STR_RETURN(BBA_NCRA)
      STR_RETURN(BBA_NCRB)
      STR_RETURN(BBA_LTPS)
      STR_RETURN(BBA_LRPS)
      STR_RETURN(BBA_IMR)
      STR_RETURN(BBA_IR)
      STR_RETURN(BBA_BP)
      STR_RETURN(BBA_TLBP)
      STR_RETURN(BBA_TWP)
      STR_RETURN(BBA_IOB)
      STR_RETURN(BBA_TRP)
      STR_RETURN(BBA_RWP)
      STR_RETURN(BBA_RRP)
      STR_RETURN(BBA_RHBP)
      STR_RETURN(BBA_RXINTT)
      STR_RETURN(BBA_NAFR_PAR0)
      STR_RETURN(BBA_NAFR_PAR1)
      STR_RETURN(BBA_NAFR_PAR2)
      STR_RETURN(BBA_NAFR_PAR3)
      STR_RETURN(BBA_NAFR_PAR4)
      STR_RETURN(BBA_NAFR_PAR5)
      STR_RETURN(BBA_NAFR_MAR0)
      STR_RETURN(BBA_NAFR_MAR1)
      STR_RETURN(BBA_NAFR_MAR2)
      STR_RETURN(BBA_NAFR_MAR3)
      STR_RETURN(BBA_NAFR_MAR4)
      STR_RETURN(BBA_NAFR_MAR5)
      STR_RETURN(BBA_NAFR_MAR6)
      STR_RETURN(BBA_NAFR_MAR7)
      STR_RETURN(BBA_NWAYC)
      STR_RETURN(BBA_NWAYS)
      STR_RETURN(BBA_GCA)
      STR_RETURN(BBA_MISC)
      STR_RETURN(BBA_TXFIFOCNT)
      STR_RETURN(BBA_WRTXFIFOD)
      STR_RETURN(BBA_MISC2)
      STR_RETURN(BBA_SI_ACTRL)
      STR_RETURN(BBA_SI_STATUS)
      STR_RETURN(BBA_SI_ACTRL2)
    default:
      if (transfer.address >= 0x100 && transfer.address <= 0xfff)
        return "packet buffer";
      else
        return "unknown";
    }
  }

#undef STR_RETURN
}

void CEXIETHERNET::MXHardReset()
{
  memset(mBbaMem.get(), 0, BBA_MEM_SIZE);

  mBbaMem[BBA_NCRB] = NCRB_PR;
  mBbaMem[BBA_NWAYC] = NWAYC_LTE | NWAYC_ANE;
  mBbaMem[BBA_MISC] = MISC1_TPF | MISC1_TPH | MISC1_TXF | MISC1_TXH;
}

void CEXIETHERNET::MXCommandHandler(u32 data, u32 size)
{
  switch (transfer.address)
  {
  case BBA_NCRA:
    if ((data & NCRA_RESET) != 0)
    {
      INFO_LOG_FMT(SP1, "Software reset");
      // MXSoftReset();
      m_network_interface->Activate();
    }
    if (((mBbaMem[BBA_NCRA] & NCRA_SR) ^ (data & NCRA_SR)) != 0)
    {
      DEBUG_LOG_FMT(SP1, "{} rx", (data & NCRA_SR) ? "start" : "stop");

      if ((data & NCRA_SR) != 0)
        m_network_interface->RecvStart();
      else
        m_network_interface->RecvStop();
    }

    // Only start transfer if there isn't one currently running
    if ((mBbaMem[BBA_NCRA] & (NCRA_ST0 | NCRA_ST1)) == 0)
    {
      // Technically transfer DMA status is kept in TXDMA - not implemented

      if ((data & NCRA_ST0) != 0)
      {
        INFO_LOG_FMT(SP1, "start tx - local DMA");
        SendFromPacketBuffer();
      }
      else if ((data & NCRA_ST1) != 0)
      {
        DEBUG_LOG_FMT(SP1, "start tx - direct FIFO");
        SendFromDirectFIFO();
        // Kind of a hack: send completes instantly, so we don't
        // actually write the "send in status" bit to the register
        data &= ~NCRA_ST1;
      }
    }
    goto write_to_register;

  case BBA_WRTXFIFOD:
    if (size == 2)
      data = Common::swap16(data & 0xffff);
    else if (size == 3)
      data = Common::swap32(data & 0xffffff) >> 8;
    else if (size == 4)
      data = Common::swap32(data);
    DirectFIFOWrite((u8*)&data, size);
    // Do not increment address
    return;

  case BBA_IR:
    data &= (data & 0xff) ^ 0xff;
    goto write_to_register;

  case BBA_TXFIFOCNT:
  case BBA_TXFIFOCNT + 1:
    // Ignore all writes to BBA_TXFIFOCNT
    transfer.address += size;
    return;

  write_to_register:
  default:
    for (int i = size - 1; i >= 0; i--)
    {
      mBbaMem[transfer.address++] = (data >> (i * 8)) & 0xff;
    }
    return;
  }
}

void CEXIETHERNET::DirectFIFOWrite(const u8* data, u32 size)
{
  // In direct mode, the hardware handles creating the state required by the
  // GMAC instead of finagling with packet descriptors and such
  u16* tx_fifo_count = (u16*)&mBbaMem[BBA_TXFIFOCNT];

  memcpy(tx_fifo.get() + *tx_fifo_count, data, size);

  *tx_fifo_count += size;
  // TODO: not sure this mask is correct.
  // However, BBA_TXFIFOCNT should never get even close to this amount,
  // so it shouldn't matter
  *tx_fifo_count &= (1 << 12) - 1;
}

void CEXIETHERNET::SendFromDirectFIFO()
{
  const u8* frame = tx_fifo.get();
  const u16 size = Common::BitCastPtr<u16>(&mBbaMem[BBA_TXFIFOCNT]);
  if (m_network_interface->SendFrame(frame, size))
    m_system.GetPowerPC().GetDebugInterface().NetworkLogger()->LogBBA(frame, size);
}

void CEXIETHERNET::SendFromPacketBuffer()
{
  ERROR_LOG_FMT(SP1, "tx packet buffer not implemented.");
}

void CEXIETHERNET::SendComplete()
{
  mBbaMem[BBA_NCRA] &= ~(NCRA_ST0 | NCRA_ST1);
  *(u16*)&mBbaMem[BBA_TXFIFOCNT] = 0;

  if (mBbaMem[BBA_IMR] & INT_T)
  {
    mBbaMem[BBA_IR] |= INT_T;

    exi_status.interrupt |= exi_status.TRANSFER;
    m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::CPU, 0);
  }

  mBbaMem[BBA_LTPS] = 0;
}

inline u8 CEXIETHERNET::HashIndex(const u8* dest_eth_addr)
{
  // Calculate CRC
  u32 crc = 0xffffffff;

  for (size_t byte_num = 0; byte_num < 6; ++byte_num)
  {
    u8 cur_byte = dest_eth_addr[byte_num];
    for (size_t bit = 0; bit < 8; ++bit)
    {
      u8 carry = ((crc >> 31) & 1) ^ (cur_byte & 1);
      crc <<= 1;
      cur_byte >>= 1;
      if (carry)
        crc = (crc ^ 0x4c11db6) | carry;
    }
  }

  // return bits used as index
  return crc >> 26;
}

inline bool CEXIETHERNET::RecvMACFilter()
{
  static u8 const broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  // Accept all destination addrs?
  if (mBbaMem[BBA_NCRB] & NCRB_PR)
    return true;

  // Unicast?
  if ((mRecvBuffer[0] & 0x01) == 0)
  {
    return memcmp(mRecvBuffer.get(), &mBbaMem[BBA_NAFR_PAR0], 6) == 0;
  }
  else if (memcmp(mRecvBuffer.get(), broadcast, 6) == 0)
  {
    // Accept broadcast?
    return !!(mBbaMem[BBA_NCRB] & NCRB_AB);
  }
  else if (mBbaMem[BBA_NCRB] & NCRB_PM)
  {
    // Accept all multicast
    return true;
  }
  else
  {
    // Lookup the dest eth address in the hashmap
    u16 index = HashIndex(mRecvBuffer.get());
    return !!(mBbaMem[BBA_NAFR_MAR0 + index / 8] & (1 << (index % 8)));
  }
}

inline void CEXIETHERNET::inc_rwp()
{
  u16* rwp = (u16*)&mBbaMem[BBA_RWP];

  if (*rwp == page_ptr(BBA_RHBP))
    *rwp = page_ptr(BBA_BP);
  else
    (*rwp)++;
}

inline void CEXIETHERNET::set_rwp(u16 value)
{
  u16* rwp = (u16*)&mBbaMem[BBA_RWP];
  *rwp = value;
}

// This function is on the critical path for receiving data.
// Be very careful about calling into the logger and other slow things
bool CEXIETHERNET::RecvHandlePacket()
{
  u8* write_ptr;
  Descriptor* descriptor;
  u32 status = 0;
  u16 rwp_initial = page_ptr(BBA_RWP);
  u16 current_rwp = 0;
  u32 off = 4;
  if (!RecvMACFilter())
    goto wait_for_next;

#ifdef BBA_TRACK_PAGE_PTRS
  INFO_LOG_FMT(SP1, "RecvHandlePacket {:x}\n{}", mRecvBufferLength,
               ArrayToString(mRecvBuffer.get(), mRecvBufferLength, 16));

  INFO_LOG_FMT(SP1, "{:x} {:x} {:x} {:x}", page_ptr(BBA_BP), page_ptr(BBA_RRP), page_ptr(BBA_RWP),
               page_ptr(BBA_RHBP));
#endif
  m_system.GetPowerPC().GetDebugInterface().NetworkLogger()->LogBBA(mRecvBuffer.get(),
                                                                    mRecvBufferLength);
  write_ptr = &mBbaMem[page_ptr(BBA_RWP) << 8];

  descriptor = (Descriptor*)write_ptr;
  current_rwp = page_ptr(BBA_RWP);
  DEBUG_LOG_FMT(SP1, "Frame recv: {:x}", mRecvBufferLength);
  for (u32 i = 0; i < mRecvBufferLength; i++)
  {
    write_ptr[off] = mRecvBuffer[i];
    off++;
    if (off == 0x100)
    {
      off = 0;
      // avoid increasing the BBA register while copying
      // sometime the OS can try to process when it's not completed
      current_rwp = current_rwp == page_ptr(BBA_RHBP) ? page_ptr(BBA_BP) : current_rwp + 1;

      write_ptr = &mBbaMem[current_rwp << 8];

      if (page_ptr(BBA_RRP) == current_rwp)
      {
        /*
        halt copy
        if (cur_packet_size >= PAGE_SIZE)
          desc.status |= FO | BF
        if (RBFIM)
          raise RBFI
        if (AUTORCVR)
          discard bad packet
        else
          inc MPC instead of receiving packets
        */
        status |= DESC_FO | DESC_BF;
        mBbaMem[BBA_IR] |= mBbaMem[BBA_IMR] & INT_RBF;
        break;
      }
    }
  }

  // Align up to next page
  if ((mRecvBufferLength + 4) % 256)
    current_rwp = current_rwp == page_ptr(BBA_RHBP) ? page_ptr(BBA_BP) : current_rwp + 1;

#ifdef BBA_TRACK_PAGE_PTRS
  INFO_LOG_FMT(SP1, "{:x} {:x} {:x} {:x}", page_ptr(BBA_BP), page_ptr(BBA_RRP), page_ptr(BBA_RWP),
               page_ptr(BBA_RHBP));
#endif

  // Is the current frame multicast?
  if ((mRecvBuffer[0] & 1) != 0)
    status |= DESC_MF;

  if ((status & DESC_BF) != 0)
  {
    if ((mBbaMem[BBA_MISC2] & MISC2_AUTORCVR) != 0)
    {
      *(u16*)&mBbaMem[BBA_RWP] = rwp_initial;
    }
    else
    {
      ERROR_LOG_FMT(SP1, "RBF while AUTORCVR == 0!");
    }
  }

  descriptor->set(current_rwp, 4 + mRecvBufferLength, status);

  set_rwp(current_rwp);

  mBbaMem[BBA_LRPS] = status;

  // Raise interrupt
  if ((mBbaMem[BBA_IMR] & INT_R) != 0)
  {
    mBbaMem[BBA_IR] |= INT_R;

    exi_status.interrupt |= exi_status.TRANSFER;
    m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::NON_CPU, 0);
  }
  else
  {
    // This occurs if software is still processing the last raised recv interrupt
    WARN_LOG_FMT(SP1, "NOT raising recv interrupt");
  }

wait_for_next:
  if ((mBbaMem[BBA_NCRA] & NCRA_SR) != 0)
    m_network_interface->RecvStart();

  return true;
}
}  // namespace ExpansionInterface
