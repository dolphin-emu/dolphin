#pragma once

#include <string>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include "Core/HW/Memmap.h"

class DefaultGeckoCodes {
  public:
    void RunCodeInject(bool bNetplayEventCode, bool bIsRanked, bool bUseNightStadium);

  private:

    static const u32 aControllerRumble = 0x80366177;
    static const u32 aBootToMainMenu = 0x8063F964;
    static const u32 aSkipMemCardCheck = 0x803C50E8;
    static const u32 aUnlimitedExtraInnings = 0x80699A10;

    static const u32 aUnlockEverything_1 = 0x800E870E;  // 0x0 loops of 0x2
    static const u32 aUnlockEverything_2 = 0x800E8710;  // 0x5 loops of 0x3
    static const u32 aUnlockEverything_3 = 0x800E8716;  // 0x5 loops of 0x1
    static const u32 aUnlockEverything_4 = 0x80361680;  // 0x29 loops of 0x1
    static const u32 aUnlockEverything_5 = 0x803616B0;  // 0x0 loops of 0x1 
    static const u32 aUnlockEverything_6 = 0x80361B20;  // 0x35 loops of 0x1
    static const u32 aUnlockEverything_7 = 0x80361C04;  // 0x3 loops of 0x1
    static const u32 aUnlockEverything_8 = 0x80361C15;  // 0x0 loops of 0x1

    static const u32 aManualSelect_1 = 0x8023da60; // write to 0
    static const u32 aManualSelect_2 = 0x8023da60;  // serial write 0->8

    static const u32 aPitchClock_1 = 0x806b4490;
    static const u32 aPitchClock_2 = 0x806b42d0;
    static const u32 aPitchClock_3 = 0x806b46b8;

    static const u32 aNeverCull_1 = 0x8001dcf8;
    static const u32 aNeverCull_2 = 0x806aa4e4;
    static const u32 aNeverCull_3 = 0x806ab8b4;


    void InjectNetplayEventCode();
    void AddRankedCodes();


    struct DefaultGeckoCode
    {
      u32 addr;
      u32 conditionalVal;  // set to 0 if no onditional needed; condition is for when we don't want to write at an addr every frame
      std::vector<u32> codeLines;
    };
    // DefaultGeckoCode mGeckoCode;

    // Generate the Game ID at 0x802EBF8C when Start Game is pressed [LittleCoaks]
    const DefaultGeckoCode sGenerateGameID = {
        0x80042CCC, 0,
        {0x3C80802E, 0x6084BF8C, 0x7C6C42E6, 0x90640000, 0x3C80800F}
    };

    // Clear Game ID when exiting mid - game [LittleCoaks]
    const DefaultGeckoCode sClearGameID_1 =
        {0x806ed704, 0,
        {0x981F01D2, 0x3D00802E, 0x6108BF8C, 0x38000000, 0x90080000}
    };

    // Clear Game ID when exiting mid - game [LittleCoaks]
    const DefaultGeckoCode sClearGameID_2 =
        {0x806edf8c, 0,
        {0x981F01D2, 0x3D00802E, 0x6108BF8C, 0x38000000, 0x90080000}
    };

    // Clear Game ID when returning to main menu after game ends [LittleCoaks]
    const DefaultGeckoCode sClearGameID_3 = {
        0x8069AB2C, 0,
        {0x98050125, 0x3E40802E, 0x6252BF8C, 0x38600000, 0x90720000}
    };

    // Clear port info after game ends [LittleCoaks]
    const DefaultGeckoCode sClearPortInfo = {
        0x8063F14C, 0x38600000,
        {0x38600000, 0x3CA0802E, 0x60A5BF90, 0x98650001, 0xB0650002, 0xB0650004}
    };

    // Clear hit result after each pitch [PeacockSlayer]
    const DefaultGeckoCode sClearHitResult = {
        0x806bbf88, 0,
        {0x99090037, 0x3ea08089, 0x62b53baa, 0x99150000, 0x3aa00000}
    };

