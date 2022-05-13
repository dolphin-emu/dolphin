
#include "Core/MSB_StatTracker.h"

#include <iomanip>
#include <fstream>
#include <ctime>

//For LocalPLayers
#include "Common/CommonPaths.h"
#include "Common/IniFile.h"
#include "Core/LocalPlayersConfig.h"
#include "Common/Version.h"

#include "Common/Swap.h"

#include <iostream>

void StatTracker::Run(){
    lookForTriggerEvents();
}

void StatTracker::lookForTriggerEvents(){
    //At Bat State Machine
    if (m_game_state == GAME_STATE::INGAME){
        switch(m_event_state){
            case (EVENT_STATE::INIT):
                //Create new event, collect runner data
                //Game is live (not inbetween innings), and has been initialized
                if (((m_game_info.event_init_frame_buffer == 0)
                 && (Memory::Read_U8(aAB_GameIsLive) == 1)                  //Not at pause menu or inbetween innings
                 && (Memory::Read_U8(aAB_PlayIsReadyToStart) == 1)          //Pitcher ready to pitch, vars initialized
                 && (Memory::Read_U8(aAB_BatterPort) != 0)                  //BatterPort initialized
                 && (Memory::Read_U8(aAB_Inning) != 0)                      //Inning initialized
                 && (Memory::Read_U8(aAB_IsReplay) == 0))
                 ||  Memory::Read_U8(aAB_PitchThrown)){

                    if (m_game_info.event_num == 0) {
                        initPlayerInfo();
                    }

                    m_game_info.events[m_game_info.event_num] = Event();

                    logEventState(m_game_info.getCurrentEvent());
                    logGameInfo();

                    m_game_info.getCurrentEvent().runner_batter = logRunnerInfo(0);
                    m_game_info.getCurrentEvent().runner_1 = logRunnerInfo(1);
                    m_game_info.getCurrentEvent().runner_2 = logRunnerInfo(2);
                    m_game_info.getCurrentEvent().runner_3 = logRunnerInfo(3);

                    std::cout << "Init event " << std::to_string(m_game_info.event_num) << std::endl;

                    //Init fielder tracker anytime fielder changes
                    u8 team_id = !m_game_info.getCurrentEvent().half_inning;
                    if (!m_fielder_tracker[team_id].initialized){
                        std::cout << "initTracker. Team=" << std::to_string(team_id) << std::endl;
                        m_fielder_tracker[team_id].initTracker(team_id);
                        m_fielder_tracker[team_id].prev_batter_roster_loc = Memory::Read_U8(aAB_BatterRosterID);
                    }
                    
                    //Produce HUD file
                    std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                    std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "a", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                    File::Delete(hud_file_path);
                    File::WriteStringToFile(hud_file_path, json);
                    std::cout << "Init HUD" << std::endl;

                    //If we had to rely on pitch being thrown to bail us out print why
                    if (Memory::Read_U8(aAB_PitchThrown)){
                        std::cout << "  Frame=" << std::to_string(m_game_info.event_init_frame_buffer) << std::endl;
                        std::cout << "  Live=" << std::to_string(Memory::Read_U8(aAB_GameIsLive)) << ":1" << std::endl;
                        std::cout << "  Actionable=" << std::to_string(Memory::Read_U8(aAB_PlayIsReadyToStart)) << ":1" << std::endl;
                        std::cout << "  BatterPort=" << std::to_string(Memory::Read_U8(aAB_BatterPort)) << std::endl;
                        std::cout << "  Inning=" << std::to_string(Memory::Read_U8(aAB_Inning)) << std::endl;
                        std::cout << "  Replay=" << std::to_string(Memory::Read_U8(aAB_IsReplay)) << ":0" << std::endl;
                    }

                    m_event_state = EVENT_STATE::WAITING_FOR_EVENT;
                }
                else if (Memory::Read_U8(aEndOfGameFlag) == 1){
                    m_event_state = EVENT_STATE::GAME_OVER;
                }

                if (m_game_info.event_init_frame_buffer > 0) { 
                    --m_game_info.event_init_frame_buffer;
                    std::cout << "  Frame=" << std::to_string(m_game_info.event_init_frame_buffer) << std::endl; 
                }

                //If game is quit before the init
                if (Memory::Read_U32(aGameId) == 0){
                    onGameQuit();
                    m_event_state = EVENT_STATE::GAME_OVER;
                    m_game_state = GAME_STATE::ENDGAME_LOGGED;
                    break;
                }

                break;
            //Look for Pitch
            case (EVENT_STATE::WAITING_FOR_EVENT):
                //Handle quit to main menu
                if (Memory::Read_U32(aGameId) == 0){
                    onGameQuit();

                    //Remove current event, wasn't finished
                    auto it = m_game_info.events.find(m_game_info.event_num);
                    m_game_info.events.erase(it);

                    m_event_state = EVENT_STATE::GAME_OVER;
                    m_game_state = GAME_STATE::ENDGAME_LOGGED;
                    break;
                }

                //Trigger Events to look for
                //1. Are runners stealing and pitcher stepped off the mound
                //2. Has pitch started?
                //Watch for Runners Stealing
                if (Memory::Read_U8(aAB_PitchThrown)
                 || (anyRunnerStealing(m_game_info.getCurrentEvent()) && Memory::Read_U8(aAB_PickoffAttempt))){
                    //If HUD not produced for this event, produce HUD JSON
                    logGameInfo();

                    std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                    std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "a", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                    File::Delete(hud_file_path);
                    File::WriteStringToFile(hud_file_path, json);

                    if(Memory::Read_U8(aAB_PitchThrown)){
                        std::cout << "Pitch detected!" << std::endl;

                        //Check for fielder swaps
                        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].evaluateFielders();

                        m_game_info.getCurrentEvent().pitch = std::make_optional(Pitch());

                        //Check if pitcher was at center of mound, if so this is a potential DB
                        if (Memory::Read_U8(aFielder_Pos_X) == 0){
                            m_game_info.getCurrentEvent().pitch->potential_db = true;
                            std::cout << "Potential DB!" << std::endl;
                        }

                        //Pitch has started
                        m_event_state = EVENT_STATE::PITCH_STARTED;
                    }
                    else if(Memory::Read_U8(aAB_PickoffAttempt)) {
                        std::cout << "Pick of attempt detected!" << std::endl;
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                }
                break;
            case (EVENT_STATE::PITCH_STARTED): //Look for contact or end of pitch
                //DBs
                //If the pitcher started in the center of the mound this is a potential DB
                //If the ball curves at any point it is no longer a DB
                if (m_game_info.getCurrentEvent().pitch->potential_db && (Memory::Read_U8(aAB_PitcherHasCtrlofPitch) == 1)) {
                    if (floatConverter(Memory::Read_U32(aAB_PitchCurveInput)) != 0) {
                        std::cout << "No longer potential DB!" << std::endl;
                        m_game_info.getCurrentEvent().pitch->potential_db = false;
                    }
                }
                //If the ball gets behind the batter, record ball position for visualization
                if ((Memory::Read_U16(aAB_FramesUnitlBallArrivesBatter) == 1)
                && (Memory::Read_U16(aAB_FramesUnitlBallArrivesBatter) != 0)){
                    logPitch(m_game_info.getCurrentEvent());
                    m_game_info.getCurrentEvent().pitch->logged = true;

                }
                //HBP or miss
                if ((Memory::Read_U8(aAB_HitByPitch) == 1) || (Memory::Read_U8(aAB_PitchThrown) == 0)){
                    m_game_info.getCurrentEvent().result_of_atbat = Memory::Read_U8(aAB_FinalResult);
                    logContactMiss(m_game_info.getCurrentEvent()); //Strike or Swing or Bunt
                    if (!m_game_info.getCurrentEvent().pitch->logged) { logPitch(m_game_info.getCurrentEvent()); }
                    m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                }
                //Contact
                else if (Memory::Read_U32(aAB_BallVel_X)){
                    logPitch(m_game_info.getCurrentEvent());
                    logContact(m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::CONTACT;
                }

                //While pitch is in flight, record runner activity 
                //Log if runners are stealing
                if (m_game_info.getCurrentEvent().runner_1) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_1.value());
                }
                if (m_game_info.getCurrentEvent().runner_2) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_2.value());
                }
                if (m_game_info.getCurrentEvent().runner_3) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_3.value());
                }

                //Else just keep looking for contact or end of pitch
                break;
            case (EVENT_STATE::CONTACT):
                if (Memory::Read_U8(aAB_ContactResult)){
                    //Indicate that pitch resulted in contact and log contact details
                    m_game_info.getCurrentEvent().pitch->pitch_result = 6;
                    logContactResult(&m_game_info.getCurrentEvent().pitch->contact.value()); //Land vs Caught vs Foul, Landing POS.
                    if(m_event_state != EVENT_STATE::LOG_FIELDER) { //If we don't need to scan for which fielder fields the ball
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                }
                else {
                    //Ball is still in air
                    Contact* contact = &m_game_info.getCurrentEvent().pitch->contact.value();
                    contact->prev_ball_x_pos = contact->ball_x_pos;
                    contact->prev_ball_y_pos = contact->ball_y_pos;
                    contact->prev_ball_z_pos = contact->ball_z_pos;
                    contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
                    contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
                    contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);

                    if (floatConverter(contact->ball_y_pos) > floatConverter(contact->ball_y_pos_max_height)){
                        contact->ball_y_pos_max_height = contact->ball_y_pos;
                    }
                }
                //Could bobble before the ball hits the ground.
                //Search for bobble if we haven't recorded one yet and the ball hasn't been collected yet
                if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                     
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble();
                }

                //See if any fielder is selected
                logManualSelectLocks(m_game_info.getCurrentEvent());

                break;
            case (EVENT_STATE::LOG_FIELDER):
                //Look for bobble if we haven't seen any fielder touch the ball yet
                if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                    
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble();
                }
                
                if (!m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                    //Returns fielder that is holding the ball. Otherwise nullptr
                    m_game_info.getCurrentEvent().pitch->contact->collect_fielder = logFielderWithBall();
                    if (m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                        //Start watching runners for outs when the ball has finally been collected
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                }

                //See if any fielder is selected
                logManualSelectLocks(m_game_info.getCurrentEvent());

                //Break out if play ends without fielding the ball (HR or other play ending hit)
                if (!Memory::Read_U8(aAB_PitchThrown)) {
                    m_game_info.getCurrentEvent().result_of_atbat = Memory::Read_U8(aAB_FinalResult);
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                break;
            case (EVENT_STATE::MONITOR_RUNNERS):
                if (!Memory::Read_U8(aAB_PitchThrown) && !Memory::Read_U8(aAB_PickoffAttempt)){
                    m_game_info.getCurrentEvent().result_of_atbat = Memory::Read_U8(aAB_FinalResult);
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                else {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_batter.value());
                    if (m_game_info.getCurrentEvent().runner_1) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_1.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_2) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_2.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_3) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_3.value());
                    }
                }
                break;
            case (EVENT_STATE::PLAY_OVER):
                if (!Memory::Read_U8(aAB_PitchThrown)){
                    m_game_info.getCurrentEvent().rbi = Memory::Read_U8(aAB_RBI);
                    m_event_state = EVENT_STATE::FINAL_RESULT;
                    std::cout << "Play over" << std::endl;
                }
                break;
            case (EVENT_STATE::FINAL_RESULT):
                //Determine if this was pitch was a DB
                if (m_game_info.getCurrentEvent().pitch->potential_db){
                    m_game_info.getCurrentEvent().pitch->db = 1;                    
                    std::cout << "Logging DB!" << std::endl;
                }

                //runner_batter out, contact_secondary
                logFinalResults(m_game_info.getCurrentEvent());

                //Log post event HUD to file
                if (true){

                    //Fill in current state for HUD
                    logGameInfo();

                    //Store current state as previous state
                    m_game_info.previous_state = m_game_info.getCurrentEvent();

                    std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                    std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "b", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                    File::Delete(hud_file_path);
                    File::WriteStringToFile(hud_file_path, json);
                }

                //If End of Inning log entire file
                if ((Memory::Read_U8(aAB_NumOutsDuringPlay) + m_game_info.getCurrentEvent().outs) >= 3){
                    m_game_info.partial = 1;
                    logGameInfo();
                    std::string jsonPath = getStatJsonPath("partial.decoded.");
                    File::Delete(jsonPath);
                    std::string json = getStatJSON(true);
                        
                    File::WriteStringToFile(jsonPath, json);

                    jsonPath = getStatJsonPath("partial.");
                    File::Delete(jsonPath);
                    json = getStatJSON(false);

                    File::WriteStringToFile(jsonPath, json);
                    std::cout << "Logging partial to " << jsonPath << std::endl;

                    m_game_info.partial = 0;
                }

                //Increment event count
                ++m_game_info.event_num;

                m_game_info.event_init_frame_buffer = m_game_info.cNumOfFramesBeforeEventInit; //Wait 1 seconds before trying to read the next event
                
                m_event_state = EVENT_STATE::INIT;
                
                std::cout << "Logging Final Result" << std::endl << "Waiting for next pitch..." << std::endl;
                std::cout << std::endl;
                break;
            case (EVENT_STATE::GAME_OVER):
                std::cout << "Game Over. Waiting for next game" << std::endl;
                break;
            default:
                std::cout << "Unknown State" << std::endl;
                m_event_state = EVENT_STATE::INIT;
                break;
        }
    }

    //Game State Machine
    switch (m_game_state){
        case (GAME_STATE::PREGAME):
            //Start recording when GameId is set AND record button is pressed AND game has started
            if ((Memory::Read_U32(aGameId) != 0) && (mTrackerInfo.mRecord) && (Memory::Read_U8(aAB_GameIsLive) == 1) ) {
                m_game_info.game_id = Memory::Read_U32(aGameId);
                m_game_info.game_active = true;
                //Sample settings
                m_game_info.ranked  = m_state.m_ranked_status;
                m_game_info.netplay = m_state.m_netplay_session;
                m_game_info.host    = m_state.m_is_host;
                m_game_info.netplay_opponent_alias = m_state.m_netplay_opponent_alias;

                m_game_state = GAME_STATE::INGAME;
                std::cout << "PREGAME->INGAME (GameID=" << std::to_string(m_game_info.game_id) << ", Ranked=" << m_game_info.ranked <<")" << std::endl;
                std::cout << "                (Netplay=" << m_game_info.netplay << ", Host=" << m_game_info.host << ")" << std::endl
                          << "                (AwayTeam=" << m_game_info.getAwayTeamPlayer().GetUsername() << ", HomeTeam=" << m_game_info.getHomeTeamPlayer().GetUsername() << ")" << std::endl; 
            }
            break;
        case (GAME_STATE::INGAME):
            if ((Memory::Read_U8(aEndOfGameFlag) == 1)){
                logGameInfo();
                std::cout << "Logging Character Stats" << std::endl;

                std::string jsonPath = getStatJsonPath("decoded.");
                std::string json = getStatJSON(true);
                    
                File::WriteStringToFile(jsonPath, json);

                jsonPath = getStatJsonPath("");
                json = getStatJSON(false);
                //File::WriteStringToFile(jsonPath, json);
                //https://projectrio-api-1.api.projectrio.app/populate_db
                const Common::HttpRequest::Response response =
                m_http.Post("https://projectrio-api-1.api.projectrio.app/populate_db/", json,
                    {
                        {"Content-Type", "application/json"},
                    }
                );

                std::cout << "Logging to " << jsonPath << std::endl;

                //Clean up partial files
                jsonPath = getStatJsonPath("partial.");
                File::Delete(jsonPath);
                jsonPath = getStatJsonPath("partial.decoded.");
                File::Delete(jsonPath);

                m_game_state = GAME_STATE::ENDGAME_LOGGED;

                std::cout << "INGAME->ENDGAME" << std::endl;
            }
            break;
        case (GAME_STATE::ENDGAME_LOGGED):
            if (Memory::Read_U32(aGameId) == 0){
                m_game_info.game_active = false; //Game is over and logged
                m_game_state = GAME_STATE::PREGAME;
                init();

                std::cout << "ENDGAME->PREGAME" << std::endl;
            }
            break;
    }
}

