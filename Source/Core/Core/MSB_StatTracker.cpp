
#include "Core/MSB_StatTracker.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>

//For LocalPLayers
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Common/Swap.h"

void StatTracker::Run(){
    lookForTriggerEvents();
}

void StatTracker::lookForTriggerEvents(){
    //At Bat - Need RosterID (FOUND) and Team1 vs Team2 (MIA)
    if (m_game_state == GAME_STATE::INGAME){
        switch(m_ab_state){
            //Look for Pitch
            case (AB_STATE::WAITING_FOR_PITCH):
                //Collect port info for players
                if (m_game_info.team0_port == 0 && m_game_info.team1_port == 0){
                    m_game_info.team0_port = 1; //Team0 is always P1/Port 1

                    u8 fielder_port = Memory::Read_U8(aAB_FieldingPort);
                    u8 batter_port = Memory::Read_U8(aAB_BattingPort);
                    if (fielder_port > 1 && fielder_port <= 4){
                        m_game_info.team1_port = fielder_port;
                    }
                    else if (batter_port > 1 && batter_port <= 4){
                        m_game_info.team1_port = batter_port;
                    }
                    else{
                        m_game_info.team1_port = 0;
                        //TODO - Disable stat submission here
                    }

                    //TODO - Call with correct netplay/local status 
                    std::cout << "Assigning Players to teams" << std::endl;
                    readPlayers(true);
                }

                if (Memory::Read_U8(aAB_PitchThrown) == 1){
                    std::cout << "Pitch detected!" << std::endl;

                    logABScenario(); //Inning, Order, 
                    m_ab_state = AB_STATE::PITCH_STARTED;
                }
                break;
            case (AB_STATE::PITCH_STARTED): //Look for contact or end of pitch
                if ((Memory::Read_U8(aAB_HitByPitch) == 1) || (Memory::Read_U8(aAB_PitchThrown) == 0)){
                    logABMiss(); //Strike or Swing or Bunt
                    logABPitch();
                    m_ab_state = AB_STATE::PLAY_OVER;
                }
                //Ball X,Y,Z Velocity is only set on contact
                else if (Memory::Read_U32(aAB_BallVel_X) != 0){
                    logABContact(); //Veolcity, Part of Bat, Charge, Charge Power, Star, Moonshot
                    m_ab_state = AB_STATE::CONTACT;
                }
                //Else just keep looking for contact or end of pitch
                break;
            case (AB_STATE::CONTACT):
                if (Memory::Read_U8(aAB_ContactResult) != 0){
                    logABContactResult(); //Land vs Caught vs Foul, Landing POS.
                    m_ab_state = AB_STATE::PLAY_OVER;
                }
                break;
            case (AB_STATE::PLAY_OVER):
                if (Memory::Read_U8(aAB_PitchThrown) == 0){
                    m_curr_ab_stat.rbi = Memory::Read_U8(aAB_RBI);
                    m_ab_state = AB_STATE::FINAL_RESULT;
                    std::cout << "Play over. Logging final results next frame." << std::endl;
                    std::cout << std::endl;
                }
                break;
            case (AB_STATE::FINAL_RESULT):
                
                //Final Stats - Collected 1 frame after aAB_PitchThrown==0
                m_curr_ab_stat.num_outs = Memory::Read_U8(aAB_NumOutsDuringPlay);
                m_curr_ab_stat.result_game = Memory::Read_U8(aAB_FinalResult);

                //Store current AB stat to the players vector
                m_ab_stats[m_curr_ab_stat.team_id][m_curr_ab_stat.roster_id].push_back(m_curr_ab_stat);
                m_curr_ab_stat = ABStats();

                
                m_ab_state = AB_STATE::WAITING_FOR_PITCH;
                std::cout << "Final Result. Logging AB. Waiting for next pitch..." << std::endl;
                std::cout << std::endl;
                break;
            default:
                std::cout << "Unknown State" << std::endl;
                m_ab_state = AB_STATE::WAITING_FOR_PITCH;
                break;
        }
    }

    //End Of Game
    switch (m_game_state){
        case (GAME_STATE::PREGAME):
            //Start recording when GameId is set AND record button is pressed
            if ((Memory::Read_U32(aGameId) != 0) && (mTrackerInfo.mRecord)){
                m_game_id = Memory::Read_U32(aGameId);
                m_game_state = GAME_STATE::INGAME;
                m_game_info.ranked = mCurrentRankedStatus;

                std::cout << "PREGAME->INGAME (GameID=" << std::to_string(m_game_id) << ")" << std::endl;
            }
            break;
        case (GAME_STATE::INGAME):
            if ((Memory::Read_U8(aEndOfGameFlag) == 1) && (m_ab_state == AB_STATE::WAITING_FOR_PITCH) ){
                logGameInfo();
                printStatsToFile();
                m_game_state = GAME_STATE::ENDGAME_LOGGED;

                std::cout << "INGAME->ENDGAME" << std::endl;
            }
            break;
        case (GAME_STATE::ENDGAME_LOGGED):
            if (Memory::Read_U32(aGameId) == 0){
                m_game_state = GAME_STATE::PREGAME;
                init();

                std::cout << "ENDGAME->PREGAME" << std::endl;
            }
            break;
    }
}

