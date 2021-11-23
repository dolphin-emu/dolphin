
#include "Core/MSB_StatTracker.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>

//For LocalPLayers
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/CommonPaths.h"
#include "Common/IniFile.h"

#include "Common/Swap.h"

void StatTracker::Run(){
    lookForTriggerEvents();
}

void StatTracker::lookForTriggerEvents(){
    //At Bat State Machine
    if (m_game_state == GAME_STATE::INGAME){
        switch(m_ab_state){
            //Look for Pitch
            case (AB_STATE::WAITING_FOR_PITCH):
                //Handle quit to main menu
                if (Memory::Read_U32(aGameId) == 0){
                    u8 quitter_port = Memory::Read_U8(aWhoQuit);
                    m_game_info.quitter_team = (quitter_port == m_game_info.away_port) ? "Away" : "Home";
                    logGameInfo();
                    //Game has ended. Write file but do not submit
                    std::pair<std::string, std::string> jsonPlusPath = getStatJSON(true);
                    File::WriteStringToFile(jsonPlusPath.second+".QUIT", jsonPlusPath.first);
                    init();
                }

                //First Pitch of the game: collect port/player names
                //Log beginning of pitch
                if (Memory::Read_U8(aAB_PitchThrown) == 1){
                    //Collect port info for players
                    if (m_game_info.team0_port == 0 && m_game_info.team1_port == 0){
                        u8 fielder_port = Memory::Read_U8(aAB_FieldingPort);
                        u8 batter_port = Memory::Read_U8(aAB_BattingPort);
                        
                        if (fielder_port == 1) { m_game_info.team0_port = fielder_port; }
                        else if (batter_port == 1) { m_game_info.team0_port = batter_port; }

                        if (fielder_port > 1 && fielder_port <= 4){
                            m_game_info.team1_port = fielder_port;
                        }
                        if (batter_port > 1 && batter_port <= 4){
                            m_game_info.team1_port = batter_port;
                        }
                        else{
                            m_game_info.team1_port = 0;
                            //TODO - Disable stat submission here
                        }

                        //Map home and away ports for scores
                        m_game_info.away_port = (batter_port > 0 && batter_port <= 4) ? batter_port : 0;
                        m_game_info.home_port = (fielder_port > 0 && fielder_port <= 4) ? fielder_port : 0;

                        readPlayerNames(!m_game_info.netplay);
                        setDefaultNames(!m_game_info.netplay);

                        std::string away_player_name;
                        std::string home_player_name;
                        if (m_game_info.away_port == m_game_info.team0_port) {
                            away_player_name = m_game_info.team0_player_name;
                            home_player_name = m_game_info.team1_player_name;
                        }
                        else{
                            away_player_name = m_game_info.team1_player_name;
                            home_player_name = m_game_info.team0_player_name;
                        }

                        std::cout << "Away Player=" << away_player_name << "(" << std::to_string(m_game_info.away_port) << "), Home Player=" << home_player_name << "(" << std::to_string(m_game_info.home_port) << ")" << std::endl;;
                    }


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
                    std::cout << "Play over" << std::endl;
                }
                break;
            case (AB_STATE::FINAL_RESULT):
                
                //Final Stats - Collected 1 frame after aAB_PitchThrown==0
                m_curr_ab_stat.num_outs_during_play = Memory::Read_U8(aAB_NumOutsDuringPlay);
                m_curr_ab_stat.result_game = Memory::Read_U8(aAB_FinalResult);

                //Store current AB stat to the players vector
                m_ab_stats[m_curr_ab_stat.team_id][m_curr_ab_stat.roster_id].push_back(m_curr_ab_stat);
                m_curr_ab_stat = ABStats();

                
                m_ab_state = AB_STATE::WAITING_FOR_PITCH;
                std::cout << "Logging Final Result" << std::endl << "Waiting for next pitch..." << std::endl;
                std::cout << std::endl;
                break;
            default:
                std::cout << "Unknown State" << std::endl;
                m_ab_state = AB_STATE::WAITING_FOR_PITCH;
                break;
        }
    }

    //Game State Machine
    switch (m_game_state){
        case (GAME_STATE::PREGAME):
            //Start recording when GameId is set AND record button is pressed
            if ((Memory::Read_U32(aGameId) != 0) && (mTrackerInfo.mRecord)){
                m_game_info.game_id = Memory::Read_U32(aGameId);
                //Sample settings
                m_game_info.ranked  = m_state.m_ranked_status;
                m_game_info.netplay = m_state.m_netplay_session;
                m_game_info.host    = m_state.m_is_host;
                m_game_info.netplay_opponent_alias = m_state.m_netplay_opponent_alias;

                m_game_state = GAME_STATE::INGAME;
                std::cout << "PREGAME->INGAME (GameID=" << std::to_string(m_game_info.game_id) << ", Ranked=" << m_game_info.ranked <<")" << std::endl;
                std::cout << "                (Netplay=" << m_game_info.netplay << ", Host=" << m_game_info.host 
                          << ", Netplay Opponent Alias=" << m_game_info.netplay_opponent_alias << ")" << std::endl; 
            }
            break;
        case (GAME_STATE::INGAME):
            if ((Memory::Read_U8(aEndOfGameFlag) == 1) && (m_ab_state == AB_STATE::WAITING_FOR_PITCH) ){
                logGameInfo();

                std::pair<std::string, std::string> jsonPlusPath = getStatJSON(true);
                File::WriteStringToFile(jsonPlusPath.second, jsonPlusPath.first);
                std::cout << "Logging to " << jsonPlusPath.second << std::endl;

                //Psuedocode for submit
                // if (submit){
                //     std::pair<std::string, std::string> jsonPlusPath = getStatJSON(false);
                //     /submit();
                // }

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

    m_game_info.stadium = Memory::Read_U8(aStadiumId);

    m_game_info.innings_selected = Memory::Read_U8(aInningsSelected);
    m_game_info.innings_played   = Memory::Read_U8(aAB_Inning);

    //Captains
    if (m_game_info.away_port == m_game_info.team0_port){
        m_game_info.away_captain = Memory::Read_U8(aTeam0_Captain);
        m_game_info.home_captain = Memory::Read_U8(aTeam1_Captain);
    }
    else{
        m_game_info.away_captain = Memory::Read_U8(aTeam1_Captain);
        m_game_info.home_captain = Memory::Read_U8(aTeam0_Captain);
    }

    m_game_info.away_score = Memory::Read_U16(aAwayTeam_Score);
    m_game_info.home_score = Memory::Read_U16(aHomeTeam_Score);

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

    stat.game_id    = m_game_info.game_id;
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

    stat.game_id   = m_game_info.game_id;
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
    m_curr_ab_stat.team_id = (Memory::Read_U8(aAB_BatterPort) == m_game_info.team0_port) ? 0 : 1; //If P1/Team1 is batting then Team=0(Team1). Else Team=1 (Team2)
    m_curr_ab_stat.roster_id = Memory::Read_U8(aAB_RosterID);

    bool away_team_is_batting = (Memory::Read_U8(aAB_BatterPort) == m_game_info.away_port); //away team is batting if batter port matches away port
    m_curr_ab_stat.inning          = Memory::Read_U8(aAB_Inning);
    m_curr_ab_stat.half_inning     = !away_team_is_batting;

    //Figure out scores
    m_curr_ab_stat.batter_score  = (away_team_is_batting) ? Memory::Read_U16(aAwayTeam_Score) : Memory::Read_U16(aHomeTeam_Score);
    m_curr_ab_stat.fielder_score = (away_team_is_batting) ? Memory::Read_U16(aHomeTeam_Score) : Memory::Read_U16(aAwayTeam_Score);

    m_curr_ab_stat.balls           = Memory::Read_U8(aAB_Balls);
    m_curr_ab_stat.strikes         = Memory::Read_U8(aAB_Strikes);
    m_curr_ab_stat.outs            = Memory::Read_U8(aAB_Outs);
    //Figure out star ownership
    if (m_curr_ab_stat.team_id == 0) {
        m_curr_ab_stat.batter_stars    = Memory::Read_U8(aAB_P1_Stars);
        m_curr_ab_stat.pitcher_stars   = Memory::Read_U8(aAB_P2_Stars);
    }
    else if (m_curr_ab_stat.team_id == 1) {
        m_curr_ab_stat.batter_stars    = Memory::Read_U8(aAB_P2_Stars);
        m_curr_ab_stat.pitcher_stars   = Memory::Read_U8(aAB_P1_Stars);
    }
    m_curr_ab_stat.is_star_chance  = Memory::Read_U8(aAB_IsStarChance);
    m_curr_ab_stat.chem_links_ob   = Memory::Read_U8(aAB_ChemLinksOnBase);

    m_curr_ab_stat.runner_on_1     = Memory::Read_U8(aAB_RunnerOn1);
    m_curr_ab_stat.runner_on_2     = Memory::Read_U8(aAB_RunnerOn2);
    m_curr_ab_stat.runner_on_3     = Memory::Read_U8(aAB_RunnerOn3);

    m_curr_ab_stat.batter_handedness = Memory::Read_U8(aAB_BatterHand);
}

void StatTracker::logABContact(){
    std::cout << "Logging Contact" << std::endl;

    m_curr_ab_stat.type_of_contact   = Memory::Read_U8(aAB_TypeOfContact);
    m_curr_ab_stat.bunt              =(Memory::Read_U8(aAB_Bunt) == 3);

    m_curr_ab_stat.swing             = (Memory::Read_U8(aAB_Miss_SwingOrBunt) == 1);
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

    m_curr_ab_stat.ball_x_accel   = Memory::Read_U32(aAB_BallAccel_X);
    m_curr_ab_stat.ball_y_accel   = Memory::Read_U32(aAB_BallAccel_Y);
    m_curr_ab_stat.ball_z_accel   = Memory::Read_U32(aAB_BallAccel_Z);

    m_curr_ab_stat.hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    m_curr_ab_stat.frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
    m_curr_ab_stat.frameOfPitchUponSwing   = Memory::Read_U16(aAB_FrameOfPitchSeqUponSwing);
    if (m_curr_ab_stat.bunt){
        m_curr_ab_stat.frameOfPitchUponSwing = 0;
    }

    logABPitch();
}

void StatTracker::logABMiss(){
    std::cout << "Logging Miss" << std::endl;

    m_curr_ab_stat.type_of_contact   = 0xFF; //Set 0 because game only sets when contact is made. Never reset
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

    m_curr_ab_stat.ball_x_accel   = 0;
    m_curr_ab_stat.ball_y_accel   = 0;
    m_curr_ab_stat.ball_z_accel   = 0;

    m_curr_ab_stat.hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    m_curr_ab_stat.frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
    m_curr_ab_stat.frameOfPitchUponSwing   = Memory::Read_U16(aAB_FrameOfPitchSeqUponSwing);

    u8 any_strike = Memory::Read_U8(aAB_Miss_AnyStrike);
    u8 miss_type  = Memory::Read_U8(aAB_Miss_SwingOrBunt);

    if (!any_strike){
        m_curr_ab_stat.frameOfPitchUponSwing = 0;
        if (m_curr_ab_stat.hit_by_pitch){ m_curr_ab_stat.result_inferred = "Walk-HPB"; }
        else if (m_curr_ab_stat.balls == 3) {  m_curr_ab_stat.result_inferred = "Walk-BB"; }
        else {
            m_curr_ab_stat.result_inferred = "Ball";
            m_curr_ab_stat.frameOfPitchUponSwing = 0;
        };
    }
    else{
        if (miss_type == 0){
            m_curr_ab_stat.swing = 0;
            m_curr_ab_stat.result_inferred = "Strike-looking";
            m_curr_ab_stat.frameOfPitchUponSwing = 0;
        }
        else if (miss_type == 1){
            m_curr_ab_stat.swing = 1;
            m_curr_ab_stat.result_inferred = "Strike-swing";
        }
        else if (miss_type == 2){
            m_curr_ab_stat.swing = 0;
            m_curr_ab_stat.result_inferred = "Strike-bunting";
            m_curr_ab_stat.frameOfPitchUponSwing = 0;
        }
        else{
            m_curr_ab_stat.result_inferred = "Unknown Miss Result";
            m_curr_ab_stat.frameOfPitchUponSwing = 0;
        }
    }
}

void StatTracker::logABPitch(){
    std::cout << "Logging Pitching" << std::endl;

    //Add this inning to the list of innings that the pitcher has pitched in
    u8 pitcher_roster_id = Memory::Read_U8(aAB_PitcherRosterID);
    u8 pitcher_team_id   = !m_curr_ab_stat.team_id;
    EndGameRosterDefensiveStats& stat = m_defensive_stats[pitcher_team_id][pitcher_roster_id];
    stat.innings_pitched.emplace(m_curr_ab_stat.inning);

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

std::pair<std::string, std::string> StatTracker::getStatJSON(bool inDecode){
    std::string away_player_name;
    std::string home_player_name;
    bool team0_is_away;
    if (m_game_info.away_port == m_game_info.team0_port) {
        team0_is_away = true;
        away_player_name = m_game_info.team0_player_name;
        home_player_name = m_game_info.team1_player_name;
    }
    else{
        team0_is_away = false;
        away_player_name = m_game_info.team1_player_name;
        home_player_name = m_game_info.team0_player_name;
    }

    std::string file_name = away_player_name 
                   + "-Vs-" + home_player_name
                   + "_" + std::to_string(m_game_info.game_id) + ".json";

    std::string full_file_path = File::GetUserPath(D_STATFILES_IDX) + file_name;

    std::stringstream json_stream;

    json_stream << "{" << std::endl;
    std::string stadium = (inDecode) ? "\"" + cStadiumIdToStadiumName.at(m_game_info.stadium) + "\"" : std::to_string(m_game_info.stadium);
    json_stream << "  \"GameID\": \"" << std::hex << m_game_info.game_id << "\"," << std::endl;
    json_stream << "  \"Date\": \"" << m_game_info.date_time << "\"," << std::endl;
    json_stream << "  \"Ranked\": " << std::to_string(m_game_info.ranked) << "," << std::endl;
    json_stream << "  \"StadiumID\": " << stadium << "," << std::endl;
    json_stream << "  \"Away Player\": \"" << away_player_name << "\"," << std::endl; //TODO MAKE THIS AN ID
    json_stream << "  \"Home Player\": \"" << home_player_name << "\"," << std::endl;

    json_stream << "  \"Away Score\": " << std::dec << m_game_info.away_score << "," << std::endl;
    json_stream << "  \"Home Score\": " << std::dec << m_game_info.home_score << "," << std::endl;

    json_stream << "  \"Innings Selected\": \"" << std::dec << std::to_string(m_game_info.innings_selected) << "\"," << std::endl;
    json_stream << "  \"Innings Played\": \"" << std::dec << std::to_string(m_game_info.innings_played) << "\"," << std::endl;
    
    //Team Captain and Roster
    for (int team=0; team < cNumOfTeams; ++team){
        //If Team0 is away team
        std::string team_label;
        u16 captain_id = 0;
        if (team == 0){
            if (team0_is_away){
                team_label = "Away";
                captain_id = m_game_info.away_captain;
            }
            else {
                team_label = "Home";
                captain_id = m_game_info.home_captain;
            }
        }
        else if (team == 1) {
            if (team0_is_away){
                team_label = "Home";
                captain_id = m_game_info.home_captain;
            }
            else {
                team_label = "Away";
                captain_id = m_game_info.away_captain;
            }
        }

        std::string captain = (inDecode) ? "\"" + cCharIdToCharName.at(captain_id) + "\"" : std::to_string(captain_id);
        json_stream << "  \"" << team_label << " Team Captain\": " << captain << "," << std::endl;
        std::string str_roster = "[";
        for (int roster=0; roster < cRosterSize; ++roster){
            std::string character = (inDecode) ? "\"" + cCharIdToCharName.at(m_game_info.rosters_char_id[team][roster]) + "\"" : std::to_string(m_game_info.rosters_char_id[team][roster]);
            str_roster = str_roster + character;
            if (roster == 8){
                str_roster = str_roster + "],";
            }
            else{
                str_roster = str_roster + ", ";
            }
        }
        json_stream << "  \"" << team_label << " Team Roster\": " << str_roster << std::endl;
    }
    json_stream << "  \"Quitter Team\": \"" << m_game_info.quitter_team << "\"," << std::endl;

    json_stream << "  \"Player Stats\": [" << std::endl;
    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        std::string team_label;
        if (team == 0){
            if (team0_is_away){
                team_label = "Away";
            }
            else{
                team_label = "Home";
            }
        }
        else if (team == 1) {
            if (team0_is_away){
                team_label = "Home";
            }
            else {
                team_label = "Away";
            }
        }

        for (int roster=0; roster < cRosterSize; ++roster){
            EndGameRosterDefensiveStats& def_stat = m_defensive_stats[team][roster];
            json_stream << "    {" << std::endl;
            json_stream << "      \"Team\": \"" << team_label << "\"," << std::endl;
            json_stream << "      \"RosterID\": " << roster << "," << std::endl;

            std::string character = (inDecode) ? "\"" + cCharIdToCharName.at(m_game_info.rosters_char_id[team][roster]) + "\"" : std::to_string(m_game_info.rosters_char_id[team][roster]);
            json_stream << "      \"Character\": " << character << "," << std::endl;

            json_stream << "      \"Is Starred\": " << std::to_string(def_stat.is_starred) << "," << std::endl;
            json_stream << "      \"Defensive Stats\": {" << std::endl;
            json_stream << "        \"Batters Faced\": " << std::to_string(def_stat.batters_faced) << "," << std::endl;
            json_stream << "        \"Runs Allowed\": " << std::dec << def_stat.runs_allowed << "," << std::endl;
            json_stream << "        \"Batters Walked\": " << def_stat.batters_walked << "," << std::endl;
            json_stream << "        \"Batters Hit\": " << def_stat.batters_hit << "," << std::endl;
            json_stream << "        \"Hits Allowed\": " << def_stat.hits_allowed << "," << std::endl;
            json_stream << "        \"HRs Allowed\": " << def_stat.homeruns_allowed << "," << std::endl;
            json_stream << "        \"Pitches Thrown\": " << def_stat.pitches_thrown << "," << std::endl;
            json_stream << "        \"Stamina\": " << def_stat.stamina << "," << std::endl;
            json_stream << "        \"Was Pitcher\": " << std::to_string(def_stat.was_pitcher) << "," << std::endl;
            json_stream << "        \"Batter Outs\": " << std::to_string(def_stat.batter_outs) << "," << std::endl;
            json_stream << "        \"Strikeouts\": " << std::to_string(def_stat.strike_outs) << "," << std::endl;
            json_stream << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "        \"Big Plays\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "        \"Innings Pitched\": " << std::to_string(def_stat.innings_pitched.size()) << std::endl;
            json_stream << "      }," << std::endl;

            EndGameRosterOffensiveStats& of_stat = m_offensive_stats[team][roster];
            json_stream << "      \"Offensive Stats\": {" << std::endl;
            json_stream << "        \"At Bats\": " << std::to_string(of_stat.at_bats) << "," << std::endl;
            json_stream << "        \"Hits\": " << std::to_string(of_stat.hits) << "," << std::endl;
            json_stream << "        \"Singles\": " << std::to_string(of_stat.singles) << "," << std::endl;
            json_stream << "        \"Doubles\": " << std::to_string(of_stat.doubles) << "," << std::endl;
            json_stream << "        \"Triples\": " << std::to_string(of_stat.triples) << "," << std::endl;
            json_stream << "        \"Homeruns\": " << std::to_string(of_stat.homeruns) << "," << std::endl;
            json_stream << "        \"Strikeouts\": " << std::to_string(of_stat.strikouts) << "," << std::endl;
            json_stream << "        \"Walks (4 Balls)\": " << std::to_string(of_stat.walks_4balls) << "," << std::endl;
            json_stream << "        \"Walks (Hit)\": " << std::to_string(of_stat.walks_hit) << "," << std::endl;
            json_stream << "        \"RBI\": " << std::to_string(of_stat.rbi) << "," << std::endl;
            json_stream << "        \"Bases Stolen\": " << std::to_string(of_stat.bases_stolen) << "," << std::endl;
            json_stream << "        \"Star Hits\": " << std::to_string(of_stat.star_hits) << std::endl;
            json_stream << "      }," << std::endl;

            //Iterate Batters vector of batting stats
            json_stream << "      \"At-Bat Stats\": [" << std::endl;
            for (auto& ab_stat : m_ab_stats[team][roster]){
                json_stream << "        {" << std::endl;
                json_stream << "          \"Batter\": " << character << "," << std::endl;
                json_stream << "          \"Inning\": " << std::to_string(ab_stat.inning) << "," << std::endl;
                json_stream << "          \"Half Inning\": " << std::to_string(ab_stat.half_inning) << "," << std::endl;
                json_stream << "          \"Batter Score\": \"" << std::dec << ab_stat.batter_score << "\"," << std::endl;
                json_stream << "          \"Fielder Score\": \"" << std::dec << ab_stat.fielder_score << "\"," << std::endl;
                json_stream << "          \"Balls\": " << std::to_string(ab_stat.balls) << "," << std::endl;
                json_stream << "          \"Strikes\": " << std::to_string(ab_stat.strikes) << "," << std::endl;
                json_stream << "          \"Outs\": " << std::to_string(ab_stat.outs) << "," << std::endl;
                json_stream << "          \"Runners on Base\": [" << std::to_string(ab_stat.runner_on_3) << "," 
                                                             << std::to_string(ab_stat.runner_on_2) << ","
                                                             << std::to_string(ab_stat.runner_on_1) << "]," << std::endl;
                json_stream << "          \"Chemistry Links on Base\": " << std::to_string(ab_stat.chem_links_ob) << "," << std::endl;
                json_stream << "          \"Star Chance\": " << std::to_string(ab_stat.is_star_chance) << "," << std::endl;
                json_stream << "          \"Batter Stars\": " << std::to_string(ab_stat.batter_stars) << "," << std::endl;
                json_stream << "          \"Pitcher Stars\": " << std::to_string(ab_stat.pitcher_stars) << "," << std::endl;

                std::string pitcher       = (inDecode) ? "\"" + cCharIdToCharName.at(ab_stat.pitcher_id) + "\"" : std::to_string(ab_stat.pitcher_id);
                std::string pitcher_hand  = (inDecode) ? "\"" + cHandToHR.at(ab_stat.pitcher_handedness) + "\"" : std::to_string(ab_stat.pitcher_handedness);
                std::string pitch_type    = (inDecode) ? "\"" + cPitchTypeToHR.at(ab_stat.pitch_type) + "\"" : std::to_string(ab_stat.pitch_type);
                std::string charge_pitch_type    = (inDecode) ? "\"" + cChargePitchTypeToHR.at(ab_stat.charge_type) + "\"" : std::to_string(ab_stat.charge_type);
                json_stream << "          \"PitcherID\": " << pitcher << "," << std::endl;
                json_stream << "          \"Pitcher Handedness\": " << pitcher_hand << "," << std::endl;
                json_stream << "          \"Pitch Type\": " << pitch_type << "," << std::endl;
                json_stream << "          \"Charge Pitch Type\": " << charge_pitch_type << "," << std::endl;
                json_stream << "          \"Star Pitch\": " << std::to_string(ab_stat.star_pitch) << "," << std::endl;
                json_stream << "          \"Pitch Speed\": " << std::to_string(ab_stat.pitch_speed) << "," << std::endl;

                
                std::string batter_handedness  = (inDecode) ? "\"" + cHandToHR.at(ab_stat.batter_handedness) + "\"" : std::to_string(ab_stat.batter_handedness);
                json_stream << "          \"Batter Handedness\": " << batter_handedness << "," << std::endl;

                //Create TypeOfSwing
                u8 typeOfSwing;
                if (ab_stat.charge_swing == 1){      typeOfSwing = 2; }
                else if (ab_stat.star_swing == 1){   typeOfSwing = 3; }
                else if (ab_stat.bunt == 1){         typeOfSwing = 4; }
                else if (ab_stat.swing == 1){        typeOfSwing = 1; }
                else {                               typeOfSwing = 0; }
                ab_stat.type_of_swing = typeOfSwing;
                std::string type_of_swing = (inDecode) ? "\"" + cTypeOfSwing.at(ab_stat.type_of_swing) + "\"" : std::to_string(ab_stat.type_of_swing);

                json_stream << "          \"Type of Swing\": " << type_of_swing << "," << std::endl;
                json_stream << "          \"Contact\": [" << std::endl;
                
                if (ab_stat.type_of_contact != 0xFF) {

                    std::string type_of_contact    = (inDecode) ? "\"" + cTypeOfContactToHR.at(ab_stat.type_of_contact) + "\"" : std::to_string(ab_stat.type_of_contact);
                    std::string input_direction    = (inDecode) ? "\"" + cInputDirectionToHR.at(ab_stat.input_direction) + "\"" : std::to_string(ab_stat.input_direction);
                    json_stream << "            \"Type of Contact\": " << type_of_contact << "," << std::endl;

                    //Convert Charge u32s to IEEE 754 Floats
                    float charge_power_up, charge_power_down;
                    float_converter.num = ab_stat.charge_power_up;
                    charge_power_up = float_converter.fnum;

                    float_converter.num = ab_stat.charge_power_down;
                    charge_power_down = float_converter.fnum;

                    json_stream << "            \"Charge Power Up\": " << charge_power_up << "," << std::endl;
                    json_stream << "            \"Charge Power Down\": " << charge_power_down << "," << std::endl;
                    json_stream << "            \"Star Swing - 5 Star\": " << std::to_string(ab_stat.moon_shot) << "," << std::endl;
                    json_stream << "            \"Input Direction\": " << input_direction << "," << std::endl;
                    json_stream << "            \"Frame Of Swing Upon Contact\": " << std::dec << ab_stat.frameOfSwingUponContact << "," << std::endl;
                    json_stream << "            \"Frame Of Pitch Upon Swing\": " << std::dec << ab_stat.frameOfPitchUponSwing << "," << std::endl;
                    json_stream << "            \"Ball Angle\": \"" << std::dec << ab_stat.ball_angle << "\"," << std::endl;
                    json_stream << "            \"Ball Vertical Power\": \"" << std::dec << ab_stat.vert_power << "\"," << std::endl;
                    json_stream << "            \"Ball Horizontal Power\": \"" << std::dec << ab_stat.horiz_power << "\"," << std::endl;


                    //Convert velocity, pos u32s to IEEE 754 Floats
                    float ball_x_velocity, ball_y_velocity, ball_z_velocity, 
                        ball_x_accel, ball_y_accel, ball_z_accel, 
                        ball_x_pos, ball_y_pos, ball_z_pos;
                    float_converter.num = ab_stat.ball_x_velocity;
                    ball_x_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_velocity;
                    ball_y_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_z_velocity;
                    ball_z_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_x_accel;
                    ball_x_accel = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_accel;
                    ball_y_accel = float_converter.fnum;

                    float_converter.num = ab_stat.ball_z_accel;
                    ball_z_accel = float_converter.fnum;

                    float_converter.num = ab_stat.ball_x_pos;
                    ball_x_pos = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_pos;
                    ball_y_pos = float_converter.fnum;

                    float_converter.num = ab_stat.ball_z_pos;
                    ball_z_pos = float_converter.fnum;

                    json_stream << "            \"Ball Velocity - X\": " << ball_x_velocity << "," << std::endl;
                    json_stream << "            \"Ball Velocity - Y\": " << ball_y_velocity << "," << std::endl;
                    json_stream << "            \"Ball Velocity - Z\": " << ball_z_velocity << "," << std::endl;
                    json_stream << "            \"Ball Acceleration - X\": " << ball_x_accel << "," << std::endl;
                    json_stream << "            \"Ball Acceleration - Y\": " << ball_y_accel << "," << std::endl;
                    json_stream << "            \"Ball Acceleration - Z\": " << ball_z_accel << "," << std::endl;
                    json_stream << "            \"Ball Landing Position - X\": " << ball_x_pos << "," << std::endl;
                    json_stream << "            \"Ball Landing Position - Y\": " << ball_y_pos << "," << std::endl;
                    json_stream << "            \"Ball Landing Position - Z\": " << ball_z_pos << std::endl;
                }
                json_stream << "          ]," << std::endl;
                json_stream << "          \"Number Outs During Play\": " << std::to_string(ab_stat.num_outs_during_play) << "," << std::endl;
                json_stream << "          \"RBI\": " << std::to_string(ab_stat.rbi) << "," << std::endl;

                json_stream << "          \"Final Result - Inferred\": \"" << ab_stat.result_inferred << "\"," << std::endl;
                json_stream << "          \"Final Result - Game\": \"" << std::to_string(ab_stat.result_game) << "\"" << std::endl;
                
                std::string end_comma = (ab_stat == m_ab_stats[team][roster].back()) ? "" : ",";
                json_stream << "        }" << end_comma << std::endl;
            }
            json_stream << "      ]" << std::endl;
            std::string end_comma = ((team == 1) && (roster==8)) ? "" : ",";
            json_stream << "    }" << end_comma << std::endl;
        }
    }

    json_stream << "  ]" << std::endl;
    json_stream << "}" << std::endl;

    return std::make_pair(json_stream.str(), full_file_path);
}

//Read players from ini file and assign to team
void StatTracker::readPlayerNames(bool local_game) {
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
                if (local_game){
                    if (port == m_game_info.team0_port) { m_game_info.team0_player_name = player.username; }
                    else if (port == m_game_info.team1_port) { m_game_info.team1_player_name = player.username; }
                }
                else {
                    if(m_game_info.host) {
                        m_game_info.team0_player_name = player.username;
                    }
                    else{
                        m_game_info.team1_player_name = player.username;
                    }
                    return;
                }
                break;

            break;
            }
        }

        // add the last code
        if (!player.username.empty())
        {
            if (local_game){
                if (port == m_game_info.team0_port) { m_game_info.team0_player_name = player.username; }
                else if (port == m_game_info.team1_port) { m_game_info.team1_player_name = player.username; }
            }
            else {
                if(m_game_info.host) {
                    m_game_info.team0_player_name = player.username;
                }
                else{
                    m_game_info.team1_player_name = player.username;
                }
                return;
            }
        }
    }

    return;
}

void StatTracker::setDefaultNames(bool local_game){
    if (local_game){
        if (m_game_info.team0_port == 0) m_game_info.team0_player_name = "CPU";
        if (m_game_info.team1_port == 0) m_game_info.team1_player_name = "CPU";
    }
    else {
        if (m_game_info.team0_player_name.empty()) m_game_info.team0_player_name = "Netplayer~" + m_game_info.netplay_opponent_alias;
        if (m_game_info.team1_player_name.empty()) m_game_info.team1_player_name = "Netplayer~" + m_game_info.netplay_opponent_alias;
    }
}
void StatTracker::setRankedStatus(bool inBool) {
    std::cout << "Ranked Status=" << inBool << std::endl;
    m_state.m_ranked_status = inBool;
}

void StatTracker::setRecordStatus(bool inBool) {
    std::cout << "Record Status=" << inBool << std::endl;
    mTrackerInfo.mRecord = inBool;
}

void StatTracker::setNetplaySession(bool netplay_session, bool is_host, std::string opponent_name){
    m_state.m_netplay_session = netplay_session;
    m_state.m_is_host = is_host;
    m_state.m_netplay_opponent_alias = opponent_name;
}
