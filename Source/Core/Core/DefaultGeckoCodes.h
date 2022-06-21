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

  const DefaultGeckoCode replayStart = {
      0x800f83f8,
      0,
      {0x38210010, 0x39c00000, 0x3dc08040, 0x39e00001, 0x99ee0000,
        0x39e00000, 0x99ee0001, 0x99EE0002}};

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

  void WriteAsm(DefaultGeckoCode CodeBlock);
  u32 aWriteAddr;  // address where the first code gets written to

  std::vector<DefaultGeckoCode> sRequiredCodes = {replayStart, replayEnd, replayQuit, replayOvertime, replayGrudgeFlag1, replayRecordTime};

  /*
  std::vector<DefaultGeckoCode> sNetplayCodes = {
      sAntiQuickPitch, sDefaultCompetitiveRules, sManualSelect_1,  sManualSelect_2, sManualSelect_3,
      sManualSelect_4, sManualSelect_5,          sManualSelect_6,  sManualSelect_7, sManualSelect_8,
      sManualSelect_9, sManualSelect_10,         sManualSelect_11, sManualSelect_12};
   */
};