void StatTracker::logGameInfo(){

    std::time_t unix_time = std::time(nullptr);

    m_game_info.end_unix_date_time = std::to_string(unix_time);
    m_game_info.end_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.end_local_date_time.pop_back();

    m_game_info.stadium = Memory::Read_U8(aStadiumId);

    m_game_info.innings_selected = Memory::Read_U8(aInningsSelected);
    m_game_info.innings_played   = Memory::Read_U8(aAB_Inning);

    ////Captains
    //if (m_game_info.away_port == m_game_info.team0_port){
    //    m_game_info.away_captain = Memory::Read_U8(aTeam0_Captain);
    //    m_game_info.home_captain = Memory::Read_U8(aTeam1_Captain);
    //}
    //else{
    //    m_game_info.away_captain = Memory::Read_U8(aTeam1_Captain);
    //    m_game_info.home_captain = Memory::Read_U8(aTeam0_Captain);
    //}

    m_game_info.away_score = Memory::Read_U16(aAwayTeam_Score);
    m_game_info.home_score = Memory::Read_U16(aHomeTeam_Score);

    for (int team=0; team < cNumOfTeams; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){
            logDefensiveStats(team, roster);
            logOffensiveStats(team, roster);
        }
    }
}

void StatTracker::logDefensiveStats(int in_team_id, int roster_id){
    u32 offset = (in_team_id * cRosterSize * c_defensive_stat_offset) + (roster_id * c_defensive_stat_offset);

    u32 ingame_attribute_table_offset = (in_team_id * cRosterSize * c_roster_table_offset) + (roster_id * c_roster_table_offset);
    u32 is_starred_offset = (in_team_id * cRosterSize) + roster_id;

    //in_team_id is in terms of team0 and team1 but we need it to 
    u8 adjusted_team_id;
    //Map team 0 and 1 to home and away
    if (in_team_id == 0){
        adjusted_team_id = (m_game_info.team0_port == m_game_info.away_port);
    }
    else{
        adjusted_team_id = (m_game_info.team1_port == m_game_info.away_port);
    }
    auto& stat = m_game_info.character_summaries[adjusted_team_id][roster_id].end_game_defensive_stats;

    m_game_info.character_summaries[adjusted_team_id][roster_id].is_starred = Memory::Read_U8(aPitcher_IsStarred + is_starred_offset);

    stat.batters_faced       = Memory::Read_U8(aPitcher_BattersFaced + offset);
    stat.runs_allowed        = Memory::Read_U16(aPitcher_RunsAllowed + offset);
    stat.earned_runs         = Memory::Read_U16(aPitcher_RunsAllowed + offset);
    stat.batters_walked      = Memory::Read_U16(aPitcher_BattersWalked + offset);
    stat.batters_hit         = Memory::Read_U16(aPitcher_BattersHit + offset);
    stat.hits_allowed        = Memory::Read_U16(aPitcher_HitsAllowed + offset);
    stat.homeruns_allowed    = Memory::Read_U16(aPitcher_HRsAllowed + offset);
    stat.pitches_thrown      = Memory::Read_U16(aPitcher_PitchesThrown + offset);
    stat.stamina             = Memory::Read_U16(aPitcher_Stamina + offset);
    stat.was_pitcher         = Memory::Read_U8(aPitcher_WasPitcher + offset);
    stat.batter_outs         = Memory::Read_U8(aPitcher_BatterOuts + offset);
    stat.outs_pitched        = Memory::Read_U8(aPitcher_OutsPitched + offset);
    stat.strike_outs         = Memory::Read_U8(aPitcher_StrikeOuts + offset);
    stat.star_pitches_thrown = Memory::Read_U8(aPitcher_StarPitchesThrown + offset);

    //Get inherent values. Doesn't strictly belong here but we need the adjusted_team_id
    m_game_info.character_summaries[adjusted_team_id][roster_id].char_id = Memory::Read_U8(aInGame_CharAttributes_CharId + ingame_attribute_table_offset);
    m_game_info.character_summaries[adjusted_team_id][roster_id].fielding_hand = Memory::Read_U8(aInGame_CharAttributes_FieldingHand + ingame_attribute_table_offset);
    m_game_info.character_summaries[adjusted_team_id][roster_id].batting_hand = Memory::Read_U8(aInGame_CharAttributes_BattingHand + ingame_attribute_table_offset);

}

void StatTracker::logOffensiveStats(int in_team_id, int roster_id){
    u32 offset = ((in_team_id * cRosterSize * c_offensive_stat_offset)) + (roster_id * c_offensive_stat_offset);

    //in_team_id is in terms of team0 and team1 but we need it to 
    u8 adjusted_team_id;
    //Map team 0 and 1 to home and away
    if (in_team_id == 0){
        adjusted_team_id = (m_game_info.team0_port == m_game_info.away_port);
    }
    else{
        adjusted_team_id = (m_game_info.team1_port == m_game_info.away_port);
    }
    auto& stat = m_game_info.character_summaries[adjusted_team_id][roster_id].end_game_offensive_stats;

    stat.at_bats          = Memory::Read_U8(aBatter_AtBats + offset);
    stat.hits             = Memory::Read_U8(aBatter_Hits + offset);
    stat.singles          = Memory::Read_U8(aBatter_Singles + offset);
    stat.doubles          = Memory::Read_U8(aBatter_Doubles + offset);
    stat.triples          = Memory::Read_U8(aBatter_Triples + offset);
    stat.homeruns         = Memory::Read_U8(aBatter_Homeruns + offset);
    stat.successful_bunts = Memory::Read_U8(aBatter_BuntSuccess + offset);
    stat.sac_flys         = Memory::Read_U8(aBatter_SacFlys + offset);
    stat.strikouts        = Memory::Read_U8(aBatter_Strikeouts + offset);
    stat.walks_4balls     = Memory::Read_U8(aBatter_Walks_4Balls + offset);
    stat.walks_hit        = Memory::Read_U8(aBatter_Walks_Hit + offset);
    stat.rbi              = Memory::Read_U8(aBatter_RBI + offset);
    stat.bases_stolen     = Memory::Read_U8(aBatter_BasesStolen + offset);
    stat.star_hits        = Memory::Read_U8(aBatter_StarHits + offset);

    m_game_info.character_summaries[adjusted_team_id][roster_id].end_game_defensive_stats.big_plays = Memory::Read_U8(aBatter_BigPlays + offset);
}