    // Store Port Info->0x802EBF94 (fielding port) and 0x802EBF95 (batting port) [LittleCoaks]
    const DefaultGeckoCode sClearPortInfo_1 = {
        0x80042CD0, 0,
        {0x3CA0802E, 0x60A5BF91, 0x3C608089, 0x60632ACA, 0x88630000, 0x2C030001, 0x4182000C,
        0x38600001, 0x48000008, 0x38600005, 0x98650000, 0x3C60800E, 0x6063874D, 0x88630000,
        0x38630001, 0x98650001, 0x386001B8}
    };

    // Store Port Info [LittleCoaks]
    const DefaultGeckoCode sClearPortInfo_2 = {
        0x806706B8, 0x3c608089,
        {0x3FE08089, 0x63FF3928, 0x7C04F800, 0x3FE0802E, 0x63FFBF91, 0x41820018, 0x887F0000,
        0x987F0004, 0x887F0001, 0x987F0003, 0x48000014, 0x887F0001, 0x987F0004, 0x887F0000,
        0x987F0003, 0x3C608089}
    };

    // Remember Who Quit; stores the port who quit a game at 0x802EBF93 [LittleCoaks]
    const DefaultGeckoCode sRememberWhoQuit_1 = {
        0x806ed700, 0,
        {0xb08300fe, 0x3e80802e, 0x6294bf93, 0x8ab40001, 0x9ab40000}
    };

    // Remember Who Quit [LittleCoaks]
    const DefaultGeckoCode sRememberWhoQuit_2 = {
        0x806edf88, 0,
        {0xb08300fe, 0x3e80802e, 0x6294bf93, 0x8ab40002, 0x9ab40000}
    };

    // Anti Quick Pitch [PeacockSlayer, LittleCoaks]
    const DefaultGeckoCode sAntiQuickPitch = {
        0x806B406C, 0xa01e0006,
        {0x3DC08089, 0x61CE80DE, 0x89CE0000, 0x2C0E0000, 0x4082001C, 0x38000000,
        0x3DC08089, 0x61CE099D, 0x89CE0000, 0x2C0E0001, 0x41820008, 0xA01E0006}
    };

