#pragma once

#include <string>
#include <map>
#include <Core/NetPlayProto.h>

class Metadata
{
public:
  static std::string getJSONString();
  static void writeJSON(std::string jsonString, bool callBatch = false);
  static void setMatchMetadata(tm* matchDateTimeParam);
  static void setPlayerName(std::string playerNameParam);
  static void setMD5(std::array<u8, 16> md5Param);

  // CONSTANTS

  static const u32 addressControllerPort1 = 0x81536A04;
  static const u32 addressControllerPort2 = 0x81536A06;
  static const u32 addressControllerPort3 = 0x81536A08;
  static const u32 addressControllerPort4 = 0x81536A0A;

  static const u32 addressLeftSideCaptainID = 0x815369f0;
  static const u32 addressRightSideCaptainID = 0x815369f4;
  static const u32 addressLeftSideSidekickID = 0x815369f8;
  static const u32 addressRightSideSidekickID = 0x815369fc;
  static const u32 addressStadiumID = 0x81536a00;

  //left team
  static const u32 addressLeftSideScore = 0x81536a56;
  static const u32 addressLeftSideShots = 0x81536a52;
  static const u32 addressLeftSideHits = 0x81536a6c;
  static const u32 addressLeftSideSteals = 0x81536a6e;
  static const u32 addressLeftSideSuperStrikes = 0x81536af2;
  static const u32 addressLeftSidePerfectPasses = 0x81536a74;

  //right team
  static const u32 addressRightSideScore = 0x81536a58;
  static const u32 addressRightSideShots = 0x81536a92;
  static const u32 addressRightSideHits = 0x81536aac;
  static const u32 addressRightSideSteals = 0x81536aae;
  static const u32 addressRightSideSuperStrikes = 0x81536B32;
  static const u32 addressRightSidePerfectPasses = 0x81536ab4;

  //ruleset
  /*
  81534c68 is 4:3/16:9
  81534c6c is difficulty
  81534c70 is amount of time for game in hex
  81534c74 is power ups on/off
  81534c75 is super strike on/off
  81534c77 is rumble on/off
  81531d76 is bowser attack on/off (81534c76 also is)
  */
  static const u32 addressMatchDifficulty = 0x81534c6c;
  // using custom time allotted instead. this one is what we see in the hud as opposed to ruleset
  static const u32 addressMatchTimeAllotted = 0x80400008;
  static const u32 addressOvertimeNotReachedBool = 0x80400002;
  // 0x80400003 is if grudge or not
  static const u32 addressTimeElapsed = 0x80400004;
  static const u32 addressMatchItemsBool = 0x81534c74;
  static const u32 addressMatchSuperStrikesBool = 0x81534c75;
  // note, this is same address for first to 7 so need to know if we're on citrus via hash
  static const u32 addressMatchBowserBool = 0x81534c76;

  //stat for item use
};