void StatTracker::logEventState(Event& in_event){
    in_event.inning          = Memory::Read_U8(aAB_Inning);
    in_event.half_inning     = Memory::Read_U8(aAB_HalfInning);

    //Figure out scores
    in_event.away_score = Memory::Read_U16(aAwayTeam_Score);
    in_event.home_score = Memory::Read_U16(aHomeTeam_Score);

    in_event.balls           = Memory::Read_U8(aAB_Balls);
    in_event.strikes         = Memory::Read_U8(aAB_Strikes);
    in_event.outs            = Memory::Read_U8(aAB_Outs);
    
    //Figure out star ownership
    if (m_game_info.team0_port == m_game_info.away_port){
        in_event.away_stars = Memory::Read_U8(aAB_P1_Stars);
        in_event.home_stars = Memory::Read_U8(aAB_P2_Stars);
    }
    else {
        in_event.away_stars = Memory::Read_U8(aAB_P2_Stars);
        in_event.home_stars = Memory::Read_U8(aAB_P1_Stars);
    }
    
    in_event.is_star_chance  = Memory::Read_U8(aAB_IsStarChance);
    in_event.chem_links_ob   = Memory::Read_U8(aAB_ChemLinksOnBase);

    //The following stamina lookup requires team_id to be in teams of team0 or team1

    u8 pitching_team_0_or_1 = Memory::Read_U8(aAB_PitcherPort) == m_game_info.team1_port;
    u8 pitcher_roster_loc = Memory::Read_U8(aAB_PitcherRosterID);
    
    //Calc the pitcher stamina offset and add it to the base stamina addr - TODO move to EventSummary
    u32 pitcherStaminaOffset = ((pitching_team_0_or_1 * cRosterSize * c_defensive_stat_offset) + (pitcher_roster_loc * c_defensive_stat_offset));
    in_event.pitcher_stamina = Memory::Read_U16(aPitcher_Stamina + pitcherStaminaOffset);

    in_event.pitcher_roster_loc = Memory::Read_U8(aAB_PitcherRosterID);
    in_event.batter_roster_loc  = Memory::Read_U8(aAB_BatterRosterID);
    in_event.catcher_roster_loc = Memory::Read_U8(aFielder_RosterLoc + (1 * cFielder_Offset));
}

void StatTracker::logContact(Event& in_event){
    std::cout << "Logging Contact" << std::endl;

    Pitch* pitch = &in_event.pitch.value();
    //Create contact object to populate and get a ptr to it
    pitch->contact = std::make_optional(Contact());
    std::cout << "  Pitch Type: " << std::to_string(in_event.pitch->pitch_type) << std::endl;
    Contact* contact = &in_event.pitch->contact.value();

    contact->type_of_contact   = Memory::Read_U8(aAB_TypeOfContact);
    contact->bunt              =(Memory::Read_U8(aAB_Bunt) == 3);

    contact->swing             = (Memory::Read_U8(aAB_Miss_SwingOrBunt) == 1);
    contact->charge_swing      = ((Memory::Read_U8(aAB_ChargeSwing) == 1) && !(Memory::Read_U8(aAB_StarSwing)));
    contact->charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    contact->charge_power_down = Memory::Read_U32(aAB_ChargeDown);
    contact->star_swing        = Memory::Read_U8(aAB_StarSwing);
    contact->moon_shot         = Memory::Read_U8(aAB_MoonShot);
    contact->input_direction_push_pull   = Memory::Read_U8(aAB_InputDirection);

    u32 aStickInput = aAB_ControlStickInput + ((Memory::Read_U8(aAB_BatterPort) - 1) * cControl_Offset);
    std::cout << " Stick Addr=" << std::hex << aStickInput << " Stick Value=" << Memory::Read_U16(aStickInput) << std::endl;
    contact->input_direction_stick   = (Memory::Read_U16(aStickInput) & 0xF); //Mask off the lower 4 bits which are the control stick directions

    contact->horiz_power       = Memory::Read_U16(aAB_HorizPower);
    contact->vert_power        = Memory::Read_U16(aAB_VertPower);
    contact->ball_angle        = Memory::Read_U16(aAB_BallAngle);

    contact->ball_x_velocity   = Memory::Read_U32(aAB_BallVel_X);
    contact->ball_y_velocity   = Memory::Read_U32(aAB_BallVel_Y);
    contact->ball_z_velocity   = Memory::Read_U32(aAB_BallVel_Z);

    //contact->ball_x_accel   = Memory::Read_U32(aAB_BallAccel_X);
    //contact->ball_y_accel   = Memory::Read_U32(aAB_BallAccel_Y);
    //contact->ball_z_accel   = Memory::Read_U32(aAB_BallAccel_Z);

    contact->hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    contact->frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
}

void StatTracker::logContactMiss(Event& in_event){
    std::cout << "Logging Miss" << std::endl;

    Pitch* pitch = &in_event.pitch.value();

    //Create contact object to populate and get a ptr to it
    pitch->contact = std::make_optional(Contact());
    Contact* contact = &in_event.pitch->contact.value();

    contact->type_of_contact   = 0xFF; //Set 0 because game only sets when contact is made. Never reset
    contact->bunt              =(Memory::Read_U8(aAB_Miss_SwingOrBunt) == 2); //Need to use miss version for bunt. Game won't set bunt regular flag unless contact is made
    contact->charge_swing      = ((Memory::Read_U8(aAB_ChargeSwing) == 1) && !(Memory::Read_U8(aAB_StarSwing)));
    contact->charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    contact->charge_power_down = Memory::Read_U32(aAB_ChargeDown);

    contact->star_swing        = Memory::Read_U8(aAB_StarSwing);
    contact->moon_shot         = Memory::Read_U8(aAB_MoonShot);
    contact->input_direction_push_pull   = Memory::Read_U8(aAB_InputDirection);
    
    u32 aStickInput = aAB_ControlStickInput + ((Memory::Read_U8(aAB_BatterPort) - 1) * cControl_Offset); //Subtract 1 from port to zero index. (Port 1 becomes port 0 which is what we need since the addr starts at port 1)
    contact->input_direction_stick   = (Memory::Read_U16(aStickInput) & 0xF); //Mask off the lower 4 bits which are the control stick directions

    contact->horiz_power       = 0;
    contact->vert_power        = 0;
    contact->ball_angle        = 0;

    contact->ball_x_velocity   = 0;
    contact->ball_y_velocity   = 0;
    contact->ball_z_velocity   = 0;

    contact->hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    //Frame collect
    contact->frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);

    u8 any_strike = Memory::Read_U8(aAB_Miss_AnyStrike);
    u8 miss_type  = Memory::Read_U8(aAB_Miss_SwingOrBunt);

    //0=HBP
    //1=BB
    //2=Ball
    //3=Strike-looking
    //4=Strike-swing
    //5=Strike-bunting
    //6=Contact
    //7=Unknown
    
    if (!any_strike){
        if (contact->hit_by_pitch){ pitch->pitch_result = 0; }
        else if (in_event.balls == 3) { pitch->pitch_result = 1; }
        else {
            pitch->pitch_result = 2;
        };
    }
    else{
        if (miss_type == 0){
            contact->swing = 0;
            pitch->pitch_result = 3;
        }
        else if (miss_type == 1){
            contact->swing = 1;
            pitch->pitch_result = 4;
        }
        else if (miss_type == 2){
            contact->swing = 0;
            pitch->pitch_result = 5;
        }
        else{
            pitch->pitch_result = 7;
        }
    }
}

void StatTracker::logPitch(Event& in_event){
    std::cout << "Logging Pitching" << std::endl;

    in_event.pitch->logged = true;
    in_event.pitch->pitcher_team_id    = Memory::Read_U8(aAB_PitcherPort) == m_game_info.away_port;
    in_event.pitch->pitcher_char_id    = Memory::Read_U8(aAB_PitcherID);
    in_event.pitch->pitch_type         = Memory::Read_U8(aAB_PitchType);
    in_event.pitch->charge_type        = Memory::Read_U8(aAB_ChargePitchType);
    in_event.pitch->star_pitch         = ((Memory::Read_U8(aAB_StarPitch_NonCaptain) > 0) || (Memory::Read_U8(aAB_StarPitch_Captain) > 0));
    in_event.pitch->pitch_speed        = Memory::Read_U8(aAB_PitchSpeed);

    in_event.pitch->ball_x_pos_upon_hit = Memory::Read_U32(aAB_BallPos_X_Upon_Hit);
    in_event.pitch->ball_z_pos_upon_hit = Memory::Read_U32(aAB_BallPos_Z_Upon_Hit);

    in_event.pitch->batter_x_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_X_Upon_Hit);
    in_event.pitch->batter_z_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_Z_Upon_Hit);
}

void StatTracker::logContactResult(Contact* in_contact){
    std::cout << "Logging Contact Result" << std::endl;

    u8 result = Memory::Read_U8(aAB_ContactResult);

    //Log primary contact result (and secondary if possible)
    if (result == 1){
        in_contact->primary_contact_result = 2; //Landed Fair
        m_event_state = EVENT_STATE::LOG_FIELDER;
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
    }
    else if (result == 2){ //Pretty sure means the ball landed and immediately was fielded
        in_contact->primary_contact_result = 3; //Landed Fair
        m_event_state = EVENT_STATE::LOG_FIELDER;
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);

        //Ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall();
    }
    else if (result == 3){
        in_contact->primary_contact_result = 0; //Out (secondary=caught)
        in_contact->secondary_contact_result = 0; //Out (secondary=caught)

        //If the ball is caught, use the balls position from the frame before to avoid the ball_pos
        //from matching the fielders
        in_contact->ball_x_pos = in_contact->prev_ball_x_pos;
        in_contact->ball_y_pos = in_contact->prev_ball_y_pos;
        in_contact->ball_z_pos = in_contact->prev_ball_z_pos; 

        //Ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall();

        //Increment outs for that position for fielder
        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(in_contact->collect_fielder->fielder_roster_loc, in_contact->collect_fielder->fielder_pos);
        //Indicate if fielder had been swapped for this batter
        in_contact->collect_fielder->fielder_swapped_for_batter = m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].wasFielderSwappedForBatter(in_contact->collect_fielder->fielder_roster_loc);

        std::cout << "Was fielder swapped. Team_id=" << (!m_game_info.getCurrentEvent().half_inning) 
                  << " Fielder Roster=" << std::to_string(in_contact->collect_fielder->fielder_roster_loc)
                  << " Swapped=" << std::to_string(in_contact->collect_fielder->fielder_swapped_for_batter) << std::endl;
    }
    else if (result == 0xFF){ // Known bug: this will be true for foul or HR. Correct when adjusting secondary contact later
        in_contact->primary_contact_result = 1; //Foul
        in_contact->secondary_contact_result = 3; //Foul
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
    }
    else{
        in_contact->primary_contact_result = result;
        in_contact->secondary_contact_result = 0xFF; //???
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
    }
}