void StatTracker::logGameInfo(){
    std::cout << "Logging EngGame Stats" << std::endl;

    time_t now = time(0);
    //char* date_time = ctime(&now);
    tm *gmtm = gmtime(&now);
    char* dt = asctime(gmtm);
    m_game_info.date_time = std::string(dt);
    m_game_info.date_time.pop_back();
    m_game_info.ranked = 0;

    m_game_info.team0_captain = Memory::Read_U8(aTeam0_Captain);
    m_game_info.team1_captain = Memory::Read_U8(aTeam1_Captain);

    m_game_info.team0_score = Memory::Read_U16(aTeam0_Score);
    m_game_info.team1_score = Memory::Read_U16(aTeam1_Score);

    for (int team=0; team < cNumOfTeams; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){
            u32 offset = ((team * cRosterSize * c_roster_table_offset)) + (roster * c_roster_table_offset);
            m_game_info.rosters_char_id[team][roster] =  Memory::Read_U8(aTeam0_RosterCharId_Start + offset);

            logDefensiveStats(team, roster);
            logOffensiveStats(team, roster);
        }
    }
}

void StatTracker::logDefensiveStats(int team_id, int roster_id){
    EndGameRosterDefensiveStats& stat = m_defensive_stats[team_id][roster_id];
    u32 offset = (team_id * cRosterSize * c_defensive_stat_offset) + (roster_id * c_defensive_stat_offset);

    u32 is_starred_offset = (team_id * cRosterSize) + roster_id;

    stat.game_id    = m_game_id;
    stat.team_id    = team_id;
    stat.roster_id  = roster_id;
    stat.is_starred = Memory::Read_U8(aPitcher_IsStarred + is_starred_offset);

    stat.batters_faced       = Memory::Read_U8(aPitcher_BattersFaced + offset);
    stat.runs_allowed        = Memory::Read_U16(aPitcher_RunsAllowed + offset);
    stat.batters_walked      = Memory::Read_U16(aPitcher_BattersWalked + offset);
    stat.batters_hit         = Memory::Read_U16(aPitcher_BattersHit + offset);
    stat.hits_allowed        = Memory::Read_U16(aPitcher_HitsAllowed + offset);
    stat.homeruns_allowed    = Memory::Read_U16(aPitcher_HRsAllowed + offset);
    stat.pitches_thrown      = Memory::Read_U16(aPitcher_PitchesThrown + offset);
    stat.stamina             = Memory::Read_U16(aPitcher_Stamina + offset);
    stat.was_pitcher         = Memory::Read_U8(aPitcher_WasPitcher + offset);
    stat.batter_outs         = Memory::Read_U8(aPitcher_BatterOuts + offset);
    stat.strike_outs         = Memory::Read_U8(aPitcher_StrikeOuts + offset);
    stat.star_pitches_thrown = Memory::Read_U8(aPitcher_StarPitchesThrown + offset);
}

