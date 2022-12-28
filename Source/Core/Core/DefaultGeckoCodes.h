#pragma once

// This Class and its ideas are property of LittleCoaks and PeacockSlayer
// Their implementation of it comes from Project Rio, whom's website and source code can be found below at:
// https://www.projectrio.online/
// https://github.com/ProjectRio/ProjectRio

#include <array>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "Core/HW/Memmap.h"

class DefaultGeckoCodes
{
public:
  void RunCodeInject(bool bIsRanked);

private:

  void InjectNetplayEventCode();
  void AddRankedCodes();

  struct DefaultGeckoCode
  {
    u32 addr;
    u32 conditionalVal;  // set to 0 if no onditional needed; condition is for when we don't want to
                         // write at an addr every frame
    std::vector<u32> codeLines;
  };

  // gameplay settings
  const DefaultGeckoCode netplayCommunitySettings = {
      0x800963bc,
      0,
      {0x3860002c, 0x3dc08153, 0x39ce4c6c, 0x39e00001, 0x99eefffc, 0x39e00004, 0x91ee0000,
       0x39e0012c, 0x91ee0004, 0x39e00001, 0x99ee0008, 0x39e00000, 0x99ee0009, 0x99ee000a}};

  const DefaultGeckoCode replayStart = {
      0x800f83f8, 0, {0x38210010, 0x3DC08040, 0x89EE0000, 0x2C0F0000, 0x40820050,
                      0x39E00001, 0x99EE0000, 0x39E00000, 0x99EE0001, 0x99EE0002,
                      0x3DC08043, 0x91EE0000, 0x91EE0004, 0x91EE0008, 0x91EE000C,
                      0x91EE003C, 0x91EE0010, 0x91EE0014, 0x91EE0018, 0x91EE001C,
                      0x91EE003E, 0x3DC08048, 0x91EE0000, 0x91EE0004, 0x60000000}};

  const DefaultGeckoCode replayEnd = {0x80100e14,
                                      0,
                                      {0x9421ff70, 0x39c00000, 0x3dc08040, 0x39ce0001, 0x39e00001,
                                       0x99ee0000, 0x39e00000, 0x99eeffff}};

  const DefaultGeckoCode replayQuit = {0x80096898,
                                       0,
                                       {0x3860001c, 0x39c00000, 0x3dc08040, 0x39ce0001, 0x39e00001,
                                        0x99ee0000, 0x39e00000, 0x99eeffff, 0x99EE0002}};

  const DefaultGeckoCode replayOvertime = {
      0x8003CF68, 0, {0x3C80802A, 0x3DC08040, 0x39E00001, 0x99EE0002, 0x39E00000}};

  const DefaultGeckoCode replayGrudgeFlag1 = {
      0x800d8c80, 0, {0x7fa4eb78, 0x3dc08040, 0x39e00001, 0x99ee0003}};

  const DefaultGeckoCode replayRecordTime = {
      0x800f90b0, 0, {0x3dc08040, 0x81e30008, 0x91ee0004, 0x806da6c8}};

  const DefaultGeckoCode replayTimeAllottedHUD = {
      0x8004149c, 0, {0x9001001c, 0x3dc08040, 0x906e0008}};

  const DefaultGeckoCode recordItemUse = {
      0x80065F68,
      0,
                                          {0x38A00000, 0x3DC08037, 0x39CE1238, 0x81EE0000, 0x7C037800, 0x40820060, 0x3DC08043, 0x81EE0000,
                                           0x3DC08041, 0x82030044, 0x7E0F71AE, 0x82030048, 0x39EF0001, 0x7E0F71AE, 0x39EF0003, 0x3DC08037, 0x39CE3708, 0x820E0000, 0x8210000C, 0x82100008, 0x3DC08041, 0x7E0F712E, 0x39EF0004, 0x3DC08043,
                      0x91EE0000, 0x81EE0008, 0x39EF0001, 0x91EE0008, 0x48000064, 0x3DC08043,
                      0x39CE0010, 0x81EE0000, 0x3DC08042, 0x82030044, 0x7E0F71AE, 0x82030048,
                      0x39EF0001, 0x7E0F71AE, 0x39EF0003, 0x3DC08037, 0x39CE3708, 0x820E0000,
                      0x8210000C, 0x82100008, 0x3DC08042, 0x7E0F712E, 0x39EF0004, 0x3DC08043,
                      0x39CE0010, 0x91EE0000, 0x81EE0008, 0x39EF0001, 0x91EE0008}};