void StatTracker::logFinalResults(Event& in_event){

    //Indicate strikeout in the runner_batter
    if (in_event.result_of_atbat == 1){
        //0x10 in runner_batter denotes strikeout
        in_event.runner_batter->out_type = 0x10;
    }

    if (in_event.pitch.has_value() && in_event.pitch->contact.has_value()){
        Contact* contact = &in_event.pitch->contact.value();
        //Fill in secondary result for contact
        if (in_event.result_of_atbat >= 0x7 && in_event.result_of_atbat <= 0xF){
            //0x10 in runner_batter denotes strikeout
            contact->secondary_contact_result = in_event.result_of_atbat;
            contact->primary_contact_result = 2; // Correct if set to foul
        }
        else if ((in_event.runner_batter->out_type == 2) || (in_event.runner_batter->out_type == 3)){
            contact->secondary_contact_result = in_event.runner_batter->out_type;
        }

        if (contact->star_swing == 1){ contact->type_of_swing = 3; }
        else if (contact->charge_swing == 1){ contact->type_of_swing = 2; }
        else if (contact->bunt == 1){ contact->type_of_swing = 4; }
        else if ((contact->swing == 1) 
            || (contact->type_of_contact != 0xFF) ) { contact->type_of_swing = 1; }
        else { contact->type_of_swing = 0; }
    }

    //multi_out
    in_event.pitch->contact->multi_out = (Memory::Read_U8(aAB_NumOutsDuringPlay) > 1);

    //Any out, increment all fielders batter out counts
    if (Memory::Read_U8(aAB_NumOutsDuringPlay) > 0) {
        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementBatterOutForPosition();
    }

    //If the runner got out at first (forced), increment outs for the fielder who first touched the ball
    if (in_event.runner_batter->out_type == 2) {
        //First fielder to touch the ball
        Fielder* fielder;

        //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielde
        if (in_event.pitch->contact->first_fielder.has_value()) { fielder = &in_event.pitch->contact->first_fielder.value(); }
        else {fielder = &in_event.pitch->contact->collect_fielder.value();}

        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(fielder->fielder_roster_loc, fielder->fielder_pos);
    }
}

std::string StatTracker::getStatJsonPath(std::string prefix){
    std::string away_player_name;
    std::string home_player_name;
    if (m_game_info.away_port == m_game_info.team0_port) {
        away_player_name = m_game_info.team0_player.GetUsername();
        home_player_name = m_game_info.team1_player.GetUsername();
    }
    else{
        away_player_name = m_game_info.team1_player.GetUsername();
        home_player_name = m_game_info.team0_player.GetUsername();
    }

    std::string file_name = prefix + away_player_name 
                   + "-Vs-" + home_player_name
                   + "_" + std::to_string(m_game_info.game_id) + ".json";

    std::string full_file_path = File::GetUserPath(D_STATFILES_IDX) + file_name;

    return full_file_path;
}