void StatTracker::logOffensiveStats(int team_id, int roster_id){
    EndGameRosterOffensiveStats& stat = m_offensive_stats[team_id][roster_id];
    u32 offset = ((team_id * cRosterSize * c_offensive_stat_offset)) + (roster_id * c_offensive_stat_offset);

    stat.game_id   = m_game_id;
    stat.team_id   = team_id;
    stat.roster_id = roster_id;

    stat.at_bats      = Memory::Read_U8(aBatter_AtBats + offset);
    stat.hits         = Memory::Read_U8(aBatter_Hits + offset);
    stat.singles      = Memory::Read_U8(aBatter_Singles + offset);
    stat.doubles      = Memory::Read_U8(aBatter_Doubles + offset);
    stat.triples      = Memory::Read_U8(aBatter_Triples + offset);
    stat.homeruns     = Memory::Read_U8(aBatter_Homeruns + offset);
    stat.strikouts    = Memory::Read_U8(aBatter_Strikeouts + offset);
    stat.walks_4balls = Memory::Read_U8(aBatter_Walks_4Balls + offset);
    stat.walks_hit    = Memory::Read_U8(aBatter_Walks_Hit + offset);
    stat.rbi          = Memory::Read_U8(aBatter_RBI + offset);
    stat.bases_stolen = Memory::Read_U8(aBatter_BasesStolen + offset);
    stat.star_hits    = Memory::Read_U8(aBatter_StarHits + offset);

    m_defensive_stats[team_id][roster_id].big_plays = Memory::Read_U8(aBatter_BigPlays + offset);
}

void StatTracker::logABScenario(){
    std::cout << "Logging Sceanrio" << std::endl;
    //TODO: Figure out "Team1 is Hitting" Addr
    m_curr_ab_stat.team_id = (Memory::Read_U8(aAB_BatterPort) == 1) ? 0 : 1; //If P1/Team1 is batting then Team=0(Team1). Else Team=1 (Team2)
    m_curr_ab_stat.roster_id = Memory::Read_U8(aAB_RosterID);

    m_curr_ab_stat.inning          = Memory::Read_U8(aAB_Inning);
    m_curr_ab_stat.half_inning     = Memory::Read_U8(aAB_HalfInning);
    m_curr_ab_stat.team0_score     = Memory::Read_U16(aTeam0_Score);
    m_curr_ab_stat.team1_score     = Memory::Read_U16(aTeam1_Score);
    m_curr_ab_stat.balls           = Memory::Read_U8(aAB_Balls);
    m_curr_ab_stat.strikes         = Memory::Read_U8(aAB_Strikes);
    m_curr_ab_stat.outs            = Memory::Read_U8(aAB_Outs);
    m_curr_ab_stat.batter_stars    = Memory::Read_U8(aAB_P1_Stars);
    m_curr_ab_stat.pitcher_stars   = Memory::Read_U8(aAB_P2_Stars);
    m_curr_ab_stat.is_star_chance  = Memory::Read_U8(aAB_IsStarChance);
    m_curr_ab_stat.chem_links_ob   = Memory::Read_U8(aAB_ChemLinksOnBase);

    m_curr_ab_stat.runner_on_1     = Memory::Read_U8(aAB_RunnerOn1);
    m_curr_ab_stat.runner_on_2     = Memory::Read_U8(aAB_RunnerOn2);
    m_curr_ab_stat.runner_on_3     = Memory::Read_U8(aAB_RunnerOn3);

    m_curr_ab_stat.batter_handedness = Memory::Read_U8(aAB_BatterHand);

    std::cout << "Inning: " << std::to_string(m_curr_ab_stat.inning) << std::endl;
}

void StatTracker::logABContact(){
    std::cout << "Logging Contact" << std::endl;

    m_curr_ab_stat.type_of_contact   = Memory::Read_U8(aAB_TypeOfContact);
    m_curr_ab_stat.bunt              =(Memory::Read_U8(aAB_Bunt) == 3);
    m_curr_ab_stat.charge_swing      = Memory::Read_U8(aAB_ChargeSwing);
    m_curr_ab_stat.charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    m_curr_ab_stat.charge_power_down = Memory::Read_U32(aAB_ChargeDown);
    m_curr_ab_stat.star_swing        = Memory::Read_U8(aAB_StarSwing);
    m_curr_ab_stat.moon_shot         = Memory::Read_U8(aAB_MoonShot);
    m_curr_ab_stat.input_direction   = Memory::Read_U8(aAB_InputDirection);

    m_curr_ab_stat.horiz_power       = Memory::Read_U16(aAB_HorizPower);
    m_curr_ab_stat.vert_power        = Memory::Read_U16(aAB_VertPower);
    m_curr_ab_stat.ball_angle        = Memory::Read_U16(aAB_BallAngle);

    m_curr_ab_stat.ball_x_velocity   = Memory::Read_U32(aAB_BallVel_X);
    m_curr_ab_stat.ball_y_velocity   = Memory::Read_U32(aAB_BallVel_Y);
    m_curr_ab_stat.ball_z_velocity   = Memory::Read_U32(aAB_BallVel_Z);

    m_curr_ab_stat.hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    m_curr_ab_stat.frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
    m_curr_ab_stat.frameOfPitchUponSwing   = Memory::Read_U16(aAB_FrameOfPitchSeqUponSwing);

    logABPitch();
}