    // Default Competitive Rules [LittleCoaks]
    const DefaultGeckoCode sDefaultCompetitiveRules = {
        0x80049D08, 0,
        {0x3DC08035, 0x61CE323B, 0x3A000000, 0x7DEE80AE, 0x2C0F0001, 0x4182001C,
        0x3A100001, 0x2C100012, 0x4082FFEC, 0x39C00004, 0x39E00001, 0x4800000C,
        0x39C00002, 0x39E00000, 0x99C3003E, 0x99E30048, 0x99E3004C, 0x3A000001,
        0x9A03003F, 0x3C60803C}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_1 = {
        0x8067AC00, 0xb0050148,
        {0xB0050148, 0x7C0E0378, 0x7C0802A6, 0x90010004, 0x9421FF00, 0xBC610008, 0x3E408023,
        0x3A526D30, 0x2C0E0000, 0x4082000C, 0x99F26D39, 0x48000264, 0x3CC08089, 0x60C6389B,
        0x88C60000, 0x2C060000, 0x40820008, 0x4800024C, 0x3B0000FF, 0x71CF0800, 0x2C0F0800,
        0x40820008, 0x3B000001, 0x71CF0400, 0x2C0F0400, 0x40820008, 0x3B000003, 0x2C1800FF,
        0x41820014, 0x71CF0010, 0x2C0F0010, 0x40820008, 0x3B180001, 0x71CF0020, 0x2C0F0020,
        0x40820008, 0x3B000005, 0x3A8000FF, 0x2C1800FF, 0x40820010, 0x39E00000, 0x99F26D39,
        0x480001E8, 0x89F26D39, 0x2C0F0001, 0x40820008, 0x480001D8, 0x1DF70268, 0x3E008088,
        0x3A2F7A9D, 0x3A317A9E, 0x7E108B78, 0x8A500000, 0x2C12000A, 0x40820008, 0x480001B4,
        0x2C120015, 0x40820008, 0x4800000C, 0x2C12000F, 0x4082000C, 0x3A400000, 0x9A500000,
        0x8A50FF0E, 0x2C120002, 0x40820008, 0x7EF4BB78, 0x3A400001, 0x9A50FF0E, 0x3AF70001,
        0x2C170009, 0x4180FFA0, 0x2C180002, 0x4082000C, 0x3B400005, 0x48000124, 0x2C180004,
        0x4082000C, 0x3B400003, 0x48000114, 0x2C180005, 0x40820074, 0x3B000000, 0x3B400000,
        0x3E408088, 0x6252F368, 0x3E608089, 0x62730B38, 0xC3930000, 0xC3B20000, 0xFFDCE828,
        0xFFDEF02A, 0xFFC0F210, 0xC3930008, 0xC3B20008, 0xFF9CE828, 0xFF9CE02A, 0xFF80E210,
        0xFFDEE02A, 0x2C180000, 0x4182000C, 0xFC0AF040, 0x4180000C, 0xFD40F090, 0x7F1AC378,
        0x3B180001, 0x3A520268, 0x2C180009, 0x4082FFB0, 0x4800009C, 0x2C1400FF, 0x40820024,
        0x2C180001, 0x4082000C, 0x3B400000, 0x48000084, 0x2C180003, 0x4082000C, 0x3B400008,
        0x48000074, 0x3E408023, 0x6252DA60, 0x7E519378, 0x3AE00000, 0x88D20000, 0x7C06A000,
        0x40820024, 0x2C180001, 0x4082000C, 0x3A520001, 0x48000024, 0x2C180003, 0x4082000C,
        0x3A52FFFF, 0x48000014, 0x3A520001, 0x3AF70001, 0x2C170009, 0x4180FFC8, 0x7C128800,
        0x4080000C, 0x3A520009, 0x48000014, 0x3A310009, 0x7C128800, 0x41800008, 0x3A51FFF7,
        0x8B520000, 0x1DFA0268, 0x3E008088, 0x3A2F7A9D, 0x3A317A9E, 0x7E108B78, 0x3AE0000F,
        0x9AF00000, 0x3AE00002, 0x9AF0FF0E, 0x3E008089, 0x62102801, 0x9B500000, 0x3E408023,
        0x3A526D30, 0x39E00001, 0x99F26D39, 0xB8610008, 0x80010104, 0x38210100, 0x7C0803A6}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_2 = {
        0x8069797c, 0x98dd01d3,
        {0x98dd00e1, 0x98dd01d3}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_3 = {
        0x80677950, 0x990a01d3,
        {0x89ca00e1, 0x2c0e0001,
        0x39c00000, 0x41820008,
        0x990a01d3}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_4 = {
        0x806663c8, 0x981c01d3,
        {0x39c00000, 0x1dee0268,
        0x3e008088, 0x3a2f7a9d,
        0x3a317a9e, 0x7e108b78,
        0x3a400000, 0x9a50ff0e,
        0x39ce0001, 0x2c0e0009,
        0x4180ffdc, 0x981c01d3}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_5 = {
        0x802EC000, 0,
        {0x7C0802A6, 0x90010004,
        0x9421FF00, 0xBC610008,
        0x3920000F, 0x3C808089,
        0x60842750, 0x39600000,
        0x38A00000, 0x1CAB0268,
        0x3CC08088, 0x39057A9D,
        0x39087A9E, 0x7CC64378,
        0x8946FF0E, 0x2C0A0002,
        0x40820010, 0xB16400B0,
        0x99260000, 0x48000014,
        0x396B0001, 0x2C0B0009,
        0x4180FFCC, 0xB28400B0,
        0xB8610008, 0x80010104,
        0x38210100, 0x7C0803A6,
        0x4E800020}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_6 = {
        0x80677920, 0xb06600b0,
        {0x7C741B78, 0x7C150378,
        0x7C0802A6, 0x90010004,
        0x9421FF00, 0xBC610008,
        0x3D20802E, 0x6129C000,
        0x7D2903A6, 0x4E800421,
        0xB8610008, 0x80010104,
        0x38210100, 0x7C0803A6,
        0x7EA0AB78}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_7 = {
        0x80672B88, 0xb00300b0,
        {0x7C140378, 0x7C0802A6,
        0x90010004, 0x9421FF00,
        0xBC610008, 0x3D20802E,
        0x6129C000, 0x7D2903A6,
        0x4E800421, 0xB8610008,
        0x80010104, 0x38210100,
        0x7C0803A6}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_8 = {
        0x8067A684, 0xb00300b0,
        {0x7C140378, 0x7C0802A6,
        0x90010004, 0x9421FF00,
        0xBC610008, 0x3D20802E,
        0x6129C000, 0x7D2903A6,
        0x4E800421, 0xB8610008,
        0x80010104, 0x38210100,
        0x7C0803A6}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_9 = {
        0x8067AECC, 0xb01f00b0,
        {0x7C140378, 0x7C0802A6,
        0x90010004, 0x9421FF00,
        0xBC610008, 0x3D20802E,
        0x6129C000, 0x7D2903A6,
        0x4E800421, 0xB8610008,
        0x80010104, 0x38210100,
        0x7C0803A6}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_10 = {
        0x8069628c, 0x881c01d3,
        {0x881c01d3, 0x3de08088,
        0x61eff368, 0x3a0f15a8,
        0x7c1c7800, 0x41800028,
        0x7c1c8000, 0x41810020,
        0x89dc00e1, 0x2c0e0002,
        0x40820014, 0x2c00000a,
        0x4182000c, 0x3800000f,
        0x981c01d3}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_11 = {
        0x80692224, 0x889f01d3,
        {0x889f01d3, 0x3de08088,
        0x61eff368, 0x3a0f15a8,
        0x7c1f7800, 0x41800028,
        0x7c1f8000, 0x41810020,
        0x89df00e1, 0x2c0e0002,
        0x40820014, 0x2c04000a,
        0x4182000c, 0x3880000f,
        0x989f01d3}
    };

    // Manual Fielder Select 3.2 [PeacockSlayers, LittleCoaks]
    const DefaultGeckoCode sManualSelect_12 = {
        0x80685060, 0x890601d3,
        {0x890601d3, 0x3de08088,
        0x61eff368, 0x3a0f15a8,
        0x7c067800, 0x41800028,
        0x7c068000, 0x41810020,
        0x89c600e1, 0x2c0e0002,
        0x40820014, 0x2c08000a,
        0x4182000c, 0x3900000f,
        0x990601d3}
    };

    // Night time Mario Stadium [LittleCoaks]
    const DefaultGeckoCode sNightStadium = {
        0x80650678, 0x98030058,
        {0x98030058, 0x89240009, 0x2C090000, 0x4082000C, 0x3A400001, 0x9A44000A}};

    // Pitch Clock (10 seconds) [LittleCoaks]
    const DefaultGeckoCode sPitchClock = {
        0x806B4070, 0x540005ef,
        {0x3DC08089, 0x61CE0AE0, 0xA1CE0000, 0x2C0E0258, // last 4 bytes of this instruction determine pitch clock length
        0x40820008, 0x38000100, 0x540005EF}
    };

    void WriteAsm(DefaultGeckoCode CodeBlock);
    u32 aWriteAddr;  // address where the first code gets written to

    std::vector<DefaultGeckoCode> sRequiredCodes =
    {sGenerateGameID, sClearGameID_1, sClearGameID_2, sClearGameID_3,
    sClearPortInfo, sClearHitResult, sClearPortInfo_1, sClearPortInfo_2,
    sRememberWhoQuit_1, sRememberWhoQuit_2};

    std::vector<DefaultGeckoCode> sNetplayCodes =
    {sAntiQuickPitch, sDefaultCompetitiveRules, sManualSelect_1, sManualSelect_2,
    sManualSelect_3, sManualSelect_4, sManualSelect_5, sManualSelect_6,
    sManualSelect_7, sManualSelect_8, sManualSelect_9, sManualSelect_10,
    sManualSelect_11, sManualSelect_12};
};