std::string StatTracker::getStatJSON(bool inDecode){
    //TODO switch to IDs when submitting game
    std::string away_player_info = (inDecode) ? m_game_info.getAwayTeamPlayer().GetUsername() : m_game_info.getAwayTeamPlayer().GetUserID();
    std::string home_player_info = (inDecode) ? m_game_info.getHomeTeamPlayer().GetUsername() : m_game_info.getHomeTeamPlayer().GetUserID();

    std::stringstream json_stream;

    json_stream << "{" << std::endl;
    std::string stadium = (inDecode) ? "\"" + cStadiumIdToStadiumName.at(m_game_info.stadium) + "\"" : std::to_string(m_game_info.stadium);
    std::string start_date_time = (inDecode) ? m_game_info.start_local_date_time : m_game_info.start_unix_date_time;
    std::string end_date_time = (inDecode) ? m_game_info.end_local_date_time : m_game_info.end_unix_date_time;
    json_stream << "  \"GameID\": \"" << std::hex << m_game_info.game_id << "\"," << std::endl;
    json_stream << "  \"Date - Start\": \"" << start_date_time << "\"," << std::endl;
    json_stream << "  \"Date - End\": \"" << end_date_time << "\"," << std::endl;
    json_stream << "  \"Ranked\": " << std::to_string(m_game_info.ranked) << "," << std::endl;
    json_stream << "  \"Netplay\": " << std::to_string(m_game_info.netplay) << "," << std::endl;
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, inDecode) << "," << std::endl;
    json_stream << "  \"Away Player\": \"" << away_player_info << "\"," << std::endl; //TODO MAKE THIS AN ID
    json_stream << "  \"Home Player\": \"" << home_player_info << "\"," << std::endl;

    json_stream << "  \"Away Score\": " << std::dec << m_game_info.away_score << "," << std::endl;
    json_stream << "  \"Home Score\": " << std::dec << m_game_info.home_score << "," << std::endl;

    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << "," << std::endl;
    json_stream << "  \"Innings Played\": " << std::to_string(m_game_info.innings_played) << "," << std::endl;
    json_stream << "  \"Quitter Team\": " << decode("QuitterTeam", m_game_info.quitter_team, inDecode) << "," << std::endl;
    //json_stream << "  \"Partial Game\": \"" << std::to_string(m_game_info.partial) << "\"," << std::endl;

    json_stream << "  \"Average Ping\": " << std::to_string(m_game_info.avg_ping) << "," << std::endl;
    json_stream << "  \"Lag Spikes\": " << std::to_string(m_game_info.lag_spikes) << "," << std::endl;
    json_stream << "  \"Version\": \"" << Common::GetRioRevStr() << "\"," << std::endl;

    json_stream << "  \"Character Game Stats\": {" << std::endl;

    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        // std::string team_label;
        u8 captain_roster_loc = 0;
        u8 tracker_team; // The tracker team is opposite of the batting team
        if (team == 0){
            captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            tracker_team = 1;
        }
        else{ // team == 1
            captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            tracker_team = 0;
        }

        for (int roster=0; roster < cRosterSize; ++roster){
            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            std::string label = "\"Team " + std::to_string(team) + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "    " << label << "{" << std::endl;
            json_stream << "      \"Team\": \""        << std::to_string(team) << "\"," << std::endl;
            json_stream << "      \"RosterID\": "      << std::to_string(roster) << "," << std::endl;
            json_stream << "      \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << "," << std::endl;
            json_stream << "      \"Superstar\": "     << std::to_string(char_summary.is_starred) << "," << std::endl;
            json_stream << "      \"Captain\": "       << std::to_string(roster == captain_roster_loc) << "," << std::endl;
            json_stream << "      \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << "," << std::endl;
            json_stream << "      \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << "," << std::endl;

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "      \"Defensive Stats\": {" << std::endl;
            json_stream << "        \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << "," << std::endl;
            json_stream << "        \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << "," << std::endl;
            json_stream << "        \"Earned Runs\": "        << std::dec << def_stat.earned_runs << "," << std::endl;
            json_stream << "        \"Batters Walked\": "      << def_stat.batters_walked << "," << std::endl;
            json_stream << "        \"Batters Hit\": "         << def_stat.batters_hit << "," << std::endl;
            json_stream << "        \"Hits Allowed\": "        << def_stat.hits_allowed << "," << std::endl;
            json_stream << "        \"HRs Allowed\": "         << def_stat.homeruns_allowed << "," << std::endl;
            json_stream << "        \"Pitches Thrown\": "      << def_stat.pitches_thrown << "," << std::endl;
            json_stream << "        \"Stamina\": "             << def_stat.stamina << "," << std::endl;
            json_stream << "        \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << "," << std::endl;
            json_stream << "        \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << "," << std::endl;
            json_stream << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "        \"Big Plays\": "           << std::to_string(def_stat.big_plays) << "," << std::endl;
            json_stream << "        \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << "," << std::endl;
            json_stream << "        \"Pitches Per Position\": [" << std::endl;

            if (m_fielder_tracker[tracker_team].pitchesAtAnyPosition(roster, 0)){
                json_stream << "          {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].pitch_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].pitchesAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].pitch_count_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "          }" << std::endl;
            }
            json_stream << "        ]," << std::endl;

            json_stream << "        \"Batter Outs Per Position\": [" << std::endl;
            if (m_fielder_tracker[tracker_team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "          {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].batter_outs_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "          }" << std::endl;
            }
            json_stream << "        ]," << std::endl;

            json_stream << "        \"Outs Per Position\": [" << std::endl;
            if (m_fielder_tracker[tracker_team].outsAtAnyPosition(roster, 0)){
                json_stream << "          {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].out_count_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "          }" << std::endl;
            }
            json_stream << "        ]" << std::endl;
            json_stream << "      }," << std::endl;

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "      \"Offensive Stats\": {" << std::endl;
            json_stream << "        \"At Bats\": "          << std::to_string(of_stat.at_bats) << "," << std::endl;
            json_stream << "        \"Hits\": "             << std::to_string(of_stat.hits) << "," << std::endl;
            json_stream << "        \"Singles\": "          << std::to_string(of_stat.singles) << "," << std::endl;
            json_stream << "        \"Doubles\": "          << std::to_string(of_stat.doubles) << "," << std::endl;
            json_stream << "        \"Triples\": "          << std::to_string(of_stat.triples) << "," << std::endl;
            json_stream << "        \"Homeruns\": "         << std::to_string(of_stat.homeruns) << "," << std::endl;
            json_stream << "        \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << "," << std::endl;
            json_stream << "        \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << "," << std::endl;
            json_stream << "        \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << "," << std::endl;
            json_stream << "        \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << "," << std::endl;
            json_stream << "        \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << "," << std::endl;
            json_stream << "        \"RBI\": "              << std::to_string(of_stat.rbi) << "," << std::endl;
            json_stream << "        \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << "," << std::endl;
            json_stream << "        \"Star Hits\": "        << std::to_string(of_stat.star_hits) << std::endl;
            json_stream << "      }" << std::endl;
            std::string commas = ((roster == 8) && (team ==1)) ? "" : ",";
            json_stream << "    }" << commas << std::endl;
        }
    }
    json_stream << "  }," << std::endl;
    //=== Events === 
    json_stream << "  \"Events\": [" << std::endl;
    for (auto event_map_iter = m_game_info.events.begin(); event_map_iter != m_game_info.events.end(); event_map_iter++) {
        u8 event_num = event_map_iter->first;
        Event& event = event_map_iter->second;

        //Don't log events with inning == 0. Means game has crashed/quit and this is an empty event
        if (event.inning == 0) {
            continue;
        }

        json_stream << "    {" << std::endl;
        json_stream << "      \"Event Num\": "               << std::to_string(event_num) << "," << std::endl;
        json_stream << "      \"Inning\": "                  << std::to_string(event.inning) << "," << std::endl;
        json_stream << "      \"Half Inning\": "             << std::to_string(event.half_inning) << "," << std::endl;
        json_stream << "      \"Away Score\": "              << std::dec << event.away_score << "," << std::endl;
        json_stream << "      \"Home Score\": "              << std::dec << event.home_score << "," << std::endl;
        json_stream << "      \"Balls\": "                   << std::to_string(event.balls) << "," << std::endl;
        json_stream << "      \"Strikes\": "                 << std::to_string(event.strikes) << "," << std::endl;
        json_stream << "      \"Outs\": "                    << std::to_string(event.outs) << "," << std::endl;
        json_stream << "      \"Star Chance\": "             << std::to_string(event.is_star_chance) << "," << std::endl;
        json_stream << "      \"Away Stars\": "              << std::to_string(event.away_stars) << "," << std::endl;
        json_stream << "      \"Home Stars\": "              << std::to_string(event.home_stars) << "," << std::endl;
        json_stream << "      \"Pitcher Stamina\": "         << std::to_string(event.pitcher_stamina) << "," << std::endl;
        json_stream << "      \"Chemistry Links on Base\": " << std::to_string(event.chem_links_ob) << "," << std::endl;
        json_stream << "      \"Pitcher Roster Loc\": "      << std::to_string(event.pitcher_roster_loc) << "," << std::endl;
        json_stream << "      \"Batter Roster Loc\": "       << std::to_string(event.batter_roster_loc) << "," << std::endl;
        json_stream << "      \"Catcher Roster Loc\": "       << std::to_string(event.catcher_roster_loc) << "," << std::endl;
        json_stream << "      \"RBI\": "                     << std::to_string(event.rbi) << "," << std::endl;
        json_stream << "      \"Result of AB\": "            << decode("AtBatResult", event.result_of_atbat, inDecode) << "," << std::endl;

        //=== Runners ===
        //Build vector of <Runner*, Label/Name>
        std::vector<std::pair<Runner*, std::string>> runners;
        if (event.runner_batter) {
            runners.push_back({&event.runner_batter.value(), "Batter"});
        }
        if (event.runner_1) {
            runners.push_back({&event.runner_1.value(), "1B"});
        }
        if (event.runner_2) {
            runners.push_back({&event.runner_2.value(), "2B"});
        }
        if (event.runner_3) {
            runners.push_back({&event.runner_3.value(), "3B"});
        }

        for (auto runner = runners.begin(); runner != runners.end(); runner++){
            Runner* runner_info = runner->first;
            std::string& label = runner->second;

            json_stream << "      \"Runner " << label << "\": {" << std::endl;
            json_stream << "        \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << "," << std::endl;
            json_stream << "        \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << "," << std::endl;
            json_stream << "        \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << "," << std::endl;
            json_stream << "        \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << "," << std::endl;
            json_stream << "        \"Out Location\": "        << std::to_string(runner_info->out_location) << "," << std::endl;
            //json_stream << "        \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << "," << std::endl;
            json_stream << "        \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << "," << std::endl;
            json_stream << "        \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << std::endl;
            std::string comma = (std::next(runner) == runners.end() && !event.pitch.has_value()) ? "" : ",";
            json_stream << "      }" << comma << std::endl;
        }


        //=== Pitch ===
        if (event.pitch.has_value()){
            Pitch* pitch = &event.pitch.value();
            json_stream << "      \"Pitch\": {" << std::endl;
            json_stream << "        \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << "," << std::endl;
            json_stream << "        \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << "," << std::endl;
            json_stream << "        \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << "," << std::endl;
            json_stream << "        \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << "," << std::endl;
            json_stream << "        \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << "," << std::endl;
            json_stream << "        \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << "," << std::endl;
            json_stream << "        \"Ball Position - X\": "   << floatConverter(pitch->ball_x_pos_upon_hit) << "," << std::endl;
            json_stream << "        \"Ball Position - Z\": "   << floatConverter(pitch->ball_z_pos_upon_hit) << "," << std::endl;
            json_stream << "        \"Batter Position - X\": " << floatConverter(pitch->batter_x_pos_upon_hit) << "," << std::endl;
            json_stream << "        \"Batter Position - Z\": " << floatConverter(pitch->batter_z_pos_upon_hit) << "," << std::endl;
            json_stream << "        \"DB\": "                 << std::to_string(pitch->db) << "," << std::endl;
            json_stream << "        \"Pitch Result\": "       << decode("PitchResult", pitch->pitch_result, inDecode) << "," << std::endl;
            if (pitch->contact.has_value()){
                json_stream << "        \"Type of Swing\": "      << decode("Swing", pitch->contact->type_of_swing, inDecode);
            }
            
            //=== Contact ===
            if (pitch->contact.has_value() && pitch->contact->type_of_contact != 0xFF){
                json_stream << "," << std::endl;

                Contact* contact = &pitch->contact.value();
                json_stream << "        \"Contact\": {" << std::endl;
                //json_stream << "          \"Type of Swing\": "                    << decode("Swing", contact->type_of_swing, inDecode) << "," << std::endl;
                json_stream << "          \"Type of Contact\": "                  << decode("Contact", contact->type_of_contact, inDecode) << "," << std::endl;
                json_stream << "          \"Charge Power Up\": "                  << floatConverter(contact->charge_power_up) << "," << std::endl;
                json_stream << "          \"Charge Power Down\": "                << floatConverter(contact->charge_power_down) << "," << std::endl;
                json_stream << "          \"Star Swing Five-Star\": "             << std::to_string(contact->moon_shot) << "," << std::endl; 
                json_stream << "          \"Input Direction - Push/Pull\": "      << decode("Stick", contact->input_direction_push_pull, inDecode) << "," << std::endl;
                json_stream << "          \"Input Direction - Stick\": "          << decode("StickVec", contact->input_direction_stick, inDecode) << "," << std::endl;
                json_stream << "          \"Frame of Swing Upon Contact\": "      << std::dec << contact->frameOfSwingUponContact << "," << std::endl;
                json_stream << "          \"Ball Angle\": \""                     << std::dec << contact->ball_angle << "\"," << std::endl;
                json_stream << "          \"Ball Vertical Power\": \""            << std::dec << contact->vert_power << "\"," << std::endl;
                json_stream << "          \"Ball Horizontal Power\": \""          << std::dec << contact->horiz_power << "\"," << std::endl;
                json_stream << "          \"Ball Velocity - X\": "                << floatConverter(contact->ball_x_velocity) << "," << std::endl;
                json_stream << "          \"Ball Velocity - Y\": "                << floatConverter(contact->ball_y_velocity) << "," << std::endl;
                json_stream << "          \"Ball Velocity - Z\": "                << floatConverter(contact->ball_z_velocity) << "," << std::endl;
                //json_stream << "          \"Ball Acceleration - X\": "            << ball_x_accel << "," << std::endl;
                //json_stream << "          \"Ball Acceleration - Y\": "            << ball_y_accel << "," << std::endl;
                //json_stream << "          \"Ball Acceleration - Z\": "            << ball_z_accel << "," << std::endl;
                json_stream << "          \"Ball Landing Position - X\": "        << floatConverter(contact->ball_x_pos) << "," << std::endl;
                json_stream << "          \"Ball Landing Position - Y\": "        << floatConverter(contact->ball_y_pos) << "," << std::endl;
                json_stream << "          \"Ball Landing Position - Z\": "        << floatConverter(contact->ball_z_pos) << "," << std::endl;

                json_stream << "          \"Ball Max Height\": "                  << floatConverter(contact->ball_y_pos_max_height) << "," << std::endl;

                json_stream << "          \"Multi-out\": "                        << std::to_string(contact->multi_out) << "," << std::endl;
                json_stream << "          \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << "," << std::endl;
                json_stream << "          \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

                //=== Fielder ===
                //TODO could be reworked
                if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                    json_stream << "," << std::endl;

                    //First fielder to touch the ball
                    Fielder* fielder;

                    //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                    
                    if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                    else {fielder = &contact->collect_fielder.value();}

                    json_stream << "          \"First Fielder\": {" << std::endl;
                    json_stream << "            \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << "," << std::endl;
                    json_stream << "            \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << "," << std::endl;
                    json_stream << "            \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << "," << std::endl;
                    json_stream << "            \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << "," << std::endl;
                    json_stream << "            \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << "," << std::endl;
                    json_stream << "            \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << "," << std::endl;
                    json_stream << "            \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_lock, inDecode) << "," << std::endl;
                    json_stream << "            \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << "," << std::endl;
                    json_stream << "            \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << "," << std::endl;
                    json_stream << "            \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << "," << std::endl;
                    json_stream << "            \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << std::endl;
                    json_stream << "          }" << std::endl;
                    /*
                    else if (contact->first_fielder.has_value() 
                            && (contact->first_fielder->fielder_roster_loc != contact->collect_fielder->fielder_roster_loc)) {
                        
                        Fielder* first_fielder  = &contact->first_fielder.value();
                        Fielder* second_fielder = &contact->collect_fielder.value();

                        json_stream << "          \"First Fielder\": {" << std::endl;
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(first_fielder->fielder_roster_loc) << "," << std::endl;
                        json_stream << "            \"Fielder Position\": "        << std::to_string(first_fielder->fielder_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Character\": "       << std::to_string(first_fielder->fielder_char_id) << "," << std::endl;
                        json_stream << "            \"Fielder Action\": "          << std::to_string(first_fielder->fielder_action) << "," << std::endl;
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(first_fielder->fielder_swapped_for_batter) << "," << std::endl;
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(first_fielder->fielder_x_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(first_fielder->fielder_y_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(first_fielder->fielder_z_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(first_fielder->bobble) << std::endl;
                        json_stream << "          }," << std::endl;
                        json_stream << "          \"Second Fielder\": {" << std::endl;
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(second_fielder->fielder_roster_loc) << "," << std::endl;
                        json_stream << "            \"Fielder Position\": "        << std::to_string(second_fielder->fielder_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Character\": "       << std::to_string(second_fielder->fielder_char_id) << "," << std::endl;
                        json_stream << "            \"Fielder Action\": "          << std::to_string(second_fielder->fielder_action) << "," << std::endl;
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(second_fielder->fielder_swapped_for_batter) << "," << std::endl;
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(second_fielder->fielder_x_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(second_fielder->fielder_y_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(second_fielder->fielder_z_pos) << "," << std::endl;
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(second_fielder->bobble) << std::endl;
                        json_stream << "          }" << std::endl;
                    }
                    */
                }
                else{ //Finish contact section
                    json_stream << std::endl;
                }
                json_stream << "        }" << std::endl;
            }
            else { //Finish pitch section
                json_stream << std::endl;
            }
            json_stream << "      }" << std::endl;
        }

        std::string end_comma = (std::next(event_map_iter) !=  m_game_info.events.end()) ? "," : "";
        json_stream << "    }" << end_comma << std::endl;
    }

    json_stream << "  ]" << std::endl;
    json_stream << "}" << std::endl;

    return json_stream.str();
}