void StatTracker::logABMiss(){
    std::cout << "Logging Miss" << std::endl;

    m_curr_ab_stat.type_of_contact   = 0; //Set 0 because game only sets when contact is made. Never reset
    m_curr_ab_stat.bunt              =(Memory::Read_U8(aAB_Miss_SwingOrBunt) == 2); //Need to use miss version for bunt. Game won't set bunt regular flag unless contact is made
    m_curr_ab_stat.charge_swing      = Memory::Read_U8(aAB_ChargeSwing);
    m_curr_ab_stat.charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    m_curr_ab_stat.charge_power_down = Memory::Read_U32(aAB_ChargeDown);

    m_curr_ab_stat.star_swing        = Memory::Read_U8(aAB_StarSwing);
    m_curr_ab_stat.moon_shot         = Memory::Read_U8(aAB_MoonShot);
    m_curr_ab_stat.input_direction   = Memory::Read_U8(aAB_InputDirection);

    m_curr_ab_stat.horiz_power       = 0;
    m_curr_ab_stat.vert_power        = 0;
    m_curr_ab_stat.ball_angle        = 0;

    m_curr_ab_stat.ball_x_velocity   = 0;
    m_curr_ab_stat.ball_y_velocity   = 0;
    m_curr_ab_stat.ball_z_velocity   = 0;

    m_curr_ab_stat.hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    m_curr_ab_stat.frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
    m_curr_ab_stat.frameOfPitchUponSwing   = Memory::Read_U16(aAB_FrameOfPitchSeqUponSwing);

    u8 any_strike = Memory::Read_U8(aAB_Miss_AnyStrike);
    u8 miss_type  = Memory::Read_U8(aAB_Miss_SwingOrBunt);

    if (!any_strike){
        if (m_curr_ab_stat.hit_by_pitch){ m_curr_ab_stat.result_inferred = "Walk-HPB"; }
        else if (m_curr_ab_stat.balls == 3) {  m_curr_ab_stat.result_inferred = "Walk-BB"; }
        else {m_curr_ab_stat.result_inferred = "Ball";};
    }
    else{
        if (miss_type == 0){
            m_curr_ab_stat.result_inferred = "Strike-looking";
        }
        else if (miss_type == 1){
            m_curr_ab_stat.result_inferred = "Strike-swing";
        }
        else if (miss_type == 2){
            m_curr_ab_stat.result_inferred = "Strike-bunting";
        }
        else{
            m_curr_ab_stat.result_inferred = "Unknown Miss Result";
        }
    }
}

void StatTracker::logABPitch(){
    std::cout << "Logging Pitching" << std::endl;

    m_curr_ab_stat.pitcher_id         = Memory::Read_U8(aAB_PitcherID);
    m_curr_ab_stat.pitcher_handedness = Memory::Read_U8(aAB_PitcherHandedness);
    m_curr_ab_stat.pitch_type         = Memory::Read_U8(aAB_PitchType);
    m_curr_ab_stat.charge_type        = Memory::Read_U8(aAB_ChargePitchType);
    m_curr_ab_stat.star_pitch         = Memory::Read_U8(aAB_StarPitch);
    m_curr_ab_stat.pitch_speed        = Memory::Read_U8(aAB_PitchSpeed);
}

void StatTracker::logABContactResult(){
    std::cout << "Logging Contact Result" << std::endl;

    u8 result = Memory::Read_U8(aAB_ContactResult);

    if (result == 1){
        m_curr_ab_stat.result_inferred = "Landed";
    }
    else if (result == 3){
        m_curr_ab_stat.result_inferred = "Caught";
    }
    else if (result == 0xFF){
        m_curr_ab_stat.result_inferred = "Foul";
    }
    else{
        m_curr_ab_stat.result_inferred = "Unknown: " + std::to_string(result);
    }
    m_curr_ab_stat.ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
    m_curr_ab_stat.ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
    m_curr_ab_stat.ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
}