  const DefaultGeckoCode recordGoalTimestamp = {
      0x8003d5b0, 0, {0x9003003C, 0x3DC08037, 0x39CE1238, 0x81EE0000, 0x3DC08037, 0x39CE3708,
                      0x820E0000, 0x8210000C, 0x82100008, 0x3E208043, 0x7C037800, 0x40820044,
                      0x3DC08043, 0x81EE0004, 0x3DC08044, 0x7E0F712E, 0x39EF0004, 0x82110020,
                      0x7E0F712E, 0x39EF0004, 0x82110024, 0x7E0F712E, 0x39EF0004, 0x3DC08043,
                      0x91EE0004, 0x39E00000, 0xB1EE000E, 0x48000040, 0x3DC08043, 0x81EE0014,
                      0x3DC08045, 0x7E0F712E, 0x39EF0004, 0x82110030, 0x7E0F712E, 0x39EF0004,
                      0x82110034, 0x7E0F712E, 0x39EF0004, 0x3DC08043, 0x91EE0014, 0x39E00000,
                      0xB1EE001E}};
  const DefaultGeckoCode recordMissedShots = {
      0x80183a84,
      0,
      {0xB0060014, 0x3DC08153, 0x39CE6A40, 0x7C0E3000, 0x3DC08043, 0x40820010, 0xA1EE000E,
       0x3A000000, 0x4800000C, 0xA1EE001E, 0x3A000001, 0x2C0F0001, 0x40820074, 0x3DC08043,
       0x2C100000, 0x40820038, 0xA1EE000C, 0x3E208046, 0x820E0028, 0x7E0F892E, 0x39EF0004,
       0x820E0020, 0x7E0F892E, 0x39EF0004, 0x820E0024, 0x7E0F892E, 0x39EF0004, 0xB1EE000C,
       0x48000034, 0xA1EE001C, 0x3E208047, 0x820E0038, 0x7E0F892E, 0x39EF0004, 0x820E0030,
       0x7E0F892E, 0x39EF0004, 0x820E0034, 0x7E0F892E, 0x39EF0004, 0xB1EE001C, 0x3DC08153,
       0x39CE6A40, 0x7C0E3000, 0x3DC08043, 0x40820044, 0x39E00001, 0xB1EE000E, 0x3DC08031,
       0x39CE2ADC, 0x81EE0000, 0x820E0004, 0x3DC08043, 0x91EE0020, 0x920E0024, 0x3E208037,
       0x3A313708, 0x82110000, 0x8210000C, 0x82100008, 0x920E0028, 0x48000040, 0x39E00001,
       0xB1EE001E, 0x3DC08031, 0x39CE2ADC, 0x81EE0000, 0x820E0004, 0x3DC08043, 0x91EE0030,
       0x920E0034, 0x3E208037, 0x3A313708, 0x82110000, 0x8210000C, 0x82100008, 0x920E0038}};

  const DefaultGeckoCode recordBallOwnership = {0x800f90a8,
                                                0,
    {0x8B8404C1, 0x8063000C, 0x81E30008, 0x3DC08040, 0x820E0004, 0x7C0F8000,
    0x41820050, 0x3DC08031, 0x39CE1009, 0x89EE0000, 0x2C0F0003, 0x3E008048,
    0x41810014, 0x81F00000, 0x39EF0001, 0x91F00000, 0x48000028, 0x2C0F0008,
    0x40820014, 0x81F00000, 0x39EF0001, 0x91F00000, 0x48000010, 0x81F00004,
    0x39EF0001, 0x91F00004, 0x806DA6C8}};

  void WriteAsm(DefaultGeckoCode CodeBlock);
  u32 aWriteAddr;  // address where the first code gets written to

  std::vector<DefaultGeckoCode> sRequiredCodes = {
      replayStart,
      replayEnd,        replayQuit,           replayOvertime,        replayGrudgeFlag1, replayRecordTime,
      replayTimeAllottedHUD, recordItemUse,      recordGoalTimestamp,
      recordMissedShots,
      recordBallOwnership};

  /*
  std::vector<DefaultGeckoCode> sNetplayCodes = {
      sAntiQuickPitch, sDefaultCompetitiveRules, sManualSelect_1,  sManualSelect_2, sManualSelect_3,
      sManualSelect_4, sManualSelect_5,          sManualSelect_6,  sManualSelect_7, sManualSelect_8,
      sManualSelect_9, sManualSelect_10,         sManualSelect_11, sManualSelect_12};
   */
};