std::string StatTracker::getHUDJSON(std::string in_event_num, Event& in_curr_event, std::optional<Event> in_prev_event, bool inDecode){
    std::stringstream json_stream;

    if (in_curr_event.inning == 0) {
        return "{}";
    }

    json_stream << "{" << std::endl;

    json_stream << "  \"Event Num\": \""             << in_event_num << "\"," << std::endl;
    json_stream << "  \"Away Player\": \""           << m_game_info.getAwayTeamPlayer().GetUsername() << "\"," << std::endl;
    json_stream << "  \"Home Player\": \""           << m_game_info.getHomeTeamPlayer().GetUsername() << "\"," << std::endl;
    json_stream << "  \"Inning\": "                  << std::to_string(in_curr_event.inning) << "," << std::endl;
    json_stream << "  \"Half Inning\": "             << std::to_string(in_curr_event.half_inning) << "," << std::endl;
    json_stream << "  \"Away Score\": "              << std::dec << in_curr_event.away_score << "," << std::endl;
    json_stream << "  \"Home Score\": "              << std::dec << in_curr_event.home_score << "," << std::endl;
    json_stream << "  \"Balls\": "                   << std::to_string(in_curr_event.balls) << "," << std::endl;
    json_stream << "  \"Strikes\": "                 << std::to_string(in_curr_event.strikes) << "," << std::endl;
    json_stream << "  \"Outs\": "                    << std::to_string(in_curr_event.outs) << "," << std::endl;
    json_stream << "  \"Star Chance\": "             << std::to_string(in_curr_event.is_star_chance) << "," << std::endl;
    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << "," << std::endl;
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << "," << std::endl;
    json_stream << "  \"Pitcher Stamina\": "         << std::to_string(in_curr_event.pitcher_stamina) << "," << std::endl;
    json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << "," << std::endl;
    json_stream << "  \"Pitcher Roster Loc\": "      << std::to_string(in_curr_event.pitcher_roster_loc) << "," << std::endl;
    json_stream << "  \"Batter Roster Loc\": "       << std::to_string(in_curr_event.batter_roster_loc) << "," << std::endl;

    for (int team=0; team < 2; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){

            u8 captain_roster_loc = 0;
            u8 tracker_team; // The tracker team is opposite of the batting team
            if (team == 0){
                captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
                tracker_team = 1;
            }
            else{ // team == 1
                captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
                tracker_team = 0;
            }

            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            std::string label = "\"Team " + std::to_string(team) + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "  " << label << "{" << std::endl;
            json_stream << "    \"Team\": \""        << std::to_string(team) << "\"," << std::endl;
            json_stream << "    \"RosterID\": "      << std::to_string(roster) << "," << std::endl;
            json_stream << "    \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << "," << std::endl;
            json_stream << "    \"Superstar\": "     << std::to_string(char_summary.is_starred) << "," << std::endl;
            json_stream << "    \"Captain\": "       << std::to_string(roster == captain_roster_loc) << "," << std::endl;
            json_stream << "    \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << "," << std::endl;
            json_stream << "    \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << "," << std::endl;

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "    \"Defensive Stats\": {" << std::endl;
            json_stream << "      \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << "," << std::endl;
            json_stream << "      \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << "," << std::endl;
            json_stream << "      \"Earned Runs\": "        << std::dec << def_stat.earned_runs << "," << std::endl;
            json_stream << "      \"Batters Walked\": "      << def_stat.batters_walked << "," << std::endl;
            json_stream << "      \"Batters Hit\": "         << def_stat.batters_hit << "," << std::endl;
            json_stream << "      \"Hits Allowed\": "        << def_stat.hits_allowed << "," << std::endl;
            json_stream << "      \"HRs Allowed\": "         << def_stat.homeruns_allowed << "," << std::endl;
            json_stream << "      \"Pitches Thrown\": "      << def_stat.pitches_thrown << "," << std::endl;
            json_stream << "      \"Stamina\": "             << def_stat.stamina << "," << std::endl;
            json_stream << "      \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << "," << std::endl;
            json_stream << "      \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << "," << std::endl;
            json_stream << "      \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "      \"Big Plays\": "           << std::to_string(def_stat.big_plays) << "," << std::endl;
            json_stream << "      \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << "," << std::endl;
            json_stream << "      \"Pitches Per Position\": [" << std::endl;

            if (m_fielder_tracker[tracker_team].pitchesAtAnyPosition(roster, 0)){
                json_stream << "        {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].pitch_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].pitchesAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].pitch_count_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "        }" << std::endl;
            }
            json_stream << "      ]," << std::endl;

            json_stream << "      \"Batter Outs Per Position\": [" << std::endl;
            if (m_fielder_tracker[tracker_team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "        {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].batter_outs_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "        }" << std::endl;
            }
            json_stream << "      ]," << std::endl;

            json_stream << "      \"Outs Per Position\": [" << std::endl;
            if (m_fielder_tracker[tracker_team].outsAtAnyPosition(roster, 0)){
                json_stream << "        {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[tracker_team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[tracker_team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[tracker_team].fielder_map[roster].out_count_by_position[pos]) << comma << std::endl;
                    }
                }
                json_stream << "        }" << std::endl;
            }
            json_stream << "      ]" << std::endl;
            json_stream << "    }," << std::endl;

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "    \"Offensive Stats\": {" << std::endl;
            json_stream << "      \"At Bats\": "          << std::to_string(of_stat.at_bats) << "," << std::endl;
            json_stream << "      \"Hits\": "             << std::to_string(of_stat.hits) << "," << std::endl;
            json_stream << "      \"Singles\": "          << std::to_string(of_stat.singles) << "," << std::endl;
            json_stream << "      \"Doubles\": "          << std::to_string(of_stat.doubles) << "," << std::endl;
            json_stream << "      \"Triples\": "          << std::to_string(of_stat.triples) << "," << std::endl;
            json_stream << "      \"Homeruns\": "         << std::to_string(of_stat.homeruns) << "," << std::endl;
            json_stream << "      \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << "," << std::endl;
            json_stream << "      \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << "," << std::endl;
            json_stream << "      \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << "," << std::endl;
            json_stream << "      \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << "," << std::endl;
            json_stream << "      \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << "," << std::endl;
            json_stream << "      \"RBI\": "              << std::to_string(of_stat.rbi) << "," << std::endl;
            json_stream << "      \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << "," << std::endl;
            json_stream << "      \"Star Hits\": "        << std::to_string(of_stat.star_hits) << std::endl;
            json_stream << "    }" << std::endl;
            json_stream << "  }," << std::endl;
        }
    }

    //=== Runners ===
    //Build vector of <Runner*, Label/Name>
    std::vector<std::pair<Runner*, std::string>> runners;
    if (in_curr_event.runner_batter) {
        runners.push_back({&in_curr_event.runner_batter.value(), "Batter"});
    }
    if (in_curr_event.runner_1) {
        runners.push_back({&in_curr_event.runner_1.value(), "1B"});
    }
    if (in_curr_event.runner_2) {
        runners.push_back({&in_curr_event.runner_2.value(), "2B"});
    }
    if (in_curr_event.runner_3) {
        runners.push_back({&in_curr_event.runner_3.value(), "3B"});
    }

    for (auto runner = runners.begin(); runner != runners.end(); runner++){
        Runner* runner_info = runner->first;
        std::string& label = runner->second;

        json_stream << "  \"Runner " << label << "\": {" << std::endl;
        json_stream << "    \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << "," << std::endl;
        json_stream << "    \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << "," << std::endl;
        json_stream << "    \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << "," << std::endl;
        json_stream << "    \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << "," << std::endl;
        json_stream << "    \"Out Location\": "        << std::to_string(runner_info->out_location) << "," << std::endl;
        //json_stream << "    \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << "," << std::endl;
        json_stream << "    \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << "," << std::endl;
        json_stream << "    \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << std::endl;
        std::string comma = (std::next(runner) == runners.end() && !in_prev_event.has_value() ) ? "" : ",";
        json_stream << "  }" << comma << std::endl;
    }

    //Previous Event - return if first event of game. Else write the event
    if (!in_prev_event.has_value()){
        json_stream << "}";
        return json_stream.str();
    }

    //=== Pitch ===

    json_stream << "  \"Previous Event\": {" << std::endl;
    json_stream << "    \"RBI\": "                     << std::to_string(in_prev_event->rbi) << "," << std::endl;
    std::string comma = (in_prev_event->pitch.has_value()) ? "," : "";
    json_stream << "    \"Result of AB\": "            << decode("AtBatResult", in_prev_event->result_of_atbat, inDecode) << comma << std::endl;
    if (in_prev_event->pitch.has_value()){
        Pitch* pitch = &in_prev_event->pitch.value();
        json_stream << "    \"Pitch\": {" << std::endl;
        json_stream << "      \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << "," << std::endl;
        json_stream << "      \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << "," << std::endl;
        json_stream << "      \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << "," << std::endl;
        json_stream << "      \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << "," << std::endl;
        json_stream << "      \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << "," << std::endl;
        json_stream << "      \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << "," << std::endl;
        json_stream << "      \"Ball Position - X\": "   << floatConverter(pitch->ball_x_pos_upon_hit) << "," << std::endl;
        json_stream << "      \"Ball Position - Z\": "   << floatConverter(pitch->ball_z_pos_upon_hit) << "," << std::endl;
        json_stream << "      \"Batter Position - X\": " << floatConverter(pitch->batter_x_pos_upon_hit) << "," << std::endl;
        json_stream << "      \"Batter Position - Z\": " << floatConverter(pitch->batter_z_pos_upon_hit) << "," << std::endl;
        json_stream << "      \"DB\": "                 << std::to_string(pitch->db) << "," << std::endl;
        json_stream << "      \"Pitch Result\": "       << decode("PitchResult", pitch->pitch_result, inDecode) << "," << std::endl;
        if (pitch->contact.has_value()){
            json_stream << "      \"Type of Swing\": "      << decode("Swing", pitch->contact->type_of_swing, inDecode);
        }
        
        //=== Contact ===
        if (pitch->contact.has_value() && pitch->contact->type_of_contact != 0xFF){
            json_stream << "," << std::endl;

            Contact* contact = &pitch->contact.value();
            json_stream << "      \"Contact\": {" << std::endl;
            json_stream << "        \"Type of Contact\": "                  << decode("Contact", contact->type_of_contact, inDecode) << "," << std::endl;
            json_stream << "        \"Charge Power Up\": "                  << floatConverter(contact->charge_power_up) << "," << std::endl;
            json_stream << "        \"Charge Power Down\": "                << floatConverter(contact->charge_power_down) << "," << std::endl;
            json_stream << "        \"Star Swing Five-Star\": "             << std::to_string(contact->moon_shot) << "," << std::endl;
            json_stream << "        \"Input Direction - Push/Pull\": "      << decode("Stick", contact->input_direction_push_pull, inDecode) << "," << std::endl;
            json_stream << "        \"Input Direction - Stick\": "          << decode("StickVec", contact->input_direction_stick, inDecode) << "," << std::endl;
            json_stream << "        \"Frame of Swing Upon Contact\": "      << std::dec << contact->frameOfSwingUponContact << "," << std::endl;
            json_stream << "        \"Ball Angle\": \""                     << std::dec << contact->ball_angle << "\"," << std::endl;
            json_stream << "        \"Ball Vertical Power\": \""            << std::dec << contact->vert_power << "\"," << std::endl;
            json_stream << "        \"Ball Horizontal Power\": \""          << std::dec << contact->horiz_power << "\"," << std::endl;
            json_stream << "        \"Ball Velocity - X\": "                << floatConverter(contact->ball_x_velocity) << "," << std::endl;
            json_stream << "        \"Ball Velocity - Y\": "                << floatConverter(contact->ball_y_velocity) << "," << std::endl;
            json_stream << "        \"Ball Velocity - Z\": "                << floatConverter(contact->ball_z_velocity) << "," << std::endl;
            //json_stream << "        \"Ball Acceleration - X\": "            << ball_x_accel << "," << std::endl;
            //json_stream << "        \"Ball Acceleration - Y\": "            << ball_y_accel << "," << std::endl;
            //json_stream << "        \"Ball Acceleration - Z\": "            << ball_z_accel << "," << std::endl;
            json_stream << "        \"Ball Landing Position - X\": "        << floatConverter(contact->ball_x_pos) << "," << std::endl;
            json_stream << "        \"Ball Landing Position - Y\": "        << floatConverter(contact->ball_y_pos) << "," << std::endl;
            json_stream << "        \"Ball Landing Position - Z\": "        << floatConverter(contact->ball_z_pos) << "," << std::endl;

            json_stream << "        \"Ball Max Height\": "                  << floatConverter(contact->ball_y_pos_max_height) << "," << std::endl;

            json_stream << "        \"Multi-out\": "                        << std::to_string(contact->multi_out) << "," << std::endl;
            json_stream << "        \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << "," << std::endl;
            json_stream << "        \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

            //=== Fielder ===
            //TODO could be reworked
            if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                json_stream << "," << std::endl;

                //First fielder to touch the ball
                Fielder* fielder;

                //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                
                if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                else {fielder = &contact->collect_fielder.value();}

                json_stream << "        \"First Fielder\": {" << std::endl;
                json_stream << "          \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << "," << std::endl;
                json_stream << "          \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << "," << std::endl;
                json_stream << "          \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << "," << std::endl;
                json_stream << "          \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << "," << std::endl;
                json_stream << "          \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << "," << std::endl;
                json_stream << "          \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << "," << std::endl;
                json_stream << "          \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_lock, inDecode) << "," << std::endl;
                json_stream << "          \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << "," << std::endl;
                json_stream << "          \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << "," << std::endl;
                json_stream << "          \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << "," << std::endl;
                json_stream << "          \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << std::endl;
                json_stream << "        }" << std::endl;
            }
            else{ //Finish contact section
                json_stream << std::endl;
            }
            json_stream << "      }" << std::endl; //close contact
        }
        else { //Finish pitch section
            json_stream << std::endl;
        }
        json_stream << "    }" << std::endl; //Close pitch
    }
    json_stream << "  }" << std::endl; //Close Previous Event
    json_stream << "}";
    return json_stream.str();
}

