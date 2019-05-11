// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/EXI/EXI_Device.h"

#include <array>

class PointerWrap;

namespace ExpansionInterface
{
// Network Control Register A
enum NCRA
{
  NCRA_RESET = 0x01,  // RESET
  NCRA_ST0 = 0x02,    // Start transmit command/status
  NCRA_ST1 = 0x04,    // "
  NCRA_SR = 0x08      // Start Receive
};

// Network Control Register B
enum NCRB
{
  NCRB_PR = 0x01,     // Promiscuous Mode
  NCRB_CA = 0x02,     // Capture Effect Mode
  NCRB_PM = 0x04,     // Pass Multicast
  NCRB_PB = 0x08,     // Pass Bad Frame
  NCRB_AB = 0x10,     // Accept Broadcast
  NCRB_HBD = 0x20,    // reserved
  NCRB_RXINTC = 0xC0  // Receive Interrupt Counter (mask)
};

// Interrupt Mask Register
// Interrupt Register
enum Interrupts
{
  INT_FRAG = 0x01,      // Fragment Counter
  INT_R = 0x02,         // Receive
  INT_T = 0x04,         // Transmit
  INT_R_ERR = 0x08,     // Receive Error
  INT_T_ERR = 0x10,     // Transmit Error
  INT_FIFO_ERR = 0x20,  // FIFO Error
  INT_BUS_ERR = 0x40,   // BUS Error
  INT_RBF = 0x80        // RX Buffer Full
};

// NWAY Configuration Register
enum NWAYC
{
  NWAYC_FD = 0x01,        // Full Duplex Mode
  NWAYC_PS100_10 = 0x02,  // Port Select 100/10
  NWAYC_ANE = 0x04,       // Autonegotiate enable

  // Autonegotiation status bits...

  NWAYC_NTTEST = 0x40,  // Reserved
  NWAYC_LTE = 0x80      // Link Test Enable
};

enum NWAYS
{
  NWAYS_LS10 = 0x01,
  NWAYS_LS100 = 0x02,
  NWAYS_LPNWAY = 0x04,
  NWAYS_ANCLPT = 0x08,
  NWAYS_100TXF = 0x10,
  NWAYS_100TXH = 0x20,
  NWAYS_10TXF = 0x40,
  NWAYS_10TXH = 0x80
};

enum MISC1
{
  MISC1_BURSTDMA = 0x01,
  MISC1_DISLDMA = 0x02,
  MISC1_TPF = 0x04,
  MISC1_TPH = 0x08,
  MISC1_TXF = 0x10,
  MISC1_TXH = 0x20,
  MISC1_TXFIFORST = 0x40,
  MISC1_RXFIFORST = 0x80
};

enum MISC2
{
  MISC2_HBRLEN0 = 0x01,
  MISC2_HBRLEN1 = 0x02,
  MISC2_RUNTSIZE = 0x04,
  MISC2_DREQBCTRL = 0x08,
  MISC2_RINTSEL = 0x10,
  MISC2_ITPSEL = 0x20,
  MISC2_A11A8EN = 0x40,
  MISC2_AUTORCVR = 0x80
};

enum
{
  BBA_NCRA = 0x00,
  BBA_NCRB = 0x01,

  BBA_LTPS = 0x04,
  BBA_LRPS = 0x05,

  BBA_IMR = 0x08,
  BBA_IR = 0x09,

  BBA_BP = 0x0a,
  BBA_TLBP = 0x0c,
  BBA_TWP = 0x0e,
  BBA_IOB = 0x10,
  BBA_TRP = 0x12,
  BBA_RWP = 0x16,
  BBA_RRP = 0x18,
  BBA_RHBP = 0x1a,

  BBA_RXINTT = 0x14,

  BBA_NAFR_PAR0 = 0x20,
  BBA_NAFR_PAR1 = 0x21,
  BBA_NAFR_PAR2 = 0x22,
  BBA_NAFR_PAR3 = 0x23,
  BBA_NAFR_PAR4 = 0x24,
  BBA_NAFR_PAR5 = 0x25,

  BBA_NAFR_MAR0 = 0x26,
  BBA_NAFR_MAR1 = 0x27,
  BBA_NAFR_MAR2 = 0x28,
  BBA_NAFR_MAR3 = 0x29,
  BBA_NAFR_MAR4 = 0x2a,
  BBA_NAFR_MAR5 = 0x2b,
  BBA_NAFR_MAR6 = 0x2c,
  BBA_NAFR_MAR7 = 0x2d,

  BBA_NWAYC = 0x30,
  BBA_NWAYS = 0x31,

  BBA_GCA = 0x32,

  BBA_MISC = 0x3d,

  BBA_TXFIFOCNT = 0x3e,
  BBA_WRTXFIFOD = 0x48,

  BBA_MISC2 = 0x50,

  BBA_SI_ACTRL = 0x5c,
  BBA_SI_STATUS = 0x5d,
  BBA_SI_ACTRL2 = 0x60
};

enum
{
  EXI_DEVTYPE_ETHER = 0x04020200
};

enum SendStatus
{
  DESC_CC0 = 0x01,
  DESC_CC1 = 0x02,
  DESC_CC2 = 0x04,
  DESC_CC3 = 0x08,
  DESC_CRSLOST = 0x10,
  DESC_UF = 0x20,
  DESC_OWC = 0x40,
  DESC_OWN = 0x80
};

enum RecvStatus
{
  DESC_BF = 0x01,
  DESC_CRC = 0x02,
  DESC_FAE = 0x04,
  DESC_FO = 0x08,
  DESC_RW = 0x10,
  DESC_MF = 0x20,
  DESC_RF = 0x40,
  DESC_RERR = 0x80
};

class CEXIEthernetBase : public IEXIDevice
{
public:
  CEXIEthernetBase();