void StatTracker::printStatsToFile(){
    std::string file_name = std::to_string(m_game_id) + ".txt";
    std::ofstream MyFile(file_name);

    MyFile << "{" << std::endl;
    MyFile << "  \"GameID\": \"" << std::hex << m_game_id << "\"," << std::endl;
    MyFile << "  \"Date\": \"" << m_game_info.date_time << "\"," << std::endl;
    MyFile << "  \"Ranked\": " << std::to_string(m_game_info.ranked) << "," << std::endl;
    MyFile << "  \"Team1 Player\": \"" << m_game_info.team0_player_name << "\"," << std::endl; //TODO MAKE THIS AN ID
    MyFile << "  \"Team2 Player\": \"" << m_game_info.team1_player_name << "\"," << std::endl;

    MyFile << "  \"Team1 Score\": \"" << std::dec << m_game_info.team0_score << "\"," << std::endl;
    MyFile << "  \"Team2 Score\": \"" << std::dec << m_game_info.team1_score << "\"," << std::endl;
    
    //Team Captain and Roster
    for (int team=0; team < cNumOfTeams; ++team){
        u16 captain_id = (team==0) ? m_game_info.team0_captain : m_game_info.team1_captain;
        MyFile << "  \"Team" << (team + 1) << " Captain\": " << std::to_string(captain_id) << "," << std::endl;
        std::string str_roster = "[";
        for (int roster=0; roster < cRosterSize; ++roster){
            str_roster = str_roster + std::to_string(m_game_info.rosters_char_id[team][roster]);
            if (roster == 8){
                str_roster = str_roster + "],";
            }
            else{
                str_roster = str_roster + ", ";
            }
        }
        MyFile << "  \"Team" << (team + 1) << " Roster\": " << str_roster << std::endl;
    }

    MyFile << "  \"Player Stats\": [" << std::endl;
    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){
            EndGameRosterDefensiveStats& def_stat = m_defensive_stats[team][roster];
            MyFile << "    {" << std::endl;
            MyFile << "      \"TeamID\": " << team << "," << std::endl;
            MyFile << "      \"RosterID\": " << roster << "," << std::endl;
            MyFile << "      \"Is Starred\": " << std::to_string(def_stat.is_starred) << "," << std::endl;
            MyFile << "      \"Defensive Stats\": {" << std::endl;
            MyFile << "        \"Batters Faced\": " << std::to_string(def_stat.batters_faced) << "," << std::endl;
            MyFile << "        \"Runs Allowed\": " << std::dec << def_stat.runs_allowed << "," << std::endl;
            MyFile << "        \"Batters Walked\": " << def_stat.batters_walked << "," << std::endl;
            MyFile << "        \"Batters Hit\": " << def_stat.batters_hit << "," << std::endl;
            MyFile << "        \"Hits Allowed\": " << def_stat.hits_allowed << "," << std::endl;
            MyFile << "        \"HRs Allowed\": " << def_stat.homeruns_allowed << "," << std::endl;
            MyFile << "        \"Pitches Thrown\": " << def_stat.pitches_thrown << "," << std::endl;
            MyFile << "        \"Stamina\": " << def_stat.stamina << "," << std::endl;
            MyFile << "        \"Was Pitcher\": " << std::to_string(def_stat.was_pitcher) << "," << std::endl;
            MyFile << "        \"Batter Outs\": " << std::to_string(def_stat.batter_outs) << "," << std::endl;
            MyFile << "        \"Strikeouts\": " << std::to_string(def_stat.strike_outs) << "," << std::endl;
            MyFile << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            MyFile << "        \"Big Plays\": " << std::to_string(def_stat.star_pitches_thrown) << std::endl;
            MyFile << "      }," << std::endl;

            EndGameRosterOffensiveStats& of_stat = m_offensive_stats[team][roster];
            MyFile << "      \"Offensive Stats\": {" << std::endl;
            MyFile << "        \"At Bats\": " << std::to_string(of_stat.at_bats) << "," << std::endl;
            MyFile << "        \"Hits\": " << std::to_string(of_stat.hits) << "," << std::endl;
            MyFile << "        \"Singles\": " << std::to_string(of_stat.singles) << "," << std::endl;
            MyFile << "        \"Doubles\": " << std::to_string(of_stat.doubles) << "," << std::endl;
            MyFile << "        \"Triples\": " << std::to_string(of_stat.triples) << "," << std::endl;
            MyFile << "        \"Homeruns\": " << std::to_string(of_stat.homeruns) << "," << std::endl;
            MyFile << "        \"Strikeouts\": " << std::to_string(of_stat.strikouts) << "," << std::endl;
            MyFile << "        \"Walks (4 Balls)\": " << std::to_string(of_stat.walks_4balls) << "," << std::endl;
            MyFile << "        \"Walks (Hit)\": " << std::to_string(of_stat.walks_hit) << "," << std::endl;
            MyFile << "        \"RBI\": " << std::to_string(of_stat.rbi) << "," << std::endl;
            MyFile << "        \"Bases Stolen\": " << std::to_string(of_stat.bases_stolen) << "," << std::endl;
            MyFile << "        \"Star Hits\": " << std::to_string(of_stat.star_hits) << std::endl;
            MyFile << "      }," << std::endl;

            //Iterate Batters vector of batting stats
            MyFile << "      \"At-Bat Stats\": [" << std::endl;
            for (auto& ab_stat : m_ab_stats[team][roster]){
                MyFile << "        {" << std::endl;
                MyFile << "          \"Inning\": " << std::to_string(ab_stat.inning) << "," << std::endl;
                MyFile << "          \"Half Inning\": " << std::to_string(ab_stat.half_inning) << "," << std::endl;
                MyFile << "          \"Team1 Score\": \"" << std::dec << ab_stat.team0_score << "\"," << std::endl;
                MyFile << "          \"Team2 Score\": \"" << std::dec << ab_stat.team1_score << "\"," << std::endl;
                MyFile << "          \"Balls\": " << std::to_string(ab_stat.balls) << "," << std::endl;
                MyFile << "          \"Strikes\": " << std::to_string(ab_stat.strikes) << "," << std::endl;
                MyFile << "          \"Outs\": " << std::to_string(ab_stat.outs) << "," << std::endl;
                MyFile << "          \"Runners on Base\": [" << std::to_string(ab_stat.runner_on_3) << "," 
                                                             << std::to_string(ab_stat.runner_on_2) << ","
                                                             << std::to_string(ab_stat.runner_on_1) << "]," << std::endl;
                MyFile << "          \"Chemistry Links on Base\": " << std::to_string(ab_stat.chem_links_ob) << "," << std::endl;
                MyFile << "          \"Star Chance\": " << std::to_string(ab_stat.is_star_chance) << "," << std::endl;
                MyFile << "          \"Batter Stars\": " << std::to_string(ab_stat.batter_stars) << "," << std::endl;
                MyFile << "          \"Pitcher Stars\": " << std::to_string(ab_stat.pitcher_stars) << "," << std::endl;

                MyFile << "          \"PitcherID \": " << std::to_string(ab_stat.pitcher_id) << "," << std::endl;
                MyFile << "          \"Pitcher Handedness\": " << std::to_string(ab_stat.pitcher_handedness) << "," << std::endl;
                MyFile << "          \"Pitch Type\": " << std::to_string(ab_stat.pitch_type) << "," << std::endl;
                MyFile << "          \"Charge Pitch Type\": " << std::to_string(ab_stat.charge_type) << "," << std::endl;
                MyFile << "          \"Star Pitch\": " << std::to_string(ab_stat.star_pitch) << "," << std::endl;
                MyFile << "          \"Pitch Speed\": " << std::to_string(ab_stat.pitch_speed) << "," << std::endl;

                MyFile << "          \"Type of Contact\": " << std::to_string(ab_stat.type_of_contact) << "," << std::endl;
                MyFile << "          \"Charge Swing\": " << std::to_string(ab_stat.charge_swing) << "," << std::endl;
                MyFile << "          \"Bunt\": " << std::to_string(ab_stat.bunt) << "," << std::endl;
                MyFile << "          \"Charge Power Up\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.charge_power_up << "\"," << std::endl;
                MyFile << "          \"Charge Power Down\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.charge_power_down << "\"," << std::endl;
                MyFile << "          \"Star Swing\": " << std::to_string(ab_stat.star_swing) << "," << std::endl;
                MyFile << "          \"Star Swing - 5 Star\": " << std::to_string(ab_stat.moon_shot) << "," << std::endl;
                MyFile << "          \"Input Direction\": " << std::to_string(ab_stat.input_direction) << "," << std::endl;
                MyFile << "          \"Batter Handedness\": " << std::to_string(ab_stat.batter_handedness) << "," << std::endl;
                MyFile << "          \"Hit by Pitch\": " << std::to_string(ab_stat.hit_by_pitch) << "," << std::endl;
                MyFile << "          \"Frame Of Swing Upon Contact\": " << std::dec << ab_stat.frameOfSwingUponContact << "," << std::endl;
                MyFile << "          \"Frame Of Pitch Upon Swing\": " << std::dec << ab_stat.frameOfPitchUponSwing << "," << std::endl;
                MyFile << "          \"Ball Angle\": \"" << std::dec << ab_stat.ball_angle << "\"," << std::endl;
                MyFile << "          \"Ball Vertical Power\": \"" << std::dec << ab_stat.vert_power << "\"," << std::endl;
                MyFile << "          \"Ball Horizontal Power\": \"" << std::dec << ab_stat.horiz_power << "\"," << std::endl;
                MyFile << "          \"Ball Velocity - X\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_x_velocity << "\"," << std::endl;
                MyFile << "          \"Ball Velocity - Y\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_y_velocity << "\"," << std::endl;
                MyFile << "          \"Ball Velocity - Z\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_z_velocity << "\"," << std::endl;

                MyFile << "          \"Ball Landing Position - X\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_x_pos << "\"," << std::endl;
                MyFile << "          \"Ball Landing Position - Y\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_y_pos << "\"," << std::endl;
                MyFile << "          \"Ball Landing Position - Z\": \"" << std::setfill('0') << std::setw(8) << std::hex << ab_stat.ball_z_pos << "\"," << std::endl;

                MyFile << "          \"Number Outs During Play\": " << std::to_string(ab_stat.num_outs) << "," << std::endl;
                MyFile << "          \"RBI\": " << std::to_string(ab_stat.rbi) << "," << std::endl;

                MyFile << "          \"Final Result - Inferred\": \"" << ab_stat.result_inferred << "\"," << std::endl;
                MyFile << "          \"Final Result - Game\": \"" << std::to_string(ab_stat.result_game) << "\"" << std::endl;
                
                std::string end_comma = (ab_stat == m_ab_stats[team][roster].back()) ? "" : ",";
                MyFile << "        }" << end_comma << std::endl;
            }
            MyFile << "      ]" << std::endl;
            std::string end_comma = ((team == 1) && (roster==8)) ? "" : ",";
            MyFile << "    }" << end_comma << std::endl;
        }
    }

    MyFile << "  ]" << std::endl;
    MyFile << "}" << std::endl;

    std::cout << "Logging to " << file_name << std::endl;
}