//Scans player for possession
std::optional<StatTracker::Fielder> StatTracker::logFielderWithBall() {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderControlStatus = aFielder_ControlStatus + (pos * cFielder_Offset);
        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);

        u32 aFielderManualSelectLock = aFielder_ManualSelectLock + (pos * cFielder_Offset);

        bool fielder_has_ball = (Memory::Read_U8(aFielderControlStatus) == 0xA);

        if (fielder_has_ball) {
            Fielder fielder_with_ball;
            //get char id
            fielder_with_ball.fielder_roster_loc = Memory::Read_U8(aFielderRosterLoc);
            fielder_with_ball.fielder_char_id = Memory::Read_U8(aFielderCharId);
            fielder_with_ball.fielder_pos = pos;

            fielder_with_ball.fielder_x_pos = Memory::Read_U32(aFielderPosX);
            fielder_with_ball.fielder_y_pos = Memory::Read_U32(aFielderPosY);
            fielder_with_ball.fielder_z_pos = Memory::Read_U32(aFielderPosZ);

            if (Memory::Read_U8(aFielderAction)) {
                fielder_with_ball.fielder_action = Memory::Read_U8(aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (Memory::Read_U8(aFielderJump)) {
                fielder_with_ball.fielder_jump = Memory::Read_U8(aFielderJump); //1 = jump
            }

            //Have to read from the array that the StatTracker maintains because if the fielder is holding the ball the flags are cleared
            std::cout << "Manual Select Locks=[" << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(0)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(1)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(2)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(3)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(4)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(5)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(6)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(7)) << ", "
                                         << std::to_string(m_game_info.getCurrentEvent().manual_select_locks.at(8)) << "]" << std::endl;

            fielder_with_ball.fielder_manual_select_lock = m_game_info.getCurrentEvent().manual_select_locks.at(pos);

            std::cout << "Manual Select Addr=" << std::hex << aFielderManualSelectLock << " Value=" << fielder_with_ball.fielder_manual_select_lock << std::endl;

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_with_ball.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_with_ball.fielder_action)
                      << " Manual Select=" << std::to_string(fielder_with_ball.fielder_manual_select_lock)
                      << " Jump=" << std::to_string(fielder_with_ball.fielder_jump) << std::endl;

            std::cout << "Logging Fielder" << std::endl;
            fielder = std::make_optional(fielder_with_ball);
            return fielder;
        }
    }
    return std::nullopt;
}

std::optional<StatTracker::Fielder> StatTracker::logFielderBobble() {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderBobbleStatus = aFielder_Bobble + (pos * cFielder_Offset);
        u32 aFielderKnockoutStatus = aFielder_Knockout + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);

        u32 aFielderManualSelectLock = aFielder_ManualSelectLock + (pos * cFielder_Offset);
        
        u8 typeOfFielderDisruption = 0x0;
        u8 bobble_addr = Memory::Read_U8(aFielderBobbleStatus);
        u8 knockout_addr = Memory::Read_U8(aFielderKnockoutStatus);

        if (knockout_addr) {
            typeOfFielderDisruption = 0x10; //Knockout - no bobble
        }
        else if (bobble_addr){
            typeOfFielderDisruption = bobble_addr; //Different types of bobbles
        }

        if (typeOfFielderDisruption > 0x1) {
            Fielder fielder_that_bobbled;
            //get char id
            fielder_that_bobbled.fielder_roster_loc = Memory::Read_U8(aFielderRosterLoc);
            fielder_that_bobbled.fielder_char_id = Memory::Read_U8(aFielderCharId);

            fielder_that_bobbled.fielder_x_pos = Memory::Read_U32(aFielderPosX);
            fielder_that_bobbled.fielder_y_pos = Memory::Read_U32(aFielderPosY);
            fielder_that_bobbled.fielder_z_pos = Memory::Read_U32(aFielderPosZ);
            fielder_that_bobbled.bobble = typeOfFielderDisruption;

            if (Memory::Read_U8(aFielderAction)) {
                fielder_that_bobbled.fielder_action = Memory::Read_U8(aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (Memory::Read_U8(aFielderJump)) {
                fielder_that_bobbled.fielder_jump = Memory::Read_U8(aFielderJump); //1 = jump
            }

            //We can read manual select now because we don't have the ball
            fielder_that_bobbled.fielder_manual_select_lock = Memory::Read_U8(aFielderManualSelectLock);

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_that_bobbled.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_that_bobbled.fielder_action) 
                      << " Jump=" << std::to_string(fielder_that_bobbled.fielder_jump)
                      << " Manual Select=" << std::to_string(fielder_that_bobbled.fielder_manual_select_lock)
                      << " Bobble=" << std::to_string(fielder_that_bobbled.bobble) << std::endl;
                      
            fielder = std::make_optional(fielder_that_bobbled);
            return fielder;
        }
    }
    return std::nullopt;
}

void StatTracker::logManualSelectLocks(Event& in_event){
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderManualSelectLock = aFielder_ManualSelectLock + (pos * cFielder_Offset);

        in_event.manual_select_locks.at(pos) = Memory::Read_U8(aFielderManualSelectLock);
    }
}

//Read players from ini file and assign to team
void StatTracker::readPlayerNames(bool local_game) {
  int team0_port = m_game_info.team0_port;
  int team1_port = m_game_info.team1_port;

  if (local_game)
  {
    // Player 1
    if (team0_port == 1)
        m_game_info.team0_player = LocalPlayers::m_local_player_1;
    else {
        m_game_info.team0_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team0_player.username = "CPU";
        m_game_info.team0_player.userid = "CPU";
    }

    // other player
    if (team1_port == 2)
        m_game_info.team1_player = LocalPlayers::m_local_player_2;
    else if (team1_port == 3)
        m_game_info.team1_player = LocalPlayers::m_local_player_3;
    else if (team1_port == 4)
        m_game_info.team1_player = LocalPlayers::m_local_player_4;
    else {
        m_game_info.team1_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team1_player.username = "CPU";
        m_game_info.team1_player.userid = "CPU";
    }
  }

  else
  {
    m_game_info.team0_player = m_game_info.NetplayerUserInfo[team0_port];
    m_game_info.team1_player = m_game_info.NetplayerUserInfo[team1_port];
  }
}