  void SetCS(int cs) override;
  bool IsPresent() const override;
  bool IsInterruptSet() override;
  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void DMAWrite(u32 addr, u32 size) override;
  void DMARead(u32 addr, u32 size) override;
  void DoState(PointerWrap& p) override;

protected:
  static constexpr std::size_t BBA_RECV_SIZE = 0x800;
  static constexpr std::size_t BBA_NUM_PAGES = 0x10;
  static constexpr std::size_t BBA_PAGE_SIZE = 0x100;
  static constexpr std::size_t BBA_MEM_SIZE = BBA_NUM_PAGES * BBA_PAGE_SIZE;
  static constexpr std::size_t BBA_TXFIFO_SIZE = 1518;

  virtual bool Activate() = 0;
  virtual bool IsActivated() const = 0;
  virtual bool SendFrame(const u8* frame, u32 size) = 0;
  virtual bool RecvInit() = 0;
  virtual void RecvStart() = 0;
  virtual void RecvStop() = 0;

  inline u16 page_ptr(int const index) const
  {
    return ((u16)m_bba_mem.raw[index + 1] << 8) | m_bba_mem.raw[index];
  }

  inline u8* PtrFromPagePtr(int const index)
  {
    return &m_bba_mem.raw[page_ptr(index) << 8];
  }

  inline const u8* PtrFromPagePtr(int const index) const
  {
    return &m_bba_mem.raw[page_ptr(index) << 8];
  }

  bool IsMXCommand(u32 const data);
  bool IsWriteCommand(u32 const data);
  const char* GetRegisterName() const;
  void MXHardReset();
  void MXCommandHandler(u32 data, u32 size);
  void DirectFIFOWrite(const u8* data, u32 size);
  void SendFromDirectFIFO();
  void SendFromPacketBuffer();
  void SendComplete();
  u8 HashIndex(const u8* dest_eth_addr) const;
  bool RecvMACFilter() const;
  void inc_rwp();
  bool RecvHandlePacket();

  union {
    std::array<u8, BBA_MEM_SIZE> raw;
#pragma pack(push, 1)
    struct {
      u8 ncra;        //0x00
      u8 ncrb;        //0x01
      u16 : 16;
      u8 ltps;        //0x04
      u8 lrps;        //0x05
      u16 : 16;
      u8 imr;         //0x08
      u8 ir;          //0x09
      u16 bp;         //0x0a
      u16 tlbp;       //0x0c
      u16 twp;        //0x0e
      u16 iob;        //0x10
      u16 trp;        //0x12
      u16 rxintt;     //0x14
      u16 rwp;        //0x16
      u16 rrp;        //0x18
      u16 rhbp;       //0x1a
      u32 : 32;
      std::array<u8, 6> nafr_par; //0x20
      std::array<u8, 8> nafr_mar; //0x26
      u16 : 16;
      u8 nwayc;       //0x30
      u8 nways;       //0x31
      u8 gca;         //0x32
      u64 : 64;
      u16 : 16;
      u8 misc;        //0x3d
      u16 txfifocnt;  //0x3e
      u64 : 64;
      u16 wrtxfifod;  //0x48
      u64 : 48;
      u8 misc2;       //0x50
      u64 : 64;
      u16 : 16;
      u8 : 8;
      u8 si_actrl;    //0x5c
      u8 si_status;   //0x5d
      u16 : 16;
      u8 si_actrl2;   //0x60
    };
#pragma pack(pop)
  } m_bba_mem;
  std::array<u8, BBA_TXFIFO_SIZE> m_tx_fifo;

  std::array<u8, BBA_RECV_SIZE> m_recv_buffer;
  u32 m_recv_buffer_length = 0;

  struct
  {
    enum
    {
      READ,
      WRITE
    } direction;

    enum
    {
      EXI,
      MX
    } region;

    u16 address;
    bool valid;
  } transfer = {};

  enum
  {
    EXI_ID,
    REVISION_ID,
    INTERRUPT_MASK,
    INTERRUPT,
    DEVICE_ID,
    ACSTART,
    HASH_READ = 8,
    HASH_WRITE,
    HASH_STATUS = 0xb,
    RESET = 0xf
  };

  // exi regs
  struct EXIStatus
  {
    enum
    {
      TRANSFER = 0x80
    };

    u8 revision_id = 0;  // 0xf0
    u8 interrupt_mask = 0;
    u8 interrupt = 0;
    u16 device_id = 0xD107;
    u8 acstart = 0x4E;
    u32 hash_challenge = 0;
    u32 hash_response = 0;
    u8 hash_status = 0;
  } exi_status;

  struct Descriptor
  {
    u32 word;

    inline void set(u32 const next_page, u32 const packet_length, u32 const status)
    {
      word = 0;
      word |= (status & 0xff) << 24;
      word |= (packet_length & 0xfff) << 12;
      word |= next_page & 0xfff;
    }
  };
};
}  // namespace ExpansionInterface
