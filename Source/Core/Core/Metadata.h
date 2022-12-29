#pragma once

#include <string>
#include <map>
#include <Core/NetPlayProto.h>
#include <Core/NetPlayClient.h>

class Metadata
{
public:
  static std::string getJSONString();
  static void writeJSON(std::string jsonString, bool callBatch = true);
  static void setMatchMetadata(tm* matchDateTimeParam);
  static void setPlayerName(std::string playerNameParam);
  static void setPlayerArray(std::vector<const NetPlay::Player*>);
  static void setNetPlayControllers(NetPlay::PadMappingArray m_pad_map);
  static void setMD5(std::array<u8, 16> md5Param);
  static std::vector<const NetPlay::Player*> getPlayerArray();
  static NetPlay::PadMappingArray getControllers();

  /*
  Game Derived Locations (we do not mark these as variables because we do not directly use them here, just in gecko codes)

  80312adc - ball x position
  80312ae0 - ball y position
  80312ae4 - ball z position

  80311010 - ball x velocity
  80311014 - ball y velocity
  80311018 - ball z velocity
  */ 

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

  //stats for item use

  /*
  one byte for item type, one byte for item amount, two bytes filler
  full word (4 bytes) for time
  80410000 for left team item use (start)
  80420000 for right team item use (start)
  80430000 for left team item offset
  80430004 for left team item flag
  80430008 for left team item count
  80430010 for right team item offset
  80430014 for right team item flag
  80430018 for right team item count
  */
  // left team item
  static const u32 addressLeftTeamItemStart = 0x80410000;
  static const u32 addressLeftTeamItemOffset = 0x80430000;
  static const u32 addressLeftTeamGoalOffset = 0x80430004;
  static const u32 addressLeftTeamItemCount = 0x80430008;
  // left team shots
  static const u32 addressLeftTeamMissedShotsOffset = 0x8043000c;
  static const u32 addressLeftTeamMissedShotsFlag = 0x8043000e;
  static const u32 addressLeftTeamMissedShotsBallXPos = 0x80430020;
  static const u32 addressLeftTeamMissedShotsBallYPos = 0x80430024;
  static const u32 addressLeftTeamMissedShotsTimestamp = 0x80430028;
  // to add extra info like ball x/y/z velocity and ball z position
  static const u32 addressLeftTeamMissedShotsVelocityFlag = 0x8043003c;
  static const u32 addressLeftTeamMissedShotsBallZPos = 0x80430040;
  static const u32 addressLeftTeamMissedShotsBallXVel = 0x80430044;
  static const u32 addressLeftTeamMissedShotsBallYVel = 0x80430048;
  static const u32 addressLeftTeamMissedShotsBallZVel = 0x8043004c;
  static const u32 addressLeftTeamMissedShotsBallChargeAmount = 0x80430060;

  // right team item
  static const u32 addressRightTeamItemStart = 0x80420000;
  static const u32 addressRightTeamItemOffset = 0x80430010;
  static const u32 addressRightTeamGoalOffset = 0x80430014;
  static const u32 addressRightTeamItemCount = 0x80430018;
  // right team shots
  static const u32 addressRightTeamMissedShotsOffset = 0x8043001c;
  static const u32 addressRightTeamMissedShotsFlag = 0x8043001e;
  static const u32 addressRightTeamMissedShotsBallXPos = 0x80430030;
  static const u32 addressRightTeamMissedShotsBallYPos = 0x80430034;
  static const u32 addressRightTeamMissedShotsTimestamp = 0x80430038;
  // to add extra info like ball x/y/z velocity and ball z position
  static const u32 addressRightTeamMissedShotsVelocityFlag = 0x8043003e;
  static const u32 addressRightTeamMissedShotsBallZPos = 0x80430050;
  static const u32 addressRightTeamMissedShotsBallXVel = 0x80430054;
  static const u32 addressRightTeamMissedShotsBallYVel = 0x80430058;
  static const u32 addressRightTeamMissedShotsBallZVel = 0x8043005c;
  static const u32 addressRightTeamMissedShotsBallChargeAmount = 0x80430064;

  // charged amount -- shared by left and right
  static const u32 addressChargedBallAmount = 0x80430068;

  // left team goal
  static const u32 addressLeftTeamGoalStart = 0x80440000;
  // right team goal
  static const u32 addressRightTeamGoalStart = 0x80450000;

  // left team missed shots
  static const u32 addressLeftTeamMissedShotsStart = 0x80460000;
  // right team missed shots
  static const u32 addressRightTeamMissedShotsStart = 0x80470000;

  // i thought about putting these next 2 at 8040000c/e and making them half words
  // but if a player got more than ffff, aka 18 minutes, we would experience disparity
  // 
  // left team ball possessed frames
  static const u32 addressLeftTeamBallOwnedFrames = 0x80480000;
  // right team ball possessed frames
  static const u32 addressRightTeamBallOwnedFrames = 0x80480004;
};