void StatTracker::setDefaultNames(bool local_game){
    //if (m_game_info.team0_port >= 5) m_game_info.team0_player_name = "CPU";
    //if (m_game_info.team1_port >= 5) m_game_info.team1_player_name = "CPU";

    //if (!local_game) {
    //    if (m_game_info.team0_player.GetUsername().empty()) m_game_info.team0_player = "Netplayer~" + m_game_info.netplay_opponent_alias;
    //    if (m_game_info.team1_player.GetUsername().empty()) m_game_info.team1_player = "Netplayer~" + m_game_info.netplay_opponent_alias;
    //}
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

void StatTracker::setAvgPing(int avgPing)
{
  //std::cout << "Avg Ping=" << avgPing << std::endl;
  m_game_info.avg_ping = avgPing;
}

void StatTracker::setLagSpikes(int nLagSpikes)
{
  //std::cout << "Number of Lag Spikes=" << nLagSpikes << std::endl;
  m_game_info.lag_spikes = nLagSpikes;
}

void StatTracker::setNetplayerUserInfo(std::map<int, LocalPlayers::LocalPlayers::Player> userInfo)
{
  for (auto player : userInfo)
    m_game_info.NetplayerUserInfo[player.first] = player.second;
}

void StatTracker::initPlayerInfo(){
    //Read start time
    std::time_t unix_time = std::time(nullptr);
    m_game_info.start_unix_date_time = std::to_string(unix_time);
    m_game_info.start_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.start_local_date_time.pop_back();
    //Collect port info for players
    if (m_game_info.team0_port == 0xFF && m_game_info.team1_port == 0xFF){
        u8 fielder_port = Memory::Read_U8(aAB_FieldingPort);
        u8 batter_port = Memory::Read_U8(aAB_BattingPort);
        
        //The lower value will always be team0. If CPU vs CPU is on Team0 == 5 and Team1 == 6
        if (fielder_port < batter_port) {
            m_game_info.team0_port = fielder_port;
            m_game_info.team1_port = batter_port;
        }
        else {
            m_game_info.team0_port = batter_port;
            m_game_info.team1_port = fielder_port;
        }

        //Map home and away ports for scores
        m_game_info.away_port = batter_port;
        m_game_info.home_port = fielder_port;

        readPlayerNames(!m_game_info.netplay);
        setDefaultNames(!m_game_info.netplay);

        std::string away_player_name;
        std::string home_player_name;
        if (m_game_info.away_port == m_game_info.team0_port) {
            away_player_name = m_game_info.team0_player.GetUsername();
            home_player_name = m_game_info.team1_player.GetUsername();
        }
        else{
            away_player_name = m_game_info.team1_player.GetUsername();
            home_player_name = m_game_info.team0_player.GetUsername();
        }

        std::cout << "Info:  Fielder Port=" << std::to_string(fielder_port) << ", Batter Port=" << std::to_string(batter_port) << std::endl;
        std::cout << "Info:  Team0 Port=" << std::to_string(m_game_info.team0_port) << ", Team1 Port=" << std::to_string(m_game_info.team1_port) << std::endl;
        std::cout << "Info:  Away Port=" << std::to_string(m_game_info.away_port) << ", Home Port=" << std::to_string(m_game_info.home_port) << std::endl;
        std::cout << "Info:  Away Player=" << (away_player_name) << ", Home Player=" << (home_player_name) << std::endl << std::endl;
    }

    //Get captain roster positions
    if ((m_game_info.team0_captain_roster_loc == 0xFF) || (m_game_info.team1_captain_roster_loc == 0xFF)) {
        m_game_info.team0_captain_roster_loc = Memory::Read_U8(aTeam0_Captain_Roster_Loc);
        m_game_info.team1_captain_roster_loc = Memory::Read_U8(aTeam1_Captain_Roster_Loc);
    }
}

void StatTracker::onGameQuit(){
    u8 quitter_port = Memory::Read_U8(aWhoQuit);
    m_game_info.quitter_team = (quitter_port == m_game_info.away_port);
    logGameInfo();

    std::cout << "Quit detected" << std::endl;

    //Game has ended. Write file but do not submit
    std::string jsonPath = getStatJsonPath("quit.decode.");
    std::string json = getStatJSON(true);
    
    File::WriteStringToFile(jsonPath, json);

    jsonPath = getStatJsonPath("quit.");
    json = getStatJSON(false);
    
    File::WriteStringToFile(jsonPath, json);
    const Common::HttpRequest::Response response =
    m_http.Post("https://projectrio-api-1.api.projectrio.app/populate_db/", json,
        {
            {"Content-Type", "application/json"},
        }
    );

    //Clean up partial files
    jsonPath = getStatJsonPath("partial.");
    File::Delete(jsonPath);
    jsonPath = getStatJsonPath("partial.decoded.");
    File::Delete(jsonPath);
}

std::optional<StatTracker::Runner> StatTracker::logRunnerInfo(u8 base){
    std::optional<Runner> runner;
    //See if there is a runner in this pos
    if (Memory::Read_U8(aRunner_RosterLoc + (base * cRunner_Offset)) != 0xFF){
        Runner init_runner;
        init_runner.roster_loc = Memory::Read_U8(aRunner_RosterLoc + (base * cRunner_Offset));
        init_runner.char_id = Memory::Read_U8(aRunner_CharId + (base * cRunner_Offset));
        init_runner.initial_base = base;
        init_runner.basepath_location = Memory::Read_U32(aRunner_BasepathPercentage + (base * cRunner_Offset));
        runner = std::make_optional(init_runner);
        return runner;        
    }
    return runner;
}

bool StatTracker::anyRunnerStealing(Event& in_event){
    u8 runner_1_stealing = Memory::Read_U8(aRunner_Stealing + (1 * cRunner_Offset));
    u8 runner_2_stealing = Memory::Read_U8(aRunner_Stealing + (2 * cRunner_Offset));
    u8 runner_3_stealing = Memory::Read_U8(aRunner_Stealing + (3 * cRunner_Offset));

    return (runner_1_stealing || runner_2_stealing || runner_3_stealing);
}

void StatTracker::logRunnerEvents(Runner* in_runner){
    //Return if no runner
    if (in_runner->out_type != 0 ) { return; }

    //Return if runner has already gotten out
    in_runner->out_type = Memory::Read_U8(aRunner_OutType + (in_runner->initial_base * cRunner_Offset));
    if (in_runner->out_type != 0) {
        in_runner->out_location = Memory::Read_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
        in_runner->result_base = 0xFF;
        in_runner->basepath_location = Memory::Read_U32(aRunner_BasepathPercentage + (in_runner->initial_base * cRunner_Offset));

        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Out. Type=" << std::to_string(in_runner->out_type)
        << " Location=" << std::to_string(in_runner->out_location) << std::endl;
    }
    else{
        in_runner->result_base = Memory::Read_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
    }

    if (Memory::Read_U8(aRunner_Stealing + (in_runner->initial_base * cRunner_Offset)) > in_runner->steal){
        in_runner->steal = Memory::Read_U8(aRunner_Stealing + (in_runner->initial_base * cRunner_Offset));
        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Steal. Type=" << std::to_string(in_runner->steal)<< std::endl;
    }
}

std::string StatTracker::decode(std::string type, u8 value, bool decode){
    if (!decode) { return std::to_string(value);}

    std::string retVal = "Unable to Decode";
    
    if (type == "Character"){
        if (cCharIdToCharName.count(value)){
            retVal = cCharIdToCharName.at(value);
        }
    }
    else if (type == "Stadium"){
        if (cStadiumIdToStadiumName.count(value)){
            retVal = cStadiumIdToStadiumName.at(value);
        }
    }
    else if (type == "Contact"){
        if (cTypeOfContactToHR.count(value)){
            retVal = cTypeOfContactToHR.at(value);
        }
    }
    else if (type == "Hand"){
        if (cHandToHR.count(value)){
            retVal = cHandToHR.at(value);
        }
    }
    else if (type == "Stick"){
        if (cInputDirectionToHR.count(value)){
            retVal = cInputDirectionToHR.at(value);
        }
    }
    else if (type == "StickVec"){
        retVal = "";
        if ( (value & 0x1) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Left";
        }
        if ( (value & 0x2) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Right";
        } 
        if ( (value & 0x4) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Down";
        } 
        if ( (value & 0x8) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Up";
        } 
    }
    else if (type == "Pitch"){
        if (cPitchTypeToHR.count(value)){
            retVal = cPitchTypeToHR.at(value);
        }
    }
    else if (type == "ChargePitch"){
        if (cChargePitchTypeToHR.count(value)){
            retVal = cChargePitchTypeToHR.at(value);
        }
    }
    else if (type == "Swing"){
        if (cTypeOfSwing.count(value)){
            retVal = cTypeOfSwing.at(value);
        }
    }
    else if (type == "Position"){
        if (cPosition.count(value)){
            retVal = cPosition.at(value);
        }
    }
    else if (type == "Action"){
        if (cFielderActions.count(value)){
            retVal = cFielderActions.at(value);
        }
    }
    else if (type == "Bobble"){
        if (cFielderBobbles.count(value)){
            retVal = cFielderBobbles.at(value);
        }
    }
    else if (type == "ManualSelect"){
        if (cManualSelectDecode.count(value)){
            retVal = cManualSelectDecode.at(value);
        }
    }
    else if (type == "Steal"){
        if (cStealType.count(value)){
            retVal = cStealType.at(value);
        }
    }
    else if (type == "Out"){
        if (cOutType.count(value)){
            retVal = cOutType.at(value);
        }
    }
    else if (type == "PrimaryContactResult"){
        if (cPrimaryContactResult.count(value)){
            retVal = cPrimaryContactResult.at(value);
        }
    }
    else if (type == "SecondaryContactResult"){
        if (cSecondaryContactResult.count(value)){
            retVal = cSecondaryContactResult.at(value);
        }
    }
    else if (type == "PitchResult"){
        if (cPitchResult.count(value)){
            retVal = cPitchResult.at(value);
        }
    }
    else if (type == "AtBatResult"){
        if (cAtBatResult.count(value)){
            retVal = cAtBatResult.at(value);
        }
    }
    else if (type == "QuitterTeam"){
        if (value == 0){
            retVal = "Home";
        }
        else if (value == 1){
            retVal = "Away";
        }
        else if (value == 2){
            retVal = "Crash";
        }
        else if (value == 0xFF){
            retVal = "None";
        }
    }
    else{
        retVal += ". Invalid Type (" + type + ")";
    }
    
    if (retVal == "Unable to Decode"){
        retVal += ". Invalid Value (" + std::to_string(value) + ").";
    }
    return ("\"" + retVal + "\"");
}