//Read players from ini file and assign to team
void StatTracker::readPlayers(bool inLocal) {
    IniFile local_players_ini;
    local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));

    for (const IniFile* ini : {&local_players_ini})
    {
        std::vector<std::string> lines;
        ini->GetLines("Local_Players_List", &lines, false);

        AddPlayers::AddPlayers player;

        u8 port = 0;
        for (auto& line : lines)
        {
            ++port;
            std::istringstream ss(line);

            // Some locales (e.g. fr_FR.UTF-8) don't split the string stream on space
            // Use the C locale to workaround this behavior
            ss.imbue(std::locale::classic());

            switch ((line)[0])
            {
            case '+':
                if (!player.username.empty())
                //players.push_back(player);
                player = AddPlayers::AddPlayers();
                ss.seekg(1, std::ios_base::cur);
                // read the code name
                std::getline(ss, player.username,
                            '[');  // stop at [ character (beginning of contributor name)
                player.username = StripSpaces(player.username);
                // read the code creator name
                std::getline(ss, player.userid, ']');
                if (port == m_game_info.team0_port) { m_game_info.team0_player_name = player.username; }
                else if (port == m_game_info.team1_port) { m_game_info.team1_player_name = player.username; }
                break;

            break;
            }
        }

        // add the last code
        if (!player.username.empty())
        {
            //players.push_back(player);

            if (port == m_game_info.team0_port) { m_game_info.team0_player_name = player.username; }
            else if (port == m_game_info.team1_port) { m_game_info.team1_player_name = player.username; }
        }
    }
    //return players;
}
void StatTracker::setRankedStatus(bool inBool) {
    mCurrentRankedStatus = inBool;
}

void StatTracker::setRecordStatus(bool inBool) {
    std::cout << "Record Status=" << inBool << std::endl;
    mTrackerInfo.mRecord = inBool;
}
