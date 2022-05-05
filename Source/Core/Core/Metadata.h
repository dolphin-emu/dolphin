#pragma once

#include <string>
#include <map>

class Metadata
{
public:
  static std::string getJSONString();
  static void writeJSON(std::string jsonString, bool callBatch = false);
  static void setMatchMetadata(tm* matchDateTimeParam);

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

  //stat for item use
};
