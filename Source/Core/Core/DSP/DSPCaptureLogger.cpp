// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPCaptureLogger.h"

#include <cstring>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/PcapFile.h"

namespace DSP
{
// Definition of the packet structures stored in PCAP capture files.

const u8 IFX_ACCESS_PACKET_MAGIC = 0;
const u8 DMA_PACKET_MAGIC = 1;

#pragma pack(push, 1)
struct IFXAccessPacket
{
  u8 magic;    // IFX_ACCESS_PACKET_MAGIC
  u8 is_read;  // 0 for writes, 1 for reads.
  u16 address;
  u16 value;
};

// Followed by the bytes of the DMA.
struct DMAPacket
{
  u8 magic;         // DMA_PACKET_MAGIC
  u16 dma_control;  // Value of the DMA control register.
  u32 gc_address;   // Address in the GC RAM.
  u16 dsp_address;  // Address in the DSP RAM.
  u16 length;       // Length in bytes.
};
#pragma pack(pop)

PCAPDSPCaptureLogger::PCAPDSPCaptureLogger(const std::string& pcap_filename)
    : m_pcap(new Common::PCAP(new File::IOFile(pcap_filename, "wb")))
{
}

PCAPDSPCaptureLogger::PCAPDSPCaptureLogger(Common::PCAP* pcap) : m_pcap(pcap)
{
}

PCAPDSPCaptureLogger::PCAPDSPCaptureLogger(std::unique_ptr<Common::PCAP>&& pcap)
    : m_pcap(std::move(pcap))
{
}

void PCAPDSPCaptureLogger::LogIFXAccess(bool read, u16 address, u16 value)
{
  IFXAccessPacket pkt;
  pkt.magic = IFX_ACCESS_PACKET_MAGIC;
  pkt.is_read = !!read;  // Make sure we actually have 0/1.
  pkt.address = address;
  pkt.value = value;

  m_pcap->AddPacket(pkt);
}

void PCAPDSPCaptureLogger::LogDMA(u16 control, u32 gc_address, u16 dsp_address, u16 length,
                                  const u8* data)
{
  // The length of a DMA cannot be above 64K, so we use a static buffer for
  // the construction of the packet.
  static u8 buffer[0x10000];

  DMAPacket* pkt = reinterpret_cast<DMAPacket*>(&buffer[0]);
  pkt->magic = DMA_PACKET_MAGIC;
  pkt->dma_control = control;
  pkt->gc_address = gc_address;
  pkt->dsp_address = dsp_address;
  pkt->length = length;
  memcpy(&buffer[sizeof(DMAPacket)], data, length);

  m_pcap->AddPacket(buffer, sizeof(DMAPacket) + length);
}
}  // namespace DSP
